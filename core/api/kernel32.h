// ThetaZ — Windows kernel32 API declarations for real Windows app compatibility
// Copyright (c) 2026 ThetaZ Project. MIT License.

#pragma once

#include "core/api/api.h"
#include "core/api/path_resolver.h"

#include <cstdint>
#include <cstddef>

namespace ThetaZ {

// ----------------------------------------------------------------------
// Extended Windows types (api.h provides DWORD/BOOL/.../WINAPI_CALL)
// ----------------------------------------------------------------------
using HANDLE = void*;
using HKEY = void*;
using ULONG = uint32_t;
using USHORT = uint16_t;
using INT = int;
using PVOID = void*;
using VOID = void;
using LPDWORD = DWORD*;
using PDWORD = DWORD*;
using PBOOL = BOOL*;
using LPBOOL = BOOL*;
using PULONG = ULONG*;
using ULONG_PTR = uint64_t;
using LPOVERLAPPED = void*;
using LPSECURITY_ATTRIBUTES = void*;
using PHKEY = HKEY*;
using LPBYTE = BYTE*;

// Write a handle/pointer-sized value into guest memory using the guest's
// pointer width. Host `void*` is 8 bytes, but 32-bit guests have 4-byte
// handles; writing sizeof(void*) there corrupts the following guest word.
inline void guest_write_handle(void* p, uintptr_t value) {
    if (!p) return;
    if (guest_is_32bit())
        *reinterpret_cast<uint32_t*>(p) = static_cast<uint32_t>(value);
    else
        *reinterpret_cast<uintptr_t*>(p) = value;
}
using SIZE_T = size_t;
using WORD = uint16_t;
using HFILE = int;

// Calendar/NLS types (used by EnumCalendarInfo, GetCalendarInfo, GetGeoInfo)
using CALID = uint32_t;
using CALINFO = uint32_t;
using LID = uint32_t;

// OLE/COM types
using HRESULT = long;
#define S_OK                0x00000000L
#define S_FALSE             0x00000001L
#define E_FAIL              0x80004005L
#define E_OUTOFMEMORY       0x8007000EL
#define E_NOTIMPL           0x80004001L
#define E_INVALIDARG        0x80070057L
#define E_NOINTERFACE       0x80004002L
#define FNM_CASEFOLD        0x00000010
#define GCT_LFNCHAR         0x00000001
#define GCT_INVALID         0x00000000
#define HEAP_ZERO_MEMORY      0x00000008
#define REGDB_E_CLASSNOTREG 0x80040154L
#define CO_E_CLASSSTRING    0x800401F3L
#define RPC_E_CHANGED_MODE  0x80010106L
typedef struct _GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[8];
} GUID;

using REFGUID = const GUID&;
using REFCLSID = const GUID&;
using REFIID = const GUID&;

using LPOLESTR = wchar_t*;
using LPCOLESTR = const wchar_t*;
using LPCLSID = GUID*;
using LPUNKNOWN = void*;
using LPVOID = void*;
using PSECURITY_DESCRIPTOR = void*;
using PACL = void*;
typedef struct {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME;

struct STARTUPINFOA {
    DWORD cb;
    LPSTR lpReserved;
    LPSTR lpDesktop;
    LPSTR lpTitle;
    DWORD dwX;
    DWORD dwY;
    DWORD dwXSize;
    DWORD dwYSize;
    DWORD dwXCountChars;
    DWORD dwYCountChars;
    DWORD dwFillAttribute;
    DWORD dwFlags;
    WORD wShowWindow;
    WORD cbReserved2;
    LPBYTE lpReserved2;
    HANDLE hStdInput;
    HANDLE hStdOutput;
    HANDLE hStdError;
};

struct PROCESS_INFORMATION {
    HANDLE hProcess;
    HANDLE hThread;
    DWORD dwProcessId;
    DWORD dwThreadId;
};

struct STARTUPINFOW {
    DWORD cb;
    LPWSTR lpReserved;
    LPWSTR lpDesktop;
    LPWSTR lpTitle;
    DWORD dwX;
    DWORD dwY;
    DWORD dwXSize;
    DWORD dwYSize;
    DWORD dwXCountChars;
    DWORD dwYCountChars;
    DWORD dwFillAttribute;
    DWORD dwFlags;
    WORD wShowWindow;
    WORD cbReserved2;
    LPBYTE lpReserved2;
    HANDLE hStdInput;
    HANDLE hStdOutput;
    HANDLE hStdError;
};

using LPFILETIME = FILETIME*;

using LPSTARTUPINFOA = STARTUPINFOA*;
using LPPROCESS_INFORMATION = PROCESS_INFORMATION*;
using LPSTARTUPINFOW = STARTUPINFOW*;
using REGSAM = uint32_t;

// ----------------------------------------------------------------------
// Registry error codes
// ----------------------------------------------------------------------
#define ERROR_SUCCESS 0
#define ERROR_INVALID_HANDLE 6
#define ERROR_CALL_NOT_IMPLEMENTED 120
#define ERROR_INVALID_PARAMETER 87
#define ERROR_MOD_NOT_FOUND 126
#define ERROR_PROC_NOT_FOUND 127
#define INVALID_HANDLE_VALUE ((HANDLE)-1)
#define ERROR_INSUFFICIENT_BUFFER 122
#define ERROR_OUTOFMEMORY 14
#define ERROR_ACCESS_DENIED 5
#define ERROR_FILE_NOT_FOUND 2
#define FILE_TYPE_DISK 1
#define ERROR_NO_MORE_ITEMS 259
#define ERROR_PATH_NOT_FOUND 3
#define ERROR_TOO_MANY_OPEN_FILES 4
#define ERROR_FILE_EXISTS 80
#define ERROR_DISK_FULL 112
#define ERROR_ALREADY_EXISTS 183
#define ERROR_NO_TOKEN 1008
#define ERROR_NOT_ENOUGH_MEMORY 8
#define ERROR_FILENAME_EXCED_RANGE 206

using LSTATUS = long;

#define REG_SZ 1
#define REG_DWORD 4
#define REG_BINARY 3
#define REG_CREATED_NEW_KEY 1

// ----------------------------------------------------------------------
// Memory information (constants defined in ntdll/virtual.h)
// ----------------------------------------------------------------------

struct MEMORY_BASIC_INFORMATION {
    void* BaseAddress;
    void* AllocationBase;
    uint32_t AllocationProtect;
    size_t RegionSize;
    uint32_t State;
    uint32_t Protect;
    uint32_t Type;
};\
using PMEMORY_BASIC_INFORMATION = MEMORY_BASIC_INFORMATION*;

// Number of positional parameters in ExceptionInformation[]
constexpr int EXCEPTION_MAXIMUM_NESTED_LEVELS = 15;

struct EXCEPTION_RECORD {
    uint32_t ExceptionCode;
    uint32_t ExceptionFlags;
    EXCEPTION_RECORD* ExceptionRecord;
    uint64_t ExceptionAddress;
    uint32_t NumberParameters;
    uint64_t ExceptionInformation[EXCEPTION_MAXIMUM_NESTED_LEVELS];
};

struct CONTEXT64 {
    uint64_t P1Home;
    uint64_t P2Home;
    uint64_t P3Home;
    uint64_t P4Home;
    uint64_t P5Home;
    uint64_t P6Home;
    uint32_t ContextFlags;
    uint32_t MxCsr;
    uint16_t SegCs;
    uint16_t SegDs;
    uint16_t SegEs;
    uint16_t SegFs;
    uint16_t SegGs;
    uint16_t SegSs;
    uint32_t EFlags;
    uint32_t Dr0;
    uint32_t Dr1;
    uint32_t Dr2;
    uint32_t Dr3;
    uint32_t Dr6;
    uint32_t Dr7;
    uint32_t Eax;
    uint32_t Ecx;
    uint32_t Edx;
    uint32_t Ebx;
    uint32_t Esp;
    uint32_t Ebp;
    uint32_t Esi;
    uint32_t Edi;
    uint32_t Eip;
    uint32_t SegCs2;
    uint32_t SegDs2;
    uint32_t SegEs2;
    uint32_t SegFs2;
    uint32_t SegGs2;
    uint32_t SegSs2;
    uint32_t EFllags;
    uint8_t FloatSave[112];
    uint8_t ExtendedRegisters[512];
};

struct EXCEPTION_POINTERS {
    EXCEPTION_RECORD* ExceptionRecord;
    CONTEXT64* ContextRecord;
};

typedef LONG (*PVECTORED_EXCEPTION_HANDLER)(EXCEPTION_POINTERS* ExceptionInfo);

// ----------------------------------------------------------------------
// Process / thread info classes
// ----------------------------------------------------------------------
typedef enum PROCESSINFOCLASS {
    ProcessBasicInformation = 0,
    ProcessIoCounters = 1,
    ProcessVmCounters = 2,
    ProcessImageFileName = 3,
    ProcessWow64Information = 5,
    ProcessMaxInformationClass = 0x1F
} PROCESSINFOCLASS;

typedef enum THREADINFOCLASS {
    ThreadBasicInformation = 0,
    ThreadTimes = 1,
    ThreadPriority = 2,
    ThreadAffinityMask = 3,
    ThreadInfoMask = 4,
    ThreadMaxInformationClass = 0x1F
} THREADINFOCLASS;

// ----------------------------------------------------------------------
// Synchronization primitives
// ----------------------------------------------------------------------
typedef struct _CONDITION_VARIABLE {
    uint64_t reserved;
} CONDITION_VARIABLE;
using PCONDITION_VARIABLE = CONDITION_VARIABLE*;

typedef struct _CRITICAL_SECTION {
    int reserved;
    int lock_count;
    int recursion_count;
    uint64_t OwningThread;
    uint64_t LockSemaphore;
    void* SpinCount;
} CRITICAL_SECTION;
using PCRITICAL_SECTION = CRITICAL_SECTION*;

typedef struct _SRWLOCK {
    long lock_state;
} SRWLOCK;
using PSRWLOCK = SRWLOCK*;

// ----------------------------------------------------------------------
// Timing
// ----------------------------------------------------------------------
typedef struct _LARGE_INTEGER {
    int64_t QuadPart;
} LARGE_INTEGER;

// ----------------------------------------------------------------------
// Console
// ----------------------------------------------------------------------
typedef struct _CONSOLE_READCONSOLE_CONTROL {
    DWORD nLength;
    DWORD nInitialFillChar;
    WORD bFollowedByEcho;
    DWORD nCtrlBreakSignal;
} CONSOLE_READCONSOLE_CONTROL;
using PCONSOLE_READCONSOLE_CONTROL = CONSOLE_READCONSOLE_CONTROL*;

// ----------------------------------------------------------------------
// WOW64 thread context
// ----------------------------------------------------------------------
struct WOW64CONTEXT {
    uint64_t P1Home;
    uint64_t P2Home;
    uint64_t P3Home;
    uint64_t P4Home;
    uint64_t P5Home;
    uint64_t P6Home;
    uint32_t ContextFlags;
    uint32_t MxCsr;
    uint32_t SegCs;
    uint32_t SegDs;
    uint32_t SegEs;
    uint32_t SegFs;
    uint32_t SegGs;
    uint32_t SegSs;
    uint32_t EFlags;
    uint32_t Dr0;
    uint32_t Dr1;
    uint32_t Dr2;
    uint32_t Dr3;
    uint32_t Dr6;
    uint32_t Dr7;
    uint32_t Eax;
    uint32_t Ecx;
    uint32_t Edx;
    uint32_t Ebx;
    uint32_t Esp;
    uint32_t Ebp;
    uint32_t Esi;
    uint32_t Edi;
    uint32_t Eip;
    uint32_t SegCs2;
    uint32_t SegDs2;
    uint32_t SegEs2;
    uint32_t SegFs2;
    uint32_t SegGs2;
    uint32_t SegSs2;
    uint32_t EFllags;
    uint8_t FloatSave[112];
    uint8_t ExtendedRegisters[512];
};

// ----------------------------------------------------------------------
// Thread context API (thread_api.cpp)
// ----------------------------------------------------------------------
BOOL WINAPI_CALL THETAZ_Wow64GetThreadContext(HANDLE hThread, WOW64CONTEXT* ctx);
BOOL WINAPI_CALL THETAZ_SetThreadContext(HANDLE hThread, WOW64CONTEXT* ctx);
BOOL WINAPI_CALL THETAZ_SuspendThread(HANDLE hThread);
BOOL WINAPI_CALL THETAZ_ResumeThread(HANDLE hThread);

// Thread function pointer typedef
typedef void* (*LPTHREAD_START_ROUTINE)(void*);

HANDLE WINAPI_CALL THETAZ_CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize, LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId);
typedef void* (*LPTHREAD_START_ROUTINE)(void*);
DWORD WINAPI_CALL THETAZ_GetThreadId(HANDLE hThread);

