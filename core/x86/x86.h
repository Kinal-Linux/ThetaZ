// ThetaZ — 32-bit x86 guest interpreter
// Copyright (c) 2026 ThetaZ Project. MIT License.

#pragma once
#include <cstdint>
#include <cstddef>

namespace ThetaZ {

// Register indices (standard x86 encoding order).
enum X86Reg : uint8_t {
    XR_EAX = 0,
    XR_ECX = 1,
    XR_EDX = 2,
    XR_EBX = 3,
    XR_ESP = 4,
    XR_EBP = 5,
    XR_ESI = 6,
    XR_EDI = 7,
};

// 32-bit guest CPU state.
struct X86CpuState {
    uint32_t regs[8] = {};
    uint32_t eip = 0;
    uint32_t eflags = 0x00000002; // reserved bit 1 always set

    // Segment bases (flat model: only FS/GS carry a real base; FS points at the
    // guest TEB for 32-bit code).
    uint32_t fs_base = 0;
    uint32_t gs_base = 0;
    uint16_t seg[6] = {};

    // x87 FPU (implemented on host doubles for correctness).
    double fpu_st[8] = {};
    int fpu_top = 0;
    uint16_t fpu_cw = 0x037F;
    uint16_t fpu_sw = 0;

    // SSE/SSE2 registers: 8 x 128-bit. Each entry is {lo, hi}.
    uint64_t xmm[8][2] = {};

