// ThetaZ — runtime kernel32 APIs (CRT / loader / file / console / sync);
// Copyright (c) 2026 ThetaZ Project. MIT License.

#include "core/api/api.h"
#include "core/api/kernel32.h"
#include "core/api/environment.h"
#include "core/exception/exception.h"
#include "core/loader/loader.h"
#include "core/ipc/thetaserver.h"
#include "core/ntdll/thread.h"
#include "core/api/path_resolver.h"
#include "core/x86/api_dispatch.h"
#include "core/x86/x86.h"
#include "core/api/window.h"
#include "core/ntdll/virtual.h"
#include "core/api/oleaut32.h"
#include "core/api/crypto.h"
#include <cwctype>
#include <wctype.h>
#include <fnmatch.h>

struct WOW64CONTEXT;

#include <cstdio>
#include <map>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cstdarg>
#include <cctype>
#include <algorithm>
#include <ctime>
#include <climits>
#include <cwchar>
#include <wctype.h>
#include <string>
#include <thread>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <atomic>
#include <condition_variable>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>

// Pull ThetaZ symbols into global scope for visibility inside namespace block
using ThetaZ::THETAZ_GetDlgItem;
using ThetaZ::THETAZ_SendMessageA;
using ThetaZ::THETAZ_PostMessageA;
using ThetaZ::THETAZ_DialogBoxParamA;
using ThetaZ::WM_SETTEXT;
using ThetaZ::WM_GETTEXT;
using ThetaZ::x86_dispatch_active;
using ThetaZ::x86_call_guest_args_ebp;
using ThetaZ::WINAPI64_SetDlgItemTextA;
using ThetaZ::WINAPI64_ShowWindow;
using ThetaZ::WINAPI64_SetWindowTextA;
using ThetaZ::WINAPI64_TranslateMessage;
using ThetaZ::WINAPI64_UpdateWindow;
using ThetaZ::g_window_mutex;
using ThetaZ::find_window;
using ThetaZ::g_x11_windows;
using ThetaZ::g_x11_display;
using ThetaZ::g_x11_mutex;
using ThetaZ::ApiRegistry;
using ThetaZ::PCONDITION_VARIABLE;
using ThetaZ::PCRITICAL_SECTION;
using ThetaZ::free_bstr;
using ThetaZ::THETAZ_SysAllocString;
using ThetaZ::THETAZ_SysAllocStringLen;
using ThetaZ::bstr_byte_len;
using ThetaZ::VT_BSTR;
using ThetaZ::THETAZ_VariantCopy;
using ThetaZ::SYSTEMTIME;
using ThetaZ::WindowsAddressSpace;
using ThetaZ::MemoryBasicInformation;
using ThetaZ::MEM_RELEASE;
using ThetaZ::MEM_DECOMMIT;
using ThetaZ::MSG_MESSAGE;
using ThetaZ::WM_KEYDOWN;
using ThetaZ::WM_CHAR;
using ThetaZ::MSG_WPARAM;
using ThetaZ::MSG_HWND;
using ThetaZ::bstr_byte_len;
using ThetaZ::VT_BSTR;
using ThetaZ::MSG_HWND;
using ThetaZ::MSG_MESSAGE;
using ThetaZ::MSG_WPARAM;
using ThetaZ::WM_KEYDOWN;
using ThetaZ::WM_CHAR;

bool query_from_proc_maps(const void* addr, MemoryBasicInformation* out);

namespace ThetaZ {

// wstrcmpi helper for case-insensitive wide string comparison
static inline int wstrcmpi(const uint16_t* s1, const uint16_t* s2) {
    if (!s1 && !s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;
    while (*s1 && *s2) {
        uint16_t c1 = (*s1 >= 'A' && *s1 <= 'Z') ? *s1 + 32 : *s1;
        uint16_t c2 = (*s2 >= 'A' && *s2 <= 'Z') ? *s2 + 32 : *s2;
        if (c1 != c2) return c1 < c2 ? -1 : 1;
        s1++; s2++;
    }
if (*s1) return 1;
    if (*s2) return -1;
    return 0;
}

// Forward declarations for cross-file functions
    std::string resolve_target(const char* application, const char* commandline);
    uint64_t alloc_child_pid();
    std::string wchar_to_utf8(const WCHAR* s);

// Forward declarations
    void WINAPI_CALL THETAZ_AcquireSRWLockExclusive(void* SRWLock);
    void WINAPI_CALL THETAZ_AcquireSRWLockShared(void* SRWLock);
    PVOID WINAPI_CALL THETAZ_AddVectoredContinueHandler(ULONG first, PVECTORED_EXCEPTION_HANDLER handler);
    PVOID WINAPI_CALL THETAZ_AddVectoredExceptionHandler(ULONG first, PVECTORED_EXCEPTION_HANDLER handler);
    BOOL WINAPI_CALL THETAZ_AllocConsole();
    BOOL WINAPI_CALL THETAZ_AreFileApisANSI();
    BOOL WINAPI_CALL THETAZ_AttachConsole(DWORD dwProcessId);
    BOOL WINAPI_CALL THETAZ_Beep(DWORD dwFreq, DWORD dwDuration);
    BOOL WINAPI_CALL THETAZ_BeginPaint(void* hwnd, void* ps);
    BOOL WINAPI_CALL THETAZ_BitBlt(void* hdcDest, int x, int y, int w, int h, void* hdcSrc, int x1, int y1);
HRESULT WINAPI_CALL THETAZ_CLSIDFromProgID(LPCOLESTR lpszProgID, LPCLSID lpclsid);
    HRESULT WINAPI_CALL THETAZ_CLSIDFromString(LPCOLESTR lpsz, LPCLSID pclsid);
    BOOL WINAPI_CALL THETAZ_CallNamedPipeA(LPCSTR lpNamedPipeName, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesRead, DWORD nTimeout);
    BOOL WINAPI_CALL THETAZ_CallNamedPipeW(LPCWSTR lpNamedPipeName, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesRead, DWORD nTimeout);
    BOOL WINAPI_CALL THETAZ_CancelIoEx(HANDLE hFile, void* lpOverlapped);
    BOOL WINAPI_CALL THETAZ_CancelWaitableTimer(HANDLE hTimer);
    LPCSTR WINAPI_CALL THETAZ_CharNextA(LPCSTR lpsz);
    BOOL WINAPI_CALL THETAZ_CheckRemoteDebuggerPresent(HANDLE hProcess, PBOOL pbDebuggerPresent);
    BOOL WINAPI_CALL THETAZ_CloseHandle(HANDLE hObject);
    BOOL WINAPI_CALL THETAZ_CloseFileHandle(HANDLE h);
    void* WINAPI_CALL THETAZ_SetUnhandledExceptionFilter(void* filter);
    HRESULT WINAPI_CALL THETAZ_CoCreateInstance(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID* ppv);
    HRESULT WINAPI_CALL THETAZ_CoGetClassObject(REFCLSID rclsid, DWORD dwClsContext, LPVOID pvReserved, REFIID riid, LPVOID* ppv);
    HRESULT WINAPI_CALL THETAZ_CoInitialize(LPVOID pvReserved);
    void WINAPI_CALL THETAZ_CoUninitialize();
    INT WINAPI_CALL THETAZ_CompareStringEx(LPCWSTR lpLocaleName, DWORD dwCmpFlags, LPCWSTR lpString1, INT cchCount1, LPCWSTR lpString2, INT cchCount2, void* lpVersionInformation, void* lpReserved, void* lParam);
    INT WINAPI_CALL THETAZ_CompareStringW(DWORD Locale, DWORD dwCmpFlags, LPCWSTR lpString1, INT cchCount1, LPCWSTR lpString2, INT cchCount2);
    BOOL WINAPI_CALL THETAZ_CopyFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, BOOL bFailIfExists);
    BOOL WINAPI_CALL THETAZ_CopyFileExW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, void* lpProgressRoutine, void* lpData, BOOL* pbCancel, DWORD dwCopyFlags);
    BOOL WINAPI_CALL THETAZ_CopyFileW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, BOOL bFailIfExists);
    BOOL WINAPI_CALL THETAZ_CreateDirectoryW(LPCWSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);
    HANDLE WINAPI_CALL THETAZ_CreateEventA(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialOwner, LPCSTR lpName);
    HANDLE WINAPI_CALL THETAZ_CreateEventExW(LPSECURITY_ATTRIBUTES lpEventAttributes, LPCWSTR lpName, DWORD dwFlags, DWORD dwDesiredAccess);
    HANDLE WINAPI_CALL THETAZ_CreateEventW(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCWSTR lpName);
    HANDLE WINAPI_CALL THETAZ_CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                                          LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
                                          DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
    HANDLE WINAPI_CALL THETAZ_CreateFileMappingA(HANDLE hFile, LPSECURITY_ATTRIBUTES lpAttributes, DWORD flProtect, DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCSTR lpName);
    HANDLE WINAPI_CALL THETAZ_CreateFileMappingW(HANDLE hFile, LPSECURITY_ATTRIBUTES lpAttributes, DWORD flProtect, DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCWSTR lpName);
    HANDLE WINAPI_CALL THETAZ_CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                                          LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
                                          DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
    HANDLE WINAPI_CALL THETAZ_CreateIoCompletionPort(HANDLE hFile, HANDLE hExistingCompletionPort, uintptr_t CompletionKey, DWORD dwNumberOfConcurrentThreads);
    HANDLE WINAPI_CALL THETAZ_CreateMemoryResourceNotification(DWORD NotificationType);
    HANDLE WINAPI_CALL THETAZ_CreateMutexA(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCSTR lpName);
    HANDLE WINAPI_CALL THETAZ_CreateMutexExW(LPSECURITY_ATTRIBUTES lpMutexAttributes, LPCWSTR lpName, DWORD dwFlags, DWORD dwDesiredAccess);
    HANDLE WINAPI_CALL THETAZ_CreateMutexW(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCWSTR lpName);
    BOOL WINAPI_CALL THETAZ_CreateNamedPipeA(LPCSTR lpName, DWORD dwOpenMode, DWORD dwPipeMode, DWORD nMaxInstances,
                                              DWORD nOutBufferSize, DWORD nInBufferSize, DWORD nDefaultTimeOut,
                                              LPSECURITY_ATTRIBUTES lpSecurityAttributes);
    BOOL WINAPI_CALL THETAZ_CreateNamedPipeW(LPCWSTR lpName, DWORD dwOpenMode, DWORD dwPipeMode, DWORD nMaxInstances,
                                              DWORD nOutBufferSize, DWORD nInBufferSize, DWORD nDefaultTimeOut,
                                              LPSECURITY_ATTRIBUTES lpSecurityAttributes);
    HANDLE WINAPI_CALL THETAZ_CreateSemaphoreA(LPSECURITY_ATTRIBUTES lpSemaphoreAttributes, LONG lInitialCount, LONG lMaximumCount, LPCSTR lpName);
    BOOL WINAPI_CALL THETAZ_CreateSymbolicLinkW(LPCWSTR lpSymlinkFileName, LPCWSTR lpTargetFileName, DWORD dwFlags);
    HANDLE WINAPI_CALL THETAZ_CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize, LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId);
    HANDLE WINAPI_CALL THETAZ_CreateTimerQueue();
    BOOL WINAPI_CALL THETAZ_CreateTimerQueueTimer(HANDLE* phNewTimer, HANDLE TimerQueue, void* Callback, void* Parameter, DWORD DueTime, DWORD Period, ULONG Flags);
    HANDLE WINAPI_CALL THETAZ_CreateWaitableTimerW(LPSECURITY_ATTRIBUTES lpTimerAttributes, BOOL bManualReset, LPCWSTR lpTimerName);
    LPVOID WINAPI_CALL THETAZ_DecodePointer(LPCVOID Ptr);
    void WINAPI_CALL THETAZ_DeleteConditionVariable(PCONDITION_VARIABLE ConditionVariable);
    void WINAPI_CALL THETAZ_DeleteCriticalSection(PCRITICAL_SECTION lpCriticalSection);
    BOOL WINAPI_CALL THETAZ_DeleteDC(void* hdc);
    BOOL WINAPI_CALL THETAZ_DeleteFileA(LPCSTR lpFileName);
    BOOL WINAPI_CALL THETAZ_DeleteFileW(LPCWSTR lpFileName);
    BOOL WINAPI_CALL THETAZ_DeleteObject(void* hobj);
    BOOL WINAPI_CALL THETAZ_DeleteTimerQueue(HANDLE TimerQueue);
    BOOL WINAPI_CALL THETAZ_DeleteTimerQueueTimer(HANDLE TimerQueue, HANDLE Timer, HANDLE CompletionEvent);
    BOOL WINAPI_CALL THETAZ_DeviceIoControl(HANDLE hDevice, DWORD dwIoControlCode, void* lpInBuffer, DWORD nInBufferSize, void* lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, void* lpOverlapped);
    BOOL WINAPI_CALL THETAZ_DisableThreadLibraryCalls(HMODULE hModule);
    BOOL WINAPI_CALL THETAZ_DuplicateHandle(HANDLE hSourceProcessHandle, HANDLE hSourceHandle, HANDLE hTargetProcessHandle, HANDLE* lpTargetHandle, DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwOptions);
    BOOL WINAPI_CALL THETAZ_EnableMenuItem(HMENU hMenu, UINT uIDEnableItem, UINT uEnable);
    LPVOID WINAPI_CALL THETAZ_EncodePointer(LPCVOID Ptr);
    BOOL WINAPI_CALL THETAZ_EndPaint(void* hwnd, void* ps);
    void WINAPI_CALL THETAZ_EnterCriticalSection(PCRITICAL_SECTION lpCriticalSection);
    BOOL WINAPI_CALL THETAZ_EnumSystemLocalesEx(LOCALE_ENUMPROCEX lpLocaleEnumProcEx, DWORD dwFlags, uintptr_t lParam, void* lpReserved);
    BOOL WINAPI_CALL THETAZ_EnumSystemLocalesW(LOCALE_ENUMPROCW lpLocaleEnumProc, DWORD dwFlags);
    void WINAPI_CALL THETAZ_ExitProcess(uint32_t exit_code);
    void WINAPI_CALL THETAZ_ExitThread(DWORD dwExitCode);
    DWORD WINAPI_CALL THETAZ_ExpandEnvironmentStringsW(LPCWSTR lpSrc, LPWSTR lpDst, DWORD nSize);
    BOOL WINAPI_CALL THETAZ_FileTimeToSystemTime(const FILETIME* lpFileTime, SYSTEMTIME* lpSystemTime);
    BOOL WINAPI_CALL THETAZ_FindClose(HANDLE hFindFile);
    HANDLE WINAPI_CALL THETAZ_FindFirstFileA(LPCSTR lpFileName, void* lpFindFileData);
    HANDLE WINAPI_CALL THETAZ_FindFirstFileExW(LPCWSTR lpFileName, void* fInfoLevelId, WIN32_FIND_DATAW* lpFindFileData, void* fSearchOp, void* lpSearchFilter, DWORD dwAdditionalFlags);
    HANDLE WINAPI_CALL THETAZ_FindFirstFileW(LPCWSTR lpFileName, WIN32_FIND_DATAW* lpFindFileData);
    BOOL WINAPI_CALL THETAZ_FindNextFileA(HANDLE hFindFile, void* lpFindFileData);
    BOOL WINAPI_CALL THETAZ_FindNextFileW(HANDLE hFindFile, WIN32_FIND_DATAW* lpFindFileData);
    DWORD WINAPI_CALL THETAZ_FlsAlloc(void* lpCallback);
    BOOL WINAPI_CALL THETAZ_FlsFree(DWORD dwFlsIndex);
    LPVOID WINAPI_CALL THETAZ_FlsGetValue(DWORD dwFlsIndex);
    LPVOID WINAPI_CALL THETAZ_FlsGetValue2(DWORD dwFlsIndex);
    BOOL WINAPI_CALL THETAZ_FlsSetValue(DWORD dwFlsIndex, LPVOID lpFlsData);
    BOOL WINAPI_CALL THETAZ_FlushFileBuffers(HANDLE hFile);
    BOOL WINAPI_CALL THETAZ_FlushViewOfFile(LPCVOID lpBaseAddress, SIZE_T dwNumberOfBytesToFlush);
    DWORD WINAPI_CALL THETAZ_FormatMessageA(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId, DWORD dwLanguageId, LPSTR lpBuffer, DWORD nSize, void* Arguments);
    DWORD WINAPI_CALL THETAZ_FormatMessageW(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId, DWORD dwLanguageId, LPWSTR lpBuffer, DWORD nSize, void* Arguments);
    BOOL WINAPI_CALL THETAZ_FreeConsole();
    BOOL WINAPI_CALL THETAZ_FreeEnvironmentStringsA(LPSTR lpszEnvironmentBlock);
    BOOL WINAPI_CALL THETAZ_FreeEnvironmentStringsW(LPWSTR lpszEnvironmentBlock);
    void* WINAPI_CALL THETAZ_GetEnvironmentStringsW();
    BOOL WINAPI_CALL THETAZ_FreeLibrary(HMODULE hLibModule);
    void WINAPI_CALL THETAZ_FreeLibraryAndExitThread(HMODULE hLibModule, DWORD dwExitCode);
    BOOL WINAPI_CALL THETAZ_FreeResource(void* hResData);
    DWORD WINAPI_CALL THETAZ_GetACP();
    int WINAPI_CALL THETAZ_GetAddrInfoW(const wchar_t* pNodeName, const wchar_t* pServiceName, const void* pHints, void** ppResult);
    BOOL WINAPI_CALL THETAZ_GetCPInfo(UINT CodePageID, CPINFO* lpCPInfo);
    BOOL WINAPI_CALL THETAZ_GetCPInfoExW(UINT CodePageID, DWORD dwFlags, CPINFOEXW* lpCPInfoEx);
    BOOL WINAPI_CALL THETAZ_GetClassInfoW(HINSTANCE hInstance, LPCWSTR lpClassName, LPWNDCLASSW lpWndClass);
    DWORD WINAPI_CALL THETAZ_GetClassLongPtrW(HWND hWnd, int nIndex);
    BOOL WINAPI_CALL THETAZ_GetClientRect(void* hwnd, void* rect);
    LPSTR WINAPI_CALL THETAZ_GetCommandLineA();
    LPWSTR WINAPI_CALL THETAZ_GetCommandLineW();
    BOOL WINAPI_CALL THETAZ_GetComputerNameExW(COMPUTER_NAME_FORMAT NameType, LPWSTR lpBuffer, LPDWORD nSize);
    BOOL WINAPI_CALL THETAZ_GetComputerNameW(LPWSTR lpBuffer, LPDWORD nSize);
    UINT WINAPI_CALL THETAZ_GetConsoleCP();
    BOOL WINAPI_CALL THETAZ_GetConsoleMode(HANDLE hConsoleHandle, LPDWORD lpMode);
    UINT WINAPI_CALL THETAZ_GetConsoleOutputCP();
    DWORD WINAPI_CALL THETAZ_GetCurrentDirectoryA(DWORD nBufferLength, LPSTR lpBuffer);
    DWORD WINAPI_CALL THETAZ_GetCurrentDirectoryW(DWORD nBufferLength, LPWSTR lpBuffer);
    HANDLE WINAPI_CALL THETAZ_GetCurrentProcess();
    DWORD WINAPI_CALL THETAZ_GetCurrentProcessId();
    HANDLE WINAPI_CALL THETAZ_GetCurrentThread();
    DWORD WINAPI_CALL THETAZ_GetCurrentThreadId();
    INT WINAPI_CALL THETAZ_GetDateFormatEx(LPCWSTR lpLocaleName, DWORD dwFlags, const SYSTEMTIME* lpDate, LPCWSTR lpFormat, LPWSTR lpDateStr, INT cchDate, void* lpCalendar);
    INT WINAPI_CALL THETAZ_GetDateFormatW(DWORD Locale, DWORD dwFlags, const SYSTEMTIME* lpDate, LPCWSTR lpFormat, LPWSTR lpDateStr, INT cchDate);
    int WINAPI_CALL THETAZ_GetDeviceCaps(void* hdc, int index);
    BOOL WINAPI_CALL THETAZ_GetDiskFreeSpaceA(LPCSTR lpRootPathName, LPDWORD lpSectorsPerCluster, LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters, LPDWORD lpTotalNumberOfClusters);
    BOOL WINAPI_CALL THETAZ_GetDiskFreeSpaceExW(LPCWSTR lpDirectoryName, ULARGE_INTEGER* lpFreeBytesAvailableToCaller, ULARGE_INTEGER* lpTotalNumberOfBytes, ULARGE_INTEGER* lpTotalNumberOfFreeBytes);
    BOOL WINAPI_CALL THETAZ_GetDiskFreeSpaceW(LPCWSTR lpRootPathName, LPDWORD lpSectorsPerCluster, LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters, LPDWORD lpTotalNumberOfClusters);
    UINT WINAPI_CALL THETAZ_GetDriveTypeA(LPCSTR lpRootPathName);
    UINT WINAPI_CALL THETAZ_GetDriveTypeW(LPCWSTR lpRootPathName);
    DWORD WINAPI_CALL THETAZ_GetEnvironmentVariableA(LPCSTR lpName, LPSTR lpBuffer, DWORD nSize);
    DWORD WINAPI_CALL THETAZ_GetEnvironmentVariableW(LPCWSTR lpName, LPWSTR lpBuffer, DWORD nSize);
    BOOL WINAPI_CALL THETAZ_GetExitCodeProcess(HANDLE process, DWORD* code);
    DWORD WINAPI_CALL THETAZ_GetExitCodeThread(HANDLE hThread, LPDWORD lpExitCode);
    DWORD WINAPI_CALL THETAZ_GetFileAttributesA(LPCSTR lpFileName);
    BOOL WINAPI_CALL THETAZ_GetFileAttributesExW(LPCWSTR lpFileName, DWORD fInfoLevelId, void* lpFileInformation);
    DWORD WINAPI_CALL THETAZ_GetFileAttributesW(LPCWSTR lpFileName);
    BOOL WINAPI_CALL THETAZ_GetFileInformationByHandle(HANDLE hFile, BY_HANDLE_FILE_INFORMATION* lpFileInformation);
    BOOL WINAPI_CALL THETAZ_GetFileInformationByHandleEx(HANDLE hFile, DWORD FileInformationClass, void* lpFileInformation, DWORD dwBufferSize);
    DWORD WINAPI_CALL THETAZ_GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh);
    BOOL WINAPI_CALL THETAZ_GetFileSizeEx(HANDLE hFile, void* lpFileSize);
    BOOL WINAPI_CALL THETAZ_GetFileTime(HANDLE hFile, FILETIME* lpCreationTime, FILETIME* lpLastAccessTime, FILETIME* lpLastWriteTime);
    DWORD WINAPI_CALL THETAZ_GetFileType(HANDLE hFile);
    DWORD WINAPI_CALL THETAZ_GetFinalPathNameByHandleW(HANDLE hFile, LPWSTR lpszFilePath, DWORD cchFilePath, DWORD dwFlags);
    DWORD WINAPI_CALL THETAZ_GetFullPathNameA(LPCSTR lpFileName, DWORD nBufferLength, LPSTR lpBuffer, LPSTR* lpFilePart);
    DWORD WINAPI_CALL THETAZ_GetFullPathNameW(LPCWSTR lpFileName, DWORD nBufferLength, LPWSTR lpBuffer, LPWSTR* lpFilePart);
    DWORD WINAPI_CALL THETAZ_GetLastError();
    void WINAPI_CALL THETAZ_GetLocalTime(void* lpSystemTime);
    INT WINAPI_CALL THETAZ_GetLocaleInfoA(DWORD Locale, DWORD LCType, LPSTR lpLCData, INT cchData);
    INT WINAPI_CALL THETAZ_GetLocaleInfoEx(LPCWSTR lpLocaleName, DWORD LCType, LPWSTR lpLCData, INT cchData);
    INT WINAPI_CALL THETAZ_GetLocaleInfoW(DWORD Locale, DWORD LCType, LPWSTR lpLCData, INT cchData);
    BOOL WINAPI_CALL THETAZ_GetLogicalProcessorInformation(SYSTEM_LOGICAL_PROCESSOR_INFORMATION* Buffer, DWORD* ReturnedLength);
    DWORD WINAPI_CALL THETAZ_GetLongPathNameW(LPCWSTR lpszShortPath, LPWSTR lpszLongPath, DWORD cchBuffer);
    BOOL WINAPI_CALL THETAZ_GetMenuItemInfoW(HWND hMenu, UINT item, BOOL fByPosition, void* lpmii);
    DWORD WINAPI_CALL THETAZ_GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize);
    DWORD WINAPI_CALL THETAZ_GetModuleFileNameW(HMODULE hModule, LPWSTR lpFilename, DWORD nSize);
    HMODULE WINAPI_CALL THETAZ_GetModuleHandleA(LPCSTR lpModuleName);
    BOOL WINAPI_CALL THETAZ_GetModuleHandleExW(DWORD dwFlags, LPCWSTR lpModuleName, HMODULE* phModule);
    HMODULE WINAPI_CALL THETAZ_GetModuleHandleW(LPCWSTR lpModuleName);
    int WINAPI_CALL THETAZ_GetNameInfoW(const SOCKADDR* pSockaddr, size_t SockaddrLength, PWCHAR pNodeBuffer, DWORD NodeBufferSize, PWCHAR pServiceBuffer, DWORD ServiceBufferSize, INT Flags);
    BOOL WINAPI_CALL THETAZ_GetNamedPipeInfo(HANDLE hNamedPipe, LPDWORD lpFlags, LPDWORD lpOutBufferSize, LPDWORD lpInBufferSize, LPDWORD lpNumberOfInstances);
    void WINAPI_CALL THETAZ_GetNativeSystemInfo(void* lpSystemInfo);
    BOOL WINAPI_CALL THETAZ_GetNumberOfConsoleInputEvents(HANDLE hConsoleInput, LPDWORD lpNumberOfEvents);
    DWORD WINAPI_CALL THETAZ_GetOEMCP();
    BOOL WINAPI_CALL THETAZ_GetOverlappedResult(HANDLE hFile, void* lpOverlapped, LPDWORD lpNumberOfBytesTransferred, BOOL bWait);
    BOOL WINAPI_CALL THETAZ_GetPhysicallyInstalledSystemMemory(uint64_t* TotalBytesInKilobytes);
    DWORD WINAPI_CALL THETAZ_GetPriorityClass(HANDLE hProcess);
    BOOL WINAPI_CALL THETAZ_GetProcessAffinityMask(HANDLE hProcess, DWORD* lpProcessAffinityMask, DWORD* lpSystemAffinityMask);
    DWORD WINAPI_CALL THETAZ_GetProcessHandleCount(HANDLE hProcess, DWORD* pdwHandleCount);
    HANDLE WINAPI_CALL THETAZ_GetProcessHeap();
    DWORD WINAPI_CALL THETAZ_GetProcessId(HANDLE hProcess);
    BOOL WINAPI_CALL THETAZ_GetProcessTimes(HANDLE hProcess, FILETIME* lpCreationTime, FILETIME* lpExitTime, FILETIME* lpKernelTime, FILETIME* lpUserTime);
    BOOL WINAPI_CALL THETAZ_GetProductInfo(DWORD dwOSMajorVersion, DWORD dwOSMinorVersion, DWORD dwSpMajorVersion, DWORD dwSpMinorVersion, DWORD* pdwReturnedProductType);
    HANDLE WINAPI_CALL THETAZ_GetPropA(HWND hWnd, LPCSTR lpString);
    BOOL WINAPI_CALL THETAZ_GetQueuedCompletionStatus(HANDLE CompletionPort, LPDWORD lpNumberOfBytesTransferred, uintptr_t* lpCompletionKey, void** lpOverlapped, DWORD dwMilliseconds);
    BOOL WINAPI_CALL THETAZ_GetSecurityDescriptorDacl(PSECURITY_DESCRIPTOR pSecurityDescriptor, LPBOOL lpbDaclPresent, PACL* pDacl, LPBOOL lpbDaclDefaulted);
    DWORD WINAPI_CALL THETAZ_GetShortPathNameA(LPCSTR lpszLongPath, LPSTR lpszShortPath, DWORD cchBuffer);
    void WINAPI_CALL THETAZ_GetStartupInfoA(void* lpStartupInfo);
    void WINAPI_CALL THETAZ_GetStartupInfoW(void* lpStartupInfo);
    HANDLE WINAPI_CALL THETAZ_GetStdHandle(DWORD nStdHandle);
    BOOL WINAPI_CALL THETAZ_GetStringTypeExA(DWORD Locale, DWORD dwInfoType, LPCSTR lpSrcStr, INT cchSrc, WORD* lpCharType);
    BOOL WINAPI_CALL THETAZ_GetStringTypeExW(DWORD Locale, DWORD dwInfoType, LPCWSTR lpSrcStr, INT cchSrc, WORD* lpCharType);
    BOOL WINAPI_CALL THETAZ_GetStringTypeW(DWORD dwInfoType, LPCWSTR lpSrcStr, INT cchSrc, WORD* lpCharType);
    DWORD WINAPI_CALL THETAZ_GetSystemDefaultLCID();
    INT WINAPI_CALL THETAZ_GetSystemDefaultLocaleName(LPWSTR lpLocaleName, INT cchLocaleName);
    UINT WINAPI_CALL THETAZ_GetSystemDirectoryW(LPWSTR lpBuffer, UINT uSize);
    void WINAPI_CALL THETAZ_GetSystemInfo(void* lpSystemInfo);
    int WINAPI_CALL THETAZ_GetSystemMetrics(int index);
    BOOL WINAPI_CALL THETAZ_GetSystemPowerStatus(SYSTEM_POWER_STATUS* lpSystemPowerStatus);
    BOOL WINAPI_CALL THETAZ_GetSystemPreferredUILanguages(DWORD dwFlags, DWORD* pulNumLanguages, LPWSTR pwszLanguagesBuffer, DWORD* pcchLanguagesBuffer);
    void WINAPI_CALL THETAZ_GetSystemTime(void* lpSystemTime);
    void WINAPI_CALL THETAZ_GetSystemTimeAsFileTime(void* lpSystemTimeAsFileTime);
    INT WINAPI_CALL THETAZ_GetSystemTimePreciseAsFileTime(void* lpFileTime);
    UINT WINAPI_CALL THETAZ_GetTempFileNameW(LPCWSTR lpPathName, LPCWSTR lpPrefixString, UINT uUnique, LPWSTR lpTempFileName);
    DWORD WINAPI_CALL THETAZ_GetTempPathA(DWORD nBufferLength, LPSTR lpBuffer);
    DWORD WINAPI_CALL THETAZ_GetTempPathW(DWORD nBufferLength, LPWSTR lpBuffer);
    BOOL WINAPI_CALL THETAZ_GetThreadIOPendingFlag(HANDLE hThread, PBOOL lpIOIsPending);
    DWORD WINAPI_CALL THETAZ_GetThreadId(HANDLE hThread);
    DWORD WINAPI_CALL THETAZ_GetThreadLocale();
    BOOL WINAPI_CALL THETAZ_GetThreadTimes(HANDLE hThread, FILETIME* lpCreationTime, FILETIME* lpExitTime, FILETIME* lpKernelTime, FILETIME* lpUserTime);
    DWORD WINAPI_CALL THETAZ_GetTickCount();
    ULONGLONG WINAPI_CALL THETAZ_GetTickCount64();
    INT WINAPI_CALL THETAZ_GetTimeFormatEx(LPCWSTR lpLocaleName, DWORD dwFlags, const SYSTEMTIME* lpTime, LPCWSTR lpFormat, LPWSTR lpTimeStr, INT cchTime, void* lpCalendar);
    INT WINAPI_CALL THETAZ_GetTimeFormatW(DWORD Locale, DWORD dwFlags, const SYSTEMTIME* lpTime, LPCWSTR lpFormat, LPWSTR lpTimeStr, INT cchTime);
    DWORD WINAPI_CALL THETAZ_GetTimeZoneInformation(TIME_ZONE_INFORMATION* lpTimeZoneInformation);
    DWORD WINAPI_CALL THETAZ_GetUserDefaultLCID();
    INT WINAPI_CALL THETAZ_GetUserDefaultLocaleName(LPWSTR lpLocaleName, INT cchLocaleName);
    DWORD WINAPI_CALL THETAZ_GetUserGeoID(DWORD GeoClass);
    BOOL WINAPI_CALL THETAZ_GetUserPreferredUILanguages(DWORD dwFlags, DWORD* pulNumLanguages, LPWSTR pwszLanguagesBuffer, DWORD* pcchLanguagesBuffer);
    BOOL WINAPI_CALL THETAZ_GetVersionExA(void* lpVersionInformation);
    BOOL WINAPI_CALL THETAZ_GetVersionExW(void* lpVersionInformation);
    BOOL WINAPI_CALL THETAZ_GetVolumeInformationW(LPCWSTR lpRootPathName, LPWSTR lpVolumeNameBuffer, DWORD nVolumeNameSize, LPDWORD lpVolumeSerialNumber, LPDWORD lpMaximumComponentLength, LPDWORD lpFileSystemFlags, LPWSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize);
    BOOL WINAPI_CALL THETAZ_GetVolumeNameForVolumeMountPointW(LPCWSTR lpszVolumeMountPoint, LPWSTR lpszVolumeName, DWORD cchBufferLength);
    BOOL WINAPI_CALL THETAZ_GetVolumePathNameW(LPCWSTR lpszFileName, LPWSTR lpszVolumePathName, DWORD cchBufferLength);
    BOOL WINAPI_CALL THETAZ_GetVolumePathNamesForVolumeNameW(LPCWSTR lpszVolumeName, LPWSTR lpszVolumePathNames, DWORD cchBufferLength, PDWORD lpcchReturnLength);
    LPVOID WINAPI_CALL THETAZ_GlobalAlloc(UINT uFlags, SIZE_T dwBytes);
    LPVOID WINAPI_CALL THETAZ_GlobalFree(LPVOID hMem);
    BOOL WINAPI_CALL THETAZ_GlobalMemoryStatusEx(MEMORYSTATUSEX* lpBuffer);
    LPVOID WINAPI_CALL THETAZ_GlobalReAlloc(LPVOID hMem, SIZE_T dwBytes, UINT uFlags);
    SIZE_T WINAPI_CALL THETAZ_GlobalSize(LPVOID hMem);
    SIZE_T WINAPI_CALL THETAZ_HeapCompact(HANDLE hHeap, DWORD dwFlags);
    HANDLE WINAPI_CALL THETAZ_HeapCreate(DWORD flOptions, SIZE_T dwInitialSize, SIZE_T dwMaximumSize);
    BOOL WINAPI_CALL THETAZ_HeapDestroy(HANDLE hHeap);
    BOOL WINAPI_CALL THETAZ_HeapFree(HANDLE hHeap, DWORD dwFlags, void* lpMem);
    BOOL WINAPI_CALL THETAZ_HeapSetInformation(HANDLE hHeap, void* HeapInformationClass, void* HeapInformation, SIZE_T HeapInformationLength);
    SIZE_T WINAPI_CALL THETAZ_HeapSize(HANDLE hHeap, DWORD dwFlags, LPCVOID lpMem);
    BOOL WINAPI_CALL THETAZ_HeapValidate(HANDLE hHeap, DWORD dwFlags, LPCVOID lpMem);
    BOOL WINAPI_CALL THETAZ_HeapWalk(HANDLE hHeap, void* lpEntry);
    BOOL WINAPI_CALL THETAZ_ImmDisableIME(DWORD id);
    void WINAPI_CALL THETAZ_InitConditionVariable(PCONDITION_VARIABLE ConditionVariable);
    BOOL WINAPI_CALL THETAZ_InitOnceBeginInitialize(void* lpInitOnce, DWORD dwFlags, BOOL* fPending, LPVOID* lpContext);
    BOOL WINAPI_CALL THETAZ_InitOnceComplete(void* lpInitOnce, DWORD dwFlags, LPVOID lpContext);
    BOOL WINAPI_CALL THETAZ_InitOnceExecuteOnce(void* lpInitOnce, void* InitOnceCallback, void* Parameter, LPVOID* lpContext);
    LONG WINAPI_CALL THETAZ_AppPolicyGetProcessTerminationMethod(HANDLE processToken, DWORD* policy);
    LONG WINAPI_CALL THETAZ_AppPolicyGetThreadInitializationType(HANDLE processToken, DWORD* policy);
    void WINAPI_CALL THETAZ_InitializeConditionVariable(void* lpConditionVariable);
    void WINAPI_CALL THETAZ_InitializeCriticalSection(PCRITICAL_SECTION lpCriticalSection);
    BOOL WINAPI_CALL THETAZ_InitializeCriticalSectionAndSpinCount(PCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount);
    BOOL WINAPI_CALL THETAZ_InitializeCriticalSectionEx(PCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount, DWORD Flags);
    void WINAPI_CALL THETAZ_InitializeSListHead(void* ListHead);
    void WINAPI_CALL THETAZ_InitializeSRWLock(void* SRWLock);
    BOOL WINAPI_CALL THETAZ_TryAcquireSRWLockExclusive(void* SRWLock);
    BOOL WINAPI_CALL THETAZ_TryAcquireSRWLockShared(void* SRWLock);
    BOOL WINAPI_CALL THETAZ_IsBadStringPtrW(LPCWSTR lpsz, UINT_PTR ucchMax);
    BOOL WINAPI_CALL THETAZ_IsDBCSLeadByte(BYTE TestChar);
    BOOL WINAPI_CALL THETAZ_IsDebuggerPresent();
    BOOL WINAPI_CALL THETAZ_IsProcessorFeaturePresent(DWORD ProcessorFeature);
    BOOL WINAPI_CALL THETAZ_IsSystemResumeAutomatic();
    BOOL WINAPI_CALL THETAZ_IsValidCodePage(UINT CodePageID);
    BOOL WINAPI_CALL THETAZ_IsValidLocale(DWORD Locale, DWORD dwFlags);
    BOOL WINAPI_CALL THETAZ_IsValidLocaleName(LPCWSTR lpLocaleName);
    INT WINAPI_CALL THETAZ_LCIDToLocaleName(DWORD Locale, LPWSTR lpName, INT cchName, DWORD dwFlags);
    INT WINAPI_CALL THETAZ_LCMapStringEx(LPCWSTR lpLocaleName, DWORD dwMapFlags, LPCWSTR lpSrcStr, INT cchSrc, LPWSTR lpDestStr, INT cchDest, void* lpVersionInformation, void* lpReserved, void* lParam);
    INT WINAPI_CALL THETAZ_LCMapStringW(DWORD Locale, DWORD dwMapFlags, LPCWSTR lpSrcStr, INT cchSrc, LPWSTR lpDestStr, INT cchDest);
    void WINAPI_CALL THETAZ_LeaveCriticalSection(PCRITICAL_SECTION lpCriticalSection);
    HMODULE WINAPI_CALL THETAZ_LoadLibraryA(LPCSTR lpLibFileName);
    HMODULE WINAPI_CALL THETAZ_LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
    HMODULE WINAPI_CALL THETAZ_LoadLibraryW(LPCWSTR lpLibFileName);
    int WINAPI_CALL THETAZ_LoadStringA(HINSTANCE hInstance, UINT uID, LPSTR lpBuffer, int cchBufferMax);
    LPVOID WINAPI_CALL THETAZ_LocalAlloc(UINT uFlags, SIZE_T uBytes);
    LPVOID WINAPI_CALL THETAZ_LocalFree(LPVOID hMem);
    LPVOID WINAPI_CALL THETAZ_LocalReAlloc(LPVOID hMem, SIZE_T uBytes, UINT uFlags);
    SIZE_T WINAPI_CALL THETAZ_LocalSize(LPVOID hMem);
    DWORD WINAPI_CALL THETAZ_LocaleNameToLCID(LPCWSTR lpName, DWORD dwFlags);
    BOOL WINAPI_CALL THETAZ_LockFile(HANDLE hFile, DWORD dwFileOffsetLow, DWORD dwFileOffsetHigh, DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh);
    BOOL WINAPI_CALL THETAZ_LockFileEx(HANDLE hFile, DWORD dwFlags, DWORD dwReserved, DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh, void* lpOverlapped);
    LPVOID WINAPI_CALL THETAZ_LockResource(void* hResData);
    ULONG WINAPI_CALL THETAZ_LsaNtStatusToWinError(void* status);
    LPVOID WINAPI_CALL THETAZ_MapViewOfFile(HANDLE hFileMappingObject, DWORD dwDesiredAccess, DWORD dwFileOffsetHigh, DWORD dwFileOffsetLow, SIZE_T dwNumberOfBytesToMap);
    HMONITOR WINAPI_CALL THETAZ_MonitorFromPoint(POINT pt, DWORD dwFlags);
    HMONITOR WINAPI_CALL THETAZ_MonitorFromWindow(HWND hwnd, DWORD dwFlags);
    BOOL WINAPI_CALL THETAZ_MoveFileExW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, DWORD dwFlags);
    DWORD WINAPI_CALL THETAZ_MsgWaitForMultipleObjectsEx(DWORD nCount, const HANDLE* pHandles, DWORD dwMilliseconds, DWORD dwWakeMask, DWORD dwFlags);
    INT WINAPI_CALL THETAZ_MulDiv(INT nNumber, INT nNumerator, INT nDenominator);
    INT WINAPI_CALL THETAZ_MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr, int cbMultiByte,
                                           LPWSTR lpWideCharStr, int cchWideChar);
    LONG WINAPI_CALL THETAZ_NtQueryInformationProcess(HANDLE processHandle, PROCESSINFOCLASS processInformationClass,
                                                   PVOID processInformation, ULONG processInformationLength,
                                                   PULONG returnLength);
    LONG WINAPI_CALL THETAZ_NtQueryInformationThread(HANDLE threadHandle, THREADINFOCLASS threadInformationClass,
                                                  PVOID threadInformation, ULONG threadInformationLength,
                                                  PULONG returnLength);
    HRESULT WINAPI_CALL THETAZ_OleInitialize(LPVOID pvReserved);
    void WINAPI_CALL THETAZ_OleUninitialize();
    HANDLE WINAPI_CALL THETAZ_OpenEventA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpName);
    HANDLE WINAPI_CALL THETAZ_OpenEventW(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName);
    HANDLE WINAPI_CALL THETAZ_OpenFileMappingA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpName);
    HANDLE WINAPI_CALL THETAZ_OpenMutexA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpName);
    HANDLE WINAPI_CALL THETAZ_OpenMutexW(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName);
    HANDLE WINAPI_CALL THETAZ_OpenProcess(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId);
    HANDLE WINAPI_CALL THETAZ_OpenSemaphoreA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpName);
    HANDLE WINAPI_CALL THETAZ_OpenThread(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwThreadId);
    void WINAPI_CALL THETAZ_OutputDebugStringA(LPCSTR lpOutputString);
    void WINAPI_CALL THETAZ_OutputDebugStringW(LPCWSTR lpOutputString);
    BOOL WINAPI_CALL THETAZ_PeekNamedPipe(HANDLE hNamedPipe, LPVOID lpBuffer, DWORD nBufferSize, LPDWORD lpBytesRead, LPDWORD lpTotalBytesAvail, LPDWORD lpBytesLeftThisMessage);
    BOOL WINAPI_CALL THETAZ_PostQueuedCompletionStatus(HANDLE CompletionPort, DWORD dwNumberOfBytesTransferred, uintptr_t dwCompletionKey, void* lpOverlapped);
    BOOL WINAPI_CALL THETAZ_ProcessIdToSessionId(DWORD dwProcessId, DWORD* pSessionId);
    BOOL WINAPI_CALL THETAZ_PulseEvent(HANDLE hEvent);
    WORD WINAPI_CALL THETAZ_QueryDepthSList(void* ListHead);
    BOOL WINAPI_CALL THETAZ_QueryFullProcessImageNameW(HANDLE hProcess, DWORD dwFlags, LPWSTR lpExeName, PDWORD lpdwSize);
    BOOL WINAPI_CALL THETAZ_QueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount);
    BOOL WINAPI_CALL THETAZ_QueryPerformanceFrequency(LARGE_INTEGER* lpFrequency);
    BOOL WINAPI_CALL THETAZ_QueryProcessCycleTime(HANDLE hProcess, uint64_t* CycleTime);
    BOOL WINAPI_CALL THETAZ_QueryThreadCycleTime(HANDLE hThread, uint64_t* CycleTime);
    BOOL WINAPI_CALL THETAZ_QueryUnbiasedInterruptTime(uint64_t* lpUnbiasedInterruptTime);
    void WINAPI_CALL THETAZ_RaiseException(DWORD dwExceptionCode, DWORD dwExceptionFlags, DWORD nNumberOfArguments, void* lpArguments);
    BOOL WINAPI_CALL THETAZ_ReadConsoleW(HANDLE hConsoleInput, LPVOID lpBuffer, DWORD nNumberOfCharsToRead,
                                      LPDWORD lpNumberOfCharsRead, PCONSOLE_READCONSOLE_CONTROL lpConsoleReadControl);
    BOOL WINAPI_CALL THETAZ_ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
                                  LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped);
    BOOL WINAPI_CALL THETAZ_ReadProcessMemory(HANDLE hProcess, LPCVOID lpBaseAddress, LPVOID lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesRead);
    BOOL WINAPI_CALL THETAZ_RegCloseKey(HKEY hKey);
    LONG WINAPI_CALL THETAZ_RegCreateKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD Reserved, LPSTR lpClass, DWORD dwOptions, REGSAM samDesired, LPSECURITY_ATTRIBUTES lpSecurityAttributes, PHKEY phkResult, LPDWORD lpdwDisposition);
    LONG WINAPI_CALL THETAZ_RegCreateKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD Reserved, LPWSTR lpClass, DWORD dwOptions, REGSAM samDesired, LPSECURITY_ATTRIBUTES lpSecurityAttributes, PHKEY phkResult, LPDWORD lpdwDisposition);
    LONG WINAPI_CALL THETAZ_RegCreateKeyW(HKEY hKey, LPCWSTR lpSubKey, PHKEY phkResult);
    LONG WINAPI_CALL THETAZ_RegDeleteValueA(HKEY hKey, LPCSTR lpValueName);
    LONG WINAPI_CALL THETAZ_RegDeleteValueW(HKEY hKey, LPCWSTR lpValueName);
    LONG WINAPI_CALL THETAZ_RegEnumKeyExA(HKEY hKey, DWORD dwIndex, LPSTR lpName, LPDWORD lpcbName, LPDWORD lpReserved, LPSTR lpClass, LPDWORD lpcbClass, LPFILETIME lpftLastWriteTime);
    LONG WINAPI_CALL THETAZ_RegEnumKeyExW(HKEY hKey, DWORD dwIndex, LPWSTR lpName, LPDWORD lpcbName, LPDWORD lpReserved, LPWSTR lpClass, LPDWORD lpcbClass, LPFILETIME lpftLastWriteTime);
    LONG WINAPI_CALL THETAZ_RegGetValueW(HKEY hKey, LPCWSTR lpSubKey, LPCWSTR lpValue, DWORD dwFlags, LPDWORD pdwType, void* pvData, LPDWORD pcbData);
    HANDLE WINAPI_CALL THETAZ_RegOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, DWORD samDesired, PHKEY phkResult);
    HANDLE WINAPI_CALL THETAZ_RegOpenKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, DWORD samDesired, PHKEY phkResult);
    LONG WINAPI_CALL THETAZ_RegQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);
    LONG WINAPI_CALL THETAZ_RegQueryValueExW(HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);
    LONG WINAPI_CALL THETAZ_RegSetValueExA(HKEY hKey, LPCSTR lpValueName, DWORD Reserved, DWORD dwType, const BYTE* lpData, DWORD cbData);
    LONG WINAPI_CALL THETAZ_RegSetValueExW(HKEY hKey, LPCWSTR lpValueName, DWORD Reserved, DWORD dwType, const BYTE* lpData, DWORD cbData);
    ATOM WINAPI_CALL THETAZ_RegisterClassExW(const void* lpwcx);
    UINT WINAPI_CALL THETAZ_RegisterClipboardFormatA(LPCSTR lpszFormat);
    UINT WINAPI_CALL THETAZ_RegisterClipboardFormatW(LPCWSTR lpszFormat);
    UINT WINAPI_CALL THETAZ_RegisterWindowMessageA(LPCSTR lpString);
    int WINAPI_CALL THETAZ_ReleaseDC(void* hwnd, void* hdc);
    BOOL WINAPI_CALL THETAZ_ReleaseMutex(HANDLE hMutex);
    void WINAPI_CALL THETAZ_ReleaseSRWLockExclusive(void* SRWLock);
    void WINAPI_CALL THETAZ_ReleaseSRWLockShared(void* SRWLock);
    BOOL WINAPI_CALL THETAZ_ReleaseSemaphore(HANDLE hSemaphore, LONG lReleaseCount, LONG* lpPreviousCount);
    BOOL WINAPI_CALL THETAZ_RemoveDirectoryW(LPCWSTR lpPathName);
    HANDLE WINAPI_CALL THETAZ_RemovePropA(HWND hWnd, LPCSTR lpString);
    BOOL WINAPI_CALL THETAZ_RemoveVectoredContinueHandler(PVOID handle);
    BOOL WINAPI_CALL THETAZ_RemoveVectoredExceptionHandler(PVOID handle);
    BOOL WINAPI_CALL THETAZ_ResetEvent(HANDLE hEvent);
    BOOL WINAPI_CALL THETAZ_ResumeThread(HANDLE hThread);
    WORD WINAPI_CALL THETAZ_RtlCaptureStackBackTrace(DWORD FramesToSkip, DWORD FramesToCapture, void** BackTrace, DWORD* BackTraceHash);
    uint64_t WINAPI_CALL THETAZ_RtlLookupFunctionEntry(uint64_t controlPc, uint64_t* imageBase, void* historyTable);
    void WINAPI_CALL THETAZ_RtlUnwind(void* TargetFrame, void* TargetIp, void* ExceptionRecord, DWORD ReturnValue);
    void WINAPI_CALL THETAZ_RtlUnwindEx(void* TargetFrame, void* TargetIp, void* ExceptionRecord, void* ReturnValue, void* ContextRecord, void* HistoryTable);
    int WINAPI_CALL THETAZ_SHFileOperationW(void* lpFileOp);
    HRESULT WINAPI_CALL THETAZ_SHGetFileInfoW(LPCWSTR pszPath, DWORD dwFileAttributes, void* psfi, UINT cbFileInfo, UINT uFlags);
    HRESULT WINAPI_CALL THETAZ_SHGetSpecialFolderLocation(HWND hwnd, int csidl, void** ppidl);
    LRESULT WINAPI_CALL THETAZ_SendMessageTimeoutA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam, UINT fuFlags, UINT uTimeout, PDWORD_PTR lpdwResult);
    LRESULT WINAPI_CALL THETAZ_SendMessageTimeoutW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam, UINT fuFlags, UINT uTimeout, PDWORD_PTR lpdwResult);
    DWORD WINAPI_CALL THETAZ_SetBkColor(void* hdc, DWORD color);
    int WINAPI_CALL THETAZ_SetBkMode(void* hdc, int mode);
    BOOL WINAPI_CALL THETAZ_SetConsoleMode(HANDLE hConsoleHandle, DWORD dwMode);
    BOOL WINAPI_CALL THETAZ_SetCurrentDirectoryA(LPCSTR lpPathName);
    BOOL WINAPI_CALL THETAZ_SetCurrentDirectoryW(LPCWSTR lpPathName);
    void WINAPI_CALL THETAZ_SetDefaultDllDirectories(DWORD DirectoryFlags);
    BOOL WINAPI_CALL THETAZ_SetDlgItemTextW(HWND hDlg, int nIDDlgItem, LPCWSTR lpString);
    BOOL WINAPI_CALL THETAZ_SetEndOfFile(HANDLE hFile);
    BOOL WINAPI_CALL THETAZ_SetEnvironmentVariableA(LPCSTR lpName, LPCSTR lpValue);
    BOOL WINAPI_CALL THETAZ_SetEnvironmentVariableW(LPCWSTR lpName, LPCWSTR lpValue);
    UINT WINAPI_CALL THETAZ_SetErrorMode(UINT uMode);
    BOOL WINAPI_CALL THETAZ_SetEvent(HANDLE hEvent);
    BOOL WINAPI_CALL THETAZ_SetFileAttributesW(LPCWSTR lpFileName, DWORD dwFileAttributes);
    BOOL WINAPI_CALL THETAZ_SetFileInformationByHandle(HANDLE hFile, DWORD FileInformationClass, void* lpFileInformation, DWORD dwBufferSize);
    DWORD WINAPI_CALL THETAZ_SetFilePointer(HANDLE hFile, LONG lDistanceToMove, LONG* lpDistanceToMoveHigh, DWORD dwMoveMethod);
    BOOL WINAPI_CALL THETAZ_SetFilePointerEx(HANDLE hFile, LARGE_INTEGER liDistanceToMove, LARGE_INTEGER* lpNewFilePointer, DWORD dwMoveMethod);
    BOOL WINAPI_CALL THETAZ_SetFileTime(HANDLE hFile, const FILETIME* lpCreationTime, const FILETIME* lpLastAccessTime, const FILETIME* lpLastWriteTime);
    BOOL WINAPI_CALL THETAZ_SetHandleInformation(HANDLE hObject, DWORD dwMask, DWORD dwFlags);
    BOOL WINAPI_CALL THETAZ_GetHandleInformation(HANDLE hObject, LPDWORD lpdwFlags);
    void WINAPI_CALL THETAZ_SetLastError(DWORD dwErrCode);
    BOOL WINAPI_CALL THETAZ_SetNamedPipeHandleState(HANDLE hNamedPipe, LPDWORD lpMode, LPDWORD lpMaxCollectionCount, LPDWORD lpCollectionDataSize);
    void WINAPI_CALL THETAZ_SetProcessDPIAware();
    BOOL WINAPI_CALL THETAZ_SetProcessMitigationPolicy(DWORD Policy, void* lpBuffer, SIZE_T dwLength);
    BOOL WINAPI_CALL THETAZ_SetStdHandle(DWORD nStdHandle, HANDLE hHandle);
    DWORD WINAPI_CALL THETAZ_SetTextColor(void* hdc, DWORD color);
    BOOL WINAPI_CALL THETAZ_SetThreadContext(HANDLE hThread, WOW64CONTEXT* ctx);
    HRESULT WINAPI_CALL THETAZ_SetThreadDescription(HANDLE hThread, PCWSTR lpThreadDescription);
    BOOL WINAPI_CALL THETAZ_SetThreadPriority(HANDLE hThread, int nPriority);
    
    BOOL WINAPI_CALL THETAZ_SetWaitableTimerEx(HANDLE hTimer, const LARGE_INTEGER* lpDueTime, LONG lPeriod, void* pfnCompletionRoutine, void* lpArgToCompletionRoutine, void* WakeContext, LONG Reserved);
    DWORD WINAPI_CALL THETAZ_SizeofResource(HMODULE hModule, void* hResInfo);
    DWORD WINAPI_CALL THETAZ_Sleep(DWORD dwMilliseconds);
    BOOL WINAPI_CALL THETAZ_SleepConditionVariableCS(PCONDITION_VARIABLE ConditionVariable, PCRITICAL_SECTION CriticalSection, DWORD dwMilliseconds);
    BOOL WINAPI_CALL THETAZ_SleepConditionVariableSRW(void* ConditionVariable, void* SRWLock, DWORD dwMilliseconds, ULONG Flags);
    HRESULT WINAPI_CALL THETAZ_StringFromCLSID(REFCLSID rclsid, LPOLESTR* lplpsz);
    BOOL WINAPI_CALL THETAZ_SuspendThread(HANDLE hThread);
    BOOL WINAPI_CALL THETAZ_SwitchToThread();
    BOOL WINAPI_CALL THETAZ_SystemParametersInfoW(UINT uiAction, UINT uiParam, void* pvParam, UINT fWinIni);
    BOOL WINAPI_CALL THETAZ_SystemTimeToFileTime(const SYSTEMTIME* lpSystemTime, FILETIME* lpFileTime);
    BOOL WINAPI_CALL THETAZ_TerminateProcess(HANDLE hProcess, UINT uExitCode);
    BOOL WINAPI_CALL THETAZ_TextOutA(void* hdc, int x, int y, const char* lpString, int c);
    BOOL WINAPI_CALL THETAZ_TextOutW(void* hdc, int x, int y, const uint16_t* lpString, int c);
    DWORD WINAPI_CALL THETAZ_TlsAlloc();
    BOOL WINAPI_CALL THETAZ_TlsFree(DWORD dwTlsIndex);
    LPVOID WINAPI_CALL THETAZ_TlsGetValue(DWORD dwTlsIndex);
    BOOL WINAPI_CALL THETAZ_TlsSetValue(DWORD dwTlsIndex, LPVOID lpTlsValue);
    BOOL WINAPI_CALL THETAZ_TryAcquireSRWLockExclusive(void* SRWLock);
    BOOL WINAPI_CALL THETAZ_TryEnterCriticalSection(PCRITICAL_SECTION lpCriticalSection);
    BOOL WINAPI_CALL THETAZ_TzSpecificLocalTimeToSystemTime(const TIME_ZONE_INFORMATION* lpTimeZoneInformation, const SYSTEMTIME* lpLocalTime, SYSTEMTIME* lpUniversalTime);
    LONG WINAPI_CALL THETAZ_UnhandledExceptionFilter(void* ExceptionInfo);
    BOOL WINAPI_CALL THETAZ_UnlockFile(HANDLE hFile, DWORD dwFileOffsetLow, DWORD dwFileOffsetHigh, DWORD nNumberOfBytesToUnlockLow, DWORD nNumberOfBytesToUnlockHigh);
    BOOL WINAPI_CALL THETAZ_UnlockFileEx(HANDLE hFile, DWORD dwReserved, DWORD nNumberOfBytesToUnlockLow, DWORD nNumberOfBytesToUnlockHigh, void* lpOverlapped);
    BOOL WINAPI_CALL THETAZ_UnmapViewOfFile(LPCVOID lpBaseAddress);
    uint64_t WINAPI_CALL THETAZ_VerSetConditionMask(uint64_t ConditionMask, DWORD TypeMask, BYTE Condition);
    BOOL WINAPI_CALL THETAZ_VerifyVersionInfoW(void* lpVersionInformation, DWORD dwTypeMask, uint64_t dwlConditionMask);
    LPVOID WINAPI_CALL THETAZ_VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect);
    BOOL WINAPI_CALL THETAZ_VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType);
    BOOL WINAPI_CALL THETAZ_VirtualProtect(LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, PDWORD lpflOldProtect);
    BOOL WINAPI_CALL THETAZ_VirtualProtectEx(HANDLE hProcess, LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, PDWORD lpflOldProtect);
    SIZE_T WINAPI_CALL THETAZ_VirtualQuery(LPCVOID lpAddress, PMEMORY_BASIC_INFORMATION lpBuffer, SIZE_T dwLength);
    SIZE_T WINAPI_CALL THETAZ_VirtualQueryEx(HANDLE hProcess, LPCVOID lpAddress, PMEMORY_BASIC_INFORMATION lpBuffer, SIZE_T dwLength);
    DWORD WINAPI_CALL THETAZ_WaitForMultipleObjects(DWORD nCount, const HANDLE* lpHandles,
                                                BOOL bWaitAll, DWORD dwMilliseconds);
    DWORD WINAPI_CALL THETAZ_WaitForMultipleObjectsEx(DWORD nCount, const HANDLE* lpHandles, BOOL bWaitAll, DWORD dwMilliseconds, BOOL bAlertable);
    DWORD WINAPI_CALL THETAZ_WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
    DWORD WINAPI_CALL THETAZ_WaitForSingleObjectEx(HANDLE hHandle, DWORD dwMilliseconds, BOOL bAlertable);
    BOOL WINAPI_CALL THETAZ_WaitNamedPipeA(LPCSTR lpNamedPipeName, DWORD nTimeout);
    BOOL WINAPI_CALL THETAZ_WaitNamedPipeW(LPCWSTR lpNamedPipeName, DWORD nTimeout);
    void WINAPI_CALL THETAZ_WakeAllConditionVariable(void* ConditionVariable);
    void WINAPI_CALL THETAZ_WakeConditionVariable(void* ConditionVariable);
    LONG WINAPI_CALL THETAZ_WerRegisterMemoryBlock(void* pvAddress, DWORD dwSize);
    LONG WINAPI_CALL THETAZ_WerUnregisterMemoryBlock(void* pvAddress);
    BOOL WINAPI_CALL THETAZ_WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr, int cchWideChar,
                                             LPSTR lpMultiByteStr, int cbMultiByte, LPCSTR lpDefaultChar, LPBOOL lpUsedDefaultChar);
    HRESULT WINAPI_CALL THETAZ_WriteClassStg(void* pstg, REFCLSID rclsid);
    HRESULT WINAPI_CALL THETAZ_WriteClassStm(void* pstg, REFCLSID rclsid);
    BOOL WINAPI_CALL THETAZ_WriteConsoleW(HANDLE hConsoleOutput, const void* lpBuffer, DWORD nNumberOfCharsToWrite,
                                       LPDWORD lpNumberOfCharsWritten, PCONSOLE_READCONSOLE_CONTROL lpConsoleReadControl);
    BOOL WINAPI_CALL THETAZ_WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
                                   LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped);
    HRESULT WINAPI_CALL THETAZ_WriteFmtUserTypeStg(void* pstg, void* pfmtetc, DWORD dwUserType, LPWSTR lpszUserType);
    BOOL WINAPI_CALL THETAZ_WriteProcessMemory(HANDLE hProcess, LPVOID lpBaseAddress, LPCVOID lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesWritten);
    BOOL WINAPI_CALL THETAZ___WSAFDIsSet(SOCKET fd, void* pfdset);
    int WINAPI_CALL THETAZ__dclass(int c);
    int WINAPI_CALL THETAZ__wcslwr(wchar_t* str);
    int WINAPI_CALL THETAZ__wrmdir(const wchar_t* dirname);
    int WINAPI_CALL THETAZ__wsopen_s(int* pfh, const wchar_t* filename, int oflag, int shflag, int pmode);
    INT WINAPI_CALL THETAZ_lstrcmpW(LPCWSTR lpString1, LPCWSTR lpString2);
    INT WINAPI_CALL THETAZ_lstrcmpiW(LPCWSTR lpString1, LPCWSTR lpString2);
    LPWSTR WINAPI_CALL THETAZ_lstrcpyW(LPWSTR lpString1, LPCWSTR lpString2);
    INT WINAPI_CALL THETAZ_lstrlenW(LPCWSTR lpString);


