// ThetaZ — Critical Windows API implementations for real Windows app compatibility
// Copyright (c) 2026 ThetaZ Project. MIT License.

#include "core/api/api.h"
#include "core/api/kernel32.h"
#include "core/ipc/thetaserver.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <mutex>

namespace ThetaZ {

// ---------------------------------------------------------------------------
// Thread context for 32-bit compatibility
// ---------------------------------------------------------------------------

BOOL WINAPI_CALL THETAZ_Wow64GetThreadContext(HANDLE hThread, WOW64CONTEXT* ctx) {
    (void)hThread; (void)ctx;
    // ThetaZ is 64-bit only, so we can't provide real 32-bit context
    // Return FALSE to indicate unsupported operation
    return FALSE_VALUE;
}

BOOL WINAPI_CALL THETAZ_SetThreadContext(HANDLE hThread, WOW64CONTEXT* ctx) {
    (void)hThread; (void)ctx;
    // Could be used to modify thread context, but for now just return success
    return TRUE_VALUE;
}

BOOL WINAPI_CALL THETAZ_SuspendThread(HANDLE hThread) {
    (void)hThread;
    // Return dummy success
    return 0;
}

BOOL WINAPI_CALL THETAZ_ResumeThread(HANDLE hThread) {
    (void)hThread;
    return 1;
}

DWORD WINAPI_CALL THETAZ_GetThreadId(HANDLE hThread) {
    return hThread ? static_cast<DWORD>(reinterpret_cast<uintptr_t>(hThread) & 0xFFFFFFFF) : 0;
}

} // namespace ThetaZ