    // Instrumentation / control.
    uint64_t insn_count = 0;
    bool halted = false;
    uint32_t exit_code = 0;
};

struct X86RunOptions {
    uint64_t max_insns = 0; // 0 = unlimited
    bool trace = false;
};

// Execute guest code from state.eip until a HLT / explicit halt or a fault.
// Returns 0 on clean stop, nonzero on fault/trap.
int x86_run(X86CpuState& state, const X86RunOptions& opts);

// ---------------------------------------------------------------------------
// 32-bit guest environment setup
// ---------------------------------------------------------------------------

// A small guest memory reservation (host allocation). Used for stack/TEB/PEB.
struct X86GuestRegion {
    uintptr_t host_addr; // host base of the allocation
    uint32_t guest_addr; // guest (identity) base of the allocation
    uint32_t size;
};

// Allocate a guest-visible region below 4GB (identity mapped). Returns the
// guest address or 0 on failure.
uint32_t x86_alloc_guest(uint32_t guest_addr, uint32_t size);

// Set up the 32-bit guest environment: stack + TEB + PEB + process parameters.
// Returns 0 on failure. On success *stack_top and *teb_addr are filled in.
int x86_setup_environment(uint32_t image_base, uint32_t stack_size,
                          uint32_t* stack_top, uint32_t* teb_addr);

// Register a 32-bit PE image's TLS directory.  The raw template and callback
// list are copied now so they remain valid after the image is unloaded.  A
// fresh TLS index is assigned and written to AddressOfIndex.  Safe to call
// after guest threads exist: every live thread gets its own data block.
bool x86_tls_register_module(uint32_t image_base, uint32_t image_size,
                             uint32_t raw_start, uint32_t raw_end,
                             uint32_t address_of_index,
                             uint32_t address_of_callbacks,
                             uint32_t size_of_zero_fill);

// Attach a new 32-bit thread's TLS array to every registered module and copy
// each module's raw TLS template into the thread's private data blocks.
void x86_tls_init_thread(uint32_t tls_array);

// Invoke registered PE TLS callbacks for the current guest thread.  reason is
// DLL_PROCESS_ATTACH (1), DLL_THREAD_ATTACH (2), DLL_THREAD_DETACH (3) or
// DLL_PROCESS_DETACH (0).  primary_image_base selects the module that owns the
// process being started (0 means all registered modules).
void x86_tls_run_callbacks(uint32_t reason, uint32_t primary_image_base,
                           uint32_t stack_top, uint32_t teb_addr);

// Route one "run" from a fresh entry point.
int x86_run_entry(uint32_t entry_point);

// Run a 32-bit guest thread (child process primary thread or spawned thread)
// via the interpreter with its own stack and TEB. `param` is the lpParameter
// passed to the Windows thread procedure. Returns the guest exit code.
int x86_run_guest_thread(uint32_t entry_point, uint32_t param,
                         uint64_t stack_base, uint32_t stack_size, uint32_t tid);

// Run a guest callback (e.g. a timer-queue WAITORTIMERCALLBACK) to completion
// on a fresh per-thread guest context (own stack, TEB, TLS). The callback gets
// `nargs` pushed 32-bit arguments and returns in EAX. Safe to call from a host
// thread that has no guest state of its own.
uint32_t x86_run_guest_callback(uint32_t fn, const uint32_t* args, int nargs,
                                uint32_t tid);

// Call a guest cdecl function (no arguments) to completion from host code.
// Used when the host API layer must invoke a guest callback (atexit handlers,
// enumeration callbacks, ...). Returns 0 on success.
int x86_call_guest(uint32_t fn);

// Call a guest function with up to 8 32-bit arguments from host code. Used by
// the API layer to invoke guest exception handlers (SEH dispatch). The guest
// TEB is installed as fs base so fs-relative access works. Returns EAX.
uint32_t x86_call_guest_args(uint32_t fn, const uint32_t* args, int nargs);
// Variant that overrides the guest EBP before the call (MSVC SEH filters and
// __finally blocks run with the frame's saved EBP). ebp == 0 keeps the caller's.
uint32_t x86_call_guest_args_ebp(uint32_t fn, const uint32_t* args, int nargs, uint32_t ebp);
// Variant that also seeds EAX. MSVC's x86 C++ handler chain receives this
// function's FuncInfo in EAX (the per-function stub loads it, __CxxFrameHandler3
// forwards it as a fifth argument), so a real MSVC guest handler needs EAX set
// or it cannot find its try/catch data. eax_value == 0 delegates to the
// x86_call_guest_args_ebp behaviour and keeps classic __except_handler3/4
// paths unchanged.
uint32_t x86_call_guest_handler(uint32_t fn, const uint32_t* args, int nargs,
                                uint32_t eax_value);

// ---------------------------------------------------------------------------
// Win32 DISPATCHER_CONTEXT offsets (32-bit layout, verified against the real
// MSVC __CxxFrameHandler3 in vcruntime140.dll).
//
// This is the SINGLE SOURCE OF TRUTH for the struct the emulator builds in
// guest memory before invoking an exception handler. The previous code wrote
// its own private offsets, so a real MSVC guest handler read HandlerData as
// zero and silently returned ExceptionContinueSearch.
//
// Layout (8 x 32-bit fields => 0x20 bytes total).
// ---------------------------------------------------------------------------
namespace seh {
constexpr uint32_t kDcControlPc       = 0x00; // ControlPc (RVA/VA of the fault)
constexpr uint32_t kDcImageBase       = 0x04; // ImageBase of the faulting image
constexpr uint32_t kDcFunctionEntry   = 0x08; // RUNTIME_FUNCTION*
constexpr uint32_t kDcEstablisherFrame= 0x0C; // EstablisherFrame (the SEH record)
constexpr uint32_t kDcTargetIp        = 0x10; // continuation address chosen
constexpr uint32_t kDcContextRecord   = 0x14; // CONTEXT*
constexpr uint32_t kDcLanguageHandler = 0x18; // PEXCEPTION_ROUTINE
constexpr uint32_t kDcHandlerData     = 0x1C; // FuncInfo / scopetable
constexpr uint32_t kDcSize            = 0x20;
static_assert(kDcHandlerData + 4 == kDcSize, "x86 DISPATCHER_CONTEXT is 0x20 bytes");
} // namespace seh

// A guest SEH handler (__except_handler3 / __CxxFrameHandler) claimed an
// exception and rewrote the guest context (Eip/Esp/Ebp/Edx) to continue in
// its __except block / C++ catch funclet. The API dispatch layer applies this
// to the running interpreter state. Edx carries the exception object pointer
// for the frameless MSVC x86 catch-funclet ABI.
struct X86SehResume {
    bool active = false;
    uint32_t eip = 0;
    uint32_t esp = 0;
    uint32_t ebp = 0;
    uint32_t edx = 0;
};
X86SehResume& x86_seh_resume_state();

// Deliver a real hardware fault to the guest SEH chain. `instruction` is the
// guest address of the faulting instruction and `esp` the guest stack pointer
// at the start of that instruction. Returns true when a guest handler claimed
// the exception; x86_seh_resume_state() then holds the continuation. Returns
// false when no handler accepted it (the caller must terminate the guest).
bool x86_raise_access_violation(uint32_t fault_addr, uint32_t instruction,
                                uint32_t esp, bool write);

// Deliver an arbitrary hardware exception (SIGFPE and friends) to the guest
// SEH chain with the supplied ExceptionInformation words. Same return
// contract as x86_raise_access_violation().
bool x86_raise_guest_exception(uint32_t code, uint32_t instruction,
                               uint32_t esp, uint32_t nargs,
                               const uint32_t* info);

// The interpreter CPU state currently being executed (valid while a guest API
// call is being dispatched). Used by the API layer to build an exception
// context for SEH dispatch.
X86CpuState* x86_current_state();

// Set the return sentinel used by host-initiated guest callbacks
// (x86_call_guest / x86_run_guest_callback). Nested interpreter runs ret to the
// sentinel to halt.
void x86_set_callback_sentinel(uint32_t sentinel);

// Current callback sentinel (nested callbacks save/restore it)
uint32_t x86_prev_callback_sentinel();

} // namespace ThetaZ
