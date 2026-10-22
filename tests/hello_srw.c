// ThetaZ SRW lock test: exclusive vs shared semantics across two threads.
#include <windows.h>
#include <stdio.h>

static SRWLOCK g_lock = SRWLOCK_INIT;
static volatile DWORD shared_writer_value = 0;
static volatile DWORD writer_ok = 0;

static DWORD WINAPI exclusive_writer(void* arg) {
    (void)arg;
    AcquireSRWLockExclusive(&g_lock);
    shared_writer_value = 0x1111;
    // While held exclusively, a TryAcquire from this same thread fails only for
    // shared (recursion allowed) — we just check the value is ours on release.
    Sleep(40);
    writer_ok = (shared_writer_value == 0x1111);
    ReleaseSRWLockExclusive(&g_lock);
    return 0;
}

int main(void) {
    // Shared locks coexist.
    AcquireSRWLockShared(&g_lock);
    AcquireSRWLockShared(&g_lock);
    printf("srw: two shared holders\n");
    // Exclusive must block while a shared lock is held: TryAcquire fails.
    BOOL try_excl = TryAcquireSRWLockExclusive(&g_lock);
    printf("srw_try_excl_blocked=%d\n", try_excl);
    ReleaseSRWLockShared(&g_lock);
    ReleaseSRWLockShared(&g_lock);

    // Now exclusive is available.
    AcquireSRWLockExclusive(&g_lock);
    printf("srw: exclusive held\n");
    BOOL try_sh = TryAcquireSRWLockShared(&g_lock);
    printf("srw_try_shared_blocked=%d\n", try_sh);
    ReleaseSRWLockExclusive(&g_lock);

    // Writer thread runs while main only reads shared.
    HANDLE h = CreateThread(NULL, 0, exclusive_writer, NULL, 0, NULL);
    WaitForSingleObject(h, 5000);

    printf("srw_writer_ok=%lu\n", (unsigned long)writer_ok);
    printf("srw_done\n");
    return 0;
}
