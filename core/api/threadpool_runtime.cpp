// ThetaZ — Threadpool API implementations
// Copyright (c) 2026 ThetaZ Project. MIT License.

#include "core/api/api.h"
#include "core/api/kernel32.h"
#include "core/api/threadpool.h"
#include "core/api/window.h"
#include "core/x86/x86.h"

#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

namespace ThetaZ {

// =========================================================================
// Threadpool object management
// =========================================================================

static std::mutex g_threadpool_mutex;
static std::unordered_map<uint64_t, void*> g_timers_map;
static std::unordered_map<uint64_t, void*> g_waits_map;
static std::unordered_map<uint64_t, void*> g_work_map;
static std::unordered_map<uint64_t, void*> g_io_map;
static std::atomic<uint64_t> g_next_wait_id{1};
static std::atomic<uint64_t> g_next_work_id{1};
static std::atomic<uint64_t> g_next_io_id{1};

// Work queue for threadpool worker
static std::mutex g_work_queue_mutex;
static std::condition_variable g_work_queue_cv;
static std::vector<void*> g_work_queue;
static std::atomic<bool> g_worker_stop{false};
static std::thread g_worker_thread;

// Forward declarations
static void worker_thread_main();

// Timer structure
struct TimerInfo {
    uint32_t callback;
    void* userdata;
    void* environment;
    FILETIME due_time;
    DWORD period;
    DWORD window_length;
    bool active;
    uint64_t timer_id;
};

// Wait structure
struct WaitInfo {
    uint32_t callback;
    void* userdata;
    void* environment;
    HANDLE handle;
    FILETIME* timeout;
    bool active;
    uint64_t timer_id;
};

// Work structure
struct WorkInfo {
    uint32_t callback;
    void* userdata;
    void* environment;
    bool pending;
    uint64_t timer_id;
};

// IO structure
struct IOInfo {
    uint32_t callback;
    HANDLE handle;
    void* userdata;
    void* environment;
    bool started;
    uint64_t timer_id;
};

// =========================================================================
// Helper: Get current time in milliseconds
// =========================================================================
static uint64_t get_time_ms() {
    using namespace std::chrono;
    return duration_cast<std::chrono::milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// =========================================================================
// Helper: Convert FILETIME to milliseconds from now
// =========================================================================
static int64_t filetime_to_ms_from_now(const FILETIME* ft) {
    if (!ft) return -1;
    
    uint64_t ft_val = (static_cast<uint64_t>(ft->dwHighDateTime) << 32) | ft->dwLowDateTime;
    
    const uint64_t EPOCH_DIFF = 116444736000000000ULL;
    uint64_t ft_ms = (ft_val - EPOCH_DIFF) / 10000;
    
    int64_t now_ms = get_time_ms();
    int64_t diff = static_cast<int64_t>(ft_ms) - now_ms;
    
    return diff > 0 ? diff : 0;
}

// =========================================================================
// Timer implementation (integrates with existing window.cpp timer system)
// =========================================================================

void* WINAPI_CALL THETAZ_CreateThreadpoolTimer(void* pfnti, void* pv, void* pcbe) {
    (void)pcbe;
    
    TimerInfo* timer = new TimerInfo();
    timer->callback = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pfnti));
    timer->userdata = pv;
    timer->environment = pcbe;
    timer->active = false;
    timer->timer_id = 0;
    timer->period = 0;
    timer->window_length = 0;
    
    std::lock_guard<std::mutex> lock(g_threadpool_mutex);
    timer->timer_id = g_next_timer_id++;
    g_timers_map[timer->timer_id] = timer;
    
    return timer;
}

void WINAPI_CALL THETAZ_CloseThreadpoolTimer(void* pti) {
    if (!pti) return;
    
    TimerInfo* timer = static_cast<TimerInfo*>(pti);
    
    std::lock_guard<std::mutex> lock(g_threadpool_mutex);
    auto it = g_timers_map.find(timer->timer_id);
    if (it != g_timers_map.end()) {
        // Remove from window timer system if active
        if (timer->active && timer->timer_id) {
            extern std::unordered_map<uint64_t, GuestTimerInfo> g_timers;
            extern std::mutex g_window_mutex;
            extern std::condition_variable g_timer_cv;
            
            std::lock_guard<std::mutex> wlock(g_window_mutex);
            g_timers.erase(timer->timer_id);
            g_timer_cv.notify_all();
        }
        
        delete it->second;
        g_timers_map.erase(it);
    }
}