// Function implementations

BOOL WINAPI_CALL THETAZ_CheckDlgButton(WinHandle hdlg, int id, uint32_t check) {
    uint64_t ctrl = THETAZ_GetDlgItem((uint64_t)(uintptr_t)hdlg, id);
    if (!ctrl) return FALSE_VALUE;
    return THETAZ_SendMessageA(ctrl, BM_SETCHECK, check ? 1 : 0, 0) ? TRUE_VALUE : FALSE_VALUE;
}

BOOL WINAPI_CALL THETAZ_CheckDlgButton_Wrapper(WinHandle hdlg, int id, uint32_t check) {
    if (x86_dispatch_active()) return THETAZ_CheckDlgButton(hdlg, id, check);
    return WINAPI64_CheckDlgButton(reinterpret_cast<uint64_t>(hdlg), id, check);
}

BOOL WINAPI_CALL THETAZ_CryptVerifySignatureW(uint64_t hHash, const uint8_t* pbSignature, DWORD dwSigLen,
    uint64_t hPubKey, const wchar_t* szDescription, DWORD dwFlags) {
    (void)hHash; (void)pbSignature; (void)dwSigLen;
    (void)hPubKey; (void)szDescription; (void)dwFlags;
    return 0;
}

BOOL WINAPI_CALL THETAZ_DestroyWindow(WinHandle hwnd) {
    uint64_t h = reinterpret_cast<uint64_t>(hwnd);
    uint64_t wnd_proc = 0;
    Window* win = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        win = find_window(h);
        if (!win) return FALSE_VALUE;
    }
    call_wnd_proc(win, WM_DESTROY, 0, 0);
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        // Destroy children (X11 subwindows die with the parent, but our
        // registry needs cleaning).
        for (auto it = g_windows.begin(); it != g_windows.end();) {
            if (it->second.parent == h) {
                auto xit = g_x11_windows.find(it->first);
                if (xit != g_x11_windows.end()) {
                    g_x11_hwnds.erase(xit->second);
                    g_x11_windows.erase(xit);
                }
                it = g_windows.erase(it);
            } else {
                ++it;
            }
        }
        g_windows.erase(h);
        auto xit = g_x11_windows.find(h);
        if (xit != g_x11_windows.end()) {
            g_x11_hwnds.erase(xit->second);
            g_x11_windows.erase(xit);
        }
    }
    return TRUE_VALUE;
}

BOOL WINAPI_CALL THETAZ_DestroyWindow_Wrapper(WinHandle hwnd) {
    if (x86_dispatch_active()) {
        return THETAZ_DestroyWindow(hwnd);
    } else {
        return WINAPI64_DestroyWindow(reinterpret_cast<uint64_t>(hwnd));
    }
}

int WINAPI_CALL THETAZ_DialogBoxParamA_Wrapper(WinHandle hInstance, const char* name,
                                                      WinHandle hWndParent, void* lpDialogFunc,
                                                      uint64_t dwInitParam) {
    if (x86_dispatch_active()) return THETAZ_DialogBoxParamA(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(hInstance)), name, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(hWndParent)), lpDialogFunc, dwInitParam);
    return WINAPI64_DialogBoxParamA(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(hInstance)), name, reinterpret_cast<uint64_t>(hWndParent), lpDialogFunc, dwInitParam);
}

BOOL WINAPI_CALL THETAZ_EnableWindow(WinHandle hwnd, BOOL enable) {
    uint64_t h = reinterpret_cast<uint64_t>(hwnd);
    std::lock_guard<std::mutex> lock(g_window_mutex);
    Window* win = find_window(h);
    if (!win) return FALSE_VALUE;
    BOOL was = win->enabled ? TRUE_VALUE : FALSE_VALUE;
    win->enabled = (enable != 0);
    return was;
}

BOOL WINAPI_CALL THETAZ_EnableWindow_Wrapper(WinHandle hwnd, BOOL enable) {
    if (x86_dispatch_active()) return THETAZ_EnableWindow(hwnd, enable);
    return WINAPI64_EnableWindow(reinterpret_cast<uint64_t>(hwnd), enable);
}

BOOL WINAPI_CALL THETAZ_EndDialog(WinHandle hdlg, int result) {
    uint64_t h = reinterpret_cast<uint64_t>(hdlg);
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        g_dialog_result_active = true;
        g_dialog_result = result;
        auto xit = g_x11_windows.find(h);
        if (xit != g_x11_windows.end()) {
            g_x11_hwnds.erase(xit->second);
            g_x11_windows.erase(xit);
        }
        g_windows.erase(h);
    }
    return TRUE_VALUE;
}

BOOL WINAPI_CALL THETAZ_EndDialog_Wrapper(WinHandle hdlg, int result) {
    if (x86_dispatch_active()) return THETAZ_EndDialog(hdlg, result);
    return WINAPI64_EndDialog(reinterpret_cast<uint64_t>(hdlg), result);
}

BOOL WINAPI_CALL THETAZ_EndPaint(WinHandle hwnd, const void* ps) {
    (void)hwnd;
    (void)ps;
    return TRUE_VALUE;
}

BOOL WINAPI_CALL THETAZ_EndPaint_Wrapper(WinHandle hwnd, const void* ps) {
    if (x86_dispatch_active()) {
        return THETAZ_EndPaint(hwnd, ps);
    } else {
        return WINAPI64_EndPaint(reinterpret_cast<uint64_t>(hwnd), ps);
    }
}

int WINAPI_CALL THETAZ_GetDlgCtrlID(WinHandle hwnd) {
    std::lock_guard<std::mutex> lock(g_window_mutex);
    Window* win = find_window(reinterpret_cast<uint64_t>(hwnd));
    return win ? static_cast<int>(win->id) : 0;
}

int WINAPI_CALL THETAZ_GetDlgCtrlID_Wrapper(WinHandle hwnd) {
    if (x86_dispatch_active()) return THETAZ_GetDlgCtrlID(hwnd);
    return WINAPI64_GetDlgCtrlID(reinterpret_cast<uint64_t>(hwnd));
}

int WINAPI_CALL THETAZ_GetDlgItemTextA(WinHandle hdlg, int id, char* buf, int max) {
    uint64_t ctrl = THETAZ_GetDlgItem(reinterpret_cast<uint64_t>(hdlg), id);
    if (!ctrl || !buf || max <= 0) return 0;
    uint64_t r = THETAZ_SendMessageA(ctrl, WM_GETTEXT, static_cast<uint64_t>(max),
                                     reinterpret_cast<uint64_t>(buf));
    return static_cast<int>(r);
}

int WINAPI_CALL THETAZ_GetDlgItemTextA_Wrapper(WinHandle hdlg, int id, char* buf, int max) {
    if (x86_dispatch_active()) return THETAZ_GetDlgItemTextA(hdlg, id, buf, max);
    return WINAPI64_GetDlgItemTextA(reinterpret_cast<uint64_t>(hdlg), id, buf, max);
}

LONG WINAPI_CALL THETAZ_GetMessageA(void* msg, WinHandle hwnd, DWORD filter_min, DWORD filter_max) {
    (void)hwnd;
    if (!msg) return -1;

    std::unique_lock<std::mutex> lock(g_window_mutex);
    while (true) {
        if (!g_queue.empty()) {
            GuestMsg m = g_queue.front();
            bool matches_filter = (filter_min == 0 && filter_max == 0) ||
                                  (m.message >= filter_min && m.message <= filter_max);
            if (m.message == WM_QUIT) {
                // WM_QUIT is never filtered and ends the loop.
                g_queue.pop_front();
                g_quit_pending = false;
                write_guest_msg(static_cast<uint8_t*>(msg), m);
                printf("[user32] GetMessageA -> WM_QUIT (%lu)\n", (unsigned long)m.wparam);
                return 0;
            }
            if (matches_filter) {
                g_queue.pop_front();
                write_guest_msg(static_cast<uint8_t*>(msg), m);
                return 1;
            }
            // Doesn't match: leave it queued and wait for a matching message.
        }
        g_queue_cond.wait(lock);
    }
}

LONG WINAPI_CALL THETAZ_GetMessageA_Wrapper(void* msg, WinHandle hwnd, DWORD filter_min, DWORD filter_max) {
    if (x86_dispatch_active()) {
        return THETAZ_GetMessageA(msg, hwnd, filter_min, filter_max);
    } else {
        return WINAPI64_GetMessageA(msg, reinterpret_cast<uint64_t>(hwnd), filter_min, filter_max);
    }
}

int WINAPI_CALL THETAZ_GetWindowTextA(WinHandle hwnd, char* buf, int max) {
    if (!buf || max <= 0) return 0;
    uint64_t r = THETAZ_SendMessageA(reinterpret_cast<uint64_t>(hwnd), WM_GETTEXT, static_cast<uint64_t>(max),
                                     reinterpret_cast<uint64_t>(buf));
    return static_cast<int>(r);
}

int WINAPI_CALL THETAZ_GetWindowTextA_Wrapper(WinHandle hwnd, char* buf, int max) {
    if (x86_dispatch_active()) return THETAZ_GetWindowTextA(hwnd, buf, max);
    return WINAPI64_GetWindowTextA(reinterpret_cast<uint64_t>(hwnd), buf, max);
}

int WINAPI_CALL THETAZ_GetWindowTextLengthA(WinHandle hwnd) {
    return static_cast<int>(THETAZ_SendMessageA(reinterpret_cast<uint64_t>(hwnd), WM_GETTEXTLENGTH, 0, 0));
}

int WINAPI_CALL THETAZ_GetWindowTextLengthA_Wrapper(WinHandle hwnd) {
    if (x86_dispatch_active()) return THETAZ_GetWindowTextLengthA(hwnd);
    return WINAPI64_GetWindowTextLengthA(reinterpret_cast<uint64_t>(hwnd));
}

BOOL WINAPI_CALL THETAZ_InvalidateRect(WinHandle hwnd, const void* rect, BOOL erase) {
    (void)erase;
    std::lock_guard<std::mutex> lock(g_window_mutex);
    Window* win = find_window(reinterpret_cast<uint64_t>(hwnd));
    if (!win) return FALSE_VALUE;
    win->dirty = true;
    if (rect) {
        const int* r = static_cast<const int*>(rect);
        win->inv_left = r[0];
        win->inv_top = r[1];
        win->inv_right = r[2];
        win->inv_bottom = r[3];
    } else {
        win->inv_left = 0;
        win->inv_top = 0;
        win->inv_right = win->w;
        win->inv_bottom = win->h;
    }
    return TRUE_VALUE;
}

BOOL WINAPI_CALL THETAZ_InvalidateRect_Wrapper(WinHandle hwnd, const void* rect, BOOL erase) {
    if (x86_dispatch_active()) {
        return THETAZ_InvalidateRect(hwnd, rect, erase);
    } else {
        return WINAPI64_InvalidateRect(reinterpret_cast<uint64_t>(hwnd), rect, erase);
    }
}

BOOL WINAPI_CALL THETAZ_IsWindow(WinHandle hwnd) {
    std::lock_guard<std::mutex> lock(g_window_mutex);
    return find_window(reinterpret_cast<uint64_t>(hwnd)) ? TRUE_VALUE : FALSE_VALUE;
}

BOOL WINAPI_CALL THETAZ_IsWindowEnabled(WinHandle hwnd) {
    std::lock_guard<std::mutex> lock(g_window_mutex);
    Window* win = find_window(reinterpret_cast<uint64_t>(hwnd));
    return (win && win->enabled) ? TRUE_VALUE : FALSE_VALUE;
}

BOOL WINAPI_CALL THETAZ_IsWindowEnabled_Wrapper(WinHandle hwnd) {
    if (x86_dispatch_active()) return THETAZ_IsWindowEnabled(hwnd);
    return WINAPI64_IsWindowEnabled(reinterpret_cast<uint64_t>(hwnd));
}

BOOL WINAPI_CALL THETAZ_IsWindowVisible(WinHandle hwnd) {
    std::lock_guard<std::mutex> lock(g_window_mutex);
    Window* win = find_window(reinterpret_cast<uint64_t>(hwnd));
    return (win && win->visible) ? TRUE_VALUE : FALSE_VALUE;
}

BOOL WINAPI_CALL THETAZ_IsWindowVisible_Wrapper(WinHandle hwnd) {
    if (x86_dispatch_active()) return THETAZ_IsWindowVisible(hwnd);
    return WINAPI64_IsWindowVisible(reinterpret_cast<uint64_t>(hwnd));
}

BOOL WINAPI_CALL THETAZ_IsWindow_Wrapper(WinHandle hwnd) {
    if (x86_dispatch_active()) return THETAZ_IsWindow(hwnd);
    return WINAPI64_IsWindow(reinterpret_cast<uint64_t>(hwnd));
}

