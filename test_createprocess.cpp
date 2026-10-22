#include "core/api/kernel32.h"

using namespace ThetaZ;

#define INFINITE 0xFFFFFFFF

int main(int argc, char** argv) {
    printf("Testing CreateProcess API...\n");

    STARTUPINFOA si = {0};
    si.cb = sizeof(STARTUPINFOA);
    PROCESS_INFORMATION pi = {0};

    BOOL success = WINAPI_CreateProcessA(
        nullptr,
        (LPSTR)"/mnt/hdd/Downloads/OfficeSetup.exe",
        nullptr,
        nullptr,
        FALSE_VALUE,
        0,
        nullptr,
        nullptr,
        &si,
        &pi
    );

    if (success) {
        printf("CreateProcess succeeded!\n");
        printf("Process ID: %lu\n", (unsigned long)pi.dwProcessId);
        printf("Process Handle: %p\n", pi.hProcess);
        printf("Thread Handle: %p\n", pi.hThread);

        // Wait for the process to finish
        DWORD wait_result = WINAPI_WaitForSingleObject(pi.hProcess, INFINITE);

        // Close handles
        WINAPI_CloseHandle(pi.hProcess);
        WINAPI_CloseHandle(pi.hThread);

        printf("Process completed.\n");
    } else {
        DWORD error = WINAPI_GetLastError();
        printf("CreateProcess failed with error %lu\n", error);
    }

    return 0;
}