void WINAPI_CALL THETAZ_SetThreadpoolTimer(void* pti, const FILETIME* pftDueTime, DWORD msPeriod, DWORD msWindowLength) {
    if (!pti) return;
    
    TimerInfo* timer = static_cast<TimerInfo*>(pti);
    
    std::lock_guard<std::mutex> lock(g_threadpool_mutex);
    
    // Remove existing timer if active
    if (timer->active && timer->timer_id) {
        extern std::unordered_map<uint64_t, GuestTimerInfo> g_timers;
        extern std::mutex g_window_mutex;
        extern std::condition_variable g_timer_cv;
        
        std::lock_guard<std::mutex> wlock(g_window_mutex);
        g_timers.erase(timer->timer_id);
    }
    
    int64_t due_ms = timer->due_time.dwLowDateTime ? filetime_to_ms_from_now(&timer->due_time) : 0;
    if (due_ms < 0) due_ms = 0;
    
    timer->period = msPeriod;
    timer->window_length = msWindowLength;
    timer->active = true;
    
    // Register with window.cpp's timer system
    extern std::unordered_map<uint64_t, GuestTimerInfo> g_timers;
    extern std::mutex g_window_mutex;
    extern std::condition_variable g_timer_cv;
    
    uint64_t now = get_time_ms();
    uint64_t due_time = now + static_cast<uint64_t>(due_ms);
    
    GuestTimerInfo info;
    info.hwnd = 0;
    info.id = static_cast<uint32_t>(timer->timer_id);
    info.period_ms = msPeriod;
    info.due_ms = due_time;
    info.func = timer->callback;
    
    std::lock_guard<std::mutex> wlock(g_window_mutex);
    g_timers[timer->timer_id] = info;
    g_timer_cv.notify_all();
}

void WINAPI_CALL THETAZ_WaitForThreadpoolTimerCallbacks(void* pti, BOOL fCancelPendingCallbacks) {
    if (!pti) return;
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    if (fCancelPendingCallbacks) {
        WINAPI_CALL THETAZ_CloseThreadpoolTimer(pti);
    }
}

// =========================================================================
// Wait implementation
// =========================================================================

void* WINAPI_CALL THETAZ_CreateThreadpoolWait(void* pfnwa, void* pv, void* pcbe) {
    WaitInfo* wait = new WaitInfo();
    wait->callback = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pfnwa));
    wait->userdata = pv;
    wait->environment = pcbe;
    wait->handle = nullptr;
    wait->timeout = nullptr;
    wait->active = false;
    
    std::lock_guard<std::mutex> lock(g_threadpool_mutex);
    wait->timer_id = g_next_wait_id++;
    g_waits_map[wait->timer_id] = wait;
    
    return wait;
}

void WINAPI_CALL THETAZ_CloseThreadpoolWait(void* pwa) {
    if (!pwa) return;
    
    WaitInfo* wait = static_cast<WaitInfo*>(pwa);
    
    std::lock_guard<std::mutex> lock(g_threadpool_mutex);
    auto it = g_waits_map.find(wait->timer_id);
    if (it != g_waits_map.end()) {
        delete it->second;
        g_waits_map.erase(it);
    }
}

void WINAPI_CALL THETAZ_SetThreadpoolWait(void* pwa, HANDLE hObject, const FILETIME* pftTimeout) {
    if (!pwa) return;
    
    WaitInfo* wait = static_cast<WaitInfo*>(pwa);
    wait->handle = hObject;
    wait->timeout = const_cast<FILETIME*>(pftTimeout);
    wait->active = true;
}

void WINAPI_CALL THETAZ_WaitForThreadpoolWaitCallbacks(void* pwa, BOOL fCancelPendingCallbacks) {
    if (!pwa) return;
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    if (fCancelPendingCallbacks) {
        WINAPI_CALL THETAZ_CloseThreadpoolWait(pwa);
    }
}

// =========================================================================
// Work implementation (with dedicated worker thread)
// =========================================================================

void* WINAPI_CALL THETAZ_CreateThreadpoolWork(void* pfnwk, void* pv, void* pcbe) {
    WorkInfo* work = new WorkInfo();
    work->callback = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pfnwk));
    work->userdata = pv;
    work->environment = pcbe;
    work->pending = false;
    
    std::lock_guard<std::mutex> lock(g_threadpool_mutex);
    work->timer_id = g_next_work_id++;
    g_work_map[work->timer_id] = work;
    
    // Start worker thread if not already running
    if (!g_worker_thread.joinable()) {
        g_worker_stop = false;
        g_worker_thread = std::thread(worker_thread_main);
    }
    
    return work;
}