BOOL WINAPI_CALL THETAZ_KillTimer(WinHandle hwnd, uintptr_t nIDEvent) {
    uint64_t h = hwnd ? reinterpret_cast<uint64_t>(hwnd) : kDesktopHwnd;
    std::lock_guard<std::mutex> lock(g_window_mutex);
    size_t erased = g_timers.erase((h << 32) | static_cast<uint32_t>(nIDEvent));
    g_timer_cv.notify_all();
    return erased ? TRUE_VALUE : FALSE_VALUE;
}

BOOL WINAPI_CALL THETAZ_KillTimer_Wrapper(WinHandle hwnd, uintptr_t nIDEvent) {
    if (x86_dispatch_active()) {
        return THETAZ_KillTimer(hwnd, nIDEvent);
    } else {
        return WINAPI64_KillTimer(reinterpret_cast<uint64_t>(hwnd), nIDEvent);
    }
}

int WINAPI_CALL THETAZ_MessageBoxA(WinHandle hwnd, const char* text,
                                          const char* caption, uint32_t type) {
    (void)hwnd;
    (void)type;
    fprintf(stderr, "[user32] MessageBoxA: \"%s\" title=\"%s\"\n",
            text ? text : "", caption ? caption : "");
    return 1; // IDOK
}

int WINAPI_CALL THETAZ_MessageBoxA_Wrapper(WinHandle hwnd, const char* text,
                                                  const char* caption, uint32_t type) {
    if (x86_dispatch_active()) return THETAZ_MessageBoxA(hwnd, text, caption, type);
    return WINAPI64_MessageBoxA(reinterpret_cast<uint64_t>(hwnd), text, caption, type);
}

BOOL WINAPI_CALL THETAZ_PathMatchSpecW(const uint16_t* p, const uint16_t* sp){
    if(!p||!sp) return FALSE;
    size_t pl=wcslen(reinterpret_cast<const wchar_t*>(p)),sl=wcslen(reinterpret_cast<const wchar_t*>(sp));
    if(wcscmp(reinterpret_cast<const wchar_t*>(sp),reinterpret_cast<const wchar_t*>(u"*.*"))==0)return TRUE;
    if(reinterpret_cast<const uint16_t*>(sp)[0]==0x2a&&reinterpret_cast<const uint16_t*>(sp)[sl-1]==0x2a){return wcsncasecmp(reinterpret_cast<const wchar_t*>(p),reinterpret_cast<const wchar_t*>(sp)+1,sl-2)==0;}
    if(reinterpret_cast<const uint16_t*>(sp)[0]==0x2a){return wcscasecmp(reinterpret_cast<const wchar_t*>(p)+pl-(sl-1),reinterpret_cast<const wchar_t*>(sp)+1)==0||wcscasecmp(reinterpret_cast<const wchar_t*>(p),reinterpret_cast<const wchar_t*>(sp)+1)==0;}
    return wcscasecmp(reinterpret_cast<const wchar_t*>(p),reinterpret_cast<const wchar_t*>(sp))==0;
}

BOOL WINAPI_CALL THETAZ_PeekMessageA(void* msg, WinHandle hwnd, DWORD filter_min, DWORD filter_max, DWORD remove) {
    (void)hwnd;
    if (!msg) return FALSE_VALUE;
    std::lock_guard<std::mutex> lock(g_window_mutex);
    for (auto it = g_queue.begin(); it != g_queue.end(); ++it) {
        bool matches_filter = (filter_min == 0 && filter_max == 0) ||
                              (it->message >= filter_min && it->message <= filter_max);
        if (it->message == WM_QUIT || matches_filter) {
            GuestMsg m = *it;
            if (remove) {
                g_queue.erase(it);
                if (m.message == WM_QUIT) g_quit_pending = false;
            }
            write_guest_msg(static_cast<uint8_t*>(msg), m);
            return TRUE_VALUE;
        }
    }
    return FALSE_VALUE;
}

BOOL WINAPI_CALL THETAZ_PeekMessageA_Wrapper(void* msg, WinHandle hwnd, DWORD filter_min, DWORD filter_max, DWORD remove) {
    if (x86_dispatch_active()) {
        return THETAZ_PeekMessageA(msg, hwnd, filter_min, filter_max, remove);
    } else {
        return WINAPI64_PeekMessageA(msg, reinterpret_cast<uint64_t>(hwnd), filter_min, filter_max, remove);
    }
}

BOOL WINAPI_CALL THETAZ_PostMessageA(WinHandle hwnd, DWORD message, uint64_t wparam, uint64_t lparam) {
    GuestMsg m;
    m.hwnd = hwnd ? reinterpret_cast<uint64_t>(hwnd) : kDesktopHwnd;
    m.message = message;
    m.wparam = wparam;
    m.lparam = lparam;
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        g_queue.push_back(m);
    }
    g_queue_cond.notify_all();
    return TRUE_VALUE;
}

BOOL WINAPI_CALL THETAZ_PostMessageA_Wrapper(WinHandle hwnd, DWORD message, uint64_t wparam, uint64_t lparam) {
    if (x86_dispatch_active()) {
        return THETAZ_PostMessageA(hwnd, message, wparam, lparam);
    } else {
        return WINAPI64_PostMessageA(reinterpret_cast<uint64_t>(hwnd), message, wparam, lparam);
    }
}

void WINAPI_CALL THETAZ_PostQuitMessage(int exit_code) {
    printf("[user32] PostQuitMessage(%d)\n", exit_code);
    std::lock_guard<std::mutex> lock(g_window_mutex);
    g_quit_pending = true;
    g_quit_exit_code = static_cast<DWORD>(exit_code);
    GuestMsg m;
    m.hwnd = 0;
    m.message = WM_QUIT;
    m.wparam = static_cast<uint64_t>(static_cast<uint32_t>(exit_code));
    g_queue.push_back(m);
    g_queue_cond.notify_all();
}

void WINAPI_CALL THETAZ_PostQuitMessage_Wrapper(int exit_code) {
    if (x86_dispatch_active()) {
        THETAZ_PostQuitMessage(exit_code);
    } else {
        WINAPI64_PostQuitMessage(exit_code);
    }
}

// CRT local-unwind helpers used by longjmp through SEH frames.
static void WINAPI_CALL ucrt__local_unwind2(void* frame_, int trylevel) {
    uint32_t frame = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(frame_));
    uint32_t scopetable = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(frame + 8));
    uint32_t saved_ebp = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(frame + 16));
    uint32_t lvl = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(frame + 12));
    if (!scopetable || scopetable >= 0x80000000u) return;
    while (lvl != 0xFFFFFFFFu && lvl != static_cast<uint32_t>(trylevel)) {
        const uint32_t* e =
            reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(scopetable + lvl * 12));
        uint32_t filter = e[1], handler = e[2];
        lvl = e[0];
        if (!filter && handler)
            x86_call_guest_args_ebp(handler, static_cast<uint32_t*>(nullptr), 0, saved_ebp);
    }
}

BOOL WINAPI_CALL THETAZ_SetDlgItemTextA(WinHandle hdlg, int id, const char* text) {
    uint64_t ctrl = THETAZ_GetDlgItem((uint64_t)(uintptr_t)hdlg, id);
    if (!ctrl) return FALSE_VALUE;
    THETAZ_SendMessageA(ctrl, WM_SETTEXT, 0, (uint64_t)(uintptr_t)text);
    return TRUE_VALUE;
}

BOOL WINAPI_CALL THETAZ_SetDlgItemTextA_Wrapper(WinHandle hdlg, int id, const char* text) {
    if (x86_dispatch_active()) return THETAZ_SetDlgItemTextA(hdlg, id, text);
    return WINAPI64_SetDlgItemTextA(reinterpret_cast<uint64_t>(hdlg), id, text);
}

BOOL WINAPI_CALL THETAZ_SetWindowTextA(WinHandle hwnd, const char* text) {
    if (!hwnd) return FALSE_VALUE;
    return THETAZ_SendMessageA(reinterpret_cast<uint64_t>(hwnd), WM_SETTEXT, 0, reinterpret_cast<uint64_t>(text))
        ? TRUE_VALUE : FALSE_VALUE;
}

BOOL WINAPI_CALL THETAZ_SetWindowTextA_Wrapper(WinHandle hwnd, const char* text) {
    if (x86_dispatch_active()) return THETAZ_SetWindowTextA(hwnd, text);
    return WINAPI64_SetWindowTextA(reinterpret_cast<uint64_t>(hwnd), text);
}

BOOL WINAPI_CALL THETAZ_ShowWindow(WinHandle hwnd, int cmd_show) {
    uint64_t h = reinterpret_cast<uint64_t>(hwnd);
    ::Window xw = 0;
    bool was_visible;
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        ThetaZ::Window* win = find_window(h);
        if (!win) return FALSE_VALUE;
        was_visible = win->visible;
        win->visible = (cmd_show != 0);
        auto it = g_x11_windows.find(h);
        if (it != g_x11_windows.end()) xw = it->second;
    }
    if (g_x11_display && xw) {
        std::lock_guard<std::mutex> lock(g_x11_mutex);
        if (cmd_show == 0) {
            XUnmapWindow(g_x11_display, xw);
        } else {
            XMapRaised(g_x11_display, xw);
        }
        XFlush(g_x11_display);
    }
    return was_visible ? TRUE_VALUE : FALSE_VALUE;
}

BOOL WINAPI_CALL THETAZ_ShowWindow_Wrapper(WinHandle hwnd, int cmd_show) {
    if (x86_dispatch_active()) {
        return THETAZ_ShowWindow(hwnd, cmd_show);
    } else {
        return WINAPI64_ShowWindow(reinterpret_cast<uint64_t>(hwnd), cmd_show);
    }
}

BOOL WINAPI_CALL THETAZ_TranslateMessage(const void* msg) {
    if (!msg) return FALSE_VALUE;
    const uint8_t* m = static_cast<const uint8_t*>(msg);
    uint32_t message = *reinterpret_cast<const uint32_t*>(m + MSG_MESSAGE);
    if (message == WM_KEYDOWN) {
        uint64_t wparam = *reinterpret_cast<const uint32_t*>(m + MSG_WPARAM);
        // Synthesize WM_CHAR for printable-ish keys (no keyboard state model).
        THETAZ_PostMessageA(reinterpret_cast<WinHandle>(*reinterpret_cast<const uint32_t*>(m + MSG_HWND)),
                            WM_CHAR, wparam, 1);
        return TRUE_VALUE;
    }
    return FALSE_VALUE;
}

BOOL WINAPI_CALL THETAZ_TranslateMessage_Wrapper(const void* msg) {
    if (x86_dispatch_active()) {
        return THETAZ_TranslateMessage(msg);
    } else {
        return WINAPI64_TranslateMessage(msg);
    }
}

BOOL WINAPI_CALL THETAZ_UpdateWindow(WinHandle hwnd) {
    (void)hwnd;
    return TRUE_VALUE;
}

BOOL WINAPI_CALL THETAZ_UpdateWindow_Wrapper(WinHandle hwnd) {
    if (x86_dispatch_active()) {
        return THETAZ_UpdateWindow(hwnd);
    } else {
        return WINAPI64_UpdateWindow(reinterpret_cast<uint64_t>(hwnd));
    }
}

// =========================================================================
// Missing kernel32 implementations (added for executable linking)
// =========================================================================

// Canonical DLL name for the signature table: api-ms-win-* api-sets and the
// runtime CRT aliases map onto the implementation module their exports live
// in. Kept in sync with sig_dll_name() in core/x86/api_dispatch.cpp.
static const char* canonical_sig_dll(const char* dll) {
    static thread_local std::string out;
    if (!dll) return dll;
    std::string n = dll;
    for (auto& c : n) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    if (n.compare(0, 15, "api-ms-win-crt") == 0) { out = "ucrtbase.dll"; return out.c_str(); }
    if (n.compare(0, 15, "api-ms-win-core") == 0 || n.compare(0, 15, "ext-ms-win-core") == 0) {
        out = "kernel32.dll";
        return out.c_str();
    }
    if (n == "vcruntime140.dll" || n == "vcruntime140_1.dll" || n == "vcruntime140d.dll" ||
        n.compare(0, 6, "msvcrt") == 0) { out = "ucrtbase.dll"; return out.c_str(); }
    out = dll;
    return out.c_str();
}

FARPROC WINAPI_CALL THETAZ_GetProcAddress(HMODULE hModule, LPCSTR lpProcName) {
    if (!hModule || !lpProcName) return nullptr;
    // A synthetic builtin module handle (api-ms-win-* api-sets and DLLs we
    // serve internally) must resolve exports under the name the guest asked
    // for, not only under the handful of hardcoded DLL names below. OfficeSetup
    // binds Fiber-Local-Storage through api-ms-win-core-fibers-l1-1-1, which
    // was not in that list, so FlsGetValue/FlsAlloc returned a stub and the
    // installer's retry loop spun on a function that returns no value.
    const char* synth_dll = builtin_handle_name((void*)(uintptr_t)hModule);
    if (synth_dll) {
        std::string synth_name = synth_dll;
        // The registry keys carry a .dll suffix; api-set handles do not.
        if (synth_name.size() < 4 ||
            synth_name.compare(synth_name.size() - 4, 4, ".dll") != 0) {
            synth_name += ".dll";
        }
        if ((uintptr_t)lpProcName < 0x10000) {
            uint16_t ord = (uint16_t)(uintptr_t)lpProcName;
            void* fn = ApiRegistry::instance().find_function_ordinal(synth_name, ord);
            if (fn) return (FARPROC)fn;
        } else {
            void* fn = ApiRegistry::instance().find_function(synth_name, lpProcName);
            if (fn) {
                if (guest_is_32bit()) {
                    // Allocate the thunk under the canonical DLL name the
                    // signature table is keyed by (kernel32.dll, ucrtbase.dll,
                    // ... for the api-ms-win-* sets). Passing the api-set name
                    // here would leave the thunk with no signature row, i.e.
                    // an argc of 0 that reads no arguments and pops none.
                    uint32_t thunk = alloc_api_thunk(fn, 0, true,
                                                     canonical_sig_dll(synth_name.c_str()),
                                                     lpProcName);
                    if (thunk) return (FARPROC)(uintptr_t)thunk;
                }
                return (FARPROC)fn;
            }
        }
    }
    PELoader& loader = PELoader::instance();
    LoadedModule* guest_mod = loader.find_module_by_base((uint64_t)(uintptr_t)hModule);
    if (guest_mod) {
        if ((uintptr_t)lpProcName < 0x10000) {
            uint16_t ord = (uint16_t)(uintptr_t)lpProcName;
            void* fn = loader.get_export_address(*guest_mod, ord, nullptr);
            if (fn) return (FARPROC)fn;
        } else {
            void* fn = loader.get_export_address(*guest_mod, 0, lpProcName);
            if (fn) return (FARPROC)fn;
        }
    }
    if ((uintptr_t)lpProcName < 0x10000) {
        uint16_t ord = (uint16_t)(uintptr_t)lpProcName;
        const char* dlls[] = {"kernel32.dll","kernelbase.dll","ntdll.dll","user32.dll","gdi32.dll","ucrtbase.dll","msvcrt.dll","vcruntime140.dll","api-ms-win-crt-string-l1-1-0.dll",nullptr};
        for (int i=0; dlls[i]; i++) {
            void* fn = ApiRegistry::instance().find_function_ordinal(dlls[i], ord);
            if (fn) {
                const char* real = ApiRegistry::instance().name_for_ordinal(dlls[i], ord);
                std::string ords = std::to_string(ord);
                const char* nm = real ? real : ords.c_str();
                if (guest_is_32bit()) {
                    uint32_t thunk = alloc_api_thunk(fn, 0, true, dlls[i], nm);
                    if (thunk) return (FARPROC)(uintptr_t)thunk;
                }
                return (FARPROC)fn;
            }
        }
        if (guest_is_32bit()) {
            std::string ords = std::to_string(ord);
            uint32_t thunk = alloc_api_thunk((void*)generic_api_stub, 0, true, "kernel32.dll", ords.c_str());
            if (thunk) return (FARPROC)(uintptr_t)thunk;
        }
        return (FARPROC)(uintptr_t)generic_api_stub;
    }
    std::string name(lpProcName);
    const char* dlls[] = {"kernel32.dll","kernelbase.dll","ntdll.dll","user32.dll","gdi32.dll","ucrtbase.dll","msvcrt.dll","vcruntime140.dll","api-ms-win-crt-string-l1-1-0.dll","api-ms-win-crt-heap-l1-1-0.dll","ole32.dll","oleaut32.dll","advapi32.dll","shlwapi.dll","version.dll","bcrypt.dll","crypt32.dll","winhttp.dll","wininet.dll","urlmon.dll","msi.dll","gdiplus.dll","uxtheme.dll","setupapi.dll","cabinet.dll","wintrust.dll","ws2_32.dll",nullptr};
    for (int i=0; dlls[i]; i++) {
        void* fn = ApiRegistry::instance().find_function(dlls[i], name);
        if (fn) {
            if (guest_is_32bit()) {
                uint32_t thunk = alloc_api_thunk(fn, 0, true, dlls[i], name.c_str());
                if (thunk) return (FARPROC)(uintptr_t)thunk;
            }
            return (FARPROC)fn;
        }
    }
    // An export we do not implement. Windows reports failure here: the
    // function pointer is NULL and the last error is ERROR_PROC_NOT_FOUND.
    // Returning a stub instead (the previous behaviour) gives the caller a
    // pointer it may call — a host function that returns 0 having read no
    // arguments — which is what corrupted Wine DLLs that resolve their own
    // debug exports (__wine_dbg_*) through GetProcAddress and then call them.
    THETAZ_SetLastError(127 /* ERROR_PROC_NOT_FOUND */);
    return nullptr;
}

LPVOID WINAPI_CALL THETAZ_HeapAlloc(HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes) {
    (void)hHeap;
    if (dwBytes == 0) return nullptr;
    LPVOID ptr = alloc_guest_heap(dwBytes);
    if (ptr && (dwFlags & HEAP_ZERO_MEMORY)) {
        memset(ptr, 0, dwBytes);
    }
    return ptr;
}

LPVOID WINAPI_CALL THETAZ_HeapReAlloc(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem, SIZE_T dwBytes) {
    (void)hHeap;
    if (!lpMem) return THETAZ_HeapAlloc(nullptr, dwFlags, dwBytes);
    if (dwBytes == 0) {
        free_guest_heap(lpMem);
        return nullptr;
    }
    LPVOID newPtr = realloc_guest_heap(lpMem, dwBytes);
    if (newPtr && (dwFlags & HEAP_ZERO_MEMORY)) {
        // Zero the newly allocated portion (approximate)
        memset(newPtr, 0, dwBytes);
    }
    return newPtr;
}

BOOL WINAPI_CALL THETAZ_FreeLibraryWhenCallbackReturns(HMODULE hModule) {
    (void)hModule;
    return TRUE;
}

BOOL WINAPI_CALL THETAZ_InitializeProcThreadAttributeList(void* lpAttributeList, DWORD dwAttributeCount, DWORD dwFlags, SIZE_T* lpSize) {
    (void)lpAttributeList; (void)dwAttributeCount; (void)dwFlags; (void)lpSize;
    return FALSE;
}

BOOL WINAPI_CALL THETAZ_UpdateProcThreadAttribute(void* lpAttributeList, DWORD dwFlags, DWORD_PTR Attribute, void* lpValue, SIZE_T cbSize, void* lpPreviousValue, SIZE_T* lpReturnSize) {
    (void)lpAttributeList; (void)dwFlags; (void)Attribute; (void)lpValue; (void)cbSize; (void)lpPreviousValue; (void)lpReturnSize;
    return FALSE;
}

BOOL WINAPI_CALL THETAZ_SetThreadpoolThreadMaximum(void* ptpp, DWORD cthrdMost) {
    (void)ptpp; (void)cthrdMost;
    return TRUE;
}

BOOL WINAPI_CALL THETAZ_SetThreadpoolThreadMinimum(void* ptpp, DWORD cthrdMic) {
    (void)ptpp; (void)cthrdMic;
    return TRUE;
}

BOOL WINAPI_CALL THETAZ_TrySubmitThreadpoolCallback(void* pfns, void* pv, void* pcbe) {
    (void)pfns; (void)pv; (void)pcbe;
    return FALSE;
}

std::string utf8_to_utf16le(const std::string& in, std::u16string& out) {
    out.clear();
    for (char c : in) out.push_back(static_cast<char16_t>(c));
    return in;
}

std::string utf16le_to_utf8(const uint16_t* s) {
    if (!s) return "";
    std::string r;
    while (*s) { r.push_back(static_cast<char>(*s & 0xFF)); s++; }
    return r;
}

static bool name_is_builtin(const std::string& path) {
    if (path.compare(0, 11, "api-ms-win-") == 0 ||
        path.compare(0, 11, "ext-ms-win-") == 0) {
        return true;
    }
    return ApiRegistry::instance().has_dll(path);
}

// Base address of the image the calling guest process started from. Child
// processes run fresh copies at their own bases; the main process (pid 0) uses
// the loader's main image base.
static uint64_t current_guest_image_base() {
    uint64_t pid = ThreadManager::instance().get_current_guest_pid();
    if (pid != 0) {
        uint64_t base = get_guest_process_image_base(pid);
        if (base) return base;
    }
    return get_main_module_base();
}

static std::string module_basename_lower(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    std::string base = (slash == std::string::npos) ? path : path.substr(slash + 1);
    for (char& c : base) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    return base;
}

// Translate a module name to its loaded base address using the same rules as
// GetModuleHandle: NULL/empty selects the calling process's main image; a full
// path is matched exactly; a bare name is matched case-insensitively against
// loaded module basenames. Builtin API DLLs (kernel32, ntdll, ...) do not have
// a mapped image, so they get their synthetic module handle.
static HMODULE module_handle_from_name(const char* raw_name) {
    std::string name = raw_name ? raw_name : "";
    PELoader& loader = PELoader::instance();
    uint64_t image_base = current_guest_image_base();

    if (name.empty()) return (HMODULE)(uintptr_t)image_base;

    if (LoadedModule* image = loader.find_module_by_base(image_base)) {
        std::string requested = name;
        for (char& c : requested) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        std::string loaded = image->name;
        for (char& c : loaded) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        bool main_match = (requested == loaded);
        if (!main_match && requested.find_first_of("/\\") == std::string::npos) {
            main_match = (module_basename_lower(image->name) == requested);
        }
        if (main_match) return (HMODULE)(uintptr_t)image_base;
    }

    LoadedModule* m = loader.find_module(name.c_str());
    if (!m && name.find_first_of("/\\") == std::string::npos) {
        m = loader.find_module_named(name.c_str());
    }
    if (m) return (HMODULE)(uintptr_t)m->base;

    if (name_is_builtin(name)) {
        if (void* h = get_builtin_module_handle(name.c_str())) {
            return (HMODULE)h;
        }
    }

    THETAZ_SetLastError(ERROR_MOD_NOT_FOUND);
    return nullptr;
}

HMODULE WINAPI_CALL THETAZ_LoadLibraryW(LPCWSTR p) {
    if (!p) return nullptr;
    std::string path = utf16le_to_utf8(reinterpret_cast<const uint16_t*>(p));
    if (path.empty()) return nullptr;
    if (name_is_builtin(path)) {
        LoadedModule* m = ThetaZ::PELoader::instance().find_module_named(path.c_str());
        if (!m) return (HMODULE)get_builtin_module_handle(path.c_str());
        return (HMODULE)(uintptr_t)m->base;
    }
    return (HMODULE)load_guest_module(path.c_str());
}
HMODULE WINAPI_CALL THETAZ_LoadLibraryA(LPCSTR p) {
    if (!p) return nullptr;
    std::string path = p;
    if (name_is_builtin(path)) {
        LoadedModule* m = ThetaZ::PELoader::instance().find_module_named(p);
        if (!m) return (HMODULE)get_builtin_module_handle(p);
        return (HMODULE)(uintptr_t)m->base;
    }
    return (HMODULE)load_guest_module(p);
}
HMODULE WINAPI_CALL THETAZ_GetModuleHandleA(LPCSTR p) {
    if (!p) return module_handle_from_name(nullptr);
    return module_handle_from_name(p);
}
BOOL WINAPI_CALL THETAZ_FreeLibrary(HMODULE h) { (void)h; return TRUE; }
HMODULE WINAPI_CALL THETAZ_LoadLibraryExW(LPCWSTR p, HANDLE hFile, DWORD flags) {
    (void)hFile;
    if (!p) return nullptr;
    std::string path = utf16le_to_utf8(reinterpret_cast<const uint16_t*>(p));
    if (path.empty()) return nullptr;
    if (flags & 0x2) {
        // LOAD_LIBRARY_AS_DATAFILE: no execution, just map. Return a synthetic handle.
        return (HMODULE)get_builtin_module_handle(path.c_str());
    }
    if (name_is_builtin(path)) {
        LoadedModule* m = ThetaZ::PELoader::instance().find_module_named(path.c_str());
        if (!m) return (HMODULE)get_builtin_module_handle(path.c_str());
        return (HMODULE)(uintptr_t)m->base;
    }
    return (HMODULE)load_guest_module(path.c_str());
}
HMODULE WINAPI_CALL THETAZ_GetModuleHandleW(LPCWSTR p) {
    if (!p) return module_handle_from_name(nullptr);
    std::string name = utf16le_to_utf8(reinterpret_cast<const uint16_t*>(p));
    return module_handle_from_name(name.c_str());
}

// Map a host path to a guest-visible Windows path. The configured drive_c
// directory maps to C:; every other host path is exposed under the Z: drive so
// that callers get a stable, round-trippable Windows path instead of a lie.
static std::string host_to_guest_path(const std::string& host) {
    if (host.empty()) return host;
    std::string drive_c = get_drive_c_path();
    while (drive_c.size() > 1 && drive_c.back() == '/') drive_c.pop_back();
    std::string p;
    if (!drive_c.empty() && host.compare(0, drive_c.size(), drive_c) == 0 &&
        (host.size() == drive_c.size() || host[drive_c.size()] == '/')) {
        p = "C:" + host.substr(drive_c.size());
    } else if (host[0] == '/') {
        p = "Z:" + host;
    } else {
        p = "Z:\\" + host;
    }
    for (char& c : p) if (c == '/') c = '\\';
    return p;
}

static std::string guest_current_directory() {
    char buf[PATH_MAX];
    if (!getcwd(buf, sizeof(buf))) return "C:\\";
    return host_to_guest_path(buf);
}

// Lexically normalize a guest Windows path: drive prefixes, rooted vs
// relative paths, mixed separators, and . / .. components. No disk access.
static std::string guest_normalize_path(const std::string& in) {
    if (in.empty()) return in;
    std::string s = in;
    for (char& c : s) if (c == '/') c = '\\';

    std::string drive;
    std::string rest;
    if (s.size() >= 2 &&
        ((s[0] >= 'A' && s[0] <= 'Z') || (s[0] >= 'a' && s[0] <= 'z')) &&
        s[1] == ':') {
        drive = s.substr(0, 2);
        rest = s.substr(2);
    } else {
        rest = s;
    }
    bool rooted = !rest.empty() && rest[0] == '\\';
    std::string cwd;
    if (drive.empty()) {
        cwd = guest_current_directory();
        drive = cwd.substr(0, 2);
        if (!rooted) {
            rest = cwd.substr(2) + "\\" + rest;
        }
    }
    std::vector<std::string> comps;
    std::string cur;
    for (char c : rest) {
        if (c == '\\') {
            if (!cur.empty()) { comps.push_back(cur); cur.clear(); }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) comps.push_back(cur);
    std::vector<std::string> out;
    for (const std::string& comp : comps) {
        if (comp == ".") continue;
        if (comp == "..") {
            if (!out.empty()) out.pop_back();
            continue;
        }
        out.push_back(comp);
    }
    std::string result = drive;
    result += "\\";
    for (size_t i = 0; i < out.size(); ++i) {
        if (i) result += "\\";
        result += out[i];
    }
    return result;
}

static DWORD module_file_name(HMODULE h, void* buf, DWORD nSize, bool wide) {
    uint64_t base = h ? static_cast<uint64_t>(reinterpret_cast<uintptr_t>(h))
                      : get_main_module_base();
    std::string path;
    LoadedModule* m = PELoader::instance().find_module_by_base(base);
    if (m && m->name[0]) {
        path = host_to_guest_path(m->name);
    } else if (const char* bn = builtin_handle_name(
                   reinterpret_cast<void*>(static_cast<uintptr_t>(base)))) {
        path = std::string("C:\\Windows\\System32\\") + bn;
    }
    if (path.empty()) {
        THETAZ_SetLastError(126); // ERROR_MOD_NOT_FOUND
        return 0;
    }
    const size_t len = path.size();
    if (!buf || nSize == 0) {
        THETAZ_SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return static_cast<DWORD>(len + 1);
    }
    size_t copy = len;
    if (copy + 1 > nSize) copy = nSize - 1;
    if (wide) {
        auto* w = static_cast<uint16_t*>(buf);
        std::u16string u16;
        utf8_to_utf16le(path, u16);
        size_t n = u16.size();
        if (n > copy) n = copy;
        for (size_t i = 0; i < n; ++i) w[i] = static_cast<uint16_t>(u16[i]);
        w[n] = 0;
        copy = n;
    } else {
        auto* a = static_cast<char*>(buf);
        memcpy(a, path.c_str(), copy);
        a[copy] = 0;
    }
    if (len + 1 > nSize) {
        THETAZ_SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return nSize;
    }
    return static_cast<DWORD>(copy);
}

DWORD WINAPI_CALL THETAZ_GetModuleFileNameW(HMODULE h, LPWSTR b, DWORD n) {
    return module_file_name(h, b, n, true);
}
DWORD WINAPI_CALL THETAZ_GetModuleFileNameA(HMODULE h, LPSTR b, DWORD n) {
    return module_file_name(h, b, n, false);
}

static DWORD full_path_name(const std::string& in, DWORD nBufferLength,
                            void* lpBuffer, void** lpFilePart, bool wide) {
    std::string full = guest_normalize_path(in);
    if (full.empty()) {
        THETAZ_SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    const DWORD len = static_cast<DWORD>(full.size());
    if (lpFilePart) *lpFilePart = nullptr;
    if (!lpBuffer || nBufferLength == 0) return len + 1;
    if (len + 1 > nBufferLength) return len + 1;
    if (lpFilePart) {
        size_t pos = full.find_last_of('\\');
        if (pos != std::string::npos && pos + 1 < full.size()) {
            uint8_t* p = static_cast<uint8_t*>(lpBuffer) + (wide ? (pos + 1) * 2 : (pos + 1));
            *lpFilePart = p;
        }
    }
    if (wide) {
        auto* w = static_cast<uint16_t*>(lpBuffer);
        std::u16string u16;
        utf8_to_utf16le(full, u16);
        for (size_t i = 0; i < u16.size(); ++i) w[i] = static_cast<uint16_t>(u16[i]);
        w[u16.size()] = 0;
    } else {
        memcpy(lpBuffer, full.c_str(), full.size() + 1);
    }
    return len;
}

DWORD WINAPI_CALL THETAZ_GetFullPathNameW(LPCWSTR lpFileName, DWORD nBufferLength,
                                          LPWSTR lpBuffer, LPWSTR* lpFilePart) {
    if (!lpFileName) { THETAZ_SetLastError(ERROR_INVALID_PARAMETER); return 0; }
    std::string in = utf16le_to_utf8(reinterpret_cast<const uint16_t*>(lpFileName));
    return full_path_name(in, nBufferLength, lpBuffer,
                          reinterpret_cast<void**>(lpFilePart), true);
}
DWORD WINAPI_CALL THETAZ_GetFullPathNameA(LPCSTR lpFileName, DWORD nBufferLength,
                                          LPSTR lpBuffer, LPSTR* lpFilePart) {
    if (!lpFileName) { THETAZ_SetLastError(ERROR_INVALID_PARAMETER); return 0; }
    return full_path_name(lpFileName, nBufferLength, lpBuffer,
                          reinterpret_cast<void**>(lpFilePart), false);
}

static DWORD current_directory(DWORD nBufferLength, void* lpBuffer, bool wide) {
    std::string cwd = guest_current_directory();
    const DWORD len = static_cast<DWORD>(cwd.size());
    if (!lpBuffer || nBufferLength == 0) return len + 1;
    if (len + 1 > nBufferLength) return len + 1;
    if (wide) {
        auto* w = static_cast<uint16_t*>(lpBuffer);
        std::u16string u16;
        utf8_to_utf16le(cwd, u16);
        for (size_t i = 0; i < u16.size(); ++i) w[i] = static_cast<uint16_t>(u16[i]);
        w[u16.size()] = 0;
    } else {
        memcpy(lpBuffer, cwd.c_str(), cwd.size() + 1);
    }
    return len;
}

DWORD WINAPI_CALL THETAZ_GetCurrentDirectoryW(DWORD nBufferLength, LPWSTR lpBuffer) {
    return current_directory(nBufferLength, lpBuffer, true);
}
DWORD WINAPI_CALL THETAZ_GetCurrentDirectoryA(DWORD nBufferLength, LPSTR lpBuffer) {
    return current_directory(nBufferLength, lpBuffer, false);
}
// ---------------------------------------------------------------------------
// x86 SEH dispatcher. Guest frames are linked through fs:[0] as
// { prev, handler } pairs (MSVC also keeps handler-specific data after the
// pair). Handlers are guest-callable addresses (builtin handlers arrive as
// API thunks), called as
//   disposition handler(EXCEPTION_RECORD*, EstablisherFrame, CONTEXT*, DISPATCHER_CONTEXT*)
// x86 CONTEXT layout (Windows): Eip@0xB8 Ebp@0xB4 Esp@0xC4 Edx@0xA8.
// The ThetaZ DISPATCHER_CONTEXT layout used by the ucrt handlers is:
//   ControlPc@0x00 ImageBase@0x08 EstablisherFrame@0x10
//   TargetIp@0x20 HandlerData@0x38
// ---------------------------------------------------------------------------
static bool maps_range_readable(uintptr_t start, size_t len);

// True when a frame's third word is a C++ FuncInfo (first field is one of the
// EH magics) rather than a classic SEH scope table. MSVC's x86 C++ handler
// chain needs that FuncInfo in EAX; a classic __except_handler3/4 frame's third
// word is a scope table and seeding it would corrupt the handler's inputs.
// The pointer is validated before it is dereferenced: a frame record's third
// word can hold anything, and dereferencing garbage would take down the host
// (this crashed the process during bring-up).
static bool looks_like_func_info(uint32_t p) {
    if (p < 0x400000u || !maps_range_readable(p, 4)) return false;
    uint32_t magic = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(p));
    return magic == 0x19930520u || magic == 0x19930521u || magic == 0x19930522u;
}

// MSVC x86 SEH registration nodes come in two layouts in practice:
//
//   4-word node (FuncInfo-style, node at [ebp-0x10]):
//     +0 prev, +4 handler, +8 encoded FuncInfo, +c trylevel, +10 saved EBP
//     __CxxFrameHandler3 reads +8/+c and the function's EBP is frame+0x10.
//
//   3-word node (__ehhandler thunk style, node at [ebp-0xc]):
//     +0 prev, +4 handler, +8 state/trylevel, +c saved EBP
//     The function's EBP is frame+0xc.
//
// Guessing from a single field is unreliable: the 4-word node's +c is a small
// try level, while the 3-word node's +10 is a return address in the image.
// Validate the candidate saved-EBP chain instead and require it to stay within
// the same stack region as the frame we are walking.
struct SehFrameLayout {
    uint32_t current_ebp = 0;
    uint32_t saved_ebp = 0;
    bool valid = false;
};

static SehFrameLayout seh_frame_layout(uint32_t frame, uint32_t walk_ebp) {
    SehFrameLayout none;
    none.current_ebp = walk_ebp;
    none.saved_ebp = walk_ebp;
    if (frame < 0x1000u || frame >= 0x80000000u || (frame & 3u) != 0) return none;
    if (!maps_range_readable(static_cast<uintptr_t>(frame), 0x14)) return none;

    auto read32 = [](uint32_t a) -> uint32_t {
        return *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(a));
    };
    constexpr uint32_t kMaxFrameSpan = 0x01000000u; // 16 MiB of stack frames
    auto plausible_saved = [&](uint32_t saved) -> bool {
        if (saved <= walk_ebp || saved >= 0x7FFF0000u) return false;
        if ((saved & 3u) != 0) return false;
        if (saved - walk_ebp >= kMaxFrameSpan) return false;
        if (!maps_range_readable(static_cast<uintptr_t>(saved), 8)) return false;
        uint32_t next = read32(saved);
        if (next == 0) return true; // bottom of the walked stack
        if (next <= saved || next >= 0x7FFF0000u) return false;
        if (next - saved >= kMaxFrameSpan) return false;
        return maps_range_readable(static_cast<uintptr_t>(next), 4);
    };

    const uint32_t saved4 = read32(frame + 0x10);
    const uint32_t saved3 = read32(frame + 0x0c);
    const bool ok4 = plausible_saved(saved4);
    const bool ok3 = plausible_saved(saved3);
    if (ok4 && !ok3) return {frame + 0x10, saved4, true};
    if (ok3 && !ok4) return {frame + 0x0c, saved3, true};
    if (ok4 && ok3) {
        // Both candidates point into the stack region. The real saved EBP is
        // the nearest valid frame; the other slot is either a return address
        // that happens to be nearby or a stale value.
        if (saved4 < saved3) return {frame + 0x10, saved4, true};
        return {frame + 0x0c, saved3, true};
    }
    // Neither slot holds a plausible saved EBP. That is the shape of an MSVC
    // __except_handler4 node, whose words are
    //   +0x00 prev, +0x04 handler, +0x08 scopetable ^ __security_cookie,
    //   +0x0C trylevel, +0x10 first context word
    // so +0x0C holds a trylevel (commonly 0) and +0x10 is not a frame pointer
    // at all. Detecting it matters: MSVC's _CxxFrameHandler takes its CONTEXT*
    // from its own argument slot, but the emulator still needs a usable EBP to
    // build the handler's frame-relative state, and falling back to the outer
    // frame's EBP here silently shifted every scope lookup by one frame.
    //
    // The cookie-encoded scopetable at +0x08 is the reliable discriminator:
    // decoding it against the image's __security_cookie yields a pointer into
    // .rdata, whereas a classic 4-word node's +0x08 is a trylevel.
    if (read32(frame + 0x08) != 0) {
        uint32_t decoded = read32(frame + 0x08);
        for (uint32_t base : {0x400000u, 0x21600000u, 0x20000000u}) {
            if (base >= 0x80000000u) continue;
            uint32_t cookie = 0;
            if (maps_range_readable(static_cast<uintptr_t>(base + 0x66E780u), 4)) {
                cookie = read32(base + 0x66E780u);
            }
            uint32_t table = decoded ^ cookie;
            if (cookie && maps_range_readable(static_cast<uintptr_t>(table), 8) &&
                table >= base && table < base + 0x800000u) {
                // Confirmed handler4: the establisher's frame pointer is the
                // nearest word BELOW this node on the stack, so use the walk
                // EBP the caller supplied rather than inventing one.
                return {0, walk_ebp, false};
            }
        }
    }
    return none;
}

