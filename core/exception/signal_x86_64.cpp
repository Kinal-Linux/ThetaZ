// ThetaZ — Copyright (c) 2026 ThetaZ Project. MIT License.

#include "core/exception/exception.h"

#include <signal.h>
#include <ucontext.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <execinfo.h>
#include <cstdlib>

namespace ThetaZ {

namespace {
// Convert a host signal into the matching Windows exception code.
ExceptionCode signal_to_exception(int sig) {
    switch (sig) {
        case SIGSEGV: return ExceptionCode::AccessViolation;
        case SIGFPE:  return ExceptionCode::IntegerDivideByZero;
        case SIGILL:  return ExceptionCode::InvalidOpcode;
        default:      return ExceptionCode::InvalidParameter;
    }
}
} // namespace

ExceptionSetup64::ExceptionSetup64(ExceptionHandler& handler)
    : handler_(handler) {
    set_active_exception_handler(&handler_);

    struct sigaction sa{};
    sa.sa_sigaction = [](int sig, siginfo_t* info, void* ucontext) {
        ucontext_t* uc = static_cast<ucontext_t*>(ucontext);

        CPUContext64 ctx;
        ctx.rax = uc->uc_mcontext.gregs[REG_RAX];
        ctx.rcx = uc->uc_mcontext.gregs[REG_RCX];
        ctx.rdx = uc->uc_mcontext.gregs[REG_RDX];
        ctx.rbx = uc->uc_mcontext.gregs[REG_RBX];
        ctx.rsp = uc->uc_mcontext.gregs[REG_RSP];
        ctx.rbp = uc->uc_mcontext.gregs[REG_RBP];
        ctx.rsi = uc->uc_mcontext.gregs[REG_RSI];
        ctx.rdi = uc->uc_mcontext.gregs[REG_RDI];
        ctx.r8 = uc->uc_mcontext.gregs[REG_R8];
        ctx.r9 = uc->uc_mcontext.gregs[REG_R9];
        ctx.r10 = uc->uc_mcontext.gregs[REG_R10];
        ctx.r11 = uc->uc_mcontext.gregs[REG_R11];
        ctx.r12 = uc->uc_mcontext.gregs[REG_R12];
        ctx.r13 = uc->uc_mcontext.gregs[REG_R13];
        ctx.r14 = uc->uc_mcontext.gregs[REG_R14];
        ctx.r15 = uc->uc_mcontext.gregs[REG_R15];
        ctx.rip = uc->uc_mcontext.gregs[REG_RIP];
        ctx.rflags = uc->uc_mcontext.gregs[REG_EFL];
        ctx.cs = uc->uc_mcontext.gregs[REG_CSGSFS] & 0xFFFF;
        ctx.ss = (uc->uc_mcontext.gregs[REG_CSGSFS] >> 48) & 0xFFFF;
        ctx.rsp_at_entry = uc->uc_mcontext.gregs[REG_RSP];
        ctx.fs_base = 0;
        ctx.gs_base = 0;
        ctx.cr2 = 0;

        if (uc->uc_mcontext.fpregs) {
            for (int i = 0; i < 16; i++) {
                uint64_t* xmm_ptr = reinterpret_cast<uint64_t*>(&ctx.xmm[i * 16]);
                xmm_ptr[0] = uc->uc_mcontext.fpregs->_xmm[i].element[0];
                xmm_ptr[1] = uc->uc_mcontext.fpregs->_xmm[i].element[1];
            }
        }

        ExceptionCode code = signal_to_exception(sig);

        fprintf(stderr, "[signal] sig=%d code=%d RIP=0x%llX fault_addr=0x%llX RSP=0x%llX\n",
                sig, static_cast<int>(code),
                (unsigned long long)ctx.rip,
                (unsigned long long)(uintptr_t)info->si_addr,
                (unsigned long long)ctx.rsp);

        // Dump stack trace via backtrace
        {
            void* bt[20];
            int n = backtrace(bt, 20);
            fprintf(stderr, "[signal] BACKTRACE (%d frames):\n", n);
            char** syms = backtrace_symbols(bt, n);
            if (syms) {
                for (int i = 0; i < n; i++) {
                    fprintf(stderr, "[signal]   [%d] %s\n", i, syms[i]);
                }
                free(syms);
            }
            // Also dump /proc/self/maps for the RIP mapping
            FILE* maps = fopen("/proc/self/maps", "r");
            if (maps) {
                char line[512];
                while (fgets(line, sizeof(line), maps)) {
                    unsigned long start = 0, end = 0;
                    if (sscanf(line, "%lx-%lx", &start, &end) == 2) {
                        if (ctx.rip >= start && ctx.rip < end) {
                            fprintf(stderr, "[signal] CRASH MAP: %s", line);
                        }
                    }
                }
                fclose(maps);
            }
        }

        ExceptionHandler* handler = get_active_exception_handler();
        if (!handler) {
            static ExceptionHandler fallback;
            handler = &fallback;
        }

        // Dispatch through the full unhandled-exception flow: host hooks, the
        // guest SEH chain, then the top-level filter. A handler may fix up ctx
        // (e.g. new RIP) for EXCEPTION_CONTINUE_EXECUTION.
        if (handler->dispatch(code, ctx)) {
            // Restore the (possibly modified) register state so execution
            // resumes at the fixed-up RIP.
            uc->uc_mcontext.gregs[REG_RAX] = ctx.rax;
            uc->uc_mcontext.gregs[REG_RCX] = ctx.rcx;
            uc->uc_mcontext.gregs[REG_RDX] = ctx.rdx;
            uc->uc_mcontext.gregs[REG_RBX] = ctx.rbx;
            uc->uc_mcontext.gregs[REG_RSP] = ctx.rsp;
            uc->uc_mcontext.gregs[REG_RBP] = ctx.rbp;
            uc->uc_mcontext.gregs[REG_RSI] = ctx.rsi;
            uc->uc_mcontext.gregs[REG_RDI] = ctx.rdi;
            uc->uc_mcontext.gregs[REG_R8] = ctx.r8;
            uc->uc_mcontext.gregs[REG_R9] = ctx.r9;
            uc->uc_mcontext.gregs[REG_R10] = ctx.r10;
            uc->uc_mcontext.gregs[REG_R11] = ctx.r11;
            uc->uc_mcontext.gregs[REG_R12] = ctx.r12;
            uc->uc_mcontext.gregs[REG_R13] = ctx.r13;
            uc->uc_mcontext.gregs[REG_R14] = ctx.r14;
            uc->uc_mcontext.gregs[REG_R15] = ctx.r15;
            uc->uc_mcontext.gregs[REG_RIP] = ctx.rip;
            uc->uc_mcontext.gregs[REG_EFL] = ctx.rflags;

            if (uc->uc_mcontext.fpregs) {
                for (int i = 0; i < 16; i++) {
                    const uint64_t* xmm_ptr = reinterpret_cast<const uint64_t*>(&ctx.xmm[i * 16]);
                    uc->uc_mcontext.fpregs->_xmm[i].element[0] = xmm_ptr[0];
                    uc->uc_mcontext.fpregs->_xmm[i].element[1] = xmm_ptr[1];
                }
            }

            (void)info;
            return; // resume at the handler-chosen RIP
        }

        // No handler fixed up the context: re-raise with the default action so
        // the process terminates instead of re-executing the fault forever.
        (void)info;
        signal(sig, SIG_DFL);
        raise(sig);
    };

    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGFPE, &sa, nullptr);
    sigaction(SIGILL, &sa, nullptr);
}

ExceptionSetup64::~ExceptionSetup64() {
    if (get_active_exception_handler() == &handler_) {
        set_active_exception_handler(nullptr);
    }
}

} // namespace ThetaZ
