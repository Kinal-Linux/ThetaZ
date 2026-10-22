// ThetaZ — 32-bit API call dispatch
// Copyright (c) 2026 ThetaZ Project. MIT License.
//
// 32-bit guest code calls host Win32 functions through "thunk" addresses that
// live in a reserved region below 4GB. The 32-bit loader writes one of these
// addresses into each IAT slot instead of a host pointer; the interpreter
// detects a call/jump to a thunk address and routes it through
// x86_dispatch_api(), which marshals the guest's 32-bit stack arguments into a
// host call using libffi with the Microsoft x64 calling convention (FFI_WIN64,
// matching the ms_abi used for all registered host functions).
//
// Argument counts and calling conventions come from a hand-authored table
// (written from the Windows API documentation, NOT derived from Wine spec
// files).

#include "core/x86/api_dispatch.h"
#include "core/x86/x86.h"
#include "core/api/api.h"

#include <ffi.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>

namespace ThetaZ {

namespace {

// Thunk region: reserved below 4GB so guest pointers can reach it. Thunks are
// spaced kThunkStride bytes apart; the guest never dereferences them, they only
// exist as call targets.
constexpr uint32_t kThunkBase = 0x76000000u;
constexpr uint32_t kThunkSize = 0x00100000u; // 1MB
constexpr size_t kThunkStride = 32;
constexpr size_t kThunkCapacity = kThunkSize / kThunkStride;

struct Thunk {
    ApiThunk info{};
    ffi_cif cif{};
    ffi_type* arg_types[24] = {};
    bool in_use = false;
    // Owned string copies: info.dll/info.name may point into a guest image
    // (which is unmapped when that process exits) or a caller's stack buffer.
    // Thunks are shared across processes, so they must own their strings.
    char dll_buf[64];
    char name_buf[128];
};

Thunk g_thunks[kThunkCapacity];
bool g_region_mapped = false;

// ---------------------------------------------------------------------------
// Signature table (hand-authored).
// flags bit 0: cdecl (1) vs stdcall (0)
// wide: bit i set => 32-bit slot i actually carries the LOW half of an 8-byte
// value (the following slot holds the high half; slot count already accounts
// for both halves).
// ---------------------------------------------------------------------------

struct SigEntry {
    const char* dll;
    const char* name;
    uint8_t argc;   // number of 32-bit stack slots
    uint8_t flags;  // bit0 = cdecl
    uint16_t wide;  // bitmask of wide (8-byte) slots
};

enum { CDECL = 1 };

#define ST(dll, name, argc) { dll, name, argc, 0, 0 }
#define STW(dll, name, argc, wide) { dll, name, argc, 0, wide }
#define CT(dll, name, argc) { dll, name, argc, CDECL, 0 }
#define CTW(dll, name, argc, wide) { dll, name, argc, CDECL, wide }

const SigEntry kSignatures[] = {
    // ---- kernel32.dll ----------------------------------------------------
    ST("kernel32.dll", "AcquireSRWLockExclusive", 1),
    ST("kernel32.dll", "AcquireSRWLockShared", 1),
    ST("kernel32.dll", "AddVectoredContinueHandler", 2),
    ST("kernel32.dll", "AddVectoredExceptionHandler", 2),
    ST("kernel32.dll", "AllocConsole", 0),
    ST("kernel32.dll", "AreFileApisANSI", 0),
    ST("kernel32.dll", "AttachConsole", 1),
    ST("kernel32.dll", "CallNamedPipeA", 7),
    ST("kernel32.dll", "CallNamedPipeW", 7),
    ST("kernel32.dll", "CancelIoEx", 2),
    ST("kernel32.dll", "CancelWaitableTimer", 1),
    ST("kernel32.dll", "CheckRemoteDebuggerPresent", 2),
    ST("kernel32.dll", "CloseHandle", 1),
    ST("kernel32.dll", "CloseThreadpoolTimer", 1),
    ST("kernel32.dll", "CloseThreadpoolWait", 1),
    ST("kernel32.dll", "CloseThreadpoolWork", 1),
    ST("kernel32.dll", "CompareStringEx", 8),
    ST("kernel32.dll", "CompareStringW", 6),
    ST("kernel32.dll", "CopyFileA", 3),
    ST("kernel32.dll", "CopyFileExW", 6),
    ST("kernel32.dll", "CopyFileW", 3),
    ST("kernel32.dll", "CreateDirectoryW", 2),
    ST("kernel32.dll", "CreateEventA", 4),
    ST("kernel32.dll", "CreateEventExW", 4),
    ST("kernel32.dll", "CreateEventW", 4),
    ST("kernel32.dll", "CreateFileA", 7),
    ST("kernel32.dll", "CreateFileMappingA", 6),
    ST("kernel32.dll", "CreateFileMappingW", 6),
    ST("kernel32.dll", "CreateFileW", 7),
    ST("kernel32.dll", "CreateIoCompletionPort", 4),
    ST("kernel32.dll", "CreateMemoryResourceNotification", 1),
    ST("kernel32.dll", "CreateMutexA", 3),
    ST("kernel32.dll", "CreateMutexExW", 4),
    ST("kernel32.dll", "CreateMutexW", 3),
    ST("kernel32.dll", "CreateNamedPipeA", 8),
    ST("kernel32.dll", "CreateNamedPipeW", 8),
    ST("kernel32.dll", "CreatePipe", 4),
    ST("kernel32.dll", "CreateProcessA", 10),
    ST("kernel32.dll", "CreateProcessW", 10),
    ST("kernel32.dll", "CreateSemaphoreA", 4),
    ST("kernel32.dll", "CreateSymbolicLinkW", 3),
    ST("kernel32.dll", "CreateThread", 6),
    ST("kernel32.dll", "CreateThreadpoolTimer", 3),
    ST("kernel32.dll", "CreateThreadpoolWait", 3),
    ST("kernel32.dll", "CreateThreadpoolWork", 3),
    ST("kernel32.dll", "CreateTimerQueueTimer", 7),
    ST("kernel32.dll", "CreateWaitableTimerW", 3),
    ST("kernel32.dll", "DecodePointer", 1),
    ST("kernel32.dll", "DeleteConditionVariable", 1),
    ST("kernel32.dll", "DeleteCriticalSection", 1),
    ST("kernel32.dll", "DeleteFileA", 1),
    ST("kernel32.dll", "DeleteFileW", 1),
    ST("kernel32.dll", "DeleteTimerQueueTimer", 3),
    ST("kernel32.dll", "DeviceIoControl", 8),
    ST("kernel32.dll", "EncodePointer", 1),
    ST("kernel32.dll", "EnterCriticalSection", 1),
    ST("kernel32.dll", "EnumSystemLocalesEx", 4),
    ST("kernel32.dll", "EnumSystemLocalesW", 2),
    ST("kernel32.dll", "ExitProcess", 1),
    ST("kernel32.dll", "ExitThread", 1),
    ST("kernel32.dll", "ExpandEnvironmentStringsW", 3),
    ST("kernel32.dll", "Beep", 2),
    ST("kernel32.dll", "FileTimeToSystemTime", 2),
    ST("kernel32.dll", "FindClose", 1),
    ST("kernel32.dll", "FindFirstFileExW", 6),
    ST("kernel32.dll", "FindFirstFileW", 2),
    ST("kernel32.dll", "FindFirstFileA", 2),
    ST("kernel32.dll", "FindNextFileW", 2),
    ST("kernel32.dll", "FindNextFileA", 2),
    ST("kernel32.dll", "FindResourceW", 3),
    ST("kernel32.dll", "FlsAlloc", 1),
    ST("kernel32.dll", "FlsFree", 1),
    ST("kernel32.dll", "FlsGetValue", 1),
    ST("kernel32.dll", "FlsGetValue2", 1),
    ST("kernelbase.dll", "FlsGetValue2", 1),
    ST("ntdll.dll", "FlsGetValue2", 1),
    ST("kernel32.dll", "FlsSetValue", 2),
    ST("kernel32.dll", "FlushFileBuffers", 1),
    ST("kernel32.dll", "FlushViewOfFile", 2),
    ST("kernel32.dll", "FreeResource", 1),
    ST("kernel32.dll", "DuplicateHandle", 7),
    ST("kernel32.dll", "FormatMessageA", 7),
    ST("kernel32.dll", "FormatMessageW", 7),
    ST("kernel32.dll", "FreeConsole", 0),
    ST("kernel32.dll", "FreeEnvironmentStringsW", 1),
    ST("kernel32.dll", "FreeEnvironmentStringsA", 1),
    ST("kernel32.dll", "FreeLibrary", 1),
    ST("kernel32.dll", "FreeLibraryAndExitThread", 2),
    ST("kernel32.dll", "GetACP", 0),
    ST("kernel32.dll", "GetCommandLineA", 0),
    ST("kernel32.dll", "GetCommandLineW", 0),
    ST("kernel32.dll", "GetComputerNameW", 2),
    ST("kernel32.dll", "GetConsoleCP", 0),
    ST("kernel32.dll", "GetConsoleMode", 2),
    ST("kernel32.dll", "GetConsoleOutputCP", 0),
    ST("kernel32.dll", "GetNumberOfConsoleInputEvents", 2),
    ST("kernel32.dll", "GetCPInfo", 2),
    ST("kernel32.dll", "GetCPInfoExW", 3),
    ST("kernel32.dll", "GetCurrentDirectoryA", 2),
    ST("kernel32.dll", "GetCurrentDirectoryW", 2),
    ST("kernel32.dll", "GetCurrentProcess", 0),
    ST("kernel32.dll", "GetCurrentProcessId", 0),
    ST("kernel32.dll", "GetCurrentThread", 0),
    ST("kernel32.dll", "GetCurrentThreadId", 0),
    ST("kernel32.dll", "GetDateFormatW", 6),
    ST("kernel32.dll", "GetDiskFreeSpaceA", 5),
    ST("kernel32.dll", "GetDiskFreeSpaceExW", 4),
    ST("kernel32.dll", "GetDiskFreeSpaceW", 5),
    ST("kernel32.dll", "GetDriveTypeW", 1),
    ST("kernel32.dll", "GetEnvironmentStrings", 0),
    ST("kernel32.dll", "GetEnvironmentStringsA", 0),
    ST("kernel32.dll", "GetEnvironmentStringsW", 0),
    ST("kernel32.dll", "GetEnvironmentVariableA", 3),
    ST("kernel32.dll", "GetEnvironmentVariableW", 3),
    ST("kernel32.dll", "GetExitCodeProcess", 2),
    ST("kernel32.dll", "GetExitCodeThread", 2),
    ST("kernel32.dll", "GetFileAttributesA", 1),
    ST("kernel32.dll", "GetFileAttributesExW", 3),
    ST("kernel32.dll", "GetFileAttributesW", 1),
    ST("kernel32.dll", "GetFileInformationByHandle", 2),
    ST("kernel32.dll", "GetFileInformationByHandleEx", 4),
    ST("kernel32.dll", "GetFileSize", 2),
    ST("kernel32.dll", "GetFileSizeEx", 2),
    ST("kernel32.dll", "GetFileTime", 4),
    ST("kernel32.dll", "GetFileType", 1),
    ST("kernel32.dll", "GetFinalPathNameByHandleW", 4),
    ST("kernel32.dll", "GetFullPathNameA", 4),
    ST("kernel32.dll", "GetFullPathNameW", 4),
    ST("kernel32.dll", "GetLastError", 0),
    ST("kernel32.dll", "GetLocaleInfoEx", 4),
    ST("kernel32.dll", "GetLocaleInfoW", 4),
    ST("kernel32.dll", "GetLocaleInfoA", 4),
    ST("kernel32.dll", "GetLocalTime", 1),
    ST("kernel32.dll", "GetLogicalProcessorInformation", 2),
    ST("kernel32.dll", "GetLongPathNameW", 3),
    ST("kernel32.dll", "GetModuleFileNameA", 3),
    ST("kernel32.dll", "GetModuleFileNameW", 3),
    ST("kernel32.dll", "GetModuleHandleA", 1),
    ST("kernel32.dll", "GetModuleHandleExW", 3),
    ST("kernel32.dll", "GetModuleHandleW", 1),
    ST("kernel32.dll", "GetNamedPipeInfo", 5),
    ST("kernel32.dll", "GetNativeSystemInfo", 1),
    ST("kernel32.dll", "GetOEMCP", 0),
    ST("kernel32.dll", "GetOverlappedResult", 4),
    ST("kernel32.dll", "GetPhysicallyInstalledSystemMemory", 1),
    ST("kernel32.dll", "GetPriorityClass", 1),
    ST("kernel32.dll", "GetProcAddress", 2),
    ST("kernel32.dll", "GetProcessAffinityMask", 3),
    ST("kernel32.dll", "GetProcessHeap", 0),
    ST("kernel32.dll", "GetProcessId", 1),
    ST("kernel32.dll", "GetProcessTimes", 5),
    ST("kernel32.dll", "GetProductInfo", 5),
    ST("kernel32.dll", "GetQueuedCompletionStatus", 5),
    ST("kernel32.dll", "GetShortPathNameA", 3),
    ST("kernel32.dll", "GetStartupInfoW", 1),
    ST("kernel32.dll", "GetStartupInfoA", 1),
    ST("kernel32.dll", "IsDBCSLeadByte", 1),
    ST("kernel32.dll", "GetStdHandle", 1),
    ST("kernel32.dll", "GetStringTypeExW", 5),
    ST("kernel32.dll", "GetStringTypeW", 4),
    ST("kernel32.dll", "GetStringTypeExA", 4),
    ST("kernel32.dll", "GetSystemDefaultLCID", 0),
    ST("kernel32.dll", "GetSystemDefaultLocaleName", 2),
    ST("kernel32.dll", "GetSystemDirectoryW", 2),
    ST("kernel32.dll", "GetSystemInfo", 1),
    ST("kernel32.dll", "GetSystemPowerStatus", 1),
    ST("kernel32.dll", "GetSystemPreferredUILanguages", 4),
    ST("kernel32.dll", "GetSystemTime", 1),
    ST("kernel32.dll", "GetSystemTimeAsFileTime", 1),
    ST("kernel32.dll", "GetTempFileNameW", 4),
    ST("kernel32.dll", "GetTempPathA", 2),
    ST("kernel32.dll", "GetTempPathW", 2),
    ST("kernel32.dll", "GetThreadId", 1),
    ST("kernel32.dll", "GetThreadIOPendingFlag", 2),
    ST("kernel32.dll", "GetThreadLocale", 0),
    ST("kernel32.dll", "GetThreadTimes", 5),
    ST("kernel32.dll", "GetTickCount", 0),
    ST("kernel32.dll", "GetTickCount64", 0),
    ST("kernel32.dll", "GetTimeFormatW", 6),
    ST("kernel32.dll", "GetTimeZoneInformation", 1),
    ST("kernel32.dll", "GetUserDefaultLCID", 0),
    ST("kernel32.dll", "GetUserDefaultLocaleName", 2),
    ST("kernel32.dll", "GetUserGeoID", 1),
    ST("kernel32.dll", "GetUserPreferredUILanguages", 4),
    ST("kernel32.dll", "GetVersionExA", 1),
    ST("kernel32.dll", "GetVersionExW", 1),
    ST("kernel32.dll", "GetVolumeInformationW", 8),
    ST("kernel32.dll", "GetVolumeNameForVolumeMountPointW", 3),
    ST("kernel32.dll", "GetVolumePathNamesForVolumeNameW", 4),
    ST("kernel32.dll", "GetVolumePathNameW", 3),
    ST("kernel32.dll", "GlobalAlloc", 2),
    ST("kernel32.dll", "GlobalFree", 1),
    ST("kernel32.dll", "GlobalMemoryStatusEx", 1),
    ST("kernel32.dll", "GlobalReAlloc", 3),
    ST("kernel32.dll", "GlobalSize", 1),
    ST("kernel32.dll", "HeapAlloc", 3),
    ST("kernel32.dll", "HeapCompact", 2),
    ST("kernel32.dll", "HeapCreate", 3),
    ST("kernel32.dll", "HeapDestroy", 1),
    ST("kernel32.dll", "HeapFree", 3),
    ST("kernel32.dll", "HeapReAlloc", 4),
    ST("kernel32.dll", "HeapSetInformation", 4),
    ST("kernel32.dll", "HeapSize", 3),
    ST("kernel32.dll", "HeapValidate", 3),
    ST("kernel32.dll", "HeapWalk", 3),
    ST("kernel32.dll", "InitializeConditionVariable", 1),
    ST("kernel32.dll", "InitializeCriticalSection", 1),
    ST("kernel32.dll", "InitializeCriticalSectionAndSpinCount", 2),
    ST("kernel32.dll", "InitializeCriticalSectionEx", 3),
    ST("kernel32.dll", "InitializeSListHead", 1),
    ST("kernel32.dll", "InitOnceBeginInitialize", 4),
    ST("kernel32.dll", "InitOnceComplete", 3),
    ST("kernel32.dll", "AppPolicyGetProcessTerminationMethod", 2),
    ST("kernel32.dll", "AppPolicyGetThreadInitializationType", 2),
    ST("kernel32.dll", "InitOnceExecuteOnce", 4),
    ST("kernel32.dll", "InterlockedPopEntrySList", 1),
    ST("kernel32.dll", "InterlockedPushEntrySList", 2),
    ST("kernel32.dll", "IsDebuggerPresent", 0),
    ST("kernel32.dll", "IsProcessorFeaturePresent", 1),
    ST("kernel32.dll", "IsSystemResumeAutomatic", 0),
    ST("kernel32.dll", "IsValidCodePage", 1),
    ST("kernel32.dll", "IsValidLocale", 2),
    ST("kernel32.dll", "IsWow64Process", 2),
    ST("kernel32.dll", "K32EnumProcesses", 3),
    ST("kernel32.dll", "K32EnumProcessModulesEx", 5),
    ST("kernel32.dll", "K32GetModuleFileNameExW", 4),
    ST("kernel32.dll", "K32GetProcessImageFileNameW", 3),
    ST("kernel32.dll", "K32GetProcessMemoryInfo", 3),
    ST("kernel32.dll", "LCIDToLocaleName", 4),
    ST("kernel32.dll", "LCMapStringEx", 9),
    ST("kernel32.dll", "LCMapStringW", 6),
    ST("kernel32.dll", "LeaveCriticalSection", 1),
    ST("kernel32.dll", "LoadLibraryA", 1),
    ST("kernel32.dll", "LoadLibraryExA", 3),
    ST("kernel32.dll", "LoadLibraryExW", 3),
    ST("kernel32.dll", "LoadLibraryW", 1),
    ST("kernel32.dll", "LoadResource", 2),
    ST("kernel32.dll", "LocalAlloc", 2),
    ST("kernel32.dll", "LocalFree", 1),
    ST("kernel32.dll", "LocalReAlloc", 3),
    ST("kernel32.dll", "LocalSize", 1),
    ST("kernel32.dll", "LocaleNameToLCID", 2),
    ST("kernel32.dll", "LockFile", 5),
    ST("kernel32.dll", "LockFileEx", 6),
    ST("kernel32.dll", "LockResource", 1),
    ST("kernel32.dll", "lstrcmpiW", 2),
    ST("kernel32.dll", "lstrcmpW", 2),
    ST("kernel32.dll", "lstrcpyW", 2),
    ST("kernel32.dll", "lstrlenW", 1),
    ST("kernel32.dll", "MapViewOfFile", 5),
    ST("kernel32.dll", "MoveFileExW", 3),
    ST("kernel32.dll", "MulDiv", 3),
    ST("kernel32.dll", "MultiByteToWideChar", 6),
    ST("kernel32.dll", "NtQueryInformationProcess", 5),
    ST("kernel32.dll", "NtQueryInformationThread", 5),
    ST("kernel32.dll", "OpenEventA", 3),
    ST("kernel32.dll", "OpenEventW", 3),
    ST("kernel32.dll", "OpenFileMappingA", 3),
    ST("kernel32.dll", "OpenMutexA", 3),
    ST("kernel32.dll", "OpenMutexW", 3),
    ST("kernel32.dll", "OpenProcess", 3),
    ST("kernel32.dll", "OpenSemaphoreA", 3),
    ST("kernel32.dll", "OpenThread", 3),
    ST("kernel32.dll", "OutputDebugStringA", 1),
    ST("kernel32.dll", "OutputDebugStringW", 1),
    ST("kernel32.dll", "PeekNamedPipe", 6),
    ST("kernel32.dll", "PostQueuedCompletionStatus", 4),
    ST("kernel32.dll", "ProcessIdToSessionId", 2),
    ST("kernel32.dll", "PulseEvent", 1),
    ST("kernel32.dll", "QueryDepthSList", 1),
    ST("kernel32.dll", "QueryFullProcessImageNameW", 4),
    ST("kernel32.dll", "QueryPerformanceCounter", 1),
    ST("kernel32.dll", "QueryPerformanceFrequency", 1),
    ST("kernel32.dll", "QueryProcessCycleTime", 2),
    ST("kernel32.dll", "QueryThreadCycleTime", 2),
    ST("kernel32.dll", "QueryUnbiasedInterruptTime", 1),
    ST("kernel32.dll", "RaiseException", 4),
    ST("kernel32.dll", "ReadConsoleW", 5),
    ST("kernel32.dll", "ReadFile", 5),
    ST("kernel32.dll", "ReadProcessMemory", 5),
    ST("kernel32.dll", "ReleaseMutex", 1),
    ST("kernel32.dll", "ReleaseSemaphore", 3),
    ST("kernel32.dll", "ReleaseSRWLockExclusive", 1),
    ST("kernel32.dll", "ReleaseSRWLockShared", 1),
    ST("kernel32.dll", "RemoveDirectoryW", 1),
    ST("kernel32.dll", "RemoveVectoredContinueHandler", 1),
    ST("kernel32.dll", "RemoveVectoredExceptionHandler", 1),
    ST("kernel32.dll", "ResetEvent", 1),
    ST("kernel32.dll", "ResumeThread", 1),
    ST("kernel32.dll", "RtlCaptureStackBackTrace", 4),
    ST("kernel32.dll", "RtlUnwind", 4),
    ST("kernel32.dll", "RtlUnwindEx", 6),
    ST("kernel32.dll", "SetConsoleMode", 2),
    ST("kernel32.dll", "SetCurrentDirectoryA", 1),
    ST("kernel32.dll", "SetCurrentDirectoryW", 1),
    ST("kernel32.dll", "SetEndOfFile", 1),
    ST("kernel32.dll", "SetEnvironmentVariableA", 2),
    ST("kernel32.dll", "SetEnvironmentVariableW", 2),
    ST("kernel32.dll", "SetErrorMode", 1),
    ST("kernel32.dll", "SetEvent", 1),
    ST("kernel32.dll", "SetFileAttributesW", 2),
    ST("kernel32.dll", "SetFileInformationByHandle", 4),
    ST("kernel32.dll", "SetFilePointer", 4),
    STW("kernel32.dll", "SetFilePointerEx", 5, (1u << 1)),
    ST("kernel32.dll", "SetFileTime", 4),
    ST("kernel32.dll", "SetHandleInformation", 3),
    ST("kernel32.dll", "SetLastError", 1),
    ST("kernel32.dll", "SetNamedPipeHandleState", 4),
    ST("kernel32.dll", "SetProcessMitigationPolicy", 3),
    ST("kernel32.dll", "SetStdHandle", 2),
    ST("kernel32.dll", "SetThreadContext", 2),
    ST("kernel32.dll", "SetThreadpoolTimer", 4),
    ST("kernel32.dll", "SetThreadpoolWait", 3),
    ST("kernel32.dll", "SetThreadPriority", 2),
    ST("kernel32.dll", "SetUnhandledExceptionFilter", 1),
    ST("kernel32.dll", "SetWaitableTimer", 6),
    ST("kernel32.dll", "SetWaitableTimerEx", 7),
    ST("kernel32.dll", "SizeofResource", 2),
    ST("kernel32.dll", "Sleep", 1),
    ST("kernel32.dll", "SleepConditionVariableCS", 3),
    ST("kernel32.dll", "SleepConditionVariableSRW", 4),
    ST("kernel32.dll", "SubmitThreadpoolWork", 1),
    ST("kernel32.dll", "SuspendThread", 1),
    ST("kernel32.dll", "SwitchToThread", 0),
    ST("kernel32.dll", "SystemTimeToFileTime", 2),
    ST("kernel32.dll", "TerminateProcess", 2),
    ST("kernel32.dll", "TerminateThread", 2),
    ST("kernel32.dll", "TlsAlloc", 0),
    ST("kernel32.dll", "TlsFree", 1),
    ST("kernel32.dll", "TlsGetValue", 1),
    ST("kernel32.dll", "TlsSetValue", 2),
    ST("kernel32.dll", "TryAcquireSRWLockExclusive", 1),
    ST("kernel32.dll", "TryAcquireSRWLockShared", 1),
    ST("kernel32.dll", "TryEnterCriticalSection", 1),
    ST("kernel32.dll", "TzSpecificLocalTimeToSystemTime", 3),
    ST("kernel32.dll", "UnhandledExceptionFilter", 1),
    ST("kernel32.dll", "UnlockFile", 5),
    ST("kernel32.dll", "UnlockFileEx", 5),
    ST("kernel32.dll", "UnmapViewOfFile", 1),
    STW("kernel32.dll", "VerifyVersionInfoW", 4, (1u << 2)),
    STW("kernel32.dll", "VerSetConditionMask", 4, (1u << 0)),
    ST("kernel32.dll", "VirtualAlloc", 4),
    ST("kernel32.dll", "VirtualFree", 3),
    ST("kernel32.dll", "VirtualProtect", 4),
    ST("kernel32.dll", "VirtualProtectEx", 5),
    ST("kernel32.dll", "VirtualQuery", 3),
    ST("kernel32.dll", "VirtualQueryEx", 4),
    ST("kernel32.dll", "WaitForMultipleObjects", 4),
    ST("kernel32.dll", "WaitForMultipleObjectsEx", 5),
    ST("kernel32.dll", "WaitForSingleObject", 2),
    ST("kernel32.dll", "WaitForSingleObjectEx", 3),
    ST("kernel32.dll", "WaitForThreadpoolTimerCallbacks", 2),
    ST("kernel32.dll", "WaitForThreadpoolWaitCallbacks", 2),
    ST("kernel32.dll", "WaitNamedPipeA", 2),
    ST("kernel32.dll", "WaitNamedPipeW", 2),
    ST("kernel32.dll", "WakeAllConditionVariable", 1),
    ST("kernel32.dll", "WakeConditionVariable", 1),
    ST("kernel32.dll", "WerRegisterMemoryBlock", 2),
    ST("kernel32.dll", "WerUnregisterMemoryBlock", 1),
    ST("kernel32.dll", "WideCharToMultiByte", 8),
    ST("kernel32.dll", "Wow64GetThreadContext", 2),
    ST("kernel32.dll", "WriteConsoleW", 5),
    ST("kernel32.dll", "WriteFile", 5),
    ST("kernel32.dll", "WriteProcessMemory", 5),
    ST("kernel32.dll", "SetDefaultDllDirectories", 1),
    ST("kernel32.dll", "SetProcessDPIAware", 0),
    ST("advapi32.dll", "LsaOpenPolicy", 4),
    ST("advapi32.dll", "LsaClose", 1),
    ST("advapi32.dll", "LsaFreeMemory", 1),
    ST("advapi32.dll", "LsaNtStatusToWinError", 1),
    ST("advapi32.dll", "LsaQueryInformationPolicy", 3),
    ST("advapi32.dll", "LsaEnumerateLogonSessions", 2),
    ST("shell32.dll", "CommandLineToArgvW", 2),    ST("ws2_32.dll", "WSAStartup", 2),
    ST("ws2_32.dll", "WSACleanup", 0),
    ST("ws2_32.dll", "WSAGetLastError", 0),
    ST("ws2_32.dll", "WSASetLastError", 1),
    ST("ws2_32.dll", "socket", 3),
    ST("ws2_32.dll", "closesocket", 1),
    ST("ws2_32.dll", "bind", 3),
    ST("ws2_32.dll", "listen", 2),
    ST("ws2_32.dll", "accept", 3),
    ST("ws2_32.dll", "connect", 3),
    ST("ws2_32.dll", "send", 4),
    ST("ws2_32.dll", "recv", 4),
    ST("ws2_32.dll", "sendto", 6),
    ST("ws2_32.dll", "recvfrom", 6),
    ST("ws2_32.dll", "setsockopt", 5),
    ST("ws2_32.dll", "getsockopt", 5),
    ST("ws2_32.dll", "getsockname", 3),
    ST("ws2_32.dll", "getpeername", 3),
    ST("ws2_32.dll", "select", 5),
    ST("ws2_32.dll", "ioctlsocket", 3),
    ST("ws2_32.dll", "shutdown", 2),
    ST("ws2_32.dll", "htonl", 1),
    ST("ws2_32.dll", "htons", 1),
    ST("ws2_32.dll", "ntohl", 1),
    ST("ws2_32.dll", "ntohs", 1),
    ST("ws2_32.dll", "inet_addr", 1),
    ST("ws2_32.dll", "inet_ntoa", 1),
    ST("ws2_32.dll", "gethostname", 2),
    ST("ws2_32.dll", "gethostbyname", 1),
    ST("user32.dll", "GetDC", 1),
    ST("user32.dll", "GetWindowDC", 1),
    ST("user32.dll", "ReleaseDC", 2),
    ST("user32.dll", "BeginPaint", 2),
    ST("user32.dll", "EndPaint", 2),
    ST("user32.dll", "GetClientRect", 2),
    ST("user32.dll", "GetSystemMetrics", 1),
    ST("user32.dll", "SetProcessDPIAware", 0),
    ST("user32.dll", "SystemParametersInfoW", 4),
    ST("gdi32.dll", "CreateCompatibleDC", 1),
    ST("gdi32.dll", "DeleteDC", 1),
    ST("gdi32.dll", "GetStockObject", 1),
    ST("gdi32.dll", "SelectObject", 2),
    ST("gdi32.dll", "GetDeviceCaps", 2),
    ST("gdi32.dll", "TextOutA", 5),
    ST("gdi32.dll", "TextOutW", 5),
    ST("gdi32.dll", "BitBlt", 9),
    ST("gdi32.dll", "SetBkMode", 2),
    ST("gdi32.dll", "SetTextColor", 2),
    ST("gdi32.dll", "SetBkColor", 2),
    ST("gdi32.dll", "CreateFontA", 14),
    ST("gdi32.dll", "DeleteObject", 1),
    ST("gdi32.dll", "CreateSolidBrush", 1),
    ST("gdi32.dll", "CreatePen", 3),

    // ---- ntdll.dll --------------------------------------------------------
    ST("ntdll.dll", "NtQueryInformationProcess", 5),
    ST("ntdll.dll", "NtQueryInformationThread", 5),
    ST("ntdll.dll", "NtOpenKey", 3),
    ST("ntdll.dll", "NtCreateKey", 7),
    ST("ntdll.dll", "NtFsControlFile", 10),
    ST("ntdll.dll", "NtCancelIoFileEx", 3),
    ST("ntdll.dll", "NtPowerInformation", 5),
    ST("ntdll.dll", "RtlAcquirePebLock", 0),
    ST("ntdll.dll", "RtlReleasePebLock", 0),
    ST("ntdll.dll", "RtlPcToFileHeader", 2),
    ST("ntdll.dll", "RtlCompareString", 3),
    ST("ntdll.dll", "RtlEqualUnicodeString", 3),
    ST("ntdll.dll", "RtlInitString", 2),
    ST("ntdll.dll", "RtlCreateUnicodeString", 2),
    ST("ntdll.dll", "RtlCreateEnvironment", 2),
    ST("ntdll.dll", "RtlDestroyEnvironment", 1),
    ST("ntdll.dll", "RtlQueryEnvironmentVariable_U", 3),
    ST("ntdll.dll", "RtlSetEnvironmentVariable", 3),
    ST("ntdll.dll", "RtlExpandEnvironmentStrings_U", 3),
    ST("ntdll.dll", "RtlGUIDFromString", 2),
    ST("ntdll.dll", "RtlIpv4AddressToStringA", 2),
    ST("ntdll.dll", "RtlIpv4AddressToStringW", 2),
    ST("ntdll.dll", "RtlIpv4StringToAddressA", 4),
    ST("ntdll.dll", "RtlIpv4StringToAddressW", 4),
    ST("ntdll.dll", "RtlIpv4StringToAddressExW", 4),
    ST("ntdll.dll", "RtlIpv6StringToAddressA", 3),
    ST("ntdll.dll", "RtlIpv6StringToAddressW", 3),
    ST("ntdll.dll", "RtlIpv6StringToAddressExW", 4),
    ST("ntdll.dll", "RtlIpv6AddressToStringExW", 5),
    ST("ntdll.dll", "NtQuerySystemInformation", 4),
    ST("ntdll.dll", "NtQueryVirtualMemory", 6),
    ST("ntdll.dll", "NtSetInformationThread", 4),
    ST("ntdll.dll", "NtQueryInformationFile", 5),
    ST("ntdll.dll", "NtSetInformationFile", 5),
    ST("ntdll.dll", "NtQueryDirectoryFile", 11),
    ST("ntdll.dll", "RtlAllocateHeap", 3),
    ST("ntdll.dll", "RtlFreeHeap", 3),
    ST("ntdll.dll", "RtlReAllocateHeap", 4),
    ST("ntdll.dll", "RtlSizeHeap", 3),
    ST("ntdll.dll", "RtlCreateHeap", 6),
    ST("ntdll.dll", "RtlDestroyHeap", 1),
    ST("ntdll.dll", "RtlAllocateHandle", 2),
    ST("ntdll.dll", "RtlFreeHandle", 1),
    ST("ntdll.dll", "RtlGetVersion", 1),
    ST("ntdll.dll", "RtlUnwind", 3),
    ST("ntdll.dll", "RtlRaiseException", 1),
    ST("ntdll.dll", "RtlRaiseStatus", 1),
    ST("ntdll.dll", "RtlInitUnicodeString", 2),
    ST("ntdll.dll", "RtlInitAnsiString", 2),
    ST("ntdll.dll", "RtlCreateUnicodeStringFromAsciiz", 2),
    ST("ntdll.dll", "RtlFreeUnicodeString", 1),
    ST("ntdll.dll", "RtlFreeAnsiString", 1),
    ST("ntdll.dll", "RtlUnicodeStringToAnsiString", 3),
    ST("ntdll.dll", "RtlAnsiStringToUnicodeString", 3),
    ST("ntdll.dll", "RtlCopyUnicodeString", 2),
    ST("ntdll.dll", "RtlEnterCriticalSection", 1),
    ST("ntdll.dll", "RtlLeaveCriticalSection", 1),
    ST("ntdll.dll", "RtlInitializeCriticalSection", 1),
    ST("ntdll.dll", "RtlDeleteCriticalSection", 1),
    ST("ntdll.dll", "RtlAcquireResourceShared", 1),
    ST("ntdll.dll", "RtlAcquireResourceExclusive", 1),
    ST("ntdll.dll", "RtlReleaseResource", 1),
    ST("ntdll.dll", "RtlConvertSRWLockExclusiveToShared", 1),
    ST("ntdll.dll", "RtlConvertSRWLockSharedToExclusive", 1),
    ST("ntdll.dll", "RtlAddVectoredExceptionHandler", 2),
    ST("ntdll.dll", "RtlRemoveVectoredExceptionHandler", 1),
    ST("ntdll.dll", "RtlAddFunctionTable", 3),
    ST("ntdll.dll", "RtlCaptureStackBackTrace", 4),
    ST("ntdll.dll", "RtlGetSystemTimePrecise", 0),
    ST("ntdll.dll", "RtlGetCurrentPeb", 0),
    ST("ntdll.dll", "RtlGetCurrentTeb", 0),
    ST("ntdll.dll", "RtlGetNtSystemRoot", 0),
    ST("ntdll.dll", "RtlNtStatusToDosError", 1),
    ST("ntdll.dll", "RtlDosPathNameToNtPathName_U", 4),
    ST("ntdll.dll", "RtlIsNameLegalDOS8Dot3", 3),
    ST("ntdll.dll", "RtlPrefixUnicodeString", 3),
    ST("ntdll.dll", "RtlCompareUnicodeString", 2),
    ST("ntdll.dll", "RtlEqualUnicodeString", 2),
    ST("ntdll.dll", "RtlUpcaseUnicodeChar", 1),
    ST("ntdll.dll", "RtlDowncaseUnicodeChar", 1),
    ST("ntdll.dll", "RtlTimeToSecondsSince1970", 1),
    ST("ntdll.dll", "RtlUnicodeToUTF8N", 5),
    ST("ntdll.dll", "RtlUTF8ToUnicodeN", 5),
    ST("ntdll.dll", "RtlZeroMemory", 2),
    ST("ntdll.dll", "RtlMoveMemory", 3),
    ST("ntdll.dll", "RtlCopyMemory", 3),
    ST("ntdll.dll", "RtlFillMemory", 3),
    ST("ntdll.dll", "RtlGetLastWin32Error", 0),
    ST("ntdll.dll", "RtlSetLastWin32Error", 1),
    ST("ntdll.dll", "RtlRestoreLastWin32Error", 1),
    ST("ntdll.dll", "RtlSetLastWin32ErrorAndNtStatusFromNtStatus", 1),
    ST("ntdll.dll", "RtlLookupFunctionEntry", 3),
    ST("ntdll.dll", "RtlVirtualUnwind", 7),
    ST("ntdll.dll", "RtlImageNtHeader", 1),
    ST("ntdll.dll", "RtlImageDirectoryEntryToData", 4),
    ST("ntdll.dll", "NtCurrentTeb", 0),
    ST("ntdll.dll", "NtGetTickCount", 0),
    ST("ntdll.dll", "NtQueryPerformanceCounter", 2),
    ST("ntdll.dll", "NtQuerySystemTime", 1),
    ST("ntdll.dll", "NtDelayExecution", 2),
    ST("ntdll.dll", "NtYieldExecution", 0),
    ST("ntdll.dll", "NtTerminateProcess", 2),
    ST("ntdll.dll", "NtTerminateThread", 2),
    ST("ntdll.dll", "NtCreateEvent", 5),
    ST("ntdll.dll", "NtOpenEvent", 4),
    ST("ntdll.dll", "NtSetEvent", 2),
    ST("ntdll.dll", "NtResetEvent", 2),
    ST("ntdll.dll", "NtPulseEvent", 2),
    ST("ntdll.dll", "NtCreateMutant", 4),
    ST("ntdll.dll", "NtOpenMutant", 4),
    ST("ntdll.dll", "NtReleaseMutant", 2),
    ST("ntdll.dll", "NtCreateSemaphore", 5),
    ST("ntdll.dll", "NtOpenSemaphore", 4),
    ST("ntdll.dll", "NtReleaseSemaphore", 3),
    ST("ntdll.dll", "NtCreateTimer", 4),
    ST("ntdll.dll", "NtOpenTimer", 4),
    ST("ntdll.dll", "NtSetTimer", 6),
    ST("ntdll.dll", "NtCancelTimer", 3),
    ST("ntdll.dll", "NtQueryTimer", 5),
    ST("ntdll.dll", "NtWaitForSingleObject", 3),
    ST("ntdll.dll", "NtWaitForMultipleObjects", 5),
    ST("ntdll.dll", "NtCreateFile", 11),
    ST("ntdll.dll", "NtOpenFile", 10),
    ST("ntdll.dll", "NtReadFile", 9),
    ST("ntdll.dll", "NtWriteFile", 9),
    ST("ntdll.dll", "NtQueryVolumeInformationFile", 5),
    ST("ntdll.dll", "NtSetVolumeInformationFile", 5),
    ST("ntdll.dll", "NtClose", 1),
    ST("ntdll.dll", "NtDuplicateObject", 7),
    ST("ntdll.dll", "NtOpenProcess", 4),
    ST("ntdll.dll", "NtOpenThread", 4),
    ST("ntdll.dll", "NtGetContextThread", 2),
    ST("ntdll.dll", "NtSetContextThread", 2),
    ST("ntdll.dll", "NtSuspendThread", 2),
    ST("ntdll.dll", "NtResumeThread", 2),
    ST("ntdll.dll", "NtProtectVirtualMemory", 5),
    ST("ntdll.dll", "NtAllocateVirtualMemory", 6),
    ST("ntdll.dll", "NtFreeVirtualMemory", 4),
    ST("ntdll.dll", "NtQueryObject", 5),
    ST("ntdll.dll", "NtQuerySection", 5),
    ST("ntdll.dll", "NtCreateSection", 7),
    ST("ntdll.dll", "NtMapViewOfSection", 10),
    ST("ntdll.dll", "NtUnmapViewOfSection", 2),
    ST("ntdll.dll", "NtFlushBuffersFile", 2),
    ST("ntdll.dll", "NtFsControlFile", 10),
    ST("ntdll.dll", "NtDeviceIoControlFile", 10),
    ST("ntdll.dll", "NtCancelIoFile", 2),
    ST("ntdll.dll", "LdrLoadDll", 4),
    ST("ntdll.dll", "LdrUnloadDll", 1),
    ST("ntdll.dll", "LdrGetDllHandle", 4),
    ST("ntdll.dll", "LdrGetProcedureAddress", 4),
    ST("ntdll.dll", "LdrAddRefDll", 2),
    ST("ntdll.dll", "LdrDisableThreadCalloutsForDll", 1),
    ST("ntdll.dll", "LdrLockLoaderLock", 3),
    ST("ntdll.dll", "LdrUnlockLoaderLock", 2),
    ST("ntdll.dll", "LdrGetDllFullName", 2),
    ST("ntdll.dll", "LdrGetDllPath", 3),
    ST("ntdll.dll", "LdrSetDllDirectory", 1),
    ST("ntdll.dll", "LdrGetDllDirectory", 1),
    ST("ntdll.dll", "LdrResolveDelayLoadedAPI", 6),
    ST("ntdll.dll", "LdrAddDllDirectory", 2),
    ST("ntdll.dll", "LdrRemoveDllDirectory", 1),
    ST("ntdll.dll", "LdrGetDllDirectoryEx", 2),
    ST("ntdll.dll", "LdrRegisterDllNotification", 4),
    ST("ntdll.dll", "LdrUnregisterDllNotification", 1),
    ST("ntdll.dll", "LdrShutdownProcess", 0),
    ST("ntdll.dll", "LdrShutdownThread", 0),
    ST("ntdll.dll", "LdrSetDllManifestProber", 1),
    ST("ntdll.dll", "LdrEnumResources", 5),
    ST("ntdll.dll", "LdrQueryImageFileExecutionOptions", 8),
    ST("ntdll.dll", "LdrDisableThreadCalloutsForDll", 1),

    // ---- user32.dll --------------------------------------------------------
    ST("user32.dll", "BeginPaint", 2),
    ST("user32.dll", "CreateWindowExA", 12),
    ST("user32.dll", "DefWindowProcA", 4),
    ST("user32.dll", "DestroyWindow", 1),
    ST("user32.dll", "DispatchMessageA", 1),
    ST("user32.dll", "EndPaint", 2),
    ST("user32.dll", "GetDesktopWindow", 0),
    ST("user32.dll", "GetMessageA", 4),
    ST("user32.dll", "InvalidateRect", 3),
    ST("user32.dll", "MessageBoxA", 4),
    ST("user32.dll", "MessageBoxW", 4),
    ST("user32.dll", "PeekMessageA", 5),
    ST("user32.dll", "PostMessageA", 4),
    ST("user32.dll", "PostQuitMessage", 1),
    ST("user32.dll", "RegisterClassA", 1),
    ST("user32.dll", "SendMessageA", 4),
    ST("user32.dll", "SetTimer", 4),
    ST("user32.dll", "KillTimer", 2),
    ST("user32.dll", "ShowWindow", 2),
    ST("user32.dll", "TranslateMessage", 1),
    ST("user32.dll", "UpdateWindow", 1),
    ST("user32.dll", "GetDlgItem", 2),
    ST("user32.dll", "GetDlgCtrlID", 1),
    ST("user32.dll", "GetDlgItemTextA", 4),
    ST("user32.dll", "SetDlgItemTextA", 3),
    ST("user32.dll", "SendDlgItemMessageA", 5),
    ST("user32.dll", "CheckDlgButton", 3),
    ST("user32.dll", "IsDlgButtonChecked", 2),
    ST("user32.dll", "EnableWindow", 2),
    ST("user32.dll", "IsWindowEnabled", 1),
    ST("user32.dll", "IsWindowVisible", 1),
    ST("user32.dll", "IsWindow", 1),
    ST("user32.dll", "GetWindowTextA", 3),
    ST("user32.dll", "GetWindowTextLengthA", 1),
    ST("user32.dll", "SetWindowTextA", 2),
    ST("user32.dll", "SetFocus", 1),
    ST("user32.dll", "GetFocus", 0),
    ST("user32.dll", "DialogBoxParamA", 5),
    ST("user32.dll", "EndDialog", 2),

    // ---- gdi32.dll --------------------------------------------------------
    ST("gdi32.dll", "CreateSolidBrush", 1),
    ST("gdi32.dll", "DeleteObject", 1),
    ST("gdi32.dll", "GetStockObject", 1),
    ST("gdi32.dll", "SelectObject", 2),
    ST("gdi32.dll", "TextOutA", 5),
    ST("gdi32.dll", "TextOutW", 5),

    // ---- ole32.dll --------------------------------------------------------
    ST("ole32.dll", "CLSIDFromString", 2),
    ST("ole32.dll", "CoCancelCall", 1),
    ST("ole32.dll", "CoCreateFreeThreadedMarshaler", 2),
    ST("ole32.dll", "CoCreateGuid", 1),
    ST("ole32.dll", "CoCreateInstance", 5),
    ST("ole32.dll", "CoDisableCallCancellation", 1),
    ST("ole32.dll", "CoEnableCallCancellation", 1),
    ST("ole32.dll", "CoInitialize", 1),
    ST("ole32.dll", "CoInitializeEx", 2),
    ST("ole32.dll", "CoInitializeSecurity", 7),
    ST("ole32.dll", "CoSetProxyBlanket", 8),
    ST("ole32.dll", "CoTaskMemAlloc", 1),
    ST("ole32.dll", "CoTaskMemFree", 1),
    ST("ole32.dll", "CoUninitialize", 0),
    ST("ole32.dll", "CreateStreamOnHGlobal", 3),
    ST("ole32.dll", "IIDFromString", 2),
    ST("ole32.dll", "StringFromCLSID", 2),
    ST("ole32.dll", "StringFromGUID2", 3),
    ST("ole32.dll", "StringFromIID", 2),

    // ---- oleaut32.dll -----------------------------------------------------
    ST("oleaut32.dll", "GetErrorInfo", 2),
    ST("oleaut32.dll", "SetErrorInfo", 2),
    ST("oleaut32.dll", "SysAllocString", 1),
    ST("oleaut32.dll", "SysAllocStringLen", 2),
    ST("oleaut32.dll", "SysFreeString", 1),
    ST("oleaut32.dll", "SysReAllocString", 2),
    ST("oleaut32.dll", "SysReAllocStringLen", 3),
    ST("oleaut32.dll", "SysStringLen", 1),
    ST("oleaut32.dll", "VariantChangeType", 4),
    ST("oleaut32.dll", "VariantClear", 1),
    ST("oleaut32.dll", "VariantCopy", 2),
    ST("oleaut32.dll", "VariantCopyInd", 2),
    ST("oleaut32.dll", "VariantInit", 1),

    // ---- ucrtbase.dll / api-ms-win-crt-* / msvcrt.dll (cdecl) -------------
    CT("ucrtbase.dll", "__C_specific_handler", 4),
    CT("vcruntime140.dll", "__C_specific_handler", 4),
    CT("api-ms-win-crt-private-l1-1-0.dll", "__C_specific_handler", 4),
    CT("ucrtbase.dll", "_CxxThrowException", 2),
    CT("ucrtbase.dll", "_except_handler3", 4),
    CT("msvcrt.dll", "_except_handler3", 4),
    CT("ucrtbase.dll", "_except_handler4_common", 6),
    CT("msvcrt.dll", "_except_handler4_common", 6),
    CT("ucrtbase.dll", "_local_unwind2", 2),
    CT("msvcrt.dll", "_local_unwind2", 2),
    CT("ucrtbase.dll", "_local_unwind4", 3),
    CT("msvcrt.dll", "_local_unwind4", 3),
    CT("msvcrt.dll", "_global_unwind2", 1),

    CT("ucrtbase.dll", "_set_app_type", 1),
    CT("ucrtbase.dll", "__set_app_type", 1),
    CT("ucrtbase.dll", "_configure_narrow_argv", 1),
    CT("ucrtbase.dll", "_initialize_narrow_environment", 0),
    CT("ucrtbase.dll", "_set_invalid_parameter_handler", 1),
    CT("ucrtbase.dll", "__p___argc", 0),
    CT("ucrtbase.dll", "__p___argv", 0),
    CT("ucrtbase.dll", "_crt_atexit", 1),
    CT("ucrtbase.dll", "_onexit", 1),
    CT("ucrtbase.dll", "_initterm", 2),
    CT("ucrtbase.dll", "_initterm_e", 2),
    CT("ucrtbase.dll", "_cexit", 0),
    CT("ucrtbase.dll", "abort", 0),
    CT("ucrtbase.dll", "exit", 1),
    CT("ucrtbase.dll", "_exit", 1),
    CT("ucrtbase.dll", "signal", 2),
    CT("ucrtbase.dll", "_set_new_mode", 1),
    CT("ucrtbase.dll", "printf", 1),
    CT("ucrtbase.dll", "puts", 1),
    CT("ucrtbase.dll", "fprintf", 2),
    CT("ucrtbase.dll", "fflush", 1),
    CT("ucrtbase.dll", "__acrt_iob_func", 1),
    CT("ucrtbase.dll", "__p__commode", 0),
    CT("ucrtbase.dll", "__p__fmode", 0),
    CT("ucrtbase.dll", "setvbuf", 4),
    CTW("ucrtbase.dll", "__stdio_common_vfprintf", 6, (1u << 0)),
    // 64-bit arg counts:
    // vfprintf = 5 (options, stream, fmt, locale, va)
    // vswprintf_s/vsnwprintf_s = 7 (options, buf, bufsz, maxcount, fmt, locale, va)
    // vsprintf_s/vswprintf_s = 6 (options, buf, bufsz, fmt, locale, va)
    // 32-bit: options takes 2 dword slots (wide bit0), so total slots = 8 and 7.
    CTW("ucrtbase.dll", "__stdio_common_vsnwprintf_s", 8, (1u << 0)),
    CTW("ucrtbase.dll", "__stdio_common_vsnprintf_s", 8, (1u << 0)),
    CTW("ucrtbase.dll", "__stdio_common_vswprintf_s", 7, (1u << 0)),
    CTW("ucrtbase.dll", "__stdio_common_vsprintf_s", 7, (1u << 0)),
    CTW("ucrtbase.dll", "__stdio_common_vsnprintf", 8, (1u << 0)),
    CTW("ucrtbase.dll", "__stdio_common_vsprintf", 7, (1u << 0)),
    CTW("ucrtbase.dll", "__stdio_common_vswprintf", 7, (1u << 0)),
    CTW("ucrtbase.dll", "_vsnprintf", 4, 0),
    CTW("ntdll.dll", "_vsnprintf", 4, 0),
    CT("ucrtbase.dll", "fwrite", 4),
    CT("ucrtbase.dll", "fread", 4),
    CT("ucrtbase.dll", "fgetc", 1),
    CT("ucrtbase.dll", "fopen", 2),
    CT("ucrtbase.dll", "fclose", 1),
    CT("ucrtbase.dll", "sqrt", 1),
    CT("ucrtbase.dll", "fabs", 1),
    CT("ucrtbase.dll", "__setusermatherr", 1),
    CT("ucrtbase.dll", "strlen", 1),
    CT("ucrtbase.dll", "strcpy", 2),
    CT("ucrtbase.dll", "strcat", 2),
    CT("ucrtbase.dll", "strcmp", 2),
    CT("ucrtbase.dll", "strncmp", 3),
    CT("ucrtbase.dll", "memcpy", 3),
    CT("ucrtbase.dll", "memset", 3),
    CT("ucrtbase.dll", "memmove", 3),
    CT("ucrtbase.dll", "memcmp", 3),
    CT("ucrtbase.dll", "_stricmp", 2),
    CT("ucrtbase.dll", "malloc", 1),
    CT("ucrtbase.dll", "free", 1),
    CT("ucrtbase.dll", "calloc", 2),
    CT("ucrtbase.dll", "realloc", 2),
    CT("ucrtbase.dll", "_setlocale", 2),
    CT("ucrtbase.dll", "setlocale", 2),
    CT("ucrtbase.dll", "localeconv", 0),
    CT("ucrtbase.dll", "_configthreadlocale", 1),
    CT("ucrtbase.dll", "_environ", 0),
    CT("ucrtbase.dll", "__p__environ", 0),

    // ---- wine_compat.cpp: gdi32.dll (Wine DLL compatibility) ----------------
    ST("gdi32.dll", "CreateCompatibleBitmap", 2),
    ST("gdi32.dll", "CreateBitmap", 5),
    ST("gdi32.dll", "CreateDIBSection", 6),
    ST("gdi32.dll", "CreateFontIndirectW", 1),
    ST("gdi32.dll", "CreatePatternBrush", 1),
    ST("gdi32.dll", "CreatePenIndirect", 1),
    ST("gdi32.dll", "CreatePolygonRgn", 3),
    ST("gdi32.dll", "CreateRectRgn", 4),
    ST("gdi32.dll", "CreateRoundRectRgn", 6),
    ST("gdi32.dll", "CreateRectRgnIndirect", 1),
    ST("gdi32.dll", "CombineRgn", 4),
    ST("gdi32.dll", "DPtoLP", 2),
    ST("gdi32.dll", "ExcludeClipRect", 4),
    ST("gdi32.dll", "ExtCreatePen", 4),
    ST("gdi32.dll", "ExtCreateRegion", 3),
    ST("gdi32.dll", "ExtTextOutW", 6),
    ST("gdi32.dll", "FillRgn", 3),
    ST("gdi32.dll", "FrameRgn", 5),
    ST("gdi32.dll", "GdiAlphaBlend", 10),
    ST("gdi32.dll", "GetBkColor", 1),
    ST("gdi32.dll", "GetBkMode", 1),
    ST("gdi32.dll", "GetClipBox", 2),
    ST("gdi32.dll", "GetClipRgn", 2),
    ST("gdi32.dll", "GetCurrentObject", 2),
    ST("gdi32.dll", "GetDIBColorTable", 4),
    ST("gdi32.dll", "GetDIBits", 6),
    ST("gdi32.dll", "GetLayout", 1),
    ST("gdi32.dll", "GetNearestColor", 2),
    ST("gdi32.dll", "GetObjectW", 3),
    ST("gdi32.dll", "GetPixel", 3),
    ST("gdi32.dll", "GetTextColor", 1),
    ST("gdi32.dll", "GetTextExtentPoint32A", 4),
    ST("gdi32.dll", "GetTextExtentPoint32W", 4),
    ST("gdi32.dll", "GetTextExtentPointA", 4),
    ST("gdi32.dll", "GetTextExtentPointW", 4),
    ST("gdi32.dll", "GetTextMetricsW", 2),
    ST("gdi32.dll", "IntersectClipRect", 4),
    ST("gdi32.dll", "MoveToEx", 4),
    ST("gdi32.dll", "OffsetWindowOrgEx", 4),
    ST("gdi32.dll", "PatBlt", 5),
    ST("gdi32.dll", "PolyPolyline", 3),
    ST("gdi32.dll", "RectVisible", 2),
    ST("gdi32.dll", "SelectClipRgn", 2),
    ST("gdi32.dll", "SelectPalette", 3),
    ST("gdi32.dll", "SetDIBColorTable", 4),
    ST("gdi32.dll", "SetDIBits", 6),
    ST("gdi32.dll", "SetPixel", 4),
    ST("gdi32.dll", "SetPolyFillMode", 2),
    ST("gdi32.dll", "SetTextAlign", 2),
    ST("gdi32.dll", "SetViewportOrgEx", 4),
    ST("gdi32.dll", "SetWindowOrgEx", 4),
    ST("gdi32.dll", "StretchDIBits", 9),

    // ---- wine_compat.cpp: user32.dll W-suffix (Wine DLL compatibility) ------
    ST("user32.dll", "AdjustWindowRect", 3),
    ST("user32.dll", "AdjustWindowRectEx", 4),
    ST("user32.dll", "AppendMenuW", 4),
    ST("user32.dll", "BeginDeferWindowPos", 1),
    ST("user32.dll", "CallWindowProcA", 5),
    ST("user32.dll", "CallWindowProcW", 5),
    ST("user32.dll", "CheckMenuItem", 3),
    ST("user32.dll", "ChildWindowFromPoint", 3),
    ST("user32.dll", "CopyIcon", 1),
    ST("user32.dll", "CopyImage", 5),
    ST("user32.dll", "CopyRect", 2),
    ST("user32.dll", "CreateCaret", 4),
    ST("user32.dll", "CreateDialogIndirectParamA", 5),
    ST("user32.dll", "CreateDialogIndirectParamW", 5),
    ST("user32.dll", "CreateIconIndirect", 1),
    ST("user32.dll", "CreatePopupMenu", 0),
    ST("user32.dll", "CreateWindowExW", 12),
    ST("user32.dll", "DefWindowProcW", 4),
    ST("user32.dll", "DeferWindowPos", 8),
    ST("user32.dll", "DestroyCaret", 0),
    ST("user32.dll", "DestroyCursor", 1),
    ST("user32.dll", "DestroyIcon", 1),
    ST("user32.dll", "DialogBoxParamW", 5),
    ST("user32.dll", "DispatchMessageW", 1),
    ST("user32.dll", "DrawEdge", 4),
    ST("user32.dll", "DrawFocusRect", 2),
    ST("user32.dll", "DrawFrameControl", 4),
    ST("user32.dll", "DrawIcon", 3),
    ST("user32.dll", "DrawIconEx", 6),
    ST("user32.dll", "DrawTextA", 4),
    ST("user32.dll", "DrawTextW", 4),
    ST("user32.dll", "EnableScrollBar", 3),
    ST("user32.dll", "EndDeferWindowPos", 1),
    ST("user32.dll", "FillRect", 3),
    ST("user32.dll", "FrameRect", 3),
    ST("user32.dll", "GetActiveWindow", 0),
    ST("user32.dll", "GetAsyncKeyState", 1),
    ST("user32.dll", "GetCapture", 0),
    ST("user32.dll", "GetClassLongW", 2),
    ST("user32.dll", "GetClassNameW", 3),
    ST("user32.dll", "GetCursorPos", 1),
    ST("user32.dll", "GetDCEx", 3),
    ST("user32.dll", "GetDoubleClickTime", 0),
    ST("user32.dll", "GetDpiForWindow", 1),
    ST("user32.dll", "GetIconInfo", 2),
    ST("user32.dll", "GetKeyNameTextW", 3),
    ST("user32.dll", "GetKeyState", 1),
    ST("user32.dll", "GetMenuState", 3),
    ST("user32.dll", "GetMessagePos", 0),
    ST("user32.dll", "GetMessageTime", 0),
    ST("user32.dll", "GetMessageW", 4),
    ST("user32.dll", "GetMonitorInfoW", 2),
    ST("user32.dll", "GetNextDlgTabItem", 3),
    ST("user32.dll", "GetParent", 1),
    ST("user32.dll", "GetPropW", 2),
    ST("user32.dll", "GetScrollInfo", 3),
    ST("user32.dll", "GetScrollPos", 2),
    ST("user32.dll", "GetScrollRange", 4),
    ST("user32.dll", "GetSysColor", 1),
    ST("user32.dll", "GetSysColorBrush", 1),
    ST("user32.dll", "GetTopWindow", 1),
    ST("user32.dll", "GetWindow", 2),
    ST("user32.dll", "GetWindowLongW", 2),
    ST("user32.dll", "GetWindowLongPtrW", 2),
    ST("user32.dll", "GetWindowRect", 2),
    ST("user32.dll", "GetWindowRgn", 2),
    ST("user32.dll", "GetWindowTextLengthW", 1),
    ST("user32.dll", "GetWindowTextW", 3),
    ST("user32.dll", "IntersectRect", 3),
    ST("user32.dll", "InvalidateRgn", 3),
    ST("user32.dll", "IsChild", 2),
    ST("user32.dll", "IsDialogMessageW", 2),
    ST("user32.dll", "IsWindowUnicode", 1),
    ST("user32.dll", "LoadBitmapW", 2),
    ST("user32.dll", "LoadCursorW", 2),
    ST("user32.dll", "LoadIconW", 2),
    ST("user32.dll", "LoadImageA", 6),
    ST("user32.dll", "LoadImageW", 6),
    ST("user32.dll", "LoadStringW", 4),
    ST("user32.dll", "MapDialogRect", 2),
    ST("user32.dll", "MapVirtualKeyW", 2),
    ST("user32.dll", "MapWindowPoints", 4),
    ST("user32.dll", "MonitorFromRect", 2),
    ST("user32.dll", "MoveWindow", 6),
    ST("user32.dll", "NotifyWinEvent", 4),
    ST("user32.dll", "PeekMessageW", 5),
    ST("user32.dll", "PostMessageW", 4),
    ST("user32.dll", "RedrawWindow", 4),
    ST("user32.dll", "RegisterClassW", 1),
    ST("user32.dll", "RegisterWindowMessageW", 1),
    ST("user32.dll", "ReleaseCapture", 0),
    ST("user32.dll", "RemovePropW", 2),
    ST("user32.dll", "ScreenToClient", 2),
    ST("user32.dll", "ScrollWindow", 4),
    ST("user32.dll", "ScrollWindowEx", 7),
    ST("user32.dll", "SendDlgItemMessageW", 5),
    ST("user32.dll", "SendMessageW", 4),
    ST("user32.dll", "SetCapture", 1),
    ST("user32.dll", "SetCaretPos", 2),
    ST("user32.dll", "SetCursor", 1),
    ST("user32.dll", "SetParent", 2),
    ST("user32.dll", "SetPropW", 3),
    ST("user32.dll", "SetScrollInfo", 3),
    ST("user32.dll", "SetScrollPos", 4),
    ST("user32.dll", "SetScrollRange", 4),
    ST("user32.dll", "SetWindowLongA", 3),
    ST("user32.dll", "SetWindowLongW", 3),
    ST("user32.dll", "SetWindowLongPtrA", 3),
    ST("user32.dll", "SetWindowLongPtrW", 3),
    ST("user32.dll", "SetWindowPos", 7),
    ST("user32.dll", "SetWindowRgn", 3),
    ST("user32.dll", "SetWindowTextW", 2),
    ST("user32.dll", "ShowCaret", 1),
    ST("user32.dll", "ShowScrollBar", 3),
    ST("user32.dll", "SubtractRect", 3),
    ST("user32.dll", "TrackMouseEvent", 1),
    ST("user32.dll", "TrackPopupMenu", 5),
    ST("user32.dll", "UnionRect", 3),
    ST("user32.dll", "UnregisterClassW", 2),
    ST("user32.dll", "ValidateRect", 2),
    ST("user32.dll", "WindowFromPoint", 4),
    ST("user32.dll", "wsprintfW", 3),

    // ---- wine_compat.cpp: kernel32.dll additional ---------------------------
    ST("kernel32.dll", "CompareFileTime", 2),
    ST("kernel32.dll", "CompareStringA", 6),
    ST("kernel32.dll", "CompareStringOrdinal", 4),
    ST("kernel32.dll", "IsBadCodePtr", 1),
    ST("kernel32.dll", "IsBadStringPtrA", 2),
    ST("kernel32.dll", "IsBadWritePtr", 2),
    ST("kernel32.dll", "GlobalAddAtomW", 1),
    ST("kernel32.dll", "GlobalDeleteAtom", 1),
    ST("kernel32.dll", "DelayLoadFailureHook", 0),
    ST("kernel32.dll", "FindResourceA", 3),

    // ---- wine_compat.cpp: shlwapi.dll ---------------------------------------
    ST("shlwapi.dll", "StrCmpIW", 2),
    ST("shlwapi.dll", "StrCmpNIW", 3),

    // ---- wine_compat.cpp: kernelbase.dll (Wine DLL compatibility) ------------
    ST("kernelbase.dll", "StrCmpIW", 2),
    ST("kernelbase.dll", "StrCmpNIW", 3),

    // ---- wine_compat.cpp: advapi32.dll additional ---------------------------
    // Registry imports are commonly loaded through both the normal and delay
    // import tables. Every one needs its real slot count: an unknown thunk gets
    // argc=0 and therefore neither reads nor removes the caller's arguments,
    // which corrupts the guest stack on return.
    ST("advapi32.dll", "RegCloseKey", 1),
    ST("advapi32.dll", "RegConnectRegistryA", 3),
    ST("advapi32.dll", "RegConnectRegistryW", 3),
    ST("advapi32.dll", "RegCopyTreeA", 3),
    ST("advapi32.dll", "RegCopyTreeW", 3),
    ST("advapi32.dll", "RegCreateKeyA", 3),
    ST("advapi32.dll", "RegCreateKeyW", 3),
    ST("advapi32.dll", "RegCreateKeyExA", 9),
    ST("advapi32.dll", "RegCreateKeyExW", 9),
    ST("advapi32.dll", "RegDeleteKeyA", 2),
    ST("advapi32.dll", "RegDeleteKeyW", 2),
    ST("advapi32.dll", "RegDeleteKeyExA", 4),
    ST("advapi32.dll", "RegDeleteKeyExW", 4),
    ST("advapi32.dll", "RegDeleteKeyValueA", 3),
    ST("advapi32.dll", "RegDeleteKeyValueW", 3),
    ST("advapi32.dll", "RegDeleteTreeA", 2),
    ST("advapi32.dll", "RegDeleteTreeW", 2),
    ST("advapi32.dll", "RegDeleteValueA", 2),
    ST("advapi32.dll", "RegDeleteValueW", 2),
    ST("advapi32.dll", "RegEnumKeyA", 4),
    ST("advapi32.dll", "RegEnumKeyW", 4),
    ST("advapi32.dll", "RegEnumKeyExA", 8),
    ST("advapi32.dll", "RegEnumKeyExW", 8),
    ST("advapi32.dll", "RegEnumValueA", 8),
    ST("advapi32.dll", "RegEnumValueW", 8),
    ST("advapi32.dll", "RegFlushKey", 1),
    ST("advapi32.dll", "RegGetKeySecurity", 4),
    ST("advapi32.dll", "RegGetValueA", 7),
    ST("advapi32.dll", "RegGetValueW", 7),
    ST("advapi32.dll", "RegLoadKeyA", 3),
    ST("advapi32.dll", "RegLoadKeyW", 3),
    ST("advapi32.dll", "RegLoadMUIStringA", 7),
    ST("advapi32.dll", "RegLoadMUIStringW", 7),
    ST("advapi32.dll", "RegNotifyChangeKeyValue", 5),
    ST("advapi32.dll", "RegOpenKeyA", 3),
    ST("advapi32.dll", "RegOpenKeyExA", 5),
    ST("advapi32.dll", "RegOpenKeyExW", 5),
    ST("advapi32.dll", "RegOpenKeyW", 3),
    ST("advapi32.dll", "RegQueryInfoKeyA", 12),
    ST("advapi32.dll", "RegQueryInfoKeyW", 12),
    ST("advapi32.dll", "RegQueryMultipleValuesA", 5),
    ST("advapi32.dll", "RegQueryMultipleValuesW", 5),
    ST("advapi32.dll", "RegQueryValueA", 4),
    ST("advapi32.dll", "RegQueryValueW", 4),
    ST("advapi32.dll", "RegQueryValueExA", 6),
    ST("advapi32.dll", "RegQueryValueExW", 6),
    ST("advapi32.dll", "RegRenameKey", 3),
    ST("advapi32.dll", "RegReplaceKeyA", 4),
    ST("advapi32.dll", "RegReplaceKeyW", 4),
    ST("advapi32.dll", "RegRestoreKeyA", 3),
    ST("advapi32.dll", "RegRestoreKeyW", 3),
    ST("advapi32.dll", "RegSaveKeyA", 3),
    ST("advapi32.dll", "RegSaveKeyW", 3),
    ST("advapi32.dll", "RegSaveKeyExA", 4),
    ST("advapi32.dll", "RegSaveKeyExW", 4),
    ST("advapi32.dll", "RegSetKeySecurity", 3),
    ST("advapi32.dll", "RegSetKeyValueA", 6),
    ST("advapi32.dll", "RegSetKeyValueW", 6),
    ST("advapi32.dll", "RegSetValueA", 5),
    ST("advapi32.dll", "RegSetValueW", 5),
    ST("advapi32.dll", "RegSetValueExA", 6),
    ST("advapi32.dll", "RegSetValueExW", 6),
    ST("advapi32.dll", "RegUnLoadKeyA", 2),
    ST("advapi32.dll", "RegUnLoadKeyW", 2),
    // ---- advapi32.dll: token / security (delay-imported by Office click-to-run) ----
    ST("advapi32.dll", "OpenProcessToken", 3),
    ST("advapi32.dll", "OpenThreadToken", 4),
    ST("advapi32.dll", "DuplicateToken", 3),
    ST("advapi32.dll", "DuplicateTokenEx", 6),
    ST("advapi32.dll", "GetTokenInformation", 5),
    ST("advapi32.dll", "SetTokenInformation", 4),
    ST("advapi32.dll", "CheckTokenMembership", 3),
    ST("advapi32.dll", "GetLengthSid", 1),
    ST("advapi32.dll", "IsValidSid", 1),
    ST("advapi32.dll", "EqualSid", 2),
    ST("advapi32.dll", "CopySid", 3),
    ST("advapi32.dll", "FreeSid", 1),
    ST("advapi32.dll", "AllocateAndInitializeSid", 11),
    ST("advapi32.dll", "CreateWellKnownSid", 4),
    ST("advapi32.dll", "GetSidSubAuthority", 2),
    ST("advapi32.dll", "GetSidSubAuthorityCount", 1),
    ST("advapi32.dll", "GetSidIdentifierAuthority", 1),
    ST("advapi32.dll", "RevertToSelf", 0),
    ST("advapi32.dll", "ImpersonateNamedPipeClient", 1),
    ST("advapi32.dll", "ImpersonateSelf", 1),
    ST("kernel32.dll", "ResolveDelayLoadedAPI", 6),
    ST("gdi32.dll", "TextOutA", 5),
    ST("advapi32.dll", "CredEnumerateW", 4),
    ST("advapi32.dll", "CredFree", 1),
    ST("advapi32.dll", "CredWriteW", 2),
    // ETW entry points. These are stdcall and must have their argument count
    // here: a delay-loaded import without a signature otherwise cleans up the
    // wrong number of stack bytes and returns into its own callback pointer.
    ST("advapi32.dll", "EventRegister", 4),
    STW("advapi32.dll", "EventUnregister", 2, 1),
    STW("advapi32.dll", "EventWrite", 5, 1),
    STW("advapi32.dll", "EventWriteTransfer", 7, 1),
    STW("advapi32.dll", "EventSetInformation", 5, 1),

    // ---- wine_compat.cpp: credui.dll ----------------------------------------
    ST("credui.dll", "CredEnumerateW", 4),
    ST("credui.dll", "CredFree", 1),
    ST("credui.dll", "CredWriteW", 2),

    // ---- wine_compat.cpp: CRT cdecl (ucrtbase/msvcrt) ----------------------
    CT("ucrtbase.dll", "floor", 1),
    CT("ucrtbase.dll", "isprint", 1),
    CT("ucrtbase.dll", "iswalnum", 1),
    CT("ucrtbase.dll", "qsort", 4),
    CT("ucrtbase.dll", "wcschr", 2),
    CT("ucrtbase.dll", "wcsncmp", 3),
    CT("ucrtbase.dll", "wcstol", 3),
    CT("msvcrt.dll", "floor", 1),
    CT("msvcrt.dll", "isprint", 1),
    CT("msvcrt.dll", "iswalnum", 1),
    CT("msvcrt.dll", "qsort", 4),
    CT("msvcrt.dll", "wcschr", 2),
    CT("msvcrt.dll", "wcsncmp", 3),
    // ---- crypto_runtime.cpp: bcrypt.dll ------------------------------------
    ST("bcrypt.dll", "BCryptOpenAlgorithmProvider", 4),
    ST("bcrypt.dll", "BCryptCloseAlgorithmProvider", 2),
    ST("bcrypt.dll", "BCryptGetProperty", 6),
    ST("bcrypt.dll", "BCryptCreateHash", 7),
    ST("bcrypt.dll", "BCryptHashData", 4),
    ST("bcrypt.dll", "BCryptFinishHash", 4),
    ST("bcrypt.dll", "BCryptDestroyHash", 1),
    ST("bcrypt.dll", "BCryptDestroyKey", 1),
    ST("bcrypt.dll", "BCryptGenRandom", 4),
    ST("bcrypt.dll", "BCryptImportKeyPair", 7),
    ST("bcrypt.dll", "BCryptVerifySignature", 7),
    ST("bcrypt.dll", "BCryptSignHash", 8),
    ST("bcrypt.dll", "BCryptEncrypt", 10),
    ST("bcrypt.dll", "BCryptDecrypt", 10),

    // ---- crypto_runtime.cpp: crypt32.dll ----------------------------------
    ST("crypt32.dll", "CertOpenStore", 5),
    ST("crypt32.dll", "CertOpenSystemStoreW", 2),
    ST("crypt32.dll", "CertCloseStore", 2),
    ST("crypt32.dll", "CertAddEncodedCertificateToStore", 6),
    ST("crypt32.dll", "CertAddStoreToCollection", 4),
    ST("crypt32.dll", "CertFindCertificateInStore", 6),
    ST("crypt32.dll", "CertEnumCertificatesInStore", 2),
    ST("crypt32.dll", "CertGetCertificateContextProperty", 4),
    ST("crypt32.dll", "CertCompareCertificate", 3),
    ST("crypt32.dll", "CertCompareCertificateName", 3),
    ST("crypt32.dll", "CertCompareIntegerBlob", 2),
    ST("crypt32.dll", "CertFindExtension", 3),
    ST("crypt32.dll", "CertGetSubjectCertificateFromStore", 3),
    ST("crypt32.dll", "CertGetCertificateChain", 8),
    ST("crypt32.dll", "CertFreeCertificateChain", 1),
    ST("crypt32.dll", "CertVerifyCertificateChainPolicy", 4),
    ST("crypt32.dll", "CertFreeCertificateContext", 1),
    ST("crypt32.dll", "CertDeleteCertificateFromStore", 1),
    ST("crypt32.dll", "CertDuplicateCertificateContext", 1),
    ST("crypt32.dll", "CertDuplicateStore", 1),
    ST("crypt32.dll", "CertOIDToAlgId", 1),

    // ---- crypto_runtime.cpp: CryptMsg/decode/SIP/wintrust -------------------
    ST("crypt32.dll", "CryptMemAlloc", 1),
    ST("crypt32.dll", "CryptMemFree", 1),
    ST("crypt32.dll", "CryptMsgOpenToDecode", 6),
    ST("crypt32.dll", "CryptMsgUpdate", 4),
    ST("crypt32.dll", "CryptMsgGetParam", 5),
    ST("crypt32.dll", "CryptMsgClose", 1),
    ST("crypt32.dll", "CryptMsgDuplicate", 1),
    ST("crypt32.dll", "CryptMsgControl", 4),
    ST("crypt32.dll", "CryptDecodeObject", 7),
    ST("crypt32.dll", "CryptDecodeObjectEx", 8),
    ST("crypt32.dll", "CryptEnumOIDInfo", 4),
    ST("crypt32.dll", "CryptRegisterOIDFunction", 5),
    ST("crypt32.dll", "CryptSIPRetrieveSubjectGuid", 3),
    ST("crypt32.dll", "CryptSIPLoad", 2),
    ST("crypt32.dll", "CryptSIPAddProvider", 1),
    ST("wintrust.dll", "WinVerifyTrustEx", 3),
    ST("wintrust.dll", "WinVerifyTrust", 3),
    ST("wintrust.dll", "WTHelperGetProvCertFromChain", 2),
    ST("wintrust.dll", "WTHelperGetProvSignerFromChain", 4),
};

#undef ST
#undef STW
#undef CT
#undef CTW

} // namespace

// ---------------------------------------------------------------------------
// Region + lookup
// ---------------------------------------------------------------------------

void init_api_dispatch() {
    if (g_region_mapped) return;
    void* p = mmap(reinterpret_cast<void*>(kThunkBase), kThunkSize,
                   PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (p == MAP_FAILED) {
        fprintf(stderr, "[Thetax86] warning: thunk region mmap failed\n");
        return;
    }
    for (Thunk& t : g_thunks) {
        t = Thunk{};
    }
    g_region_mapped = true;
}

bool is_api_thunk_addr(uint32_t guest_addr) {
    return guest_addr >= kThunkBase &&
           guest_addr < kThunkBase + kThunkSize &&
           ((guest_addr - kThunkBase) % kThunkStride) == 0;
}

const ApiThunk* api_thunk_at(uint32_t guest_addr) {
    if (!is_api_thunk_addr(guest_addr)) return nullptr;
    size_t idx = (guest_addr - kThunkBase) / kThunkStride;
    if (idx >= kThunkCapacity) return nullptr;
    return g_thunks[idx].in_use ? &g_thunks[idx].info : nullptr;
}

// ---------------------------------------------------------------------------
// Signature table lookup
// ---------------------------------------------------------------------------

// Map the api-ms-win-crt-* DLL aliases (and msvcrt.dll) to their canonical
// ucrtbase.dll signatures. The arg-count table is keyed by the canonical name.
static const char* sig_dll_name(const char* dll) {
    if (!dll) return dll;
    if (strncmp(dll, "api-ms-win-crt-", 15) == 0) return "ucrtbase.dll";
    if (strcasecmp(dll, "msvcrt.dll") == 0) return "ucrtbase.dll";
    if (strcasecmp(dll, "vcruntime140.dll") == 0) return "ucrtbase.dll";
    return dll;
}

bool api_signature_for(const char* dll, const char* name, int* argc, bool* is_stdcall) {
    if (!dll || !name) return false;
    const char* canon = sig_dll_name(dll);
    for (const SigEntry& e : kSignatures) {
        if (strcasecmp(e.dll, canon) == 0 && strcmp(e.name, name) == 0) {
            if (argc) *argc = e.argc;
            if (is_stdcall) *is_stdcall = (e.flags & CDECL) == 0;
            return true;
        }
    }
    return false;
}

static const SigEntry* find_sig(const char* dll, const char* name) {
    if (!dll || !name) return nullptr;
    const char* canon = sig_dll_name(dll);
    for (const SigEntry& e : kSignatures) {
        if (strcasecmp(e.dll, canon) == 0 && strcmp(e.name, name) == 0) return &e;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Thunk allocation
// ---------------------------------------------------------------------------

uint32_t alloc_api_thunk(void* fn, int argc, bool stdcall,
                         const char* dll, const char* name) {
    if (!g_region_mapped) init_api_dispatch();
    if (!g_region_mapped) return 0;

    const SigEntry* sig = find_sig(dll, name);
    const char* sig_name = name;
    if (!sig && name && name[0] == '#') {
        // Ordinal reference ("#115"). Resolve to the real export name so the
        // signature table gives us the correct argc. Without this the thunk
        // gets argc=0 and the host function is called with zero args (garbage
        // registers as arguments, e.g. WSAStartup's lpWSAData) and stdcall
        // pops the wrong number of bytes. The resolved name also replaces the
        // caller's temporary "#115" buffer (which would dangle) and lets the
        // dispatch path find the signature again.
        uint16_t ord = static_cast<uint16_t>(strtoul(name + 1, nullptr, 10));
        if (ord) {
            const char* rname = ApiRegistry::instance().name_for_ordinal(dll, ord);
            if (rname) {
                sig = find_sig(dll, rname);
                sig_name = rname;
            }
        }
    }
    if (sig) {
        argc = sig->argc;
        stdcall = (sig->flags & CDECL) == 0;
    }
    const bool known = (sig != nullptr);

    // Deduplicate: every guest process reloads the same images and allocates
    // the same import thunks (the IAT of each fresh child copy must hold a
    // thunk address). The table is a shared 1MB region with no per-process
    // reclaim, so a 32k-thunk cap is exhausted after ~dozens of processes.
    // Only identical (fn, argc, stdcall, known, dll, name) requests may share a
    // slot: generic stubs all use the same host function pointer, and sharing
    // one thunk between them would mislabel every later API in traces (and use
    // the wrong signature for marshalling).
    const char* want_dll = dll ? dll : "";
    const char* want_name = sig_name ? sig_name : "";
    for (size_t i = 0; i < kThunkCapacity; i++) {
        const Thunk& t = g_thunks[i];
        if (!t.in_use) continue;
        if (t.info.fn == fn && t.info.argc == argc &&
            t.info.stdcall == stdcall && t.info.known == known &&
            strcmp(t.dll_buf, want_dll) == 0 &&
            strcmp(t.name_buf, want_name) == 0) {
            return kThunkBase + static_cast<uint32_t>(i * kThunkStride);
        }
    }

    for (size_t i = 0; i < kThunkCapacity; i++) {
        if (g_thunks[i].in_use) continue;
        Thunk& t = g_thunks[i];

        snprintf(t.dll_buf, sizeof(t.dll_buf), "%s", dll ? dll : "");
        snprintf(t.name_buf, sizeof(t.name_buf), "%s", sig_name ? sig_name : "");
        t.info.dll = t.dll_buf;
        t.info.name = t.name_buf;
        t.info.fn = fn;
        t.info.argc = argc;
        t.info.stdcall = stdcall;
        t.info.known = known;

        // Build the ffi cif. Every guest arg is read into a 64-bit value and
        // zero-extended (32-bit guests only push 32-bit words; pointers live
        // below 4GB), so all args are passed as uint64. Using ffi_type_uint32
        // here would write only the low 4 bytes to the argument register,
        // leaving garbage in the upper half of pointer args.
        int nargs = 0;
        for (int slot = 0; slot < argc; ) {
            bool wide = sig && (sig->wide & (1u << slot));
            t.arg_types[nargs++] = &ffi_type_uint64;
            slot += wide ? 2 : 1;
        }
        if (nargs > static_cast<int>(sizeof(t.arg_types) / sizeof(t.arg_types[0]))) {
            nargs = static_cast<int>(sizeof(t.arg_types) / sizeof(t.arg_types[0]));
        }
        // Host WINAPI functions are compiled with the default SysV ABI on Linux
        // (WINAPI_CALL expands to nothing), so the cif must use the default ABI
        // as well. FFI_WIN64 here would place args in the wrong registers.
        if (ffi_prep_cif(&t.cif, FFI_DEFAULT_ABI, nargs, &ffi_type_uint64, t.arg_types) != FFI_OK) {
            t.in_use = false;
            return 0;
        }
        t.in_use = true;
        return kThunkBase + static_cast<uint32_t>(i * kThunkStride);
    }
    fprintf(stderr, "[Thetax86] thunk table exhausted\n");
    return 0;
}

// ---------------------------------------------------------------------------
// Dispatch
// ---------------------------------------------------------------------------

static inline uint32_t guest_mr32(uint32_t a) {
    return *reinterpret_cast<volatile uint32_t*>(static_cast<uintptr_t>(a));
}

// Allocate a thunk for a function resolved at runtime (GetProcAddress). The
// signature table is consulted by name so the marshalling is correct.
uint32_t x86_alloc_proc_thunk(void* fn, const char* name) {
    if (!fn || !name) return 0;
    // Guard against ordinal values passed as fake string pointers.
    if (reinterpret_cast<uintptr_t>(name) < 0x10000) {
        return alloc_api_thunk(fn, 1, true, "", "#ord");
    }
    for (const SigEntry& e : kSignatures) {
        if (strcmp(e.name, name) == 0) {
            return alloc_api_thunk(fn, e.argc, (e.flags & CDECL) == 0, e.dll, name);
        }
    }
    // Unknown signature: assume stdcall with a single pointer arg.
    return alloc_api_thunk(fn, 1, true, "", name);
}

// ---------------------------------------------------------------------------
// 64-bit guest import shims (ms_abi -> SysV trampolines)
// ---------------------------------------------------------------------------
// A 64-bit (PE32+) guest executes natively and calls its imports with the
// Microsoft x64 ABI: integer args in RCX/RDX/R8/R9. Host API functions are
// compiled SysV (RDI/RSI/RDX/RCX). XMM registers pass float args identically
// in both ABIs. Stack args 5+ do NOT match: the guest writes them at
// [rsp+0x20+8k] (ms_abi, before the call -> [rsp+0x28+8k] after the return
// address is pushed) while SysV reads them at [rsp+8+8k], so the shim must
// move them down by 0x20 into the guest's shadow space.
namespace {

constexpr size_t kShimStride = 32;
constexpr size_t kShimPoolSize = 128u << 20; // 128 MB => ~4M shims

uint8_t* g_shim_pool = nullptr;
size_t g_shim_next = 0;

uint8_t* shim_pool() {
    if (g_shim_pool) return g_shim_pool;
    g_shim_pool = static_cast<uint8_t*>(
        mmap(nullptr, kShimPoolSize, PROT_READ | PROT_WRITE | PROT_EXEC,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if (g_shim_pool == MAP_FAILED) g_shim_pool = nullptr;
    return g_shim_pool;
}

} // namespace

// Emit `mov r/m64, r64`-style memory ops against [rsp + disp32] that survive a
// SysV call with the ms_abi register discipline. Helpers below write raw bytes
// so the generated trampoline never touches a compiler-generated frame.
static uint8_t* emit_mov_reg_to_mem(uint8_t* w, uint8_t reg, uint32_t disp) {
    // 48 89 <modrm(reg, mod=10, rm=100)> 24 <disp32>  ; mov [rsp+disp], r<reg>
    *w++ = 0x48;
    *w++ = 0x89;
    *w++ = static_cast<uint8_t>(0x80 | (reg << 3) | 0x04);
    *w++ = 0x24;
    memcpy(w, &disp, 4);
    return w + 4;
}

static uint8_t* emit_mov_mem_to_reg(uint8_t* w, uint8_t reg, uint32_t disp) {
    // 48 8B <modrm> 24 <disp32>  ; mov r<reg>, [rsp+disp]
    *w++ = 0x48;
    *w++ = 0x8B;
    *w++ = static_cast<uint8_t>(0x80 | (reg << 3) | 0x04);
    *w++ = 0x24;
    memcpy(w, &disp, 4);
    return w + 4;
}

// movdqu [rsp+disp], xmmN  (F3 0F 7F) and movdqu xmmN, [rsp+disp] (F3 0F 6F).
static uint8_t* emit_movdqu_mem_xmm(uint8_t* w, unsigned xmm_idx, uint32_t disp) {
    if (xmm_idx >= 8) { *w++ = 0x44; } // REX.R for xmm8-15
    *w++ = 0xF3; *w++ = 0x0F; *w++ = 0x7F;
    *w++ = static_cast<uint8_t>(0x80 | ((xmm_idx & 7) << 3) | 0x04);
    *w++ = 0x24;
    memcpy(w, &disp, 4);
    return w + 4;
}

static uint8_t* emit_movdqu_xmm_mem(uint8_t* w, unsigned xmm_idx, uint32_t disp) {
    if (xmm_idx >= 8) { *w++ = 0x44; }
    *w++ = 0xF3; *w++ = 0x0F; *w++ = 0x6F;
    *w++ = static_cast<uint8_t>(0x80 | ((xmm_idx & 7) << 3) | 0x04);
    *w++ = 0x24;
    memcpy(w, &disp, 4);
    return w + 4;
}

// mov r8/r9, [rsp+disp32] for guest args 5/6 (REX.W|REX.R variants).
static uint8_t* emit_mov_r8_rsp(uint8_t* w, uint32_t disp) {
    *w++ = 0x4C; *w++ = 0x8B; *w++ = 0x84; *w++ = 0x24;
    memcpy(w, &disp, 4);
    return w + 4;
}

static uint8_t* emit_mov_r9_rsp(uint8_t* w, uint32_t disp) {
    *w++ = 0x4C; *w++ = 0x8B; *w++ = 0x8C; *w++ = 0x24;
    memcpy(w, &disp, 4);
    return w + 4;
}

// mov rax, [rsp+disp32] / mov [rsp+disp32], rax (for stack arg relocation).
static uint8_t* emit_mov_rax_rsp(uint8_t* w, uint32_t disp) {
    *w++ = 0x48; *w++ = 0x8B; *w++ = 0x84; *w++ = 0x24;
    memcpy(w, &disp, 4);
    return w + 4;
}

static uint8_t* emit_mov_rsp_rax(uint8_t* w, uint32_t disp) {
    *w++ = 0x48; *w++ = 0x89; *w++ = 0x84; *w++ = 0x24;
    memcpy(w, &disp, 4);
    return w + 4;
}

uint64_t x86_alloc64_shim(void* fn, const char* dll, const char* name) {
    if (!fn) return 0;
    uint8_t* pool = shim_pool();
    if (!pool) return 0;

    // 64-bit arg count, looked up from the signature table when a name is
    // available. The table stores 32-bit guest STACK SLOT counts; the 64-bit
    // count is argc minus the number of wide (8-byte) slots.
    int argc64 = 0;
    if (dll && name) {
        const char* canon = sig_dll_name(dll);
        for (const SigEntry& e : kSignatures) {
            if (strcasecmp(e.dll, canon) == 0 && strcmp(e.name, name) == 0) {
                argc64 = e.argc - __builtin_popcount(e.wide);
                break;
            }
        }
        if (getenv("THETAZ_DEBUG_SHIM") && (strstr(name, "RegSet") || strstr(name, "RegQuery"))) {
            fprintf(stderr, "[shim] %s!%s -> argc64=%d (canon=%s)\n",
                    dll, name, argc64, canon ? canon : "?");
        }
    }

    // Number of stack arguments the guest passes beyond the first 4 register
    // args. Guest args 5-6 go to host r8/r9; guest args 7+ go to host stack.
    size_t n_stack = 0;
    if (argc64 > 6) {
        n_stack = static_cast<size_t>(argc64 - 6);
    }

    // Frame layout. E = guest rsp at shim entry (E % 16 == 8). R = E - frame.
    // SysV fn runs with rsp == R (we `call`, so fn entry is R-8, %16 == 8).
    //   [R + 0x00 .. R + 8*n_stack)  guest stack args 7+ (SysV [fn_rsp+8+8k])
    //   [R + 8n .. R + 8n + 8)       saved guest rsi
    //   [R + 8n + 8 .. R + 8n + 0x10) saved guest rdi
    //   [R + 8n + 0x10 .. + 0xB0)    saved guest xmm6-15 (10 * 16 bytes)
    // frame must satisfy frame % 16 == 8 so that R % 16 == 0 before the call.
    size_t frame = 8 * n_stack + 0xB0;
    if ((frame & 0xF) == 0) frame += 8;

    // Exact emitted byte counts (matching the emission helpers below):
    //   sub rsp, frame        7
    //   save rsi/rdi          16
    //   xmm6-15 save          2*9 + 8*10 = 98   (xmm6-7 no REX, xmm8-15 REX.R)
    //   register moves        12
    //   arg5/arg6 moves       8 each
    //   stack arg relocation  16 per stack arg
    //   movabs + call rax     12
    //   xmm6-15 restore       98
    //   restore rsi/rdi       16
    //   add rsp, frame + ret  8
    size_t size = 7 + 16 + 98 + 12 +
                  (argc64 >= 5 ? 8 : 0) + (argc64 >= 6 ? 8 : 0) +
                  n_stack * 16 +
                  12 + 98 + 16 + 8;
    size_t stride = size <= kShimStride ? kShimStride : ((size + 15) & ~size_t(15));
    if (g_shim_next + stride > kShimPoolSize) return 0;

    uint8_t* p = pool + g_shim_next;
    g_shim_next += stride;

    uint8_t* w = p;

    // sub rsp, frame  (48 81 EC imm32) — carve the whole frame below the guest
    // rsp. All further [rsp+off] operands are then relative to R = E - frame.
    *w++ = 0x48; *w++ = 0x81; *w++ = 0xEC;
    const uint32_t f32 = static_cast<uint32_t>(frame);
    memcpy(w, &f32, 4); w += 4;

    // Save guest non-volatile registers the SysV callee will clobber.
    const uint32_t save_rsi = static_cast<uint32_t>(8 * n_stack);
    const uint32_t save_rdi = static_cast<uint32_t>(8 * n_stack + 8);
    const uint32_t xmm_base = static_cast<uint32_t>(8 * n_stack + 0x10);
    w = emit_mov_reg_to_mem(w, 6, save_rsi); // mov [rsp+..], rsi
    w = emit_mov_reg_to_mem(w, 7, save_rdi); // mov [rsp+..], rdi
    for (unsigned i = 0; i < 10; i++) {
        w = emit_movdqu_mem_xmm(w, 6 + i, xmm_base + 16u * i);
    }

    // Register argument translation: ms_abi rcx/rdx/r8/r9 -> SysV rdi/rsi/rdx/rcx.
    const uint8_t moves[4][3] = {
        {0x48, 0x89, 0xCF}, // mov rdi, rcx
        {0x48, 0x89, 0xD6}, // mov rsi, rdx
        {0x4C, 0x89, 0xC2}, // mov rdx, r8
        {0x4C, 0x89, 0xC9}, // mov rcx, r9
    };
    for (const auto& m : moves) {
        *w++ = m[0]; *w++ = m[1]; *w++ = m[2];
    }

    // Guest args 5/6 -> host r8/r9. Guest wrote them at [E+0x28]/[E+0x30].
    if (argc64 >= 5) {
        w = emit_mov_r8_rsp(w, static_cast<uint32_t>(frame + 0x28));
    }
    if (argc64 >= 6) {
        w = emit_mov_r9_rsp(w, static_cast<uint32_t>(frame + 0x30));
    }
    // Guest stack args 7+ at [E+0x38+8k] -> SysV slots [R+8k]. The R frame has
    // no shadow space, so a SysV arg7 is exactly at [R+8]. Copy ascending: the
    // source addresses lie strictly above all destinations.
    for (size_t k = 0; k < n_stack; k++) {
        w = emit_mov_rax_rsp(w, static_cast<uint32_t>(frame + 0x38 + 8u * k));
        w = emit_mov_rsp_rax(w, static_cast<uint32_t>(8u * k));
    }

    // mov rax, imm64 ; call rax  (pushes the return address; fn entry rsp = R-8).
    *w++ = 0x48;
    *w++ = 0xB8;
    *reinterpret_cast<uint64_t*>(w) = reinterpret_cast<uint64_t>(fn);
    w += 8;
    *w++ = 0xFF;
    *w++ = 0xD0;

    // Restore the guest's registers (rax holds the SysV return value).
    for (unsigned i = 0; i < 10; i++) {
        w = emit_movdqu_xmm_mem(w, 6 + i, xmm_base + 16u * i);
    }
    w = emit_mov_mem_to_reg(w, 6, save_rsi); // mov rsi, [rsp+..]
    w = emit_mov_mem_to_reg(w, 7, save_rdi); // mov rdi, [rsp+..]
    // add rsp, frame ; ret
    *w++ = 0x48; *w++ = 0x81; *w++ = 0xC4;
    memcpy(w, &f32, 4); w += 4;
    *w++ = 0xC3;

    return reinterpret_cast<uint64_t>(p);
}

// True while a 32-bit interpreted API dispatch is in flight. Lets the shared
// Win32 API layer return guest-friendly values (e.g. GetProcAddress must hand a
// thunk address instead of a raw host pointer, which would truncate below 4GB).
static thread_local bool g_x86_dispatch_active = false;

bool x86_dispatch_active() {
    return g_x86_dispatch_active;
}

uint32_t x86_dispatch_api(X86CpuState& s, uint32_t thunk_addr, uint32_t ret_addr) {
    if (!g_region_mapped) init_api_dispatch();
    size_t idx = (thunk_addr - kThunkBase) / kThunkStride;
    if (idx >= kThunkCapacity || !g_thunks[idx].in_use) {
        fprintf(stderr, "[Thetax86] dispatch to non-thunk 0x%08x\n", thunk_addr);
        return ret_addr;
    }
    Thunk& t = g_thunks[idx];
    const SigEntry* sig = find_sig(t.info.dll, t.info.name);

    if (!t.info.known) {
        // No signature-table entry: assume a plain stdcall function with
        // `t.info.argc` 32-bit args (GetProcAddress-resolved runtime bindings).
        // The CIF was built for argc args, so read that many from the stack.
        uint32_t esp = s.regs[XR_ESP];
        ffi_arg values[24];
        void* avalues[24];
        int nargs = 0;
        for (int slot = 0; slot < t.info.argc && nargs < 24; slot++, nargs++) {
            values[nargs] = guest_mr32(esp + 4 * slot);
            avalues[nargs] = &values[nargs];
        }
        ffi_arg rvalue = 0;
        ffi_call(&t.cif, reinterpret_cast<void (*)(void)>(t.info.fn), &rvalue, avalues);
        // A guest SEH handler may have claimed an exception raised by this API
        // (unknown-signature thunks can raise too); resume where it decided.
        X86SehResume& seh = x86_seh_resume_state();
        if (seh.active) {
            s.eip = seh.eip;
            s.regs[XR_ESP] = seh.esp;
            s.regs[XR_EBP] = seh.ebp;
            s.regs[XR_EDX] = seh.edx;
            seh.active = false;
            return s.eip;
        }
        s.regs[XR_EAX] = static_cast<uint32_t>(rvalue);
        s.regs[XR_EDX] = static_cast<uint32_t>(rvalue >> 32);
        if (t.info.stdcall) s.regs[XR_ESP] += 4 * t.info.argc;
        s.eip = ret_addr;
        return ret_addr;
    }

    // Read guest args from the stack. The interpreter did not push a return
    // address, so the first arg is at [esp].
    uint32_t esp = s.regs[XR_ESP];
    ffi_arg values[24];
    void* avalues[24];
    int nargs = 0;
    for (int slot = 0; slot < t.info.argc; ) {
        bool wide = (sig->wide & (1u << slot)) != 0;
        uint64_t v;
        if (wide) {
            v = static_cast<uint64_t>(guest_mr32(esp + 4 * slot)) |
                (static_cast<uint64_t>(guest_mr32(esp + 4 * slot + 4)) << 32);
            slot += 2;
        } else {
            v = guest_mr32(esp + 4 * slot);
            slot += 1;
        }
        values[nargs] = v;
        avalues[nargs] = &values[nargs];
        nargs++;
    }

    ffi_arg rvalue = 0;
    g_x86_dispatch_active = true;
    set_guest_vararg_slot(4);
    ffi_call(&t.cif, reinterpret_cast<void (*)(void)>(t.info.fn), &rvalue, avalues);
    set_guest_vararg_slot(8);
    g_x86_dispatch_active = false;

    // A guest SEH handler may have claimed an exception thrown by an API
    // (RaiseException / C++ throw) and rewritten the context; resume there
    // instead of the instruction after the API call.
    X86SehResume& seh = x86_seh_resume_state();
    if (seh.active) {
        s.eip = seh.eip;
        s.regs[XR_ESP] = seh.esp;
        s.regs[XR_EBP] = seh.ebp;
        s.regs[XR_EDX] = seh.edx;
        seh.active = false;
        return s.eip;
    }
    if (getenv("THETAZ_TRACE_API")) {
        fprintf(stderr, "[api] %s!%s argc=%d args=[",
                t.info.dll ? t.info.dll : "?", t.info.name ? t.info.name : "?",
                t.info.argc);
        for (int slot = 0; slot < t.info.argc && slot < 8; ) {
            bool wide = (sig->wide & (1u << slot)) != 0;
            fprintf(stderr, "%s0x%llx", slot ? "," : "",
                    wide ? (static_cast<unsigned long long>(guest_mr32(esp + 4 * slot)) |
                           (static_cast<unsigned long long>(guest_mr32(esp + 4 * slot + 4)) << 32))
                         : static_cast<unsigned long long>(guest_mr32(esp + 4 * slot)));
            slot += wide ? 2 : 1;
        }
        fprintf(stderr, "] -> 0x%llx  (caller=0x%x)\n", static_cast<unsigned long long>(rvalue), s.eip);
    }

    s.regs[XR_EAX] = static_cast<uint32_t>(rvalue);
    s.regs[XR_EDX] = static_cast<uint32_t>(rvalue >> 32);
    if (t.info.stdcall) s.regs[XR_ESP] += 4 * t.info.argc;
    s.eip = ret_addr;
    return ret_addr;
}

} // namespace ThetaZ
