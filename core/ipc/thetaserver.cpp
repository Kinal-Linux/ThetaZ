// ThetaZ — Copyright (c) 2026 ThetaZ Project. MIT License.

#include "core/ipc/thetaserver.h"
#include "core/ntdll/thread.h"

#include <cstdio>
#include <vector>
#include <algorithm>
#include <chrono>
#include <thread>

namespace ThetaZ {

ThetaServer& ThetaServer::instance() {
    static ThetaServer server;
    return server;
}

ThetaServer::ThetaServer() {}
ThetaServer::~ThetaServer() {}

namespace {

uint64_t steady_clock_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

uint64_t add_ms_saturating(uint64_t base, uint64_t delta) {
    if (delta > UINT64_MAX - base) return UINT64_MAX;
    return base + delta;
}

} // namespace

uint64_t ThetaServer::register_process(uint64_t pid, uint64_t ppid) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto proc = std::make_shared<ProcessObject>();
    proc->handle = pid;
    proc->owner_pid = pid;
    proc->ppid = ppid;
    proc->priority = 0x08;

    processes_[pid] = proc;
    objects_[pid] = proc;
    handle_tables_[pid] = {pid};

    printf("[θserver] Registered process pid=%lu ppid=%lu\n", pid, ppid);
    return pid;
}

uint64_t ThetaServer::register_thread(uint64_t tid, uint64_t pid) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto thread = std::make_shared<ThreadObject>();
    thread->handle = tid;
    thread->owner_pid = pid;
    thread->pid = pid;

    threads_[tid] = thread;
    objects_[tid] = thread;

    auto& table = handle_tables_[pid];
    table.push_back(tid);

    printf("[θserver] Registered thread tid=%lu pid=%lu\n", tid, pid);
    return tid;
}

bool ThetaServer::set_process_main_thread(uint64_t pid, uint64_t tid) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = processes_.find(pid);
    if (it == processes_.end()) return false;
    it->second->main_thread_id = tid;
    return true;
}

bool ThetaServer::terminate_process(uint64_t pid, uint32_t exit_code) {
    std::shared_ptr<ProcessObject> proc;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = processes_.find(pid);
        if (it == processes_.end()) return false;

        // A process terminates once. Its other threads may still be unwinding
        // through their exit path after the primary thread ended the process;
        // letting them re-terminate would overwrite the exit code the parent
        // observes.
        if (it->second->terminated) return false;

        it->second->terminated = true;
        it->second->exit_code = exit_code;
        proc = it->second;

        printf("[θserver] Process %lu terminated with code %u\n", pid, exit_code);

        // Clean up all handles owned by this process (keep the process object
        // itself so WaitForSingleObject/GetExitCodeProcess still resolve it).
        // Thread objects are kept too: the parent received the primary thread's
        // handle from CreateProcess and may still wait on it (hThread) after
        // the child exits.
        auto table_it = handle_tables_.find(pid);
        if (table_it != handle_tables_.end()) {
            for (uint64_t handle : table_it->second) {
                if (handle == pid) continue;
                auto obj_it = objects_.find(handle);
                if (obj_it != objects_.end() && obj_it->second->type != ObjectType::Thread) {
                    obj_it->second->ref_count--;
                    if (obj_it->second->ref_count <= 0 && !obj_it->second->permanent) {
                        objects_.erase(obj_it);
                    }
                }
            }
            handle_tables_.erase(table_it);
        }
    }
    {
        std::lock_guard<std::mutex> lock(proc->obj_mutex);
        proc->obj_cond.notify_all();
    }
    return true;
}

bool ThetaServer::terminate_thread(uint64_t tid, uint32_t exit_code) {
    std::shared_ptr<ThreadObject> thread;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = threads_.find(tid);
        if (it == threads_.end()) return false;
        it->second->terminated = true;
        it->second->exit_code = exit_code;
        thread = it->second;
    }
    {
        std::lock_guard<std::mutex> lock(thread->obj_mutex);
        thread->obj_cond.notify_all();
    }

    // If this was a child process's primary thread, the child has exited.
    // Notify waiters (mirrors terminate_process semantics for that process).
    std::vector<std::shared_ptr<ProcessObject>> died;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& entry : processes_) {
            if (entry.second->main_thread_id == tid && !entry.second->terminated) {
                entry.second->terminated = true;
                entry.second->exit_code = exit_code;
                died.push_back(entry.second);
            }
        }
    }
    for (auto& proc : died) {
        std::lock_guard<std::mutex> lock(proc->obj_mutex);
        proc->obj_cond.notify_all();
    }
    return true;
}

