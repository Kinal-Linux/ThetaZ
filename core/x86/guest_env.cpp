// ThetaZ — 32-bit guest environment setup
// Copyright (c) 2026 ThetaZ Project. MIT License.
//
// Sets up the 32-bit guest world: a low (below-4GB) stack, TEB (fs:[0]),
// PEB, and process parameters. Guest memory is identity-mapped, so the host
// address of any allocation equals its 32-bit guest address.

#include "core/x86/x86.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <atomic>
#include <mutex>
#include <vector>
#include <unistd.h>
#include <sys/mman.h>

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

namespace ThetaZ {

namespace {

// Fixed 32-bit environment addresses (real Windows x86 uses the same PEB/TEB
// addresses; the process parameters live right below them).
constexpr uint32_t kX86StackBase = 0x00F00000; // stack grows down from here
constexpr uint32_t kX86StackSize = 0x00100000; // 1MB
constexpr uint32_t kX86TebAddr = 0x7FFF0000;
constexpr uint32_t kX86PebAddr = 0x7FFDF000;
constexpr uint32_t kX86ParamsAddr = 0x7FFE0000;
constexpr uint32_t kX86CmdlineAddr = 0x7FFE1000;

// Captured by x86_setup_environment for x86_run_entry.
uint32_t g_stack_top = 0;
uint32_t g_teb_addr = 0;
uint32_t g_peb_addr = 0;

// 32-bit TEB (x86) offsets.
enum {
    TEB_StackBase = 0x04,
    TEB_StackLimit = 0x08,
    TEB_Self = 0x18,
    TEB_Peb = 0x30,
    TEB_TlsPointer = 0x2C,   // ThreadLocalStoragePointer (TLS array)
    TEB_ClientId_Process = 0x40,
    TEB_ClientId_Thread = 0x44,
};

// 32-bit TLS layout.  The TEB's ThreadLocalStoragePointer points at a 4KB
// array with one slot per TLS-bearing module.  Each thread's module data
// blocks live in a dedicated low region (see tls_alloc_data) and are filled
// from the PE TLS raw-data template; this is required by the MSVC CRT's
// thread-safe static initialisation (TLS offset 8 = _Init_thread_epoch).
constexpr uint32_t kX86TlsArrayAddr = 0x7FFE3000;
constexpr uint32_t kX86TlsDataRegion = 0x7E000000; // below PEB/TEB, above shared

// 32-bit PEB (x86) offsets.
enum {
    PEB_ImageBaseAddress = 0x08,
    PEB_ProcessParameters = 0x10,
    PEB_ProcessHeap = 0x18,
    PEB_NumberOfProcessors = 0x64,
};

// 32-bit RTL_USER_PROCESS_PARAMETERS (x86) offsets.
enum {
    PARAMS_ImagePathName = 0x34, // UNICODE_STRING
    PARAMS_CommandLine = 0x3C,   // UNICODE_STRING
    PARAMS_Environment = 0x44,
};

void write_us32(uint32_t addr, uint16_t length, uint16_t max_length, uint32_t buffer) {
    *reinterpret_cast<uint16_t*>(static_cast<uintptr_t>(addr)) = length;
    *reinterpret_cast<uint16_t*>(static_cast<uintptr_t>(addr + 2)) = max_length;
    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(addr + 8)) = buffer;
}

} // namespace