// ----------------------------------------------------------------------
// Vectored exception handling API (exception.cpp)
// ----------------------------------------------------------------------
PVOID WINAPI_CALL THETAZ_AddVectoredExceptionHandler(ULONG first, PVECTORED_EXCEPTION_HANDLER handler);
PVOID WINAPI_CALL THETAZ_AddVectoredContinueHandler(ULONG first, PVECTORED_EXCEPTION_HANDLER handler);
BOOL WINAPI_CALL THETAZ_RemoveVectoredExceptionHandler(PVOID handle);
BOOL WINAPI_CALL THETAZ_RemoveVectoredContinueHandler(PVOID handle);

// ----------------------------------------------------------------------
// Process information API (kernel32.cpp)
// ----------------------------------------------------------------------
LONG WINAPI_CALL THETAZ_NtQueryInformationProcess(HANDLE processHandle, PROCESSINFOCLASS processInformationClass,
                                                   PVOID processInformation, ULONG processInformationLength,
                                                   PULONG returnLength);
LONG WINAPI_CALL THETAZ_NtQueryInformationThread(HANDLE threadHandle, THREADINFOCLASS threadInformationClass,
                                                  PVOID threadInformation, ULONG threadInformationLength,
                                                  PULONG returnLength);
DWORD WINAPI_CALL THETAZ_GetProcessId(HANDLE hProcess);

// ----------------------------------------------------------------------
// Memory API (kernel32.cpp)
// ----------------------------------------------------------------------
LPVOID WINAPI_CALL THETAZ_VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect);
BOOL WINAPI_CALL THETAZ_VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType);
SIZE_T WINAPI_CALL THETAZ_VirtualQuery(LPCVOID lpAddress, PMEMORY_BASIC_INFORMATION lpBuffer, SIZE_T dwLength);
BOOL WINAPI_CALL THETAZ_VirtualProtect(LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, PDWORD lpflOldProtect);
SIZE_T WINAPI_CALL THETAZ_VirtualQueryEx(HANDLE hProcess, LPCVOID lpAddress, PMEMORY_BASIC_INFORMATION lpBuffer, SIZE_T dwLength);
BOOL WINAPI_CALL THETAZ_VirtualProtectEx(HANDLE hProcess, LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, PDWORD lpflOldProtect);
BOOL WINAPI_CALL THETAZ_WriteProcessMemory(HANDLE hProcess, LPVOID lpBaseAddress, LPCVOID lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesWritten);
BOOL WINAPI_CALL THETAZ_ReadProcessMemory(HANDLE hProcess, LPCVOID lpBaseAddress, LPVOID lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesRead);

// ----------------------------------------------------------------------
// String conversion API (kernel32.cpp)
// ----------------------------------------------------------------------
INT WINAPI_CALL THETAZ_MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr, int cbMultiByte,
                                           LPWSTR lpWideCharStr, int cchWideChar);
BOOL WINAPI_CALL THETAZ_WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr, int cchWideChar,
                                             LPSTR lpMultiByteStr, int cbMultiByte, LPCSTR lpDefaultChar, LPBOOL lpUsedDefaultChar);

// ----------------------------------------------------------------------
// Debug API (kernel32.cpp)
// ----------------------------------------------------------------------
BOOL WINAPI_CALL THETAZ_IsDebuggerPresent();
BOOL WINAPI_CALL THETAZ_CheckRemoteDebuggerPresent(HANDLE hProcess, PBOOL pbDebuggerPresent);

// ----------------------------------------------------------------------
// Condition variable API (kernel32.cpp)
// ----------------------------------------------------------------------
void WINAPI_CALL THETAZ_InitConditionVariable(PCONDITION_VARIABLE ConditionVariable);
void WINAPI_CALL THETAZ_DeleteConditionVariable(PCONDITION_VARIABLE ConditionVariable);
BOOL WINAPI_CALL THETAZ_SleepConditionVariableCS(PCONDITION_VARIABLE ConditionVariable, PCRITICAL_SECTION CriticalSection, DWORD dwMilliseconds);
BOOL WINAPI_CALL THETAZ_SleepConditionVariableSRW(void* ConditionVariable, void* SRWLock, DWORD dwMilliseconds, ULONG Flags);
void WINAPI_CALL THETAZ_WakeAllConditionVariable(void* ConditionVariable);
void WINAPI_CALL THETAZ_WakeConditionVariable(void* ConditionVariable);

// ----------------------------------------------------------------------
// Timing API (kernel32.cpp)
// ----------------------------------------------------------------------
DWORD WINAPI_CALL THETAZ_GetTickCount();
ULONGLONG WINAPI_CALL THETAZ_GetTickCount64();
BOOL WINAPI_CALL THETAZ_QueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount);
BOOL WINAPI_CALL THETAZ_QueryPerformanceFrequency(LARGE_INTEGER* lpFrequency);

// ----------------------------------------------------------------------
// Console / file I/O API (kernel32.cpp)
// ----------------------------------------------------------------------
BOOL WINAPI_CALL THETAZ_WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
                                   LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped);
BOOL WINAPI_CALL THETAZ_ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
                                  LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped);
BOOL WINAPI_CALL THETAZ_WriteConsoleW(HANDLE hConsoleOutput, const void* lpBuffer, DWORD nNumberOfCharsToWrite,
                                       LPDWORD lpNumberOfCharsWritten, PCONSOLE_READCONSOLE_CONTROL lpConsoleReadControl);
BOOL WINAPI_CALL THETAZ_ReadConsoleW(HANDLE hConsoleInput, LPVOID lpBuffer, DWORD nNumberOfCharsToRead,
                                      LPDWORD lpNumberOfCharsRead, PCONSOLE_READCONSOLE_CONTROL lpConsoleReadControl);

// ----------------------------------------------------------------------
// Named pipe API (kernel32.cpp)
// ----------------------------------------------------------------------
BOOL WINAPI_CALL THETAZ_WaitNamedPipeW(LPCWSTR lpNamedPipeName, DWORD nTimeout);
BOOL WINAPI_CALL THETAZ_WaitNamedPipeA(LPCSTR lpNamedPipeName, DWORD nTimeout);
BOOL WINAPI_CALL THETAZ_CreateNamedPipeW(LPCWSTR lpName, DWORD dwOpenMode, DWORD dwPipeMode, DWORD nMaxInstances,
                                          DWORD nOutBufferSize, DWORD nInBufferSize, DWORD nDefaultTimeOut,
                                          LPSECURITY_ATTRIBUTES lpSecurityAttributes);
BOOL WINAPI_CALL THETAZ_CreateNamedPipeA(LPCSTR lpName, DWORD dwOpenMode, DWORD dwPipeMode, DWORD nMaxInstances,
                                          DWORD nOutBufferSize, DWORD nInBufferSize, DWORD nDefaultTimeOut,
                                          LPSECURITY_ATTRIBUTES lpSecurityAttributes);
BOOL WINAPI_CALL THETAZ_SetNamedPipeHandleState(HANDLE hNamedPipe, LPDWORD lpMode, LPDWORD lpMaxCollectionCount, LPDWORD lpCollectionDataSize);
BOOL WINAPI_CALL THETAZ_GetNamedPipeInfo(HANDLE hNamedPipe, LPDWORD lpFlags, LPDWORD lpOutBufferSize, LPDWORD lpInBufferSize, LPDWORD lpNumberOfInstances);
BOOL WINAPI_CALL THETAZ_CallNamedPipeW(LPCWSTR lpNamedPipeName, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesRead, DWORD nTimeout);
BOOL WINAPI_CALL THETAZ_CallNamedPipeA(LPCSTR lpNamedPipeName, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesRead, DWORD nTimeout);

// ----------------------------------------------------------------------
// Layer initialization
// ----------------------------------------------------------------------
// Environment and PathResolver initialization
void initialize_environment();
void initialize_path_resolver();
PathResolver& get_path_resolver();
void initialize_kernel32_api();

// ----------------------------------------------------------------------
// Additional kernel32 APIs implemented in kernel32.cpp (CRT + app support)
// ----------------------------------------------------------------------
DWORD WINAPI_CALL THETAZ_Sleep(DWORD dwMilliseconds);
DWORD WINAPI_CALL THETAZ_GetLastError();
void WINAPI_CALL THETAZ_SetLastError(DWORD dwErrCode);

DWORD WINAPI_CALL THETAZ_TlsAlloc();
BOOL WINAPI_CALL THETAZ_TlsFree(DWORD dwTlsIndex);
LPVOID WINAPI_CALL THETAZ_TlsGetValue(DWORD dwTlsIndex);
BOOL WINAPI_CALL THETAZ_TlsSetValue(DWORD dwTlsIndex, LPVOID lpTlsValue);

void WINAPI_CALL THETAZ_InitializeCriticalSection(PCRITICAL_SECTION lpCriticalSection);
void WINAPI_CALL THETAZ_DeleteCriticalSection(PCRITICAL_SECTION lpCriticalSection);
void WINAPI_CALL THETAZ_EnterCriticalSection(PCRITICAL_SECTION lpCriticalSection);
void WINAPI_CALL THETAZ_LeaveCriticalSection(PCRITICAL_SECTION lpCriticalSection);

HMODULE WINAPI_CALL THETAZ_GetModuleHandleA(LPCSTR lpModuleName);
HMODULE WINAPI_CALL THETAZ_GetModuleHandleW(LPCWSTR lpModuleName);
void* WINAPI_CALL THETAZ_GetProcAddress(HMODULE hModule, LPCSTR lpProcName);
HMODULE WINAPI_CALL THETAZ_LoadLibraryA(LPCSTR lpLibFileName);
HMODULE WINAPI_CALL THETAZ_LoadLibraryW(LPCWSTR lpLibFileName);
BOOL WINAPI_CALL THETAZ_FreeLibrary(HMODULE hLibModule);