// Context of the most recent software exception on this host thread. RtlUnwind
// is entered from guest cleanup code, so the guest CPU state no longer points
// at the original throw site; the dispatcher needs this snapshot to build the
// corresponding CONTEXT for each unwind handler.
struct SavedExceptionContext {
    uint32_t fault_pc = 0;
    uint32_t fault_esp = 0;
    uint32_t fault_ebp = 0;
    uint32_t fs_base = 0;
    uint32_t record = 0;
};
static thread_local SavedExceptionContext g_saved_exception;

// Where the exception being dispatched originated. Software RaiseException
// calls and host faults inside an API dispatch both have a guest call frame
// (with stdcall arguments) under them; a hardware fault in guest code has
// none, and its control PC is the faulting instruction itself.
enum class RaiseOrigin {
    Api,           // guest called RaiseException (or a C++ throw helper)
    HardwareGuest, // host signal while interpreting a guest instruction
    HardwareApi,   // host signal inside a Win32 API called by the guest
};
static thread_local RaiseOrigin g_raise_origin = RaiseOrigin::Api;

static uint32_t image_base_for_address(uint32_t addr) {
    for (const auto& entry : PELoader::instance().get_modules()) {
        const LoadedModule& m = entry.second;
        if (addr >= m.base && addr < m.base + m.size) {
            return static_cast<uint32_t>(m.base);
        }
    }
    return 0;
}

void WINAPI_CALL THETAZ_RaiseException(DWORD code, DWORD flags, DWORD nargs, void* lpArguments) {
    const RaiseOrigin origin = g_raise_origin;
    g_raise_origin = RaiseOrigin::Api;
    const bool hardware_guest = (origin == RaiseOrigin::HardwareGuest);
    if (!guest_is_32bit() ||
        (!x86_dispatch_active() && origin == RaiseOrigin::Api)) {
        // Software RaiseException must arrive through an API dispatch. Hardware
        // faults are delivered by the interpreter even when no dispatch is in
        // flight; anything else is a host-side programming error.
        if (guest_is_32bit() && getenv("THETAZ_TRACE_SEH")) {
            fprintf(stderr, "[seh] RaiseException(0x%08X) without an x86 dispatch context\n",
                    static_cast<unsigned>(code));
        }
        return;
    }
    X86CpuState* st = x86_current_state();
    if (!st) return;

    if (getenv("THETAZ_TRACE_SEH")) {
        auto rd = [](uint32_t a) -> uint32_t {
            return *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(a));
        };
        const bool ok_orig = maps_range_readable(0xA6E780u, 4);
        const bool ok_child = maps_range_readable(0x21C6E780u, 4);
        const uint32_t c_orig = ok_orig ? rd(0xA6E780u) : 0;
        const uint32_t c_child = ok_child ? rd(0x21C6E780u) : 0;
        const uint32_t ebp = st->regs[XR_EBP];
        const uint32_t enc = maps_range_readable(ebp - 8, 4) ? rd(ebp - 8) : 0;
        const uint32_t dec = enc ^ c_child;
        fprintf(stderr,
                "[seh] cookie original@0xA6E780=%s0x%08X child@0x21C6E780=%s0x%08X "
                "enc[ebp-8]=0x%08X dec_original=0x%08X dec_child=0x%08X\n",
                ok_orig ? "" : "unmapped ", c_orig, ok_child ? "" : "unmapped ", c_child,
                enc, enc ^ c_orig, dec);
        if (maps_range_readable(dec, 0x60)) {
            fprintf(stderr, "[seh] func@0x%08X:", dec);
            for (int i = 0; i < 24; i++) {
                fprintf(stderr, " 0x%08X", rd(dec + i * 4));
            }
            fprintf(stderr, "\n");
        }
        if (maps_range_readable(0x21691917u, 0x60)) {
            const uint32_t push_imm = rd(0x2169191du);
            const uint32_t cookie_imm = rd(0x21691936u);
            fprintf(stderr,
                    "[seh] child code push_imm=0x%08X cookie_imm=0x%08X "
                    "bytes69=%02X%02X bytesA1=%02X\n",
                    push_imm, cookie_imm,
                    *reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(0x21691917u)),
                    *reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(0x21691918u)),
                    *reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(0x21691935u)));
        }
    }

    const uint32_t fault_pc = st->eip;
    const uint32_t fault_esp = st->regs[XR_ESP];
    g_saved_exception.fault_pc = fault_pc;
    g_saved_exception.fault_esp = fault_esp;
    g_saved_exception.fault_ebp = st->regs[XR_EBP];
    g_saved_exception.fs_base = st->fs_base;

    // EXCEPTION_RECORD embeds ExceptionInformation[15] inline at 0x14, so the
    // RaiseException arguments must be copied into the record, not referenced.
    uint32_t rec = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(alloc_guest_shared(0x50)));
    if (!rec) return;
    uint32_t args_guest = 0;
    if (nargs > 15) nargs = 15;
    if (nargs && lpArguments) {
        args_guest = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(alloc_guest_shared(nargs * 4)));
        if (args_guest) memcpy(reinterpret_cast<void*>(static_cast<uintptr_t>(args_guest)),
                               lpArguments, nargs * 4);
    }
    auto w32 = [](uint32_t addr, uint32_t v) {
        *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(addr)) = v;
    };
    w32(rec + 0x00, code);
    w32(rec + 0x04, flags);
    w32(rec + 0x08, 0);        // ExceptionRecord (nested)
    w32(rec + 0x0C, fault_pc); // ExceptionAddress
    w32(rec + 0x10, nargs);
    // EXCEPTION_RECORD.ExceptionInformation is a FIXED 15-slot array (60 bytes).
    // Windows raises with 3 parameters for a C++ throw, yet MSVC's
    // __CxxFrameHandler reads ExceptionInformation[3] and bails out when it is
    // nonzero ("cmp [record+0x1C],0; jne decline"). Leaving the slots past
    // nargs as recycled guest-buffer garbage made every C++ handler decline the
    // exception, so clear the whole array first and then copy the real args.
    memset(reinterpret_cast<void*>(static_cast<uintptr_t>(rec + 0x14)), 0, 15 * 4);
    if (args_guest && nargs) {
        memcpy(reinterpret_cast<void*>(static_cast<uintptr_t>(rec + 0x14)),
               reinterpret_cast<void*>(static_cast<uintptr_t>(args_guest)), nargs * 4);
    }
    g_saved_exception.record = rec;

    uint32_t ctx = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(alloc_guest_shared(0x2CC)));
    if (!ctx) return;
    memset(reinterpret_cast<void*>(static_cast<uintptr_t>(ctx)), 0, 0x2CC);
    w32(ctx + 0x00, 0x00010007); // CONTEXT_i386 | CONTROL | INTEGER
    w32(ctx + 0x9C, st->regs[XR_EDI]);
    w32(ctx + 0xA0, st->regs[XR_ESI]);
    w32(ctx + 0xA4, st->regs[XR_EBX]);
    w32(ctx + 0xA8, st->regs[XR_EDX]);
    w32(ctx + 0xAC, st->regs[XR_ECX]);
    w32(ctx + 0xB0, st->regs[XR_EAX]);
    w32(ctx + 0xB4, st->regs[XR_EBP]);
    w32(ctx + 0xB8, fault_pc);
    *reinterpret_cast<uint16_t*>(static_cast<uintptr_t>(ctx + 0xBC)) = 0x23; // SegCs
    w32(ctx + 0xC0, st->eflags);
    w32(ctx + 0xC4, fault_esp);
    *reinterpret_cast<uint16_t*>(static_cast<uintptr_t>(ctx + 0xC8)) = 0x2B; // SegSs
    w32(ctx + 0x90, 0x3B); // SegFs
    w32(ctx + 0x94, 0x2B); // SegEs
    w32(ctx + 0x98, 0x2B); // SegDs

    uint32_t disp = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(alloc_guest_shared(0x40)));
    if (!disp) return;
    memset(reinterpret_cast<void*>(static_cast<uintptr_t>(disp)), 0, 0x40);

    // Each guest thread owns its own TEB. Reading the fixed main-thread TEB
    // here would dispatch another thread's exception chain (and has caused
    // real C++ exceptions on child threads to terminate via int3).
    const uint32_t fs_head = st->fs_base
        ? *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(st->fs_base))
        : 0;
    const bool seh_trace = getenv("THETAZ_TRACE_SEH") != nullptr;
    if (seh_trace) {
        fprintf(stderr, "[seh] raise code=0x%08X flags=0x%X nargs=%u pc=0x%08X esp=0x%08X ebp=0x%08X head=0x%08X rec=0x%08X ctx=0x%08X disp=0x%08X\n",
                (unsigned)code, (unsigned)flags, (unsigned)nargs, fault_pc, fault_esp,
                st->regs[XR_EBP], fs_head, rec, ctx, disp);
        if (args_guest) {
            for (DWORD i = 0; i < nargs && i < 8; i++) {
                fprintf(stderr, "[seh]   info[%u]=0x%08X\n", (unsigned)i,
                        *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(args_guest + i * 4)));
            }
        }
        if (code == 0xE06D7363u && args_guest && nargs >= 3) {
            uint32_t obj = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(args_guest + 4));
            uint32_t ti = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(args_guest + 8));
            if (maps_range_readable(obj, 16)) {
                fprintf(stderr, "[seh]   object@0x%08X:", obj);
                for (int i = 0; i < 4; i++) {
                    fprintf(stderr, " 0x%08X",
                            *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(obj + i * 4)));
                }
                fprintf(stderr, "\n");
            }
            if (maps_range_readable(ti, 32)) {
                fprintf(stderr, "[seh]   throwinfo@0x%08X:", ti);
                for (int i = 0; i < 8; i++) {
                    fprintf(stderr, " 0x%08X",
                            *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ti + i * 4)));
                }
                fprintf(stderr, "\n");
                uint32_t ca = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ti + 12));
                if (maps_range_readable(ca, 4)) {
                    int n = *reinterpret_cast<int*>(static_cast<uintptr_t>(ca));
                    fprintf(stderr, "[seh]   catchable=%d", n);
                    for (int i = 0; i < n && i < 8; i++) {
                        uint32_t ct = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ca + 4 + i * 4));
                        if (!maps_range_readable(ct, 20)) continue;
                        uint32_t td = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ct + 4));
                        if (maps_range_readable(td + 8, 1)) {
                            fprintf(stderr, " '%s'", reinterpret_cast<const char*>(static_cast<uintptr_t>(td + 8)));
                        }
                    }
                    fprintf(stderr, "\n");
                }
            }
        }
        uint32_t f = fs_head;
        for (int i = 0; i < 8 && f >= 0x1000 && f < 0x80000000u; i++) {
            fprintf(stderr, "[seh]   frame[%d]=0x%08X prev=0x%08X handler=0x%08X "
                            "+8=0x%08X +c=0x%08X +10=0x%08X\n",
                    i, f,
                    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(f)),
                    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(f + 4)),
                    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(f + 8)),
                    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(f + 12)),
                    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(f + 16)));
            f = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(f));
        }
    }

    // Only the innermost frame corresponds to the faulting instruction. Outer
    // frames are reached through the caller's EBP and use its return address.
    bool first_handler = true;
    // Set by call_handler when a handler changed the context's Eip away from
    // this frame's control PC: that is the observable proof it accepted, and it
    // is the only signal that works for a guest's own MSVC handler (whose
    // disposition values follow the EXCEPTION_DISPOSITION enum, where 0 means
    // ExceptionContinueSearch — the opposite of what this code assumed).
    bool handler_rewrote_context = false;
    auto call_handler = [&](uint32_t frame, uint32_t& inner_ebp) -> uint32_t {
        uint32_t handler = *(reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(frame + 4)));
        if (!handler || handler == 0xFFFFFFFFu) return 0;
        const bool is_first = first_handler;
        first_handler = false;
        // The exception occurred in the callee whose EBP is inner_ebp. The
        // control PC is the return address in THIS frame's function, i.e.
        // [inner_ebp+4]. Reading [own_ebp+4] instead would shift every outer
        // frame by one level and make __CxxFrameHandler3 look up the wrong
        // try/catch state.
        //
        // For a hardware fault raised directly by a guest instruction, the
        // instruction itself (not the caller's return address) selects the
        // try/except region in the innermost frame.
        const SehFrameLayout layout = seh_frame_layout(frame, inner_ebp);
        uint32_t control_pc = inner_ebp;
        if (hardware_guest && is_first) {
            control_pc = fault_pc;
        } else if (maps_range_readable(static_cast<uintptr_t>(inner_ebp) + 4, 4)) {
            control_pc = *(reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(inner_ebp + 4)));
        }
        uint32_t context_ebp = layout.valid ? layout.current_ebp : inner_ebp;
        if (hardware_guest && is_first) context_ebp = st->regs[XR_EBP];
        // HandlerData for table-driven handlers lives in the record's third word.
        uint32_t handler_data = *(reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(frame + 8)));
        w32(ctx + 0xB8, control_pc);
        w32(ctx + 0xB4, context_ebp);
        // Build the guest-visible DISPATCHER_CONTEXT at the real Win32 field
        // offsets (see seh:: in core/x86/x86.h). A real MSVC guest handler
        // reads HandlerData from +0x1C; writing it past the end of the struct
        // left it zero and every C++ handler declined the exception.
        w32(disp + seh::kDcControlPc, control_pc);
        w32(disp + seh::kDcImageBase, image_base_for_address(control_pc));
        w32(disp + seh::kDcFunctionEntry, 0);      // no RUNTIME_FUNCTION for PE
        w32(disp + seh::kDcEstablisherFrame, frame);
        w32(disp + seh::kDcTargetIp, 0);           // filled by the handler
        w32(disp + seh::kDcContextRecord, ctx);
        w32(disp + seh::kDcLanguageHandler, handler);
        w32(disp + seh::kDcHandlerData, handler_data);
        uint32_t hargs[4] = { rec, frame, ctx, disp };
        // MSVC's x86 C++ handler chain expects this function's FuncInfo in EAX.
        // For a classic __except_handler3/4 frame the third word is a scope
        // table, not a FuncInfo, and seeding it would be wrong: only pass it
        // when it looks like a FuncInfo (magic in the first field).
        uint32_t eax_seed = looks_like_func_info(handler_data) ? handler_data : 0;
        if (seh_trace) {
            fprintf(stderr, "[seh] call handler=0x%08X frame=0x%08X control=0x%08X%s hdata=0x%08X eax=0x%08X\n",
                    handler, frame, control_pc, (hardware_guest && is_first) ? " (hardware)" : "",
                    handler_data, eax_seed);
            if (maps_range_readable(rec, 0x20)) {
                auto rd = [](uint32_t a) { return *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(a)); };
                fprintf(stderr, "   [seh] record: code=0x%08X flags=0x%08X nargs=%u "
                        "info[0]=0x%08X info[1]=0x%08X info[2]=0x%08X info[3]=0x%08X\n",
                        rd(rec), rd(rec + 4), rd(rec + 0x10), rd(rec + 0x14),
                        rd(rec + 0x18), rd(rec + 0x1C), rd(rec + 0x20));
            }
            if (maps_range_readable(disp, seh::kDcSize)) {
                auto rd = [](uint32_t a) { return *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(a)); };
                fprintf(stderr, "   [seh] dispCtx: pc=0x%08X img=0x%08X fe=0x%08X est=0x%08X "
                        "target=0x%08X ctx=0x%08X lang=0x%08X hdata=0x%08X\n",
                        rd(disp + seh::kDcControlPc), rd(disp + seh::kDcImageBase),
                        rd(disp + seh::kDcFunctionEntry), rd(disp + seh::kDcEstablisherFrame),
                        rd(disp + seh::kDcTargetIp), rd(disp + seh::kDcContextRecord),
                        rd(disp + seh::kDcLanguageHandler), rd(disp + seh::kDcHandlerData));
            }
        }
        // Context Eip as this handler found it: it must match the control PC we
        // just wrote, so comparing after the call detects a rewrite.
        const uint32_t ctx_eip_before = *(reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ctx + 0xB8)));
        uint32_t disposition = x86_call_guest_handler(handler, hargs, 4, eax_seed);
        const uint32_t ctx_eip_after = *(reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ctx + 0xB8)));
        handler_rewrote_context =
            (ctx_eip_after != ctx_eip_before) || (ctx_eip_after != control_pc);
        if (seh_trace) {
            fprintf(stderr, "[seh] handler=0x%08X returned 0x%08X\n", handler, disposition);
        }
        // Walk to the caller's frame. For a 4-word node the saved EBP is at
        // +0x10; for a 3-word node it is at +0xc. Using the detected layout
        // keeps the control PC of every outer frame in its own function
        // instead of turning the previous return address into a stack frame.
        inner_ebp = layout.valid ? layout.saved_ebp : inner_ebp;
        return disposition;
    };

    // Pass 1: find a frame whose handler accepts the exception.
    uint32_t frame = fs_head;
    uint32_t accepting = 0;
    uint32_t depth = 0;
    uint32_t inner_ebp = st->regs[XR_EBP];
    while (frame && depth++ < 64) {
        if (frame < 0x1000 || frame >= 0x80000000u) break;
        uint32_t prev = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(frame));
        uint32_t handler = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(frame + 4));
        if (!handler || handler == 0xFFFFFFFFu) { frame = prev; continue; }
        // A handler may resolve the exception itself via RtlUnwind, which
        // leaves a pending resume state. Stop dispatching in that case.
        X86SehResume& seh_pre = x86_seh_resume_state();
        if (seh_pre.active) return;
        uint32_t disposition = call_handler(frame, inner_ebp);
        uint32_t target = *(reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(disp + seh::kDcTargetIp)));
        if (seh_trace) {
            fprintf(stderr, "[seh] pass1 frame=0x%08X handler=0x%08X disp=0x%X target=0x%08X ctxeip=0x%08X ctxesp=0x%08X ctxebp=0x%08X\n",
                    frame, handler, (unsigned)disposition, target,
                    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ctx + 0xB8)),
                    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ctx + 0xC4)),
                    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ctx + 0xB4)));
        }
        X86SehResume& seh = x86_seh_resume_state();
        if (seh.active) return; // handler unwound and chose a resume point
        // A handler "accepts" when it rewrites the context to continue
        // somewhere else, reports EXCEPTION_CONTINUE_EXECUTION (0xFFFFFFFF), or
        // — ThetaZ's own table handlers' convention — returns
        // EXCEPTION_EXECUTE_HANDLER (1) with a nonzero TargetIp. Plain
        // EXCEPTION_DISPOSITION 0/2/3 mean keep searching; note 0 is
        // ExceptionContinueSearch, NOT acceptance, which is why the context
        // rewrite check carries the decision for a guest's own MSVC handler.
        // A bare disposition of 1 with an untouched context is deliberately not
        // accepted: resuming at the fault PC would loop forever.
        const bool accepted =
            handler_rewrote_context || disposition == 0xFFFFFFFF ||
            (disposition == 1 && target != 0);
        if (accepted) {
            accepting = frame;
            break;
        }
        frame = prev;
    }

    if (!accepting) {
        // No handler claimed the exception: leave it to the guest's unhandled
        // exception filter path by falling through the API return.
        return;
    }

    // Pass 2: unwind the frames between the current head and the accepting
    // frame, running __finally blocks and destructor funclets.
    uint32_t unwind_flags = flags | 0x6u; // EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND
    w32(rec + 0x04, unwind_flags);
    uint32_t uf = fs_head;
    uint32_t udepth = 0;
    uint32_t unwind_inner_ebp = st->regs[XR_EBP];
    first_handler = true;
    while (uf && uf != accepting && udepth++ < 64) {
        if (uf < 0x1000 || uf >= 0x80000000u) break;
        uint32_t prev = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(uf));
        uint32_t handler = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(uf + 4));
        if (handler && handler != 0xFFFFFFFFu) call_handler(uf, unwind_inner_ebp);
        uf = prev;
    }
    w32(rec + 0x04, flags);

    // Resume at the accepting handler's continuation.
    uint32_t resume_eip = *(reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(disp + seh::kDcTargetIp)));
    if (!resume_eip) resume_eip = *(reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ctx + 0xB8)));
    uint32_t resume_esp = *(reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ctx + 0xC4)));
    uint32_t resume_ebp = *(reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ctx + 0xB4)));
    uint32_t resume_edx = *(reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ctx + 0xA8)));
    // A table-driven __except handler that leaves Esp untouched expects the
    // RaiseException stdcall arguments to be gone after resuming. Hardware
    // faults have no RaiseException frame: the guest instruction did not push
    // four arguments, so the stack must be left exactly as the handler set it.
    if (origin == RaiseOrigin::Api && resume_esp == fault_esp) {
        resume_esp = fault_esp + 4 * 4;
    }
    if (st->fs_base) {
        *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(st->fs_base)) = accepting;
    }
    X86SehResume& seh = x86_seh_resume_state();
    seh.active = true;
    seh.eip = resume_eip;
    seh.esp = resume_esp;
    seh.ebp = resume_ebp;
    seh.edx = resume_edx;
}

namespace {
// Common delivery path for host hardware faults. The interpreter snapshots
// EIP/ESP at the start of the faulting instruction before the host signal
// handler longjmps out of the memory access.
bool raise_host_exception(uint32_t code, uint32_t instruction, uint32_t esp,
                          uint32_t nargs, const uint32_t* info) {
    X86CpuState* st = x86_current_state();
    if (!st) return false;
    const RaiseOrigin origin = x86_dispatch_active()
        ? RaiseOrigin::HardwareApi
        : RaiseOrigin::HardwareGuest;
    st->eip = instruction;
    st->regs[XR_ESP] = esp;
    X86SehResume& seh = x86_seh_resume_state();
    const bool prev_active = seh.active;
    seh.active = false;
    g_raise_origin = origin;
    THETAZ_RaiseException(code, 0, nargs, const_cast<uint32_t*>(info));
    g_raise_origin = RaiseOrigin::Api;
    const bool handled = seh.active;
    if (!handled) seh.active = prev_active;
    return handled;
}
} // namespace

bool x86_raise_access_violation(uint32_t fault_addr, uint32_t instruction,
                                uint32_t esp, bool write) {
    // EXCEPTION_READ_FAULT (0) / EXCEPTION_WRITE_FAULT (1), then the address.
    uint32_t info[2] = { write ? 1u : 0u, fault_addr };
    return raise_host_exception(0xC0000005u, instruction, esp, 2, info);
}

bool x86_raise_guest_exception(uint32_t code, uint32_t instruction,
                               uint32_t esp, uint32_t nargs,
                               const uint32_t* info) {
    if (nargs > 15) nargs = 15;
    return raise_host_exception(code, instruction, esp, nargs, info);
}

BOOL WINAPI_CALL THETAZ_HeapFree(HANDLE h, DWORD f, void* p){ (void)h;(void)f; if(p) free_guest_heap(p); return TRUE; }
SIZE_T WINAPI_CALL THETAZ_HeapSize(HANDLE h, DWORD f, LPCVOID p){
    (void)h; (void)f;
    if (!p) return (SIZE_T)-1;
    return (SIZE_T)HeapBlockSize(p);
}
SIZE_T WINAPI_CALL THETAZ_HeapCompact(HANDLE h, DWORD f) {
    (void)h; (void)f;
    return 0;
}
BOOL WINAPI_CALL THETAZ_HeapDestroy(HANDLE h) {
    (void)h;
    return TRUE;
}
LPVOID WINAPI_CALL THETAZ_LocalAlloc(UINT f, SIZE_T n) {
    return THETAZ_HeapAlloc(nullptr, (f & 0x40) ? HEAP_ZERO_MEMORY : 0, n);
}
LPVOID WINAPI_CALL THETAZ_LocalReAlloc(LPVOID p, SIZE_T n, UINT f) {
    return THETAZ_HeapReAlloc(nullptr, (f & 0x40) ? HEAP_ZERO_MEMORY : 0, p, n);
}
SIZE_T WINAPI_CALL THETAZ_LocalSize(LPVOID p) {
    if (!p) return 0;
    return (SIZE_T)HeapBlockSize(p);
}
LPVOID WINAPI_CALL THETAZ_GlobalReAlloc(LPVOID p, SIZE_T n, UINT f) {
    return THETAZ_HeapReAlloc(nullptr, (f & 0x40) ? HEAP_ZERO_MEMORY : 0, p, n);
}
SIZE_T WINAPI_CALL THETAZ_GlobalSize(LPVOID p) {
    if (!p) return 0;
    return (SIZE_T)HeapBlockSize(p);
}
BOOL WINAPI_CALL THETAZ_RtlValidateHeap(HANDLE h, DWORD f, LPCVOID p) {
    (void)h; (void)f; (void)p;
    return TRUE;
}
namespace {
uint32_t g_ptr_cookie = 0;
uint32_t ptr_cookie() {
    if (!g_ptr_cookie) {
        uint32_t seed = (uint32_t)getpid() ^ 0x9E3779B9u;
        seed ^= (uint32_t)(clock() & 0xFFFFFFFFu);
        if (!seed) seed = 0xA5A5A5A5u;
        g_ptr_cookie = seed;
    }
    return g_ptr_cookie;
}
uint32_t rotl32(uint32_t v, unsigned n) {
    n &= 31;
    return n ? (v << n) | (v >> (32 - n)) : v;
}
uint32_t rotr32(uint32_t v, unsigned n) {
    n &= 31;
    return n ? (v >> n) | (v << (32 - n)) : v;
}
}
LPVOID WINAPI_CALL THETAZ_EncodePointer(LPCVOID p) {
    uint32_t ptr = (uint32_t)(uintptr_t)p;
    uint32_t cookie = ptr_cookie();
    return (LPVOID)(uintptr_t)(rotl32(ptr, cookie & 0x1F) ^ cookie);
}
LPVOID WINAPI_CALL THETAZ_DecodePointer(LPCVOID p) {
    uint32_t ptr = (uint32_t)(uintptr_t)p;
    uint32_t cookie = ptr_cookie();
    return (LPVOID)(uintptr_t)rotr32(ptr ^ cookie, cookie & 0x1F);
}
void WINAPI_CALL THETAZ_GetSystemInfo(void* si) {
    if (!si) return;
    uint8_t* b = (uint8_t*)si;
    memset(b, 0, 36);
    long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
    if (nprocs < 1) nprocs = 4;
    *reinterpret_cast<uint16_t*>(b + 0) = 0;
    *reinterpret_cast<uint32_t*>(b + 4) = 0x1000;
    *reinterpret_cast<uint32_t*>(b + 8) = 0x10000;
    *reinterpret_cast<uint32_t*>(b + 12) = 0x7FFEFFFF;
    uint32_t mask = nprocs >= 32 ? 0xFFFFFFFFu : ((1u << nprocs) - 1);
    *reinterpret_cast<uint32_t*>(b + 16) = mask;
    *reinterpret_cast<uint32_t*>(b + 20) = (uint32_t)nprocs;
    *reinterpret_cast<uint32_t*>(b + 24) = 586;
    *reinterpret_cast<uint32_t*>(b + 28) = 0x10000;
    *reinterpret_cast<uint16_t*>(b + 32) = 6;
}
BOOL WINAPI_CALL THETAZ_TryEnterCriticalSection(void* cs){ (void)cs; return TRUE; }
uint64_t WINAPI64_SendMessageA(uint64_t h, uint32_t m, uint64_t w, uint64_t l){ (void)h;(void)m;(void)w;(void)l; return 0; }
BOOL WINAPI64_SetWindowTextA(uint64_t h, const char* t){ (void)h;(void)t; return 0; }
BOOL WINAPI64_ShowWindow(uint64_t h, int c){ (void)h;(void)c; return 0; }
BOOL WINAPI64_TranslateMessage(const void* m){ (void)m; return 0; }
BOOL WINAPI64_UpdateWindow(uint64_t h){ (void)h; return 0; }
uint64_t WINAPI64_GetDlgItem(uint64_t h, int id){ (void)h;(void)id; return 0; }
BOOL WINAPI64_DialogBoxParamA(uint32_t a, const char* b, uint64_t c, void* d, intptr_t e){ (void)a;(void)b;(void)c;(void)d;(void)e; return 0; }
BOOL WINAPI64_DestroyWindow(uint64_t h){ (void)h; return 0; }
uint64_t WINAPI64_DefWindowProcA(uint64_t h, uint32_t m, uint64_t w, uint64_t l){ (void)h;(void)m;(void)w;(void)l; return 0; }
void WINAPI64_PostQuitMessage(int c){ (void)c; }
uint64_t THETAZ_GetDlgItem(uint64_t h, int id){ (void)h;(void)id; return 0; }
uint64_t THETAZ_SendMessageA(uint64_t h, uint32_t m, uint64_t w, uint64_t l){ (void)h;(void)m;(void)w;(void)l; return 0; }
BOOL WINAPI64_SetDlgItemTextA(uint64_t h, int id, const char* t){ (void)h;(void)id;(void)t; return 0; }
BOOL WINAPI_CALL THETAZ_TryEnterCriticalSection(struct _CRITICAL_SECTION* cs){ (void)cs; return TRUE; }
extern const uint64_t kDesktopHwnd = 0x1000;
void write_guest_msg(uint8_t* d, const GuestMsg& m){ (void)d;(void)m; }
BOOL WINAPI64_CheckDlgButton(uint64_t h, int id, uint32_t c){ (void)h;(void)id;(void)c; return 0; }
int generic_api_stub(){return 0;}
void theta_record_command_line(int a, char** b){ (void)a;(void)b; }
void theta_record_environment(){}

static std::mutex g_stdhandle_mutex;
static std::unordered_map<uint32_t, uintptr_t> g_std_handles;

BOOL WINAPI_CALL THETAZ_SetStdHandle(DWORD nStdHandle, HANDLE hHandle) {
    std::lock_guard<std::mutex> lock(g_stdhandle_mutex);
    g_std_handles[nStdHandle] = reinterpret_cast<uintptr_t>(hHandle);
    return TRUE;
}

HANDLE WINAPI_CALL THETAZ_GetStdHandle(DWORD nStdHandle) {
    {
        std::lock_guard<std::mutex> lock(g_stdhandle_mutex);
        auto it = g_std_handles.find(nStdHandle);
        if (it != g_std_handles.end()) return reinterpret_cast<HANDLE>(it->second);
    }
    switch (nStdHandle) {
        case 0xFFFFFFF6u: return reinterpret_cast<HANDLE>(0x90000001u);
        case 0xFFFFFFF5u: return reinterpret_cast<HANDLE>(0x90000002u);
        case 0xFFFFFFF4u: return reinterpret_cast<HANDLE>(0x90000003u);
        default: return reinterpret_cast<HANDLE>(0x90000000u);
    }
}

static std::mutex g_cmdline_mutex;
static std::unordered_map<uint64_t, std::string> g_cmdline_by_pid;
static std::unordered_map<uint64_t, uint32_t> g_cmdline_a_cache;
static std::unordered_map<uint64_t, uint32_t> g_cmdline_w_cache;

// CreateProcess stores the child's command line here so GetCommandLineA/W
// return the string the parent passed instead of the main process's line.
// Without this every self-relaunched child re-runs the parent's startup logic.
void set_guest_process_command_line(uint64_t pid, const std::string& cmdline) {
    std::lock_guard<std::mutex> lock(g_cmdline_mutex);
    g_cmdline_by_pid[pid] = cmdline;
}

static std::string command_line_for_current_process() {
    uint64_t pid = ThreadManager::instance().get_current_guest_pid();
    {
        std::lock_guard<std::mutex> lock(g_cmdline_mutex);
        auto it = g_cmdline_by_pid.find(pid);
        if (it != g_cmdline_by_pid.end()) {
            if (getenv("THETAZ_TRACE_CP"))
                fprintf(stderr, "[cmdline] pid=%llu \"%s\"\n",
                        (unsigned long long)pid, it->second.c_str());
            return it->second;
        }
    }
    std::string main_cmd = get_crt_command_line();
    if (getenv("THETAZ_TRACE_CP"))
        fprintf(stderr, "[cmdline] pid=%llu main \"%s\"\n",
                (unsigned long long)pid, main_cmd.c_str());
    return main_cmd.empty() ? std::string("thetaz") : main_cmd;
}

LPSTR WINAPI_CALL THETAZ_GetCommandLineA() {
    uint64_t pid = ThreadManager::instance().get_current_guest_pid();
    {
        std::lock_guard<std::mutex> lock(g_cmdline_mutex);
        auto it = g_cmdline_a_cache.find(pid);
        if (it != g_cmdline_a_cache.end()) {
            return reinterpret_cast<LPSTR>(static_cast<uintptr_t>(it->second));
        }
    }
    std::string s = command_line_for_current_process();
    void* a = alloc_guest_shared(s.size() + 1);
    if (!a) return nullptr;
    memcpy(a, s.c_str(), s.size() + 1);
    uint32_t addr = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(a));
    {
        std::lock_guard<std::mutex> lock(g_cmdline_mutex);
        g_cmdline_a_cache[pid] = addr;
    }
    return reinterpret_cast<LPSTR>(static_cast<uintptr_t>(addr));
}
LPWSTR WINAPI_CALL THETAZ_GetCommandLineW() {
    uint64_t pid = ThreadManager::instance().get_current_guest_pid();
    {
        std::lock_guard<std::mutex> lock(g_cmdline_mutex);
        auto it = g_cmdline_w_cache.find(pid);
        if (it != g_cmdline_w_cache.end()) {
            return reinterpret_cast<LPWSTR>(static_cast<uintptr_t>(it->second));
        }
    }
    std::string s = command_line_for_current_process();
    void* w = alloc_guest_shared((s.size() + 1) * 2);
    if (!w) return nullptr;
    uint16_t* p = static_cast<uint16_t*>(w);
    for (size_t i = 0; i <= s.size(); i++) p[i] = static_cast<uint16_t>(static_cast<unsigned char>(s[i]));
    uint32_t addr = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(w));
    {
        std::lock_guard<std::mutex> lock(g_cmdline_mutex);
        g_cmdline_w_cache[pid] = addr;
    }
    return reinterpret_cast<LPWSTR>(static_cast<uintptr_t>(addr));
}

// CommandLineToArgvW: split a Windows command line into an argv array the
// caller releases with a single LocalFree. The whole result (pointer array +
// UTF-16 strings) is one guest-heap block.
void* WINAPI_CALL THETAZ_CommandLineToArgvW(LPCWSTR lpCmdLine, int* pNumArgs) {
    if (!pNumArgs) { THETAZ_SetLastError(ERROR_INVALID_PARAMETER); return nullptr; }
    std::u16string cmd;
    if (lpCmdLine) {
        const uint16_t* p = reinterpret_cast<const uint16_t*>(lpCmdLine);
        while (*p) cmd.push_back(static_cast<char16_t>(*p++));
    } else {
        std::string s = command_line_for_current_process();
        cmd.assign(s.begin(), s.end());
    }

    // Windows rules: whitespace separates, double quotes group, and runs of
    // backslashes before a quote are halved (odd run escapes the quote).
    std::vector<std::u16string> args;
    size_t i = 0;
    const size_t n = cmd.size();
    while (i < n) {
        while (i < n && (cmd[i] == u' ' || cmd[i] == u'\t')) i++;
        if (i >= n) break;
        std::u16string arg;
        bool in_quotes = false;
        while (i < n) {
            char16_t c = cmd[i];
            if (!in_quotes && (c == u' ' || c == u'\t')) break;
            if (c == u'\\') {
                size_t slashes = 0;
                while (i < n && cmd[i] == u'\\') { slashes++; i++; }
                if (i < n && cmd[i] == u'"') {
                    arg.append(slashes / 2, u'\\');
                    if (slashes % 2 == 1) {
                        arg.push_back(u'"');
                        i++;
                    } else {
                        in_quotes = !in_quotes;
                        i++;
                    }
                    continue;
                }
                arg.append(slashes, u'\\');
                continue;
            }
            if (c == u'"') { in_quotes = !in_quotes; i++; continue; }
            arg.push_back(c);
            i++;
        }
        args.push_back(std::move(arg));
    }

    const int count = static_cast<int>(args.size());
    size_t array_bytes = (static_cast<size_t>(count) + 1) * 4;
    size_t strings_bytes = 0;
    for (const auto& a : args) strings_bytes += (a.size() + 1) * 2;
    void* block = calloc_guest_heap(1, array_bytes + strings_bytes);
    if (!block) { THETAZ_SetLastError(ERROR_OUTOFMEMORY); return nullptr; }
    uint8_t* base = static_cast<uint8_t*>(block);
    uint32_t* arr = reinterpret_cast<uint32_t*>(base);
    uint8_t* sp = base + array_bytes;
    for (int k = 0; k < count; k++) {
        uint16_t* dst = reinterpret_cast<uint16_t*>(sp);
        arr[k] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(dst));
        for (size_t j = 0; j < args[k].size(); j++) dst[j] = static_cast<uint16_t>(args[k][j]);
        dst[args[k].size()] = 0;
        sp += (args[k].size() + 1) * 2;
    }
    arr[count] = 0;
    *pNumArgs = count;
    return block;
}

