#include <cstdio>
#include <cstring>
#include <string>

#include "core/api/api.h"
#include "core/api/path_resolver.h"
#include "core/api/kernel32.h"
#include "core/api/ntdll_api.h"
#include "core/api/mfplat.h"
#include "core/loader/loader.h"

#include <thread>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <cctype>
#include <cstring>

using ThetaZ::LoadedModule;
using ThetaZ::PELoader;

#include <sys/mman.h>

namespace ThetaZ {

namespace {
std::string normalize_dll(std::string dll) {
    std::transform(dll.begin(), dll.end(), dll.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    // Ensure .dll extension so "ucrtbase" matches "ucrtbase.dll" registrations.
    if (dll.size() >= 4 && dll.substr(dll.size() - 4) == ".dll") return dll;
    // Don't append for special forms like api-ms-win-* (already have extensions)
    auto dot = dll.find('.');
    if (dot == std::string::npos) dll += ".dll";
    return dll;
}

// Base address of the main loaded image (for GetModuleHandleA(NULL)).
uint64_t g_main_module_base = 0;

// Whether the main image is a 32-bit (PE32) guest. Struct layouts passed to
// guest code (PROCESS_INFORMATION, etc.) differ between the 32-bit and 64-bit
// guest worlds, so API implementations must pick the right one.
bool g_main_module_is_32bit = true;

} // namespace

// Shared low-memory bump allocator. Pages are identity-mapped below 4GB so a
// 32-bit guest (whose world is identity-mapped) can dereference the returned
// pointers, while the 64-bit guest simply uses them as host pointers.
// Region 0x7D000000..0x7DF00000 is clear of the PEB/TEB (0x7FFD+), the thunk
// region (0x76000000) and the 32-bit guest stack (0x00F00000).
namespace {
constexpr uintptr_t kSharedLowBase = 0x7D000000u;
constexpr size_t kSharedLowMax = 0x00F00000u;  // 15 MB
} // namespace

void* alloc_guest_shared(size_t size) {
    static std::mutex mtx;
    static uintptr_t cursor = kSharedLowBase;
    std::lock_guard<std::mutex> lock(mtx);
    size = (size + 0xFFFu) & ~size_t(0xFFFu);
    if (size == 0) size = 0x1000u;
    if (cursor + size > kSharedLowBase + kSharedLowMax) return nullptr;
    void* p = mmap(reinterpret_cast<void*>(cursor), size,
                   PROT_READ | PROT_WRITE | PROT_EXEC,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (p == MAP_FAILED) return nullptr;
    cursor += size;
    return p;
}

// ---------------------------------------------------------------------------
// Guest-visible heap (below 4GB)
// ---------------------------------------------------------------------------
// First-fit free-list allocator over a fixed low-memory region. Blocks are
// contiguous; the free list is kept in address order so freeing can coalesce
// with neighbours. All payloads are 8-byte aligned.
namespace {
// 0x60000000..0x76000000 is clear of the thunk region (0x76000000), the shared
// low region (0x7D000000), the PEB/TEB (0x7FFD+) and the 32-bit image/stack.
constexpr uintptr_t kHeapLowBase = 0x60000000u;
constexpr size_t kHeapLowSize = 0x16000000u;  // 352 MB

struct HeapBlock {
    size_t size;      // payload bytes (multiple of 8), excluding the header
    HeapBlock* next;  // free-list next (address order)
    HeapBlock* prev;  // free-list prev (address order)
    bool free;
};
static_assert(sizeof(HeapBlock) % 8 == 0, "HeapBlock must stay 8-byte aligned");

HeapBlock*& heap_root() {
    static HeapBlock* root = [] {
        void* base = mmap(reinterpret_cast<void*>(kHeapLowBase), kHeapLowSize,
                          PROT_READ | PROT_WRITE | PROT_EXEC,
                          MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        if (base == MAP_FAILED) return static_cast<HeapBlock*>(nullptr);
        HeapBlock* b = static_cast<HeapBlock*>(base);
        b->size = kHeapLowSize - sizeof(HeapBlock);
        b->next = nullptr;
        b->prev = nullptr;
        b->free = true;
        return b;
    }();
    return root;
}

std::mutex& heap_mutex() {
    static std::mutex m;
    return m;
}

inline HeapBlock* block_of(void* p) {
    return reinterpret_cast<HeapBlock*>(static_cast<char*>(p) - sizeof(HeapBlock));
}

inline char* block_end(HeapBlock* b) {
    return reinterpret_cast<char*>(b + 1) + b->size;
}

inline bool in_heap_region(void* p) {
    if (!p) return false;
    uintptr_t a = reinterpret_cast<uintptr_t>(p);
    return a >= kHeapLowBase && a < kHeapLowBase + kHeapLowSize;
}
} // namespace

void* alloc_guest_heap(size_t size) {
    if (size == 0) size = 1;
    size = (size + 7u) & ~size_t(7u);
    std::lock_guard<std::mutex> lock(heap_mutex());
    HeapBlock*& root = heap_root();
    if (!root) {
        fprintf(stderr, "[heap] root mmap failed at 0x%x\n", (unsigned)kHeapLowBase);
        return nullptr;
    }
    for (HeapBlock* b = root; b != nullptr; b = b->next) {
        if (b->free && b->size >= size) {
            const size_t min_payload = sizeof(HeapBlock) + 8;
            if (b->size >= size + min_payload) {
                HeapBlock* rest = reinterpret_cast<HeapBlock*>(
                    reinterpret_cast<char*>(b + 1) + size);
                rest->size = b->size - size - sizeof(HeapBlock);
                rest->free = true;
                // rest takes b's place in the free list.
                rest->prev = b->prev;
                rest->next = b->next;
                if (rest->prev) rest->prev->next = rest;
                if (rest->next) rest->next->prev = rest;
                if (b == root) root = rest;
                b->size = size;
            } else {
                if (b->prev) b->prev->next = b->next;
                if (b->next) b->next->prev = b->prev;
                if (b == root) root = b->next;
            }
            b->free = false;
            b->next = nullptr;
            b->prev = nullptr;
            return b + 1;
        }
    }
    bool trace = getenv("THETAZ_TRACE_API") || getenv("THETAZ_TRACE_HEAP");
    if (trace || size >= 0x100000) {
        size_t free_count = 0;
        size_t free_bytes = 0;
        size_t biggest = 0;
        for (HeapBlock* b = root; b != nullptr; b = b->next) {
            if (b->free) {
                free_count++;
                free_bytes += b->size;
                if (b->size > biggest) biggest = b->size;
            }
        }
        fprintf(stderr,
                "[heap] alloc_guest_heap(%zu) FAILED  free_blocks=%zu free_bytes=%zu/%zu biggest=%zu\n",
                size, free_count, free_bytes, (size_t)kHeapLowSize, biggest);
    }
    return nullptr;
}

void free_guest_heap(void* p) {
    if (!p || !in_heap_region(p)) return;
    HeapBlock* b = block_of(p);
    std::lock_guard<std::mutex> lock(heap_mutex());
    HeapBlock*& root = heap_root();
    if (root == nullptr) return;
    b->free = true;
    // Coalesce with the physically-next block if it is also free.
    HeapBlock* after = reinterpret_cast<HeapBlock*>(block_end(b));
    for (HeapBlock* cur = root; cur != nullptr; cur = cur->next) {
        if (cur == after && cur->free) {
            if (cur->prev) cur->prev->next = cur->next;
            if (cur->next) cur->next->prev = cur->prev;
            if (root == cur) root = cur->next;
            b->size += sizeof(HeapBlock) + cur->size;
            break;
        }
    }
    // Insert into free list in address order, coalescing with the previous.
    HeapBlock* prev = nullptr;
    HeapBlock* cur = root;
    while (cur && cur < b) {
        prev = cur;
        cur = cur->next;
    }
    if (prev && block_end(prev) == reinterpret_cast<char*>(b)) {
        // merge b into prev
        prev->size += sizeof(HeapBlock) + b->size;
    } else {
        b->prev = prev;
        b->next = cur;
        if (prev) prev->next = b;
        if (cur) cur->prev = b;
        if (!prev) root = b;
    }
}

size_t HeapBlockSize(const void* p) {
    if (!p || !in_heap_region(const_cast<void*>(p))) return 0;
    HeapBlock* b = block_of(const_cast<void*>(p));
    std::lock_guard<std::mutex> lock(heap_mutex());
    return b->size;
}

void* realloc_guest_heap(void* p, size_t size) {
    if (!p) return alloc_guest_heap(size);
    if (!in_heap_region(p)) return alloc_guest_heap(size);
    if (size == 0) {
        free_guest_heap(p);
        return nullptr;
    }
    size = (size + 7u) & ~size_t(7u);
    HeapBlock* b = block_of(p);
    if (b->size >= size) return p;
    void* np = alloc_guest_heap(size);
    if (!np) return nullptr;
    memcpy(np, p, b->size);
    free_guest_heap(p);
    return np;
}

void* calloc_guest_heap(size_t count, size_t size) {
    if (size != 0 && count > static_cast<size_t>(-1) / size) return nullptr;
    size_t total = count * size;
    void* p = alloc_guest_heap(total);
    if (p) memset(p, 0, total);
    return p;
}

uint64_t get_main_module_base() {
    return g_main_module_base;
}

void set_main_module_base(uint64_t base) {
    g_main_module_base = base;
}

static std::mutex g_builtin_mutex;
static std::unordered_map<std::string, void*> g_builtin_handles;
static std::unordered_map<void*, std::string> g_builtin_names;
static std::atomic<uintptr_t> g_next_builtin_handle{0x78000000};

void* get_builtin_module_handle(const char* name) {
    if (!name || !name[0]) return nullptr;
    std::string n = name;
    for (auto& c : n) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    {
        std::lock_guard<std::mutex> lock(g_builtin_mutex);
        auto it = g_builtin_handles.find(n);
        if (it != g_builtin_handles.end()) return it->second;
    }
    void* h = reinterpret_cast<void*>(g_next_builtin_handle.fetch_add(0x1000));
    std::lock_guard<std::mutex> lock(g_builtin_mutex);
    g_builtin_handles[n] = h;
    g_builtin_names[h] = n;
    return h;
}

const char* builtin_handle_name(void* handle) {
    if (!handle) return nullptr;
    std::lock_guard<std::mutex> lock(g_builtin_mutex);
    auto it = g_builtin_names.find(handle);
    return it != g_builtin_names.end() ? it->second.c_str() : nullptr;
}

static std::string g_main_image_dir;

void set_main_image_dir(const char* dir) {
    g_main_image_dir = dir ? dir : "";
}

const std::string& get_main_image_dir() {
    return g_main_image_dir;
}

bool guest_is_32bit() {
    return g_main_module_is_32bit;
}

void set_guest_is_32bit(bool is_32bit) {
    g_main_module_is_32bit = is_32bit;
}

namespace {
// pid -> image base for spawned child processes. The main process (pid 0) uses
// g_main_module_base. Children are fresh image copies at their own base, and
// per-process state like the __security_cookie lives in each image's .data.
std::unordered_map<uint64_t, uint64_t> g_pid_image_bases;
std::mutex g_pid_image_bases_mutex;
} // namespace

void set_guest_process_image_base(uint64_t pid, uint64_t base) {
    std::lock_guard<std::mutex> lock(g_pid_image_bases_mutex);
    if (pid == 0) {
        g_main_module_base = base;
        return;
    }
    g_pid_image_bases[pid] = base;
}

uint64_t get_guest_process_image_base(uint64_t pid) {
    if (pid == 0) return g_main_module_base;
    std::lock_guard<std::mutex> lock(g_pid_image_bases_mutex);
    auto it = g_pid_image_bases.find(pid);
    return it != g_pid_image_bases.end() ? it->second : 0;
}

void release_guest_process_state(uint64_t pid) {
    if (pid == 0) return;
    {
        std::lock_guard<std::mutex> lock(g_pid_image_bases_mutex);
        g_pid_image_bases.erase(pid);
    }
    ThetaZ::release_fls_process(pid);
}

void* load_guest_module(const char* path) {
    if (!path || !path[0]) return nullptr;
    PELoader& loader = PELoader::instance();
    LoadedModule* module = loader.find_module_named(path);
    if (module) return reinterpret_cast<void*>(module->base);

    module = loader.load_image(path);
    if (!module) return nullptr;

    return reinterpret_cast<void*>(module->base);
}

// ---------------------------------------------------------------------------
// ApiRegistry
// ---------------------------------------------------------------------------
ApiRegistry& ApiRegistry::instance() {
    static ApiRegistry reg;
    return reg;
}

ApiRegistry::ApiRegistry() {}

void ApiRegistry::register_function(const std::string& dll, const std::string& name, void* func) {
    if (!func) return;
    by_name_[normalize_dll(dll)][name] = func;
}

void ApiRegistry::register_function_ordinal(const std::string& dll, uint16_t ordinal, void* func) {
    if (!func) return;
    by_ordinal_[normalize_dll(dll)][ordinal] = func;
}

void* ApiRegistry::find_function(const std::string& dll, const std::string& name) const {
    std::string ndll = normalize_dll(dll);
    // Direct hit first.
    auto dit = by_name_.find(ndll);
    if (dit != by_name_.end()) {
        auto it = dit->second.find(name);
        if (it != dit->second.end()) return it->second;
    }
    // Transparent forwarding (matches api_set_target in kernel32_runtime.cpp):
    //  - kernelbase.dll forwards to kernel32, ntdll, shlwapi, advapi32
    //  - api-ms-win-core-*/ext-ms-win-core-* alias into kernel32 (or ntdll)
    //  - api-ms-win-crt-*/ext-ms-win-crt-* alias into ucrtbase
    const char* targets[6] = { nullptr };
    int ntargets = 0;
    if (ndll == "kernelbase.dll") { targets[0] = "kernel32.dll"; targets[1] = "ntdll.dll"; targets[2] = "shlwapi.dll"; targets[3] = "advapi32.dll"; targets[4] = "version.dll"; ntargets = 5; }
    else if (ndll == "kernel32.dll") { targets[0] = "ntdll.dll"; ntargets = 1; }
    else if (ndll.compare(0, 15, "api-ms-win-core") == 0) { targets[0] = "kernel32.dll"; targets[1] = "ntdll.dll"; targets[2] = "shlwapi.dll"; ntargets = 3; }
    else if (ndll.compare(0, 15, "ext-ms-win-core") == 0) { targets[0] = "kernel32.dll"; targets[1] = "ntdll.dll"; ntargets = 2; }
    else if (ndll.compare(0, 14, "api-ms-win-crt") == 0) { targets[0] = "ucrtbase.dll"; ntargets = 1; }
    else if (ndll.compare(0, 14, "ext-ms-win-crt") == 0) { targets[0] = "ucrtbase.dll"; ntargets = 1; }
    else if (ndll == "vcruntime140.dll" || ndll == "vcruntime140_1.dll" ||
             ndll.compare(0, 7, "msvcrt") == 0) {
        targets[0] = "ucrtbase.dll"; ntargets = 1;
    }
    else if (ndll == "shlwapi.dll") { targets[0] = "kernelbase.dll"; targets[1] = "kernel32.dll"; ntargets = 2; }
    else if (ndll == "advapi32.dll" || ndll == "advapi32") { targets[0] = "kernel32.dll"; targets[1] = "ntdll.dll"; targets[2] = "ucrtbase.dll"; ntargets = 3; }
    else if (ndll == "shell32.dll" || ndll == "shell32") { targets[0] = "kernel32.dll"; targets[1] = "kernelbase.dll"; targets[2] = "shlwapi.dll"; ntargets = 3; }
    else if (ndll == "comctl32.dll" || ndll == "comctl32") { targets[0] = "gdi32.dll"; ntargets = 1; }
    else if (ndll == "comdlg32.dll" || ndll == "comdlg32") { targets[0] = "kernel32.dll"; ntargets = 1; }
    for (int ti = 0; ti < ntargets; ti++) {
        const char* target = targets[ti];
        if (!target || std::string(target) == ndll) continue;
        auto tit = by_name_.find(std::string(target));
        if (tit != by_name_.end()) {
            auto it = tit->second.find(name);
            if (it != tit->second.end()) return it->second;
        }
    }
    fprintf(stderr, "[api-lookup] Function not found: %s!%s (DLL has %zu functions)\n",
            dll.c_str(), name.c_str(), dit != by_name_.end() ? dit->second.size() : 0);
    return nullptr;
}

void* ApiRegistry::find_function_ordinal(const std::string& dll, uint16_t ordinal) const {
    std::string ndll = normalize_dll(dll);
    auto dit = by_ordinal_.find(ndll);
    if (dit != by_ordinal_.end()) {
        auto it = dit->second.find(ordinal);
        if (it != dit->second.end()) return it->second;
    }
    const char* targets[8] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    int ntargets = 0;
    if (ndll == "kernelbase.dll") { targets[0] = "kernel32.dll"; targets[1] = "ntdll.dll"; targets[2] = "shlwapi.dll"; ntargets = 3; }
    else if (ndll.compare(0, 15, "api-ms-win-core") == 0) { targets[0] = "kernel32.dll"; targets[1] = "ntdll.dll"; ntargets = 2; }
    else if (ndll.compare(0, 15, "ext-ms-win-core") == 0) { targets[0] = "kernel32.dll"; targets[1] = "ntdll.dll"; ntargets = 2; }
    else if (ndll.compare(0, 14, "api-ms-win-crt") == 0) { targets[0] = "ucrtbase.dll"; ntargets = 1; }
    else if (ndll.compare(0, 14, "ext-ms-win-crt") == 0) { targets[0] = "ucrtbase.dll"; ntargets = 1; }
    else if (ndll == "vcruntime140.dll" || ndll == "vcruntime140_1.dll" ||
             ndll.compare(0, 7, "msvcrt") == 0) {
        targets[0] = "ucrtbase.dll"; ntargets = 1;
    }
    else if (ndll == "ole32.dll" || ndll == "ole32") { targets[0] = "kernel32.dll"; targets[1] = "ntdll.dll"; targets[2] = "shlwapi.dll"; ntargets = 3; }
    else if (ndll == "user32.dll" || ndll == "user32") { targets[0] = "kernel32.dll"; targets[1] = "ntdll.dll"; ntargets = 2; }
    else if (ndll == "gdi32.dll" || ndll == "gdi32") { targets[0] = "kernel32.dll"; targets[1] = "ntdll.dll"; ntargets = 2; }
    else if (ndll == "advapi32.dll" || ndll == "advapi32") { targets[0] = "kernel32.dll"; targets[1] = "ntdll.dll"; targets[2] = "ucrtbase.dll"; ntargets = 3; }
    else if (ndll == "shell32.dll" || ndll == "shell32") { targets[0] = "kernel32.dll"; targets[1] = "kernelbase.dll"; targets[2] = "shlwapi.dll"; ntargets = 3; }
    else if (ndll == "msvcrt.dll" || ndll == "msvcrt") { targets[0] = "ucrtbase.dll"; ntargets = 1; }
    else if (ndll == "version.dll" || ndll == "version") { targets[0] = "kernel32.dll"; targets[1] = "ntdll.dll"; ntargets = 2; }
    else if (ndll == "oleaut32.dll" || ndll == "oleaut32") { targets[0] = "kernel32.dll"; targets[1] = "ntdll.dll"; targets[2] = "ole32.dll"; ntargets = 3; }
    else if (ndll == "comctl32.dll" || ndll == "comctl32") { targets[0] = "user32.dll"; targets[1] = "gdi32.dll"; ntargets = 2; }
    else if (ndll == "comdlg32.dll" || ndll == "comdlg32") { targets[0] = "kernel32.dll"; targets[1] = "ntdll.dll"; ntargets = 2; }
    else if (ndll == "mso.dll" || ndll == "mso") { targets[0] = "oleaut32.dll"; targets[1] = "ole32.dll"; targets[2] = "kernel32.dll"; ntargets = 3; }
    else if (ndll == "mso20win32client.dll" || ndll == "mso20win32client") { targets[0] = "oleaut32.dll"; targets[1] = "ole32.dll"; targets[2] = "kernel32.dll"; ntargets = 3; }
    else if (ndll == "mso30win32client.dll" || ndll == "mso30win32client") { targets[0] = "oleaut32.dll"; targets[1] = "ole32.dll"; targets[2] = "kernel32.dll"; ntargets = 3; }
    else if (ndll == "pagelayout.dll" || ndll == "pagelayout") { targets[0] = "kernel32.dll"; targets[1] = "ntdll.dll"; targets[2] = "gdi32.dll"; ntargets = 3; }
    else if (ndll == "mspts70.dll" || ndll == "mspts70") { targets[0] = "kernel32.dll"; targets[1] = "ntdll.dll"; ntargets = 2; }
    for (int ti = 0; ti < ntargets; ti++) {
        const char* target = targets[ti];
        if (!target || std::string(target) == ndll) continue;
        auto tit = by_ordinal_.find(std::string(target));
        if (tit != by_ordinal_.end()) {
            auto it = tit->second.find(ordinal);
            if (it != tit->second.end()) return it->second;
        }
    }
    return nullptr;
}

const char* ApiRegistry::name_for_ordinal(const std::string& dll, uint16_t ordinal) const {
    const std::string ndll = normalize_dll(dll);
    auto dit = by_ordinal_.find(ndll);
    if (dit == by_ordinal_.end()) return nullptr;
    auto oit = dit->second.find(ordinal);
    if (oit == dit->second.end()) return nullptr;
    // Match the ordinal's function pointer against the by-name table so the
    // real export name can be recovered (for signature-table lookups).
    auto ndit = by_name_.find(ndll);
    if (ndit == by_name_.end()) return nullptr;
    for (auto& pair : ndit->second) {
        if (pair.second == oit->second) return pair.first.c_str();
    }
    return nullptr;
}

bool ApiRegistry::has_dll(const std::string& dll) const {
    const std::string ndll = normalize_dll(dll);
    return by_name_.find(ndll) != by_name_.end() || by_ordinal_.find(ndll) != by_ordinal_.end();
}

void register_api(const std::string& dll, const std::string& name, void* func) {
    ApiRegistry::instance().register_function(dll, name, func);
}

void register_api_ordinal(const std::string& dll, uint16_t ordinal, void* func) {
    ApiRegistry::instance().register_function_ordinal(dll, ordinal, func);
}

void initialize_api_layer() {
    initialize_kernel32_api();
    initialize_oleaut32_layer();
    initialize_ws2_32_layer();
    initialize_ntdll_api();
    initialize_mfplat_layer();

    // Register missing OLEAUT32 ordinals that WINWORD needs
    // These are registered here to ensure they're available even if oleaut32.cpp registration
    // happens after the initial imports are resolved
    using ThetaZ::THETAZ_SafeArrayCreate;
    using ThetaZ::THETAZ_SafeArrayDestroy;
    using ThetaZ::THETAZ_SafeArrayGetDim;
    using ThetaZ::THETAZ_SafeArrayGetLBound;
    using ThetaZ::THETAZ_VarBstrFromUI1;
    using ThetaZ::THETAZ_VarBstrFromI2;
    using ThetaZ::THETAZ_VarBstrFromI4;
    using ThetaZ::THETAZ_VarBstrFromR4;
    using ThetaZ::THETAZ_VarBstrFromR8;
    using ThetaZ::THETAZ_VarBstrFromI8;

    register_api_ordinal("oleaut32.dll", 17, (void*)THETAZ_SafeArrayCreate);
    register_api_ordinal("oleaut32.dll", 18, (void*)THETAZ_SafeArrayDestroy);
    register_api_ordinal("oleaut32.dll", 21, (void*)THETAZ_SafeArrayGetDim);
    register_api_ordinal("oleaut32.dll", 25, (void*)THETAZ_SafeArrayGetLBound);
    register_api_ordinal("oleaut32.dll", 149, (void*)THETAZ_VarBstrFromUI1);
    register_api_ordinal("oleaut32.dll", 152, (void*)THETAZ_VarBstrFromI2);
    register_api_ordinal("oleaut32.dll", 153, (void*)THETAZ_VarBstrFromI4);
    register_api_ordinal("oleaut32.dll", 155, (void*)THETAZ_VarBstrFromR4);
    register_api_ordinal("oleaut32.dll", 162, (void*)THETAZ_VarBstrFromR8);
    register_api_ordinal("oleaut32.dll", 176, (void*)THETAZ_VarBstrFromI8);
}

} // namespace ThetaZ