uint32_t x86_alloc_guest(uint32_t guest_addr, uint32_t size) {
    if (guest_addr == 0 || size == 0) return 0;
    void* p = mmap(reinterpret_cast<void*>(static_cast<uintptr_t>(guest_addr)),
                   size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (p == MAP_FAILED ||
        reinterpret_cast<uintptr_t>(p) != guest_addr) {
        return 0;
    }
    return guest_addr;
}

namespace {

// PE TLS module registry.  A module's raw template is copied at registration
// time, so unloading a child image never leaves dangling TLS source data.
struct X86TlsModule {
    uint32_t index = 0;
    uint32_t image_base = 0;
    uint32_t image_size = 0;
    uint32_t raw_size = 0;
    uint32_t zero_fill = 0;
    uint32_t address_of_index = 0;
    std::vector<uint8_t> raw;
    std::vector<uint32_t> callbacks;
};

struct X86TlsThread {
    uint32_t array = 0;
    std::vector<uint32_t> data; // one guest data block per module index
};

std::mutex g_tls_mutex;
std::vector<X86TlsModule> g_tls_modules;
std::vector<X86TlsThread> g_tls_threads;
uint32_t g_tls_data_next = kX86TlsDataRegion;

bool tls_trace_enabled() {
    static int enabled = -1;
    if (enabled < 0) enabled = getenv("THETAZ_TRACE_TLS") ? 1 : 0;
    return enabled != 0;
}

uint32_t tls_alloc_data(uint32_t size) {
    constexpr uint32_t kTlsDataLimit = 0x7FF00000; // keep clear of PEB/TEB
    if (size == 0) size = 0x1000;
    size = (size + 0xFFFu) & ~0xFFFu;
    for (;;) {
        if (g_tls_data_next >= kTlsDataLimit ||
            size > kTlsDataLimit - g_tls_data_next) {
            return 0;
        }
        uint32_t addr = g_tls_data_next;
        g_tls_data_next += size;
        if (x86_alloc_guest(addr, size) != 0) return addr;
    }
}

uint32_t tls_module_data_size(const X86TlsModule& module) {
    uint64_t total = static_cast<uint64_t>(module.raw_size) + module.zero_fill;
    if (total == 0) total = 1;
    if (total > (16u << 20)) return 0; // reject absurd SizeOfZeroFill
    uint32_t size = static_cast<uint32_t>((total + 0xFFFu) & ~0xFFFu);
    return size ? size : 0x1000;
}

void tls_populate_thread(X86TlsThread& thread, const X86TlsModule& module) {
    uint32_t size = tls_module_data_size(module);
    uint32_t data = size ? tls_alloc_data(size) : 0;
    if (data == 0) {
        fprintf(stderr, "[Thetax86] TLS: failed to allocate data for module %u\n",
                module.index);
        thread.data.push_back(0);
        return;
    }
    if (!module.raw.empty()) {
        memcpy(reinterpret_cast<void*>(static_cast<uintptr_t>(data)),
               module.raw.data(), module.raw.size());
    }
    // Anonymous mappings are already zeroed; SizeOfZeroFill needs no write.
    *reinterpret_cast<uint32_t*>(
        static_cast<uintptr_t>(thread.array + module.index * 4)) = data;
    thread.data.push_back(data);
    if (tls_trace_enabled()) {
        fprintf(stderr,
                "[tls] thread array=0x%08x index=%u data=0x%08x raw=%u zero=%u\n",
                thread.array, module.index, data, module.raw_size,
                module.zero_fill);
    }
}

} // namespace

bool x86_tls_register_module(uint32_t image_base, uint32_t image_size,
                             uint32_t raw_start, uint32_t raw_end,
                             uint32_t address_of_index,
                             uint32_t address_of_callbacks,
                             uint32_t size_of_zero_fill) {
    if (image_base == 0 || image_size == 0) return false;

    std::lock_guard<std::mutex> lock(g_tls_mutex);
    if (g_tls_modules.size() >= 1024) {
        fprintf(stderr, "[Thetax86] TLS: module index space exhausted\n");
        return false;
    }

    X86TlsModule module;
    module.index = static_cast<uint32_t>(g_tls_modules.size());
    module.image_base = image_base;
    module.image_size = image_size;
    module.address_of_index = address_of_index;
    module.zero_fill = size_of_zero_fill;

    if (raw_start != 0 && raw_end > raw_start &&
        raw_start >= image_base && raw_end <= image_base + image_size) {
        module.raw_size = raw_end - raw_start;
        module.raw.resize(module.raw_size);
        memcpy(module.raw.data(),
               reinterpret_cast<const void*>(static_cast<uintptr_t>(raw_start)),
               module.raw_size);
    }

    if (address_of_callbacks >= image_base &&
        address_of_callbacks + 4 <= image_base + image_size) {
        for (unsigned i = 0; i < 256; i++) {
            uint32_t slot = address_of_callbacks + i * 4;
            if (slot + 4 > image_base + image_size) break;
            uint32_t fn = *reinterpret_cast<uint32_t*>(
                static_cast<uintptr_t>(slot));
            if (fn == 0) break;
            module.callbacks.push_back(fn);
        }
    }

    // Windows writes the assigned TLS index into the image's AddressOfIndex.
    // The field normally lives in the image's writable data section.
    if (address_of_index >= image_base &&
        address_of_index + 4 <= image_base + image_size) {
        *reinterpret_cast<uint32_t*>(
            static_cast<uintptr_t>(address_of_index)) = module.index;
    } else if (address_of_index != 0) {
        fprintf(stderr,
                "[Thetax86] TLS: AddressOfIndex 0x%08x outside image 0x%08x+0x%x\n",
                address_of_index, image_base, image_size);
    }

    if (tls_trace_enabled()) {
        fprintf(stderr,
                "[tls] register index=%u base=0x%08x size=0x%x raw=0x%08x..0x%08x "
                "(%u) index_va=0x%08x callbacks=0x%08x ncb=%zu zero=0x%x\n",
                module.index, image_base, image_size, raw_start, raw_end,
                module.raw_size, address_of_index, address_of_callbacks,
                module.callbacks.size(), size_of_zero_fill);
    }

    for (auto& thread : g_tls_threads) {
        tls_populate_thread(thread, module);
    }
    g_tls_modules.push_back(std::move(module));
    return true;
}

void x86_tls_init_thread(uint32_t tls_array) {
    if (tls_array == 0) return;
    std::lock_guard<std::mutex> lock(g_tls_mutex);
    for (const auto& thread : g_tls_threads) {
        if (thread.array == tls_array) return; // already attached
    }

    X86TlsThread thread;
    thread.array = tls_array;
    for (const auto& module : g_tls_modules) {
        tls_populate_thread(thread, module);
    }
    g_tls_threads.push_back(std::move(thread));
    if (tls_trace_enabled()) {
        fprintf(stderr, "[tls] attached array=0x%08x modules=%zu\n",
                tls_array, g_tls_modules.size());
    }
}


int x86_setup_environment(uint32_t image_base, uint32_t stack_size,
                          uint32_t* stack_top_out, uint32_t* teb_addr_out) {
    if (stack_size == 0) stack_size = kX86StackSize;
    uint32_t stack_base = kX86StackBase - stack_size;

    if (x86_alloc_guest(stack_base, stack_size) == 0) {
        fprintf(stderr, "[Thetax86] failed to allocate 32-bit stack\n");
        return 0;
    }
    if (x86_alloc_guest(kX86TebAddr, 0x1000) == 0) {
        fprintf(stderr, "[Thetax86] failed to allocate 32-bit TEB\n");
        return 0;
    }
    if (x86_alloc_guest(kX86PebAddr, 0x1000) == 0) {
        fprintf(stderr, "[Thetax86] failed to allocate 32-bit PEB\n");
        return 0;
    }
    if (x86_alloc_guest(kX86ParamsAddr, 0x1000) == 0) {
        fprintf(stderr, "[Thetax86] failed to allocate process parameters\n");
        return 0;
    }
    if (x86_alloc_guest(kX86CmdlineAddr, 0x1000) == 0) {
        fprintf(stderr, "[Thetax86] failed to allocate command line buffer\n");
        return 0;
    }

    uint32_t stack_top = stack_base + stack_size;
    g_stack_top = stack_top;
    g_teb_addr = kX86TebAddr;
    g_peb_addr = kX86PebAddr;

    // --- RTL_USER_PROCESS_PARAMETERS (x86) ---
    write_us32(kX86ParamsAddr + PARAMS_ImagePathName,
               sizeof(uint16_t) * 6, sizeof(uint16_t) * 7, kX86CmdlineAddr);
    write_us32(kX86ParamsAddr + PARAMS_CommandLine,
               sizeof(uint16_t) * 6, sizeof(uint16_t) * 7, kX86CmdlineAddr + 16);
    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(kX86ParamsAddr + PARAMS_Environment)) = 0;
    const wchar_t* cmd = L"thetaz";
    uint32_t cmd_addr = kX86CmdlineAddr + 16;
    for (int i = 0; i <= 6; i++) {
        *reinterpret_cast<uint16_t*>(static_cast<uintptr_t>(cmd_addr + i * 2)) = cmd[i];
    }

    // --- PEB (x86) ---
    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(kX86PebAddr + PEB_ImageBaseAddress)) = image_base;
    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(kX86PebAddr + PEB_ProcessParameters)) = kX86ParamsAddr;
    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(kX86PebAddr + PEB_ProcessHeap)) = 0;
    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(kX86PebAddr + PEB_NumberOfProcessors)) = 1;

    // --- 32-bit TLS setup ---
    // The TLS array points into per-thread module data blocks in the dedicated
    // low TLS region. Loader-registered PE TLS templates are copied here.
    if (x86_alloc_guest(kX86TlsArrayAddr, 0x1000) == 0) {
        fprintf(stderr, "[Thetax86] failed to allocate 32-bit TLS array\n");
        return 0;
    }
    memset(reinterpret_cast<void*>(static_cast<uintptr_t>(kX86TlsArrayAddr)),
           0, 0x1000);
    x86_tls_init_thread(kX86TlsArrayAddr);

    // --- TEB (x86) ---
    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(kX86TebAddr + TEB_Self)) = kX86TebAddr;
    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(kX86TebAddr + TEB_Peb)) = kX86PebAddr;
    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(kX86TebAddr + TEB_TlsPointer)) = kX86TlsArrayAddr;
    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(kX86TebAddr + TEB_StackBase)) = stack_top;
    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(kX86TebAddr + TEB_StackLimit)) = stack_base;
    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(kX86TebAddr + TEB_ClientId_Process)) =
        static_cast<uint32_t>(getpid());
    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(kX86TebAddr + TEB_ClientId_Thread)) = 1;

    if (stack_top_out) *stack_top_out = stack_top;
    if (teb_addr_out) *teb_addr_out = kX86TebAddr;
    return 1;
}

