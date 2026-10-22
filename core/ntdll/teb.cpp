// ThetaZ — Copyright (c) 2026 ThetaZ Project. MIT License.

#include "core/ntdll/teb.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <asm/prctl.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <cerrno>

namespace ThetaZ {

std::vector<void*> g_allocations;

void* alloc_zero(size_t size) {
    void* p = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) return nullptr;
    memset(p, 0, size);
    g_allocations.push_back(p);
    return p;
}

uint64_t g_main_peb = 0;
uint64_t g_main_teb = 0;
uint64_t g_ldr_addr = 0;

uint64_t get_main_peb() { return g_main_peb; }
uint64_t get_main_teb() { return g_main_teb; }

bool set_gs_base(uint64_t address) {
    if (syscall(SYS_arch_prctl, ARCH_SET_GS, address) != 0) {
        perror("arch_prctl(ARCH_SET_GS)");
        return false;
    }
    return true;
}

static uint64_t write_unicode_string(uint8_t* params, size_t offset,
                                     const std::vector<wchar_t>& str) {
    // UNICODE_STRING: Length, MaximumLength, padding, Buffer
    size_t bytes = str.empty() ? 0 : (str.size() - 1) * sizeof(wchar_t);
    uint16_t length = static_cast<uint16_t>(bytes);
    uint16_t max_length = static_cast<uint16_t>(str.size() * sizeof(wchar_t));

    wchar_t* buffer = static_cast<wchar_t*>(alloc_zero(str.size() * sizeof(wchar_t)));
    for (size_t i = 0; i < str.size(); i++) buffer[i] = str[i];

    uint8_t* us = params + offset;
    *reinterpret_cast<uint16_t*>(us) = length;
    *reinterpret_cast<uint16_t*>(us + 2) = max_length;
    *reinterpret_cast<uint64_t*>(us + 8) = reinterpret_cast<uint64_t>(buffer);
    return reinterpret_cast<uint64_t>(buffer);
}

