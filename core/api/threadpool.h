// ThetaZ — Threadpool API declarations
// Copyright (c) 2026 ThetaZ Project. MIT License.

#pragma once

#include "core/api/kernel32.h"

namespace ThetaZ {

// =========================================================================
// Threadpool types
// =========================================================================

// Threadpool object types
enum ThreadpoolObjectType {
    TP_OBJECT_TYPE_SIMPLE = 0,
    TP_OBJECT_TYPE_WORK = 1,
    TP_OBJECT_TYPE_TIMER = 2,
    TP_OBJECT_TYPE_WAIT = 3,
    TP_OBJECT_TYPE_IO = 4,
};

// TP_WAIT_RESULT
enum TP_WAIT_RESULT {
    WAIT_OBJECT_0 = 0,
    WAIT_TIMEOUT = 258,
    WAIT_FAILED = 0xFFFFFFFF,
};

// Threadpool object types (opaque pointers for guest visibility)
struct TP_TIMER {
    uint32_t callback;        // guest function pointer
    PVOID userdata;
    void* environment;
    FILETIME due_time;
    DWORD period;             // milliseconds, 0 = one-shot
    DWORD window_length;      // coalescing window
    bool active;
    uint64_t timer_id;        // for g_timers map
};

struct TP_WAIT {
    uint32_t callback;        // guest function pointer
    PVOID userdata;
    void* environment;
    HANDLE handle;
    FILETIME* timeout;
    bool active;
};

struct TP_WORK {
    uint32_t callback;        // guest function pointer
    PVOID userdata;
    void* environment;
    bool pending;
};

struct TP_IO {
    ThreadpoolObjectType type;
    uint32_t callback;        // guest function pointer
    HANDLE handle;
    PVOID userdata;
    void* environment;
    bool started;
};

// Type aliases
using TP_POOL = void*;
using PTP_POOL = TP_POOL*;
using TP_CG = void*;
using PTP_CG = TP_CG*;
using TP_CLEANUP_GROUP = void*;
using PTP_CLEANUP_GROUP = void*;

using PTP_TIMER = void*;
using PTP_WAIT = void*;
using PTP_WORK = void*;
using PTP_IO = TP_IO*;

using PTP_CALLBACK_ENVIRON = void*;

// =========================================================================
// Legacy threadpool API (kernel32_batch.cpp)
// =========================================================================
PTP_POOL WINAPI_CALL THETAZ_CreateThreadpool(void* r);
PTP_CG WINAPI_CALL THETAZ_CreateThreadpoolCleanupGroup();
PTP_IO WINAPI_CALL THETAZ_CreateThreadpoolIo(HANDLE f, void* c, void* x, void* e);
void WINAPI_CALL THETAZ_SetThreadpoolThreadMaximum(void* p, DWORD n);
BOOL WINAPI_CALL THETAZ_SetThreadpoolThreadMinimum(void* p, DWORD n);
void WINAPI_CALL THETAZ_CloseThreadpoolIo(void* i);
void WINAPI_CALL THETAZ_StartThreadpoolIo(void* i);
void WINAPI_CALL THETAZ_CancelThreadpoolIo(void* i);
void WINAPI_CALL THETAZ_FreeLibraryWhenCallbackReturns(void* m, void* c);
BOOL WINAPI_CALL THETAZ_TrySubmitThreadpoolCallback(void* c, void* x, void* e);
void WINAPI_CALL THETAZ_WaitForThreadpoolWorkCallbacks(void* w, BOOL c);

BOOL WINAPI_CALL THETAZ_InitializeProcThreadAttributeList(void* l, DWORD c, DWORD f, SIZE_T* s);
BOOL WINAPI_CALL THETAZ_UpdateProcThreadAttribute(void* l, DWORD f, DWORD a, void* v, SIZE_T sz, void* p, void* r);

// =========================================================================
// New threadpool API (kernel32.h / kernel32_runtime_unique.cpp)
// =========================================================================
void* WINAPI_CALL THETAZ_CreateThreadpoolTimer(void* pfnti, void* pv, void* pcbe);
void* WINAPI_CALL THETAZ_CreateThreadpoolWait(void* pfnwa, void* pv, void* pcbe);
void* WINAPI_CALL THETAZ_CreateThreadpoolWork(void* pfnwk, void* pv, void* pcbe);
void WINAPI_CALL THETAZ_CloseThreadpoolTimer(void* pti);
void WINAPI_CALL THETAZ_CloseThreadpoolWait(void* pwa);
void WINAPI_CALL THETAZ_CloseThreadpoolWork(void* pwk);
void WINAPI_CALL THETAZ_SetThreadpoolTimer(void* pti, const FILETIME* pftDueTime, DWORD msPeriod, DWORD msWindowLength);
void WINAPI_CALL THETAZ_SetThreadpoolWait(void* pwa, HANDLE hObject, const FILETIME* pftTimeout);
void WINAPI_CALL THETAZ_SubmitThreadpoolWork(void* pwk);
void WINAPI_CALL THETAZ_WaitForThreadpoolTimerCallbacks(void* pti, BOOL fCancelPendingCallbacks);
void WINAPI_CALL THETAZ_WaitForThreadpoolWaitCallbacks(void* pwa, BOOL fCancelPendingCallbacks);

// =========================================================================
// Threadpool cleanup (kernel32.h)
// =========================================================================
void WINAPI_CALL THETAZ_CloseThreadpool(void* ptpp);
void WINAPI_CALL THETAZ_CloseThreadpoolCleanupGroup(void* pcg);
void WINAPI_CALL THETAZ_CloseThreadpoolCleanupGroupMembers(void* pcg, BOOL fCancelPendingCallbacks, void* pvCleanupContext);

} // namespace ThetaZ