BOOL WINAPI_CALL THETAZ_CloseHandle(HANDLE hObject);
HANDLE WINAPI_CALL THETAZ_CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                                      LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
                                      DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
HANDLE WINAPI_CALL THETAZ_CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                                      LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
                                      DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);

void* WINAPI_CALL THETAZ_SetUnhandledExceptionFilter(void* lpTopLevelExceptionFilter);

HANDLE WINAPI_CALL THETAZ_GetCurrentProcess();
HANDLE WINAPI_CALL THETAZ_GetCurrentThread();
DWORD WINAPI_CALL THETAZ_GetCurrentThreadId();
DWORD WINAPI_CALL THETAZ_GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize);
DWORD WINAPI_CALL THETAZ_GetModuleFileNameW(HMODULE hModule, LPWSTR lpFilename, DWORD nSize);
LPSTR WINAPI_CALL THETAZ_GetCommandLineA();
LPWSTR WINAPI_CALL THETAZ_GetCommandLineW();
void WINAPI_CALL THETAZ_GetStartupInfoW(void* lpStartupInfo);
void WINAPI_CALL THETAZ_GetSystemInfo(void* lpSystemInfo);
void WINAPI_CALL THETAZ_GetNativeSystemInfo(void* lpSystemInfo);
void WINAPI_CALL THETAZ_GetSystemTimeAsFileTime(void* lpSystemTimeAsFileTime);
DWORD WINAPI_CALL THETAZ_GetACP();
DWORD WINAPI_CALL THETAZ_GetOEMCP();
DWORD WINAPI_CALL THETAZ_GetFileAttributesA(LPCSTR lpFileName);
DWORD WINAPI_CALL THETAZ_GetFileAttributesW(LPCWSTR lpFileName);
BOOL WINAPI_CALL THETAZ_GetFileSizeEx(HANDLE hFile, void* lpFileSize);
BOOL WINAPI_CALL THETAZ_SetCurrentDirectoryA(LPCSTR lpPathName);
BOOL WINAPI_CALL THETAZ_SetCurrentDirectoryW(LPCWSTR lpPathName);
DWORD WINAPI_CALL THETAZ_GetCurrentDirectoryA(DWORD nBufferLength, LPSTR lpBuffer);
DWORD WINAPI_CALL THETAZ_GetCurrentDirectoryW(DWORD nBufferLength, LPWSTR lpBuffer);
BOOL WINAPI_CALL THETAZ_SetEnvironmentVariableA(LPCSTR lpName, LPCSTR lpValue);
BOOL WINAPI_CALL THETAZ_SetEnvironmentVariableW(LPCWSTR lpName, LPCWSTR lpValue);
DWORD WINAPI_CALL THETAZ_GetEnvironmentVariableA(LPCSTR lpName, LPSTR lpBuffer, DWORD nSize);
DWORD WINAPI_CALL THETAZ_GetEnvironmentVariableW(LPCWSTR lpName, LPWSTR lpBuffer, DWORD nSize);
void* WINAPI_CALL THETAZ_GetEnvironmentStringsW();
void* WINAPI_CALL THETAZ_GetEnvironmentStrings();
void* WINAPI_CALL THETAZ_GetEnvironmentStringsA();
BOOL WINAPI_CALL THETAZ_FreeEnvironmentStringsA(LPSTR lpszEnvironmentBlock);
BOOL WINAPI_CALL THETAZ_SetThreadPriority(HANDLE hThread, int nPriority);
DWORD WINAPI_CALL THETAZ_GetTempPathA(DWORD nBufferLength, LPSTR lpBuffer);
DWORD WINAPI_CALL THETAZ_GetTempPathW(DWORD nBufferLength, LPWSTR lpBuffer);
void WINAPI_CALL THETAZ_GetSystemTime(void* lpSystemTime);
void WINAPI_CALL THETAZ_GetLocalTime(void* lpSystemTime);
void WINAPI_CALL THETAZ_RaiseException(DWORD dwExceptionCode, DWORD dwExceptionFlags, DWORD nNumberOfArguments, void* lpArguments);
BOOL WINAPI_CALL THETAZ_TerminateProcess(HANDLE hProcess, UINT uExitCode);
HANDLE WINAPI_CALL THETAZ_GetProcessHeap();
void* WINAPI_CALL THETAZ_HeapAlloc(HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes);
BOOL WINAPI_CALL THETAZ_HeapFree(HANDLE hHeap, DWORD dwFlags, void* lpMem);
void* WINAPI_CALL THETAZ_HeapReAlloc(HANDLE hHeap, DWORD dwFlags, void* lpMem, SIZE_T dwBytes);
BOOL WINAPI_CALL THETAZ_HeapSetInformation(HANDLE hHeap, void* HeapInformationClass, void* HeapInformation, SIZE_T HeapInformationLength);
BOOL WINAPI_CALL THETAZ_FlushFileBuffers(HANDLE hFile);
BOOL WINAPI_CALL THETAZ_GetVersionExA(void* lpVersionInformation);
BOOL WINAPI_CALL THETAZ_GetVersionExW(void* lpVersionInformation);

// ----------------------------------------------------------------------
// Local/Global heap + string core (kernel32.cpp)
// ----------------------------------------------------------------------
LPVOID WINAPI_CALL THETAZ_LocalAlloc(UINT uFlags, SIZE_T uBytes);
LPVOID WINAPI_CALL THETAZ_LocalFree(LPVOID hMem);
LPVOID WINAPI_CALL THETAZ_LocalReAlloc(LPVOID hMem, SIZE_T uBytes, UINT uFlags);
SIZE_T WINAPI_CALL THETAZ_LocalSize(LPVOID hMem);
LPVOID WINAPI_CALL THETAZ_GlobalAlloc(UINT uFlags, SIZE_T dwBytes);
LPVOID WINAPI_CALL THETAZ_GlobalFree(LPVOID hMem);
LPVOID WINAPI_CALL THETAZ_GlobalReAlloc(LPVOID hMem, SIZE_T dwBytes, UINT uFlags);
SIZE_T WINAPI_CALL THETAZ_GlobalSize(LPVOID hMem);
HANDLE WINAPI_CALL THETAZ_HeapCreate(DWORD flOptions, SIZE_T dwInitialSize, SIZE_T dwMaximumSize);
BOOL WINAPI_CALL THETAZ_HeapDestroy(HANDLE hHeap);
SIZE_T WINAPI_CALL THETAZ_HeapSize(HANDLE hHeap, DWORD dwFlags, LPCVOID lpMem);
SIZE_T WINAPI_CALL THETAZ_HeapCompact(HANDLE hHeap, DWORD dwFlags);
BOOL WINAPI_CALL THETAZ_HeapValidate(HANDLE hHeap, DWORD dwFlags, LPCVOID lpMem);
BOOL WINAPI_CALL THETAZ_HeapWalk(HANDLE hHeap, void* lpEntry);
INT WINAPI_CALL THETAZ_lstrcmpW(LPCWSTR lpString1, LPCWSTR lpString2);
INT WINAPI_CALL THETAZ_lstrcmpiW(LPCWSTR lpString1, LPCWSTR lpString2);
INT WINAPI_CALL THETAZ_lstrlenW(LPCWSTR lpString);
LPWSTR WINAPI_CALL THETAZ_lstrcpyW(LPWSTR lpString1, LPCWSTR lpString2);
INT WINAPI_CALL THETAZ_MulDiv(INT nNumber, INT nNumerator, INT nDenominator);
LPVOID WINAPI_CALL THETAZ_DecodePointer(LPCVOID Ptr);
LPVOID WINAPI_CALL THETAZ_EncodePointer(LPCVOID Ptr);

// ----------------------------------------------------------------------
// Stdio / console / debug (kernel32.cpp)
// ----------------------------------------------------------------------
BOOL WINAPI_CALL THETAZ_SetStdHandle(DWORD nStdHandle, HANDLE hHandle);
HANDLE WINAPI_CALL THETAZ_GetStdHandle(DWORD nStdHandle);
void WINAPI_CALL THETAZ_OutputDebugStringA(LPCSTR lpOutputString);
void WINAPI_CALL THETAZ_OutputDebugStringW(LPCWSTR lpOutputString);
BOOL WINAPI_CALL THETAZ_SetConsoleMode(HANDLE hConsoleHandle, DWORD dwMode);
BOOL WINAPI_CALL THETAZ_GetConsoleMode(HANDLE hConsoleHandle, LPDWORD lpMode);
BOOL WINAPI_CALL THETAZ_GetNumberOfConsoleInputEvents(HANDLE hConsoleInput, LPDWORD lpNumberOfEvents);
UINT WINAPI_CALL THETAZ_GetConsoleOutputCP();
UINT WINAPI_CALL THETAZ_GetConsoleCP();
BOOL WINAPI_CALL THETAZ_AllocConsole();
BOOL WINAPI_CALL THETAZ_FreeConsole();
BOOL WINAPI_CALL THETAZ_AttachConsole(DWORD dwProcessId);
BOOL WINAPI_CALL THETAZ_FreeEnvironmentStringsW(LPWSTR lpszEnvironmentBlock);

// ----------------------------------------------------------------------
// File system API (kernel32.cpp)
// ----------------------------------------------------------------------
typedef struct _WIN32_FIND_DATAW {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD dwReserved0;
    DWORD dwReserved1;
    WCHAR cFileName[260];
    WCHAR cAlternateFileName[14];
} WIN32_FIND_DATAW;

typedef struct _BY_HANDLE_FILE_INFORMATION {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD dwVolumeSerialNumber;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD nNumberOfLinks;
    DWORD nFileIndexHigh;
    DWORD nFileIndexLow;
} BY_HANDLE_FILE_INFORMATION;

typedef struct _WIN32_FILE_ATTRIBUTE_DATA {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
} WIN32_FILE_ATTRIBUTE_DATA;

