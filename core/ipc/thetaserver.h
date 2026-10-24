// ThetaZ — Copyright (c) 2026 ThetaZ Project. MIT License.

#pragma once
#include "core/ipc/ipc.h"

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>

namespace ThetaZ {

// WaitForSingleObject-style results (kernel32 wait status values).
namespace wait_status {
constexpr uint32_t Object0 = 0x00000000;      // WAIT_OBJECT_0
constexpr uint32_t Abandoned = 0x00000080;    // WAIT_ABANDONED
constexpr uint32_t Timeout = 0x00000102;      // WAIT_TIMEOUT
constexpr uint32_t Infinite = 0xFFFFFFFF;     // WAIT_INFINITE
constexpr uint32_t Failed = 0xFFFFFFFF;       // WAIT_FAILED
}

enum class ObjectType : uint8_t {
    None = 0,
    Process = 1,
    Thread = 2,
    Event = 3,
    Semaphore = 4,
    Mutex = 5,
    Timer = 6,
    File = 7,
    Section = 8,
    Window = 9,
    Key = 10,
};

struct ThetaServerObject {
    uint64_t handle;
    ObjectType type;
    uint64_t owner_pid;
    std::string name;
    int ref_count;
    bool permanent;

    // Per-object wait state. Waiters block on `obj_cond` while holding
    // `obj_mutex`; setters notify under the same lock.
    std::mutex obj_mutex;
    std::condition_variable obj_cond;

    ThetaServerObject() : handle(0), type(ObjectType::None), owner_pid(0), ref_count(1), permanent(false) {}
    virtual ~ThetaServerObject() = default;
};

struct EventObject : ThetaServerObject {
    bool manual_reset;
    bool initial_state;
    std::atomic<bool> signaled;
    std::atomic<uint64_t> pulse_count;

    EventObject() : ThetaServerObject(), manual_reset(false), initial_state(false), signaled(false), pulse_count(0) {
        type = ObjectType::Event;
    }
};

struct SemaphoreObject : ThetaServerObject {
    std::atomic<int> count;
    int max_count;

    SemaphoreObject() : ThetaServerObject(), count(0), max_count(1) {
        type = ObjectType::Semaphore;
    }
};

struct MutexObject : ThetaServerObject {
    uint64_t owner;
    int recursion_count;
    bool abandoned;

    MutexObject() : ThetaServerObject(), owner(0), recursion_count(0), abandoned(false) {
        type = ObjectType::Mutex;
    }
};

struct TimerObject : ThetaServerObject {
    bool is_manual;
    // Absolute steady-clock deadline in milliseconds. 0 while unarmed; the
    // clock epoch is boot time, so a live deadline is never zero.
    std::atomic<uint64_t> due_time;
    std::atomic<bool> armed;
    std::atomic<bool> signaled;
    int period;   // milliseconds, 0 = one-shot

    TimerObject() : ThetaServerObject(), is_manual(false), due_time(0),
                    armed(false), signaled(false), period(0) {
        type = ObjectType::Timer;
    }
};

struct ProcessObject : ThetaServerObject {
    uint64_t ppid;
    uint64_t main_thread_id;   // tid of the process's primary thread (0 = unknown)
    uint32_t exit_code;
    bool terminated;
    uint8_t priority;
    uint32_t affinity;

    ProcessObject() : ThetaServerObject(), ppid(0), main_thread_id(0), exit_code(0),
                      terminated(false), priority(0x08), affinity(0) {
        type = ObjectType::Process;
    }
};

struct ThreadObject : ThetaServerObject {
    uint64_t pid;
    uint32_t exit_code;
    bool terminated;
    bool suspended;
    uint32_t priority;
    uint32_t affinity;

    ThreadObject() : ThetaServerObject(), pid(0), exit_code(0), terminated(false), suspended(false), priority(0x08), affinity(0) {
        type = ObjectType::Thread;
    }
};

class ThetaServer {
public:
    static ThetaServer& instance();

    uint64_t register_process(uint64_t pid, uint64_t ppid);
    uint64_t register_thread(uint64_t tid, uint64_t pid);
    bool set_process_main_thread(uint64_t pid, uint64_t tid);
    bool terminate_process(uint64_t pid, uint32_t exit_code);
    bool terminate_thread(uint64_t tid, uint32_t exit_code);

    // Number of threads of `pid` that have not terminated yet. Used to decide
    // whether a child's image can be unmapped: while other threads still run
    // guest code from it, unmapping would leave them (and the SEH handlers
    // dispatched for them) executing unmapped memory.
    size_t live_thread_count(uint64_t pid);

    uint64_t create_event(const std::string& name, bool manual_reset, bool initial_state, uint64_t owner_pid);
    uint64_t create_semaphore(const std::string& name, int initial_count, int max_count, uint64_t owner_pid);
    uint64_t create_mutex(const std::string& name, uint64_t owner_pid);
    uint64_t create_timer(const std::string& name, bool manual, uint64_t owner_pid);

    bool open_object(const std::string& name, ObjectType type, uint64_t pid, uint64_t& handle);
    void close_handle(uint64_t pid, uint64_t handle);

    bool set_event(uint64_t handle);
    bool reset_event(uint64_t handle);
    bool pulse_event(uint64_t handle);

    int release_semaphore(uint64_t handle, int count);
    bool acquire_mutex(uint64_t handle);
    bool release_mutex(uint64_t handle);

    // Arm a timer to fire due_ms from now; period_ms repeats (-1 = none).
    bool set_timer(uint64_t handle, uint64_t due_ms, int period_ms);
    bool cancel_timer(uint64_t handle);

    // Blocking waits with timeout semantics. `timeout_ms` of 0xFFFFFFFF
    // (WAIT_INFINITE) blocks forever; 0 does a non-blocking check.
    uint32_t wait_for_object(uint64_t handle, uint64_t timeout_ms);
    uint32_t wait_for_any(const std::vector<uint64_t>& handles, uint64_t timeout_ms);
    uint32_t wait_for_all(const std::vector<uint64_t>& handles, uint64_t timeout_ms);

    ProcessObject* get_process(uint64_t pid);
    ThreadObject* get_thread(uint64_t tid);
    // Thread exit code (STILL_ACTIVE=0x103 if the thread hasn't terminated).
    uint32_t get_thread_exit_code(uint64_t tid);

    std::vector<uint64_t> get_waitable_handles(uint64_t pid, ObjectType filter_type);

    void shutdown();

private:
    ThetaServer();
    ~ThetaServer();

    // Non-consuming "would a wait on this handle succeed right now?" check.
    bool object_ready(uint64_t handle);

    uint64_t next_handle() {
        static std::atomic<uint64_t> h{0x1000};
        return h++;
    }

    std::unordered_map<uint64_t, std::shared_ptr<ThetaServerObject>> objects_;
    std::unordered_map<uint64_t, std::shared_ptr<ProcessObject>> processes_;
    std::unordered_map<uint64_t, std::shared_ptr<ThreadObject>> threads_;
    std::unordered_map<uint64_t, std::vector<uint64_t>> handle_tables_;
    mutable std::mutex mutex_;
};

} // namespace ThetaZ