// 32-bit guest STARTUPINFO layout (pointer fields are 4 bytes). The host
// STARTUPINFO* structs cannot be used directly because pointers are 8 bytes
// here, which would corrupt the guest structure.
struct GuestStartupInfoW {
    uint32_t cb;
    uint32_t lpReserved;
    uint32_t lpDesktop;
    uint32_t lpTitle;
    uint32_t dwX;
    uint32_t dwY;
    uint32_t dwXSize;
    uint32_t dwYSize;
    uint32_t dwXCountChars;
    uint32_t dwYCountChars;
    uint32_t dwFillAttribute;
    uint32_t dwFlags;
    uint16_t wShowWindow;
    uint16_t cbReserved2;
    uint32_t lpReserved2;
    uint32_t hStdInput;
    uint32_t hStdOutput;
    uint32_t hStdError;
};
static_assert(sizeof(GuestStartupInfoW) == 68, "STARTUPINFO guest size");

static void fill_startup_info(void* raw, size_t guest_size) {
    if (!raw) return;
    auto* si = static_cast<GuestStartupInfoW*>(raw);
    uint32_t caller_cb = si->cb;
    size_t clear = guest_size;
    if (caller_cb >= 4 && caller_cb < clear) clear = caller_cb;
    memset(raw, 0, clear);
    if (clear >= 4) si->cb = static_cast<uint32_t>(guest_size);
}

void WINAPI_CALL THETAZ_GetStartupInfoW(void* lpStartupInfo) {
    fill_startup_info(lpStartupInfo, sizeof(GuestStartupInfoW));
}

void WINAPI_CALL THETAZ_GetStartupInfoA(void* lpStartupInfo) {
    fill_startup_info(lpStartupInfo, sizeof(GuestStartupInfoW));
}

static bool utf8_next(const uint8_t*& p, const uint8_t* end, uint32_t& cp) {
    if (end && p >= end) return false;
    uint8_t c = *p;
    if (c < 0x80) { cp = c; p++; return true; }
    int len;
    uint32_t v;
    if ((c & 0xE0) == 0xC0) { len = 2; v = c & 0x1F; }
    else if ((c & 0xF0) == 0xE0) { len = 3; v = c & 0x0F; }
    else if ((c & 0xF8) == 0xF0) { len = 4; v = c & 0x07; }
    else return false;
    c &= 0xFF;
    if (end && (size_t)(end - p) < (size_t)len) return false;
    for (int i = 1; i < len; i++) {
        uint8_t cc = p[i];
        if ((cc & 0xC0) != 0x80) return false;
        v = (v << 6) | (cc & 0x3F);
    }
    p += len;
    cp = v;
    return true;
}

static void utf16_emit(std::u16string& out, uint32_t cp) {
    if (cp < 0x10000) {
        out.push_back(static_cast<char16_t>(cp));
    } else {
        cp -= 0x10000;
        out.push_back(static_cast<char16_t>(0xD800 + (cp >> 10)));
        out.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
    }
}

INT WINAPI_CALL THETAZ_MultiByteToWideChar(UINT cp, DWORD flags, LPCSTR src, int cb, LPWSTR dst, int cch) {
    (void)cp; (void)flags;
    if (!src) return 0;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(src);
    const uint8_t* end = (cb < 0) ? nullptr : p + cb;
    std::u16string out;
    uint32_t cpnt = 0;
    while (true) {
        if (end ? p >= end : (*p == 0)) break;
        if (!utf8_next(p, end, cpnt)) break;
        utf16_emit(out, cpnt);
    }
    if (!dst || cch <= 0) return static_cast<INT>(out.size());
    uint16_t* d = reinterpret_cast<uint16_t*>(dst);
    size_t n = out.size() < static_cast<size_t>(cch - 1) ? out.size() : static_cast<size_t>(cch - 1);
    for (size_t i = 0; i < n; i++) d[i] = static_cast<uint16_t>(out[i]);
    d[n] = 0;
    return static_cast<INT>(out.size());
}

BOOL WINAPI_CALL THETAZ_WideCharToMultiByte(UINT cp, DWORD flags, LPCWSTR wsrc, int cch, LPSTR dst, int cb,
        const char* defchar, BOOL* useddef) {
    (void)cp; (void)flags; (void)defchar;
    if (useddef) *useddef = FALSE;
    if (!wsrc) return 0;
    const uint16_t* p = reinterpret_cast<const uint16_t*>(wsrc);
    int count = 0;
    std::string out;
    while (true) {
        if (cch >= 0 && count >= cch) break;
        uint16_t u = *p;
        if (cch < 0 && u == 0) break;
        uint32_t cpnt = u;
        if (u >= 0xD800 && u <= 0xDBFF) {
            uint16_t lo = p[1];
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                cpnt = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00);
                p++; count++;
            }
        }
        if (cpnt < 0x80) out.push_back(static_cast<char>(cpnt));
        else if (cpnt < 0x800) { out.push_back(static_cast<char>(0xC0 | (cpnt >> 6))); out.push_back(static_cast<char>(0x80 | (cpnt & 0x3F))); }
        else if (cpnt < 0x10000) { out.push_back(static_cast<char>(0xE0 | (cpnt >> 12))); out.push_back(static_cast<char>(0x80 | ((cpnt >> 6) & 0x3F))); out.push_back(static_cast<char>(0x80 | (cpnt & 0x3F))); }
        else { out.push_back(static_cast<char>(0xF0 | (cpnt >> 18))); out.push_back(static_cast<char>(0x80 | ((cpnt >> 12) & 0x3F))); out.push_back(static_cast<char>(0x80 | ((cpnt >> 6) & 0x3F))); out.push_back(static_cast<char>(0x80 | (cpnt & 0x3F))); }
        p++; count++;
    }
    size_t need = out.size() + 1;
    if (!dst) return static_cast<BOOL>(need);
    if (cb <= 0 || (size_t)cb < out.size()) {
        // Manually copy up to cb-1 bytes
        size_t n = out.size() < (size_t)(cb - 1) ? out.size() : (size_t)(cb - 1);
        memcpy(dst, out.data(), n);
        dst[n] = 0;
        if (useddef) *useddef = TRUE;
        return static_cast<BOOL>(need);
    }
    memcpy(dst, out.data(), out.size());
    dst[out.size()] = 0;
    return static_cast<BOOL>(out.size());
}

DWORD WINAPI_CALL THETAZ_ExpandEnvironmentStringsW(LPCWSTR src, LPWSTR dst, DWORD nSize) {
    if (!src) return 0;
    const uint16_t* p = reinterpret_cast<const uint16_t*>(src);
    std::u16string in;
    while (*p) in.push_back(*p++);
    std::u16string out;
    for (size_t i = 0; i < in.size(); ) {
        if (in[i] == static_cast<uint16_t>('%')) {
            size_t j = i + 1;
            while (j < in.size() && in[j] != static_cast<uint16_t>('%')) j++;
            if (j < in.size()) {
                std::string var;
                for (size_t k = i + 1; k < j; k++) var.push_back(static_cast<char>(in[k] & 0xFF));
                const char* val = var.empty() ? nullptr : getenv(var.c_str());
                if (!val) val = "";
                const uint8_t* vp = reinterpret_cast<const uint8_t*>(val);
                while (*vp) { uint32_t c = *vp++; out.push_back(static_cast<char16_t>(c)); }
                i = j + 1;
                continue;
            }
        }
        out.push_back(in[i++]);
    }
    DWORD need = static_cast<DWORD>(out.size() + 1);
    if (!dst || nSize < need) return need;
    uint16_t* d = reinterpret_cast<uint16_t*>(dst);
    for (size_t i = 0; i < out.size(); i++) d[i] = *reinterpret_cast<uint16_t*>(&out[i]);
    d[out.size()] = 0;
    return static_cast<DWORD>(out.size());
}

DWORD WINAPI_CALL THETAZ_GetOEMCP() { return 437; }

BOOL WINAPI_CALL THETAZ_EnumSystemLocalesW(LOCALE_ENUMPROCW lpLocaleEnumProc, DWORD dwFlags) {
    (void)lpLocaleEnumProc; (void)dwFlags;
    return TRUE;
}

uint64_t WINAPI_CALL THETAZ_VerSetConditionMask(uint64_t ConditionMask, DWORD Type, BYTE Condition) {
    if (Type >= 1 && Type <= 8 && Condition >= 1 && Condition <= 7) {
        uint64_t shift = static_cast<uint64_t>(Type - 1) * 3;
        ConditionMask &= ~(7ull << shift);
        ConditionMask |= static_cast<uint64_t>(Condition) << shift;
    }
    return ConditionMask;
}

void initialize_kernel32_runtime(){
    auto s=(void*)generic_api_stub;
    register_api("kernel32.dll", "VirtualAlloc", (void*)THETAZ_VirtualAlloc);
    register_api("kernel32.dll", "VirtualFree", (void*)THETAZ_VirtualFree);
    register_api("kernel32.dll", "VirtualQuery", (void*)THETAZ_VirtualQuery);
    register_api("kernel32.dll", "VirtualProtect", (void*)THETAZ_VirtualProtect);
    register_api("kernel32.dll", "WriteConsoleW", s);
    register_api("kernel32.dll", "FreeEnvironmentStringsW", (void*)THETAZ_FreeEnvironmentStringsW);
    register_api("kernel32.dll", "GetEnvironmentStringsW", (void*)THETAZ_GetEnvironmentStringsW);
    register_api("kernel32.dll", "GetCommandLineA", (void*)THETAZ_GetCommandLineA);
    register_api("kernel32.dll", "GetCommandLineW", (void*)THETAZ_GetCommandLineW);
    register_api("shell32.dll", "CommandLineToArgvW", (void*)THETAZ_CommandLineToArgvW);
    register_api("kernel32.dll", "CommandLineToArgvW", (void*)THETAZ_CommandLineToArgvW);
    register_api("kernelbase.dll", "CommandLineToArgvW", (void*)THETAZ_CommandLineToArgvW);
    register_api("kernel32.dll", "GetStartupInfoA", (void*)THETAZ_GetStartupInfoA);
    register_api("kernel32.dll", "GetStartupInfoW", (void*)THETAZ_GetStartupInfoW);
    register_api("kernelbase.dll", "GetStartupInfoA", (void*)THETAZ_GetStartupInfoA);
    register_api("kernelbase.dll", "GetStartupInfoW", (void*)THETAZ_GetStartupInfoW);
    register_api("kernel32.dll", "SetStdHandle", (void*)THETAZ_SetStdHandle);
    register_api("kernel32.dll", "CompareStringW", s);
    register_api("kernel32.dll", "EnumSystemLocalesW", (void*)THETAZ_EnumSystemLocalesW);
    register_api("kernel32.dll", "GetOEMCP", (void*)THETAZ_GetOEMCP);
    register_api("kernel32.dll", "GetModuleHandleW", (void*)THETAZ_GetModuleHandleW);
    register_api("kernel32.dll", "GetProcAddress", (void*)THETAZ_GetProcAddress);
    register_api("kernel32.dll", "LoadLibraryW", (void*)THETAZ_LoadLibraryW);
    register_api("kernel32.dll", "GetModuleFileNameW", (void*)THETAZ_GetModuleFileNameW);
    register_api("kernel32.dll", "OutputDebugStringA", (void*)THETAZ_OutputDebugStringA);
    register_api("kernel32.dll", "CompareStringEx", (void*)THETAZ_CompareStringEx);
    register_api("kernelbase.dll", "CompareStringEx", (void*)THETAZ_CompareStringEx);
    register_api("kernel32.dll", "MulDiv", (void*)THETAZ_MulDiv);
    register_api("kernelbase.dll", "MulDiv", (void*)THETAZ_MulDiv);
    register_api("kernel32.dll", "LocalFree", (void*)THETAZ_LocalFree);
    register_api("kernel32.dll", "HeapFree", (void*)THETAZ_HeapFree);
    register_api("kernel32.dll", "HeapAlloc", (void*)THETAZ_HeapAlloc);
    register_api("kernel32.dll", "HeapReAlloc", (void*)THETAZ_HeapReAlloc);
    register_api("kernel32.dll", "HeapSize", (void*)THETAZ_HeapSize);
    register_api("kernel32.dll", "HeapCompact", (void*)THETAZ_HeapCompact);
    register_api("kernel32.dll", "HeapDestroy", (void*)THETAZ_HeapDestroy);
    register_api("kernel32.dll", "LocalAlloc", (void*)THETAZ_LocalAlloc);
    register_api("kernel32.dll", "LocalReAlloc", (void*)THETAZ_LocalReAlloc);
    register_api("kernel32.dll", "LocalSize", (void*)THETAZ_LocalSize);
    register_api("kernel32.dll", "GlobalReAlloc", (void*)THETAZ_GlobalReAlloc);
    register_api("kernel32.dll", "GlobalSize", (void*)THETAZ_GlobalSize);
    register_api("kernel32.dll", "RtlValidateHeap", (void*)THETAZ_RtlValidateHeap);
    register_api("kernel32.dll", "GetSystemInfo", (void*)THETAZ_GetSystemInfo);
    register_api("kernel32.dll", "GetProcessHeap", (void*)THETAZ_GetProcessHeap);
    register_api("kernel32.dll", "FreeLibrary", (void*)THETAZ_FreeLibrary);
    register_api("kernel32.dll", "GetExitCodeThread", (void*)THETAZ_GetExitCodeThread);
    register_api("kernel32.dll", "SetProcessMitigationPolicy", (void*)THETAZ_SetProcessMitigationPolicy);
    register_api("kernel32.dll", "CreateEventExW", (void*)THETAZ_CreateEventExW);
    register_api("kernelbase.dll", "CreateEventExW", (void*)THETAZ_CreateEventExW);
    register_api("kernel32.dll", "SetHandleInformation", (void*)THETAZ_SetHandleInformation);
    register_api("kernelbase.dll", "SetHandleInformation", (void*)THETAZ_SetHandleInformation);
    register_api("kernel32.dll", "GetHandleInformation", (void*)THETAZ_GetHandleInformation);
    register_api("kernelbase.dll", "GetHandleInformation", (void*)THETAZ_GetHandleInformation);
    register_api("kernel32.dll", "GetPriorityClass", (void*)THETAZ_GetPriorityClass);
    register_api("kernelbase.dll", "GetPriorityClass", (void*)THETAZ_GetPriorityClass);
    register_api("kernel32.dll", "WriteFile", (void*)THETAZ_WriteFile);
    register_api("kernel32.dll", "DeleteFileW", s);
    register_api("kernel32.dll", "WideCharToMultiByte", (void*)THETAZ_WideCharToMultiByte);
    register_api("kernel32.dll", "IsWow64Process", (void*)THETAZ_IsWow64Process);
    register_api("kernel32.dll", "GetModuleHandleExW", s);
    register_api("kernel32.dll", "ExpandEnvironmentStringsW", (void*)THETAZ_ExpandEnvironmentStringsW);
    register_api("kernel32.dll", "GlobalFree", s);
    register_api("kernel32.dll", "MultiByteToWideChar", (void*)THETAZ_MultiByteToWideChar);
    register_api("kernel32.dll", "InitializeCriticalSectionEx", (void*)THETAZ_InitializeCriticalSectionEx);
    // Both spellings matter here: Office's bootstrap calls
    // InitializeCriticalSectionAndSpinCount, and a signature alone is not
    // enough -- the previous behaviour was an unregistered name falling through
    // to the generic stub, which returns FALSE. Office treats that as fatal and
    // throws boost::system::system_error(GetLastError()).
    register_api("kernel32.dll", "InitializeCriticalSectionAndSpinCount",
                 (void*)THETAZ_InitializeCriticalSectionAndSpinCount);
    register_api("kernelbase.dll", "InitializeCriticalSectionAndSpinCount",
                 (void*)THETAZ_InitializeCriticalSectionAndSpinCount);
    register_api("ntdll.dll", "RtlInitializeCriticalSectionAndSpinCount",
                 (void*)THETAZ_InitializeCriticalSectionAndSpinCount);
    register_api("kernel32.dll", "DecodePointer", (void*)THETAZ_DecodePointer);
    register_api("kernel32.dll", "EncodePointer", (void*)THETAZ_EncodePointer);
    register_api("kernel32.dll", "CreateMutexW", (void*)THETAZ_CreateMutexW);
    register_api("kernel32.dll", "GetNativeSystemInfo", s);
    register_api("kernel32.dll", "VerSetConditionMask", (void*)THETAZ_VerSetConditionMask);
    register_api("kernel32.dll", "VerifyVersionInfoW", s);
    register_api("kernel32.dll", "GetUserDefaultLocaleName", (void*)THETAZ_GetUserDefaultLocaleName);
    register_api("kernel32.dll", "FlsFree", (void*)THETAZ_FlsFree);
    register_api("kernel32.dll", "FlsAlloc", (void*)THETAZ_FlsAlloc);
    register_api("kernel32.dll", "LocaleNameToLCID", s);
    register_api("kernel32.dll", "AttachConsole", s);
    register_api("kernel32.dll", "AllocConsole", (void*)THETAZ_AllocConsole);
    register_api("kernel32.dll", "GetStdHandle", (void*)THETAZ_GetStdHandle);
    register_api("kernel32.dll", "FreeConsole", s);
    register_api("kernel32.dll", "FindClose", (void*)THETAZ_FindClose);
    register_api("kernel32.dll", "FindFirstFileW", (void*)THETAZ_FindFirstFileW);
    register_api("kernel32.dll", "CopyFileW", (void*)THETAZ_CopyFileW);
    register_api("kernel32.dll", "UnmapViewOfFile", s);
    register_api("kernel32.dll", "CreateFileA", s);
    register_api("kernel32.dll", "GetFileSize", (void*)THETAZ_GetFileSize);
    register_api("kernel32.dll", "GetFileSizeEx", (void*)THETAZ_GetFileSizeEx);
    register_api("kernel32.dll", "CreateFileMappingW", s);
    register_api("kernel32.dll", "MapViewOfFile", s);
    register_api("kernel32.dll", "GetStringTypeExW", s);
    register_api("kernel32.dll", "GetUserDefaultLCID", s);
    register_api("kernel32.dll", "LoadLibraryA", (void*)THETAZ_LoadLibraryA);
    register_api("kernel32.dll", "GetModuleHandleA", (void*)THETAZ_GetModuleHandleA);
    register_api("kernel32.dll", "LCMapStringW", s);
    register_api("kernel32.dll", "FormatMessageA", s);
    register_api("kernel32.dll", "GetSystemTimeAsFileTime", (void*)THETAZ_GetSystemTimeAsFileTime);
    register_api("kernel32.dll", "GetSystemTimePreciseAsFileTime", (void*)THETAZ_GetSystemTimePreciseAsFileTime);
    register_api("kernel32.dll", "GetTickCount64", (void*)THETAZ_GetTickCount64);
    register_api("kernel32.dll", "K32GetProcessMemoryInfo", (void*)THETAZ_K32GetProcessMemoryInfo);
    register_api("kernel32.dll", "GlobalMemoryStatusEx", s);
    register_api("kernel32.dll", "OpenProcess", (void*)THETAZ_OpenProcess);
    register_api("kernel32.dll", "RaiseException", (void*)THETAZ_RaiseException);
    register_api("kernelbase.dll", "RaiseException", (void*)THETAZ_RaiseException);
    register_api("kernel32.dll", "IsValidCodePage", (void*)THETAZ_IsValidCodePage);
    register_api("kernel32.dll", "GetSystemTime", (void*)THETAZ_GetSystemTime);
    register_api("kernel32.dll", "SystemTimeToFileTime", (void*)THETAZ_SystemTimeToFileTime);
    register_api("kernel32.dll", "FileTimeToSystemTime", (void*)THETAZ_FileTimeToSystemTime);
    register_api("kernel32.dll", "GetCPInfoExW", (void*)THETAZ_GetCPInfoExW);
    // GetSystemDirectoryW must NOT be a generic stub: kernel32_batch.cpp
    // registers the real wide implementation, and the registry keeps the LAST
    // registration. Initialize order puts kernel32_batch first and this file
    // second, so a stub here would shadow the real one and make the installer's
    // GetSystemDirectoryW call fail (returning 0 and writing no buffer), which
    // drives it into an error path that dereferences a bogus COM pointer.
    register_api("kernel32.dll", "GetTempPathW", s);
    register_api("kernel32.dll", "CreateDirectoryW", s);
    register_api("kernel32.dll", "GetFileAttributesExW", s);
    register_api("kernel32.dll", "SetFileAttributesW", s);
    register_api("kernel32.dll", "GetProcessTimes", s);
    register_api("kernel32.dll", "TerminateProcess", (void*)THETAZ_TerminateProcess);
    register_api("kernel32.dll", "GetModuleFileNameA", (void*)THETAZ_GetModuleFileNameA);
    register_api("kernelbase.dll", "GetModuleFileNameA", (void*)THETAZ_GetModuleFileNameA);
    register_api("kernelbase.dll", "GetModuleFileNameW", (void*)THETAZ_GetModuleFileNameW);
    register_api("kernel32.dll", "GetShortPathNameA", s);
    register_api("kernel32.dll", "K32GetModuleFileNameExW", s);
    register_api("kernel32.dll", "LoadLibraryExW", (void*)THETAZ_LoadLibraryExW);
    register_api("kernel32.dll", "FindResourceW", s);
    register_api("kernel32.dll", "SizeofResource", s);
    register_api("kernel32.dll", "LoadResource", s);
    register_api("kernel32.dll", "ReleaseSemaphore", (void*)THETAZ_ReleaseSemaphore);
    register_api("kernel32.dll", "WaitForSingleObjectEx", s);
    register_api("kernel32.dll", "SetEvent", (void*)THETAZ_SetEvent);
    register_api("kernel32.dll", "CreateEventA", (void*)THETAZ_CreateEventA);
    register_api("kernel32.dll", "CreateEventW", (void*)THETAZ_CreateEventW);
    register_api("kernelbase.dll", "CreateEventW", (void*)THETAZ_CreateEventW);
    register_api("kernel32.dll", "InitializeSRWLock", (void*)THETAZ_InitializeSRWLock);
    register_api("kernel32.dll", "AcquireSRWLockExclusive", (void*)THETAZ_AcquireSRWLockExclusive);
    register_api("kernel32.dll", "ReleaseSRWLockExclusive", (void*)THETAZ_ReleaseSRWLockExclusive);
    register_api("kernel32.dll", "AcquireSRWLockShared", (void*)THETAZ_AcquireSRWLockShared);
    register_api("kernel32.dll", "ReleaseSRWLockShared", (void*)THETAZ_ReleaseSRWLockShared);
    register_api("kernel32.dll", "InitializeConditionVariable", (void*)THETAZ_InitializeConditionVariable);
    register_api("kernel32.dll", "WakeConditionVariable", (void*)THETAZ_WakeConditionVariable);
    register_api("kernel32.dll", "WakeAllConditionVariable", (void*)THETAZ_WakeAllConditionVariable);
    register_api("kernel32.dll", "SleepConditionVariableSRW", (void*)THETAZ_SleepConditionVariableSRW);
    register_api("kernel32.dll", "TryAcquireSRWLockExclusive", (void*)THETAZ_TryAcquireSRWLockExclusive);
    register_api("kernel32.dll", "TryAcquireSRWLockShared", (void*)THETAZ_TryAcquireSRWLockShared);
    register_api("kernel32.dll", "WaitForSingleObject", (void*)THETAZ_WaitForSingleObject);

    register_api("kernel32.dll", "SetErrorMode", s);
    register_api("kernel32.dll", "GetFileInformationByHandleEx", s);
    register_api("kernel32.dll", "GetCurrentDirectoryW", (void*)THETAZ_GetCurrentDirectoryW);
    register_api("kernel32.dll", "GetCurrentDirectoryA", (void*)THETAZ_GetCurrentDirectoryA);
    register_api("kernelbase.dll", "GetCurrentDirectoryW", (void*)THETAZ_GetCurrentDirectoryW);
    register_api("kernelbase.dll", "GetCurrentDirectoryA", (void*)THETAZ_GetCurrentDirectoryA);
    register_api("kernel32.dll", "GetTempFileNameW", s);
    register_api("kernel32.dll", "CopyFileExW", s);
    register_api("kernel32.dll", "GetVolumePathNamesForVolumeNameW", s);
    register_api("kernel32.dll", "SetFileInformationByHandle", s);
    register_api("kernel32.dll", "GetVolumePathNameW", s);
    register_api("kernel32.dll", "GetVolumeNameForVolumeMountPointW", s);
    register_api("kernel32.dll", "GetVolumeInformationW", s);
    register_api("kernel32.dll", "WaitForMultipleObjects", s);
    register_api("kernel32.dll", "K32EnumProcessModulesEx", s);
    register_api("kernel32.dll", "RtlCaptureStackBackTrace", s);
    register_api("kernel32.dll", "GetFileType", (void*)THETAZ_GetFileType);
    register_api("kernelbase.dll", "GetFileType", (void*)THETAZ_GetFileType);
    register_api("kernel32.dll", "GetConsoleMode", (void*)THETAZ_GetConsoleMode);
    register_api("kernelbase.dll", "GetConsoleMode", (void*)THETAZ_GetConsoleMode);
    register_api("kernel32.dll", "CreatePipe", (void*)THETAZ_CreatePipe);
    register_api("kernelbase.dll", "CreatePipe", (void*)THETAZ_CreatePipe);
    register_api("kernel32.dll", "PeekNamedPipe", (void*)THETAZ_PeekNamedPipe);
    register_api("kernelbase.dll", "PeekNamedPipe", (void*)THETAZ_PeekNamedPipe);
    register_api("kernel32.dll", "ReadFile", (void*)THETAZ_ReadFile);
    register_api("kernelbase.dll", "ReadFile", (void*)THETAZ_ReadFile);
    register_api("kernel32.dll", "SetFilePointer", (void*)THETAZ_SetFilePointer);
    register_api("kernel32.dll", "GetOverlappedResult", s);
    register_api("kernel32.dll", "GetFileAttributesW", s);
    register_api("kernel32.dll", "GetFileTime", s);
    register_api("kernel32.dll", "QueryPerformanceCounter", (void*)THETAZ_QueryPerformanceCounter);
    register_api("kernel32.dll", "QueryPerformanceFrequency", (void*)THETAZ_QueryPerformanceFrequency);
    register_api("kernel32.dll", "GlobalAlloc", s);
    register_api("kernel32.dll", "SetFileTime", s);
    register_api("kernel32.dll", "CancelIoEx", s);
    register_api("kernel32.dll", "GetProcessAffinityMask", s);
    register_api("kernel32.dll", "CreateWaitableTimerW", (void*)THETAZ_CreateWaitableTimerW);
    register_api("kernelbase.dll", "CreateWaitableTimerW", (void*)THETAZ_CreateWaitableTimerW);
    register_api("kernel32.dll", "SetWaitableTimer", (void*)THETAZ_SetWaitableTimer);
    register_api("kernelbase.dll", "SetWaitableTimer", (void*)THETAZ_SetWaitableTimer);
    register_api("kernel32.dll", "SetWaitableTimerEx", (void*)THETAZ_SetWaitableTimerEx);
    register_api("kernelbase.dll", "SetWaitableTimerEx", (void*)THETAZ_SetWaitableTimerEx);
    register_api("kernel32.dll", "CancelWaitableTimer", (void*)THETAZ_CancelWaitableTimer);
    register_api("kernelbase.dll", "CancelWaitableTimer", (void*)THETAZ_CancelWaitableTimer);
    register_api("kernel32.dll", "QueryDepthSList", s);
    register_api("kernel32.dll", "TryEnterCriticalSection", s);
    register_api("kernel32.dll", "InitializeSListHead", s);
    register_api("kernel32.dll", "InterlockedPushEntrySList", s);
    register_api("kernel32.dll", "InterlockedPopEntrySList", s);
    register_api("kernel32.dll", "GetTickCount", (void*)THETAZ_GetTickCount);
    register_api("kernel32.dll", "InitOnceExecuteOnce", (void*)THETAZ_InitOnceExecuteOnce);
    register_api("kernelbase.dll", "InitOnceExecuteOnce", (void*)THETAZ_InitOnceExecuteOnce);
    register_api("kernel32.dll", "InitOnceBeginInitialize", (void*)THETAZ_InitOnceBeginInitialize);
    register_api("kernelbase.dll", "InitOnceBeginInitialize", (void*)THETAZ_InitOnceBeginInitialize);
    register_api("kernel32.dll", "InitOnceComplete", (void*)THETAZ_InitOnceComplete);
    register_api("kernelbase.dll", "InitOnceComplete", (void*)THETAZ_InitOnceComplete);
    register_api("kernel32.dll", "AppPolicyGetProcessTerminationMethod", (void*)THETAZ_AppPolicyGetProcessTerminationMethod);
    register_api("kernelbase.dll", "AppPolicyGetProcessTerminationMethod", (void*)THETAZ_AppPolicyGetProcessTerminationMethod);
    register_api("kernel32.dll", "AppPolicyGetThreadInitializationType", (void*)THETAZ_AppPolicyGetThreadInitializationType);
    register_api("kernelbase.dll", "AppPolicyGetThreadInitializationType", (void*)THETAZ_AppPolicyGetThreadInitializationType);
    register_api("kernel32.dll", "GetSystemPowerStatus", s);
    register_api("kernel32.dll", "GetTimeZoneInformation", s);
    register_api("kernel32.dll", "AreFileApisANSI", s);
    register_api("kernel32.dll", "HeapCreate", s);
    register_api("kernel32.dll", "GetFullPathNameW", (void*)THETAZ_GetFullPathNameW);
    register_api("kernel32.dll", "GetFullPathNameA", (void*)THETAZ_GetFullPathNameA);
    register_api("kernelbase.dll", "GetFullPathNameW", (void*)THETAZ_GetFullPathNameW);
    register_api("kernelbase.dll", "GetFullPathNameA", (void*)THETAZ_GetFullPathNameA);
    register_api("kernel32.dll", "GetDiskFreeSpaceW", s);
    register_api("kernel32.dll", "LockFile", s);
    register_api("kernel32.dll", "HeapValidate", s);
    register_api("kernel32.dll", "HeapSize", (void*)THETAZ_HeapSize);
    register_api("kernel32.dll", "GetTempPathA", s);
    register_api("kernel32.dll", "GetDiskFreeSpaceA", s);
    register_api("kernel32.dll", "GetFileAttributesA", s);
    register_api("kernel32.dll", "OutputDebugStringW", (void*)THETAZ_OutputDebugStringW);
    register_api("kernel32.dll", "FlushViewOfFile", s);
    register_api("kernel32.dll", "CloseThreadpoolTimer", s);
    register_api("kernel32.dll", "SetThreadpoolTimer", s);
    register_api("kernel32.dll", "WaitForThreadpoolTimerCallbacks", s);
    register_api("kernel32.dll", "CreateThreadpoolTimer", s);
    register_api("kernel32.dll", "CloseThreadpoolWait", s);
    register_api("kernel32.dll", "SetThreadpoolWait", s);
    register_api("kernel32.dll", "WaitForThreadpoolWaitCallbacks", s);
    register_api("kernel32.dll", "CreateThreadpoolWait", s);
    register_api("kernel32.dll", "CreateThreadpoolWork", s);
    register_api("kernel32.dll", "SubmitThreadpoolWork", s);
    register_api("kernel32.dll", "TlsAlloc", (void*)THETAZ_TlsAlloc);
    register_api("kernel32.dll", "TlsFree", (void*)THETAZ_TlsFree);
    register_api("kernel32.dll", "FlsGetValue", (void*)THETAZ_FlsGetValue);
    register_api("kernel32.dll", "FlsGetValue2", (void*)THETAZ_FlsGetValue2);
    register_api("kernelbase.dll", "FlsGetValue2", (void*)THETAZ_FlsGetValue2);
    register_api("ntdll.dll", "FlsGetValue2", (void*)THETAZ_FlsGetValue2);
    register_api("kernel32.dll", "TlsGetValue", (void*)THETAZ_TlsGetValue);
    register_api("kernel32.dll", "FlsSetValue", (void*)THETAZ_FlsSetValue);
    register_api("kernel32.dll", "TlsSetValue", (void*)THETAZ_TlsSetValue);
    register_api("kernel32.dll", "ResetEvent", (void*)THETAZ_ResetEvent);
    register_api("kernel32.dll", "CreateSemaphoreA", (void*)THETAZ_CreateSemaphoreA);
    register_api("kernel32.dll", "CreateMutexA", (void*)THETAZ_CreateMutexA);
    register_api("kernel32.dll", "GetVersionExW", s);
    register_api("kernel32.dll", "GetDiskFreeSpaceExW", s);
    register_api("kernel32.dll", "CreateFileW", (void*)THETAZ_CreateFileW);
    register_api("kernel32.dll", "DeviceIoControl", s);
    register_api("kernel32.dll", "GetComputerNameW", s);
    register_api("kernel32.dll", "MulDiv", s);
    register_api("kernel32.dll", "FormatMessageW", s);
    register_api("kernel32.dll", "GetLogicalProcessorInformation", s);
    register_api("kernel32.dll", "HeapAlloc", (void*)THETAZ_HeapAlloc);
    register_api("kernel32.dll", "OpenEventW", s);
    register_api("kernel32.dll", "ReleaseMutex", (void*)THETAZ_ReleaseMutex);
    register_api("kernel32.dll", "CreateFileA", (void*)THETAZ_CreateFileA);
    register_api("kernel32.dll", "CloseHandle", (void*)THETAZ_CloseFileHandle);
    register_api("kernel32.dll", "GetModuleHandleExW", (void*)THETAZ_GetModuleHandleExW);
    register_api("kernel32.dll", "DisableThreadLibraryCalls", (void*)THETAZ_DisableThreadLibraryCalls);
    register_api("kernel32.dll", "IsBadStringPtrW", (void*)THETAZ_IsBadStringPtrW);
    register_api("kernel32.dll", "GetEnvironmentVariableW", (void*)THETAZ_GetEnvironmentVariableW);
    register_api("kernel32.dll", "ResolveDelayLoadedAPI", (void*)THETAZ_ResolveDelayLoadedAPI);
    register_api("kernel32.dll", "RtlUnwind", (void*)THETAZ_RtlUnwind);
    register_api("kernel32.dll", "lstrcmpW", (void*)THETAZ_lstrcmpW);
    register_api("kernel32.dll", "lstrcmpiW", (void*)THETAZ_lstrcmpiW);
    register_api("kernel32.dll", "CompareStringW", (void*)THETAZ_CompareStringW);
    register_api("kernel32.dll", "GetLocaleInfoW", (void*)THETAZ_GetLocaleInfoW);
    register_api("kernel32.dll", "GetDateFormatW", (void*)THETAZ_GetDateFormatW);
    register_api("kernel32.dll", "GetTimeFormatW", (void*)THETAZ_GetTimeFormatW);
    register_api("advapi32.dll", "RegOpenKeyExW", (void*)THETAZ_RegOpenKeyExW);
    register_api("advapi32.dll", "RegCreateKeyExW", (void*)THETAZ_RegCreateKeyExW);
    register_api("advapi32.dll", "RegCloseKey", (void*)THETAZ_RegCloseKey);
    register_api("advapi32.dll", "RegSetValueExW", (void*)THETAZ_RegSetValueExW);
    register_api("advapi32.dll", "RegQueryValueExW", (void*)THETAZ_RegQueryValueExW);
    register_api("advapi32.dll", "RegDeleteValueW", (void*)THETAZ_RegDeleteValueW);
    register_api("advapi32.dll", "RegOpenKeyExA", (void*)THETAZ_RegOpenKeyExA);
    register_api("advapi32.dll", "RegCreateKeyExA", (void*)THETAZ_RegCreateKeyExA);
    register_api("advapi32.dll", "RegCreateKeyW", (void*)THETAZ_RegCreateKeyW);
    register_api("advapi32.dll", "RegQueryValueExA", (void*)THETAZ_RegQueryValueExA);
    register_api("advapi32.dll", "RegSetValueExA", (void*)THETAZ_RegSetValueExA);
    register_api("advapi32.dll", "RegDeleteValueA", (void*)THETAZ_RegDeleteValueA);
    register_api("advapi32.dll", "RegGetValueW", (void*)THETAZ_RegGetValueW);
    register_api("kernel32.dll", "SetUnhandledExceptionFilter", (void*)THETAZ_SetUnhandledExceptionFilter);
    register_api("gdi32.dll", "DeleteObject", (void*)THETAZ_DeleteObject);
    register_api("gdi32.dll", "TextOutA", (void*)THETAZ_TextOutA);
    register_api("kernel32.dll", "CreateMutexExW", s);
    register_api("kernel32.dll", "OpenMutexW", s);
    register_api("kernel32.dll", "GetLongPathNameW", s);
    register_api("kernel32.dll", "GetFinalPathNameByHandleW", s);
    register_api("kernel32.dll", "TzSpecificLocalTimeToSystemTime", s);
    register_api("kernel32.dll", "QueryUnbiasedInterruptTime", s);
    register_api("kernel32.dll", "IsDebuggerPresent", s);
    register_api("kernel32.dll", "FindFirstFileExW", s);
    register_api("kernel32.dll", "MoveFileExW", s);
    register_api("kernel32.dll", "FindNextFileW", (void*)THETAZ_FindNextFileW);
    register_api("kernel32.dll", "RemoveDirectoryW", s);

    register_api("kernel32.dll", "SetFilePointerEx", (void*)THETAZ_SetFilePointerEx);
    register_api("kernel32.dll", "SetEndOfFile", s);
    register_api("kernel32.dll", "GetFileSizeEx", (void*)THETAZ_GetFileSizeEx);
    register_api("kernel32.dll", "FlushFileBuffers", (void*)THETAZ_FlushFileBuffers);
    register_api("kernel32.dll", "LockFileEx", s);
    register_api("kernel32.dll", "UnlockFileEx", s);
}
BOOL WINAPI_CALL THETAZ_VerifyVersionInfoW(void* a, DWORD b, uint64_t c){ (void)a;(void)b;(void)c; return FALSE; }
BOOL WINAPI_CALL THETAZ_GetProductInfo(DWORD a, DWORD b, DWORD c, DWORD d, DWORD* e){ (void)a;(void)b;(void)c;(void)d; if(e) *e=0; return FALSE; }
BOOL WINAPI_CALL THETAZ_GetPhysicallyInstalledSystemMemory(uint64_t* a){ (void)a; if(a) *a=0; return FALSE; }
BOOL WINAPI_CALL THETAZ_QueryProcessCycleTime(HANDLE h, uint64_t* t){ (void)h;(void)t; return FALSE; }
BOOL WINAPI_CALL THETAZ_QueryThreadCycleTime(HANDLE h, uint64_t* t){ (void)h;(void)t; return FALSE; }
UINT WINAPI_CALL THETAZ_SetErrorMode(UINT m){ (void)m; return 0; }
void* WINAPI_CALL THETAZ_RtlCaptureContext(void* c){ (void)c; return nullptr; }
uint64_t WINAPI_CALL THETAZ_RtlLookupFunctionEntry(uint64_t a, uint64_t* b, void* c){ (void)a;(void)b;(void)c; return 0; }
void* WINAPI_CALL THETAZ_RtlVirtualUnwind(DWORD a, uint64_t b, uint64_t c, void* d, void* e, void** f, uint64_t* g, void* h){ (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h; return nullptr; }

int WINAPI_CALL THETAZ__wcslwr(wchar_t* s){ (void)s; return 0; }
int WINAPI_CALL THETAZ_GetNameInfoW(const SOCKADDR* a, size_t b, WCHAR* c, DWORD d, WCHAR* e, DWORD f, INT g){ (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g; return 0; }
BOOL WINAPI_CALL THETAZ_GetProcessTimes(HANDLE h, FILETIME* a, FILETIME* b, FILETIME* c, FILETIME* d){ (void)h;(void)a;(void)b;(void)c;(void)d; return FALSE; }
BOOL WINAPI_CALL THETAZ_GetThreadTimes(HANDLE h, FILETIME* a, FILETIME* b, FILETIME* c, FILETIME* d){ (void)h;(void)a;(void)b;(void)c;(void)d; return FALSE; }
BOOL WINAPI_CALL THETAZ_GetProcessAffinityMask(HANDLE h, DWORD* a, DWORD* b){ (void)h; if(a) *a=0; if(b) *b=0; return TRUE; }
DWORD WINAPI_CALL THETAZ_GetPriorityClass(HANDLE h) {
    if (!h) { THETAZ_SetLastError(ERROR_INVALID_HANDLE); return 0; }
    const uintptr_t hv = reinterpret_cast<uintptr_t>(h);
    if (hv == static_cast<uintptr_t>(-1)) return 0x00000020u; // NORMAL_PRIORITY_CLASS
    if (ThetaServer::instance().get_process(static_cast<uint64_t>(hv))) return 0x00000020u;
    if (ThetaServer::instance().get_thread(static_cast<uint64_t>(hv))) return 0x00000020u;
    THETAZ_SetLastError(ERROR_INVALID_HANDLE);
    return 0;
}

namespace {
std::mutex g_handle_flags_mutex;
std::unordered_map<uintptr_t, uint32_t> g_handle_flags;
}
BOOL WINAPI_CALL THETAZ_SetHandleInformation(HANDLE h, DWORD mask, DWORD flags) {
    if (!h || h == reinterpret_cast<HANDLE>(static_cast<uintptr_t>(-1))) {
        THETAZ_SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    std::lock_guard<std::mutex> lock(g_handle_flags_mutex);
    uint32_t& cur = g_handle_flags[reinterpret_cast<uintptr_t>(h)];
    cur = (cur & ~mask) | (flags & mask);
    return TRUE;
}
BOOL WINAPI_CALL THETAZ_GetHandleInformation(HANDLE h, LPDWORD flags) {
    if (!flags) { THETAZ_SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    if (!h || h == reinterpret_cast<HANDLE>(static_cast<uintptr_t>(-1))) {
        THETAZ_SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    std::lock_guard<std::mutex> lock(g_handle_flags_mutex);
    auto it = g_handle_flags.find(reinterpret_cast<uintptr_t>(h));
    *flags = (it != g_handle_flags.end()) ? it->second : 0;
    return TRUE;
}
BOOL WINAPI_CALL THETAZ_SetThreadPriority(HANDLE h, int p){ (void)h;(void)p; return TRUE; }
BOOL WINAPI_CALL THETAZ_IsWow64Process(HANDLE h, PBOOL b){ (void)h; if(b) *b=FALSE; return TRUE; }
BOOL WINAPI_CALL THETAZ_SetProcessMitigationPolicy(DWORD a, void* b, SIZE_T c){ (void)a;(void)b;(void)c; return TRUE; }
BOOL WINAPI_CALL THETAZ_GetThreadIOPendingFlag(HANDLE h, PBOOL b){ (void)h; if(b) *b=FALSE; return TRUE; }
BOOL WINAPI_CALL THETAZ_ProcessIdToSessionId(DWORD a, DWORD* b){ (void)a; if(b) *b=0; return TRUE; }
BOOL WINAPI_CALL THETAZ_QueryFullProcessImageNameW(HANDLE h, DWORD f, LPWSTR b, PDWORD s){ (void)h;(void)f;(void)b;(void)s; return FALSE; }
DWORD WINAPI_CALL THETAZ_GetProcessHandleCount(HANDLE h, DWORD* c){ (void)h; if(c) *c=0; return 0; }
namespace {
uint64_t current_filetime_100ns() {
    struct timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);
    // 11644473600 seconds between 1601-01-01 and 1970-01-01.
    return (static_cast<uint64_t>(ts.tv_sec) + 11644473600ULL) * 10000000ULL +
           static_cast<uint64_t>(ts.tv_nsec) / 100ULL;
}
} // namespace

HANDLE WINAPI_CALL THETAZ_CreateWaitableTimerW(LPSECURITY_ATTRIBUTES attrs, BOOL manual, LPCWSTR name) {
    (void)attrs;
    std::string timer_name;
    if (name)
        timer_name = utf16le_to_utf8(reinterpret_cast<const uint16_t*>(name));

    const uint64_t pid = ThreadManager::instance().get_current_guest_pid();
    if (!timer_name.empty()) {
        uint64_t existing = 0;
        if (ThetaServer::instance().open_object(timer_name, ObjectType::Timer, pid, existing)) {
            THETAZ_SetLastError(ERROR_ALREADY_EXISTS);
            return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(existing));
        }
    }

    const uint64_t handle = ThetaServer::instance().create_timer(
        timer_name, manual != FALSE, pid);
    if (!handle) {
        THETAZ_SetLastError(ERROR_OUTOFMEMORY);
        return nullptr;
    }
    return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(handle));
}

BOOL WINAPI_CALL THETAZ_SetWaitableTimerEx(HANDLE timer, const LARGE_INTEGER* due_time,
                                           LONG period, void* completion_routine,
                                           void* completion_arg, void* wake_context,
                                           LONG reserved) {
    (void)completion_arg;
    (void)wake_context;
    (void)reserved;
    if (!timer) {
        THETAZ_SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    if (!due_time) {
        THETAZ_SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    // A completion routine must run in the guest. Silently arming the timer
    // without delivering the APC would be fake success, so reject it honestly
    // until guest APC delivery exists.
    if (completion_routine) {
        THETAZ_SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
        return FALSE;
    }

    const int64_t when = due_time->QuadPart;
    uint64_t due_ms = 0;
    if (when < 0) {
        // Negative values are relative intervals in 100 ns units.
        const uint64_t interval_100ns =
            static_cast<uint64_t>(-(when + 1)) + 1;
        due_ms = (interval_100ns + 9999ULL) / 10000ULL;
    } else if (when > 0) {
        // Positive values are absolute UTC FILETIME values.
        const uint64_t absolute = static_cast<uint64_t>(when);
        const uint64_t now = current_filetime_100ns();
        due_ms = absolute <= now ? 0 : (absolute - now + 9999ULL) / 10000ULL;
    }

    if (!ThetaServer::instance().set_timer(
            static_cast<uint64_t>(reinterpret_cast<uintptr_t>(timer)),
            due_ms, period > 0 ? static_cast<int>(period) : 0)) {
        THETAZ_SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    return TRUE;
}

BOOL WINAPI_CALL THETAZ_SetWaitableTimer(HANDLE timer, const LARGE_INTEGER* due_time,
                                         LONG period, void* completion_routine,
                                         void* completion_arg, BOOL resume) {
    (void)resume;
    return THETAZ_SetWaitableTimerEx(timer, due_time, period, completion_routine,
                                     completion_arg, nullptr, 0);
}

BOOL WINAPI_CALL THETAZ_CancelWaitableTimer(HANDLE timer) {
    if (!timer) {
        THETAZ_SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    if (!ThetaServer::instance().cancel_timer(
            static_cast<uint64_t>(reinterpret_cast<uintptr_t>(timer)))) {
        THETAZ_SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    return TRUE;
}
void WINAPI_CALL THETAZ_InitializeSListHead(void* h){ (void)h; }
void* WINAPI_CALL THETAZ_InterlockedPushEntrySList(void* a, void* b){ (void)a;(void)b; return nullptr; }
void* WINAPI_CALL THETAZ_InterlockedPopEntrySList(void* a){ (void)a; return nullptr; }
WORD WINAPI_CALL THETAZ_QueryDepthSList(void* a){ (void)a; return 0; }
// ---------------------------------------------------------------------------
// One-time initialization. The guest INIT_ONCE is a single 32-bit word:
// 0 = not started, 1 = in progress, 2 = done. The context pointer is kept host
// side because the Windows structure only has room for the state word.
// ---------------------------------------------------------------------------
namespace {
std::mutex g_init_once_mutex;
std::condition_variable g_init_once_cv;
std::unordered_map<uintptr_t, void*> g_init_once_context;
}
BOOL WINAPI_CALL THETAZ_InitOnceBeginInitialize(void* p, DWORD flags, BOOL* fPending,
                                                LPVOID* lpContext) {
    if (!p || !fPending) { THETAZ_SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    const uintptr_t key = reinterpret_cast<uintptr_t>(p);
    volatile uint32_t* state = reinterpret_cast<volatile uint32_t*>(key);
    std::unique_lock<std::mutex> lock(g_init_once_mutex);
    for (;;) {
        uint32_t st = *state;
        if (st == 2) {
            *fPending = FALSE;
            if (lpContext) *lpContext = g_init_once_context[key];
            return TRUE;
        }
        if (st == 1) { g_init_once_cv.wait(lock); continue; }
        if (flags & 0x1u) { // INIT_ONCE_CHECK_ONLY
            *fPending = TRUE;
            if (lpContext) *lpContext = nullptr;
            return TRUE;
        }
        *state = 1;
        *fPending = TRUE;
        if (lpContext) *lpContext = nullptr;
        return TRUE;
    }
}
BOOL WINAPI_CALL THETAZ_InitOnceComplete(void* p, DWORD flags, LPVOID context) {
    if (!p) { THETAZ_SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    const uintptr_t key = reinterpret_cast<uintptr_t>(p);
    std::lock_guard<std::mutex> lock(g_init_once_mutex);
    if (flags & 0x4u) { // INIT_ONCE_INIT_FAILED
        g_init_once_context.erase(key);
        *reinterpret_cast<volatile uint32_t*>(key) = 0;
    } else {
        g_init_once_context[key] = context;
        *reinterpret_cast<volatile uint32_t*>(key) = 2;
    }
    g_init_once_cv.notify_all();
    return TRUE;
}
BOOL WINAPI_CALL THETAZ_InitOnceExecuteOnce(void* p, void* callback, void* parameter,
                                            LPVOID* context) {
    if (!p || !callback) { THETAZ_SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    BOOL pending = FALSE;
    void* ctx = nullptr;
    if (!THETAZ_InitOnceBeginInitialize(p, 0, &pending, &ctx)) return FALSE;
    if (!pending) {
        if (context) *context = ctx;
        return TRUE;
    }
    uint32_t ctx_slot = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(alloc_guest_shared(4)));
    if (!ctx_slot) { THETAZ_InitOnceComplete(p, 0x4, nullptr); return FALSE; }
    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ctx_slot)) =
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ctx));
    BOOL ok = FALSE;
    if (x86_dispatch_active() && guest_is_32bit()) {
        uint32_t args[3] = {
            static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)),
            static_cast<uint32_t>(reinterpret_cast<uintptr_t>(parameter)),
            ctx_slot,
        };
        ok = x86_call_guest_args(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(callback)),
                                 args, 3) ? TRUE : FALSE;
    } else {
        typedef BOOL(__attribute__((ms_abi)) * InitOnceFn)(void*, void*, void**);
        ok = reinterpret_cast<InitOnceFn>(callback)(p, parameter,
                                                    reinterpret_cast<void**>(static_cast<uintptr_t>(ctx_slot)));
    }
    void* new_ctx = reinterpret_cast<void*>(static_cast<uintptr_t>(
        *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ctx_slot))));
    if (!ok) { THETAZ_InitOnceComplete(p, 0x4, nullptr); return FALSE; }
    if (!THETAZ_InitOnceComplete(p, 0, new_ctx)) return FALSE;
    if (context) *context = new_ctx;
    return TRUE;
}