// Per-thread TEB blocks (0x4000 each: TLS array @ block, TLS data @ block+0x1000,
// TEB @ block+0x3000) allocated downward from 0x7FFD0000 (below the PEB at
// 0x7FFDF000, above the shared-low region at 0x7DF00000). SHARED between
// x86_run_guest_thread and x86_run_guest_callback so they never overlap.
namespace {
std::mutex g_teb_block_mutex;
uint32_t g_next_teb_block = 0x7FFD0000;
} // namespace

uint32_t x86_alloc_teb_block() {
    std::lock_guard<std::mutex> lock(g_teb_block_mutex);
    if (g_next_teb_block < 0x7DF00000 + 0x4000) return 0;
    uint32_t block = g_next_teb_block - 0x4000;
    if (x86_alloc_guest(block, 0x4000) == 0) return 0;
    g_next_teb_block = block;
    return block;
}

int x86_run_guest_thread(uint32_t entry_point, uint32_t param,
                         uint64_t stack_base, uint32_t stack_size, uint32_t tid) {
    uint32_t host_sp = 0;
    __asm__ volatile("movl %%esp, %0" : "=r"(host_sp));
    if (getenv("THETAZ_TRACE_STACK")) {
        fprintf(stderr, "[stack] tid=%u host_sp=0x%lx requested_base=0x%llx size=0x%x\n",
                tid, (unsigned long)host_sp, (unsigned long long)stack_base, stack_size);
    }
    uint32_t block = x86_alloc_teb_block();
    if (block == 0) {
        fprintf(stderr, "[Thetax86] x86_run_guest_thread: TEB alloc failed\n");
        return 1;
    }
    uint32_t teb = block + 0x3000;
    uint32_t tls_array = block;
    memset(reinterpret_cast<void*>(static_cast<uintptr_t>(block)), 0, 0x4000);
    x86_tls_init_thread(tls_array);

    // The guest is 32-bit: its stack must live below 4GB (identity-mapped).
    // ThreadManager mmaps stacks at host addresses, so allocate a low stack.
    // The zone grows DOWN from 0x20000000: it sits above the low Wine DLL
    // range (which tops out around 0x10000000) and below the re-mapped
    // relaunch images, so per-thread stacks never collide with loaded modules.
    // The old downward-from-0x00E00000 zone collided with i386 Wine DLLs
    // (e.g. webservices @ 0x00C00000) and with the main guest stack.
    if (stack_size == 0) stack_size = 1u << 20;
    if (stack_base == 0 || stack_base + stack_size > 0x100000000ull) {
        static std::atomic<uint32_t> next_stack{0x20000000};
        uint32_t base = next_stack.fetch_sub(stack_size) - stack_size;
        if (base < 0x18000000) base = 0x20000000 - stack_size;
        void* p = mmap(reinterpret_cast<void*>(static_cast<uintptr_t>(base)),
                       stack_size, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        if (p == MAP_FAILED) {
            fprintf(stderr, "[Thetax86] x86_run_guest_thread: stack alloc failed at 0x%08x size=0x%x\n",
                    base, stack_size);
            return 1;
        }
        stack_base = base;
    }
    uint32_t stack_top = static_cast<uint32_t>(stack_base) + stack_size;
    if (getenv("THETAZ_TRACE_STACK")) {
        fprintf(stderr, "[stack] tid=%u guest_base=0x%08x guest_top=0x%08x\n",
                tid, static_cast<uint32_t>(stack_base), stack_top);
    }

    // TEB (x86).
    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(teb + TEB_Self)) = teb;
    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(teb + TEB_Peb)) = kX86PebAddr;
    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(teb + TEB_TlsPointer)) = tls_array;
    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(teb + TEB_StackBase)) = stack_top;
    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(teb + TEB_StackLimit)) = stack_base;
    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(teb + TEB_ClientId_Process)) =
        static_cast<uint32_t>(getpid());
    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(teb + TEB_ClientId_Thread)) = tid;

    X86CpuState state{};
    state.eip = entry_point;
    state.regs[XR_ESP] = (stack_top - 0x40) & 0xFFFFFFF0u;
    state.fs_base = teb;
    state.gs_base = 0;
    state.seg[0] = 0; state.seg[1] = 0; state.seg[2] = 0;
    state.seg[3] = 0; state.seg[4] = 0; state.seg[5] = 0;

    // Windows calls the thread procedure as
    //   DWORD WINAPI ThreadProc(LPVOID lpParameter)
    // so the entry must see its parameter at [esp+4] and a return address at
    // [esp]. The sentinel stops the interpreter when ThreadProc returns and is
    // never executed; the guest's EAX becomes the thread exit code.
    const uint32_t sentinel = 0xCA11B020u;
    auto mw32 = [](uint32_t a, uint32_t v) {
        *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(a)) = v;
    };
    state.regs[XR_ESP] -= 4;
    mw32(state.regs[XR_ESP], param);
    state.regs[XR_ESP] -= 4;
    mw32(state.regs[XR_ESP], sentinel);

    x86_set_callback_sentinel(sentinel);
    X86RunOptions opts{};
    opts.trace = getenv("THETAZ_TRACE_INSNS") != nullptr;
    int rc = x86_run(state, opts);
    // The interpreter restores the previous sentinel on exit; leaving this
    // thread's sentinel installed would leak it into nested guest calls.
    x86_set_callback_sentinel(x86_prev_callback_sentinel());
    return rc;
}

