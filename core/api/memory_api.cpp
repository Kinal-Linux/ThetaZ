// ThetaZ — VirtualAlloc/Free/Query/Protect backed by the WindowsAddressSpace.
// Implemented in a separate TU because kernel32.h defines MEM_*/PAGE_* as macros
// while core/ntdll/virtual.h defines them as constexpr (they must not clash).
// Copyright (c) 2026 ThetaZ Project. MIT License.

#include "core/api/api.h"
#include "core/ntdll/virtual.h"

#include <sys/mman.h>
#include <cstdio>
#include <cstring>
#include <strings.h>

#ifndef MEM_IMAGE
#define MEM_IMAGE 0x01000000
#endif

namespace ThetaZ {

// Same type as kernel32.h's MEMORY_BASIC_INFORMATION (name must match for
// symbol-mangling); layout matches virtual.h's MemoryBasicInformation.
struct MEMORY_BASIC_INFORMATION {
    void* BaseAddress;
    void* AllocationBase;
    uint32_t AllocationProtect;
    size_t RegionSize;
    uint32_t State;
    uint32_t Protect;
    uint32_t Type;
};
using PMEMORY_BASIC_INFORMATION = MEMORY_BASIC_INFORMATION*;

// Fill a MemoryBasicInformation from /proc/self/maps for any address that maps
// to a real region of this process (guest images, host libs, heaps, stacks).
// Returns false when the address is not mapped at all.
bool query_from_proc_maps(const void* addr, MemoryBasicInformation* out) {
    uintptr_t target = reinterpret_cast<uintptr_t>(addr);
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return false;

    char line[512];
    bool found = false;
    while (fgets(line, sizeof(line), f)) {
        unsigned long long start, end, off = 0;
        char perm[8] = {0};
        int n = sscanf(line, "%llx-%llx %7s %llx", &start, &end, perm, &off);
        if (n < 3) continue;
        if (target < start || target >= end) continue;

        uint32_t prot = PAGE_NOACCESS;
        if (perm[0] == 'r' && perm[1] == 'w' && perm[2] == 'x') {
            prot = PAGE_EXECUTE_READWRITE;
        } else if (perm[0] == 'r' && perm[2] == 'x') {
            prot = PAGE_EXECUTE_READ;
        } else if (perm[0] == 'r' && perm[1] == 'w') {
            prot = PAGE_READWRITE;
        } else if (perm[0] == 'r') {
            prot = PAGE_READONLY;
        } else if (perm[2] == 'x') {
            prot = PAGE_EXECUTE;
        }

        out->base_address = reinterpret_cast<void*>(start);
        out->allocation_base = reinterpret_cast<void*>(start);
        out->allocation_protect = prot;
        out->region_size = static_cast<size_t>(end - start);
        out->state = (prot == PAGE_NOACCESS) ? MEM_RESERVE : MEM_COMMIT;
        out->protect = prot;
        out->type = MEM_PRIVATE;

        // File-backed PE image mappings report MEM_IMAGE on Windows.
        const char* path = strrchr(line, '/');
        if (path) {
            const char* ext = strrchr(path, '.');
            if (ext && (strcasecmp(ext, ".dll") == 0 || strcasecmp(ext, ".exe") == 0)) {
                out->type = MEM_IMAGE;
            }
        }
        found = true;
        break;
    }
    fclose(f);
    return found;
}

LPVOID WINAPI_CALL THETAZ_VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect) {
    WindowsAddressSpace& space = WindowsAddressSpace::instance();
    if (dwSize == 0) return nullptr;

    const bool want_reserve = (flAllocationType & MEM_RESERVE) != 0;
    const bool want_commit = (flAllocationType & MEM_COMMIT) != 0;

    if (lpAddress == nullptr) {
        if (want_reserve && want_commit) {
            return space.reserve_and_commit(dwSize, 0x1000, flProtect);
        }
        if (want_reserve) {
            return space.reserve(dwSize, 0x1000);
        }
        if (want_commit) {
            return space.reserve_and_commit(dwSize, 0x1000, flProtect);
        }
        return nullptr;
    }

    // Fixed address. Windows rounds the requested address down to a page.
    uintptr_t base = reinterpret_cast<uintptr_t>(lpAddress) & ~0xFFFULL;
    fprintf(stderr, "[VirtualAlloc] fixed base=0x%lx size=0x%lx prot=0x%x reserve=%d commit=%d\n", base, dwSize, flProtect, want_reserve, want_commit);
    if (want_commit) {
        if (!space.commit(reinterpret_cast<void*>(base), dwSize, flProtect)) {
            int prot = PROT_READ | PROT_WRITE | PROT_EXEC;
            void* p = mmap(reinterpret_cast<void*>(base), dwSize, prot, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
            fprintf(stderr, "[VirtualAlloc] fixed fallback mmap base=0x%lx p=%p prot=0x%x err=%m\n", base, p, prot);
            if (p == MAP_FAILED) return nullptr;
            return reinterpret_cast<void*>(base);
        }
    }
    if (want_reserve && !want_commit) {
        if (!space.reserve(dwSize, 0x1000)) {
            int prot = PROT_READ | PROT_WRITE | PROT_EXEC;
            void* p = mmap(reinterpret_cast<void*>(base), dwSize, prot, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
            fprintf(stderr, "[VirtualAlloc] fixed reserve fallback mmap base=0x%lx p=%p prot=0x%x err=%m\n", base, p, prot);
            if (p == MAP_FAILED) return nullptr;
            return reinterpret_cast<void*>(base);
        }
    }
    return reinterpret_cast<void*>(base);
}

BOOL WINAPI_CALL THETAZ_VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType) {
    WindowsAddressSpace& space = WindowsAddressSpace::instance();
    if (!lpAddress) return 0;

    if (dwFreeType & MEM_RELEASE) {
        return space.release(lpAddress) ? 1 : 0;
    }
    if (dwFreeType & MEM_DECOMMIT) {
        return space.decommit(lpAddress, dwSize) ? 1 : 0;
    }
    return 0;
}

SIZE_T WINAPI_CALL THETAZ_VirtualQuery(LPCVOID lpAddress, PMEMORY_BASIC_INFORMATION lpBuffer, SIZE_T dwLength) {
    if (!lpBuffer || dwLength < sizeof(MEMORY_BASIC_INFORMATION)) return 0;

    MemoryBasicInformation info;
    if (WindowsAddressSpace::instance().query(const_cast<void*>(lpAddress), &info)) {
        memset(lpBuffer, 0, sizeof(MEMORY_BASIC_INFORMATION));
        lpBuffer->BaseAddress = info.base_address;
        lpBuffer->AllocationBase = info.allocation_base;
        lpBuffer->AllocationProtect = info.allocation_protect;
        lpBuffer->RegionSize = info.region_size;
        lpBuffer->State = info.state;
        lpBuffer->Protect = info.protect;
        lpBuffer->Type = info.type;
        return sizeof(MEMORY_BASIC_INFORMATION);
    }

    // The address is not tracked by WindowsAddressSpace (guest images mapped by
    // the PELoader, Wine DLL blobs, heaps, stacks, the 32-bit identity-mapped
    // world, shim pools...). Answer from /proc/self/maps, which sees every real
    // mapping of this process and also distinguishes reserved (PROT_NONE, no
    // perms -> MEM_RESERVE) from committed regions.
    if (query_from_proc_maps(lpAddress, &info)) {
        memset(lpBuffer, 0, sizeof(MEMORY_BASIC_INFORMATION));
        lpBuffer->BaseAddress = info.base_address;
        lpBuffer->AllocationBase = info.allocation_base;
        lpBuffer->AllocationProtect = info.allocation_protect;
        lpBuffer->RegionSize = info.region_size;
        lpBuffer->State = info.state;
        lpBuffer->Protect = info.protect;
        lpBuffer->Type = info.type;
        return sizeof(MEMORY_BASIC_INFORMATION);
    }

    memset(lpBuffer, 0, sizeof(MEMORY_BASIC_INFORMATION));
    return 0;
}

BOOL WINAPI_CALL THETAZ_VirtualProtect(LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, DWORD* lpflOldProtect) {
    if (!lpAddress) return 0;
    if (WindowsAddressSpace::instance().protect(lpAddress, dwSize, flNewProtect, lpflOldProtect)) return 1;

    // Not tracked by WindowsAddressSpace: guest images mmap'd by the PELoader,
    // Wine DLL blobs and the shim pool are all deliberately kept RW(X) (guest
    // code patches its IAT/data at runtime), and the identity-mapped 32-bit
    // world is always writable through the interpreter. Restricting protection
    // on natively-executed guest memory would SIGSEGV the host process, so just
    // report success against the real backing mapping.
    MemoryBasicInformation info;
    if (query_from_proc_maps(lpAddress, &info)) {
        if (lpflOldProtect) *lpflOldProtect = info.protect;
        return 1;
    }
    return 0;
}

} // namespace ThetaZ