uint64_t setup_windows_teb(uint64_t image_base, const wchar_t* command_line) {
    uint8_t* peb = static_cast<uint8_t*>(alloc_zero(peb_offsets::PEB_SIZE));
    uint8_t* teb = static_cast<uint8_t*>(alloc_zero(teb_offsets::TEB_SIZE));
    if (!peb || !teb) return 0;

    g_main_peb = reinterpret_cast<uint64_t>(peb);
    g_main_teb = reinterpret_cast<uint64_t>(teb);

    // --- PEB ---
    *reinterpret_cast<uint8_t*>(peb + 0x00) = 0;  // InheritedAddressSpace
    *reinterpret_cast<uint8_t*>(peb + 0x02) = 0;  // BeingDebugged
    *reinterpret_cast<uint64_t*>(peb + peb_offsets::ImageBaseAddress) = image_base;
    *reinterpret_cast<uint32_t*>(peb + peb_offsets::SessionId) = 1;

    // --- PEB_LDR_DATA ---
    // Allocate and initialize PEB_LDR_DATA
    uint8_t* ldr = static_cast<uint8_t*>(alloc_zero(0x1000));
    if (!ldr) return 0;
    *reinterpret_cast<uint64_t*>(peb + peb_offsets::Ldr) = reinterpret_cast<uint64_t>(ldr);

    // PEB_LDR_DATA layout:
    // 0x00: Length (ULONG)
    // 0x04: Initialized (BOOLEAN)
    // 0x08: SsHandle (HANDLE)
    // 0x10: InLoadOrderModuleList (LIST_ENTRY)
    // 0x20: InMemoryOrderModuleList (LIST_ENTRY)
    // 0x30: InInitializationOrderModuleList (LIST_ENTRY)
    *reinterpret_cast<uint32_t*>(ldr + 0x00) = 0x48;  // Length
    *reinterpret_cast<uint8_t*>(ldr + 0x04) = 1;       // Initialized = TRUE

    // Initialize the three LIST_ENTRY heads to point to themselves (empty lists)
    uint64_t* in_load_order = reinterpret_cast<uint64_t*>(ldr + 0x10);
    uint64_t* in_memory_order = reinterpret_cast<uint64_t*>(ldr + 0x20);
    uint64_t* in_init_order = reinterpret_cast<uint64_t*>(ldr + 0x30);
    in_load_order[0] = in_load_order[1] = reinterpret_cast<uint64_t>(ldr + 0x10);
    in_memory_order[0] = in_memory_order[1] = reinterpret_cast<uint64_t>(ldr + 0x20);
    in_init_order[0] = in_init_order[1] = reinterpret_cast<uint64_t>(ldr + 0x30);

    // Store the module list head for later use (add_module_to_ldr)
    // We'll use a global to track the ldr address
    g_ldr_addr = reinterpret_cast<uint64_t>(ldr);

    // ProcessParameters with command line
    uint8_t* params = static_cast<uint8_t*>(alloc_zero(params_offsets::PARAMS_SIZE));
    std::vector<wchar_t> cmd;
    if (command_line) {
        const wchar_t* p = command_line;
        while (*p) { cmd.push_back(*p++); }
    } else {
        cmd = {L't', L'h', L'e', L't', L'a', L'z'};
    }
    cmd.push_back(L'\0');

    write_unicode_string(params, params_offsets::CommandLine, cmd);
    write_unicode_string(params, params_offsets::ImagePathName, cmd);

    *reinterpret_cast<uint64_t*>(peb + peb_offsets::ProcessParameters) =
        reinterpret_cast<uint64_t>(params);

    // --- TEB ---
    *reinterpret_cast<uint64_t*>(teb + teb_offsets::Self) = reinterpret_cast<uint64_t>(teb);
    *reinterpret_cast<uint64_t*>(teb + teb_offsets::ProcessEnvironmentBlock) =
        reinterpret_cast<uint64_t>(peb);
    *reinterpret_cast<uint64_t*>(teb + teb_offsets::ClientId_UniqueProcess) = getpid();

    // Stack limits: use the current real stack via pthread
    pthread_attr_t attr;
    void* stack_addr = nullptr;
    size_t stack_size = 0;
    if (pthread_getattr_np(pthread_self(), &attr) == 0) {
        pthread_attr_getstack(&attr, &stack_addr, &stack_size);
        pthread_attr_destroy(&attr);
    }
    if (stack_addr) {
        *reinterpret_cast<uint64_t*>(teb + teb_offsets::StackBase) =
            reinterpret_cast<uint64_t>(stack_addr) + stack_size;
        *reinterpret_cast<uint64_t*>(teb + teb_offsets::StackLimit) =
            reinterpret_cast<uint64_t>(stack_addr);
        *reinterpret_cast<uint64_t*>(teb + teb_offsets::DeallocationStack) =
            reinterpret_cast<uint64_t>(stack_addr);
    }

    // Point GS at the TEB (Linux libc uses FS for its own TLS, so this is safe)
    if (!set_gs_base(reinterpret_cast<uint64_t>(teb))) {
        return 0;
    }

    // Add the main module to the LDR lists
    add_module_to_ldr(image_base, "thetaz.exe");

    // Debug: verify PEB Ldr field
    uint64_t* peb_ldr = reinterpret_cast<uint64_t*>(g_main_peb + peb_offsets::Ldr);
    fprintf(stderr, "[setup_windows_teb] PEB Ldr field = 0x%lx (expected 0x%lx)\n",
            *peb_ldr, g_ldr_addr);
    if (*peb_ldr) {
        uint64_t* head_load = reinterpret_cast<uint64_t*>(*peb_ldr + 0x10);
        fprintf(stderr, "[setup_windows_teb] InLoadOrderModuleList head = 0x%lx (Flink=0x%lx Blink=0x%lx)\n",
                *peb_ldr + 0x10, *head_load, *(head_load + 1));
    }

    return reinterpret_cast<uint64_t>(teb);
}