size_t ThetaServer::live_thread_count(uint64_t pid) {
    if (pid == 0) return 0; // pid 0 is not a child process
    std::lock_guard<std::mutex> lock(mutex_);
    size_t n = 0;
    for (const auto& entry : threads_) {
        const std::shared_ptr<ThreadObject>& t = entry.second;
        if (t && t->pid == pid && !t->terminated) ++n;
    }
    return n;
}

uint64_t ThetaServer::create_event(const std::string& name, bool manual_reset, bool initial_state, uint64_t owner_pid) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto event = std::make_shared<EventObject>();
    event->handle = next_handle();
    event->owner_pid = owner_pid;
    event->name = name;
    event->manual_reset = manual_reset;
    event->initial_state = initial_state;
    event->signaled.store(initial_state);

    objects_[event->handle] = event;
    handle_tables_[owner_pid].push_back(event->handle);

    printf("[θserver] Created %s event \"%s\" handle=%lu\n",
           manual_reset ? "manual" : "auto", name.c_str(), event->handle);
    return event->handle;
}

uint64_t ThetaServer::create_semaphore(const std::string& name, int initial_count, int max_count, uint64_t owner_pid) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto sem = std::make_shared<SemaphoreObject>();
    sem->handle = next_handle();
    sem->owner_pid = owner_pid;
    sem->name = name;
    sem->count.store(initial_count);
    sem->max_count = max_count;

    objects_[sem->handle] = sem;
    handle_tables_[owner_pid].push_back(sem->handle);

    printf("[θserver] Created semaphore \"%s\" count=%d handle=%lu\n",
           name.c_str(), initial_count, sem->handle);
    return sem->handle;
}

uint64_t ThetaServer::create_mutex(const std::string& name, uint64_t owner_pid) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto mutex = std::make_shared<MutexObject>();
    mutex->handle = next_handle();
    mutex->owner_pid = owner_pid;
    mutex->name = name;

    objects_[mutex->handle] = mutex;
    handle_tables_[owner_pid].push_back(mutex->handle);

    printf("[θserver] Created mutex \"%s\" handle=%lu\n", name.c_str(), mutex->handle);
    return mutex->handle;
}

uint64_t ThetaServer::create_timer(const std::string& name, bool manual, uint64_t owner_pid) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto timer = std::make_shared<TimerObject>();
    timer->handle = next_handle();
    timer->owner_pid = owner_pid;
    timer->name = name;
    timer->is_manual = manual;

    objects_[timer->handle] = timer;
    handle_tables_[owner_pid].push_back(timer->handle);
    return timer->handle;
}

bool ThetaServer::set_timer(uint64_t handle, uint64_t due_ms, int period_ms) {
    std::shared_ptr<TimerObject> timer;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = objects_.find(handle);
        if (it == objects_.end() || it->second->type != ObjectType::Timer) return false;
        timer = std::static_pointer_cast<TimerObject>(it->second);
    }
    std::lock_guard<std::mutex> lock(timer->obj_mutex);
    timer->due_time.store(add_ms_saturating(steady_clock_ms(), due_ms));
    timer->period = period_ms > 0 ? period_ms : 0;
    timer->armed.store(true);
    timer->signaled.store(false);
    timer->obj_cond.notify_all();
    return true;
}

bool ThetaServer::cancel_timer(uint64_t handle) {
    std::shared_ptr<TimerObject> timer;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = objects_.find(handle);
        if (it == objects_.end() || it->second->type != ObjectType::Timer) return false;
        timer = std::static_pointer_cast<TimerObject>(it->second);
    }
    std::lock_guard<std::mutex> lock(timer->obj_mutex);
    // CancelWaitableTimer stops the timer but does not change an already
    // signaled state, so a manual-reset timer that fired stays signaled.
    timer->due_time.store(0);
    timer->armed.store(false);
    timer->period = 0;
    timer->obj_cond.notify_all();
    return true;
}

bool ThetaServer::open_object(const std::string& name, ObjectType type, uint64_t pid, uint64_t& handle) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (name.empty()) {
        return false;
    }

    for (const auto& [h, obj] : objects_) {
        if (obj->type == type && obj->name == name) {
            handle = h;
            obj->ref_count++;
            handle_tables_[pid].push_back(h);
            return true;
        }
    }

    return false;
}

void ThetaServer::close_handle(uint64_t pid, uint64_t handle) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto& table = handle_tables_[pid];
    table.erase(std::remove(table.begin(), table.end(), handle), table.end());

    auto it = objects_.find(handle);
    if (it != objects_.end()) {
        it->second->ref_count--;
        if (it->second->ref_count <= 0 && !it->second->permanent) {
            objects_.erase(it);
        }
    }
}