DWORD WINAPI_CALL THETAZ_GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh);
BOOL WINAPI_CALL THETAZ_GetFileTime(HANDLE hFile, FILETIME* lpCreationTime, FILETIME* lpLastAccessTime, FILETIME* lpLastWriteTime);
BOOL WINAPI_CALL THETAZ_SetFileTime(HANDLE hFile, const FILETIME* lpCreationTime, const FILETIME* lpLastAccessTime, const FILETIME* lpLastWriteTime);
DWORD WINAPI_CALL THETAZ_SetFilePointer(HANDLE hFile, LONG lDistanceToMove, LONG* lpDistanceToMoveHigh, DWORD dwMoveMethod);
BOOL WINAPI_CALL THETAZ_SetFilePointerEx(HANDLE hFile, LARGE_INTEGER liDistanceToMove, LARGE_INTEGER* lpNewFilePointer, DWORD dwMoveMethod);
DWORD WINAPI_CALL THETAZ_GetFileType(HANDLE hFile);
BOOL WINAPI_CALL THETAZ_SetEndOfFile(HANDLE hFile);
BOOL WINAPI_CALL THETAZ_CopyFileW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, BOOL bFailIfExists);
BOOL WINAPI_CALL THETAZ_CopyFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, BOOL bFailIfExists);
BOOL WINAPI_CALL THETAZ_CopyFileExW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, void* lpProgressRoutine, void* lpData, BOOL* pbCancel, DWORD dwCopyFlags);
BOOL WINAPI_CALL THETAZ_DeleteFileA(LPCSTR lpFileName);
BOOL WINAPI_CALL THETAZ_DeleteFileW(LPCWSTR lpFileName);
BOOL WINAPI_CALL THETAZ_MoveFileExW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, DWORD dwFlags);
BOOL WINAPI_CALL THETAZ_CreateDirectoryW(LPCWSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);
BOOL WINAPI_CALL THETAZ_RemoveDirectoryW(LPCWSTR lpPathName);
DWORD WINAPI_CALL THETAZ_GetFullPathNameA(LPCSTR lpFileName, DWORD nBufferLength, LPSTR lpBuffer, LPSTR* lpFilePart);
DWORD WINAPI_CALL THETAZ_GetFullPathNameW(LPCWSTR lpFileName, DWORD nBufferLength, LPWSTR lpBuffer, LPWSTR* lpFilePart);
UINT WINAPI_CALL THETAZ_GetTempFileNameW(LPCWSTR lpPathName, LPCWSTR lpPrefixString, UINT uUnique, LPWSTR lpTempFileName);
HANDLE WINAPI_CALL THETAZ_FindFirstFileW(LPCWSTR lpFileName, WIN32_FIND_DATAW* lpFindFileData);
HANDLE WINAPI_CALL THETAZ_FindFirstFileA(LPCSTR lpFileName, void* lpFindFileData);
HANDLE WINAPI_CALL THETAZ_FindFirstFileExW(LPCWSTR lpFileName, void* fInfoLevelId, WIN32_FIND_DATAW* lpFindFileData, void* fSearchOp, void* lpSearchFilter, DWORD dwAdditionalFlags);
BOOL WINAPI_CALL THETAZ_FindNextFileW(HANDLE hFindFile, WIN32_FIND_DATAW* lpFindFileData);
BOOL WINAPI_CALL THETAZ_FindNextFileA(HANDLE hFindFile, void* lpFindFileData);
BOOL WINAPI_CALL THETAZ_FindClose(HANDLE hFindFile);
BOOL WINAPI_CALL THETAZ_GetFileAttributesExW(LPCWSTR lpFileName, DWORD fInfoLevelId, void* lpFileInformation);
LPVOID WINAPI_CALL THETAZ_MapViewOfFile(HANDLE hFileMappingObject, DWORD dwDesiredAccess, DWORD dwFileOffsetHigh, DWORD dwFileOffsetLow, SIZE_T dwNumberOfBytesToMap);
BOOL WINAPI_CALL THETAZ_UnmapViewOfFile(LPCVOID lpBaseAddress);
BOOL WINAPI_CALL THETAZ_FlushViewOfFile(LPCVOID lpBaseAddress, SIZE_T dwNumberOfBytesToFlush);
HANDLE WINAPI_CALL THETAZ_CreateFileMappingA(HANDLE hFile, LPSECURITY_ATTRIBUTES lpAttributes, DWORD flProtect, DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCSTR lpName);
HANDLE WINAPI_CALL THETAZ_CreateFileMappingW(HANDLE hFile, LPSECURITY_ATTRIBUTES lpAttributes, DWORD flProtect, DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCWSTR lpName);
HANDLE WINAPI_CALL THETAZ_OpenFileMappingA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpName);
BOOL WINAPI_CALL THETAZ_GetFileInformationByHandle(HANDLE hFile, BY_HANDLE_FILE_INFORMATION* lpFileInformation);
BOOL WINAPI_CALL THETAZ_GetFileInformationByHandleEx(HANDLE hFile, DWORD FileInformationClass, void* lpFileInformation, DWORD dwBufferSize);
DWORD WINAPI_CALL THETAZ_GetFinalPathNameByHandleW(HANDLE hFile, LPWSTR lpszFilePath, DWORD cchFilePath, DWORD dwFlags);
BOOL WINAPI_CALL THETAZ_SetFileAttributesW(LPCWSTR lpFileName, DWORD dwFileAttributes);
BOOL WINAPI_CALL THETAZ_SetHandleInformation(HANDLE hObject, DWORD dwMask, DWORD dwFlags);
BOOL WINAPI_CALL THETAZ_SetFileInformationByHandle(HANDLE hFile, DWORD FileInformationClass, void* lpFileInformation, DWORD dwBufferSize);
BOOL WINAPI_CALL THETAZ_LockFile(HANDLE hFile, DWORD dwFileOffsetLow, DWORD dwFileOffsetHigh, DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh);
BOOL WINAPI_CALL THETAZ_LockFileEx(HANDLE hFile, DWORD dwFlags, DWORD dwReserved, DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh, void* lpOverlapped);
BOOL WINAPI_CALL THETAZ_UnlockFile(HANDLE hFile, DWORD dwFileOffsetLow, DWORD dwFileOffsetHigh, DWORD nNumberOfBytesToUnlockLow, DWORD nNumberOfBytesToUnlockHigh);
BOOL WINAPI_CALL THETAZ_UnlockFileEx(HANDLE hFile, DWORD dwReserved, DWORD nNumberOfBytesToUnlockLow, DWORD nNumberOfBytesToUnlockHigh, void* lpOverlapped);
BOOL WINAPI_CALL THETAZ_CreatePipe(HANDLE* hReadPipe, HANDLE* hWritePipe, LPSECURITY_ATTRIBUTES lpPipeAttributes, DWORD nSize);
DWORD WINAPI_CALL THETAZ_ExpandEnvironmentStringsW(LPCWSTR lpSrc, LPWSTR lpDst, DWORD nSize);
DWORD WINAPI_CALL THETAZ_GetLongPathNameW(LPCWSTR lpszShortPath, LPWSTR lpszLongPath, DWORD cchBuffer);
DWORD WINAPI_CALL THETAZ_GetShortPathNameA(LPCSTR lpszLongPath, LPSTR lpszShortPath, DWORD cchBuffer);
BOOL WINAPI_CALL THETAZ_PeekNamedPipe(HANDLE hNamedPipe, LPVOID lpBuffer, DWORD nBufferSize, LPDWORD lpBytesRead, LPDWORD lpTotalBytesAvail, LPDWORD lpBytesLeftThisMessage);

// ----------------------------------------------------------------------
// Synchronization (kernel32.cpp)
// ----------------------------------------------------------------------
DWORD WINAPI_CALL THETAZ_WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
DWORD WINAPI_CALL THETAZ_WaitForSingleObjectEx(HANDLE hHandle, DWORD dwMilliseconds, BOOL bAlertable);
DWORD WINAPI_CALL THETAZ_WaitForMultipleObjectsEx(DWORD nCount, const HANDLE* lpHandles, BOOL bWaitAll, DWORD dwMilliseconds, BOOL bAlertable);
BOOL WINAPI_CALL THETAZ_InitializeCriticalSectionEx(PCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount, DWORD Flags);
BOOL WINAPI_CALL THETAZ_TryEnterCriticalSection(PCRITICAL_SECTION lpCriticalSection);
HANDLE WINAPI_CALL THETAZ_CreateEventW(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCWSTR lpName);
HANDLE WINAPI_CALL THETAZ_CreateEventExW(LPSECURITY_ATTRIBUTES lpEventAttributes, LPCWSTR lpName, DWORD dwFlags, DWORD dwDesiredAccess);
HANDLE WINAPI_CALL THETAZ_CreateMutexW(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCWSTR lpName);
HANDLE WINAPI_CALL THETAZ_CreateMutexExW(LPSECURITY_ATTRIBUTES lpMutexAttributes, LPCWSTR lpName, DWORD dwFlags, DWORD dwDesiredAccess);
HANDLE WINAPI_CALL THETAZ_CreateWaitableTimerW(LPSECURITY_ATTRIBUTES lpTimerAttributes, BOOL bManualReset, LPCWSTR lpTimerName);
BOOL WINAPI_CALL THETAZ_SetWaitableTimer(HANDLE hTimer, const LARGE_INTEGER* lpDueTime, LONG lPeriod, void* pfnCompletionRoutine, void* lpArgToCompletionRoutine, BOOL fResume);
BOOL WINAPI_CALL THETAZ_SetWaitableTimerEx(HANDLE hTimer, const LARGE_INTEGER* lpDueTime, LONG lPeriod, void* pfnCompletionRoutine, void* lpArgToCompletionRoutine, void* WakeContext, LONG Reserved);
BOOL WINAPI_CALL THETAZ_CancelWaitableTimer(HANDLE hTimer);
BOOL WINAPI_CALL THETAZ_SwitchToThread();
BOOL WINAPI_CALL THETAZ_InitOnceBeginInitialize(void* lpInitOnce, DWORD dwFlags, BOOL* fPending, LPVOID* lpContext);
BOOL WINAPI_CALL THETAZ_InitOnceComplete(void* lpInitOnce, DWORD dwFlags, LPVOID lpContext);
BOOL WINAPI_CALL THETAZ_InitOnceExecuteOnce(void* lpInitOnce, void* InitOnceCallback, void* Parameter, LPVOID* lpContext);
DWORD WINAPI_CALL THETAZ_FlsAlloc(void* lpCallback);
BOOL WINAPI_CALL THETAZ_FlsFree(DWORD dwFlsIndex);
LPVOID WINAPI_CALL THETAZ_FlsGetValue(DWORD dwFlsIndex);
LPVOID WINAPI_CALL THETAZ_FlsGetValue2(DWORD dwFlsIndex);
BOOL WINAPI_CALL THETAZ_FlsSetValue(DWORD dwFlsIndex, LPVOID lpFlsData);
void WINAPI_CALL THETAZ_InitializeSListHead(void* ListHead);
void* WINAPI_CALL THETAZ_InterlockedPushEntrySList(void* ListHead, void* ListEntry);
void* WINAPI_CALL THETAZ_InterlockedPopEntrySList(void* ListHead);
WORD WINAPI_CALL THETAZ_QueryDepthSList(void* ListHead);
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
void WINAPI_CALL THETAZ_InitializeSRWLock(void* SRWLock);
void WINAPI_CALL THETAZ_InitializeConditionVariable(void* lpConditionVariable);
void WINAPI_CALL THETAZ_CloseThreadpoolCleanupGroupMembers(void* pcg, BOOL fCancelPendingCallbacks, void* pvCleanupContext);
void WINAPI_CALL THETAZ_CloseThreadpoolCleanupGroup(void* pcg);
void WINAPI_CALL THETAZ_CloseThreadpool(void* ptpp);
void* WINAPI_CALL THETAZ_ResolveDelayLoadedAPI(void* parent_base, void* descriptor, void* failure_dll_hook, void* failure_sys_hook, void* thunk_addr, ULONG flags);
void WINAPI_CALL SetLastError(DWORD dwErrorCode);

// ----------------------------------------------------------------------
// Locale / time / system info (kernel32.cpp)
// ----------------------------------------------------------------------
typedef struct _SYSTEMTIME {
    WORD wYear;
    WORD wMonth;
    WORD wDayOfWeek;
    WORD wDay;
    WORD wHour;
    WORD wMinute;
    WORD wSecond;
    WORD wMilliseconds;
} SYSTEMTIME;