uint64_t setup_thread_teb(uint64_t peb, uint64_t tid,
                          uint64_t stack_base, uint64_t stack_limit) {
    uint8_t* teb = static_cast<uint8_t*>(alloc_zero(teb_offsets::TEB_SIZE));
    if (!teb) return 0;

    *reinterpret_cast<uint64_t*>(teb + teb_offsets::Self) = reinterpret_cast<uint64_t>(teb);
    *reinterpret_cast<uint64_t*>(teb + teb_offsets::ProcessEnvironmentBlock) = peb;
    *reinterpret_cast<uint64_t*>(teb + teb_offsets::ClientId_UniqueProcess) = getpid();
    *reinterpret_cast<uint64_t*>(teb + teb_offsets::ClientId_UniqueThread) = tid;
    *reinterpret_cast<uint64_t*>(teb + teb_offsets::ExceptionList) = 0;
    *reinterpret_cast<uint64_t*>(teb + teb_offsets::StackBase) = stack_base;
    *reinterpret_cast<uint64_t*>(teb + teb_offsets::StackLimit) = stack_limit;
    *reinterpret_cast<uint64_t*>(teb + teb_offsets::DeallocationStack) = stack_limit;
    return reinterpret_cast<uint64_t>(teb);
}

// Add a module to the PEB LDR module lists
void add_module_to_ldr(uint64_t base, const char* name) {
    if (!g_ldr_addr) return;

    uint8_t* ldr = reinterpret_cast<uint8_t*>(g_ldr_addr);

    // LDR_DATA_TABLE_ENTRY layout (x64):
    // 0x00: InLoadOrderLinks (LIST_ENTRY) - Flink, Blink
    // 0x10: InMemoryOrderLinks (LIST_ENTRY)
    // 0x20: InInitializationOrderLinks (LIST_ENTRY)
    // 0x30: DllBase (PVOID)
    // 0x38: EntryPoint (PVOID)
    // 0x40: SizeOfImage (ULONG)
    // 0x48: FullDllName (UNICODE_STRING) - 16 bytes
    // 0x58: BaseDllName (UNICODE_STRING) - 16 bytes
    // 0x68: Flags (ULONG)
    // 0x6C: LoadCount (USHORT)
    // 0x6E: TlsIndex (USHORT)
    // 0x70: HashLinks (LIST_ENTRY) or TimeDateStamp
    // 0x78: SectionPointer (PVOID)
    // 0x80: CheckSum (ULONG)
    // 0x84: TimeDateStamp (ULONG)
    // 0x88: LoadedImports (PVOID)
    // 0x90: EntryPointActivationContext (PVOID)
    // 0x98: PatchInformation (PVOID)

    // Allocate entry
    uint8_t* entry = static_cast<uint8_t*>(alloc_zero(0x100));
    if (!entry) return;

    // DllBase
    *reinterpret_cast<uint64_t*>(entry + 0x30) = base; // actual base
    *reinterpret_cast<uint64_t*>(entry + 0x38) = 0; // EntryPoint

    // BaseDllName (UNICODE_STRING at 0x58)
    std::vector<wchar_t> name_w;
    const char* p = name;
    while (*p) { name_w.push_back(static_cast<wchar_t>(*p++)); }
    name_w.push_back(L'\0');
    uint8_t* name_buf = static_cast<uint8_t*>(alloc_zero(name_w.size() * 2));
    for (size_t i = 0; i < name_w.size(); i++) {
        *reinterpret_cast<uint16_t*>(name_buf + i * 2) = name_w[i];
    }
    *reinterpret_cast<uint64_t*>(entry + 0x58) = reinterpret_cast<uint64_t>(name_buf); // Buffer
    *reinterpret_cast<uint16_t*>(entry + 0x58 + 8) = static_cast<uint16_t>((name_w.size() - 1) * 2); // Length
    *reinterpret_cast<uint16_t*>(entry + 0x58 + 10) = static_cast<uint16_t>(name_w.size() * 2); // MaximumLength

    // FullDllName (same as BaseDllName for now)
    *reinterpret_cast<uint64_t*>(entry + 0x48) = reinterpret_cast<uint64_t>(name_buf); // Buffer
    *reinterpret_cast<uint16_t*>(entry + 0x48 + 8) = static_cast<uint16_t>((name_w.size() - 1) * 2); // Length
    *reinterpret_cast<uint16_t*>(entry + 0x48 + 10) = static_cast<uint16_t>(name_w.size() * 2); // MaximumLength

    // Flags
    *reinterpret_cast<uint32_t*>(entry + 0x68) = 0x00000001; // LDRP_ENTRY_PROCESSED
    // LoadCount
    *reinterpret_cast<uint16_t*>(entry + 0x6C) = 0xFFFF;

    // Insert into the three lists
    // LIST_ENTRY heads in PEB_LDR_DATA:
    // 0x10: InLoadOrderModuleList
    // 0x20: InMemoryOrderModuleList
    // 0x30: InInitializationOrderModuleList
    // Each LIST_ENTRY has Flink at offset 0, Blink at offset 8

    uint64_t ldr_addr = g_ldr_addr;
    // InLoadOrderModuleList head at ldr + 0x10
    uint64_t* head_load = reinterpret_cast<uint64_t*>(g_ldr_addr + 0x10);
    // InMemoryOrderModuleList head at ldr + 0x20
    uint64_t* head_mem = reinterpret_cast<uint64_t*>(g_ldr_addr + 0x20);
    // InInitializationOrderModuleList head at ldr + 0x30
    uint64_t* head_init = reinterpret_cast<uint64_t*>(g_ldr_addr + 0x30);

    // Insert at the end of each list (before the head)
    // For InLoadOrderLinks at entry + 0x00
    uint64_t* flink_load = head_load;
    uint64_t* blink_load = reinterpret_cast<uint64_t*>(*head_load + 8); // head's Blink
    *reinterpret_cast<uint64_t*>(entry + 0x00) = reinterpret_cast<uint64_t>(flink_load); // Flink
    *reinterpret_cast<uint64_t*>(entry + 0x08) = reinterpret_cast<uint64_t>(blink_load); // Blink
    *reinterpret_cast<uint64_t*>(*head_load + 8) = reinterpret_cast<uint64_t>(entry); // head's Blink = entry
    *head_load = reinterpret_cast<uint64_t>(entry); // head's Flink = entry


    // InMemoryOrderLinks at entry + 0x10
    uint64_t* flink_mem = head_mem;
    uint64_t* blink_mem = reinterpret_cast<uint64_t*>(*head_mem + 8);
    *reinterpret_cast<uint64_t*>(entry + 0x10) = reinterpret_cast<uint64_t>(flink_mem);
    *reinterpret_cast<uint64_t*>(entry + 0x18) = reinterpret_cast<uint64_t>(blink_mem);
    *reinterpret_cast<uint64_t*>(*head_mem + 8) = reinterpret_cast<uint64_t>(entry);
    *head_mem = reinterpret_cast<uint64_t>(entry);

    // InInitializationOrderLinks at entry + 0x20
    uint64_t* flink_init = head_init;
    uint64_t* blink_init = reinterpret_cast<uint64_t*>(*head_init + 8);
    *reinterpret_cast<uint64_t*>(entry + 0x20) = reinterpret_cast<uint64_t>(flink_init);
    *reinterpret_cast<uint64_t*>(entry + 0x28) = reinterpret_cast<uint64_t>(blink_init);
    *reinterpret_cast<uint64_t*>(*head_init + 8) = reinterpret_cast<uint64_t>(entry);
    *head_init = reinterpret_cast<uint64_t>(entry);
}
} // namespace ThetaZ