bool ThetaServer::set_event(uint64_t handle) {
    std::shared_ptr<EventObject> event;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = objects_.find(handle);
        if (it == objects_.end() || it->second->type != ObjectType::Event) {
            return false;
        }
        event = std::static_pointer_cast<EventObject>(it->second);
    }

    std::lock_guard<std::mutex> lock(event->obj_mutex);
    event->signaled.store(true);
    event->obj_cond.notify_all();
    return true;
}

bool ThetaServer::reset_event(uint64_t handle) {
    std::shared_ptr<EventObject> event;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = objects_.find(handle);
        if (it == objects_.end() || it->second->type != ObjectType::Event) {
            return false;
        }
        event = std::static_pointer_cast<EventObject>(it->second);
    }

    std::lock_guard<std::mutex> lock(event->obj_mutex);
    event->signaled.store(false);
    return true;
}

bool ThetaServer::pulse_event(uint64_t handle) {
    std::shared_ptr<EventObject> event;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = objects_.find(handle);
        if (it == objects_.end() || it->second->type != ObjectType::Event) {
            return false;
        }
        event = std::static_pointer_cast<EventObject>(it->second);
    }

    // Release all current waiters, then reset to non-signaled. The pulse_count
    // bump lets waiters observe the transient state even after we reset.
    std::lock_guard<std::mutex> lock(event->obj_mutex);
    event->signaled.store(true);
    event->pulse_count.fetch_add(1);
    event->obj_cond.notify_all();
    event->signaled.store(false);
    return true;
}

int ThetaServer::release_semaphore(uint64_t handle, int count) {
    std::shared_ptr<SemaphoreObject> sem;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = objects_.find(handle);
        if (it == objects_.end() || it->second->type != ObjectType::Semaphore) {
            return -1;
        }
        sem = std::static_pointer_cast<SemaphoreObject>(it->second);
    }

    std::lock_guard<std::mutex> lock(sem->obj_mutex);
    int old = sem->count.load();
    if (old + count > sem->max_count) {
        return -1;
    }
    sem->count.fetch_add(count);
    if (old == 0 && count > 0) sem->obj_cond.notify_all();
    return old;
}

bool ThetaServer::acquire_mutex(uint64_t handle) {
    std::shared_ptr<MutexObject> mutex;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = objects_.find(handle);
        if (it == objects_.end() || it->second->type != ObjectType::Mutex) {
            return false;
        }
        mutex = std::static_pointer_cast<MutexObject>(it->second);
    }

    uint64_t current = ThreadManager::instance().get_current_thread();

    std::lock_guard<std::mutex> lock(mutex->obj_mutex);
    if (mutex->owner == current) {
        mutex->recursion_count++;
        return true;
    }
    if (mutex->owner == 0) {
        mutex->owner = current;
        mutex->recursion_count = 1;
        return true;
    }
    return false; // Held by another thread; WaitForSingleObject blocks instead.
}

bool ThetaServer::release_mutex(uint64_t handle) {
    std::shared_ptr<MutexObject> mutex;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = objects_.find(handle);
        if (it == objects_.end() || it->second->type != ObjectType::Mutex) {
            return false;
        }
        mutex = std::static_pointer_cast<MutexObject>(it->second);
    }

    uint64_t current = ThreadManager::instance().get_current_thread();

    std::lock_guard<std::mutex> lock(mutex->obj_mutex);
    if (mutex->owner != current) {
        return false;
    }
    mutex->recursion_count--;
    if (mutex->recursion_count == 0) {
        mutex->owner = 0;
        mutex->obj_cond.notify_all();
    }
    return true;
}

