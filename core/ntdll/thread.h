// ThetaZ — Copyright (c) 2026 ThetaZ Project. MIT License.

#pragma once
#include <cstdint>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <pthread.h>

using LPVOID = void*;
using SIZE_T = size_t;
using DWORD = uint32_t;
using BOOL = int32_t;

namespace ThetaZ {

struct ThreadContext {
    uint64_t teb;
    uint64_t thread_id;
    uint64_t stack_pointer;
    uint64_t stack_base;
    uint64_t stack_size;
    bool running;
    bool initialized;

    uint8_t xmm[16 * 16];
    uint8_t ymm[32 * 16];

    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rip;
    uint64_t rflags;

    ThreadContext()
        : teb(0), thread_id(0), stack_pointer(0), stack_base(0), stack_size(0),
          running(false), initialized(false) {
        memset(xmm, 0, sizeof(xmm));
        memset(ymm, 0, sizeof(ymm));
    }
};

struct ThreadRecord {
    ThreadContext ctx;
    pthread_t pthread;
    uint64_t teb;
    uint64_t stack_addr;
    size_t stack_size;
    uint32_t exit_code;
    bool terminated;
    bool suspended;

    ThreadRecord()
        : pthread(0), teb(0), stack_addr(0), stack_size(0),
          exit_code(0), terminated(false), suspended(false) {}
};

class ThreadManager {
public:
    static ThreadManager& instance();

    // If `start_addr` is 0, registers the calling (main) host thread without
    // spawning anything. Otherwise spawns a real pthread that runs the guest
    // thread proc and returns via exit_thread(). Returns the Windows tid.
    // `pid` != 0 marks the thread as a child process's primary thread: it is
    // registered with ThetaServer under `pid` and its exit terminates that
    // child process.
    uint64_t create_thread(uint64_t start_addr, uint64_t param, uint64_t stack_size = 0, uint64_t pid = 0);

    // Ask another thread to stop (Windows TerminateThread semantics).
    void terminate_thread(uint64_t thread_id, uint32_t exit_code);

    // The current guest thread has finished; records the exit code, notifies
    // waiters, and terminates the host pthread.
    void exit_thread(uint32_t exit_code);

    ThreadContext* get_thread_context(uint64_t thread_id);
    void set_thread_context(uint64_t thread_id, const ThreadContext& ctx);

    void suspend_thread(uint64_t thread_id);
    void resume_thread(uint64_t thread_id);

    uint32_t get_exit_code(uint64_t thread_id);

    // Windows thread id of the calling host thread.
    uint64_t get_current_thread() const;
    static uint64_t get_pthread_id();
    static uint64_t get_thread_id_from_teb(uint64_t teb);
    static void set_current_windows_tid(uint64_t tid);

    // Guest process id of the calling thread (0 = the main process).
    static uint64_t get_current_guest_pid();

private:
    ThreadManager();
    ThreadRecord* get_record(uint64_t thread_id);
    void initialize_thread_context(ThreadContext& ctx);

    uint64_t next_thread_id_;
    std::unordered_map<uint64_t, ThreadRecord> threads_;
    std::unordered_map<uint64_t, uint64_t> teb_to_tid_;
    std::unordered_map<pthread_t, uint64_t> pthread_to_tid_;
    mutable std::mutex mutex_;
};

} // namespace ThetaZ
