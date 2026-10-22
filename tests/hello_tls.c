// ThetaZ per-thread TLS test: TlsAlloc/Set/Get across two threads.
#include <windows.h>
#include <stdio.h>
#include <stdint.h>

static DWORD g_slot;
static volatile DWORD worker_val = 0;

static DWORD WINAPI tls_worker(void* arg) {
    TlsSetValue(g_slot, arg);
    Sleep(50);
    worker_val = (DWORD)(uintptr_t)TlsGetValue(g_slot);
    return 0;
}

int main(void) {
    g_slot = TlsAlloc();
    if (g_slot == 0xFFFFFFFF) { printf("tls_alloc_fail\n"); return 1; }

    TlsSetValue(g_slot, (LPVOID)(uintptr_t)0xABCD);
    printf("slot=%lu\n", (unsigned long)g_slot);
    printf("main_tls=%lx\n", (unsigned long)(uintptr_t)TlsGetValue(g_slot));

    HANDLE h = CreateThread(NULL, 0, tls_worker, (void*)(uintptr_t)0x1234, 0, NULL);
    if (!h) { printf("tls_thread_fail\n"); return 2; }
    WaitForSingleObject(h, 5000);

    printf("worker_tls=%lx\n", (unsigned long)worker_val);
    printf("main_tls_after=%lx\n", (unsigned long)(uintptr_t)TlsGetValue(g_slot));

    TlsFree(g_slot);
    return 0;
}