uint32_t ThetaServer::wait_for_object(uint64_t handle, uint64_t timeout_ms) {
    std::shared_ptr<ThetaServerObject> obj;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = objects_.find(handle);
        if (it == objects_.end()) return wait_status::Failed;
        obj = it->second;
    }

    switch (obj->type) {
    case ObjectType::Event: {
        auto* e = static_cast<EventObject*>(obj.get());
        std::unique_lock<std::mutex> lock(e->obj_mutex);
        uint64_t seen = e->pulse_count.load();
        auto pred = [&] { return e->signaled.load() || e->pulse_count.load() != seen; };
        if (!pred()) {
            if (timeout_ms == wait_status::Infinite) e->obj_cond.wait(lock, pred);
            else if (timeout_ms == 0) return wait_status::Timeout;
            else if (!e->obj_cond.wait_for(lock, std::chrono::milliseconds(timeout_ms), pred))
                return wait_status::Timeout;
        }
        if (e->pulse_count.load() != seen) return wait_status::Object0;
        if (!e->manual_reset) e->signaled.store(false);
        return wait_status::Object0;
    }
    case ObjectType::Semaphore: {
        auto* s = static_cast<SemaphoreObject*>(obj.get());
        std::unique_lock<std::mutex> lock(s->obj_mutex);
        auto pred = [&] { return s->count.load() > 0; };
        if (!pred()) {
            if (timeout_ms == wait_status::Infinite) s->obj_cond.wait(lock, pred);
            else if (timeout_ms == 0) return wait_status::Timeout;
            else if (!s->obj_cond.wait_for(lock, std::chrono::milliseconds(timeout_ms), pred))
                return wait_status::Timeout;
        }
        s->count.fetch_sub(1);
        return wait_status::Object0;
    }
    case ObjectType::Mutex: {
        auto* m = static_cast<MutexObject*>(obj.get());
        uint64_t current = ThreadManager::instance().get_current_thread();
        std::unique_lock<std::mutex> lock(m->obj_mutex);
        auto pred = [&] { return m->owner == 0 || m->owner == current; };
        if (!pred()) {
            if (timeout_ms == wait_status::Infinite) m->obj_cond.wait(lock, pred);
            else if (timeout_ms == 0) return wait_status::Timeout;
            else if (!m->obj_cond.wait_for(lock, std::chrono::milliseconds(timeout_ms), pred))
                return wait_status::Timeout;
        }
        if (m->owner == current) {
            m->recursion_count++;
        } else {
            m->owner = current;
            m->recursion_count = 1;
        }
        return wait_status::Object0;
    }
    case ObjectType::Thread: {
        auto* t = static_cast<ThreadObject*>(obj.get());
        std::unique_lock<std::mutex> lock(t->obj_mutex);
        auto pred = [&] { return t->terminated; };
        if (!pred()) {
            if (timeout_ms == wait_status::Infinite) t->obj_cond.wait(lock, pred);
            else if (timeout_ms == 0) return wait_status::Timeout;
            else if (!t->obj_cond.wait_for(lock, std::chrono::milliseconds(timeout_ms), pred))
                return wait_status::Timeout;
        }
        return wait_status::Object0;
    }
    case ObjectType::Process: {
        auto* p = static_cast<ProcessObject*>(obj.get());
        std::unique_lock<std::mutex> lock(p->obj_mutex);
        auto pred = [&] { return p->terminated; };
        if (!pred()) {
            if (timeout_ms == wait_status::Infinite) p->obj_cond.wait(lock, pred);
            else if (timeout_ms == 0) return wait_status::Timeout;
            else if (!p->obj_cond.wait_for(lock, std::chrono::milliseconds(timeout_ms), pred))
                return wait_status::Timeout;
        }
        return wait_status::Object0;
    }
    case ObjectType::Timer: {
        auto* timer = static_cast<TimerObject*>(obj.get());
        std::unique_lock<std::mutex> lock(timer->obj_mutex);
        const bool timed = timeout_ms != wait_status::Infinite;
        const uint64_t wait_deadline = timed
            ? add_ms_saturating(steady_clock_ms(), timeout_ms)
            : UINT64_MAX;
        for (;;) {
            if (timer->signaled.load()) {
                if (!timer->is_manual) {
                    // Synchronization timer: a completed wait resets it.
                    timer->signaled.store(false);
                    if (timer->period > 0) {
                        timer->due_time.store(add_ms_saturating(
                            steady_clock_ms(), static_cast<uint64_t>(timer->period)));
                        timer->armed.store(true);
                    } else {
                        timer->armed.store(false);
                    }
                }
                return wait_status::Object0;
            }
            const uint64_t now = steady_clock_ms();
            if (timer->armed.load() && now >= timer->due_time.load()) {
                timer->signaled.store(true);
                continue;
            }
            if (timed && now >= wait_deadline) return wait_status::Timeout;
            uint64_t wake = wait_deadline;
            if (timer->armed.load()) {
                const uint64_t due = timer->due_time.load();
                if (due < wake) wake = due;
            }
            if (wake == UINT64_MAX) {
                timer->obj_cond.wait(lock);
            } else {
                timer->obj_cond.wait_until(
                    lock, std::chrono::steady_clock::time_point(
                              std::chrono::milliseconds(wake)));
            }
        }
    }
    default:
        return wait_status::Failed;
    }
}