uint32_t x86_run_guest_callback(uint32_t fn, const uint32_t* args, int nargs,
                                uint32_t tid) {
    if (!fn) return 0;
    if (nargs > 8) nargs = 8;
    if (!args) nargs = 0;

    // Fresh per-thread TEB (own fs base), TLS array/data and a low stack, same
    // layout as x86_run_guest_thread.
    uint32_t block = x86_alloc_teb_block();
    if (block == 0) return 0;
    uint32_t teb = block + 0x3000;
    uint32_t tls_array = block;
    memset(reinterpret_cast<void*>(static_cast<uintptr_t>(block)), 0, 0x4000);
    x86_tls_init_thread(tls_array);

    uint32_t stack_size = 1u << 20;
    // Callback stacks use their own sub-zone growing down from 0x18000000 so
    // they never collide with per-thread guest stacks (which grow down from
    // 0x20000000).
    static std::atomic<uint32_t> next_stack{0x18000000};
    uint32_t base = next_stack.fetch_sub(stack_size) - stack_size;
    if (base < 0x10000000) base = 0x18000000 - stack_size;
    void* p = mmap(reinterpret_cast<void*>(static_cast<uintptr_t>(base)),
                   stack_size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (p == MAP_FAILED) {
        fprintf(stderr, "[Thetax86] x86_run_guest_callback: stack alloc failed at 0x%08x\n", base);
        return 0;
    }
    uint32_t stack_top = base + stack_size;

    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(teb + TEB_Self)) = teb;
    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(teb + TEB_Peb)) = kX86PebAddr;
    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(teb + TEB_TlsPointer)) = tls_array;
    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(teb + TEB_StackBase)) = stack_top;
    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(teb + TEB_StackLimit)) = base;
    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(teb + TEB_ClientId_Process)) =
        static_cast<uint32_t>(getpid());
    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(teb + TEB_ClientId_Thread)) = tid;

    X86CpuState state{};
    state.eip = fn;
    state.regs[XR_ESP] = (stack_top - 0x40) & 0xFFFFFFF0u;
    state.fs_base = teb;
    state.gs_base = 0;
    state.seg[0] = 0; state.seg[1] = 0; state.seg[2] = 0;
    state.seg[3] = 0; state.seg[4] = 0; state.seg[5] = 0;

    const uint32_t sentinel = 0xCA11B010u;
    auto mw32 = [](uint32_t a, uint32_t v) {
        *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(a)) = v;
    };
    for (int i = nargs - 1; i >= 0; i--) {
        state.regs[XR_ESP] -= 4;
        mw32(state.regs[XR_ESP], args[i]);
    }
    state.regs[XR_ESP] -= 4;
    mw32(state.regs[XR_ESP], sentinel);

    x86_set_callback_sentinel(sentinel);
    X86RunOptions opts{};
    x86_run(state, opts);
    x86_set_callback_sentinel(x86_prev_callback_sentinel());
    return state.regs[XR_EAX];
}

int x86_run_entry(uint32_t entry_point) {
    if (g_stack_top == 0 || g_teb_addr == 0) {
        fprintf(stderr, "[Thetax86] x86_run_entry: environment not set up\n");
        return 1;
    }

    X86CpuState state{};
    state.eip = entry_point;
    // Start ESP a little below the top of the mapping: the mingw CRT reads its
    // own return address at [esp] during startup, and that read must land in
    // the allocated stack.
    state.regs[XR_ESP] = (g_stack_top - 0x40) & 0xFFFFFFF0u;
    state.fs_base = g_teb_addr;
    state.gs_base = 0;
    state.seg[0] = 0; // CS
    state.seg[1] = 0; // SS
    state.seg[2] = 0; // DS
    state.seg[3] = 0; // ES
    state.seg[4] = 0; // FS (base in fs_base)
    state.seg[5] = 0; // GS

    X86RunOptions opts{};
    opts.trace = getenv("THETAZ_TRACE_INSNS") != nullptr;
    int rc = x86_run(state, opts);
    if (rc != 0) {
        fprintf(stderr, "[Thetax86] interpreter stopped with rc=%d at eip=0x%08x\n",
                rc, state.eip);
    }
    return rc;
}

} // namespace ThetaZ
