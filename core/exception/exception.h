// ThetaZ — Copyright (c) 2026 ThetaZ Project. MIT License.

#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <unordered_map>
#include <utility>
#include <mutex>
#include <memory>

namespace ThetaZ {

// x64 Windows EXCEPTION_REGISTRATION frame layout (pushed via SEH registration)
namespace except_offsets {
    constexpr size_t Handler = 0x00;    // Address of exception handler function
    constexpr size_t Prev = 0x08;       // Previous exception registration (NULL = none)
}

// x64 Windows EXCEPTION_RECORD layout (first exception in a chain)
namespace except_record_offsets {
    constexpr size_t ExceptionCode = 0x00;    // uint32_t ExceptionCode
    constexpr size_t ExceptionFlags = 0x04;   // uint32_t ExceptionFlags
    constexpr size_t ExceptionRecord = 0x08;  // PEXCEPTION_RECORD (if nested)
    constexpr size_t ExceptionAddress = 0x10; // uint64_t ExceptionAddress
    constexpr size_t NumberParameters = 0x18; // uint32_t NumberParameters (number of DWORDs in Params)
    constexpr size_t ExceptionInformation = 0x1C; // const DWORD ExceptionInformation[15]
}

// x64 Windows TEB->ExceptionList is a singly-linked list of EXCEPTION_REGISTRATION frames
// First exception is at GS:[0x20], each frame points to the previous via Prev field.
class ExceptionRegistrationFrame {
public:
    explicit ExceptionRegistrationFrame(uint64_t frame_ptr)
        : frame_ptr_(frame_ptr) {}

    uint64_t get_handler() const {
        return *reinterpret_cast<uint64_t*>(frame_ptr_ + except_offsets::Handler);
    }

    uint64_t get_prev() const {
        return *reinterpret_cast<uint64_t*>(frame_ptr_ + except_offsets::Prev);
    }

    void set_prev(uint64_t prev) const {
        *reinterpret_cast<uint64_t*>(frame_ptr_ + except_offsets::Prev) = prev;
    }

private:
    uint64_t frame_ptr_;
};

enum class ExceptionCode : uint32_t {
    AccessViolation = 0xC0000005,
    IntegerDivideByZero = 0xC0000094,
    IntegerOverflow = 0xC0000095,
    FloatDivideByZero = 0xC0000096,
    BoundCheck = 0xC0000097,
    InvalidOpcode = 0xC000001D,
    StackOverflow = 0xC00000FD,
    InvalidHandle = 0xC0000008,
    InvalidParameter = 0xC000000D,
    ControlCExit = 0xC000013A,
    ExceptionContinueSearch = 0x00000000,
    ExceptionExecuteHandler = 0x00000008
};

struct CPUContext64 {
    uint64_t rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp_at_entry;
    uint64_t ss;
    uint64_t fs_base;
    uint64_t gs_base;
    uint64_t cr2;
    uint8_t xmm[16 * 16];

    CPUContext64() { memset(this, 0, sizeof(*this)); }
};

// Guest-visible Win64 structures. These layouts match the PE-visible types
// (CONTEXT, EXCEPTION_RECORD from winnt.h) exactly so guest SEH handlers can
// read and modify them through the pointers we pass.
struct M128A {
    uint64_t low;
    uint64_t high;
};

#pragma pack(push, 8)
struct alignas(16) WinContext {
    uint64_t p1_home;
    uint64_t p2_home;
    uint64_t p3_home;
    uint64_t p4_home;
    uint64_t p5_home;
    uint64_t p6_home;
    uint32_t context_flags;
    uint32_t mx_csr;
    uint16_t seg_cs;
    uint16_t seg_ds;
    uint16_t seg_es;
    uint16_t seg_fs;
    uint16_t seg_gs;
    uint16_t seg_ss;
    uint32_t eflags;
    uint64_t dr0;
    uint64_t dr1;
    uint64_t dr2;
    uint64_t dr3;
    uint64_t dr6;
    uint64_t dr7;
    uint64_t rax;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rbx;
    uint64_t rsp;
    uint64_t rbp;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rip;
    uint8_t flt_save_control[0x20];
    M128A float_registers[8];
    M128A xmm[16];
    uint8_t flt_save_reserved[96];
    M128A vector_register[26];
    uint64_t vector_control;
    uint64_t debug_control;
    uint64_t last_branch_to_rip;
    uint64_t last_branch_from_rip;
    uint64_t last_exception_to_rip;
    uint64_t last_exception_from_rip;
};

struct WinExceptionRecord {
    uint32_t exception_code;
    uint32_t exception_flags;
    WinExceptionRecord* exception_record;
    void* exception_address;
    uint32_t number_parameters;
    uint64_t exception_information[15];
};
#pragma pack(pop)

// Handler dispositions returned by guest SEH handlers.

class ExceptionHandler {
public:
    ExceptionHandler();
    ~ExceptionHandler();

    using ExceptionHandlerFunc = void(*)(ExceptionCode, const CPUContext64&, void* user_data);
    void register_handler(ExceptionCode code, ExceptionHandlerFunc handler, void* user_data = nullptr);

    const CPUContext64* get_current_context() const;
    void set_current_context(const CPUContext64& ctx, void* user_data = nullptr);

    static void unhandled_exception(ExceptionCode code, const CPUContext64& ctx, void* user_data);

    // Full unhandled-exception flow: host override hooks, then the guest SEH
    // chain, then the top-level filter (SetUnhandledExceptionFilter). Returns
    // true if a handler claimed the exception and fixed up `ctx` (resume at the
    // new RIP); false if nothing handled it (caller should terminate).
    bool dispatch(ExceptionCode code, CPUContext64& ctx, void* user_data = nullptr);

    // Walk the TEB->ExceptionList chain and dispatch to each guest handler.
    // Handlers run with the Win64 ABI and may fix up `ctx` (registers); on a
    // claim, the registers are copied back so the signal handler can resume at
    // the modified RIP.
    bool walk_exception_chain(ExceptionCode code, CPUContext64& ctx);

private:
    std::unordered_map<ExceptionCode, std::pair<ExceptionHandlerFunc, void*>> handlers_;
    CPUContext64 current_context_;
    mutable std::mutex mutex_;
};

class ExceptionSetup64 {
public:
    ExceptionSetup64(ExceptionHandler& handler);
    ~ExceptionSetup64();

private:
    ExceptionHandler& handler_;
};

// The handler instance that signal delivery dispatches to (set by ExceptionSetup64).
void set_active_exception_handler(ExceptionHandler* handler);
ExceptionHandler* get_active_exception_handler();

// SetUnhandledExceptionFilter: stores the guest top-level filter (called with an
// EXCEPTION_POINTERS* when no SEH frame claims the exception) and returns the
// previously installed filter.
void set_unhandled_exception_filter(void* filter);
void* get_unhandled_exception_filter();

} // namespace ThetaZ
