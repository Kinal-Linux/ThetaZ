#include "core/api/kernel32.h"
#include "core/exception/exception.h"
#include "core/loader/loader.h"
#include "core/ntdll/thread.h"
#include "core/x86/api_dispatch.h"

#include "core/api/path_resolver.h"
#include "core/api/environment.h"

#include <chrono>
#include <clocale>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <thread>
#include <ctime>
#include <cwchar>
#include <mutex>
#include <unordered_map>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <cstdint>

namespace ThetaZ {

using ThetaZ::PathResolver;

// Global singleton for path resolver
static ThetaZ::PathResolver* g_path_resolver = nullptr;

using ThetaZ::LPDWORD;

namespace {

thread_local DWORD g_last_error = 0;

std::unordered_map<CRITICAL_SECTION*, std::recursive_mutex>& critical_section_locks() {
    static std::unordered_map<CRITICAL_SECTION*, std::recursive_mutex> locks;
    return locks;
}

std::mutex& cs_map_mutex() {
    static std::mutex m;
    return m;
}

} // namespace

// ---------------------------------------------------------------------------
// Error handling
// ---------------------------------------------------------------------------
DWORD WINAPI_CALL THETAZ_GetLastError() {
    return g_last_error;
}

void WINAPI_CALL THETAZ_SetLastError(DWORD dwErrCode) {
    g_last_error = dwErrCode;
}

// ---------------------------------------------------------------------------
// Handle / thread
// ---------------------------------------------------------------------------
BOOL WINAPI_CALL THETAZ_CloseHandle(HANDLE hObject) {
    (void)hObject;
    return TRUE;
}

HANDLE WINAPI_CALL THETAZ_GetCurrentProcess() {
    return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(-1));
}

HANDLE WINAPI_CALL THETAZ_GetCurrentThread() {
    return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(-2));
}

DWORD WINAPI_CALL THETAZ_GetCurrentThreadId() {
    return static_cast<DWORD>(ThreadManager::instance().get_current_thread());
}

HANDLE WINAPI_CALL THETAZ_CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes,
                                       SIZE_T dwStackSize,
                                       void* (*lpStartAddress)(void*),
                                       void* lpParameter,
                                       DWORD dwCreationFlags,
                                       LPDWORD lpThreadId) {
    (void)lpThreadAttributes;
    (void)dwCreationFlags;
    uint64_t tid = ThreadManager::instance().create_thread(
        reinterpret_cast<uint64_t>(lpStartAddress),
        reinterpret_cast<uint64_t>(lpParameter),
        static_cast<uint64_t>(dwStackSize));
    if (tid == 0) return nullptr;
    if (lpThreadId) *lpThreadId = static_cast<DWORD>(tid);
    return reinterpret_cast<HANDLE>(tid);
}

DWORD WINAPI_CALL THETAZ_Sleep(DWORD dwMilliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(dwMilliseconds));
    return 0;
}

// ---------------------------------------------------------------------------
// Critical sections
// ---------------------------------------------------------------------------
void WINAPI_CALL THETAZ_InitializeCriticalSection(PCRITICAL_SECTION lpCriticalSection) {
    if (!lpCriticalSection) return;
    auto* p = reinterpret_cast<uint8_t*>(lpCriticalSection);
    *reinterpret_cast<int32_t*>(p+4) = -1;
    *reinterpret_cast<int32_t*>(p+8) = 0;
    *reinterpret_cast<int32_t*>(p+12) = 0;
    *reinterpret_cast<int32_t*>(p+16) = 0;
    *reinterpret_cast<int32_t*>(p+20) = 0;
    std::lock_guard<std::mutex> lock(cs_map_mutex());
    critical_section_locks()[lpCriticalSection];
}

void WINAPI_CALL THETAZ_DeleteCriticalSection(PCRITICAL_SECTION lpCriticalSection) {
    if (!lpCriticalSection) return;
    std::lock_guard<std::mutex> lock(cs_map_mutex());
    critical_section_locks().erase(lpCriticalSection);
}

void WINAPI_CALL THETAZ_EnterCriticalSection(PCRITICAL_SECTION lpCriticalSection) {
    if (!lpCriticalSection) return;
    auto* p = reinterpret_cast<uint8_t*>(lpCriticalSection);
    *reinterpret_cast<int32_t*>(p+8) = 1;
    *reinterpret_cast<int32_t*>(p+12) = 1;
    std::lock_guard<std::mutex> lock(cs_map_mutex());
    critical_section_locks()[lpCriticalSection].lock();
}

void WINAPI_CALL THETAZ_LeaveCriticalSection(PCRITICAL_SECTION lpCriticalSection) {
    if (!lpCriticalSection) return;
    auto* p = reinterpret_cast<uint8_t*>(lpCriticalSection);
    *reinterpret_cast<int32_t*>(p+8) = 0;
    *reinterpret_cast<int32_t*>(p+12) = 0;
    std::lock_guard<std::mutex> lock(cs_map_mutex());
    critical_section_locks()[lpCriticalSection].unlock();
}

// ---------------------------------------------------------------------------
// API registration
// ---------------------------------------------------------------------------
void initialize_kernel32_api() {
    register_api("kernel32.dll", "CreateThread", (void*)THETAZ_CreateThread);
    register_api("kernel32.dll", "GetCurrentThreadId", (void*)THETAZ_GetCurrentThreadId);
    register_api("kernel32.dll", "GetCurrentThread", (void*)THETAZ_GetCurrentThread);
    register_api("kernel32.dll", "GetCurrentProcess", (void*)THETAZ_GetCurrentProcess);
    register_api("kernel32.dll", "CloseHandle", (void*)THETAZ_CloseHandle);
    register_api("kernel32.dll", "GetLastError", (void*)THETAZ_GetLastError);
    register_api("kernel32.dll", "SetLastError", (void*)THETAZ_SetLastError);
    register_api("kernel32.dll", "Sleep", (void*)THETAZ_Sleep);
    register_api("kernel32.dll", "InitializeCriticalSection", (void*)THETAZ_InitializeCriticalSection);
    register_api("kernel32.dll", "DeleteCriticalSection", (void*)THETAZ_DeleteCriticalSection);
    register_api("kernel32.dll", "EnterCriticalSection", (void*)THETAZ_EnterCriticalSection);
    register_api("kernel32.dll", "LeaveCriticalSection", (void*)THETAZ_LeaveCriticalSection);
}

} // namespace ThetaZ