typedef struct _TIME_ZONE_INFORMATION {
    LONG Bias;
    WCHAR StandardName[32];
    SYSTEMTIME StandardDate;
    LONG StandardBias;
    WCHAR DaylightName[32];
    SYSTEMTIME DaylightDate;
    LONG DaylightBias;
} TIME_ZONE_INFORMATION;

typedef struct _CPINFO {
    UINT MaxCharSize;
    BYTE DefaultChar[2];
    BYTE LeadByte[12];
} CPINFO;

typedef struct _CPINFOEXW {
    UINT MaxCharSize;
    BYTE DefaultChar[2];
    BYTE LeadByte[12];
    WCHAR UnicodeDefaultChar;
    WCHAR UnicodeLeadByte[4];
    WCHAR CodePageName[260];
} CPINFOEXW;

typedef struct _SYSTEM_LOGICAL_PROCESSOR_INFORMATION {
    uintptr_t ProcessorMask;
    DWORD Relationship;
    BYTE data[24];
} SYSTEM_LOGICAL_PROCESSOR_INFORMATION;

typedef struct _MEMORYSTATUSEX {
    DWORD dwLength;
    DWORD dwMemoryLoad;
    uint64_t ullTotalPhys;
    uint64_t ullAvailPhys;
    uint64_t ullTotalPageFile;
    uint64_t ullAvailPageFile;
    uint64_t ullTotalVirtual;
    uint64_t ullAvailVirtual;
    uint64_t ullAvailExtendedVirtual;
} MEMORYSTATUSEX;

typedef struct _SYSTEM_POWER_STATUS {
    BYTE ACLineStatus;
    BYTE BatteryFlag;
    BYTE BatteryLifePercent;
    BYTE SystemStatusFlag;
    DWORD BatteryLifeTime;
    DWORD BatteryFullLifeTime;
} SYSTEM_POWER_STATUS;

typedef BOOL (*LOCALE_ENUMPROCW)(LPWSTR lpLocaleString);
typedef BOOL (*LOCALE_ENUMPROCEX)(LPWSTR lpLocaleString, DWORD dwFlags, uintptr_t lParam);

INT WINAPI_CALL THETAZ_CompareStringW(DWORD Locale, DWORD dwCmpFlags, LPCWSTR lpString1, INT cchCount1, LPCWSTR lpString2, INT cchCount2);
INT WINAPI_CALL THETAZ_CompareStringEx(LPCWSTR lpLocaleName, DWORD dwCmpFlags, LPCWSTR lpString1, INT cchCount1, LPCWSTR lpString2, INT cchCount2, void* lpVersionInformation, void* lpReserved, void* lParam);
INT WINAPI_CALL THETAZ_LCMapStringW(DWORD Locale, DWORD dwMapFlags, LPCWSTR lpSrcStr, INT cchSrc, LPWSTR lpDestStr, INT cchDest);
INT WINAPI_CALL THETAZ_LCMapStringEx(LPCWSTR lpLocaleName, DWORD dwMapFlags, LPCWSTR lpSrcStr, INT cchSrc, LPWSTR lpDestStr, INT cchDest, void* lpVersionInformation, void* lpReserved, void* lParam);
DWORD WINAPI_CALL THETAZ_GetSystemDefaultLCID();
DWORD WINAPI_CALL THETAZ_GetUserDefaultLCID();
INT WINAPI_CALL THETAZ_GetSystemDefaultLocaleName(LPWSTR lpLocaleName, INT cchLocaleName);
INT WINAPI_CALL THETAZ_GetUserDefaultLocaleName(LPWSTR lpLocaleName, INT cchLocaleName);
INT WINAPI_CALL THETAZ_LCIDToLocaleName(DWORD Locale, LPWSTR lpName, INT cchName, DWORD dwFlags);
DWORD WINAPI_CALL THETAZ_LocaleNameToLCID(LPCWSTR lpName, DWORD dwFlags);
BOOL WINAPI_CALL THETAZ_EnumSystemLocalesW(LOCALE_ENUMPROCW lpLocaleEnumProc, DWORD dwFlags);
BOOL WINAPI_CALL THETAZ_EnumSystemLocalesEx(LOCALE_ENUMPROCEX lpLocaleEnumProcEx, DWORD dwFlags, uintptr_t lParam, void* lpReserved);
INT WINAPI_CALL THETAZ_GetLocaleInfoW(DWORD Locale, DWORD LCType, LPWSTR lpLCData, INT cchData);
INT WINAPI_CALL THETAZ_GetLocaleInfoEx(LPCWSTR lpLocaleName, DWORD LCType, LPWSTR lpLCData, INT cchData);
INT WINAPI_CALL THETAZ_GetLocaleInfoA(DWORD Locale, DWORD LCType, LPSTR lpLCData, INT cchData);
BOOL WINAPI_CALL THETAZ_IsValidLocale(DWORD Locale, DWORD dwFlags);
DWORD WINAPI_CALL THETAZ_GetUserGeoID(DWORD GeoClass);
BOOL WINAPI_CALL THETAZ_GetSystemPreferredUILanguages(DWORD dwFlags, DWORD* pulNumLanguages, LPWSTR pwszLanguagesBuffer, DWORD* pcchLanguagesBuffer);
BOOL WINAPI_CALL THETAZ_GetUserPreferredUILanguages(DWORD dwFlags, DWORD* pulNumLanguages, LPWSTR pwszLanguagesBuffer, DWORD* pcchLanguagesBuffer);
DWORD WINAPI_CALL THETAZ_GetThreadLocale();
BOOL WINAPI_CALL THETAZ_GetStringTypeW(DWORD dwInfoType, LPCWSTR lpSrcStr, INT cchSrc, WORD* lpCharType);
BOOL WINAPI_CALL THETAZ_GetStringTypeExW(DWORD Locale, DWORD dwInfoType, LPCWSTR lpSrcStr, INT cchSrc, WORD* lpCharType);
BOOL WINAPI_CALL THETAZ_GetStringTypeExA(DWORD Locale, DWORD dwInfoType, LPCSTR lpSrcStr, INT cchSrc, WORD* lpCharType);
BOOL WINAPI_CALL THETAZ_FileTimeToSystemTime(const FILETIME* lpFileTime, SYSTEMTIME* lpSystemTime);
BOOL WINAPI_CALL THETAZ_SystemTimeToFileTime(const SYSTEMTIME* lpSystemTime, FILETIME* lpFileTime);
DWORD WINAPI_CALL THETAZ_GetTimeZoneInformation(TIME_ZONE_INFORMATION* lpTimeZoneInformation);
BOOL WINAPI_CALL THETAZ_TzSpecificLocalTimeToSystemTime(const TIME_ZONE_INFORMATION* lpTimeZoneInformation, const SYSTEMTIME* lpLocalTime, SYSTEMTIME* lpUniversalTime);
INT WINAPI_CALL THETAZ_GetDateFormatW(DWORD Locale, DWORD dwFlags, const SYSTEMTIME* lpDate, LPCWSTR lpFormat, LPWSTR lpDateStr, INT cchDate);
INT WINAPI_CALL THETAZ_GetTimeFormatW(DWORD Locale, DWORD dwFlags, const SYSTEMTIME* lpTime, LPCWSTR lpFormat, LPWSTR lpTimeStr, INT cchTime);
BOOL WINAPI_CALL THETAZ_QueryUnbiasedInterruptTime(uint64_t* lpUnbiasedInterruptTime);
UINT WINAPI_CALL THETAZ_GetSystemDirectoryW(LPWSTR lpBuffer, UINT uSize);
BOOL WINAPI_CALL THETAZ_GetComputerNameW(LPWSTR lpBuffer, LPDWORD nSize);
BOOL WINAPI_CALL THETAZ_GetLogicalProcessorInformation(SYSTEM_LOGICAL_PROCESSOR_INFORMATION* Buffer, DWORD* ReturnedLength);
BOOL WINAPI_CALL THETAZ_GetProductInfo(DWORD dwOSMajorVersion, DWORD dwOSMinorVersion, DWORD dwSpMajorVersion, DWORD dwSpMinorVersion, DWORD* pdwReturnedProductType);
BOOL WINAPI_CALL THETAZ_GetPhysicallyInstalledSystemMemory(uint64_t* TotalBytesInKilobytes);
BOOL WINAPI_CALL THETAZ_GetCPInfo(UINT CodePageID, CPINFO* lpCPInfo);
BOOL WINAPI_CALL THETAZ_GetCPInfoExW(UINT CodePageID, DWORD dwFlags, CPINFOEXW* lpCPInfoEx);
BOOL WINAPI_CALL THETAZ_IsValidCodePage(UINT CodePageID);
BOOL WINAPI_CALL THETAZ_IsProcessorFeaturePresent(DWORD ProcessorFeature);
BOOL WINAPI_CALL THETAZ_IsSystemResumeAutomatic();
BOOL WINAPI_CALL THETAZ_GlobalMemoryStatusEx(MEMORYSTATUSEX* lpBuffer);
BOOL WINAPI_CALL THETAZ_GetSystemPowerStatus(SYSTEM_POWER_STATUS* lpSystemPowerStatus);
BOOL WINAPI_CALL THETAZ_AreFileApisANSI();
DWORD WINAPI_CALL THETAZ_FormatMessageA(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId, DWORD dwLanguageId, LPSTR lpBuffer, DWORD nSize, void* Arguments);
DWORD WINAPI_CALL THETAZ_FormatMessageW(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId, DWORD dwLanguageId, LPWSTR lpBuffer, DWORD nSize, void* Arguments);

// ----------------------------------------------------------------------
// Process / misc (kernel32.cpp)
// ----------------------------------------------------------------------
typedef struct _ULARGE_INTEGER {
    uint64_t QuadPart;
} ULARGE_INTEGER;

typedef struct _PROCESS_MEMORY_COUNTERS {
    DWORD cb;
    DWORD PageFaultCount;
    uint64_t PeakWorkingSetSize;
    uint64_t WorkingSetSize;
    uint64_t QuotaPeakPagedPoolUsage;
    uint64_t QuotaPagedPoolUsage;
    uint64_t QuotaPeakNonPagedPoolUsage;
    uint64_t QuotaNonPagedPoolUsage;
    uint64_t PagefileUsage;
    uint64_t PeakPagefileUsage;
} PROCESS_MEMORY_COUNTERS;

