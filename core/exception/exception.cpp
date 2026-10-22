// ThetaZ — Critical Windows API implementations for real Windows app compatibility
// Copyright (c) 2026 ThetaZ Project. MIT License.

#include <cstdio>
#include <cstring>
#include <string>

#include "core/api/api.h"
#include "core/api/kernel32.h"
#include "core/ipc/thetaserver.h"
#include "core/exception/exception.h"
#include "core/ntdll/thread.h"
#include "core/ntdll/teb.h"

#include <thread>
#include <mutex>

namespace ThetaZ {

// Struct for vectored exception handler linked list
struct VectoredHandlerEntry {
    PVECTORED_EXCEPTION_HANDLER handler;
    VectoredHandlerEntry* next;
};

namespace {
VectoredHandlerEntry* g_exception_handlers = nullptr;
std::mutex g_exception_mutex;
} // namespace

// ---------------------------------------------------------------------------
// Vectored exception handlers (AddVectoredExceptionHandler and friends)
// ---------------------------------------------------------------------------

PVOID WINAPI_CALL WINAPI_AddVectoredExceptionHandler(ULONG first, PVECTORED_EXCEPTION_HANDLER handler) {
    (void)first;
    if (!handler) return nullptr;
    std::lock_guard<std::mutex> lock(g_exception_mutex);
    VectoredHandlerEntry* entry = new VectoredHandlerEntry{handler, g_exception_handlers};
    g_exception_handlers = entry;
    return reinterpret_cast<PVOID>(entry);
}

PVOID WINAPI_CALL WINAPI_AddVectoredContinueHandler(ULONG first, PVECTORED_EXCEPTION_HANDLER handler) {
    return WINAPI_AddVectoredExceptionHandler(first, handler);
}

BOOL WINAPI_CALL WINAPI_RemoveVectoredExceptionHandler(PVOID handle) {
    if (!handle) return FALSE;
    std::lock_guard<std::mutex> lock(g_exception_mutex);
    VectoredHandlerEntry* prev = nullptr;
    VectoredHandlerEntry* cur = g_exception_handlers;
    while (cur && cur != handle) {
        prev = cur;
        cur = cur->next;
    }
    if (!cur) return FALSE;
    if (prev) {
        prev->next = cur->next;
    } else {
        g_exception_handlers = cur->next;
    }
    delete cur;
    return TRUE;
}

BOOL WINAPI_CALL WINAPI_RemoveVectoredContinueHandler(PVOID handle) {
    return WINAPI_RemoveVectoredExceptionHandler(handle);
}

LONG WINAPI_CALL WINAPI_CallFirstVectoredExceptionHandler(void* exception_info) {
    std::lock_guard<std::mutex> lock(g_exception_mutex);
    VectoredHandlerEntry* entry = g_exception_handlers;
    while (entry) {
        LONG result = entry->handler(reinterpret_cast<EXCEPTION_POINTERS*>(exception_info));
        if (result != EXCEPTION_CONTINUE_SEARCH) {
            return result;
        }
        entry = entry->next;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

// ---------------------------------------------------------------------------
// Active exception handler singleton
// ---------------------------------------------------------------------------

namespace {
ExceptionHandler* g_active_exception_handler = nullptr;
void* g_unhandled_exception_filter = nullptr;
} // namespace

void set_active_exception_handler(ExceptionHandler* handler) {
    g_active_exception_handler = handler;
}

ExceptionHandler* get_active_exception_handler() {
    return g_active_exception_handler;
}

void set_unhandled_exception_filter(void* filter) {
    g_unhandled_exception_filter = filter;
}

void* get_unhandled_exception_filter() {
    return g_unhandled_exception_filter;
}

// ---------------------------------------------------------------------------
// ExceptionHandler
// ---------------------------------------------------------------------------

ExceptionHandler::ExceptionHandler() {}

ExceptionHandler::~ExceptionHandler() {}

void ExceptionHandler::register_handler(ExceptionCode code, ExceptionHandlerFunc handler, void* user_data) {
    std::lock_guard<std::mutex> lock(mutex_);
    handlers_[code] = std::make_pair(handler, user_data);
}

const CPUContext64* ExceptionHandler::get_current_context() const {
    return &current_context_;
}

void ExceptionHandler::set_current_context(const CPUContext64& ctx, void* user_data) {
    (void)user_data;
    std::lock_guard<std::mutex> lock(mutex_);
    current_context_ = ctx;
}

void ExceptionHandler::unhandled_exception(ExceptionCode code, const CPUContext64& ctx, void* user_data) {
    (void)user_data;
    fprintf(stderr, "Unhandled exception: 0x%08X at RIP=0x%llX\n",
            static_cast<uint32_t>(code),
            static_cast<unsigned long long>(ctx.rip));
}

bool ExceptionHandler::dispatch(ExceptionCode code, CPUContext64& ctx, void* user_data) {
    // 1. Host-registered handlers.
    {
        std::lock_guard<std::mutex> lock(mutex_);
        current_context_ = ctx;
        auto it = handlers_.find(code);
        if (it != handlers_.end()) {
            it->second.first(code, ctx, it->second.second);
            return true;
        }
    }

    // 2. Guest vectored handlers.
    WinExceptionRecord record;
    memset(&record, 0, sizeof(record));
    record.exception_code = static_cast<uint32_t>(code);
    record.exception_flags = 0;
    record.exception_address = reinterpret_cast<void*>(ctx.rip);

    EXCEPTION_POINTERS pointers;
    pointers.ExceptionRecord = reinterpret_cast<EXCEPTION_RECORD*>(&record);
    pointers.ContextRecord = nullptr;
    if (WINAPI_CallFirstVectoredExceptionHandler(&pointers) == EXCEPTION_CONTINUE_EXECUTION) {
        return true;
    }

    // 3. Guest SEH chain (TEB->ExceptionList).
    if (walk_exception_chain(code, ctx)) {
        return true;
    }

    // 4. Top-level filter (SetUnhandledExceptionFilter).
    void* filter = g_unhandled_exception_filter;
    if (filter) {
        WinContext fctx;
        memset(&fctx, 0, sizeof(fctx));
        fctx.p1_home = ctx.rax;
        fctx.p2_home = ctx.rcx;
        fctx.p3_home = ctx.rdx;
        fctx.p4_home = ctx.rbx;
        fctx.p5_home = ctx.rsp;
        fctx.p6_home = ctx.rbp;
        fctx.context_flags = 0x0010007F; // CONTEXT_CONTROL|INTEGER|SEGMENTS|FLOATING_POINT
        fctx.seg_cs = static_cast<uint16_t>(ctx.cs);
        fctx.seg_ss = static_cast<uint16_t>(ctx.ss);
        fctx.eflags = static_cast<uint32_t>(ctx.rflags);
        fctx.rax = ctx.rax;
        fctx.rcx = ctx.rcx;
        fctx.rdx = ctx.rdx;
        fctx.rbx = ctx.rbx;
        fctx.rsp = ctx.rsp;
        fctx.rbp = ctx.rbp;
        fctx.rsi = ctx.rsi;
        fctx.rdi = ctx.rdi;
        fctx.r8 = ctx.r8;
        fctx.r9 = ctx.r9;
        fctx.r10 = ctx.r10;
        fctx.r11 = ctx.r11;
        fctx.r12 = ctx.r12;
        fctx.r13 = ctx.r13;
        fctx.r14 = ctx.r14;
        fctx.r15 = ctx.r15;
        fctx.rip = ctx.rip;
        memcpy(fctx.xmm, ctx.xmm, sizeof(fctx.xmm));

        EXCEPTION_POINTERS fps;
        fps.ExceptionRecord = reinterpret_cast<EXCEPTION_RECORD*>(&record);
        fps.ContextRecord = reinterpret_cast<CONTEXT64*>(&fctx);
        // Guest filter code uses the Windows x64 (ms_abi) convention.
        LONG (__attribute__((ms_abi)) *filter_fn)(void*) = reinterpret_cast<LONG (__attribute__((ms_abi)) *)(void*)>(filter);
        LONG result = filter_fn(&fps);
        fprintf(stderr, "Top-level filter returned 0x%X\n", static_cast<unsigned int>(result));
        if (result == EXCEPTION_CONTINUE_EXECUTION) {
            // Copy the filter's register fix-ups (e.g. advanced Rip) back into
            // ctx so the signal handler can resume at the chosen point.
            ctx.rax = fctx.rax;
            ctx.rcx = fctx.rcx;
            ctx.rdx = fctx.rdx;
            ctx.rbx = fctx.rbx;
            ctx.rsp = fctx.rsp;
            ctx.rbp = fctx.rbp;
            ctx.rsi = fctx.rsi;
            ctx.rdi = fctx.rdi;
            ctx.r8 = fctx.r8;
            ctx.r9 = fctx.r9;
            ctx.r10 = fctx.r10;
            ctx.r11 = fctx.r11;
            ctx.r12 = fctx.r12;
            ctx.r13 = fctx.r13;
            ctx.r14 = fctx.r14;
            ctx.r15 = fctx.r15;
            ctx.rip = fctx.rip;
            ctx.rflags = fctx.eflags;
            memcpy(ctx.xmm, fctx.xmm, sizeof(ctx.xmm));
            return true;
        }
    }

    unhandled_exception(code, ctx, user_data);
    return false;
}

bool ExceptionHandler::walk_exception_chain(ExceptionCode code, CPUContext64& ctx) {
    // Locate the current guest TEB (thread environment block). The SEH chain
    // head lives at TEB+0; each frame is { Handler, Prev }.
    uint64_t tid = ThreadManager::instance().get_current_thread();
    if (tid == 0) return false;
    ThreadContext* tctx = ThreadManager::instance().get_thread_context(tid);
    if (!tctx || !tctx->teb) return false;

    uint64_t teb = tctx->teb;
    uint64_t frame_ptr = *reinterpret_cast<uint64_t*>(teb + teb_offsets::ExceptionList);
    if (frame_ptr == 0 || frame_ptr == ~0ull) return false;

    // Build a guest-visible EXCEPTION_RECORD on the host stack.
    WinExceptionRecord record;
    memset(&record, 0, sizeof(record));
    record.exception_code = static_cast<uint32_t>(code);
    record.exception_flags = 0;
    record.exception_address = reinterpret_cast<void*>(ctx.rip);

    // Build a guest-visible Win64 CONTEXT mirroring the current register state
    // so handlers can read (and fix up, e.g. Rip) the faulting context.
    WinContext wctx;
    memset(&wctx, 0, sizeof(wctx));
    wctx.p1_home = ctx.rax;
    wctx.p2_home = ctx.rcx;
    wctx.p3_home = ctx.rdx;
    wctx.p4_home = ctx.rbx;
    wctx.p5_home = ctx.rsp;
    wctx.p6_home = ctx.rbp;
    wctx.context_flags = 0x0010007F; // CONTEXT_CONTROL|INTEGER|SEGMENTS|FLOATING_POINT
    wctx.seg_cs = static_cast<uint16_t>(ctx.cs);
    wctx.seg_ss = static_cast<uint16_t>(ctx.ss);
    wctx.eflags = static_cast<uint32_t>(ctx.rflags);
    wctx.rax = ctx.rax;
    wctx.rcx = ctx.rcx;
    wctx.rdx = ctx.rdx;
    wctx.rbx = ctx.rbx;
    wctx.rsp = ctx.rsp;
    wctx.rbp = ctx.rbp;
    wctx.rsi = ctx.rsi;
    wctx.rdi = ctx.rdi;
    wctx.r8 = ctx.r8;
    wctx.r9 = ctx.r9;
    wctx.r10 = ctx.r10;
    wctx.r11 = ctx.r11;
    wctx.r12 = ctx.r12;
    wctx.r13 = ctx.r13;
    wctx.r14 = ctx.r14;
    wctx.r15 = ctx.r15;
    wctx.rip = ctx.rip;
    memcpy(wctx.xmm, ctx.xmm, sizeof(wctx.xmm));

    // Guest 64-bit code is compiled with the Windows x64 (ms_abi) convention;
    // call handlers directly with ms_abi so the args land in rcx/rdx/r8/r9.
    using SehHandler = LONG(__attribute__((ms_abi)) *)(WinExceptionRecord*, uint64_t, WinContext*, void*);
    size_t depth = 0;
    while (frame_ptr && depth < 16) {
        uint64_t handler = *reinterpret_cast<uint64_t*>(frame_ptr + except_offsets::Handler);
        uint64_t prev = *reinterpret_cast<uint64_t*>(frame_ptr + except_offsets::Prev);
        if (handler == 0) break;
        SehHandler fn = reinterpret_cast<SehHandler>(handler);
        LONG disposition = fn(&record, frame_ptr, &wctx, nullptr);
        if (disposition != EXCEPTION_CONTINUE_SEARCH) {
            // Handler claimed it; copy any register fix-ups back into ctx so the
            // signal handler can resume at the handler-chosen RIP.
            ctx.rax = wctx.rax;
            ctx.rcx = wctx.rcx;
            ctx.rdx = wctx.rdx;
            ctx.rbx = wctx.rbx;
            ctx.rsp = wctx.rsp;
            ctx.rbp = wctx.rbp;
            ctx.rsi = wctx.rsi;
            ctx.rdi = wctx.rdi;
            ctx.r8 = wctx.r8;
            ctx.r9 = wctx.r9;
            ctx.r10 = wctx.r10;
            ctx.r11 = wctx.r11;
            ctx.r12 = wctx.r12;
            ctx.r13 = wctx.r13;
            ctx.r14 = wctx.r14;
            ctx.r15 = wctx.r15;
            ctx.rip = wctx.rip;
            ctx.rflags = wctx.eflags;
            memcpy(ctx.xmm, wctx.xmm, sizeof(ctx.xmm));
            return true;
        }
        frame_ptr = prev;
        depth++;
    }
    return false;
}

} // namespace ThetaZ
