// ThetaZ — ntdll.dll API layer (Rtl*/Nt*/Ldr* functions)
// Copyright (c) 2026 ThetaZ Project. MIT License.
//
// ntdll is the NT kernel-mode contract surface: Rtl* (runtime library),
// Nt*/Zw* (system calls) and Ldr* (loader). WINWORD.exe and other modern
// apps import heavily from it. Most of these forward to the existing
// kernel32 implementations (which ntdll normally backs), so the signatures
// must match the NT calling convention (4th-arg-in-ECX for x86 stdcall is
// still just stdcall for us — WINAPI_CALL).

#pragma once

#include "core/api/api.h"
#include "core/api/kernel32.h"

#include <cstdint>
#include <cstddef>

namespace ThetaZ {

// Common NTSTATUS values (severity bit 0x80000000 = failure).
#define NTSTATUS_SUCCESS                ((NTSTATUS)0x00000000L)
#define STATUS_WAIT_0                   ((NTSTATUS)0x00000000L)
#define STATUS_SUCCESS                  ((NTSTATUS)0x00000000L)
#define STATUS_PENDING                  ((NTSTATUS)0x00000103L)
#define STATUS_TIMEOUT                  ((NTSTATUS)0x00000102L)
#define STATUS_INVALID_PARAMETER        ((NTSTATUS)0xC000000DL)
#define STATUS_NOT_FOUND                ((NTSTATUS)0xC0000225L)
#define STATUS_NO_MEMORY                ((NTSTATUS)0xC0000017L)
#define STATUS_ACCESS_DENIED            ((NTSTATUS)0xC0000022L)
#define STATUS_INFO_LENGTH_MISMATCH     ((NTSTATUS)0xC0000004L)
#define STATUS_BUFFER_TOO_SMALL         ((NTSTATUS)0xC0000023L)
#define STATUS_OBJECT_NAME_NOT_FOUND    ((NTSTATUS)0xC0000034L)
#define STATUS_UNSUCCESSFUL             ((NTSTATUS)0xC0000001L)
#define STATUS_NOT_IMPLEMENTED          ((NTSTATUS)0xC0000002L)
#define STATUS_OBJECT_TYPE_MISMATCH     ((NTSTATUS)0xC0000024L)
#define STATUS_INVALID_HANDLE           ((NTSTATUS)0xC0000008L)
#define STATUS_END_OF_FILE              ((NTSTATUS)0xC0000011L)
#define STATUS_INVALID_IMAGE_FORMAT     ((NTSTATUS)0xC000007BL)
#define STATUS_OBJECT_PATH_NOT_FOUND    ((NTSTATUS)0xC000003AL)
#define STATUS_DLL_NOT_FOUND            ((NTSTATUS)0xC0000135L)
#define STATUS_ILLEGAL_FUNCTION         ((NTSTATUS)0xC00000AAL)
#define STATUS_VARIABLE_NOT_FOUND       ((NTSTATUS)0xC0000100L)

// UNICODE_STRING / ANSI_STRING (NT layout, also used by the loader).
typedef struct _STRING {
    USHORT Length;
    USHORT MaximumLength;
    char*  Buffer;
} STRING, *PSTRING, ANSI_STRING, *PANSI_STRING;

typedef struct _UNICODE_STRING {
    USHORT  Length;
    USHORT  MaximumLength;
    PWSTR   Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

// RTL_OSVERSIONINFOW / RTL_OSVERSIONINFOEXW (fixed 32/64-bit layouts).
typedef struct _RTL_OSVERSIONINFOW {
    ULONG dwOSVersionInfoSize;
    ULONG dwMajorVersion;
    ULONG dwMinorVersion;
    ULONG dwBuildNumber;
    ULONG dwPlatformId;
    WCHAR szCSDVersion[128];
} RTL_OSVERSIONINFOW, *PRTL_OSVERSIONINFOW;

typedef struct _RTL_OSVERSIONINFOEXW {
    ULONG dwOSVersionInfoSize;
    ULONG dwMajorVersion;
    ULONG dwMinorVersion;
    ULONG dwBuildNumber;
    ULONG dwPlatformId;
    WCHAR szCSDVersion[128];
    USHORT wServicePackMajor;
    USHORT wServicePackMinor;
    USHORT wSuiteMask;
    UCHAR wProductType;
    UCHAR wReserved;
} RTL_OSVERSIONINFOEXW, *PRTL_OSVERSIONINFOEXW;

// ---------------------------------------------------------------------------
// Rtl* runtime library
// ---------------------------------------------------------------------------
void* WINAPI_CALL NTDLL_RtlAllocateHeap(void* HeapHandle, ULONG Flags, SIZE_T Size);
BOOL  WINAPI_CALL NTDLL_RtlFreeHeap(void* HeapHandle, ULONG Flags, void* BaseAddress);
void* WINAPI_CALL NTDLL_RtlReAllocateHeap(void* HeapHandle, ULONG Flags, void* BaseAddress, SIZE_T Size);
SIZE_T WINAPI_CALL NTDLL_RtlSizeHeap(void* HeapHandle, ULONG Flags, void* BaseAddress);
void* WINAPI_CALL NTDLL_RtlCreateHeap(ULONG Flags, void* BaseAddress, SIZE_T ReserveSize,
                                      SIZE_T CommitSize, void* Lock, void* Parameters);
void* WINAPI_CALL NTDLL_RtlDestroyHeap(void* HeapHandle);
SIZE_T WINAPI_CALL NTDLL_RtlCompactHeap(void* HeapHandle, ULONG Flags);
SIZE_T WINAPI_CALL NTDLL_RtlGetProcessHeaps(ULONG NumberOfHeaps, void** ProcessHeaps);

void  WINAPI_CALL NTDLL_RtlInitializeCriticalSection(void* lpCriticalSection);
void  WINAPI_CALL NTDLL_RtlDeleteCriticalSection(void* lpCriticalSection);
void  WINAPI_CALL NTDLL_RtlEnterCriticalSection(void* lpCriticalSection);
void  WINAPI_CALL NTDLL_RtlLeaveCriticalSection(void* lpCriticalSection);
BOOL  WINAPI_CALL NTDLL_RtlTryEnterCriticalSection(void* lpCriticalSection);
BOOL  WINAPI_CALL NTDLL_RtlInitializeCriticalSectionAndSpinCount(void* lpCriticalSection, ULONG dwSpinCount);
BOOL  WINAPI_CALL NTDLL_RtlSetCriticalSectionSpinCount(void* lpCriticalSection, ULONG dwSpinCount);

void  WINAPI_CALL NTDLL_RtlAcquireSRWLockExclusive(void* SRWLock);
void  WINAPI_CALL NTDLL_RtlAcquireSRWLockShared(void* SRWLock);
void  WINAPI_CALL NTDLL_RtlReleaseSRWLockExclusive(void* SRWLock);
void  WINAPI_CALL NTDLL_RtlReleaseSRWLockShared(void* SRWLock);

void* WINAPI_CALL NTDLL_RtlGetCurrentPeb();
void* WINAPI_CALL NTDLL_RtlGetCurrentTeb();

void  WINAPI_CALL NTDLL_RtlGetLastWin32Error();
void  WINAPI_CALL NTDLL_RtlSetLastWin32Error(DWORD dwErrCode);

NTSTATUS WINAPI_CALL NTDLL_RtlGetVersion(PRTL_OSVERSIONINFOW lpVersionInformation);
ULONGLONG WINAPI_CALL NTDLL_RtlGetSystemTimePrecise();
NTSTATUS WINAPI_CALL NTDLL_RtlNtStatusToDosError(NTSTATUS Status);

void  WINAPI_CALL NTDLL_RtlZeroMemory(void* dst, SIZE_T len);
void  WINAPI_CALL NTDLL_RtlFillMemory(void* dst, SIZE_T len, BYTE fill);
void  WINAPI_CALL NTDLL_RtlCopyMemory(void* dst, const void* src, SIZE_T len);
void  WINAPI_CALL NTDLL_RtlMoveMemory(void* dst, const void* src, SIZE_T len);
SIZE_T WINAPI_CALL NTDLL_RtlCompareMemory(const void* a, const void* b, SIZE_T len);
BOOL  WINAPI_CALL NTDLL_RtlEqualMemory(const void* a, const void* b, SIZE_T len);

void  WINAPI_CALL NTDLL_RtlInitUnicodeString(PUNICODE_STRING dest, const WCHAR* src);
void  WINAPI_CALL NTDLL_RtlInitAnsiString(PSTRING dest, const char* src);
BOOL  WINAPI_CALL NTDLL_RtlCreateUnicodeStringFromAsciiz(PUNICODE_STRING dest, const char* src);
void  WINAPI_CALL NTDLL_RtlFreeUnicodeString(PUNICODE_STRING str);
void  WINAPI_CALL NTDLL_RtlFreeAnsiString(PSTRING str);
NTSTATUS WINAPI_CALL NTDLL_RtlUnicodeStringToAnsiString(PSTRING dest, const PUNICODE_STRING src, BOOL alloc);
NTSTATUS WINAPI_CALL NTDLL_RtlAnsiStringToUnicodeString(PUNICODE_STRING dest, const PSTRING src, BOOL alloc);
void  WINAPI_CALL NTDLL_RtlCopyUnicodeString(PUNICODE_STRING dest, const PUNICODE_STRING src);

void  WINAPI_CALL NTDLL_RtlGenRandom(void* buffer, ULONG length);
ULONG WINAPI_CALL NTDLL_RtlRandom(unsigned long* seed);
USHORT WINAPI_CALL NTDLL_RtlDowncaseUnicodeChar(USHORT ch);

NTSTATUS WINAPI_CALL NTDLL_RtlImageNtHeader(void* BaseAddress, void** NtHeader);
NTSTATUS WINAPI_CALL NTDLL_RtlImageDirectoryEntryToData(void* BaseAddress, BOOL mapped, USHORT directory, ULONG* size);
NTSTATUS WINAPI_CALL NTDLL_RtlFindExportedRoutineByName(void* imageBase, const char* name, void** routine);
NTSTATUS WINAPI_CALL NTDLL_LdrGetProcedureAddress(void* ModuleHandle, void* procName, USHORT ordinal, void** procAddress);
NTSTATUS WINAPI_CALL NTDLL_RtlRaiseException(void* ExceptionRecord);
void  WINAPI_CALL NTDLL_RtlRaiseStatus(NTSTATUS Status);

// ---------------------------------------------------------------------------
// Nt* system calls (thin wrappers over kernel32)
// ---------------------------------------------------------------------------
NTSTATUS WINAPI_CALL NTDLL_NtClose(void* handle);
NTSTATUS WINAPI_CALL NTDLL_NtQuerySystemTime(ULONGLONG* SystemTime);
NTSTATUS WINAPI_CALL NTDLL_NtQueryPerformanceCounter(void* lpPerformanceCount, void* lpFrequency);
NTSTATUS WINAPI_CALL NTDLL_NtGetTickCount();
NTSTATUS WINAPI_CALL NTDLL_NtDelayExecution(BOOL alertable, void* delayInterval);
NTSTATUS WINAPI_CALL NTDLL_NtYieldExecution();
NTSTATUS WINAPI_CALL NTDLL_NtTerminateProcess(void* processHandle, LONG exitStatus);
NTSTATUS WINAPI_CALL NTDLL_NtTerminateThread(void* threadHandle, LONG exitStatus);
NTSTATUS WINAPI_CALL NTDLL_NtCurrentTeb();
NTSTATUS WINAPI_CALL NTDLL_NtQueryInformationProcess(void* processHandle, ULONG infoClass,
                                                    void* info, ULONG len, ULONG* retLen);
NTSTATUS WINAPI_CALL NTDLL_NtQuerySystemInformation(ULONG infoClass, void* info,
                                                    ULONG len, ULONG* retLen);
NTSTATUS WINAPI_CALL NTDLL_NtQueryInformationThread(void* threadHandle, ULONG infoClass,
                                                    void* info, ULONG len, ULONG* retLen);
NTSTATUS WINAPI_CALL NTDLL_NtSetInformationThread(void* threadHandle, ULONG infoClass,
                                                  void* info, ULONG len);
NTSTATUS WINAPI_CALL NTDLL_NtSuspendThread(void* threadHandle, ULONG* previousCount);
NTSTATUS WINAPI_CALL NTDLL_NtResumeThread(void* threadHandle, ULONG* previousCount);
NTSTATUS WINAPI_CALL NTDLL_NtQueryVirtualMemory(void* processHandle, const void* baseAddr,
                                                ULONG infoClass, void* info, SIZE_T len, SIZE_T* retLen);
NTSTATUS WINAPI_CALL NTDLL_NtProtectVirtualMemory(void* processHandle, void** baseAddr,
                                                  SIZE_T* regionSize, ULONG newProtect, ULONG* oldProtect);
NTSTATUS WINAPI_CALL NTDLL_NtAllocateVirtualMemory(void* processHandle, void** baseAddr,
                                                   ULONG_PTR zeroBits, SIZE_T* regionSize,
                                                   ULONG allocType, ULONG protect);
NTSTATUS WINAPI_CALL NTDLL_NtFreeVirtualMemory(void* processHandle, void** baseAddr,
                                               SIZE_T* regionSize, ULONG freeType);
NTSTATUS WINAPI_CALL NTDLL_NtCreateEvent(void* eventHandle, ULONG access, void* attrs,
                                         ULONG type, BOOLEAN initialState, const WCHAR* name);
NTSTATUS WINAPI_CALL NTDLL_NtOpenEvent(void* eventHandle, ULONG access, void* attrs, void* name);
NTSTATUS WINAPI_CALL NTDLL_NtSetEvent(void* eventHandle, ULONG* previousState);
NTSTATUS WINAPI_CALL NTDLL_NtResetEvent(void* eventHandle, ULONG* previousState);
NTSTATUS WINAPI_CALL NTDLL_NtPulseEvent(void* eventHandle, ULONG* previousState);
NTSTATUS WINAPI_CALL NTDLL_NtWaitForSingleObject(void* handle, BOOL alertable, void* timeout);
NTSTATUS WINAPI_CALL NTDLL_NtWaitForMultipleObjects(ULONG count, void** handles, ULONG waitType,
                                                    BOOL alertable, void* timeout);
NTSTATUS WINAPI_CALL NTDLL_NtCreateMutant(void* mutantHandle, ULONG access, void* attrs, BOOLEAN initial);
NTSTATUS WINAPI_CALL NTDLL_NtOpenMutant(void* mutantHandle, ULONG access, void* attrs, void* name);
NTSTATUS WINAPI_CALL NTDLL_NtReleaseMutant(void* mutantHandle, LONG* previousCount);
NTSTATUS WINAPI_CALL NTDLL_NtCreateSemaphore(void* semHandle, ULONG access, void* attrs,
                                             LONG initial, LONG maximum, const WCHAR* name);
NTSTATUS WINAPI_CALL NTDLL_NtOpenSemaphore(void* semHandle, ULONG access, void* attrs, void* name);
NTSTATUS WINAPI_CALL NTDLL_NtReleaseSemaphore(void* semHandle, LONG releaseCount, LONG* prevCount);
NTSTATUS WINAPI_CALL NTDLL_NtCreateTimer(void* timerHandle, ULONG access, void* attrs, ULONG type);
NTSTATUS WINAPI_CALL NTDLL_NtOpenTimer(void* timerHandle, ULONG access, void* attrs, void* name);
NTSTATUS WINAPI_CALL NTDLL_NtSetTimer(void* timerHandle, void* dueTime, void* timerApc,
                                      void* timerContext, BOOL resume, LONG period, void* state);
NTSTATUS WINAPI_CALL NTDLL_NtCancelTimer(void* timerHandle, BOOL* state);
NTSTATUS WINAPI_CALL NTDLL_NtQueryObject(void* objectHandle, ULONG infoClass, void* info,
                                         ULONG len, ULONG* retLen);
NTSTATUS WINAPI_CALL NTDLL_NtCreateFile(void* fileHandle, ULONG access, void* objAttrs,
                                        void* ioStatus, void* allocSize, ULONG fileAttrs,
                                        ULONG shareAccess, ULONG openDisposition, ULONG createOptions,
                                        void* eaBuffer, ULONG eaLength);
NTSTATUS WINAPI_CALL NTDLL_NtOpenFile(void* fileHandle, ULONG access, void* objAttrs,
                                      void* ioStatus, ULONG shareAccess, ULONG openOptions);
NTSTATUS WINAPI_CALL NTDLL_NtReadFile(void* fileHandle, void* event, void* apc, void* apcCtx,
                                      void* ioStatus, void* buffer, ULONG length, void* byteOffset, void* key);
NTSTATUS WINAPI_CALL NTDLL_NtWriteFile(void* fileHandle, void* event, void* apc, void* apcCtx,
                                       void* ioStatus, void* buffer, ULONG length, void* byteOffset, void* key);
NTSTATUS WINAPI_CALL NTDLL_NtQueryInformationFile(void* fileHandle, void* ioStatus,
                                                  void* info, ULONG len, ULONG infoClass);
NTSTATUS WINAPI_CALL NTDLL_NtQueryAttributesFile(void* objAttrs, void* fileInfo);
NTSTATUS WINAPI_CALL NTDLL_NtQueryFullAttributesFile(void* objAttrs, void* fileInfo);
NTSTATUS WINAPI_CALL NTDLL_NtDeviceIoControlFile(void* fileHandle, void* event, void* apc, void* apcCtx,
                                                 void* ioStatus, ULONG ioControlCode, void* inBuf,
                                                 ULONG inLen, void* outBuf, ULONG outLen);
NTSTATUS WINAPI_CALL NTDLL_NtFlushBuffersFile(void* fileHandle, void* ioStatus);
NTSTATUS WINAPI_CALL NTDLL_NtDuplicateObject(void* sourceProcess, void* sourceHandle,
                                             void* targetProcess, void** targetHandle,
                                             ULONG access, ULONG attrs, ULONG options);
 NTSTATUS WINAPI_CALL NTDLL_NtQuerySystemTime2(ULONGLONG* SystemTime);
 NTSTATUS WINAPI_CALL NTDLL_NtOpenProcess(void* processHandle, ULONG access, void* attrs, void* cid);
 NTSTATUS WINAPI_CALL NTDLL_NtOpenThread(void* threadHandle, ULONG access, void* attrs, void* cid);
 NTSTATUS WINAPI_CALL NTDLL_NtGetContextThread(void* threadHandle, void* context);
 NTSTATUS WINAPI_CALL NTDLL_NtSetContextThread(void* threadHandle, void* context);

// ---------------------------------------------------------------------------
// Ldr* loader functions
// ---------------------------------------------------------------------------
NTSTATUS WINAPI_CALL NTDLL_LdrLoadDll(const WCHAR* pathToFile, void* flags, const PUNICODE_STRING moduleName, void** moduleHandle);
NTSTATUS WINAPI_CALL NTDLL_LdrUnloadDll(void* moduleHandle);
NTSTATUS WINAPI_CALL NTDLL_LdrGetDllHandle(const WCHAR* baseName, void* flags, const PUNICODE_STRING moduleName, void** moduleHandle);
NTSTATUS WINAPI_CALL NTDLL_LdrAddRefDll(ULONG flags, void* moduleHandle);
NTSTATUS WINAPI_CALL NTDLL_LdrDisableThreadCalloutsForDll(void* moduleHandle);
NTSTATUS WINAPI_CALL NTDLL_LdrLockLoaderLock(ULONG flags, ULONG* disposed, void** cookie);
NTSTATUS WINAPI_CALL NTDLL_LdrUnlockLoaderLock(ULONG flags, void* cookie);
NTSTATUS WINAPI_CALL NTDLL_LdrGetDllFullName(void* moduleHandle, PUNICODE_STRING fullName);
NTSTATUS WINAPI_CALL NTDLL_LdrGetDllPath(const WCHAR* name, ULONG flags, PUNICODE_STRING path);
NTSTATUS WINAPI_CALL NTDLL_LdrSetDllDirectory(const WCHAR* path);
NTSTATUS WINAPI_CALL NTDLL_LdrGetDllDirectory(PUNICODE_STRING path);
NTSTATUS WINAPI_CALL NTDLL_LdrResolveDelayLoadedAPI(void* parent, void* delayload, void* failureDllHook,
                                                    void* failureSystemHook, void* thunkAddress, void* flags);
NTSTATUS WINAPI_CALL NTDLL_LdrAddDllDirectory(const WCHAR* path, void** cookie);
NTSTATUS WINAPI_CALL NTDLL_LdrRemoveDllDirectory(void* cookie);
NTSTATUS WINAPI_CALL NTDLL_LdrSetDefaultDllDirectories(ULONG flags);

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------
void initialize_ntdll_api();

} // namespace ThetaZ