BOOL WINAPI_CALL THETAZ_SetProcessMitigationPolicy(DWORD Policy, void* lpBuffer, SIZE_T dwLength);
BOOL WINAPI_CALL THETAZ_IsWow64Process(HANDLE hProcess, PBOOL Wow64Process);
BOOL WINAPI_CALL THETAZ_GetModuleHandleExW(DWORD dwFlags, LPCWSTR lpModuleName, HMODULE* phModule);
uint64_t WINAPI_CALL THETAZ_VerSetConditionMask(uint64_t ConditionMask, DWORD TypeMask, BYTE Condition);
BOOL WINAPI_CALL THETAZ_VerifyVersionInfoW(void* lpVersionInformation, DWORD dwTypeMask, uint64_t dwlConditionMask);
HANDLE WINAPI_CALL THETAZ_OpenProcess(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId);
BOOL WINAPI_CALL THETAZ_DuplicateHandle(HANDLE hSourceProcessHandle, HANDLE hSourceHandle, HANDLE hTargetProcessHandle, HANDLE* lpTargetHandle, DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwOptions);
BOOL WINAPI_CALL THETAZ_Beep(DWORD dwFreq, DWORD dwDuration);
HANDLE WINAPI_CALL THETAZ_OpenThread(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwThreadId);
BOOL WINAPI_CALL THETAZ_GetProcessTimes(HANDLE hProcess, FILETIME* lpCreationTime, FILETIME* lpExitTime, FILETIME* lpKernelTime, FILETIME* lpUserTime);
BOOL WINAPI_CALL THETAZ_GetThreadTimes(HANDLE hThread, FILETIME* lpCreationTime, FILETIME* lpExitTime, FILETIME* lpKernelTime, FILETIME* lpUserTime);
BOOL WINAPI_CALL THETAZ_GetProcessAffinityMask(HANDLE hProcess, DWORD* lpProcessAffinityMask, DWORD* lpSystemAffinityMask);
DWORD WINAPI_CALL THETAZ_GetPriorityClass(HANDLE hProcess);
BOOL WINAPI_CALL THETAZ_ProcessIdToSessionId(DWORD dwProcessId, DWORD* pSessionId);
UINT WINAPI_CALL THETAZ_SetErrorMode(UINT uMode);
void WINAPI_CALL THETAZ_RtlUnwind(void* TargetFrame, void* TargetIp, void* ExceptionRecord, DWORD ReturnValue);
void WINAPI_CALL THETAZ_RtlUnwindEx(void* TargetFrame, void* TargetIp, void* ExceptionRecord, void* ReturnValue, void* ContextRecord, void* HistoryTable);
WORD WINAPI_CALL THETAZ_RtlCaptureStackBackTrace(DWORD FramesToSkip, DWORD FramesToCapture, void** BackTrace, DWORD* BackTraceHash);
LONG WINAPI_CALL THETAZ_UnhandledExceptionFilter(void* ExceptionInfo);
HMODULE WINAPI_CALL THETAZ_LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
void WINAPI_CALL THETAZ_FreeLibraryAndExitThread(HMODULE hLibModule, DWORD dwExitCode);
void WINAPI_CALL THETAZ_ExitThread(DWORD dwExitCode);
BOOL WINAPI_CALL THETAZ_K32EnumProcesses(DWORD* lpidProcess, DWORD cb, LPDWORD lpcbNeeded);
BOOL WINAPI_CALL THETAZ_K32EnumProcessModulesEx(HANDLE hProcess, HMODULE* lphModule, DWORD cb, LPDWORD lpcbNeeded, DWORD dwFilterFlag);
DWORD WINAPI_CALL THETAZ_K32GetModuleFileNameExW(HANDLE hProcess, HMODULE hModule, LPWSTR lpFilename, DWORD nSize);
DWORD WINAPI_CALL THETAZ_K32GetProcessImageFileNameW(HANDLE hProcess, LPWSTR lpImageFileName, DWORD nSize);
BOOL WINAPI_CALL THETAZ_K32GetProcessMemoryInfo(HANDLE hProcess, PROCESS_MEMORY_COUNTERS* ppsmemCounters, DWORD cb);
BOOL WINAPI_CALL THETAZ_QueryProcessCycleTime(HANDLE hProcess, uint64_t* CycleTime);
BOOL WINAPI_CALL THETAZ_QueryThreadCycleTime(HANDLE hThread, uint64_t* CycleTime);
HANDLE WINAPI_CALL THETAZ_CreateIoCompletionPort(HANDLE hFile, HANDLE hExistingCompletionPort, uintptr_t CompletionKey, DWORD dwNumberOfConcurrentThreads);
BOOL WINAPI_CALL THETAZ_PostQueuedCompletionStatus(HANDLE CompletionPort, DWORD dwNumberOfBytesTransferred, uintptr_t dwCompletionKey, void* lpOverlapped);
BOOL WINAPI_CALL THETAZ_GetQueuedCompletionStatus(HANDLE CompletionPort, LPDWORD lpNumberOfBytesTransferred, uintptr_t* lpCompletionKey, void** lpOverlapped, DWORD dwMilliseconds);
HANDLE WINAPI_CALL THETAZ_CreateMemoryResourceNotification(DWORD NotificationType);
BOOL WINAPI_CALL THETAZ_DeviceIoControl(HANDLE hDevice, DWORD dwIoControlCode, void* lpInBuffer, DWORD nInBufferSize, void* lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, void* lpOverlapped);
BOOL WINAPI_CALL THETAZ_GetOverlappedResult(HANDLE hFile, void* lpOverlapped, LPDWORD lpNumberOfBytesTransferred, BOOL bWait);
BOOL WINAPI_CALL THETAZ_CancelIoEx(HANDLE hFile, void* lpOverlapped);
BOOL WINAPI_CALL THETAZ_GetThreadIOPendingFlag(HANDLE hThread, PBOOL lpIOIsPending);
BOOL WINAPI_CALL THETAZ_GetDiskFreeSpaceA(LPCSTR lpRootPathName, LPDWORD lpSectorsPerCluster, LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters, LPDWORD lpTotalNumberOfClusters);
BOOL WINAPI_CALL THETAZ_GetDiskFreeSpaceW(LPCWSTR lpRootPathName, LPDWORD lpSectorsPerCluster, LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters, LPDWORD lpTotalNumberOfClusters);
BOOL WINAPI_CALL THETAZ_GetDiskFreeSpaceExW(LPCWSTR lpDirectoryName, ULARGE_INTEGER* lpFreeBytesAvailableToCaller, ULARGE_INTEGER* lpTotalNumberOfBytes, ULARGE_INTEGER* lpTotalNumberOfFreeBytes);
UINT WINAPI_CALL THETAZ_GetDriveTypeW(LPCWSTR lpRootPathName);
BOOL WINAPI_CALL THETAZ_GetVolumeInformationW(LPCWSTR lpRootPathName, LPWSTR lpVolumeNameBuffer, DWORD nVolumeNameSize, LPDWORD lpVolumeSerialNumber, LPDWORD lpMaximumComponentLength, LPDWORD lpFileSystemFlags, LPWSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize);
BOOL WINAPI_CALL THETAZ_GetVolumeNameForVolumeMountPointW(LPCWSTR lpszVolumeMountPoint, LPWSTR lpszVolumeName, DWORD cchBufferLength);
BOOL WINAPI_CALL THETAZ_GetVolumePathNamesForVolumeNameW(LPCWSTR lpszVolumeName, LPWSTR lpszVolumePathNames, DWORD cchBufferLength, PDWORD lpcchReturnLength);
BOOL WINAPI_CALL THETAZ_GetVolumePathNameW(LPCWSTR lpszFileName, LPWSTR lpszVolumePathName, DWORD cchBufferLength);
char* WINAPI_CALL THETAZ_PathFindExtensionA(LPCSTR p);
wchar_t* WINAPI_CALL THETAZ_PathFindExtensionW(LPCWSTR p);
void WINAPI_CALL THETAZ_PathRemoveExtensionA(LPSTR p);
void WINAPI_CALL THETAZ_PathRemoveExtensionW(LPWSTR p);
BOOL WINAPI_CALL THETAZ_PathRenameExtensionA(LPSTR p, LPCSTR e);
BOOL WINAPI_CALL THETAZ_PathRenameExtensionW(LPWSTR p, LPCWSTR e);
BOOL WINAPI_CALL THETAZ_PathMatchSpecW(LPCWSTR p, LPCWSTR sp);
HRESULT WINAPI_CALL THETAZ_PathMatchSpecExW(const wchar_t* p, const wchar_t* sp, DWORD f, void* o);

char* WINAPI_CALL THETAZ_PathFindFileNameA(LPCSTR p);
LPWSTR WINAPI_CALL THETAZ_PathFindFileNameW(LPCWSTR p);
wchar_t* WINAPI_CALL THETAZ_PathFindNextComponentW(LPCWSTR p);
BOOL WINAPI_CALL THETAZ_PathSkipRootW(LPWSTR p);
void WINAPI_CALL THETAZ_PathStripPathA(LPSTR p);
void WINAPI_CALL THETAZ_PathStripPathW(LPWSTR p);
BOOL WINAPI_CALL THETAZ_PathSearchAndQualifyA(LPCSTR p, LPSTR b, UINT* l);
BOOL WINAPI_CALL THETAZ_PathSearchAndQualifyW(LPCWSTR p, LPWSTR b, UINT* l);
BOOL WINAPI_CALL THETAZ_PathStripToRootA(LPSTR p);
BOOL WINAPI_CALL THETAZ_PathStripToRootW(LPWSTR p);

char* WINAPI_CALL THETAZ_PathGetArgsA(LPCSTR p);
wchar_t* WINAPI_CALL THETAZ_PathGetArgsW(LPCWSTR p);
int WINAPI_CALL THETAZ_PathGetCharTypeA(LPCSTR p, DWORD* type);
int WINAPI_CALL THETAZ_PathGetCharTypeW(LPCWSTR p, DWORD* type);
int WINAPI_CALL THETAZ_PathGetDriveNumberA(LPCSTR p);
int WINAPI_CALL THETAZ_PathGetDriveNumberW(LPCWSTR p);
BOOL WINAPI_CALL THETAZ_PathIsFileSpecA(LPCSTR p);
BOOL WINAPI_CALL THETAZ_PathIsFileSpecW(LPCWSTR p);
BOOL WINAPI_CALL THETAZ_PathIsLFNFileSpecA(LPCSTR p);
BOOL WINAPI_CALL THETAZ_PathIsLFNFileSpecW(LPCWSTR p);
BOOL WINAPI_CALL THETAZ_PathIsPrefixA(LPCSTR p, LPCSTR prefix);
BOOL WINAPI_CALL THETAZ_PathIsPrefixW(LPCWSTR p, LPCWSTR prefix);
BOOL WINAPI_CALL THETAZ_PathIsRelativeA(LPCSTR p);
BOOL WINAPI_CALL THETAZ_PathIsRelativeW(LPCWSTR p);
BOOL WINAPI_CALL THETAZ_PathIsRootA(LPCSTR p);
BOOL WINAPI_CALL THETAZ_PathIsRootW(LPCWSTR p);
BOOL WINAPI_CALL THETAZ_PathIsSameRootA(LPCSTR p1, LPCSTR p2);
BOOL WINAPI_CALL THETAZ_PathIsSameRootW(LPCWSTR p1, LPCWSTR p2);
BOOL WINAPI_CALL THETAZ_PathIsUNCA(LPCSTR p);
BOOL WINAPI_CALL THETAZ_PathIsUNCW(LPCWSTR p);
BOOL WINAPI_CALL THETAZ_PathIsUNCServerA(LPCSTR p);
BOOL WINAPI_CALL THETAZ_PathIsUNCServerW(LPCWSTR p);
BOOL WINAPI_CALL THETAZ_PathIsUNCServerShareA(LPCSTR p);
BOOL WINAPI_CALL THETAZ_PathIsUNCServerShareW(LPCWSTR p);
BOOL WINAPI_CALL THETAZ_PathIsURLA(LPCSTR p);
BOOL WINAPI_CALL THETAZ_PathIsURLW(LPCWSTR p);
BOOL WINAPI_CALL THETAZ_PathIsValidCharA(LPCSTR p, DWORD flags);
BOOL WINAPI_CALL THETAZ_PathIsValidCharW(LPCWSTR p, DWORD flags);
BOOL WINAPI_CALL THETAZ_PathMatchSpecA(LPCSTR p, LPCSTR spec);
BOOL WINAPI_CALL THETAZ_PathMatchSpecExA(LPCSTR p, LPCSTR spec, DWORD flags, void* output);
int WINAPI_CALL THETAZ_PathParseIconLocationA(LPSTR p);
int WINAPI_CALL THETAZ_PathParseIconLocationW(LPWSTR p);
void WINAPI_CALL THETAZ_PathQuoteSpacesA(LPSTR p);
void WINAPI_CALL THETAZ_PathQuoteSpacesW(LPWSTR p);
BOOL WINAPI_CALL THETAZ_PathRelativePathToA(LPSTR to, LPCSTR from, DWORD fromAttr, LPCSTR toPath, DWORD toAttr);
BOOL WINAPI_CALL THETAZ_PathRelativePathToW(LPWSTR to, LPCWSTR from, DWORD fromAttr, LPCWSTR toPath, DWORD toAttr);
void WINAPI_CALL THETAZ_PathRemoveBackslashA(LPSTR p);
void WINAPI_CALL THETAZ_PathRemoveBackslashW(LPWSTR p);
void WINAPI_CALL THETAZ_PathRemoveBlanksA(LPSTR p);
void WINAPI_CALL THETAZ_PathRemoveBlanksW(LPWSTR p);
void WINAPI_CALL THETAZ_PathRemoveFileSpecA(LPSTR p);
BOOL WINAPI_CALL THETAZ_PathRemoveFileSpecW(LPWSTR p);
BOOL WINAPI_CALL THETAZ_PathUnExpandEnvStringsA(LPCSTR p, LPSTR buf, UINT* len);
BOOL WINAPI_CALL THETAZ_PathUnExpandEnvStringsW(LPCWSTR p, LPWSTR buf, UINT* len);
void WINAPI_CALL THETAZ_PathUnquoteSpacesA(LPSTR p);
void WINAPI_CALL THETAZ_PathUnquoteSpacesW(LPWSTR p);