bool ThetaServer::object_ready(uint64_t handle) {
    std::shared_ptr<ThetaServerObject> obj;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = objects_.find(handle);
        if (it == objects_.end()) return false;
        obj = it->second;
    }

    std::lock_guard<std::mutex> lock(obj->obj_mutex);
    switch (obj->type) {
    case ObjectType::Event:
        return static_cast<EventObject*>(obj.get())->signaled.load();
    case ObjectType::Semaphore:
        return static_cast<SemaphoreObject*>(obj.get())->count.load() > 0;
    case ObjectType::Mutex: {
        auto* m = static_cast<MutexObject*>(obj.get());
        uint64_t current = ThreadManager::instance().get_current_thread();
        return m->owner == 0 || m->owner == current;
    }
    case ObjectType::Thread:
        return static_cast<ThreadObject*>(obj.get())->terminated;
    case ObjectType::Process:
        return static_cast<ProcessObject*>(obj.get())->terminated;
    case ObjectType::Timer: {
        auto* timer = static_cast<TimerObject*>(obj.get());
        if (timer->signaled.load()) return true;
        if (!timer->armed.load()) return false;
        if (steady_clock_ms() < timer->due_time.load()) return false;
        if (timer->is_manual) {
            timer->signaled.store(true);
            if (timer->period > 0)
                timer->due_time.store(add_ms_saturating(
                    steady_clock_ms(), static_cast<uint64_t>(timer->period)));
            else
                timer->armed.store(false);
        }
        return true;
    }
    default:
        return false;
    }
}

uint32_t ThetaServer::wait_for_any(const std::vector<uint64_t>& handles, uint64_t timeout_ms) {
    for (uint64_t h : handles) {
        bool exists = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            exists = objects_.count(h) != 0;
        }
        if (!exists) return wait_status::Failed;
    }

    uint64_t deadline = 0;
    if (timeout_ms != wait_status::Infinite) deadline = steady_clock_ms() + timeout_ms;

    for (;;) {
        for (size_t i = 0; i < handles.size(); i++) {
            uint32_t result = wait_for_object(handles[i], 0);
            if (result == wait_status::Timeout || result == wait_status::Failed) continue;
            return wait_status::Object0 + i;
        }
        if (timeout_ms != wait_status::Infinite && steady_clock_ms() >= deadline) {
            return wait_status::Timeout;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

uint32_t ThetaServer::wait_for_all(const std::vector<uint64_t>& handles, uint64_t timeout_ms) {
    for (uint64_t h : handles) {
        bool exists = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            exists = objects_.count(h) != 0;
        }
        if (!exists) return wait_status::Failed;
    }

    uint64_t deadline = 0;
    if (timeout_ms != wait_status::Infinite) deadline = steady_clock_ms() + timeout_ms;

    for (;;) {
        bool all_ready = true;
        for (uint64_t h : handles) {
            if (!object_ready(h)) {
                all_ready = false;
                break;
            }
        }
        if (all_ready) {
            bool consumed = true;
            for (uint64_t h : handles) {
                if (wait_for_object(h, 0) == wait_status::Timeout) {
                    consumed = false;
                    break;
                }
            }
            if (consumed) return wait_status::Object0;
        }
        if (timeout_ms != wait_status::Infinite && steady_clock_ms() >= deadline) {
            return wait_status::Timeout;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

ProcessObject* ThetaServer::get_process(uint64_t pid) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = processes_.find(pid);
    if (it != processes_.end()) {
        return it->second.get();
    }
    return nullptr;
}

ThreadObject* ThetaServer::get_thread(uint64_t tid) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = threads_.find(tid);
    if (it != threads_.end()) {
        return it->second.get();
    }
    return nullptr;
}

uint32_t ThetaServer::get_thread_exit_code(uint64_t tid) {
    ThreadObject* t = get_thread(tid);
    if (!t || !t->terminated) return 0x103; // STILL_ACTIVE
    return t->exit_code;
}

std::vector<uint64_t> ThetaServer::get_waitable_handles(uint64_t pid, ObjectType filter_type) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<uint64_t> handles;
    auto table_it = handle_tables_.find(pid);
    if (table_it == handle_tables_.end()) return handles;

    for (uint64_t handle : table_it->second) {
        auto obj_it = objects_.find(handle);
        if (obj_it != objects_.end() && (filter_type == ObjectType::None || obj_it->second->type == filter_type)) {
            handles.push_back(handle);
        }
    }

    return handles;
}

void ThetaServer::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    objects_.clear();
    processes_.clear();
    threads_.clear();
    handle_tables_.clear();
}

} // namespace ThetaZ
