// ThetaZ — Copyright (c) 2026 ThetaZ Project. MIT License.

#include "core/ntdll/sync.h"
#include "core/ntdll/thread.h"

#include <thread>

namespace ThetaZ {

void Semaphore::wait() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (count_ == 0) {
        cond_.wait(lock);
    }
    count_--;
}

bool Semaphore::wait_for(uint32_t milliseconds) {
    std::unique_lock<std::mutex> lock(mutex_);
    while (count_ == 0) {
        if (cond_.wait_for(lock, std::chrono::milliseconds(milliseconds)) == std::cv_status::timeout) {
            return false;
        }
    }
    count_--;
    return true;
}

void Semaphore::signal(uint32_t count) {
    std::lock_guard<std::mutex> lock(mutex_);
    count_ += count;
    cond_.notify_all();
}

void Event::set() {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = true;
    if (manual_reset_) {
        cond_.notify_all();
    } else {
        cond_.notify_one();
    }
}

void Event::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = false;
}

void Event::wait() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!state_) {
        cond_.wait(lock);
    }
}

bool Event::wait_for(uint32_t milliseconds) {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!state_) {
        if (cond_.wait_for(lock, std::chrono::milliseconds(milliseconds)) == std::cv_status::timeout) {
            return false;
        }
    }
    return true;
}

uint64_t CriticalSection::get_current_thread() const {
    return ThreadManager::instance().get_current_thread();
}

void CriticalSection::enter() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (lock_count_ > 0 && owner_ != get_current_thread()) {
        cond_.wait(lock);
    }
    lock_count_++;
    owner_ = get_current_thread();
    recursion_count_++;
}

void CriticalSection::leave() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (--recursion_count_ == 0) {
        lock_count_--;
        owner_ = 0;
        cond_.notify_one();
    }
}

bool CriticalSection::try_enter() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (lock_count_ > 0 && owner_ != get_current_thread()) {
        return false;
    }
    lock_count_++;
    owner_ = get_current_thread();
    recursion_count_++;
    return true;
}

uint64_t Mutex::get_current_thread() const {
    return ThreadManager::instance().get_current_thread();
}

void Mutex::lock() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (owner_ != 0 && owner_ != get_current_thread()) {
        cond_.wait(lock);
    }
    owner_ = get_current_thread();
    recursion_count_++;
}

void Mutex::unlock() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (--recursion_count_ == 0) {
        owner_ = 0;
        cond_.notify_one();
    }
}

bool Mutex::try_lock() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (owner_ != 0 && owner_ != get_current_thread()) {
        return false;
    }
    owner_ = get_current_thread();
    recursion_count_++;
    return true;
}

void SpinLock::lock() {
    while (locked_.exchange(true)) {
        std::this_thread::yield();
    }
}

void SpinLock::unlock() {
    locked_.store(false);
}

void ReaderWriterLock::read_lock() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (exclusive_ || writers_ > 0) {
        readers_cond_.wait(lock);
    }
    readers_++;
}

void ReaderWriterLock::read_unlock() {
    std::lock_guard<std::mutex> lock(mutex_);
    readers_--;
    if (readers_ == 0) {
        writers_cond_.notify_all();
    }
}

void ReaderWriterLock::write_lock() {
    std::unique_lock<std::mutex> lock(mutex_);
    writers_++;
    while (exclusive_ || readers_ > 0) {
        writers_cond_.wait(lock);
    }
    exclusive_ = true;
    writers_--;
}

void ReaderWriterLock::write_unlock() {
    std::lock_guard<std::mutex> lock(mutex_);
    exclusive_ = false;
    readers_cond_.notify_all();
    writers_cond_.notify_all();
}

bool ReaderWriterLock::try_read_lock() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (exclusive_ || writers_ > 0) return false;
    readers_++;
    return true;
}

bool ReaderWriterLock::try_write_lock() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (exclusive_ || readers_ > 0 || writers_ > 0) return false;
    exclusive_ = true;
    return true;
}

WaitHandle WaitHandle::create_semaphore(uint32_t initial) {
    WaitHandle handle;
    handle.type = TypeSemaphore;
    handle.semaphore = std::make_shared<Semaphore>(initial);
    return handle;
}

WaitHandle WaitHandle::create_event(bool manual_reset, bool initial) {
    WaitHandle handle;
    handle.type = TypeEvent;
    handle.event = std::make_shared<Event>(manual_reset, initial);
    return handle;
}

WaitHandle WaitHandle::create_mutex(bool initial) {
    WaitHandle handle;
    handle.type = TypeMutex;
    handle.mutex = std::make_shared<Mutex>();
    (void)initial;
    return handle;
}

} // namespace ThetaZ