// AppPolicy*: the CRT asks how the app expects to terminate. Desktop/console
// apps use ExitProcess semantics (0); TerminateProcess semantics is (1).
LONG WINAPI_CALL THETAZ_AppPolicyGetProcessTerminationMethod(HANDLE token, DWORD* policy) {
    (void)token;
    if (!policy) return 0x80070057L; // E_INVALIDARG
    *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(policy)) = 0;
    return 0;
}
LONG WINAPI_CALL THETAZ_AppPolicyGetThreadInitializationType(HANDLE token, DWORD* policy) {
    (void)token;
    if (!policy) return 0x80070057L;
    *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(policy)) = 0;
    return 0;
}
BOOL WINAPI_CALL THETAZ_TerminateProcess(HANDLE h, UINT c){
    // Pseudo-handle -1 denotes the current process. Terminating it must
    // actually end the guest process; returning success here would let the
    // CCRT's fatal-error path continue into a trap instruction.
    if (h == reinterpret_cast<HANDLE>(static_cast<uintptr_t>(-1))) {
        terminate_current_process(c);
        return TRUE;
    }
    // Child process handles are the child pid in ThetaZ.
    if (ThetaServer::instance().get_process(
            static_cast<uint64_t>(reinterpret_cast<uintptr_t>(h)))) {
        ThetaServer::instance().terminate_process(
            static_cast<uint64_t>(reinterpret_cast<uintptr_t>(h)), c);
        return TRUE;
    }
    THETAZ_SetLastError(ERROR_INVALID_HANDLE);
    return FALSE;
}
DWORD WINAPI_CALL THETAZ_GetExitCodeThread(HANDLE h, LPDWORD c){
    if (!c) return FALSE;
    if (!h) { *c = 0; return FALSE; }
    ThreadObject* t = ThetaServer::instance().get_thread((uint64_t)(uintptr_t)h);
    if (!t) { *c = 0; return FALSE; }
    *c = t->terminated ? t->exit_code : 0x103;
    return TRUE;
}
HANDLE WINAPI_CALL THETAZ_OpenProcess(DWORD a, BOOL b, DWORD c){ (void)a;(void)b;(void)c; return (HANDLE)0x6; }
BOOL WINAPI_CALL THETAZ_InitializeCriticalSectionEx(PCRITICAL_SECTION a, DWORD b, DWORD c){ (void)b;(void)c; THETAZ_InitializeCriticalSection(a); return TRUE; }
BOOL WINAPI_CALL THETAZ_InitializeCriticalSectionAndSpinCount(PCRITICAL_SECTION a, DWORD b){ (void)b; THETAZ_InitializeCriticalSection(a); return TRUE; }
void WINAPI_CALL THETAZ_InitConditionVariable(void* a){ (void)a; }
void WINAPI_CALL THETAZ_DeleteConditionVariable(void* a){ (void)a; }
void release_fls_process(uint64_t p){ (void)p; }
BOOL WINAPI64_IsWindow(uint64_t h){ (void)h; return 0; }
BOOL WINAPI64_IsWindowEnabled(uint64_t h){ (void)h; return 0; }
BOOL WINAPI64_IsWindowVisible(uint64_t h){ (void)h; return 0; }
BOOL WINAPI64_KillTimer(uint64_t h, uintptr_t i){ (void)h;(void)i; return 0; }
BOOL WINAPI64_GetMessageA(void* m, uint64_t h, uint32_t a, uint32_t b){ (void)m;(void)h;(void)a;(void)b; return 0; }
BOOL WINAPI64_GetWindowTextA(uint64_t h, char* b, int m){ (void)h;(void)b;(void)m; return 0; }
int WINAPI64_GetWindowTextLengthA(uint64_t h){ (void)h; return 0; }
BOOL WINAPI64_InvalidateRect(uint64_t h, const void* r, BOOL e){ (void)h;(void)r;(void)e; return 0; }
BOOL WINAPI64_MessageBoxA(uint64_t h, const char* a, const char* b, uint32_t c){ (void)h;(void)a;(void)b;(void)c; return 0; }
BOOL WINAPI64_PeekMessageA(void* m, uint64_t h, uint32_t a, uint32_t b, uint32_t c){ (void)m;(void)h;(void)a;(void)b;(void)c; return 0; }
BOOL WINAPI64_PostMessageA(uint64_t h, uint32_t m, uint64_t w, uint64_t l){ (void)h;(void)m;(void)w;(void)l; return 0; }
BOOL WINAPI64_EnableWindow(uint64_t h, BOOL e){ (void)h;(void)e; return 0; }
INT_PTR WINAPI64_EndDialog(uint64_t h, INT_PTR r){ (void)h;(void)r; return 0; }
BOOL WINAPI64_EndPaint(uint64_t h, const void* p){ (void)h;(void)p; return 0; }
int WINAPI64_GetDlgCtrlID(uint64_t h){ (void)h; return 0; }
BOOL WINAPI64_GetDlgItemTextA(uint64_t h, int id, char* b, int m){ (void)h;(void)id;(void)b;(void)m; return 0; }
void call_wnd_proc(Window* w, uint32_t m, uint64_t wp, uint64_t lp){ (void)w;(void)m;(void)wp;(void)lp; }
std::string windows_to_linux_path(const std::string& s) { return s; }
BOOL WINAPI_CALL THETAZ_ReleaseMutex(HANDLE h) {
    if (!h) return FALSE;
    return ThetaServer::instance().release_mutex((uint64_t)(uintptr_t)h) ? TRUE : FALSE;
}
HANDLE WINAPI_CALL THETAZ_CreateEventA(LPSECURITY_ATTRIBUTES a, BOOL manual, BOOL initial, LPCSTR n) {
    (void)a;
    std::string name = n ? n : "";
    uint64_t pid = ThreadManager::instance().get_current_guest_pid();
    uint64_t h = ThetaServer::instance().create_event(name, manual != FALSE, initial != FALSE, pid);
    return (HANDLE)(uintptr_t)h;
}
HANDLE WINAPI_CALL THETAZ_CreateEventW(LPSECURITY_ATTRIBUTES a, BOOL manual, BOOL initial, LPCWSTR n) {
    (void)a;
    std::string name;
    if (n) {
        const uint16_t* p = reinterpret_cast<const uint16_t*>(n);
        while (*p) name.push_back(static_cast<char>(*p++ & 0xFF));
    }
    uint64_t pid = ThreadManager::instance().get_current_guest_pid();
    uint64_t h = ThetaServer::instance().create_event(name, manual != FALSE, initial != FALSE, pid);
    return (HANDLE)(uintptr_t)h;
}
HANDLE WINAPI_CALL THETAZ_CreateEventExW(LPSECURITY_ATTRIBUTES a, LPCWSTR n,
                                         DWORD flags, DWORD desired_access) {
    (void)desired_access;
    // CREATE_EVENT_MANUAL_RESET = 1, CREATE_EVENT_INITIAL_SET = 2.
    return THETAZ_CreateEventW(a, (flags & 0x1u) ? TRUE : FALSE,
                               (flags & 0x2u) ? TRUE : FALSE, n);
}
HANDLE WINAPI_CALL THETAZ_CreateSemaphoreA(LPSECURITY_ATTRIBUTES a, LONG init, LONG max, LPCSTR n) {
    (void)a;
    std::string name = n ? n : "";
    uint64_t pid = ThreadManager::instance().get_current_guest_pid();
    uint64_t h = ThetaServer::instance().create_semaphore(name, (int)init, (int)max, pid);
    return (HANDLE)(uintptr_t)h;
}
HANDLE WINAPI_CALL THETAZ_CreateMutexA(LPSECURITY_ATTRIBUTES a, BOOL owned, LPCSTR n) {
    (void)a;
    std::string name = n ? n : "";
    uint64_t pid = ThreadManager::instance().get_current_guest_pid();
    uint64_t h = ThetaServer::instance().create_mutex(name, pid);
    if (h && owned) ThetaServer::instance().acquire_mutex(h);
    return (HANDLE)(uintptr_t)h;
}
BOOL WINAPI_CALL THETAZ_ReleaseSemaphore(HANDLE h, LONG count, LONG* prev) {
    if (!h) return FALSE;
    int prev_val = ThetaServer::instance().release_semaphore((uint64_t)(uintptr_t)h, (int)count);
    if (prev_val < 0) return FALSE;
    if (prev) *prev = (LONG)prev_val;
    return TRUE;
}
BOOL WINAPI_CALL THETAZ_GetModuleHandleExW(DWORD flags, LPCWSTR name, HMODULE* out) {
    if (!out) {
        THETAZ_SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    constexpr DWORD GET_MODULE_HANDLE_EX_FLAG_PIN = 0x1;
    constexpr DWORD GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT = 0x2;
    constexpr DWORD GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS = 0x4;

    if (flags & GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS) {
        // lpModuleName is an address in this mode; find the image containing it.
        uint64_t addr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(name));
        PELoader& loader = PELoader::instance();
        const LoadedModule* found = nullptr;
        for (const auto& entry : loader.get_modules()) {
            const LoadedModule& m = entry.second;
            if (m.base && addr >= m.base && addr < m.base + m.size) {
                found = &m;
                break;
            }
        }
        if (!found) {
            THETAZ_SetLastError(ERROR_MOD_NOT_FOUND);
            return FALSE;
        }
        *out = (HMODULE)(uintptr_t)found->base;
        return TRUE;
    }

    std::string aname;
    if (name) aname = utf16le_to_utf8(reinterpret_cast<const uint16_t*>(name));
    HMODULE h = module_handle_from_name(aname.empty() ? nullptr : aname.c_str());
    if (!h) return FALSE;

    // ThetaZ never unloads builtin modules and child images are only released
    // when their process terminates. Reference counting is therefore a no-op,
    // but both PIN and UNCHANGED_REFCOUNT are accepted with their documented
    // success result.
    (void)(flags & (GET_MODULE_HANDLE_EX_FLAG_PIN |
                    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT));
    *out = h;
    return TRUE;
}
BOOL WINAPI_CALL THETAZ_DisableThreadLibraryCalls(HMODULE h) {
    if (!h) return FALSE;
    PELoader& loader = PELoader::instance();
    LoadedModule* m = loader.find_module_by_base((uint64_t)(uintptr_t)h);
    if (!m) return FALSE;
    m->disable_thread_calls = true;
    return TRUE;
}
static bool maps_range_readable(uintptr_t start, size_t len) {
    if (len == 0) return true;
    uintptr_t end = start + len;
    if (end < start) return false;
    FILE* fp = fopen("/proc/self/maps", "r");
    if (!fp) return false;
    char line[256];
    uintptr_t covered = start;
    while (fgets(line, sizeof(line), fp)) {
        uintptr_t rs, re;
        char perms[5] = {0};
        if (sscanf(line, "%lx-%lx %4s", &rs, &re, perms) != 3) continue;
        if (perms[0] != 'r') continue;
        if (rs <= covered && re > covered) {
            covered = re;
            if (covered >= end) break;
        }
    }
    fclose(fp);
    return covered >= end;
}
void WINAPI_CALL THETAZ_RtlUnwind(void* target_frame, void* target_ip, void* rec_, DWORD retval) {
    if (!guest_is_32bit()) return;
    if (!x86_dispatch_active()) return;
    uint32_t target = (uint32_t)(uintptr_t)target_frame;
    uint32_t tip = (uint32_t)(uintptr_t)target_ip;
    uint32_t rec = (uint32_t)(uintptr_t)rec_;
    uint32_t local_rec = 0;
    if (!rec) {
        local_rec = (uint32_t)(uintptr_t)alloc_guest_shared(128);
        if (!local_rec) return;
        memset(reinterpret_cast<void*>(static_cast<uintptr_t>(local_rec)), 0, 128);
        *(uint32_t*)(uintptr_t)local_rec = 0;
        *(uint32_t*)(uintptr_t)(local_rec + 4) = 0x2;
        rec = local_rec;
    }
    X86CpuState* st = x86_current_state();
    uint32_t teb = (st && st->fs_base) ? st->fs_base : 0x7FFF0000u;
    uint32_t fs_head = *(uint32_t*)(uintptr_t)teb;
    uint32_t frame = fs_head;
    uint32_t old_flags = *(uint32_t*)(uintptr_t)(rec + 4);
    const uint32_t unwind_flags = old_flags | 0x6u; // UNWINDING | EXIT_UNWIND
    *(uint32_t*)(uintptr_t)(rec + 4) = unwind_flags;
    (void)tip;
    (void)retval;

    // Build a CONTEXT/DISPATCHER_CONTEXT pair valid for guest handlers. The
    // previous implementation passed NULL for both; MSVC's __CxxFrameHandler3
    // dereferences them (it validates CONTEXT.Esp and walks FuncInfo state).
    uint32_t ctx = st ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(alloc_guest_shared(0x2CC))) : 0;
    uint32_t disp = st ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(alloc_guest_shared(0x40))) : 0;
    if (ctx && st) {
        memset(reinterpret_cast<void*>(static_cast<uintptr_t>(ctx)), 0, 0x2CC);
        auto w32 = [](uint32_t addr, uint32_t v) {
            *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(addr)) = v;
        };
        w32(ctx + 0x00, 0x00010007);
        w32(ctx + 0x9C, st->regs[XR_EDI]);
        w32(ctx + 0xA0, st->regs[XR_ESI]);
        w32(ctx + 0xA4, st->regs[XR_EBX]);
        w32(ctx + 0xA8, st->regs[XR_EDX]);
        w32(ctx + 0xAC, st->regs[XR_ECX]);
        w32(ctx + 0xB0, st->regs[XR_EAX]);
        const bool have_saved = (g_saved_exception.record == rec && g_saved_exception.fault_pc);
        w32(ctx + 0xB4, have_saved ? g_saved_exception.fault_ebp : st->regs[XR_EBP]);
        w32(ctx + 0xB8, have_saved ? g_saved_exception.fault_pc : st->eip);
        *reinterpret_cast<uint16_t*>(static_cast<uintptr_t>(ctx + 0xBC)) = 0x23;
        w32(ctx + 0xC0, st->eflags);
        w32(ctx + 0xC4, have_saved ? g_saved_exception.fault_esp : st->regs[XR_ESP]);
        *reinterpret_cast<uint16_t*>(static_cast<uintptr_t>(ctx + 0xC8)) = 0x2B;
        w32(ctx + 0x90, 0x3B);
        w32(ctx + 0x94, 0x2B);
        w32(ctx + 0x98, 0x2B);
    }
    if (disp) memset(reinterpret_cast<void*>(static_cast<uintptr_t>(disp)), 0, 0x40);

    const bool unwind_trace = getenv("THETAZ_TRACE_SEH") != nullptr;
    if (unwind_trace) {
        uint32_t esp = st ? st->regs[XR_ESP] : 0;
        uint32_t ebp = st ? st->regs[XR_EBP] : 0;
        fprintf(stderr, "[seh] unwind target=0x%08X tip=0x%08X head=0x%08X rec=0x%08X flags=0x%X eip=0x%08X esp=0x%08X ebp=0x%08X ret=0x%08X caller_ebp=0x%08X\n",
                target, tip, fs_head, rec, old_flags,
                st ? st->eip : 0, esp, ebp,
                (st && maps_range_readable(esp, 4)) ? *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(esp)) : 0,
                (st && maps_range_readable(ebp, 8)) ? *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ebp + 4)) : 0);
        uint32_t b = ebp;
        for (int i = 0; i < 8 && b >= 0x1000 && b < 0x80000000u && maps_range_readable(b, 8); i++) {
            fprintf(stderr, "[seh] unwind backtrace[%d] ebp=0x%08X ret=0x%08X\n",
                    i, b, *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(b + 4)));
            uint32_t next = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(b));
            if (next <= b) break;
            b = next;
        }
    }
    // RtlUnwind runs on the handler's stack. Start from the original throwing
    // context recorded by RaiseException so the first frame's control PC is
    // the real throw site, not the inside of _UnwindNestedFrames.
    uint32_t inner_ebp = (g_saved_exception.record == rec) ? g_saved_exception.fault_ebp
                                                           : (st ? st->regs[XR_EBP] : 0);
    int depth = 0;
    while (frame && frame != target && depth++ < 64) {
        if (frame < 0x1000 || frame >= 0x80000000u) break;
        uint32_t handler = *(uint32_t*)(uintptr_t)(frame + 4);
        uint32_t next = *(uint32_t*)(uintptr_t)frame;
        if (handler && handler != 0xFFFFFFFFu && ctx && disp) {
            const SehFrameLayout layout = seh_frame_layout(frame, inner_ebp);
            uint32_t saved_ebp = layout.valid ? layout.current_ebp : inner_ebp;
            uint32_t control_pc = inner_ebp;
            if (maps_range_readable(static_cast<uintptr_t>(inner_ebp) + 4, 4)) {
                control_pc = *(uint32_t*)(uintptr_t)(inner_ebp + 4);
            }
            auto w32 = [](uint32_t addr, uint32_t v) {
                *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(addr)) = v;
            };
            w32(ctx + 0xB8, control_pc);
            w32(ctx + 0xB4, saved_ebp);
            // Same real Win32 DISPATCHER_CONTEXT layout as the dispatch path
            // (see seh:: in core/x86/x86.h). During an unwind TargetIp carries
            // the continuation address the unwind is heading for.
            uint32_t unwind_hdata = *(uint32_t*)(uintptr_t)(frame + 8);
            w32(disp + seh::kDcControlPc, control_pc);
            w32(disp + seh::kDcImageBase, image_base_for_address(control_pc));
            w32(disp + seh::kDcFunctionEntry, 0);
            w32(disp + seh::kDcEstablisherFrame, frame);
            w32(disp + seh::kDcTargetIp, tip);
            w32(disp + seh::kDcContextRecord, ctx);
            w32(disp + seh::kDcLanguageHandler, handler);
            w32(disp + seh::kDcHandlerData, unwind_hdata);
            uint32_t hargs[4] = { rec, frame, ctx, disp };
            uint32_t eax_seed = looks_like_func_info(unwind_hdata) ? unwind_hdata : 0;
            uint32_t disposition = x86_call_guest_handler(handler, hargs, 4, eax_seed);
            if (unwind_trace) {
                fprintf(stderr, "[seh] unwind frame=0x%08X handler=0x%08X eip=0x%08X ebp=0x%08X disp=0x%X\n",
                        frame, handler, control_pc, saved_ebp, disposition);
            }
            inner_ebp = layout.valid ? layout.saved_ebp : inner_ebp;
        }
        frame = next;
    }
    *(uint32_t*)(uintptr_t)(rec + 4) = old_flags;
    *(uint32_t*)(uintptr_t)teb = target ? target : frame;
}
namespace {
constexpr uintptr_t kHkcr = 0x80000000u;
constexpr uintptr_t kHkcu = 0x80000001u;
constexpr uintptr_t kHklm = 0x80000002u;
constexpr uintptr_t kHku = 0x80000003u;
constexpr uintptr_t kHkcc = 0x80000005u;
struct RegValue {
    DWORD type = 0;
    std::vector<uint8_t> data;
};
struct RegKey {
    std::string name;
    std::unordered_map<std::string, std::shared_ptr<RegKey>> subkeys;
    std::unordered_map<std::string, RegValue> values;
};
std::mutex g_reg_mutex;
std::unordered_map<uintptr_t, std::shared_ptr<RegKey>> g_reg_handles;
std::atomic<uintptr_t> g_reg_next{0x30000};
std::shared_ptr<RegKey> g_reg_root;
std::once_flag g_reg_once;

std::vector<std::string> reg_split(const std::string& path);

int reg_hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

void reg_append_utf8_cp(std::string& out, uint32_t cp) {
    if (cp == 0) return;
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

std::string reg_utf16_to_utf8(const std::u16string& in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); i++) {
        uint32_t cp = in[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < in.size()) {
            uint32_t lo = in[i + 1];
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                i++;
            } else {
                cp = 0xFFFD;
            }
        } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
            cp = 0xFFFD;
        }
        reg_append_utf8_cp(out, cp);
    }
    return out;
}

// Wine's .reg text format escapes UTF-16 code units as \xNNNN and uses \xNN
// for small string bytes. The "wide" mode is used for key and value names,
// where every escape is a UTF-16 code unit; string data uses the shorter form
// when the writer emitted it.
bool reg_parse_escaped_text(const std::string& s, size_t& pos, std::u16string& out,
        bool stop_at_quote, bool wide) {
    const uint8_t* base = reinterpret_cast<const uint8_t*>(s.data());
    const uint8_t* end = base + s.size();
    while (pos < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[pos]);
        if (stop_at_quote && c == '"') {
            pos++;
            return true;
        }
        if (c == '\\') {
            pos++;
            if (pos >= s.size()) return false;
            char e = s[pos++];
            switch (e) {
                case '\\': out.push_back(u'\\'); break;
                case '"': out.push_back(u'"'); break;
                case 'n': out.push_back(u'\n'); break;
                case 'r': out.push_back(u'\r'); break;
                case 't': out.push_back(u'\t'); break;
                case '0': out.push_back(0); break;
                case 'x': {
                    size_t start = pos;
                    unsigned v = 0;
                    int n = 0;
                    while (n < 4 && pos < s.size()) {
                        int d = reg_hex_digit(s[pos]);
                        if (d < 0) break;
                        v = (v << 4) | static_cast<unsigned>(d);
                        pos++;
                        n++;
                    }
                    if (n == 0) {
                        out.push_back(u'x');
                        break;
                    }
                    if (!wide && n >= 4 && v <= 0xFF) {
                        unsigned first2 = 0;
                        for (int i = 0; i < 2; i++)
                            first2 = (first2 << 4) | static_cast<unsigned>(reg_hex_digit(s[start + i]));
                        out.push_back(static_cast<char16_t>(first2));
                        pos = start + 2;
                    } else {
                        out.push_back(static_cast<char16_t>(v & 0xFFFF));
                    }
                    break;
                }
                default:
                    out.push_back(static_cast<char16_t>(static_cast<unsigned char>(e)));
                    break;
            }
        } else {
            const uint8_t* p = base + pos;
            uint32_t cp = 0;
            if (utf8_next(p, end, cp)) {
                pos = static_cast<size_t>(p - base);
                utf16_emit(out, cp);
            } else {
                out.push_back(static_cast<char16_t>(c));
                pos++;
            }
        }
    }
    return !stop_at_quote;
}

bool reg_parse_hex_number(const std::string& s, size_t begin, size_t end, unsigned& out) {
    unsigned v = 0;
    int n = 0;
    for (size_t i = begin; i < end; i++) {
        int d = reg_hex_digit(s[i]);
        if (d < 0) return false;
        v = (v << 4) | static_cast<unsigned>(d);
        n++;
        if (n > 8) return false;
    }
    if (n == 0) return false;
    out = v;
    return true;
}

bool reg_parse_hex_bytes(const std::string& s, size_t pos, std::vector<uint8_t>& out) {
    int hi = -1;
    for (; pos < s.size(); pos++) {
        unsigned char c = static_cast<unsigned char>(s[pos]);
        if (c == ',' || std::isspace(c)) continue;
        int d = reg_hex_digit(static_cast<char>(c));
        if (d < 0) return false;
        if (hi < 0) {
            hi = d;
        } else {
            out.push_back(static_cast<uint8_t>((hi << 4) | d));
            hi = -1;
        }
    }
    return hi < 0;
}

void reg_append_utf16le(std::vector<uint8_t>& out, const std::u16string& s) {
    out.reserve(out.size() + s.size() * 2);
    for (char16_t c : s) {
        out.push_back(static_cast<uint8_t>(c & 0xFF));
        out.push_back(static_cast<uint8_t>((c >> 8) & 0xFF));
    }
}

