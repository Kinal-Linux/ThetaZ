#include "core/api/kernel32.h"

namespace ThetaZ {

void* WINAPI_CALL WINAPI_CreateThread(void* security, size_t stack_size, void* start_address, void* param, uint32_t flags, uint32_t* thread_id) {
    fprintf(stderr, "CreateThread: Creating thread (start_address=%p)\n", start_address);
    return NULL;
}

uint32_t WINAPI_CALL WINAPI_GetCurrentThreadId() {
    return (uint32_t)pthread_self();
}

void WINAPI_CALL WINAPI_Sleep(uint32_t milliseconds) {
    usleep(milliseconds * 1000);
}

void WINAPI_CALL WINAPI_EnterCriticalSection(void* critical_section) {
    fprintf(stderr, "EnterCriticalSection: %p\n", critical_section);
}

void WINAPI_CALL WINAPI_LeaveCriticalSection(void* critical_section) {
    fprintf(stderr, "LeaveCriticalSection: %p\n", critical_section);
}

void WINAPI_CALL WINAPI_CloseHandle(void* handle) {
    fprintf(stderr, "CloseHandle: %p\n", handle);
}

uint32_t WINAPI_CALL WINAPI_GetLastError() {
    return GetLastError_value;
}

} // namespace ThetaZ
