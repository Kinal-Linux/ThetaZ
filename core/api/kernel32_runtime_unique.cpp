// ThetaZ — runtime kernel32 APIs (CRT / loader / file / console / sync);
// Copyright (c) 2026 ThetaZ Project. MIT License.

#include "core/api/api.h"
#include "core/api/kernel32.h"
#include "core/exception/exception.h"
#include "core/loader/loader.h"
#include "core/ipc/thetaserver.h"
#include "core/ntdll/thread.h"
#include "core/api/path_resolver.h"
#include "core/x86/api_dispatch.h"
#include "core/x86/x86.h"

#include <cstdio>
#include <map>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cstdarg>
#include <cctype>
#include <ctime>
#include <climits>
#include <cwchar>
#include <string>
#include <thread>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>

namespace ThetaZ {



void initialize_kernel32_runtime() {
    // Memory API (implemented in memory_api.cpp);
    register_api("kernel32.dll", "VirtualAlloc", (void*)THETAZ_VirtualAlloc);
    register_api("kernel32.dll", "VirtualFree", (void*)THETAZ_VirtualFree);
    register_api("kernel32.dll", "VirtualQuery", (void*)THETAZ_VirtualQuery);
    register_api("kernel32.dll", "VirtualProtect", (void*)THETAZ_VirtualProtect);
    register_api("kernel32.dll", "VirtualQueryEx", (void*)THETAZ_VirtualQuery);
    register_api("kernel32.dll", "VirtualProtectEx", (void*)THETAZ_VirtualProtect);

    // Thread API (implemented in thread_api.cpp);
    register_api("kernel32.dll", "GetThreadId", (void*)THETAZ_GetThreadId);
    register_api("kernel32.dll", "SuspendThread", (void*)THETAZ_SuspendThread);
    register_api("kernel32.dll", "ResumeThread", (void*)THETAZ_ResumeThread);
    register_api("kernel32.dll", "InitializeCriticalSectionAndSpinCount", (void*)THETAZ_InitializeCriticalSectionAndSpinCount);
    register_api("kernel32.dll", "SetUnhandledExceptionFilter", (void*)THETAZ_SetUnhandledExceptionFilter);
    register_api("kernel32.dll", "SetProcessDPIAware", (void*)THETAZ_SetProcessDPIAware);
    register_api("kernel32.dll", "SetDefaultDllDirectories", (void*)THETAZ_SetDefaultDllDirectories);
    register_api("shell32.dll", "CommandLineToArgvW", (void*)THETAZ_CommandLineToArgvW);
    register_api("kernel32.dll", "CommandLineToArgvW", (void*)THETAZ_CommandLineToArgvW);
    register_api("user32.dll", "SetProcessDPIAware", (void*)THETAZ_SetProcessDPIAware);
    register_api("user32.dll", "GetDC", (void*)THETAZ_GetDC);
    register_api("user32.dll", "GetWindowDC", (void*)THETAZ_GetWindowDC);
    register_api("user32.dll", "ReleaseDC", (void*)THETAZ_ReleaseDC);
    register_api("user32.dll", "BeginPaint", (void*)THETAZ_BeginPaint);
    register_api("user32.dll", "EndPaint", (void*)THETAZ_EndPaint);
    register_api("user32.dll", "GetClientRect", (void*)THETAZ_GetClientRect);
    register_api("user32.dll", "GetSystemMetrics", (void*)THETAZ_GetSystemMetrics);
    register_api("user32.dll", "SystemParametersInfoW", (void*)THETAZ_SystemParametersInfoW);
    register_api("gdi32.dll", "CreateCompatibleDC", (void*)THETAZ_CreateCompatibleDC);
    register_api("gdi32.dll", "DeleteDC", (void*)THETAZ_DeleteDC);
    register_api("gdi32.dll", "GetStockObject", (void*)THETAZ_GetStockObject);
    register_api("gdi32.dll", "SelectObject", (void*)THETAZ_SelectObject);
    register_api("gdi32.dll", "GetDeviceCaps", (void*)THETAZ_GetDeviceCaps);
    register_api("gdi32.dll", "TextOutA", (void*)THETAZ_TextOutA);
    register_api("gdi32.dll", "TextOutW", (void*)THETAZ_TextOutW);
    register_api("gdi32.dll", "BitBlt", (void*)THETAZ_BitBlt);
    register_api("gdi32.dll", "SetBkMode", (void*)THETAZ_SetBkMode);
    register_api("gdi32.dll", "SetTextColor", (void*)THETAZ_SetTextColor);
    register_api("gdi32.dll", "SetBkColor", (void*)THETAZ_SetBkColor);
    register_api("gdi32.dll", "CreateFontA", (void*)THETAZ_CreateFontA);
    register_api("gdi32.dll", "DeleteObject", (void*)THETAZ_DeleteObject);
    register_api("gdi32.dll", "CreateSolidBrush", (void*)THETAZ_CreateSolidBrush);
    register_api("gdi32.dll", "CreatePen", (void*)THETAZ_CreatePen);
    register_api("kernel32.dll", "UnhandledExceptionFilter", (void*)THETAZ_UnhandledExceptionFilter);
    register_api("kernel32.dll", "CopyFileW", (void*)THETAZ_CopyFileW);
    register_api("kernel32.dll", "CreateEventA", (void*)THETAZ_CreateEventA);
    register_api("kernel32.dll", "OpenEventA", (void*)THETAZ_OpenEventA);
    register_api("kernel32.dll", "CreateMutexA", (void*)THETAZ_CreateMutexA);
    register_api("kernel32.dll", "OpenMutexA", (void*)THETAZ_OpenMutexA);
    register_api("kernel32.dll", "CreateSemaphoreA", (void*)THETAZ_CreateSemaphoreA);
    register_api("kernel32.dll", "OpenSemaphoreA", (void*)THETAZ_OpenSemaphoreA);
    register_api("kernel32.dll", "ReleaseSemaphore", (void*)THETAZ_ReleaseSemaphore);
    register_api("kernel32.dll", "CreatePipe", (void*)THETAZ_CreatePipe);
    register_api("kernel32.dll", "PeekNamedPipe", (void*)THETAZ_PeekNamedPipe);
    register_api("kernel32.dll", "FindFirstFileW", (void*)THETAZ_FindFirstFileW);
    register_api("kernel32.dll", "CreateSymbolicLinkW", (void*)THETAZ_CreateSymbolicLinkW);
    register_api("kernel32.dll", "GetDiskFreeSpaceA", (void*)THETAZ_GetDiskFreeSpaceA);
        register_api("kernel32.dll", "GetStartupInfoW", (void*)THETAZ_GetStartupInfoW);
        register_api("kernel32.dll", "GetStartupInfoA", (void*)THETAZ_GetStartupInfoA);
        register_api("kernel32.dll", "IsDBCSLeadByte", (void*)THETAZ_IsDBCSLeadByte);
        register_api("kernel32.dll", "InitOnceBeginInitialize", (void*)THETAZ_InitOnceBeginInitialize);
        register_api("kernel32.dll", "InitOnceComplete", (void*)THETAZ_InitOnceComplete);
        register_api("kernel32.dll", "SwitchToThread", (void*)THETAZ_SwitchToThread);
        register_api("kernel32.dll", "TryAcquireSRWLockExclusive", (void*)THETAZ_TryAcquireSRWLockExclusive);
        register_api("kernel32.dll", "LCMapStringEx", (void*)THETAZ_LCMapStringEx);

        register_api("kernel32.dll", "GetDateFormatW", (void*)THETAZ_GetDateFormatW);
        register_api("kernel32.dll", "GetTimeFormatW", (void*)THETAZ_GetTimeFormatW);
        register_api("kernel32.dll", "GetDateFormatEx", (void*)THETAZ_GetDateFormatEx);
        register_api("kernel32.dll", "GetTimeFormatEx", (void*)THETAZ_GetTimeFormatEx);
        register_api("kernel32.dll", "CreateFileMappingW", (void*)THETAZ_CreateFileMappingW);
        register_api("kernel32.dll", "CreateFileMappingA", (void*)THETAZ_CreateFileMappingA);
        register_api("kernel32.dll", "OpenFileMappingA", (void*)THETAZ_OpenFileMappingA);
        register_api("kernel32.dll", "MapViewOfFile", (void*)THETAZ_MapViewOfFile);
        register_api("kernel32.dll", "UnmapViewOfFile", (void*)THETAZ_UnmapViewOfFile);
        register_api("kernel32.dll", "FlushViewOfFile", (void*)THETAZ_FlushViewOfFile);
        register_api("kernel32.dll", "FindResourceW", (void*)THETAZ_FindResourceW);
        register_api("kernel32.dll", "LoadResource", (void*)THETAZ_LoadResource);
        register_api("kernel32.dll", "LockResource", (void*)THETAZ_LockResource);
        register_api("kernel32.dll", "SizeofResource", (void*)THETAZ_SizeofResource);
        register_api("kernel32.dll", "K32EnumProcesses", (void*)THETAZ_K32EnumProcesses);
        register_api("kernel32.dll", "K32EnumProcessModulesEx", (void*)THETAZ_K32EnumProcessModulesEx);
        register_api("kernel32.dll", "K32GetModuleFileNameExW", (void*)THETAZ_K32GetModuleFileNameExW);
        register_api("kernel32.dll", "K32GetProcessImageFileNameW", (void*)THETAZ_K32GetProcessImageFileNameW);
        register_api("kernel32.dll", "K32GetProcessMemoryInfo", (void*)THETAZ_K32GetProcessMemoryInfo);
        register_api("kernel32.dll", "CreateIoCompletionPort", (void*)THETAZ_CreateIoCompletionPort);
        register_api("kernel32.dll", "PostQueuedCompletionStatus", (void*)THETAZ_PostQueuedCompletionStatus);
        register_api("kernel32.dll", "GetQueuedCompletionStatus", (void*)THETAZ_GetQueuedCompletionStatus);
        register_api("kernel32.dll", "CreateMemoryResourceNotification", (void*)THETAZ_CreateMemoryResourceNotification);
        register_api("kernel32.dll", "WerRegisterMemoryBlock", (void*)THETAZ_WerRegisterMemoryBlock);
        register_api("kernel32.dll", "WerUnregisterMemoryBlock", (void*)THETAZ_WerUnregisterMemoryBlock);

    // ADVAPI32.
    register_api("advapi32.dll", "RegGetValueW", (void*)THETAZ_RegGetValueW);
    register_api("advapi32.dll", "RegOpenKeyExA", (void*)THETAZ_RegOpenKeyExA);
    register_api("advapi32.dll", "RegOpenKeyExW", (void*)THETAZ_RegOpenKeyExW);
    register_api("advapi32.dll", "RegQueryValueExA", (void*)THETAZ_RegQueryValueExA);
    register_api("advapi32.dll", "RegQueryValueExW", (void*)THETAZ_RegQueryValueExW);
    register_api("advapi32.dll", "RegCloseKey", (void*)THETAZ_RegCloseKey);
    register_api("advapi32.dll", "RegCreateKeyExA", (void*)THETAZ_RegCreateKeyExA);
    register_api("advapi32.dll", "RegCreateKeyExW", (void*)THETAZ_RegCreateKeyExW);
    register_api("advapi32.dll", "RegCreateKeyW", (void*)THETAZ_RegCreateKeyW);
    register_api("advapi32.dll", "RegSetValueExA", (void*)THETAZ_RegSetValueExA);
    register_api("advapi32.dll", "RegSetValueExW", (void*)THETAZ_RegSetValueExW);
    register_api("advapi32.dll", "RegDeleteValueA", (void*)THETAZ_RegDeleteValueA);
    register_api("advapi32.dll", "RegDeleteValueW", (void*)THETAZ_RegDeleteValueW);
    register_api("advapi32.dll", "RegEnumKeyExA", (void*)THETAZ_RegEnumKeyExA);
    register_api("advapi32.dll", "RegEnumKeyExW", (void*)THETAZ_RegEnumKeyExW);
    register_api("advapi32.dll", "GetSecurityDescriptorDacl", (void*)THETAZ_GetSecurityDescriptorDacl);

    // OLE32.
    register_api("ole32.dll", "StringFromGUID2", (void*)THETAZ_StringFromGUID2);
    register_api("ole32.dll", "CoInitialize", (void*)THETAZ_CoInitialize);
    register_api("ole32.dll", "CoUninitialize", (void*)THETAZ_CoUninitialize);
    register_api("ole32.dll", "CoCreateInstance", (void*)THETAZ_CoCreateInstance);
    register_api("ole32.dll", "OleInitialize", (void*)THETAZ_OleInitialize);
    register_api("ole32.dll", "OleUninitialize", (void*)THETAZ_OleUninitialize);
    register_api("ole32.dll", "CLSIDFromProgID", (void*)&THETAZ_CLSIDFromProgID);
    register_api("ole32.dll", "StringFromCLSID", (void*)&THETAZ_StringFromCLSID);
    register_api("ole32.dll", "CLSIDFromString", (void*)&THETAZ_CLSIDFromString);
    register_api("ole32.dll", "CoGetClassObject", (void*)&THETAZ_CoGetClassObject);

    // IMM32.
    register_api("imm32.dll", "ImmDisableIME", (void*)THETAZ_ImmDisableIME);

    // Module / loader.
    register_api("kernel32.dll", "GetModuleHandleW", (void*)THETAZ_GetModuleHandleW);
    register_api("kernel32.dll", "GetModuleHandleA", (void*)THETAZ_GetModuleHandleA);
    register_api("kernel32.dll", "GetModuleHandleExW", (void*)THETAZ_GetModuleHandleExW);
    register_api("kernel32.dll", "DisableThreadLibraryCalls", (void*)THETAZ_DisableThreadLibraryCalls);
    register_api("kernel32.dll", "IsBadStringPtrW", (void*)THETAZ_IsBadStringPtrW);
    register_api("kernel32.dll", "GetProcAddress", (void*)THETAZ_GetProcAddress);
    register_api("kernel32.dll", "LoadLibraryW", (void*)THETAZ_LoadLibraryW);
    register_api("kernel32.dll", "LoadLibraryA", (void*)THETAZ_LoadLibraryA);
    register_api("kernel32.dll", "LoadLibraryExW", (void*)THETAZ_LoadLibraryExW);
    register_api("kernel32.dll", "FreeLibrary", (void*)THETAZ_FreeLibrary);
    register_api("kernel32.dll", "GetModuleFileNameW", (void*)THETAZ_GetModuleFileNameW);
    register_api("kernel32.dll", "GetModuleFileNameA", (void*)THETAZ_GetModuleFileNameA);

    // Command line / environment.
    register_api("kernel32.dll", "GetCommandLineA", (void*)THETAZ_GetCommandLineA);
    register_api("kernel32.dll", "GetCommandLineW", (void*)THETAZ_GetCommandLineW);
    register_api("kernel32.dll", "GetEnvironmentStringsW", (void*)THETAZ_GetEnvironmentStringsW);
    register_api("kernel32.dll", "GetEnvironmentStrings", (void*)THETAZ_GetEnvironmentStrings);
    register_api("kernel32.dll", "FreeEnvironmentStringsW", (void*)THETAZ_FreeEnvironmentStringsW);
    register_api("kernel32.dll", "GetEnvironmentVariableA", (void*)THETAZ_GetEnvironmentVariableA);
    register_api("kernel32.dll", "GetEnvironmentVariableW", (void*)THETAZ_GetEnvironmentVariableW);
    register_api("kernel32.dll", "SetEnvironmentVariableA", (void*)THETAZ_SetEnvironmentVariableA);
    register_api("kernel32.dll", "SetEnvironmentVariableW", (void*)THETAZ_SetEnvironmentVariableW);

    // Std handles / console / debug.
    register_api("kernel32.dll", "GetStdHandle", (void*)THETAZ_GetStdHandle);
    register_api("kernel32.dll", "SetStdHandle", (void*)THETAZ_SetStdHandle);
    register_api("kernel32.dll", "OutputDebugStringA", (void*)THETAZ_OutputDebugStringA);
    register_api("kernel32.dll", "OutputDebugStringW", (void*)THETAZ_OutputDebugStringW);
    register_api("kernel32.dll", "IsDebuggerPresent", (void*)THETAZ_IsDebuggerPresent);
    register_api("kernel32.dll", "AllocConsole", (void*)THETAZ_AllocConsole);
    register_api("kernel32.dll", "FreeConsole", (void*)THETAZ_FreeConsole);
    register_api("kernel32.dll", "AttachConsole", (void*)THETAZ_AttachConsole);
    register_api("kernel32.dll", "SetConsoleMode", (void*)THETAZ_SetConsoleMode);
    register_api("kernel32.dll", "GetConsoleMode", (void*)THETAZ_GetConsoleMode);
    register_api("kernel32.dll", "GetConsoleOutputCP", (void*)THETAZ_GetConsoleOutputCP);
    register_api("kernel32.dll", "GetConsoleCP", (void*)THETAZ_GetConsoleCP);

    // Time.
    register_api("kernel32.dll", "GetSystemTimeAsFileTime", (void*)THETAZ_GetSystemTimeAsFileTime);
    register_api("kernel32.dll", "GetSystemTimePreciseAsFileTime", (void*)THETAZ_GetSystemTimePreciseAsFileTime);
    register_api("kernel32.dll", "GetSystemTime", (void*)THETAZ_GetSystemTime);
    register_api("kernel32.dll", "GetLocalTime", (void*)THETAZ_GetLocalTime);
    register_api("kernel32.dll", "GetTickCount", (void*)THETAZ_GetTickCount);
    register_api("kernel32.dll", "GetTickCount64", (void*)THETAZ_GetTickCount64);
    register_api("kernel32.dll", "QueryPerformanceCounter", (void*)THETAZ_QueryPerformanceCounter);
    register_api("kernel32.dll", "QueryPerformanceFrequency", (void*)THETAZ_QueryPerformanceFrequency);
    register_api("kernel32.dll", "QueryUnbiasedInterruptTime", (void*)THETAZ_QueryUnbiasedInterruptTime);
    register_api("kernel32.dll", "FileTimeToSystemTime", (void*)THETAZ_FileTimeToSystemTime);
    register_api("kernel32.dll", "SystemTimeToFileTime", (void*)THETAZ_SystemTimeToFileTime);
    register_api("kernel32.dll", "GetTimeZoneInformation", (void*)THETAZ_GetTimeZoneInformation);
    register_api("kernel32.dll", "TzSpecificLocalTimeToSystemTime", (void*)THETAZ_TzSpecificLocalTimeToSystemTime);

    // System info.
    register_api("kernel32.dll", "GetSystemInfo", (void*)THETAZ_GetSystemInfo);
    register_api("kernel32.dll", "GetNativeSystemInfo", (void*)THETAZ_GetNativeSystemInfo);
    register_api("kernel32.dll", "GetVersionExW", (void*)THETAZ_GetVersionExW);
    register_api("kernel32.dll", "GetACP", (void*)THETAZ_GetACP);
    register_api("kernel32.dll", "GetOEMCP", (void*)THETAZ_GetOEMCP);
    register_api("kernel32.dll", "IsValidCodePage", (void*)THETAZ_IsValidCodePage);
    register_api("kernel32.dll", "AreFileApisANSI", (void*)THETAZ_AreFileApisANSI);
    register_api("kernel32.dll", "GetCPInfo", (void*)THETAZ_GetCPInfo);
    register_api("kernel32.dll", "GetCPInfoExW", (void*)THETAZ_GetCPInfoExW);
    register_api("kernel32.dll", "GlobalMemoryStatusEx", (void*)THETAZ_GlobalMemoryStatusEx);
    register_api("kernel32.dll", "GetLogicalProcessorInformation", (void*)THETAZ_GetLogicalProcessorInformation);
    register_api("kernel32.dll", "GetSystemPowerStatus", (void*)THETAZ_GetSystemPowerStatus);
    register_api("kernel32.dll", "GetSystemDirectoryW", (void*)THETAZ_GetSystemDirectoryW);
    register_api("kernel32.dll", "GetComputerNameW", (void*)THETAZ_GetComputerNameW);
    register_api("kernel32.dll", "IsProcessorFeaturePresent", (void*)THETAZ_IsProcessorFeaturePresent);
    register_api("kernel32.dll", "IsSystemResumeAutomatic", (void*)THETAZ_IsSystemResumeAutomatic);

    // Heap / Local / Global.
    register_api("kernel32.dll", "GetProcessHeap", (void*)THETAZ_GetProcessHeap);
    register_api("kernel32.dll", "HeapAlloc", (void*)THETAZ_HeapAlloc);
    register_api("kernel32.dll", "HeapFree", (void*)THETAZ_HeapFree);
    register_api("kernel32.dll", "HeapReAlloc", (void*)THETAZ_HeapReAlloc);
    register_api("kernel32.dll", "HeapCreate", (void*)THETAZ_HeapCreate);
    register_api("kernel32.dll", "HeapDestroy", (void*)THETAZ_HeapDestroy);
    register_api("kernel32.dll", "HeapSize", (void*)THETAZ_HeapSize);
    register_api("kernel32.dll", "HeapCompact", (void*)THETAZ_HeapCompact);
    register_api("kernel32.dll", "HeapValidate", (void*)THETAZ_HeapValidate);
    register_api("kernel32.dll", "HeapSetInformation", (void*)THETAZ_HeapSetInformation);
    register_api("kernel32.dll", "LocalAlloc", (void*)THETAZ_LocalAlloc);
    register_api("kernel32.dll", "LocalFree", (void*)THETAZ_LocalFree);
    register_api("kernel32.dll", "LocalReAlloc", (void*)THETAZ_LocalReAlloc);
    register_api("kernel32.dll", "LocalSize", (void*)THETAZ_LocalSize);
    register_api("kernel32.dll", "GlobalAlloc", (void*)THETAZ_GlobalAlloc);
    register_api("kernel32.dll", "GlobalFree", (void*)THETAZ_GlobalFree);
    register_api("kernel32.dll", "GlobalReAlloc", (void*)THETAZ_GlobalReAlloc);
    register_api("kernel32.dll", "GlobalSize", (void*)THETAZ_GlobalSize);
    register_api("kernel32.dll", "DecodePointer", (void*)THETAZ_DecodePointer);
    register_api("kernel32.dll", "EncodePointer", (void*)THETAZ_EncodePointer);
    register_api("kernel32.dll", "MulDiv", (void*)THETAZ_MulDiv);

    // String conversion.
    register_api("kernel32.dll", "MultiByteToWideChar", (void*)THETAZ_MultiByteToWideChar);
    register_api("kernel32.dll", "WideCharToMultiByte", (void*)THETAZ_WideCharToMultiByte);
    register_api("kernel32.dll", "lstrcmpW", (void*)THETAZ_lstrcmpW);
    register_api("kernel32.dll", "lstrcmpiW", (void*)THETAZ_lstrcmpiW);
    register_api("kernel32.dll", "lstrlenW", (void*)THETAZ_lstrlenW);
    register_api("kernel32.dll", "lstrcpyW", (void*)THETAZ_lstrcpyW);

    // Locale.
    register_api("kernel32.dll", "CompareStringW", (void*)THETAZ_CompareStringW);
    register_api("kernel32.dll", "CompareStringEx", (void*)THETAZ_CompareStringEx);
    register_api("kernel32.dll", "LCMapStringW", (void*)THETAZ_LCMapStringW);
    register_api("kernel32.dll", "GetUserDefaultLCID", (void*)THETAZ_GetUserDefaultLCID);
    register_api("kernel32.dll", "GetSystemDefaultLCID", (void*)THETAZ_GetSystemDefaultLCID);
    register_api("kernel32.dll", "GetThreadLocale", (void*)THETAZ_GetThreadLocale);
    register_api("kernel32.dll", "GetUserGeoID", (void*)THETAZ_GetUserGeoID);
    register_api("kernel32.dll", "GetUserDefaultLocaleName", (void*)THETAZ_GetUserDefaultLocaleName);
    register_api("kernel32.dll", "GetSystemDefaultLocaleName", (void*)THETAZ_GetSystemDefaultLocaleName);
    register_api("kernel32.dll", "LocaleNameToLCID", (void*)THETAZ_LocaleNameToLCID);
    register_api("kernel32.dll", "LCIDToLocaleName", (void*)THETAZ_LCIDToLocaleName);
    register_api("kernel32.dll", "EnumSystemLocalesW", (void*)THETAZ_EnumSystemLocalesW);
    register_api("kernel32.dll", "EnumSystemLocalesEx", (void*)THETAZ_EnumSystemLocalesEx);
    register_api("kernel32.dll", "GetStringTypeW", (void*)THETAZ_GetStringTypeW);
    register_api("kernel32.dll", "GetStringTypeExW", (void*)THETAZ_GetStringTypeExW);
    register_api("kernel32.dll", "IsValidLocale", (void*)THETAZ_IsValidLocale);
    register_api("kernel32.dll", "IsValidLocaleName", (void*)THETAZ_IsValidLocaleName);
    register_api("kernel32.dll", "GetSystemPreferredUILanguages", (void*)THETAZ_GetSystemPreferredUILanguages);
    register_api("kernel32.dll", "GetUserPreferredUILanguages", (void*)THETAZ_GetUserPreferredUILanguages);
    register_api("kernel32.dll", "GetLocaleInfoW", (void*)THETAZ_GetLocaleInfoW);
    register_api("kernel32.dll", "GetLocaleInfoEx", (void*)THETAZ_GetLocaleInfoEx);

    // TLS / FLS.
    register_api("kernel32.dll", "TlsAlloc", (void*)THETAZ_TlsAlloc);
    register_api("kernel32.dll", "TlsFree", (void*)THETAZ_TlsFree);
    register_api("kernel32.dll", "TlsGetValue", (void*)THETAZ_TlsGetValue);
    register_api("kernel32.dll", "TlsSetValue", (void*)THETAZ_TlsSetValue);
    register_api("kernel32.dll", "FlsAlloc", (void*)THETAZ_FlsAlloc);
    register_api("kernel32.dll", "FlsFree", (void*)THETAZ_FlsFree);
    register_api("kernel32.dll", "FlsGetValue", (void*)THETAZ_FlsGetValue);
    register_api("kernel32.dll", "FlsSetValue", (void*)THETAZ_FlsSetValue);


    // File I/O.
    register_api("kernel32.dll", "CreateFileW", (void*)THETAZ_CreateFileW);
    register_api("kernel32.dll", "CreateFileA", (void*)THETAZ_CreateFileA);
    register_api("kernel32.dll", "ReadFile", (void*)THETAZ_ReadFile);
    register_api("kernel32.dll", "WriteFile", (void*)THETAZ_WriteFile);
    register_api("kernel32.dll", "WriteConsoleW", (void*)THETAZ_WriteConsoleW);
    register_api("kernel32.dll", "ReadConsoleW", (void*)THETAZ_ReadConsoleW);
    register_api("kernel32.dll", "GetFileSize", (void*)THETAZ_GetFileSize);
    register_api("kernel32.dll", "GetFileSizeEx", (void*)THETAZ_GetFileSizeEx);
    register_api("kernel32.dll", "GetFileType", (void*)THETAZ_GetFileType);
    register_api("kernel32.dll", "SetFilePointer", (void*)THETAZ_SetFilePointer);
    register_api("kernel32.dll", "SetFilePointerEx", (void*)THETAZ_SetFilePointerEx);
    register_api("kernel32.dll", "SetEndOfFile", (void*)THETAZ_SetEndOfFile);
    register_api("kernel32.dll", "FlushFileBuffers", (void*)THETAZ_FlushFileBuffers);
    register_api("kernel32.dll", "GetFileTime", (void*)THETAZ_GetFileTime);
    register_api("kernel32.dll", "SetFileTime", (void*)THETAZ_SetFileTime);
    register_api("kernel32.dll", "GetFileInformationByHandle", (void*)THETAZ_GetFileInformationByHandle);
    register_api("kernel32.dll", "GetFileInformationByHandleEx", (void*)THETAZ_GetFileInformationByHandleEx);
    register_api("kernel32.dll", "GetFileAttributesW", (void*)THETAZ_GetFileAttributesW);
    register_api("kernel32.dll", "GetFileAttributesA", (void*)THETAZ_GetFileAttributesA);
    register_api("kernel32.dll", "GetFileAttributesExW", (void*)THETAZ_GetFileAttributesExW);
    register_api("kernel32.dll", "SetFileAttributesW", (void*)THETAZ_SetFileAttributesW);
    register_api("kernel32.dll", "SetFileInformationByHandle", (void*)THETAZ_SetFileInformationByHandle);
    register_api("kernel32.dll", "SetHandleInformation", (void*)THETAZ_SetHandleInformation);
    register_api("kernel32.dll", "GetCurrentDirectoryW", (void*)THETAZ_GetCurrentDirectoryW);
    register_api("kernel32.dll", "GetCurrentDirectoryA", (void*)THETAZ_GetCurrentDirectoryA);
    register_api("kernel32.dll", "SetCurrentDirectoryW", (void*)THETAZ_SetCurrentDirectoryW);
    register_api("kernel32.dll", "SetCurrentDirectoryA", (void*)THETAZ_SetCurrentDirectoryA);
    register_api("kernel32.dll", "GetFullPathNameA", (void*)THETAZ_GetFullPathNameA);
    register_api("kernel32.dll", "GetFullPathNameW", (void*)THETAZ_GetFullPathNameW);
    register_api("kernel32.dll", "GetTempPathW", (void*)THETAZ_GetTempPathW);
    register_api("kernel32.dll", "GetTempPathA", (void*)THETAZ_GetTempPathA);

    register_api("kernel32.dll", "GetTempFileNameW", (void*)THETAZ_GetTempFileNameW);
    register_api("kernel32.dll", "DeleteFileW", (void*)THETAZ_DeleteFileW);
    register_api("kernel32.dll", "DeleteFileA", (void*)THETAZ_DeleteFileA);
    register_api("kernel32.dll", "MoveFileExW", (void*)THETAZ_MoveFileExW);
    register_api("kernel32.dll", "CreateDirectoryW", (void*)THETAZ_CreateDirectoryW);
    register_api("kernel32.dll", "RemoveDirectoryW", (void*)THETAZ_RemoveDirectoryW);
    register_api("kernel32.dll", "CopyFileExW", (void*)THETAZ_CopyFileExW);
    register_api("kernel32.dll", "FindFirstFileExW", (void*)THETAZ_FindFirstFileExW);
    register_api("kernel32.dll", "FindNextFileW", (void*)THETAZ_FindNextFileW);
    register_api("kernel32.dll", "FindClose", (void*)THETAZ_FindClose);
    register_api("kernel32.dll", "GetDiskFreeSpaceW", (void*)THETAZ_GetDiskFreeSpaceW);
    register_api("kernel32.dll", "GetDiskFreeSpaceExW", (void*)THETAZ_GetDiskFreeSpaceExW);
    register_api("kernel32.dll", "GetVolumeInformationW", (void*)THETAZ_GetVolumeInformationW);
    register_api("kernel32.dll", "GetFinalPathNameByHandleW", (void*)THETAZ_GetFinalPathNameByHandleW);
    register_api("kernel32.dll", "GetLongPathNameW", (void*)THETAZ_GetLongPathNameW);
    register_api("kernel32.dll", "GetShortPathNameA", (void*)THETAZ_GetShortPathNameA);
    register_api("kernel32.dll", "GetVolumeNameForVolumeMountPointW", (void*)THETAZ_GetVolumeNameForVolumeMountPointW);
    register_api("kernel32.dll", "GetVolumePathNamesForVolumeNameW", (void*)THETAZ_GetVolumePathNamesForVolumeNameW);
    register_api("kernel32.dll", "GetVolumePathNameW", (void*)THETAZ_GetVolumePathNameW);
    register_api("kernel32.dll", "GetDriveTypeW", (void*)THETAZ_GetDriveTypeW);
    register_api("kernel32.dll", "GetOverlappedResult", (void*)THETAZ_GetOverlappedResult);
    register_api("kernel32.dll", "LockFile", (void*)THETAZ_LockFile);
    register_api("kernel32.dll", "UnlockFile", (void*)THETAZ_UnlockFile);
    register_api("kernel32.dll", "LockFileEx", (void*)THETAZ_LockFileEx);
    register_api("kernel32.dll", "UnlockFileEx", (void*)THETAZ_UnlockFileEx);
    register_api("kernel32.dll", "DeviceIoControl", (void*)THETAZ_DeviceIoControl);
    register_api("kernel32.dll", "CancelIoEx", (void*)THETAZ_CancelIoEx);
    register_api("kernel32.dll", "ExpandEnvironmentStringsW", (void*)THETAZ_ExpandEnvironmentStringsW);

    // FormatMessage / RaiseException.
    register_api("kernel32.dll", "FormatMessageA", (void*)THETAZ_FormatMessageA);
    register_api("kernel32.dll", "FormatMessageW", (void*)THETAZ_FormatMessageW);
    register_api("kernel32.dll", "RaiseException", (void*)THETAZ_RaiseException);
        register_api("kernel32.dll", "RtlUnwind", (void*)THETAZ_RtlUnwind);
        register_api("kernel32.dll", "RtlCaptureStackBackTrace", (void*)THETAZ_RtlCaptureStackBackTrace);
    register_api("kernel32.dll", "CreateTimerQueueTimer", (void*)THETAZ_CreateTimerQueueTimer);
    register_api("kernel32.dll", "DeleteTimerQueueTimer", (void*)THETAZ_DeleteTimerQueueTimer);
    register_api("kernel32.dll", "CreateTimerQueue", (void*)THETAZ_CreateTimerQueue);
    register_api("kernel32.dll", "DeleteTimerQueue", (void*)THETAZ_DeleteTimerQueue);

    // Synchronization.
    register_api("kernel32.dll", "CreateEventW", (void*)THETAZ_CreateEventW);
    register_api("kernel32.dll", "CreateEventExW", (void*)THETAZ_CreateEventExW);
    register_api("kernel32.dll", "SetEvent", (void*)THETAZ_SetEvent);
    register_api("kernel32.dll", "ResetEvent", (void*)THETAZ_ResetEvent);
    register_api("kernel32.dll", "PulseEvent", (void*)THETAZ_PulseEvent);
    register_api("kernel32.dll", "OpenEventW", (void*)THETAZ_OpenEventW);
    register_api("kernel32.dll", "CreateMutexW", (void*)THETAZ_CreateMutexW);
    register_api("kernel32.dll", "CreateMutexExW", (void*)THETAZ_CreateMutexExW);
    register_api("kernel32.dll", "ReleaseMutex", (void*)THETAZ_ReleaseMutex);
    register_api("kernel32.dll", "OpenMutexW", (void*)THETAZ_OpenMutexW);
    register_api("kernel32.dll", "WaitForSingleObject", (void*)THETAZ_WaitForSingleObject);
    register_api("kernel32.dll", "WaitForSingleObjectEx", (void*)THETAZ_WaitForSingleObjectEx);
    register_api("kernel32.dll", "WaitForMultipleObjects", (void*)THETAZ_WaitForMultipleObjects);
    register_api("kernel32.dll", "WaitForMultipleObjectsEx", (void*)THETAZ_WaitForMultipleObjectsEx);

    register_api("kernel32.dll", "CreateWaitableTimerW", (void*)THETAZ_CreateWaitableTimerW);
    register_api("kernel32.dll", "SetWaitableTimerEx", (void*)THETAZ_SetWaitableTimerEx);
    register_api("kernel32.dll", "CancelWaitableTimer", (void*)THETAZ_CancelWaitableTimer);

    // Threadpool / slist / init-once.
    register_api("kernel32.dll", "InitializeSListHead", (void*)THETAZ_InitializeSListHead);
    register_api("kernel32.dll", "InterlockedPushEntrySList", (void*)THETAZ_InterlockedPushEntrySList);
    register_api("kernel32.dll", "InterlockedPopEntrySList", (void*)THETAZ_InterlockedPopEntrySList);
    register_api("kernel32.dll", "QueryDepthSList", (void*)THETAZ_QueryDepthSList);
    register_api("kernel32.dll", "InitOnceExecuteOnce", (void*)THETAZ_InitOnceExecuteOnce);
    register_api("kernel32.dll", "CreateThreadpoolTimer", (void*)THETAZ_CreateThreadpoolTimer);
    register_api("kernel32.dll", "CreateThreadpoolWait", (void*)THETAZ_CreateThreadpoolWait);
    register_api("kernel32.dll", "CreateThreadpoolWork", (void*)THETAZ_CreateThreadpoolWork);
    register_api("kernel32.dll", "CloseThreadpoolTimer", (void*)THETAZ_CloseThreadpoolTimer);
    register_api("kernel32.dll", "CloseThreadpoolWait", (void*)THETAZ_CloseThreadpoolWait);
    register_api("kernel32.dll", "CloseThreadpoolWork", (void*)THETAZ_CloseThreadpoolWork);
    register_api("kernel32.dll", "SetThreadpoolTimer", (void*)THETAZ_SetThreadpoolTimer);
    register_api("kernel32.dll", "SetThreadpoolWait", (void*)THETAZ_SetThreadpoolWait);
    register_api("kernel32.dll", "SubmitThreadpoolWork", (void*)THETAZ_SubmitThreadpoolWork);
    register_api("kernel32.dll", "WaitForThreadpoolTimerCallbacks", (void*)THETAZ_WaitForThreadpoolTimerCallbacks);
    register_api("kernel32.dll", "WaitForThreadpoolWaitCallbacks", (void*)THETAZ_WaitForThreadpoolWaitCallbacks);

    // Process / thread info.
    register_api("kernel32.dll", "TerminateProcess", (void*)THETAZ_TerminateProcess);
    register_api("kernel32.dll", "GetExitCodeThread", (void*)THETAZ_GetExitCodeThread);
    register_api("kernel32.dll", "OpenProcess", (void*)THETAZ_OpenProcess);
    register_api("kernel32.dll", "GetProcessTimes", (void*)THETAZ_GetProcessTimes);
    register_api("kernel32.dll", "GetThreadTimes", (void*)THETAZ_GetThreadTimes);
    register_api("kernel32.dll", "GetProcessAffinityMask", (void*)THETAZ_GetProcessAffinityMask);
    register_api("kernel32.dll", "GetPriorityClass", (void*)THETAZ_GetPriorityClass);
    register_api("kernel32.dll", "SetThreadPriority", (void*)THETAZ_SetThreadPriority);
    register_api("kernel32.dll", "IsWow64Process", (void*)THETAZ_IsWow64Process);
    register_api("kernel32.dll", "SetProcessMitigationPolicy", (void*)THETAZ_SetProcessMitigationPolicy);
    register_api("kernel32.dll", "GetThreadIOPendingFlag", (void*)THETAZ_GetThreadIOPendingFlag);
    register_api("kernel32.dll", "ProcessIdToSessionId", (void*)THETAZ_ProcessIdToSessionId);
    register_api("kernel32.dll", "QueryFullProcessImageNameW", (void*)THETAZ_QueryFullProcessImageNameW);
    register_api("kernel32.dll", "GetProcessHandleCount", (void*)THETAZ_GetProcessHandleCount);
    register_api("kernel32.dll", "VerSetConditionMask", (void*)THETAZ_VerSetConditionMask);
    register_api("kernel32.dll", "VerifyVersionInfoW", (void*)THETAZ_VerifyVersionInfoW);
    register_api("kernel32.dll", "GetProductInfo", (void*)THETAZ_GetProductInfo);
    register_api("kernel32.dll", "GetPhysicallyInstalledSystemMemory", (void*)THETAZ_GetPhysicallyInstalledSystemMemory);
    register_api("kernel32.dll", "QueryProcessCycleTime", (void*)THETAZ_QueryProcessCycleTime);
    register_api("kernel32.dll", "QueryThreadCycleTime", (void*)THETAZ_QueryThreadCycleTime);
    register_api("kernel32.dll", "SetErrorMode", (void*)THETAZ_SetErrorMode);

    // 64-bit guest runtime (Rtl* are ntdll exports forwarded by kernel32);
    register_api("kernel32.dll", "RtlCaptureContext", (void*)THETAZ_RtlCaptureContext);
    register_api("ntdll.dll", "RtlCaptureContext", (void*)THETAZ_RtlCaptureContext);
    register_api("kernel32.dll", "RtlLookupFunctionEntry", (void*)THETAZ_RtlLookupFunctionEntry);
    register_api("ntdll.dll", "RtlLookupFunctionEntry", (void*)THETAZ_RtlLookupFunctionEntry);
    register_api("kernel32.dll", "RtlVirtualUnwind", (void*)THETAZ_RtlVirtualUnwind);
    register_api("ntdll.dll", "RtlVirtualUnwind", (void*)THETAZ_RtlVirtualUnwind);

// App-V shim DLL (Office Click-to-Run): ordinal 1 = S_OK stub.


    // Additional WINWORD unresolved functions
    register_api("kernel32.dll", "ResolveDelayLoadedAPI", (void*)(HRESULT(*)(void*))THETAZ_ResolveDelayLoadedAPI);
    register_api("user32.dll", "RegisterClassExW", (void*)THETAZ_RegisterClassExW);
    register_api("user32.dll", "MsgWaitForMultipleObjectsEx", (void*)THETAZ_MsgWaitForMultipleObjectsEx);
    register_api("ws2_32.dll", "GetAddrInfoW", (void*)THETAZ_GetAddrInfoW);
    register_api("kernel32.dll", "DuplicateHandle", (void*)THETAZ_DuplicateHandle);
    register_api("msvcrt.dll", "_wgetenv", (void*)THETAZ__wgetenv);
    register_api("msvcrt.dll", "_wcslwr", (void*)THETAZ__wcslwr);
    register_api("shell32.dll", "SHGetSpecialFolderLocation", (void*)THETAZ_SHGetSpecialFolderLocation);
    register_api("shell32.dll", "SHGetFileInfoW", (void*)THETAZ_SHGetFileInfoW);
    register_api("shell32.dll", "SHFileOperationW", (void*)THETAZ_SHFileOperationW);
    register_api("kernel32.dll", "SetThreadDescription", (void*)THETAZ_SetThreadDescription);
    register_api("user32.dll", "SetDlgItemTextW", (void*)THETAZ_SetDlgItemTextW);
    register_api("user32.dll", "SendMessageTimeoutW", (void*)THETAZ_SendMessageTimeoutW);
    register_api("user32.dll", "SendMessageTimeoutA", (void*)THETAZ_SendMessageTimeoutA);
    register_api("user32.dll", "RemovePropA", (void*)THETAZ_RemovePropA);
    register_api("user32.dll", "RegisterWindowMessageA", (void*)THETAZ_RegisterWindowMessageA);
    register_api("user32.dll", "RegisterClipboardFormatW", (void*)THETAZ_RegisterClipboardFormatW);
    register_api("user32.dll", "RegisterClipboardFormatA", (void*)THETAZ_RegisterClipboardFormatA);
    register_api("user32.dll", "MonitorFromWindow", (void*)THETAZ_MonitorFromWindow);
    register_api("user32.dll", "MonitorFromPoint", (void*)THETAZ_MonitorFromPoint);
    register_api("kernel32.dll", "LoadStringA", (void*)THETAZ_LoadStringA);
        register_api("kernel32.dll", "InitializeSRWLock", (void*)THETAZ_InitializeSRWLock);
    register_api("user32.dll", "GetPropA", (void*)THETAZ_GetPropA);
    register_api("ws2_32.dll", "GetNameInfoW", (void*)THETAZ_GetNameInfoW);
    register_api("user32.dll", "GetMenuItemInfoW", (void*)THETAZ_GetMenuItemInfoW);
    register_api("kernel32.dll", "GetDriveTypeA", (void*)THETAZ_GetDriveTypeA);
    register_api("kernel32.dll", "GetComputerNameExW", (void*)THETAZ_GetComputerNameExW);
    register_api("user32.dll", "GetClassLongPtrW", (void*)THETAZ_GetClassLongPtrW);
    register_api("user32.dll", "GetClassInfoW", (void*)THETAZ_GetClassInfoW);
    register_api("user32.dll", "EnableMenuItem", (void*)THETAZ_EnableMenuItem);
    register_api("msvcrt.dll", "_dclass", (void*)THETAZ__dclass);
    register_api("user32.dll", "CharNextA", (void*)THETAZ_CharNextA);
    register_api("msvcrt.dll", "_wsopen_s", (void*)THETAZ__wsopen_s);
    register_api("ws2_32.dll", "WSASocketW", (void*)THETAZ_WSASocketW);
    register_api("ws2_32.dll", "WSASend", (void*)THETAZ_WSASend);
    register_api("ws2_32.dll", "__WSAFDIsSet", (void*)THETAZ___WSAFDIsSet);
    register_api("msvcrt.dll", "_wrmdir", (void*)THETAZ__wrmdir);
    register_api("ole32.dll", "WriteFmtUserTypeStg", (void*)THETAZ_WriteFmtUserTypeStg);
    register_api("ole32.dll", "WriteClassStm", (void*)THETAZ_WriteClassStm);
    register_api("ole32.dll", "WriteClassStg", (void*)THETAZ_WriteClassStg);
    register_api("ws2_32.dll", "WSASocketW", (void*)THETAZ_WSASocketW);
    register_api("ws2_32.dll", "WSASend", (void*)THETAZ_WSASend);
    register_api("ws2_32.dll", "__WSAFDIsSet", (void*)THETAZ___WSAFDIsSet);
    register_api("msvcrt.dll", "_wrmdir", (void*)THETAZ__wrmdir);
    register_api("ole32.dll", "WriteFmtUserTypeStg", (void*)THETAZ_WriteFmtUserTypeStg);
    register_api("ole32.dll", "WriteClassStm", (void*)THETAZ_WriteClassStm);
    register_api("ole32.dll", "WriteClassStg", (void*)THETAZ_WriteClassStg);

    // 64-bit guest runtime (Rtl* are ntdll exports forwarded by kernel32);
}
} // namespace ThetaZ
