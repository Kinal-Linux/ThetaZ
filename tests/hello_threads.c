// ThetaZ real-thread test: CreateThread/Join/ExitCode, cross-thread events,
// semaphore timeout, blocking mutex wait.
#include <windows.h>
#include <stdio.h>
#include <stdint.h>

static volatile LONG counter = 0;
static volatile DWORD worker_tid = 0;

static DWORD WINAPI worker(void* arg) {
    worker_tid = GetCurrentThreadId();
    for (LONG i = 0; i < 100; i++) InterlockedIncrement(&counter);
    return (DWORD)(uintptr_t)arg;
}

static HANDLE g_event;
static DWORD WINAPI event_worker(void* arg) {
    (void)arg;
    Sleep(50);
    SetEvent(g_event);
    return 0;
}

static HANDLE g_mutex;
static HANDLE g_mutex_held;
static DWORD WINAPI mutex_worker(void* arg) {
    (void)arg;
    WaitForSingleObject(g_mutex, INFINITE);
    SetEvent(g_mutex_held);
    Sleep(200);
    ReleaseMutex(g_mutex);
    return 0;
}

int main(void) {
    printf("main_tid=%lu\n", (unsigned long)GetCurrentThreadId());

    // CreateThread + join + exit code
    DWORD tid = 0;
    HANDLE h = CreateThread(NULL, 0, worker, (void*)(uintptr_t)7, 0, &tid);
    if (!h) { printf("thread_fail\n"); return 1; }
    DWORD join_wait = WaitForSingleObject(h, 5000);
    printf("join_wait=%lu\n", (unsigned long)join_wait);
    DWORD code = 0;
    GetExitCodeThread(h, &code);
    printf("exit_code=%lu\n", (unsigned long)code);
    printf("counter=%ld\n", (long)counter);
    printf("worker_tid=%lu\n", (unsigned long)worker_tid);
    CloseHandle(h);

    // Event signaled from another thread
    g_event = CreateEventA(NULL, FALSE, FALSE, NULL);
    HANDLE he = CreateThread(NULL, 0, event_worker, NULL, 0, NULL);
    DWORD ev_wait = WaitForSingleObject(g_event, 5000);
    printf("event_wait=%lu\n", (unsigned long)ev_wait);
    WaitForSingleObject(he, 5000);
    CloseHandle(he);
    CloseHandle(g_event);

    // Semaphore timeout + release
    HANDLE sem = CreateSemaphoreA(NULL, 0, 1, NULL);
    DWORD sem_timeout = WaitForSingleObject(sem, 100);
    printf("sem_timeout=%lu\n", (unsigned long)sem_timeout);
    LONG prev = -1;
    ReleaseSemaphore(sem, 1, &prev);
    DWORD sem_acquire = WaitForSingleObject(sem, 100);
    printf("sem_acquire=%lu prev=%ld\n", (unsigned long)sem_acquire, (long)prev);
    CloseHandle(sem);

    // Mutex: worker holds it; main times out, then acquires after release
    g_mutex = CreateMutexA(NULL, FALSE, NULL);
    g_mutex_held = CreateEventA(NULL, FALSE, FALSE, NULL);
    HANDLE hm = CreateThread(NULL, 0, mutex_worker, NULL, 0, NULL);
    WaitForSingleObject(g_mutex_held, 5000);
    DWORD mtx_timeout = WaitForSingleObject(g_mutex, 100);
    printf("mutex_timeout=%lu\n", (unsigned long)mtx_timeout);
    DWORD mtx_acquire = WaitForSingleObject(g_mutex, 5000);
    printf("mutex_acquire=%lu\n", (unsigned long)mtx_acquire);
    ReleaseMutex(g_mutex);
    WaitForSingleObject(hm, 5000);
    CloseHandle(hm);
    CloseHandle(g_mutex);
    CloseHandle(g_mutex_held);

    printf("threads_done\n");
    return 0;
}