void reg_ensure_nul(std::u16string& s) {
    if (s.empty() || s.back() != 0) s.push_back(0);
}

void reg_ensure_multi_sz(std::u16string& s) {
    if (s.empty() || s.back() != 0) s.push_back(0);
    if (s.size() < 2 || s[s.size() - 2] != 0) s.push_back(0);
}

bool reg_parse_value_name(const std::string& s, size_t& pos, std::string& name) {
    if (pos >= s.size()) return false;
    if (s[pos] == '@') {
        name.clear();
        pos++;
        return true;
    }
    if (s[pos] != '"') return false;
    pos++;
    std::u16string u;
    if (!reg_parse_escaped_text(s, pos, u, true, true)) return false;
    name = reg_utf16_to_utf8(u);
    return true;
}

bool reg_parse_value_data(const std::string& s, size_t pos, RegValue& v) {
    while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) pos++;
    if (pos >= s.size()) {
        v.type = 1;
        std::u16string empty(1, 0);
        reg_append_utf16le(v.data, empty);
        return true;
    }
    if (s[pos] == '"') {
        pos++;
        std::u16string u;
        if (!reg_parse_escaped_text(s, pos, u, true, false)) return false;
        reg_ensure_nul(u);
        v.type = 1;
        reg_append_utf16le(v.data, u);
        return true;
    }
    if (s.compare(pos, 4, "str(") == 0) {
        size_t close = s.find(')', pos + 4);
        if (close == std::string::npos) return false;
        unsigned type = 0;
        if (!reg_parse_hex_number(s, pos + 4, close, type)) return false;
        pos = close + 1;
        if (pos >= s.size() || s[pos] != ':') return false;
        pos++;
        while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) pos++;
        if (pos >= s.size() || s[pos] != '"') return false;
        pos++;
        std::u16string u;
        if (!reg_parse_escaped_text(s, pos, u, true, false)) return false;
        if (type == 7) reg_ensure_multi_sz(u);
        else reg_ensure_nul(u);
        v.type = type;
        reg_append_utf16le(v.data, u);
        return true;
    }
    if (s.compare(pos, 4, "hex(") == 0) {
        size_t close = s.find(')', pos + 4);
        if (close == std::string::npos) return false;
        unsigned type = 0;
        if (!reg_parse_hex_number(s, pos + 4, close, type)) return false;
        pos = close + 1;
        if (pos >= s.size() || s[pos] != ':') return false;
        pos++;
        v.type = type;
        return reg_parse_hex_bytes(s, pos, v.data);
    }
    if (s.compare(pos, 4, "hex:") == 0) {
        v.type = 3;
        return reg_parse_hex_bytes(s, pos + 4, v.data);
    }
    if (s.compare(pos, 6, "dword:") == 0) {
        unsigned x = 0;
        int n = 0;
        for (size_t i = pos + 6; i < s.size(); i++) {
            unsigned char c = static_cast<unsigned char>(s[i]);
            if (std::isspace(c)) continue;
            int d = reg_hex_digit(static_cast<char>(c));
            if (d < 0 || n >= 8) return false;
            x = (x << 4) | static_cast<unsigned>(d);
            n++;
        }
        if (n == 0) return false;
        x <<= (8 - n) * 4;
        v.type = 4;
        v.data = {
            static_cast<uint8_t>(x & 0xFF),
            static_cast<uint8_t>((x >> 8) & 0xFF),
            static_cast<uint8_t>((x >> 16) & 0xFF),
            static_cast<uint8_t>((x >> 24) & 0xFF),
        };
        return true;
    }
    return false;
}

bool reg_parse_section_line(const std::string& line, std::string& path) {
    size_t lb = line.find('[');
    if (lb == std::string::npos) return false;
    size_t rb = line.rfind(']');
    if (rb == std::string::npos || rb <= lb) return false;
    std::string raw = line.substr(lb + 1, rb - lb - 1);
    std::u16string u;
    size_t pos = 0;
    if (!reg_parse_escaped_text(raw, pos, u, false, true)) return false;
    if (pos != raw.size()) return false;
    path = reg_utf16_to_utf8(u);
    return true;
}

std::string reg_strip_root_prefix(const std::string& path) {
    static const char* roots[] = {
        "HKEY_CLASSES_ROOT\\",
        "HKEY_CURRENT_USER\\",
        "HKEY_LOCAL_MACHINE\\",
        "HKEY_USERS\\",
        "HKEY_CURRENT_CONFIG\\",
    };
    for (const char* root : roots) {
        size_t n = std::strlen(root);
        if (path.compare(0, n, root) == 0) return path.substr(n);
    }
    return path;
}

bool reg_ends_with_continuation(const std::string& s) {
    if (s.empty() || s.back() != '\\') return false;
    size_t n = 0;
    for (size_t i = s.size(); i > 0 && s[i - 1] == '\\'; i--) n++;
    return (n % 2) == 1;
}

size_t reg_load_hive(const std::string& path, const std::shared_ptr<RegKey>& root) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return 0;
    std::string line;
    std::string logical;
    std::shared_ptr<RegKey> current = root;
    size_t lineno = 0;
    size_t loaded_values = 0;
    size_t malformed = 0;
    auto process = [&](const std::string& text, size_t where) {
        if (text.empty()) return;
        if (text[0] == '#' || text[0] == ';') return;
        if (text.compare(0, 13, "WINE REGISTRY") == 0) return;
        if (text[0] == '[') {
            std::string key_path;
            if (!reg_parse_section_line(text, key_path)) {
                malformed++;
                if (getenv("THETAZ_TRACE_REG"))
                    fprintf(stderr, "[reg] %s:%zu malformed section: %s\n", path.c_str(), where, text.c_str());
                return;
            }
            key_path = reg_strip_root_prefix(key_path);
            current = root;
            for (const auto& part : reg_split(key_path)) {
                if (part.empty()) continue;
                auto it = current->subkeys.find(part);
                if (it == current->subkeys.end()) {
                    auto nk = std::make_shared<RegKey>();
                    nk->name = part;
                    current->subkeys[part] = nk;
                    current = nk;
                } else {
                    current = it->second;
                }
            }
            return;
        }
        if (text[0] == '"' || text[0] == '@') {
            if (!current) return;
            size_t pos = 0;
            std::string name;
            if (!reg_parse_value_name(text, pos, name)) {
                malformed++;
                if (getenv("THETAZ_TRACE_REG"))
                    fprintf(stderr, "[reg] %s:%zu malformed value name: %s\n", path.c_str(), where, text.c_str());
                return;
            }
            if (pos >= text.size() || text[pos] != '=') {
                malformed++;
                if (getenv("THETAZ_TRACE_REG"))
                    fprintf(stderr, "[reg] %s:%zu missing '=': %s\n", path.c_str(), where, text.c_str());
                return;
            }
            pos++;
            RegValue v;
            if (!reg_parse_value_data(text, pos, v)) {
                malformed++;
                if (getenv("THETAZ_TRACE_REG"))
                    fprintf(stderr, "[reg] %s:%zu malformed value data: %s\n", path.c_str(), where, text.c_str());
                return;
            }
            current->values[name] = std::move(v);
            loaded_values++;
            return;
        }
    };
    while (std::getline(f, line)) {
        lineno++;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!logical.empty()) {
            size_t p = 0;
            while (p < line.size() && (line[p] == ' ' || line[p] == '\t')) p++;
            logical += line.substr(p);
        } else {
            logical = line;
        }
        if (reg_ends_with_continuation(logical)) {
            logical.pop_back();
            continue;
        }
        process(logical, lineno);
        logical.clear();
    }
    if (!logical.empty()) process(logical, lineno);
    const char* tr = getenv("THETAZ_TRACE_REG");
    if (tr && tr[0] && tr[0] != '0') {
        fprintf(stderr, "[reg] Loaded %s: %zu values, %zu malformed lines\n",
                path.c_str(), loaded_values, malformed);
    } else if (malformed > 0) {
        fprintf(stderr, "Registry: warning: skipped %zu malformed lines in %s\n", malformed, path.c_str());
    }
    return loaded_values;
}

void reg_init() {
    std::call_once(g_reg_once, [] {
        g_reg_root = std::make_shared<RegKey>();
        g_reg_root->name = "";
        auto mk = [&](const char* n) {
            auto k = std::make_shared<RegKey>();
            k->name = n;
            g_reg_root->subkeys[n] = k;
            return k;
        };
        mk("HKEY_CLASSES_ROOT");
        mk("HKEY_CURRENT_USER");
        mk("HKEY_LOCAL_MACHINE");
        mk("HKEY_USERS");
        mk("HKEY_CURRENT_CONFIG");
        g_reg_handles[kHkcr] = g_reg_root->subkeys["HKEY_CLASSES_ROOT"];
        g_reg_handles[kHkcu] = g_reg_root->subkeys["HKEY_CURRENT_USER"];
        g_reg_handles[kHklm] = g_reg_root->subkeys["HKEY_LOCAL_MACHINE"];
        g_reg_handles[kHku] = g_reg_root->subkeys["HKEY_USERS"];
        g_reg_handles[kHkcc] = g_reg_root->subkeys["HKEY_CURRENT_CONFIG"];

        std::string prefix;
        try {
            prefix = ThetaZ::get_prefix_path();
        } catch (...) {
            prefix.clear();
        }
        if (!prefix.empty()) {
            auto user = g_reg_root->subkeys["HKEY_CURRENT_USER"];
            auto machine = g_reg_root->subkeys["HKEY_LOCAL_MACHINE"];
            // Wine's default-user hive supplies values that user.reg may not
            // repeat; user.reg is loaded afterwards and overrides it.
            reg_load_hive(prefix + "/userdef.reg", user);
            reg_load_hive(prefix + "/user.reg", user);
            reg_load_hive(prefix + "/system.reg", machine);
        }
    });
}
std::shared_ptr<RegKey> reg_lookup(uintptr_t h) {
    reg_init();
    std::lock_guard<std::mutex> lock(g_reg_mutex);
    auto it = g_reg_handles.find(h);
    return it != g_reg_handles.end() ? it->second : nullptr;
}
std::vector<std::string> reg_split(const std::string& path) {
    std::vector<std::string> parts;
    size_t start = 0;
    for (size_t i = 0; i <= path.size(); i++) {
        if (i == path.size() || path[i] == '\\' || path[i] == '/') {
            if (i > start) parts.push_back(path.substr(start, i - start));
            start = i + 1;
        }
    }
    return parts;
}
}
static LONG reg_open_create(HKEY base, const std::string& sub, bool create, uintptr_t* out) {
    auto root = reg_lookup((uintptr_t)base);
    if (!root) return 2;
    std::lock_guard<std::mutex> lock(g_reg_mutex);
    auto node = root;
    if (!sub.empty()) {
        for (const auto& part : reg_split(sub)) {
            auto it = node->subkeys.find(part);
            if (it == node->subkeys.end()) {
                if (!create) return 2;
                auto nk = std::make_shared<RegKey>();
                nk->name = part;
                node->subkeys[part] = nk;
                node = nk;
            } else {
                node = it->second;
            }
        }
    }
    uintptr_t h = g_reg_next.fetch_add(1);
    g_reg_handles[h] = node;
    if (out) *out = h;
    return 0;
}
HANDLE WINAPI_CALL THETAZ_RegOpenKeyExW(HKEY base, LPCWSTR sub, DWORD opt, DWORD sam, PHKEY out) {
    (void)opt; (void)sam;
    std::string s = sub ? utf16le_to_utf8(reinterpret_cast<const uint16_t*>(sub)) : "";
    uintptr_t h = 0;
    LONG rc = reg_open_create(base, s, false, &h);
    guest_write_handle(out, h);
    const char* tr = getenv("THETAZ_TRACE_REG");
    if (tr && tr[0] && tr[0] != '0') {
        fprintf(stderr, "[reg] Open base=0x%08llx sub=\"%s\" rc=%ld\n",
                (unsigned long long)(uintptr_t)base, s.c_str(), (long)rc);
    }
    return (HANDLE)(uintptr_t)(uint32_t)rc;
}
LONG WINAPI_CALL THETAZ_RegCreateKeyExW(HKEY base, LPCWSTR sub, DWORD rsv, LPWSTR cls, DWORD opts,
        REGSAM sam, LPSECURITY_ATTRIBUTES sa, PHKEY out, LPDWORD disp) {
    (void)rsv; (void)cls; (void)opts; (void)sam; (void)sa;
    std::string s = sub ? utf16le_to_utf8(reinterpret_cast<const uint16_t*>(sub)) : "";
    uintptr_t h = 0;
    LONG rc = reg_open_create(base, s, true, &h);
    guest_write_handle(out, h);
    if (disp) *disp = 1;
    const char* tr = getenv("THETAZ_TRACE_REG");
    if (tr && tr[0] && tr[0] != '0') {
        fprintf(stderr, "[reg] Create base=0x%08llx sub=\"%s\" rc=%ld\n",
                (unsigned long long)(uintptr_t)base, s.c_str(), (long)rc);
    }
    return rc;
}
BOOL WINAPI_CALL THETAZ_RegCloseKey(HKEY h) {
    if (!h) return FALSE;
    uintptr_t uh = (uintptr_t)h;
    if (uh >= 0x80000000u && uh <= 0x80000005u) return TRUE;
    std::lock_guard<std::mutex> lock(g_reg_mutex);
    g_reg_handles.erase(uh);
    return TRUE;
}
LONG WINAPI_CALL THETAZ_RegSetValueExW(HKEY h, LPCWSTR vname, DWORD rsvd, DWORD type, const BYTE* data, DWORD size) {
    (void)rsvd;
    uint64_t raw_r9 = 0;
    __asm__ volatile("mov %%r9, %0" : "=r"(raw_r9));
    volatile DWORD cb = size;
    volatile const BYTE* buf = data;
    if (getenv("THETAZ_DEBUG_REG2")) {
        const BYTE* dp = const_cast<const BYTE*>(buf);
        fprintf(stderr, "[reg2] data=%p size=%u raw_r9=0x%llx b0=%02x\n",
                (void*)dp, cb, (unsigned long long)raw_r9, dp ? dp[0] : 0);
    }
    auto key = reg_lookup((uintptr_t)h);
    if (!key) return 2;
    std::string vn = (vname && vname[0]) ? utf16le_to_utf8(reinterpret_cast<const uint16_t*>(vname)) : "";
    RegValue v;
    v.type = type;
    DWORD n = cb;
    const BYTE* p = const_cast<const BYTE*>(buf);
    if (p && n) v.data.assign(p, p + n);
    std::lock_guard<std::mutex> lock(g_reg_mutex);
    key->values[vn] = std::move(v);
    return 0;
}
LONG WINAPI_CALL THETAZ_RegQueryValueExW(HKEY h, LPCWSTR vname, LPDWORD rsvd, LPDWORD type,
        LPBYTE data, LPDWORD dataSize) {
    (void)rsvd;
    auto key = reg_lookup((uintptr_t)h);
    if (!key) return 2;
    std::string vn = (vname && vname[0]) ? utf16le_to_utf8(reinterpret_cast<const uint16_t*>(vname)) : "";
    const char* tr = getenv("THETAZ_TRACE_REG");
    if (tr && tr[0] && tr[0] != '0') {
        fprintf(stderr, "[reg] Query h=0x%08llx name=\"%s\"\n",
                (unsigned long long)(uintptr_t)h, vn.c_str());
    }
    std::lock_guard<std::mutex> lock(g_reg_mutex);
    auto it = key->values.find(vn);
    if (it == key->values.end()) return 2;
    DWORD need = (DWORD)it->second.data.size();
    if (type) *type = it->second.type;
    if (!dataSize) return 0;
    if (!data) {
        *dataSize = need;
        return 0;
    }
    if (*dataSize < need) {
        *dataSize = need;
        return 234;
    }
    if (need) memcpy(data, it->second.data.data(), need);
    *dataSize = need;
    return 0;
}
LONG WINAPI_CALL THETAZ_RegDeleteValueW(HKEY h, LPCWSTR vname) {
    auto key = reg_lookup((uintptr_t)h);
    if (!key) return 2;
    std::string vn = (vname && vname[0]) ? utf16le_to_utf8(reinterpret_cast<const uint16_t*>(vname)) : "";
    std::lock_guard<std::mutex> lock(g_reg_mutex);
    auto it = key->values.find(vn);
    if (it == key->values.end()) return 2;
    key->values.erase(it);
    return 0;
}
static std::u16string ansi_to_w(const char* s) {
    std::u16string r;
    if (!s) return r;
    while (*s) r.push_back((char16_t)(unsigned char)*s++);
    return r;
}
HANDLE WINAPI_CALL THETAZ_RegOpenKeyExA(HKEY base, LPCSTR sub, DWORD opt, DWORD sam, PHKEY out) {
    std::u16string w = ansi_to_w(sub);
    return THETAZ_RegOpenKeyExW(base, (LPCWSTR)w.c_str(), opt, sam, out);
}
LONG WINAPI_CALL THETAZ_RegCreateKeyExA(HKEY base, LPCSTR sub, DWORD rsv, LPSTR cls, DWORD opts,
        REGSAM sam, LPSECURITY_ATTRIBUTES sa, PHKEY out, LPDWORD disp) {
    std::u16string w = ansi_to_w(sub);
    std::u16string wc;
    if (cls) { const char* c = (const char*)cls; while (*c) wc.push_back((char16_t)(unsigned char)*c++); }
    return THETAZ_RegCreateKeyExW(base, (LPCWSTR)w.c_str(), rsv,
        wc.empty() ? nullptr : (LPWSTR)wc.c_str(), opts, sam, sa, out, disp);
}
LONG WINAPI_CALL THETAZ_RegCreateKeyW(HKEY base, LPCWSTR sub, PHKEY out) {
    DWORD disp = 0;
    return THETAZ_RegCreateKeyExW(base, sub, 0, nullptr, 0, 0, nullptr, out, &disp);
}
LONG WINAPI_CALL THETAZ_RegQueryValueExA(HKEY h, LPCSTR vname, LPDWORD rsvd, LPDWORD type,
        LPBYTE data, LPDWORD dataSize) {
    std::u16string w = ansi_to_w(vname);
    auto key = reg_lookup((uintptr_t)h);
    if (!key) return 2;
    std::string vn;
    if (!w.empty()) { for (auto c : w) vn.push_back((char)(c & 0xFF)); }
    std::lock_guard<std::mutex> lock(g_reg_mutex);
    auto it = key->values.find(vn);
    if (it == key->values.end()) return 2;
    DWORD need = (DWORD)it->second.data.size();
    if (type) *type = it->second.type;
    if (!dataSize) return 0;
    if (!data) { *dataSize = need; return 0; }
    if (*dataSize < need) { *dataSize = need; return 234; }
    if (need) memcpy(data, it->second.data.data(), need);
    *dataSize = need;
    return 0;
}
LONG WINAPI_CALL THETAZ_RegSetValueExA(HKEY h, LPCSTR vname, DWORD rsvd, DWORD type,
        const BYTE* data, DWORD size) {
    std::u16string w = ansi_to_w(vname);
    return THETAZ_RegSetValueExW(h, (LPCWSTR)w.c_str(), rsvd, type, data, size);
}
LONG WINAPI_CALL THETAZ_RegDeleteValueA(HKEY h, LPCSTR vname) {
    std::u16string w = ansi_to_w(vname);
    return THETAZ_RegDeleteValueW(h, (LPCWSTR)w.c_str());
}
LONG WINAPI_CALL THETAZ_RegGetValueW(HKEY h, LPCWSTR sub, LPCWSTR val, DWORD flags,
        LPDWORD type, void* data, LPDWORD size) {
    (void)flags;
    uintptr_t hh = (uintptr_t)h;
    bool need_close = false;
    if (sub && sub[0]) {
        std::string s = utf16le_to_utf8(reinterpret_cast<const uint16_t*>(sub));
        uintptr_t nh = 0;
        if (reg_open_create((HKEY)hh, s, false, &nh) != 0) return 2;
        hh = nh;
        need_close = true;
    }
    LONG rc = THETAZ_RegQueryValueExW((HKEY)hh,
        (LPCWSTR)val, nullptr, type, (LPBYTE)data, size);
    if (need_close) {
        std::lock_guard<std::mutex> lock(g_reg_mutex);
        g_reg_handles.erase(hh);
    }
    return rc;
}
namespace {
struct FindState {
    DIR* dir = nullptr;
    std::string dir_path;
    std::string pattern;
};
std::mutex g_find_mutex;
std::unordered_map<uintptr_t, FindState> g_find_handles;
std::atomic<uintptr_t> g_find_next{0x40000};
bool find_match(const std::string& pat, const std::string& name) {
    size_t pi = 0, ni = 0, star = std::string::npos, mark = 0;
    auto lower = [](char c) { return (char)tolower((unsigned char)c); };
    while (ni < name.size()) {
        if (pi < pat.size() && (pat[pi] == '?' || lower(pat[pi]) == lower(name[ni]))) {
            pi++; ni++;
        } else if (pi < pat.size() && pat[pi] == '*') {
            star = pi++;
            mark = ni;
        } else if (star != std::string::npos) {
            pi = star + 1;
            ni = ++mark;
        } else {
            return false;
        }
    }
    while (pi < pat.size() && pat[pi] == '*') pi++;
    return pi == pat.size();
}
void find_fill(const std::string& dir, const std::string& name, WIN32_FIND_DATAW* out) {
    memset(out, 0, sizeof(*out));
    std::string full = dir.empty() ? name : dir + "/" + name;
    struct stat st;
    bool is_dir = false;
    uint64_t sz = 0;
    if (::stat(full.c_str(), &st) == 0) {
        is_dir = S_ISDIR(st.st_mode);
        sz = (uint64_t)st.st_size;
    }
    out->dwFileAttributes = is_dir ? 0x10u : 0x80u;
    out->nFileSizeLow = (DWORD)(sz & 0xFFFFFFFFu);
    out->nFileSizeHigh = (DWORD)(sz >> 32);
    for (size_t i = 0; i < 259 && i < name.size(); i++)
        out->cFileName[i] = (WCHAR)(unsigned char)name[i];
    out->cFileName[259] = 0;
}
}
HANDLE WINAPI_CALL THETAZ_FindFirstFileW(LPCWSTR pat, WIN32_FIND_DATAW* data) {
    if (!pat || !data) return (HANDLE)(intptr_t)-1;
    std::string full = utf16le_to_utf8(reinterpret_cast<const uint16_t*>(pat));
    if (full.empty()) return (HANDLE)(intptr_t)-1;
    std::string dir, filepat;
    size_t slash = full.find_last_of("/\\");
    if (slash == std::string::npos) { dir = "."; filepat = full; }
    else if (slash == 0) { dir = "/"; filepat = full.substr(1); }
    else { dir = full.substr(0, slash); filepat = full.substr(slash + 1); }
    if (filepat.empty()) filepat = "*";
    DIR* d = opendir(dir.c_str());
    if (!d) return (HANDLE)(intptr_t)-1;
    struct dirent* e = nullptr;
    while ((e = readdir(d)) != nullptr) {
        std::string nm = e->d_name;
        if (nm == "." || nm == "..") continue;
        if (!find_match(filepat, nm)) continue;
        find_fill(dir == "." ? "" : dir, nm, data);
        uintptr_t h = g_find_next.fetch_add(1);
        std::lock_guard<std::mutex> lock(g_find_mutex);
        FindState st;
        st.dir = d;
        st.dir_path = dir;
        st.pattern = filepat;
        g_find_handles[h] = std::move(st);
        return (HANDLE)h;
    }
    closedir(d);
    return (HANDLE)(intptr_t)-1;
}
BOOL WINAPI_CALL THETAZ_FindNextFileW(HANDLE h, WIN32_FIND_DATAW* data) {
    if (!h || !data) return FALSE;
    std::lock_guard<std::mutex> lock(g_find_mutex);
    auto it = g_find_handles.find((uintptr_t)h);
    if (it == g_find_handles.end() || !it->second.dir) return FALSE;
    struct dirent* e = nullptr;
    while ((e = readdir(it->second.dir)) != nullptr) {
        std::string nm = e->d_name;
        if (nm == "." || nm == "..") continue;
        if (!find_match(it->second.pattern, nm)) continue;
        find_fill(it->second.dir_path == "." ? "" : it->second.dir_path, nm, data);
        return TRUE;
    }
    return FALSE;
}
BOOL WINAPI_CALL THETAZ_FindClose(HANDLE h) {
    if (!h) return FALSE;
    DIR* d = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_find_mutex);
        auto it = g_find_handles.find((uintptr_t)h);
        if (it == g_find_handles.end()) return TRUE;
        d = it->second.dir;
        g_find_handles.erase(it);
    }
    if (d) closedir(d);
    return TRUE;
}
BOOL WINAPI_CALL THETAZ_CopyFileW(LPCWSTR src, LPCWSTR dst, BOOL fail_if_exists) {
    if (!src || !dst) return FALSE;
    std::string s = utf16le_to_utf8(reinterpret_cast<const uint16_t*>(src));
    std::string d = utf16le_to_utf8(reinterpret_cast<const uint16_t*>(dst));
    if (s.empty() || d.empty()) return FALSE;
    int infd = ::open(s.c_str(), O_RDONLY);
    if (infd < 0) return FALSE;
    int flags = O_WRONLY | O_CREAT | (fail_if_exists ? O_EXCL : O_TRUNC);
    int outfd = ::open(d.c_str(), flags, 0644);
    if (outfd < 0) { ::close(infd); return FALSE; }
    char buf[65536];
    ssize_t n;
    bool ok = true;
    while ((n = ::read(infd, buf, sizeof(buf))) > 0) {
        ssize_t w = 0;
        while (w < n) {
            ssize_t k = ::write(outfd, buf + w, (size_t)(n - w));
            if (k <= 0) { ok = false; break; }
            w += k;
        }
        if (!ok) break;
    }
    if (n < 0) ok = false;
    ::close(infd);
    ::close(outfd);
    return ok ? TRUE : FALSE;
}
static int wcmp(const uint16_t* a, const uint16_t* b, int n1, int n2, bool fold) {
    int i1 = 0, i2 = 0;
    auto lower = [](uint16_t c) -> uint16_t {
        if (c >= 'A' && c <= 'Z') return c + 32;
        return c;
    };
    for (;;) {
        bool e1 = (n1 >= 0) ? (i1 >= n1) : (a[i1] == 0);
        bool e2 = (n2 >= 0) ? (i2 >= n2) : (b[i2] == 0);
        if (e1 && e2) return 0;
        if (e1) return -1;
        if (e2) return 1;
        uint16_t c1 = a[i1], c2 = b[i2];
        if (fold) { c1 = lower(c1); c2 = lower(c2); }
        if (c1 < c2) return -1;
        if (c1 > c2) return 1;
        i1++; i2++;
    }
}
INT WINAPI_CALL THETAZ_lstrcmpW(LPCWSTR a, LPCWSTR b) {
    if (!a || !b) return (a == b) ? 0 : (a ? 1 : -1);
    return wcmp(reinterpret_cast<const uint16_t*>(a),
                reinterpret_cast<const uint16_t*>(b), -1, -1, false);
}
INT WINAPI_CALL THETAZ_lstrcmpiW(LPCWSTR a, LPCWSTR b) {
    if (!a || !b) return (a == b) ? 0 : (a ? 1 : -1);
    return wcmp(reinterpret_cast<const uint16_t*>(a),
                reinterpret_cast<const uint16_t*>(b), -1, -1, true);
}
INT WINAPI_CALL THETAZ_CompareStringW(DWORD loc, DWORD flags, LPCWSTR s1, INT n1,
        LPCWSTR s2, INT n2) {
    (void)loc;
    if (!s1 || !s2) return 0;
    int r = wcmp(reinterpret_cast<const uint16_t*>(s1),
                 reinterpret_cast<const uint16_t*>(s2), n1, n2,
                 (flags & 0x1) != 0);
    return r + 2;
}
INT WINAPI_CALL THETAZ_CompareStringEx(LPCWSTR locale, DWORD flags, LPCWSTR s1,
        INT n1, LPCWSTR s2, INT n2, void* version, void* reserved, void* param) {
    (void)locale; (void)version; (void)reserved; (void)param;
    return THETAZ_CompareStringW(0, flags, s1, n1, s2, n2);
}
INT WINAPI_CALL THETAZ_MulDiv(INT number, INT numerator, INT denominator) {
    if (denominator == 0) return -1;
    long long product = static_cast<long long>(number) * static_cast<long long>(numerator);
    long long den = static_cast<long long>(denominator);
    long long q = product / den;
    long long r = product % den;
    if (r != 0 && (r < 0) != (den < 0)) {
        long long ar = r < 0 ? -r : r;
        long long ad = den < 0 ? -den : den;
        if (2 * ar >= ad) q += (product < 0) ? -1 : 1;
    }
    if (q > 2147483647LL || q < -2147483648LL) return -1;
    return static_cast<INT>(q);
}
static const wchar_t* locale_string(DWORD type) {
    switch (type) {
    case 2: return L"English";
    case 6: return L"United States";
    case 14: return L".";
    case 15: return L",";
    case 29: return L"/";
    case 30: return L":";
    case 31: return L"M/d/yyyy";
    case 32: return L"h:mm:ss tt";
    default: return nullptr;
    }
}
INT WINAPI_CALL THETAZ_GetLocaleInfoW(DWORD loc, DWORD type, LPWSTR buf, INT size) {
    (void)loc;
    const wchar_t* s = locale_string(type);
    if (!s) return 0;
    int len = 0;
    while (s[len]) len++;
    len++;
    if (!buf || size == 0) return len;
    if (size < len) return 0;
    uint16_t* out = reinterpret_cast<uint16_t*>(buf);
    for (int i = 0; i < len; i++) out[i] = (uint16_t)s[i];
    return len - 1;
}
static int fmt_num(uint16_t* out, int v, int digits) {
    char tmp[8];
    snprintf(tmp, sizeof(tmp), "%02d", v);
    int n = 0;
    if (digits == 4) {
        snprintf(tmp, sizeof(tmp), "%04d", v);
        for (int i = 0; i < 4; i++) out[n++] = (uint16_t)tmp[i];
    } else {
        const char* src = tmp;
        if (digits == 1 && tmp[0] == '0') src++;
        while (*src) out[n++] = (uint16_t)*src++;
    }
    return n;
}
INT WINAPI_CALL THETAZ_GetDateFormatW(DWORD loc, DWORD flags, const SYSTEMTIME* dt,
        LPCWSTR fmt, LPWSTR buf, INT size) {
    (void)loc; (void)flags;
    SYSTEMTIME now = {2026, 9, 4, 10, 6, 0, 0, 0};
    if (dt) now = *dt;
    uint16_t tmp[64];
    int n = 0;
    if (!fmt || !fmt[0]) {
        n += fmt_num(tmp + n, now.wMonth, 1);
        tmp[n++] = '/';
        n += fmt_num(tmp + n, now.wDay, 1);
        tmp[n++] = '/';
        n += fmt_num(tmp + n, now.wYear, 4);
    } else {
        const uint16_t* f = reinterpret_cast<const uint16_t*>(fmt);
        for (int i = 0; f[i] && n < 60; ) {
            if (f[i] == 'y' || f[i] == 'Y') {
                int k = 0;
                while (f[i] == 'y' || f[i] == 'Y') { k++; i++; }
                n += fmt_num(tmp + n, now.wYear, k >= 4 ? 4 : 2);
            } else if (f[i] == 'M') {
                int k = 0;
                while (f[i] == 'M') { k++; i++; }
                n += fmt_num(tmp + n, now.wMonth, k >= 2 ? 2 : 1);
            } else if (f[i] == 'd') {
                int k = 0;
                while (f[i] == 'd') { k++; i++; }
                n += fmt_num(tmp + n, now.wDay, k >= 2 ? 2 : 1);
            } else {
                tmp[n++] = f[i++];
            }
        }
    }
    tmp[n++] = 0;
    if (!buf || size == 0) return n;
    if (size < n) return 0;
    uint16_t* out = reinterpret_cast<uint16_t*>(buf);
    for (int i = 0; i < n; i++) out[i] = tmp[i];
    return n - 1;
}
INT WINAPI_CALL THETAZ_GetTimeFormatW(DWORD loc, DWORD flags, const SYSTEMTIME* tm,
        LPCWSTR fmt, LPWSTR buf, INT size) {
    (void)loc; (void)flags; (void)fmt;
    SYSTEMTIME now = {2026, 9, 4, 10, 6, 30, 0, 0};
    if (tm) now = *tm;
    uint16_t tmp[64];
    int n = 0;
    n += fmt_num(tmp + n, now.wHour, 1);
    tmp[n++] = ':';
    n += fmt_num(tmp + n, now.wMinute, 2);
    tmp[n++] = ':';
    n += fmt_num(tmp + n, now.wSecond, 2);
    tmp[n++] = 0;
    if (!buf || size == 0) return n;
    if (size < n) return 0;
    uint16_t* out = reinterpret_cast<uint16_t*>(buf);
    for (int i = 0; i < n; i++) out[i] = tmp[i];
    return n - 1;
}
void* WINAPI_CALL THETAZ_ResolveDelayLoadedAPI(void* parent_base, void* descriptor,
        void* failure_dll_hook, void* failure_sys_hook, void* thunk_addr, ULONG flags) {
    (void)failure_dll_hook; (void)failure_sys_hook; (void)flags;
    if (!parent_base || !descriptor || !thunk_addr) return nullptr;
    uint8_t* desc = (uint8_t*)descriptor;
    uint64_t pbase = (uint64_t)(uintptr_t)parent_base;
    uint32_t name_rva = *reinterpret_cast<uint32_t*>(desc + 4);
    uint32_t iat_rva = *reinterpret_cast<uint32_t*>(desc + 12);
    uint32_t int_rva = *reinterpret_cast<uint32_t*>(desc + 16);
    if (!name_rva || !int_rva) return nullptr;
    const char* dll = (const char*)(uintptr_t)(pbase + name_rva);
    if (!dll || !dll[0]) return nullptr;
    PELoader& loader = PELoader::instance();
    uint32_t thunk_u = (uint32_t)(uintptr_t)thunk_addr;
    uint32_t iat_base = (uint32_t)(pbase + iat_rva);
    uint32_t int_base = (uint32_t)(pbase + int_rva);
    if (thunk_u < iat_base) return nullptr;
    uint32_t index = (thunk_u - iat_base) / 4;
    uint32_t int_entry = *reinterpret_cast<uint32_t*>(int_base + index * 4);
    void* fn = nullptr;
    LoadedModule* mod = loader.find_module_named(dll);
    if (!mod) {
        void* base = load_guest_module(dll);
        if (base) mod = loader.find_module_by_base((uint64_t)(uintptr_t)base);
    }
    if (!mod) return nullptr;
    if (int_entry & 0x80000000u) {
        fn = loader.get_export_address(*mod, (uint16_t)(int_entry & 0xFFFF), nullptr);
    } else {
        uint32_t hint_name = (uint32_t)(pbase + (int_entry & 0x7FFFFFFFu));
        const char* fname = (const char*)(uintptr_t)(hint_name + 2);
        if (fname && fname[0]) fn = loader.get_export_address(*mod, 0, fname);
    }
    if (!fn) return nullptr;
    *reinterpret_cast<uint32_t*>(thunk_u) = (uint32_t)(uintptr_t)fn;
    return fn;
}
DWORD WINAPI_CALL THETAZ_GetEnvironmentVariableW(LPCWSTR name, LPWSTR buf, DWORD size) {
    if (!name || !name[0]) return 0;
    std::string aname = utf16le_to_utf8(reinterpret_cast<const uint16_t*>(name));
    if (aname.empty()) return 0;
    const char* val = getenv(aname.c_str());
    if (!val) {
        THETAZ_SetLastError(203);
        return 0;
    }
    std::u16string wval;
    utf8_to_utf16le(val, wval);
    DWORD need = (DWORD)wval.size() + 1;
    if (!buf || size == 0) return need;
    if (size < need) return need;
    uint16_t* out = reinterpret_cast<uint16_t*>(buf);
    for (size_t i = 0; i < wval.size(); i++) out[i] = (uint16_t)wval[i];
    out[wval.size()] = 0;
    return (DWORD)wval.size();
}
BOOL WINAPI_CALL THETAZ_IsBadStringPtrW(LPCWSTR s, UINT_PTR max) {
    if (!s) return TRUE;
    uintptr_t addr = (uintptr_t)s;
    if (addr >= 0x100000000ULL && guest_is_32bit()) return TRUE;
    size_t bytes = (size_t)max * 2;
    if (bytes == 0) return FALSE;
    const uint16_t* ws = reinterpret_cast<const uint16_t*>(addr);
    size_t checked = 0;
    while (checked < max) {
        uintptr_t page = (addr + checked * 2) & ~0xFFFULL;
        if (!maps_range_readable(page, 0x1000)) return TRUE;
        for (; checked < max; checked++) {
            if (((addr + checked * 2) & 0xFFF) == 0 && checked > 0) break;
            if (ws[checked] == 0) return FALSE;
        }
    }
    return maps_range_readable(addr, bytes) ? FALSE : TRUE;
}
namespace {
std::mutex g_srw_map_mutex;
std::unordered_map<void*, std::shared_ptr<std::shared_mutex>> g_srw_locks;
std::shared_ptr<std::shared_mutex> srw_for(void* p) {
    std::lock_guard<std::mutex> lock(g_srw_map_mutex);
    auto it = g_srw_locks.find(p);
    if (it != g_srw_locks.end()) return it->second;
    auto m = std::make_shared<std::shared_mutex>();
    g_srw_locks[p] = m;
    return m;
}
struct GuestCondVar {
    std::mutex mutex;
    std::condition_variable cv;
};
std::mutex g_cv_map_mutex;
std::unordered_map<void*, std::shared_ptr<GuestCondVar>> g_condvars;
std::shared_ptr<GuestCondVar> cv_for(void* p) {
    std::lock_guard<std::mutex> lock(g_cv_map_mutex);
    auto it = g_condvars.find(p);
    if (it != g_condvars.end()) return it->second;
    auto c = std::make_shared<GuestCondVar>();
    g_condvars[p] = c;
    return c;
}
}
void WINAPI_CALL THETAZ_InitializeSRWLock(void* p) {
    if (!p) return;
    guest_write_handle(p, 0);
    srw_for(p);
}
void WINAPI_CALL THETAZ_AcquireSRWLockExclusive(void* p) {
    if (!p) return;
    srw_for(p)->lock();
}
void WINAPI_CALL THETAZ_ReleaseSRWLockExclusive(void* p) {
    if (!p) return;
    srw_for(p)->unlock();
}
void WINAPI_CALL THETAZ_AcquireSRWLockShared(void* p) {
    if (!p) return;
    srw_for(p)->lock_shared();
}
void WINAPI_CALL THETAZ_ReleaseSRWLockShared(void* p) {
    if (!p) return;
    srw_for(p)->unlock_shared();
}
void WINAPI_CALL THETAZ_InitializeConditionVariable(void* p) {
    if (!p) return;
    guest_write_handle(p, 0);
    cv_for(p);
}
void WINAPI_CALL THETAZ_WakeConditionVariable(void* p) {
    if (!p) return;
    auto c = cv_for(p);
    std::lock_guard<std::mutex> lock(c->mutex);
    c->cv.notify_one();
}
void WINAPI_CALL THETAZ_WakeAllConditionVariable(void* p) {
    if (!p) return;
    auto c = cv_for(p);
    std::lock_guard<std::mutex> lock(c->mutex);
    c->cv.notify_all();
}
BOOL WINAPI_CALL THETAZ_TryAcquireSRWLockExclusive(void* p) {
    if (!p) return FALSE;
    return srw_for(p)->try_lock() ? TRUE : FALSE;
}
BOOL WINAPI_CALL THETAZ_TryAcquireSRWLockShared(void* p) {
    if (!p) return FALSE;
    return srw_for(p)->try_lock_shared() ? TRUE : FALSE;
}
BOOL WINAPI_CALL THETAZ_SleepConditionVariableSRW(void* cv, void* srw, DWORD ms, ULONG flags) {
    if (!cv || !srw) return FALSE;
    bool shared = (flags & 0x1) != 0;
    auto c = cv_for(cv);
    auto m = srw_for(srw);
    if (shared) m->unlock_shared();
    else m->unlock();
    bool woke = false;
    {
        std::unique_lock<std::mutex> lock(c->mutex);
        if (ms == 0xFFFFFFFF) {
            c->cv.wait(lock);
            woke = true;
        } else if (ms == 0) {
            woke = false;
        } else {
            woke = c->cv.wait_for(lock, std::chrono::milliseconds(ms)) == std::cv_status::no_timeout;
        }
    }
    if (shared) m->lock_shared();
    else m->lock();
    return woke ? TRUE : FALSE;
}
void* WINAPI_CALL THETAZ_GetEnvironmentStringsW() {
    static void* cached = nullptr;
    if (cached) return cached;
    uint16_t* block = (uint16_t*)alloc_guest_shared(4);
    if (!block) return nullptr;
    block[0] = 0;
    block[1] = 0;
    cached = block;
    return cached;
}
BOOL WINAPI_CALL THETAZ_FreeEnvironmentStringsW(LPWSTR p) {
    (void)p;
    return TRUE;
}
void* WINAPI_CALL THETAZ_SetUnhandledExceptionFilter(void* filter) {
    void* prev = get_unhandled_exception_filter();
    set_unhandled_exception_filter(filter);
    return prev;
}
BOOL WINAPI_CALL THETAZ_DeleteObject(void* hobj) {
    uintptr_t h = (uintptr_t)hobj;
    if (h >= 0x8001 && h <= 0x8006) return FALSE;
    return TRUE;
}
BOOL WINAPI_CALL THETAZ_TextOutA(void* hdc, int x, int y, const char* s, int c) {
    (void)hdc; (void)x; (void)y;
    if (!s || c <= 0) return FALSE;
    std::string text(s, (size_t)c);
    printf("%s\n", text.c_str());
    fflush(stdout);
    return TRUE;
}
namespace {
std::mutex g_file_mutex;
std::unordered_map<uintptr_t, int> g_file_fds;
std::atomic<uintptr_t> g_file_next{0x20000};

// Pseudo standard handles returned by GetStdHandle map to the host's fds.
static int fd_for_handle(uintptr_t h) {
    if (h == 0x90000001u) return 0;
    if (h == 0x90000002u) return 1;
    if (h == 0x90000003u) return 2;
    std::lock_guard<std::mutex> lock(g_file_mutex);
    auto it = g_file_fds.find(h);
    return it == g_file_fds.end() ? -1 : it->second;
}
static uintptr_t register_fd(int fd) {
    uintptr_t h = g_file_next.fetch_add(1);
    std::lock_guard<std::mutex> lock(g_file_mutex);
    g_file_fds[h] = fd;
    return h;
}
static bool unregister_fd(uintptr_t h, int* fd_out) {
    std::lock_guard<std::mutex> lock(g_file_mutex);
    auto it = g_file_fds.find(h);
    if (it == g_file_fds.end()) return false;
    if (fd_out) *fd_out = it->second;
    g_file_fds.erase(it);
    return true;
}
}
namespace {
// Convert a guest UTF-16 string to UTF-8, including surrogate pairs. The
// legacy utf16le_to_utf8 helper truncates to the low byte, which corrupts
// non-ASCII paths (user profiles, temporary directories, ...).
static std::string utf16_path_to_utf8(const uint16_t* s) {
    std::string out;
    if (!s) return out;
    for (size_t i = 0; s[i]; ++i) {
        uint32_t cp = s[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && s[i + 1] >= 0xDC00 && s[i + 1] <= 0xDFFF) {
            cp = 0x10000u + ((cp - 0xD800u) << 10) + (s[i + 1] - 0xDC00u);
            ++i;
        } else if (cp >= 0xD800 && cp <= 0xDFFF) {
            cp = 0xFFFDu;
        }
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

static DWORD win32_error_from_errno(int err) {
    switch (err) {
    case 0: return ERROR_SUCCESS;
    case ENOENT: return ERROR_FILE_NOT_FOUND;
    case ENOTDIR: return ERROR_PATH_NOT_FOUND;
    case EACCES:
    case EPERM:
    case EROFS:
    case EISDIR: return ERROR_ACCESS_DENIED;
    case EEXIST: return ERROR_FILE_EXISTS;
    case EMFILE:
    case ENFILE: return ERROR_TOO_MANY_OPEN_FILES;
    case ENOSPC: return ERROR_DISK_FULL;
    case ENAMETOOLONG: return ERROR_FILENAME_EXCED_RANGE;
    case EINVAL: return ERROR_INVALID_PARAMETER;
    case EBUSY: return 32; // ERROR_SHARING_VIOLATION
    default: return ERROR_ACCESS_DENIED;
    }
}

static std::string guest_file_path_to_host(const std::string& raw) {
    if (raw.empty()) return {};
    // ThetaZ-internal callers occasionally pass already-translated host paths.
    if (raw[0] == '/') return raw;

    std::string path = raw;
    // \\?\C:\... is an extended-length path; strip the marker before
    // normalizing. \\?\UNC\server\share stays a UNC path and is handled below.
    if (path.compare(0, 4, "\\\\?\\") == 0) {
        path = path.substr(4);
        if (path.compare(0, 4, "UNC\\") == 0) path = "\\\\" + path.substr(4);
    }

    // \\.\NAME device namespace. The common character pseudo-files map to
    // their Unix equivalents; other device names cannot be emulated safely.
    std::string device;
    if (path.compare(0, 4, "\\\\.\\") == 0 || path.compare(0, 4, "//./") == 0) {
        device = path.substr(4);
        for (char& c : device) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        if (device == "nul") return "/dev/null";
        if (device == "con" || device == "conin$" || device == "conout$") return "/dev/tty";
        if (device == "aux" || device == "prn" || device.compare(0, 3, "com") == 0 ||
            device.compare(0, 3, "lpt") == 0) {
            return "/dev/null";
        }
        path = device;
    }

    // Treat bare reserved device names the same way Windows does, regardless
    // of a directory prefix or a trailing extension.
    std::string base = path;
    size_t slash = base.find_last_of("/\\");
    if (slash != std::string::npos) base = base.substr(slash + 1);
    size_t dot = base.find('.');
    if (dot != std::string::npos) base.resize(dot);
    for (char& c : base) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    if (base == "nul") return "/dev/null";
    if (base == "con" || base == "conin$" || base == "conout$") return "/dev/tty";
    if (base == "aux" || base == "prn") return "/dev/null";

    std::string normalized = guest_normalize_path(path);
    std::string host = get_path_resolver().translate_windows_to_linux(normalized);
    return host;
}

static HANDLE open_guest_file(const std::string& guest_name, DWORD access, DWORD share,
                              DWORD disp, DWORD flags, bool wide) {
    (void)share;
    if (guest_name.empty()) {
        THETAZ_SetLastError(ERROR_PATH_NOT_FOUND);
        return (HANDLE)(intptr_t)-1;
    }

    std::string host = guest_file_path_to_host(guest_name);
    const bool trace = getenv("THETAZ_TRACE_FS") != nullptr;
    auto trace_result = [&](const char* result, int err, int fd) {
        if (!trace) return;
        fprintf(stderr, "[fs] CreateFile%s(\"%s\") -> host=\"%s\" access=0x%08x disp=%u flags=0x%08x : %s",
                wide ? "W" : "A", guest_name.c_str(), host.c_str(),
                access, disp, flags, result);
        if (err) fprintf(stderr, " errno=%d (%s)", err, strerror(err));
        if (fd >= 0) fprintf(stderr, " fd=%d", fd);
        fprintf(stderr, "\n");
    };

    if (host.empty()) {
        THETAZ_SetLastError(ERROR_PATH_NOT_FOUND);
        trace_result("invalid path", 0, -1);
        return (HANDLE)(intptr_t)-1;
    }

    // Opening a directory is only legal with FILE_FLAG_BACKUP_SEMANTICS.
    struct stat st{};
    bool exists = (::stat(host.c_str(), &st) == 0);
    bool is_dir = exists && S_ISDIR(st.st_mode);
    if (is_dir && !(flags & 0x02000000u)) {
        THETAZ_SetLastError(ERROR_ACCESS_DENIED);
        trace_result("directory without FILE_FLAG_BACKUP_SEMANTICS", 0, -1);
        return (HANDLE)(intptr_t)-1;
    }

    // Translate the Win32 access mask. GENERIC_READ/WRITE/ALL, plus the
    // specific data/attribute rights used by importers and installers.
    const bool generic_all = (access & 0x10000000u) != 0;
    bool want_write = generic_all ||
        (access & (0x40000000u | 0x00000002u | 0x00000004u | 0x00000100u | 0x00010000u | 0x00040000u)) != 0;
    bool want_read = generic_all ||
        (access & (0x80000000u | 0x00000001u | 0x00000008u | 0x00000080u | 0x00020000u | 0x00020000u)) != 0;
    const bool append_only = (access & 0x00000004u) != 0 && !(access & 0x00000002u) && !generic_all;

    int oflags = 0;
    if (want_read && want_write) oflags |= O_RDWR;
    else if (want_write) oflags |= O_WRONLY;
    else oflags |= O_RDONLY;

    DWORD already_exists_error = ERROR_SUCCESS;
    switch (disp) {
    case 1: // CREATE_NEW
        oflags |= O_CREAT | O_EXCL;
        break;
    case 2: // CREATE_ALWAYS
        oflags |= O_CREAT | O_TRUNC;
        if (exists) already_exists_error = ERROR_ALREADY_EXISTS;
        break;
    case 3: // OPEN_EXISTING
        break;
    case 4: // OPEN_ALWAYS
        oflags |= O_CREAT;
        if (exists) already_exists_error = ERROR_ALREADY_EXISTS;
        break;
    case 5: // TRUNCATE_EXISTING
        oflags |= O_TRUNC;
        if (!want_write) oflags = (oflags & ~O_WRONLY) | O_RDWR;
        break;
    default:
        THETAZ_SetLastError(ERROR_INVALID_PARAMETER);
        trace_result("invalid disposition", 0, -1);
        return (HANDLE)(intptr_t)-1;
    }
    if (append_only) oflags |= O_APPEND;

    int fd = ::open(host.c_str(), oflags, 0644);
    if (fd < 0) {
        int saved = errno;
        DWORD err = win32_error_from_errno(saved);
        if (disp == 1 && saved == EEXIST) err = ERROR_FILE_EXISTS;
        THETAZ_SetLastError(err);
        trace_result("FAILED", saved, -1);
        return (HANDLE)(intptr_t)-1;
    }

    // FILE_FLAG_DELETE_ON_CLOSE is naturally represented by unlinking now:
    // the open descriptor keeps the file alive until the last close.
    if (flags & 0x04000000u) ::unlink(host.c_str());

    if (already_exists_error != ERROR_SUCCESS) THETAZ_SetLastError(already_exists_error);
    else THETAZ_SetLastError(ERROR_SUCCESS);
    trace_result("ok", 0, fd);
    return (HANDLE)register_fd(fd);
}
} // namespace

HANDLE WINAPI_CALL THETAZ_CreateFileA(LPCSTR name, DWORD access, DWORD share,
                                     LPSECURITY_ATTRIBUTES sa, DWORD disp,
                                     DWORD flags, HANDLE tmpl) {
    (void)sa; (void)tmpl;
    if (!name || !name[0]) {
        THETAZ_SetLastError(ERROR_PATH_NOT_FOUND);
        return (HANDLE)(intptr_t)-1;
    }
    return open_guest_file(name, access, share, disp, flags, false);
}

HANDLE WINAPI_CALL THETAZ_CreateFileW(LPCWSTR name, DWORD access, DWORD share,
                                     LPSECURITY_ATTRIBUTES sa, DWORD disp,
                                     DWORD flags, HANDLE tmpl) {
    (void)sa; (void)tmpl;
    if (!name) {
        THETAZ_SetLastError(ERROR_PATH_NOT_FOUND);
        return (HANDLE)(intptr_t)-1;
    }
    std::string guest_name = utf16_path_to_utf8(
        reinterpret_cast<const uint16_t*>(name));
    return open_guest_file(guest_name, access, share, disp, flags, true);
}

DWORD WINAPI_CALL THETAZ_SetFilePointer(HANDLE h, LONG l, LONG* hi, DWORD m) {
    int fd = fd_for_handle((uintptr_t)h);
    if (fd < 0) {
        THETAZ_SetLastError(ERROR_INVALID_HANDLE);
        return 0xFFFFFFFFu; // INVALID_SET_FILE_POINTER
    }
    int64_t distance = static_cast<uint32_t>(l);
    if (hi) {
        distance |= static_cast<int64_t>(static_cast<int32_t>(
            static_cast<uint32_t>(*reinterpret_cast<uint32_t*>(
                reinterpret_cast<uintptr_t>(hi))))) << 32;
    }
    int whence = SEEK_SET;
    if (m == 1) whence = SEEK_CUR;
    else if (m == 2) whence = SEEK_END;
    else if (m != 0) {
        THETAZ_SetLastError(ERROR_INVALID_PARAMETER);
        return 0xFFFFFFFFu;
    }
    off_t pos = ::lseek(fd, static_cast<off_t>(distance), whence);
    if (pos == static_cast<off_t>(-1)) {
        THETAZ_SetLastError(win32_error_from_errno(errno));
        return 0xFFFFFFFFu;
    }
    if (hi) {
        *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(hi)) =
            static_cast<uint32_t>(static_cast<uint64_t>(pos) >> 32);
    }
    THETAZ_SetLastError(ERROR_SUCCESS);
    return static_cast<DWORD>(static_cast<uint64_t>(pos) & 0xFFFFFFFFu);
}

BOOL WINAPI_CALL THETAZ_SetFilePointerEx(HANDLE h, LARGE_INTEGER distance,
                                         LARGE_INTEGER* new_pos, DWORD m) {
    int fd = fd_for_handle((uintptr_t)h);
    if (fd < 0) {
        THETAZ_SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    int whence = SEEK_SET;
    if (m == 1) whence = SEEK_CUR;
    else if (m == 2) whence = SEEK_END;
    else if (m != 0) {
        THETAZ_SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    off_t pos = ::lseek(fd, static_cast<off_t>(distance.QuadPart), whence);
    if (pos == static_cast<off_t>(-1)) {
        THETAZ_SetLastError(win32_error_from_errno(errno));
        return FALSE;
    }
    if (new_pos) {
        *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(new_pos)) =
            static_cast<int64_t>(pos);
    }
    THETAZ_SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL WINAPI_CALL THETAZ_GetFileSizeEx(HANDLE h, void* size_out) {
    int fd = fd_for_handle((uintptr_t)h);
    if (fd < 0) {
        THETAZ_SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    struct stat st{};
    if (::fstat(fd, &st) != 0) {
        THETAZ_SetLastError(win32_error_from_errno(errno));
        return FALSE;
    }
    if (size_out) {
        *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(size_out)) =
            static_cast<int64_t>(st.st_size);
    }
    THETAZ_SetLastError(ERROR_SUCCESS);
    return TRUE;
}

DWORD WINAPI_CALL THETAZ_GetFileSize(HANDLE h, LPDWORD size_high) {
    int fd = fd_for_handle((uintptr_t)h);
    if (fd < 0) {
        THETAZ_SetLastError(ERROR_INVALID_HANDLE);
        return 0xFFFFFFFFu;
    }
    struct stat st{};
    if (::fstat(fd, &st) != 0) {
        THETAZ_SetLastError(win32_error_from_errno(errno));
        return 0xFFFFFFFFu;
    }
    uint64_t size = static_cast<uint64_t>(st.st_size);
    if (!size_high && (size >> 32) != 0) {
        THETAZ_SetLastError(ERROR_INVALID_PARAMETER);
        return 0xFFFFFFFFu;
    }
    if (size_high) {
        *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(size_high)) =
            static_cast<uint32_t>(size >> 32);
    }
    THETAZ_SetLastError(ERROR_SUCCESS);
    return static_cast<DWORD>(size & 0xFFFFFFFFu);
}

BOOL WINAPI_CALL THETAZ_FlushFileBuffers(HANDLE h) {
    int fd = fd_for_handle((uintptr_t)h);
    if (fd < 0) {
        THETAZ_SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    if (::fsync(fd) != 0) {
        THETAZ_SetLastError(win32_error_from_errno(errno));
        return FALSE;
    }
    THETAZ_SetLastError(ERROR_SUCCESS);
    return TRUE;
}
BOOL WINAPI_CALL THETAZ_ReadFile(HANDLE h, LPVOID b, DWORD n, LPDWORD r, LPOVERLAPPED o) {
    (void)o;
    int fd = fd_for_handle((uintptr_t)h);
    if (fd < 0) { THETAZ_SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    ssize_t rc = b && n ? ::read(fd, b, (size_t)n) : 0;
    if (r) *(reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(r))) = rc > 0 ? (DWORD)rc : 0;
    return rc >= 0 ? TRUE : FALSE;
}
BOOL WINAPI_CALL THETAZ_WriteFile(HANDLE h, LPCVOID buf, DWORD n, LPDWORD written, LPOVERLAPPED ov) {
    (void)ov;
    int fd = fd_for_handle((uintptr_t)h);
    if (fd < 0) { THETAZ_SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    ssize_t rc = buf && n ? ::write(fd, buf, (size_t)n) : 0;
    if (written) *(reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(written))) =
        rc >= 0 ? (DWORD)rc : 0;
    return rc >= 0 ? TRUE : FALSE;
}
BOOL WINAPI_CALL THETAZ_CreatePipe(HANDLE* hReadPipe, HANDLE* hWritePipe,
                                   LPSECURITY_ATTRIBUTES sa, DWORD nSize) {
    (void)sa; (void)nSize;
    if (!hReadPipe || !hWritePipe) {
        THETAZ_SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    int fds[2];
    if (::pipe(fds) != 0) return FALSE;
    HANDLE rd = (HANDLE)register_fd(fds[0]);
    HANDLE wr = (HANDLE)register_fd(fds[1]);
    // Output parameters are guest 32-bit HANDLE slots; write exactly 4 bytes.
    *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(hReadPipe)) = (uint32_t)(uintptr_t)rd;
    *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(hWritePipe)) = (uint32_t)(uintptr_t)wr;
    return TRUE;
}
BOOL WINAPI_CALL THETAZ_PeekNamedPipe(HANDLE h, LPVOID buf, DWORD bufSize, LPDWORD bytesRead,
                                      LPDWORD totalAvail, LPDWORD bytesLeft) {
    int fd = fd_for_handle((uintptr_t)h);
    if (fd < 0) { THETAZ_SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    if (bytesRead) *(reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(bytesRead))) = 0;
    if (bytesLeft) *(reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(bytesLeft))) = 0;
    int avail = 0;
    if (::ioctl(fd, FIONREAD, &avail) != 0) return FALSE;
    if (totalAvail) *(reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(totalAvail))) = (uint32_t)avail;
    // Linux cannot peek a pipe without consuming; report availability only.
    (void)buf; (void)bufSize;
    return TRUE;
}
DWORD WINAPI_CALL THETAZ_GetFileType(HANDLE h) {
    int fd = fd_for_handle((uintptr_t)h);
    if (fd < 0) { THETAZ_SetLastError(ERROR_INVALID_HANDLE); return 0; } // FILE_TYPE_UNKNOWN
    if (fd <= 2 && ::isatty(fd)) return 2; // FILE_TYPE_CHAR
    struct stat st{};
    if (::fstat(fd, &st) == 0) {
        if (S_ISFIFO(st.st_mode) || S_ISSOCK(st.st_mode)) return 3; // FILE_TYPE_PIPE
        if (S_ISCHR(st.st_mode)) return 2;
        if (S_ISREG(st.st_mode)) return 1;
    }
    return 0;
}
BOOL WINAPI_CALL THETAZ_GetConsoleMode(HANDLE h, LPDWORD lpMode) {
    int fd = fd_for_handle((uintptr_t)h);
    if (fd < 0 || fd > 2 || !::isatty(fd)) {
        THETAZ_SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    if (lpMode) {
        *(reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(lpMode))) =
            (fd == 0) ? 0x0007u /* ENABLE_ECHO|LINE|PROCESSED input */
                      : 0x0003u /* ENABLE_PROCESSED_OUTPUT|WRAP_AT_EOL */;
    }
    return TRUE;
}
BOOL WINAPI_CALL THETAZ_CloseFileHandle(HANDLE h) {
    if (!h || h == (HANDLE)(intptr_t)-1) return FALSE;
    uintptr_t hv = (uintptr_t)h;
    if (hv == 0x90000001u || hv == 0x90000002u || hv == 0x90000003u) return TRUE;
    int fd = -1;
    if (unregister_fd(hv, &fd) && fd >= 0) ::close(fd);
    // The handle may also name a ThetaServer object (event, mutex, thread...).
    uint64_t pid = ThreadManager::instance().get_current_guest_pid();
    ThetaServer::instance().close_handle(pid, (uint64_t)hv);
    return TRUE;
}
HANDLE WINAPI_CALL THETAZ_OpenEventW(DWORD a, BOOL b, LPCWSTR c){ (void)a;(void)b;(void)c; return (HANDLE)0x1; }
BOOL WINAPI_CALL THETAZ_SetEvent(HANDLE h){
    if (!h) return FALSE;
    return ThetaServer::instance().set_event((uint64_t)(uintptr_t)h) ? TRUE : FALSE;
}
BOOL WINAPI_CALL THETAZ_ResetEvent(HANDLE h){
    if (!h) return FALSE;
    return ThetaServer::instance().reset_event((uint64_t)(uintptr_t)h) ? TRUE : FALSE;
}
BOOL WINAPI_CALL THETAZ_PulseEvent(HANDLE h){ (void)h; return TRUE; }
DWORD WINAPI_CALL THETAZ_WaitForSingleObject(HANDLE h, DWORD t){
    if (!h) return 0xFFFFFFFF;
    uint64_t timeout = (t == 0xFFFFFFFF) ? 0xFFFFFFFF : t;
    return ThetaServer::instance().wait_for_object((uint64_t)(uintptr_t)h, timeout);
}
DWORD WINAPI_CALL THETAZ_WaitForMultipleObjects(DWORD n, const HANDLE* h, BOOL b, DWORD t){ (void)n;(void)h;(void)b;(void)t; return 0; }
HANDLE WINAPI_CALL THETAZ_CreateMutexW(LPSECURITY_ATTRIBUTES a, BOOL b, LPCWSTR c){ (void)a;(void)b;(void)c; return (HANDLE)0x2; }
HANDLE WINAPI_CALL THETAZ_OpenMutexW(DWORD a, BOOL b, LPCWSTR c){ (void)a;(void)b;(void)c; return (HANDLE)0x3; }