void WINAPI_CALL THETAZ_CloseThreadpoolWork(void* pwk) {
    if (!pwk) return;
    
    WorkInfo* work = static_cast<WorkInfo*>(pwk);
    
    std::lock_guard<std::mutex> lock(g_threadpool_mutex);
    auto it = g_work_map.find(work->timer_id);
    if (it != g_work_map.end()) {
        delete it->second;
        g_work_map.erase(it);
    }
}

void WINAPI_CALL THETAZ_SubmitThreadpoolWork(void* pwk) {
    if (!pwk) return;
    
    WorkInfo* work = static_cast<WorkInfo*>(pwk);
    work->pending = true;
    
    {
        std::lock_guard<std::mutex> lock(g_work_queue_mutex);
        g_work_queue.push_back(work);
    }
    g_work_queue_cv.notify_one();
}

void WINAPI_CALL THETAZ_WaitForThreadpoolWorkCallbacks(void* pwk, BOOL fCancelPendingCallbacks) {
    if (!pwk) return;
    
    WorkInfo* work = static_cast<WorkInfo*>(pwk);
    
    while (work->pending) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    if (fCancelPendingCallbacks) {
        WINAPI_CALL THETAZ_CloseThreadpoolWork(pwk);
    }
}

static void worker_thread_main() {
    while (!g_worker_stop) {
        WorkInfo* work = nullptr;
        
        {
            std::unique_lock<std::mutex> lock(g_work_queue_mutex);
            g_work_queue_cv.wait(lock, [] { return g_worker_stop || !g_work_queue.empty(); });
            
            if (g_worker_stop && g_work_queue.empty()) break;
            
            if (!g_work_queue.empty()) {
                work = static_cast<WorkInfo*>(g_work_queue.front());
                g_work_queue.erase(g_work_queue.begin());
            }
        }
        
        if (work) {
            work->pending = true;
            
            if (work->callback) {
                uint32_t args[3] = {
                    0,  // PTP_CALLBACK_INSTANCE instance
                    static_cast<uint32_t>(reinterpret_cast<uintptr_t>(work->userdata)),
                    static_cast<uint32_t>(reinterpret_cast<uintptr_t>(work))
                };
                x86_call_guest_args(work->callback, args, 3);
            }
            
            work->pending = false;
        }
    }
}

// =========================================================================
// Threadpool cleanup implementations
// =========================================================================

// Forward declaration
static void threadpool_shutdown();

void WINAPI_CALL THETAZ_CloseThreadpool(void* ptpp) {
    (void)ptpp;
    // In a full implementation, this would close the threadpool and wait for all threads to complete
    // For now, we just clean up our internal state
    threadpool_shutdown();
}

void WINAPI_CALL THETAZ_CloseThreadpoolCleanupGroup(void* pcg) {
    (void)pcg;
    // In a full implementation, this would close the cleanup group
    // For now, we just acknowledge the call
}

void WINAPI_CALL THETAZ_CloseThreadpoolCleanupGroupMembers(void* pcg, BOOL fCancelPendingCallbacks, void* pvCleanupContext) {
    (void)pcg;
    if (fCancelPendingCallbacks) {
        // Cancel any pending callbacks in the cleanup group
        // This would iterate through work items and cancel them
    }
    (void)pvCleanupContext;
}

// =========================================================================
// Cleanup on shutdown
// =========================================================================
void threadpool_shutdown() {
    g_worker_stop = true;
    g_work_queue_cv.notify_all();
    
    if (g_worker_thread.joinable()) {
        g_worker_thread.join();
    }
    
    // Clean up all objects
    std::lock_guard<std::mutex> lock(g_threadpool_mutex);
    
    for (auto& [id, timer] : g_timers_map) {
        delete static_cast<TimerInfo*>(timer);
    }
    g_timers_map.clear();
    
    for (auto& [id, wait] : g_waits_map) {
        delete static_cast<WaitInfo*>(wait);
    }
    g_waits_map.clear();
    
    for (auto& [id, work] : g_work_map) {
        delete static_cast<WorkInfo*>(work);
    }
    g_work_map.clear();
    
    for (auto& [id, io] : g_io_map) {
        delete static_cast<IOInfo*>(io);
    }
    g_io_map.clear();
}

} // namespace ThetaZ