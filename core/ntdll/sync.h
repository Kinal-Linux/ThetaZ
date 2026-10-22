// ThetaZ — Copyright (c) 2026 ThetaZ Project. MIT License.

#pragma once
#include <cstdint>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <atomic>

namespace ThetaZ {

class Semaphore {
public:
    explicit Semaphore(uint32_t initial_count = 0) : count_(initial_count) {}

    void wait();
    bool wait_for(uint32_t milliseconds);
    void signal(uint32_t count = 1);
    uint32_t value() const { return count_; }

private:
    mutable std::mutex mutex_;
    std::condition_variable cond_;
    uint32_t count_;
};

class Event {
public:
    Event(bool manual_reset = false, bool initial_state = false)
        : manual_reset_(manual_reset), state_(initial_state) {}

    void set();
    void reset();
    void wait();
    bool wait_for(uint32_t milliseconds);

private:
    mutable std::mutex mutex_;
    std::condition_variable cond_;
    bool manual_reset_;
    bool state_;
};

class CriticalSection {
public:
    CriticalSection() : lock_count_(0), owner_(0), recursion_count_(0) {}

    void enter();
    void leave();
    bool try_enter();

private:
    uint64_t get_current_thread() const;
    mutable std::mutex mutex_;
    mutable std::condition_variable cond_;
    int lock_count_;
    uint64_t owner_;
    int recursion_count_;
};

class Mutex {
public:
    Mutex() : owner_(0), recursion_count_(0) {}

    void lock();
    void unlock();
    bool try_lock();

private:
    uint64_t get_current_thread() const;
    mutable std::mutex mutex_;
    mutable std::condition_variable cond_;
    uint64_t owner_;
    int recursion_count_;
};

class SpinLock {
public:
    SpinLock() : locked_(false) {}
    void lock();
    void unlock();

private:
    std::atomic<bool> locked_;
};

class ReaderWriterLock {
public:
    ReaderWriterLock() : readers_(0), writers_(0), exclusive_(false) {}
    void read_lock();
    void read_unlock();
    void write_lock();
    void write_unlock();
    bool try_read_lock();
    bool try_write_lock();

private:
    mutable std::mutex mutex_;
    std::condition_variable readers_cond_;
    std::condition_variable writers_cond_;
    int readers_;
    int writers_;
    bool exclusive_;
};

struct WaitHandle {
    enum Type { TypeSemaphore, TypeEvent, TypeMutex, TypeCriticalSection };

    Type type;
    std::shared_ptr<Semaphore> semaphore;
    std::shared_ptr<Event> event;
    std::shared_ptr<Mutex> mutex;
    std::shared_ptr<CriticalSection> cs;

    static WaitHandle create_semaphore(uint32_t initial);
    static WaitHandle create_event(bool manual_reset, bool initial);
    static WaitHandle create_mutex(bool initial);
};

using HANDLE = WaitHandle;

} // namespace ThetaZ
