// ThetaZ — Copyright (c) 2026 ThetaZ Project. MIT License.

#include "core/ntdll/thread.h"
#include "core/ntdll/teb.h"
#include "core/api/api.h"
#include "core/ipc/thetaserver.h"
#include "core/loader/loader.h"
#include "core/x86/x86.h"

#include <cstdio>
#include <csignal>
#include <memory>
#include <condition_variable>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>

namespace ThetaZ {

namespace {

// The Windows thread id of the calling host thread. Set for the main thread at
// startup and for each guest thread in its entry routine.
thread_local uint64_t g_current_windows_tid = 0;

// The guest process id of the calling host thread. 0 means "the main process"
// (the one the emulator launched with argv[1]); a child-process primary thread
// sets this to its child pid so process-exit routing works per process.
thread_local uint64_t g_current_guest_pid = 0;

// Per-thread suspend/resume state. Suspend delivers SIGUSR1, whose handler
// blocks until SIGUSR2 (resume) clears the flag.
struct SuspendState {
    std::mutex mutex;
    std::condition_variable cv;
    bool suspended = false;
};

SuspendState& suspend_state() {
    static thread_local SuspendState state;
    return state;
}

void suspend_signal_handler(int) {
    SuspendState& s = suspend_state();
    std::unique_lock<std::mutex> lock(s.mutex);
    s.suspended = true;
    s.cv.wait(lock, [&] { return !s.suspended; });
}

void resume_signal_handler(int) {
    SuspendState& s = suspend_state();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.suspended = false;
    s.cv.notify_one();
}

struct ThreadStartArg {
    ThreadManager* mgr;
    uint64_t tid;
    uint64_t start_addr;
    uint64_t param;
    uint64_t pid;   // 0 = ordinary thread; != 0 = child-process primary thread
};

void install_suspend_handlers() {
    struct sigaction sa {};
    sa.sa_handler = suspend_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, nullptr);
    sa.sa_handler = resume_signal_handler;
    sigaction(SIGUSR2, &sa, nullptr);
}

void* thread_entry(void* arg) {
    std::unique_ptr<ThreadStartArg> a(static_cast<ThreadStartArg*>(arg));
    ThreadManager& mgr = ThreadManager::instance();

    install_suspend_handlers();
    if (a->start_addr) {
        // Own TEB + GS base so gs:[0x30]/gs:[0x60] and the exception list are
        // this thread's, not the main thread's.
        set_gs_base(mgr.get_thread_context(a->tid)->teb);
    }
    ThreadManager::set_current_windows_tid(a->tid);
    if (a->pid != 0) {
        // A child-process primary thread: remember the process for exit routing
        // (ExitProcess / CRT exit). The θserver object was already registered
        // synchronously by create_thread.
        g_current_guest_pid = a->pid;
    }

    // 32-bit guest thread: run its entry point through the interpreter with a
    // fresh stack and per-thread TEB. 64-bit guest threads call the host proc.
    // The bitness is the guest's, not the start address's: 64-bit children are
    // remapped to low relaunch bases (0x20000000+) that also sit below 4GB.
    if (a->start_addr && guest_is_32bit()) {
        ThreadContext* ctx = mgr.get_thread_context(a->tid);
        uint64_t stack_base = ctx ? ctx->stack_base : 0;
        uint32_t stack_size = ctx ? static_cast<uint32_t>(ctx->stack_size) : (1u << 20);
        int rc = x86_run_guest_thread(static_cast<uint32_t>(a->start_addr),
                                      static_cast<uint32_t>(a->param),
                                      stack_base, stack_size,
                                      static_cast<uint32_t>(a->tid));
        if (a->pid != 0) {
            // Free the child's image mapping to reclaim low-zone address space.
            uint64_t child_base = get_guest_process_image_base(a->pid);
            if (child_base) {
                PELoader::instance().unload_image(child_base);
                set_guest_process_image_base(a->pid, 0);
            }
            // rc is the guest thread's 32-bit exit value carried in an int;
            // high-bit codes such as 0xC0000005 are valid exit codes and must
            // not be coerced to success.
            ThetaServer::instance().terminate_process(a->pid, static_cast<uint32_t>(rc));
        }
        ThreadManager::instance().exit_thread(static_cast<uint32_t>(rc));
    } else if (a->start_addr) {
        // 64-bit guest: call the proc with the guest's MS x64 ABI.
        using GuestProc = uint32_t (__attribute__((ms_abi)) *)(void*);
        GuestProc proc = reinterpret_cast<GuestProc>(a->start_addr);
        uint32_t exit_code = proc(reinterpret_cast<void*>(a->param));
        if (a->pid != 0) {
            // A child-process primary thread returning means the process ends.
            ThetaServer::instance().terminate_process(a->pid, exit_code);
        }
        ThreadManager::instance().exit_thread(exit_code);
    }

    return nullptr;
}

} // namespace