DWORD WINAPI_CALL THETAZ_GetTickCount(){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (DWORD)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
ULONGLONG WINAPI_CALL THETAZ_GetTickCount64(){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ULONGLONG)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}


// =========================================================================
// Missing threadpool/heap/GetProcAddress implementations
// =========================================================================
namespace {
std::atomic<DWORD> g_tls_next{0};
thread_local std::unordered_map<DWORD, LPVOID> t_tls_values;
}
DWORD WINAPI_CALL THETAZ_TlsAlloc() {
    return g_tls_next.fetch_add(1);
}
BOOL WINAPI_CALL THETAZ_TlsFree(DWORD idx) {
    (void)idx;
    return TRUE;
}
BOOL WINAPI_CALL THETAZ_TlsSetValue(DWORD idx, LPVOID val) {
    t_tls_values[idx] = val;
    return TRUE;
}
LPVOID WINAPI_CALL THETAZ_TlsGetValue(DWORD idx) {
    auto it = t_tls_values.find(idx);
    return it != t_tls_values.end() ? it->second : nullptr;
}

// Fiber-Local Storage (UCRT per-thread state)
namespace {
std::atomic<DWORD> g_fls_next{0};
std::mutex g_fls_mutex;
std::unordered_map<DWORD, void*> g_fls_callbacks;
thread_local std::unordered_map<DWORD, void*> t_fls_values;
}
DWORD WINAPI_CALL THETAZ_FlsAlloc(void* cb) {
    DWORD idx = g_fls_next.fetch_add(1);
    if (cb) {
        std::lock_guard<std::mutex> lock(g_fls_mutex);
        g_fls_callbacks[idx] = cb;
    }
    return idx;
}
BOOL WINAPI_CALL THETAZ_FlsFree(DWORD idx) {
    {
        std::lock_guard<std::mutex> lock(g_fls_mutex);
        g_fls_callbacks.erase(idx);
    }
    t_fls_values.erase(idx);
    return TRUE;
}
LPVOID WINAPI_CALL THETAZ_FlsGetValue(DWORD idx) {
    auto it = t_fls_values.find(idx);
    return it != t_fls_values.end() ? it->second : nullptr;
}
LPVOID WINAPI_CALL THETAZ_FlsGetValue2(DWORD idx) {
    auto it = t_fls_values.find(idx);
    return it != t_fls_values.end() ? it->second : nullptr;
}
BOOL WINAPI_CALL THETAZ_FlsSetValue(DWORD idx, LPVOID v) {
    if (v) t_fls_values[idx] = v;
    else t_fls_values.erase(idx);
    return TRUE;
}

// Time / FILETIME
static constexpr uint64_t kEpoch1601Offset = 116444736000000000ULL;
void WINAPI_CALL THETAZ_GetSystemTimeAsFileTime(void* out) {
    if (!out) return;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t ft = (uint64_t)ts.tv_sec * 10000000ULL + (uint64_t)ts.tv_nsec / 100 + kEpoch1601Offset;
    *(uint64_t*)out = ft;
}
INT WINAPI_CALL THETAZ_GetSystemTimePreciseAsFileTime(void* out) {
    if (!out) return 0;
    // Same source and contract as GetSystemTimeAsFileTime (a FILETIME at 100ns
    // resolution). The emulator exposes no finer-grained system-time clock, and
    // the Office installer delay-imports this only for coarse progress timing.
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t ft = (uint64_t)ts.tv_sec * 10000000ULL + (uint64_t)ts.tv_nsec / 100 + kEpoch1601Offset;
    *(uint64_t*)out = ft;
    return 0;
}
void WINAPI_CALL THETAZ_GetSystemTime(void* out) {
    if (!out) return;
    uint16_t* s = (uint16_t*)out;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    gmtime_r((const time_t*)&ts.tv_sec, &tm);
    s[0] = (uint16_t)(tm.tm_year + 1900);
    s[1] = (uint16_t)(tm.tm_mon + 1);
    s[2] = (uint16_t)tm.tm_wday;
    s[3] = (uint16_t)tm.tm_mday;
    s[4] = (uint16_t)tm.tm_hour;
    s[5] = (uint16_t)tm.tm_min;
    s[6] = (uint16_t)tm.tm_sec;
    s[7] = (uint16_t)(ts.tv_nsec / 1000000);
}
void WINAPI_CALL THETAZ_GetLocalTime(void* out) {
    if (!out) return;
    uint16_t* s = (uint16_t*)out;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    time_t t = ts.tv_sec;
    struct tm tm;
    localtime_r(&t, &tm);
    s[0] = (uint16_t)(tm.tm_year + 1900);
    s[1] = (uint16_t)(tm.tm_mon + 1);
    s[2] = (uint16_t)tm.tm_wday;
    s[3] = (uint16_t)tm.tm_mday;
    s[4] = (uint16_t)tm.tm_hour;
    s[5] = (uint16_t)tm.tm_min;
    s[6] = (uint16_t)tm.tm_sec;
    s[7] = (uint16_t)(ts.tv_nsec / 1000000);
}
BOOL WINAPI_CALL THETAZ_FileTimeToSystemTime(const FILETIME* ft_, SYSTEMTIME* st_) {
    if (!ft_ || !st_) return FALSE;
    uint64_t ft = *(const uint64_t*)ft_;
    if (ft < kEpoch1601Offset) return FALSE;
    uint64_t since1970 = (ft - kEpoch1601Offset) / 10000000ULL;
    time_t t = (time_t)since1970;
    struct tm tm;
    gmtime_r(&t, &tm);
    uint16_t* s = (uint16_t*)st_;
    s[0] = (uint16_t)(tm.tm_year + 1900);
    s[1] = (uint16_t)(tm.tm_mon + 1);
    s[2] = (uint16_t)tm.tm_wday;
    s[3] = (uint16_t)tm.tm_mday;
    s[4] = (uint16_t)tm.tm_hour;
    s[5] = (uint16_t)tm.tm_min;
    s[6] = (uint16_t)tm.tm_sec;
    s[7] = (uint16_t)((ft / 10000) % 1000);
    return TRUE;
}
BOOL WINAPI_CALL THETAZ_SystemTimeToFileTime(const SYSTEMTIME* st_, FILETIME* ft_) {
    if (!st_ || !ft_) return FALSE;
    const uint16_t* s = (const uint16_t*)st_;
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = s[0] - 1900;
    tm.tm_mon = s[1] - 1;
    tm.tm_mday = s[3];
    tm.tm_hour = s[4];
    tm.tm_min = s[5];
    tm.tm_sec = s[6];
    time_t t = timegm(&tm);
    uint64_t ft = ((uint64_t)t * 10000000ULL) + kEpoch1601Offset + (uint64_t)s[7] * 10000ULL;
    *(uint64_t*)ft_ = ft;
    return TRUE;
}

// Process / timing
HANDLE WINAPI_CALL THETAZ_GetProcessHeap() {
    return (HANDLE)1;
}
BOOL WINAPI_CALL THETAZ_QueryPerformanceCounter(LARGE_INTEGER* p) {
    if (p) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        p->QuadPart = (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
    }
    return TRUE;
}
BOOL WINAPI_CALL THETAZ_QueryPerformanceFrequency(LARGE_INTEGER* p) {
    if (p) p->QuadPart = 1000000000;
    return TRUE;
}
void WINAPI_CALL THETAZ_OutputDebugStringA(LPCSTR s) {
    if (s) fprintf(stderr, "%s\n", s);
}
void WINAPI_CALL THETAZ_OutputDebugStringW(LPCWSTR s) {
    if (s) {
        std::string a = utf16le_to_utf8(reinterpret_cast<const uint16_t*>(s));
        fprintf(stderr, "%s\n", a.c_str());
    }
}

static uint64_t host_rss_bytes() {
    FILE* f = fopen("/proc/self/statm", "r");
    if (!f) return 0;
    unsigned long total = 0, rss_pages = 0;
    if (fscanf(f, "%lu %lu", &total, &rss_pages) != 2) rss_pages = 0;
    fclose(f);
    return (uint64_t)rss_pages * 4096ULL;
}

BOOL WINAPI_CALL THETAZ_K32GetProcessMemoryInfo(HANDLE hProcess, PROCESS_MEMORY_COUNTERS* pmc_, DWORD cb) {
    (void)hProcess;
    if (!pmc_) {
        return TRUE;
    }
    if (cb >= 72) {
        // x86 PROCESS_MEMORY_COUNTERS: cb, then 4-byte PageFaultCount, then 8 SIZE_T (4-byte on x86) fields
        uint8_t* p = (uint8_t*)pmc_;
        memset(p, 0, 72);
        *(uint32_t*)(p + 0) = 72;
        *(uint32_t*)(p + 4) = 0; // PageFaultCount
        uint64_t rss = host_rss_bytes();
        uint64_t peak = rss; // coarse: current == peak
        *(uint32_t*)(p + 8)  = (uint32_t)peak;   // PeakWorkingSetSize
        *(uint32_t*)(p + 12) = (uint32_t)rss;    // WorkingSetSize
        *(uint32_t*)(p + 16) = 0;                // QuotaPeakPagedPoolUsage
        *(uint32_t*)(p + 20) = 0;                // QuotaPagedPoolUsage
        *(uint32_t*)(p + 24) = 0;                // QuotaPeakNonPagedPoolUsage
        *(uint32_t*)(p + 28) = 0;                // QuotaNonPagedPoolUsage
        *(uint32_t*)(p + 32) = (uint32_t)rss;    // PagefileUsage
        *(uint32_t*)(p + 36) = (uint32_t)rss;    // PeakPagefileUsage
    } else if (cb >= 40) {
        uint8_t* p = (uint8_t*)pmc_;
        memset(p, 0, 40);
        *(uint32_t*)(p + 0) = 40;
        *(uint32_t*)(p + 4) = 0;
        uint64_t rss = host_rss_bytes();
        *(uint32_t*)(p + 8)  = (uint32_t)rss;
        *(uint32_t*)(p + 12) = (uint32_t)rss;
        *(uint32_t*)(p + 16) = 0;
        *(uint32_t*)(p + 20) = 0;
        *(uint32_t*)(p + 24) = 0;
        *(uint32_t*)(p + 28) = 0;
        *(uint32_t*)(p + 32) = (uint32_t)rss;
        *(uint32_t*)(p + 36) = (uint32_t)rss;
    }
    return TRUE;
}

INT WINAPI_CALL THETAZ_GetUserDefaultLocaleName(LPWSTR out, INT size) {
    const char* loc = "en-US";
    int loc_chars = 5;
    if (!out || size <= 0) return loc_chars + 1;
    if (size <= loc_chars) return loc_chars + 1;
    uint16_t* w = reinterpret_cast<uint16_t*>(out);
    for (int i = 0; i < loc_chars; i++) w[i] = (uint16_t)loc[i];
    w[loc_chars] = 0;
    return loc_chars;
}

BOOL WINAPI_CALL THETAZ_AllocConsole() {
    return TRUE;
}

LPVOID WINAPI_CALL THETAZ_LocalFree(LPVOID hMem) {
    if (!hMem) return nullptr;
    uintptr_t p = (uintptr_t)hMem;
    if (p >= 0x60000000 && p < 0x76000000) {
        free_guest_heap(hMem);
    }
    return nullptr;
}

BOOL WINAPI_CALL THETAZ_GetCPInfo(UINT cp, CPINFO* info) {
    if (!info) return FALSE;
    memset(info, 0, sizeof(*info));
    info->MaxCharSize = (cp == 65001) ? 4 : 1;
    info->DefaultChar[0] = '?';
    info->DefaultChar[1] = '?';
    info->LeadByte[0] = 0;
    return TRUE;
}

BOOL WINAPI_CALL THETAZ_GetCPInfoExW(UINT cp, DWORD flags, CPINFOEXW* info) {
    (void)flags;
    if (!info) return FALSE;
    memset(info, 0, sizeof(*info));
    info->MaxCharSize = (cp == 65001) ? 4 : 1;
    info->DefaultChar[0] = '?';
    info->DefaultChar[1] = '?';
    info->UnicodeDefaultChar = L'?';
    const char* name = (cp == 65001) ? "65001 (UTF-8)" : "Windows-1252";
    size_t n = 0;
    while (name[n] && n < 259) { info->CodePageName[n] = (WCHAR)name[n]; n++; }
    info->CodePageName[n] = 0;
    return TRUE;
}

BOOL WINAPI_CALL THETAZ_IsValidCodePage(UINT cp) {
    switch (cp) {
    case 0: case 1: case 437: case 65001: case 1252: case 1200: case 1201:
        return TRUE;
    default:
        return TRUE;
    }
}

} // namespace ThetaZ