HANDLE WINAPI_CALL THETAZ_OpenEventA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpName);
HANDLE WINAPI_CALL THETAZ_OpenEventW(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName);
HANDLE WINAPI_CALL THETAZ_OpenMutexA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpName);
HANDLE WINAPI_CALL THETAZ_OpenMutexW(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName);
HANDLE WINAPI_CALL THETAZ_OpenSemaphoreA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpName);
BOOL WINAPI_CALL THETAZ_CreateSymbolicLinkW(LPCWSTR lpSymlinkFileName, LPCWSTR lpTargetFileName, DWORD dwFlags);
void* WINAPI_CALL THETAZ_FindResourceW(HMODULE hModule, LPCWSTR lpName, LPCWSTR lpType);
void* WINAPI_CALL THETAZ_LoadResource(HMODULE hModule, void* hResInfo);
LPVOID WINAPI_CALL THETAZ_LockResource(void* hResData);
DWORD WINAPI_CALL THETAZ_SizeofResource(HMODULE hModule, void* hResInfo);
BOOL WINAPI_CALL THETAZ_FreeResource(void* hResData);
BOOL WINAPI_CALL THETAZ_CreateTimerQueueTimer(HANDLE* phNewTimer, HANDLE TimerQueue, void* Callback, void* Parameter, DWORD DueTime, DWORD Period, ULONG Flags);
BOOL WINAPI_CALL THETAZ_DeleteTimerQueueTimer(HANDLE TimerQueue, HANDLE Timer, HANDLE CompletionEvent);
HANDLE WINAPI_CALL THETAZ_CreateTimerQueue();
BOOL WINAPI_CALL THETAZ_DeleteTimerQueue(HANDLE TimerQueue);
LONG WINAPI_CALL THETAZ_WerRegisterMemoryBlock(void* pvAddress, DWORD dwSize);
LONG WINAPI_CALL THETAZ_WerUnregisterMemoryBlock(void* pvAddress);
BOOL WINAPI_CALL THETAZ_QueryFullProcessImageNameW(HANDLE hProcess, DWORD dwFlags, LPWSTR lpExeName, PDWORD lpdwSize);

BOOL WINAPI_CALL THETAZ_CreateProcessA(LPCSTR lpApplicationName, LPSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCSTR lpCurrentDirectory, LPSTARTUPINFOA lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation);
BOOL WINAPI_CALL THETAZ_CreateProcessW(LPCWSTR lpApplicationName, LPWSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation);
LONG WINAPI_CALL THETAZ_RegCreateKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD Reserved, LPSTR lpClass, DWORD dwOptions, REGSAM samDesired, LPSECURITY_ATTRIBUTES lpSecurityAttributes, PHKEY phkResult, LPDWORD lpdwDisposition);
LONG WINAPI_CALL THETAZ_RegCreateKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD Reserved, LPWSTR lpClass, DWORD dwOptions, REGSAM samDesired, LPSECURITY_ATTRIBUTES lpSecurityAttributes, PHKEY phkResult, LPDWORD lpdwDisposition);
LONG WINAPI_CALL THETAZ_RegQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);
LONG WINAPI_CALL THETAZ_RegQueryValueExW(HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);
LONG WINAPI_CALL THETAZ_RegSetValueExA(HKEY hKey, LPCSTR lpValueName, DWORD Reserved, DWORD dwType, const BYTE* lpData, DWORD cbData);
LONG WINAPI_CALL THETAZ_RegSetValueExW(HKEY hKey, LPCWSTR lpValueName, DWORD Reserved, DWORD dwType, const BYTE* lpData, DWORD cbData);
// ADVAPI32 registry functions are declared in kernel32_runtime.cpp
LONG WINAPI_CALL THETAZ_RegEnumKeyExA(HKEY hKey, DWORD dwIndex, LPSTR lpName, LPDWORD lpcbName, LPDWORD lpReserved, LPSTR lpClass, LPDWORD lpcbClass, LPFILETIME lpftLastWriteTime);
LONG WINAPI_CALL THETAZ_RegEnumKeyExW(HKEY hKey, DWORD dwIndex, LPWSTR lpName, LPDWORD lpcbName, LPDWORD lpReserved, LPWSTR lpClass, LPDWORD lpcbClass, LPFILETIME lpftLastWriteTime);
LONG WINAPI_CALL THETAZ_RegDeleteValueA(HKEY hKey, LPCSTR lpValueName);

// OLE32 declarations
int WINAPI_CALL THETAZ_StringFromGUID2(REFGUID rguid, LPOLESTR lpsz, int cchMax);
HRESULT WINAPI_CALL THETAZ_CoInitialize(LPVOID pvReserved);
void WINAPI_CALL THETAZ_CoUninitialize();
HRESULT WINAPI_CALL THETAZ_CoCreateInstance(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID* ppv);
HRESULT WINAPI_CALL THETAZ_OleInitialize(LPVOID pvReserved);
void WINAPI_CALL THETAZ_OleUninitialize();
HRESULT WINAPI_CALL THETAZ_CLSIDFromProgID(LPCOLESTR lpszProgID, LPCLSID lpclsid);
HRESULT WINAPI_CALL THETAZ_StringFromCLSID(REFCLSID rclsid, LPOLESTR* lplpsz);
HRESULT WINAPI_CALL THETAZ_CLSIDFromString(LPCOLESTR lpsz, LPCLSID pclsid);
HRESULT WINAPI_CALL THETAZ_CoGetClassObject(REFCLSID rclsid, DWORD dwClsContext, LPVOID pvReserved, REFIID riid, LPVOID* ppv);

// IMM32 declarations
BOOL WINAPI_CALL THETAZ_ImmDisableIME(DWORD id);

// Shared string/path helpers (implemented in kernel32_runtime.cpp)
std::string windows_to_linux_path(const std::string& windows_path);
std::string utf8_to_utf16le(const std::string& in, std::u16string& out);
std::string utf16le_to_utf8(const uint16_t* s);

// ntdll_api.cpp uses these kernel32 implementations; declared here so the
// ntdll layer can forward to them.
void WINAPI_CALL THETAZ_ExitProcess(uint32_t exit_code);
BOOL WINAPI_CALL THETAZ_SetEvent(HANDLE hEvent);
BOOL WINAPI_CALL THETAZ_ResetEvent(HANDLE hEvent);
BOOL WINAPI_CALL THETAZ_PulseEvent(HANDLE hEvent);
BOOL WINAPI_CALL THETAZ_ReleaseMutex(HANDLE hMutex);
DWORD WINAPI_CALL THETAZ_WaitForMultipleObjects(DWORD nCount, const HANDLE* lpHandles,
                                                BOOL bWaitAll, DWORD dwMilliseconds);
void WINAPI_CALL THETAZ_AcquireSRWLockExclusive(void* SRWLock);
void WINAPI_CALL THETAZ_AcquireSRWLockShared(void* SRWLock);
void WINAPI_CALL THETAZ_ReleaseSRWLockExclusive(void* SRWLock);
void WINAPI_CALL THETAZ_ReleaseSRWLockShared(void* SRWLock);

// ADVAPI32 additional
BOOL WINAPI_CALL THETAZ_GetSecurityDescriptorDacl(PSECURITY_DESCRIPTOR pSecurityDescriptor, LPBOOL lpbDaclPresent, PACL* pDacl, LPBOOL lpbDaclDefaulted);

// ADVAPI32 LSA policy functions
void* WINAPI_CALL THETAZ_LsaOpenPolicy(void* systemName, void* objectAttributes, ULONG access, void** policyHandle);
void* WINAPI_CALL THETAZ_LsaClose(void* objectHandle);
void* WINAPI_CALL THETAZ_LsaFreeMemory(void* buffer);
ULONG WINAPI_CALL THETAZ_LsaNtStatusToWinError(void* status);
void* WINAPI_CALL THETAZ_LsaQueryInformationPolicy(void* policyHandle, ULONG infoClass, void** buffer);
void* WINAPI_CALL THETAZ_LsaEnumerateLogonSessions(void* logonSessionCount, void** logonSessionList);

// Windows constants
#define TRUE 1
#define FALSE 0

// Windows constants (continued)
#define TRUE_VALUE 1
#define FALSE_VALUE 0

// Exception handling constants
#define EXCEPTION_CONTINUE_SEARCH 0
#define EXCEPTION_CONTINUE_EXECUTION (-1)