ThreadManager& ThreadManager::instance() {
    static ThreadManager manager;
    return manager;
}

ThreadManager::ThreadManager() : next_thread_id_(0) {}

ThreadRecord* ThreadManager::get_record(uint64_t thread_id) {
    auto it = threads_.find(thread_id);
    if (it == threads_.end()) return nullptr;
    return &it->second;
}

uint64_t ThreadManager::create_thread(uint64_t start_addr, uint64_t param, uint64_t stack_size, uint64_t pid) {
    std::lock_guard<std::mutex> lock(mutex_);

    uint64_t tid = ++next_thread_id_;

    ThreadRecord rec;
    rec.ctx.thread_id = tid;
    rec.pthread = pthread_self();

    if (start_addr == 0) {
        // Main thread: no pthread, reuse the TEB created by setup_windows_teb.
        rec.ctx.rip = 0;
        rec.teb = get_main_teb();
    } else {
        rec.ctx.rip = start_addr;
        rec.ctx.rsp = param;

        if (stack_size == 0) stack_size = 1u << 20;
        void* stack = mmap(nullptr, stack_size, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (stack != MAP_FAILED) {
            rec.stack_addr = reinterpret_cast<uint64_t>(stack);
            rec.stack_size = stack_size;
            rec.ctx.stack_base = rec.stack_addr;
            rec.ctx.stack_pointer = rec.stack_addr + stack_size;
        }

        rec.teb = setup_thread_teb(get_main_peb(), tid,
                                   rec.ctx.stack_base, rec.ctx.stack_pointer);

        // Register the thread object synchronously so the parent can wait on
        // its handle (WaitForSingleObject/GetExitCodeThread) immediately, even
        // before the new pthread has scheduled thread_entry.
        ThetaServer::instance().register_thread(tid, pid);
    }

    rec.ctx.teb = rec.teb;

    if (rec.teb) {
        // TEB->ClientId.UniqueThread is the canonical Windows thread id.
        *reinterpret_cast<uint64_t*>(rec.teb + 0x048) = tid;
    }

    initialize_thread_context(rec.ctx);
    rec.ctx.running = true;
    rec.ctx.initialized = true;

    threads_[tid] = rec;
    if (rec.teb) teb_to_tid_[rec.teb] = tid;

    if (start_addr == 0) {
        pthread_to_tid_[pthread_self()] = tid;
    } else {
        auto* arg = new ThreadStartArg{this, tid, start_addr, param, pid};
        pthread_t pthread = 0;
        if (pthread_create(&pthread, nullptr, thread_entry, arg) != 0) {
            delete arg;
            threads_.erase(tid);
            if (rec.teb) teb_to_tid_.erase(rec.teb);
            return 0;
        }
        threads_[tid].pthread = pthread;
        pthread_to_tid_[pthread] = tid;
        pthread_detach(pthread);
    }

    printf("[ThreadManager] Created thread tid=%lu start=0x%lX\n", tid, start_addr);
    return tid;
}

void ThreadManager::terminate_thread(uint64_t thread_id, uint32_t exit_code) {
    pthread_t pthread = 0;
    bool is_self = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ThreadRecord* rec = get_record(thread_id);
        if (!rec || rec->terminated) return;
        rec->terminated = true;
        rec->exit_code = exit_code;
        rec->ctx.running = false;
        pthread = rec->pthread;
        is_self = (pthread == pthread_self());
    }
    ThetaServer::instance().terminate_thread(thread_id, exit_code);
    if (!is_self && pthread) pthread_cancel(pthread);
}

void ThreadManager::exit_thread(uint32_t exit_code) {
    uint64_t tid = g_current_windows_tid;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ThreadRecord* rec = get_record(tid);
        if (rec) {
            rec->terminated = true;
            rec->exit_code = exit_code;
            rec->ctx.running = false;
        }
    }
    ThetaServer::instance().terminate_thread(tid, exit_code);
    // The stack can't be unmapped from the thread that runs on it; process
    // teardown reclaims it.
    pthread_exit(nullptr);
}

ThreadContext* ThreadManager::get_thread_context(uint64_t thread_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    ThreadRecord* rec = get_record(thread_id);
    return rec ? &rec->ctx : nullptr;
}

void ThreadManager::set_thread_context(uint64_t thread_id, const ThreadContext& ctx) {
    std::lock_guard<std::mutex> lock(mutex_);
    ThreadRecord* rec = get_record(thread_id);
    if (rec) rec->ctx = ctx;
}

void ThreadManager::suspend_thread(uint64_t thread_id) {
    pthread_t pthread = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ThreadRecord* rec = get_record(thread_id);
        if (!rec || rec->terminated) return;
        rec->suspended = true;
        pthread = rec->pthread;
    }
    if (pthread && pthread != pthread_self()) pthread_kill(pthread, SIGUSR1);
}

void ThreadManager::resume_thread(uint64_t thread_id) {
    pthread_t pthread = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ThreadRecord* rec = get_record(thread_id);
        if (!rec || rec->terminated) return;
        rec->suspended = false;
        pthread = rec->pthread;
    }
    if (pthread && pthread != pthread_self()) pthread_kill(pthread, SIGUSR2);
}

uint32_t ThreadManager::get_exit_code(uint64_t thread_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    ThreadRecord* rec = get_record(thread_id);
    return rec ? rec->exit_code : 0;
}

uint64_t ThreadManager::get_current_thread() const {
    if (g_current_windows_tid) return g_current_windows_tid;
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = pthread_to_tid_.find(pthread_self());
    return it != pthread_to_tid_.end() ? it->second : 0;
}

uint64_t ThreadManager::get_pthread_id() {
    return static_cast<uint64_t>(pthread_self());
}

uint64_t ThreadManager::get_thread_id_from_teb(uint64_t teb) {
    ThreadManager& mgr = instance();
    std::lock_guard<std::mutex> lock(mgr.mutex_);
    auto it = mgr.teb_to_tid_.find(teb);
    return it != mgr.teb_to_tid_.end() ? it->second : 0;
}

void ThreadManager::set_current_windows_tid(uint64_t tid) {
    g_current_windows_tid = tid;
}

uint64_t ThreadManager::get_current_guest_pid() {
    return g_current_guest_pid;
}

void ThreadManager::initialize_thread_context(ThreadContext& ctx) {
    ctx.rax = ctx.rbx = ctx.rcx = ctx.rdx = 0;
    ctx.rsi = ctx.rdi = ctx.rbp = 0;
    ctx.r8 = ctx.r9 = ctx.r10 = ctx.r11 = 0;
    ctx.r12 = ctx.r13 = ctx.r14 = ctx.r15 = 0;
    ctx.rflags = 0x2;
}

} // namespace ThetaZ