// User32/GDI32 functions (window.cpp, user32.cpp)
    BOOL WINAPI_CALL THETAZ_BeginPaint(void* hwnd, void* ps);
    BOOL WINAPI_CALL THETAZ_BitBlt(void* hdcDest, int x, int y, int w, int h, void* hdcSrc, int x1, int y1);
    LPCSTR WINAPI_CALL THETAZ_CharNextA(LPCSTR lpsz);
    void* WINAPI_CALL THETAZ_CommandLineToArgvW(LPCWSTR lpCmdLine, int* pNumArgs);
    void* WINAPI_CALL THETAZ_CreateCompatibleDC(void* hdc);
    HANDLE WINAPI_CALL THETAZ_CreateEventA(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialOwner, LPCSTR lpName);
    void* WINAPI_CALL THETAZ_CreateFontA(int h, int w, int e, int o, int weight, BOOL i, BOOL u, BOOL s, const char* face);
    HANDLE WINAPI_CALL THETAZ_CreateMutexA(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCSTR lpName);
    void* WINAPI_CALL THETAZ_CreatePen(int style, int width, DWORD color);
    HANDLE WINAPI_CALL THETAZ_CreateSemaphoreA(LPSECURITY_ATTRIBUTES lpSemaphoreAttributes, LONG lInitialCount, LONG lMaximumCount, LPCSTR lpName);
    void* WINAPI_CALL THETAZ_CreateSolidBrush(DWORD color);
    BOOL WINAPI_CALL THETAZ_DeleteDC(void* hdc);
    BOOL WINAPI_CALL THETAZ_DeleteObject(void* hobj);
    BOOL WINAPI_CALL THETAZ_DisableThreadLibraryCalls(HMODULE hModule);
    BOOL WINAPI_CALL THETAZ_EnableMenuItem(HMENU hMenu, UINT uIDEnableItem, UINT uEnable);
    BOOL WINAPI_CALL THETAZ_EndPaint(void* hwnd, void* ps);
    int WINAPI_CALL THETAZ_GetAddrInfoW(const wchar_t* pNodeName, const wchar_t* pServiceName, const void* pHints, void** ppResult);
    BOOL WINAPI_CALL THETAZ_GetClassInfoW(HINSTANCE hInstance, LPCWSTR lpClassName, LPWNDCLASSW lpWndClass);
    DWORD WINAPI_CALL THETAZ_GetClassLongPtrW(HWND hWnd, int nIndex);
    BOOL WINAPI_CALL THETAZ_GetClientRect(void* hwnd, void* rect);
    BOOL WINAPI_CALL THETAZ_GetComputerNameExW(COMPUTER_NAME_FORMAT NameType, LPWSTR lpBuffer, LPDWORD nSize);
    void* WINAPI_CALL THETAZ_GetDC(void* hwnd);
    INT WINAPI_CALL THETAZ_GetDateFormatEx(LPCWSTR lpLocaleName, DWORD dwFlags, const SYSTEMTIME* lpDate, LPCWSTR lpFormat, LPWSTR lpDateStr, INT cchDate, void* lpCalendar);
    int WINAPI_CALL THETAZ_GetDeviceCaps(void* hdc, int index);
    UINT WINAPI_CALL THETAZ_GetDriveTypeA(LPCSTR lpRootPathName);
    DWORD WINAPI_CALL THETAZ_GetExitCodeThread(HANDLE hThread, LPDWORD lpExitCode);
    BOOL WINAPI_CALL THETAZ_GetMenuItemInfoW(HWND hMenu, UINT item, BOOL fByPosition, void* lpmii);
    int WINAPI_CALL THETAZ_GetNameInfoW(const SOCKADDR* pSockaddr, size_t SockaddrLength, PWCHAR pNodeBuffer, DWORD NodeBufferSize, PWCHAR pServiceBuffer, DWORD ServiceBufferSize, INT Flags);
    DWORD WINAPI_CALL THETAZ_GetProcessHandleCount(HANDLE hProcess, DWORD* pdwHandleCount);
    HANDLE WINAPI_CALL THETAZ_GetPropA(HWND hWnd, LPCSTR lpString);
    void WINAPI_CALL THETAZ_GetStartupInfoA(void* lpStartupInfo);
    void* WINAPI_CALL THETAZ_GetStockObject(int fnObject);
    int WINAPI_CALL THETAZ_GetSystemMetrics(int index);
    INT WINAPI_CALL THETAZ_GetSystemTimePreciseAsFileTime(void* lpFileTime);
    DWORD WINAPI_CALL THETAZ_PowerDeterminePlatformRole();
    INT WINAPI_CALL THETAZ_GetTimeFormatEx(LPCWSTR lpLocaleName, DWORD dwFlags, const SYSTEMTIME* lpTime, LPCWSTR lpFormat, LPWSTR lpTimeStr, INT cchTime, void* lpCalendar);
    void* WINAPI_CALL THETAZ_GetWindowDC(void* hwnd);
    BOOL WINAPI_CALL THETAZ_InitializeCriticalSectionAndSpinCount(PCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount);
    BOOL WINAPI_CALL THETAZ_IsBadStringPtrW(LPCWSTR lpsz, UINT_PTR ucchMax);
    BOOL WINAPI_CALL THETAZ_IsDBCSLeadByte(BYTE TestChar);
    BOOL WINAPI_CALL THETAZ_IsValidLocaleName(LPCWSTR lpLocaleName);
    int WINAPI_CALL THETAZ_LoadStringA(HINSTANCE hInstance, UINT uID, LPSTR lpBuffer, int cchBufferMax);
    HMONITOR WINAPI_CALL THETAZ_MonitorFromPoint(POINT pt, DWORD dwFlags);
    HMONITOR WINAPI_CALL THETAZ_MonitorFromWindow(HWND hwnd, DWORD dwFlags);
    DWORD WINAPI_CALL THETAZ_MsgWaitForMultipleObjectsEx(DWORD nCount, const HANDLE* pHandles, DWORD dwMilliseconds, DWORD dwWakeMask, DWORD dwFlags);
    BOOL WINAPI_CALL THETAZ_RegCloseKey(HKEY hKey);
    LONG WINAPI_CALL THETAZ_RegCreateKeyW(HKEY hKey, LPCWSTR lpSubKey, PHKEY phkResult);
    LONG WINAPI_CALL THETAZ_RegDeleteValueW(HKEY hKey, LPCWSTR lpValueName);
    LONG WINAPI_CALL THETAZ_RegGetValueW(HKEY hKey, LPCWSTR lpSubKey, LPCWSTR lpValue, DWORD dwFlags, LPDWORD pdwType, void* pvData, LPDWORD pcbData);
    HANDLE WINAPI_CALL THETAZ_RegOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, DWORD samDesired, PHKEY phkResult);
    HANDLE WINAPI_CALL THETAZ_RegOpenKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, DWORD samDesired, PHKEY phkResult);
    ATOM WINAPI_CALL THETAZ_RegisterClassExW(const void* lpwcx);
    UINT WINAPI_CALL THETAZ_RegisterClipboardFormatA(LPCSTR lpszFormat);
    UINT WINAPI_CALL THETAZ_RegisterClipboardFormatW(LPCWSTR lpszFormat);
    UINT WINAPI_CALL THETAZ_RegisterWindowMessageA(LPCSTR lpString);
    int WINAPI_CALL THETAZ_ReleaseDC(void* hwnd, void* hdc);
    BOOL WINAPI_CALL THETAZ_ReleaseSemaphore(HANDLE hSemaphore, LONG lReleaseCount, LONG* lpPreviousCount);
    HANDLE WINAPI_CALL THETAZ_RemovePropA(HWND hWnd, LPCSTR lpString);
    void* WINAPI_CALL THETAZ_RtlCaptureContext(void* pCtx);
    uint64_t WINAPI_CALL THETAZ_RtlLookupFunctionEntry(uint64_t controlPc, uint64_t* imageBase, void* historyTable);
    void* WINAPI_CALL THETAZ_RtlVirtualUnwind(DWORD handlerType, uint64_t imageBase, uint64_t controlPc, void* functionEntry, void* contextRecord, void** handlerData, uint64_t* establisherFrame, void* contextPointers);
    int WINAPI_CALL THETAZ_SHFileOperationW(void* lpFileOp);
    HRESULT WINAPI_CALL THETAZ_SHGetFileInfoW(LPCWSTR pszPath, DWORD dwFileAttributes, void* psfi, UINT cbFileInfo, UINT uFlags);
    HRESULT WINAPI_CALL THETAZ_SHGetSpecialFolderLocation(HWND hwnd, int csidl, void** ppidl);
    void* WINAPI_CALL THETAZ_SelectObject(void* hdc, void* hgdiobj);
    LRESULT WINAPI_CALL THETAZ_SendMessageTimeoutA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam, UINT fuFlags, UINT uTimeout, PDWORD_PTR lpdwResult);
    LRESULT WINAPI_CALL THETAZ_SendMessageTimeoutW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam, UINT fuFlags, UINT uTimeout, PDWORD_PTR lpdwResult);
    DWORD WINAPI_CALL THETAZ_SetBkColor(void* hdc, DWORD color);
    int WINAPI_CALL THETAZ_SetBkMode(void* hdc, int mode);
    void WINAPI_CALL THETAZ_SetDefaultDllDirectories(DWORD DirectoryFlags);
    BOOL WINAPI_CALL THETAZ_SetDlgItemTextW(HWND hDlg, int nIDDlgItem, LPCWSTR lpString);
    void WINAPI_CALL THETAZ_SetProcessDPIAware();
    DWORD WINAPI_CALL THETAZ_SetTextColor(void* hdc, DWORD color);
    HRESULT WINAPI_CALL THETAZ_SetThreadDescription(HANDLE hThread, PCWSTR lpThreadDescription);
    BOOL WINAPI_CALL THETAZ_SystemParametersInfoW(UINT uiAction, UINT uiParam, void* pvParam, UINT fWinIni);
    BOOL WINAPI_CALL THETAZ_TextOutA(void* hdc, int x, int y, const char* lpString, int c);
    BOOL WINAPI_CALL THETAZ_TextOutW(void* hdc, int x, int y, const uint16_t* lpString, int c);
    int WINAPI_CALL THETAZ_GetTextExtentPoint32A(void* hdc, const char* str, int len, void* size);
    int WINAPI_CALL THETAZ_GetTextExtentPoint32W(void* hdc, const uint16_t* str, int len, void* size);
    int WINAPI_CALL THETAZ_GetTextExtentPointA(void* hdc, const char* str, int len, void* size);
    int WINAPI_CALL THETAZ_GetTextExtentPointW(void* hdc, const uint16_t* str, int len, void* size);
    BOOL WINAPI_CALL THETAZ_TryAcquireSRWLockExclusive(void* SRWLock);
    int WINAPI_CALL THETAZ_WSASend(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);
    SOCKET WINAPI_CALL THETAZ_WSASocketW(int af, int type, int protocol, LPWSAPROTOCOL_INFOW lpProtocolInfo, GROUP g, DWORD dwFlags);
    HRESULT WINAPI_CALL THETAZ_WriteClassStg(void* pstg, REFCLSID rclsid);
    HRESULT WINAPI_CALL THETAZ_WriteClassStm(void* pstg, REFCLSID rclsid);
    HRESULT WINAPI_CALL THETAZ_WriteFmtUserTypeStg(void* pstg, void* pfmtetc, DWORD dwUserType, LPWSTR lpszUserType);
    BOOL WINAPI_CALL THETAZ___WSAFDIsSet(SOCKET fd, void* pfdset);
    int WINAPI_CALL THETAZ__dclass(int c);
    int WINAPI_CALL THETAZ__wcslwr(wchar_t* str);
    wchar_t* WINAPI_CALL THETAZ__wgetenv(const wchar_t* varname);
    int WINAPI_CALL THETAZ__wrmdir(const wchar_t* dirname);
    int WINAPI_CALL THETAZ__wsopen_s(int* pfh, const wchar_t* filename, int oflag, int shflag, int pmode);

} // namespace ThetaZ
