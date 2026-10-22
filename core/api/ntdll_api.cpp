// ThetaZ — ntdll.dll API layer (Rtl*/Nt*/Ldr* functions)
// Copyright (c) 2026 ThetaZ Project. MIT License.
//
// ntdll is the NT "kernel mode contract" surface that user-mode DLLs (and
// apps like WINWORD.exe) call for the real implementation of the Win32 APIs.
// On Windows, kernel32/user32/etc forward into ntdll; here we go the other
// way and implement ntdll in terms of the existing kernel32 layer so both
// import surfaces resolve to the same host functions.

#include "core/api/api.h"
#include "core/api/kernel32.h"
#include "core/api/ntdll_api.h"
#include "core/loader/loader.h"
#include "core/ntdll/thread.h"
#include "core/ntdll/teb.h"
#include "core/ipc/thetaserver.h"
#include "core/x86/api_dispatch.h"
#include "core/x86/x86.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <cctype>

namespace ThetaZ {

namespace {

// NtCreateFile access flags (subset).
constexpr DWORD kFileReadData = 1;
constexpr DWORD kFileWriteData = 2;
constexpr DWORD kFileAppendData = 4;
constexpr DWORD kGenericRead = 0x80000000u;
constexpr DWORD kGenericWrite = 0x40000000u;

// NtCreateFile disposition values.
constexpr ULONG kFileOpen = 1;
constexpr ULONG kFileCreate = 2;
constexpr ULONG kFileOpenIf = 3;
constexpr ULONG kFileOverwrite = 4;
constexpr ULONG kFileOverwriteIf = 5;

uint32_t guest_peb_addr() {
    if (x86_dispatch_active()) return 0x7FFDF000u;   // 32-bit guest PEB
    uint64_t peb = get_main_peb();
    return peb ? static_cast<uint32_t>(peb) : 0x7FFDF000u;
}

uint32_t guest_teb_addr() {
    if (x86_dispatch_active()) {
        X86CpuState* st = x86_current_state();
        if (st && st->fs_base) return st->fs_base;
        return 0x7FFF0000u;
    }
    uint64_t teb = get_main_teb();
    return teb ? static_cast<uint32_t>(teb) : 0x7FFF0000u;
}

// Convert an NT-relative timeout (negative = 100ns relative, 0 = wait forever,
// positive = absolute) into milliseconds for the kernel32 WaitFor* layer.
DWORD nt_timeout_to_ms(const void* timeout) {
    if (!timeout) return 0xFFFFFFFF;
    int64_t value = *static_cast<const int64_t*>(timeout);
    if (value == 0) return 0xFFFFFFFF;
    if (value < 0) {
        int64_t ns100 = -value;
        return static_cast<DWORD>((ns100 + 9999) / 10000); // 100ns -> ms, round up
    }
    // Absolute 100ns since 1601: convert to a relative ms count.
    using namespace std::chrono;
    auto now = time_point_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now());
    constexpr int64_t kEpochDiff100ns =
        116444736000000000LL; // 1601->1970 in 100ns units
    int64_t now_ns = now.time_since_epoch().count();
    int64_t target_100ns = value;
    int64_t now_100ns = now_ns / 100 + kEpochDiff100ns;
    int64_t delta = target_100ns - now_100ns;
    if (delta <= 0) return 0;
    return static_cast<DWORD>((delta + 9999) / 10000);
}

// Read a UNICODE_STRING at a guest address into a std::string (utf8).
// Buffer offset is 4 for a 32-bit guest, 8 for a 64-bit guest.
std::string unicode_string_to_utf8(const void* us) {
    const uint8_t* p = static_cast<const uint8_t*>(us);
    uint16_t len = *reinterpret_cast<const uint16_t*>(p);      // Length (bytes)
    const void* buf = nullptr;
    if (x86_dispatch_active()) {
        buf = reinterpret_cast<const void*>(
            static_cast<uintptr_t>(*reinterpret_cast<const uint32_t*>(p + 4)));
    } else {
        buf = *reinterpret_cast<void* const*>(p + 8);
    }
    if (!buf || len == 0) return "";
    return utf16le_to_utf8(static_cast<const uint16_t*>(buf));
}

// Read an ANSI_STRING (byte Length + char* Buffer) at a guest address.
std::string ansi_string_to_utf8(const void* s) {
    const uint8_t* p = static_cast<const uint8_t*>(s);
    uint16_t len = *reinterpret_cast<const uint16_t*>(p);
    const void* buf = nullptr;
    if (x86_dispatch_active()) {
        buf = reinterpret_cast<const void*>(
            static_cast<uintptr_t>(*reinterpret_cast<const uint32_t*>(p + 4)));
    } else {
        buf = *reinterpret_cast<void* const*>(p + 8);
    }
    if (!buf || len == 0) return "";
    return std::string(static_cast<const char*>(buf), len);
}

// Write a UNICODE_STRING into guest memory using the guest's layout.
void write_guest_unicode_string(void* dest, uint16_t length, uint16_t maxlength, void* buffer) {
    uint8_t* p = static_cast<uint8_t*>(dest);
    *reinterpret_cast<uint16_t*>(p + 0) = length;
    *reinterpret_cast<uint16_t*>(p + 2) = maxlength;
    if (x86_dispatch_active()) {
        *reinterpret_cast<uint32_t*>(p + 4) = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(buffer));
    } else {
        *reinterpret_cast<void**>(p + 8) = buffer;
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Rtl* runtime library
// ---------------------------------------------------------------------------

void* WINAPI_CALL NTDLL_RtlAllocateHeap(void* HeapHandle, ULONG Flags, SIZE_T Size) {
    (void)HeapHandle;
    return THETAZ_HeapAlloc(reinterpret_cast<HANDLE>(1), Flags, Size);
}

BOOL WINAPI_CALL NTDLL_RtlFreeHeap(void* HeapHandle, ULONG Flags, void* BaseAddress) {
    (void)HeapHandle;
    return THETAZ_HeapFree(reinterpret_cast<HANDLE>(1), Flags, BaseAddress);
}

void* WINAPI_CALL NTDLL_RtlReAllocateHeap(void* HeapHandle, ULONG Flags, void* BaseAddress, SIZE_T Size) {
    (void)HeapHandle;
    return THETAZ_HeapReAlloc(reinterpret_cast<HANDLE>(1), Flags, BaseAddress, Size);
}

SIZE_T WINAPI_CALL NTDLL_RtlSizeHeap(void* HeapHandle, ULONG Flags, void* BaseAddress) {
    (void)HeapHandle;
    return THETAZ_HeapSize(reinterpret_cast<HANDLE>(1), Flags, BaseAddress);
}

void* WINAPI_CALL NTDLL_RtlCreateHeap(ULONG Flags, void* BaseAddress, SIZE_T ReserveSize,
                                      SIZE_T CommitSize, void* Lock, void* Parameters) {
    (void)Flags; (void)BaseAddress; (void)ReserveSize; (void)CommitSize; (void)Lock; (void)Parameters;
    return reinterpret_cast<void*>(2); // same as kernel32 HeapCreate
}

void* WINAPI_CALL NTDLL_RtlDestroyHeap(void* HeapHandle) {
    (void)HeapHandle;
    return nullptr; // NULL = success
}

SIZE_T WINAPI_CALL NTDLL_RtlCompactHeap(void* HeapHandle, ULONG Flags) {
    (void)HeapHandle; (void)Flags;
    return 0;
}

SIZE_T WINAPI_CALL NTDLL_RtlGetProcessHeaps(ULONG NumberOfHeaps, void** ProcessHeaps) {
    if (NumberOfHeaps >= 1 && ProcessHeaps) ProcessHeaps[0] = reinterpret_cast<void*>(1);
    return 1;
}

void WINAPI_CALL NTDLL_RtlInitializeCriticalSection(void* lpCriticalSection) {
    THETAZ_InitializeCriticalSection(static_cast<PCRITICAL_SECTION>(lpCriticalSection));
}

void WINAPI_CALL NTDLL_RtlDeleteCriticalSection(void* lpCriticalSection) {
    THETAZ_DeleteCriticalSection(static_cast<PCRITICAL_SECTION>(lpCriticalSection));
}

void WINAPI_CALL NTDLL_RtlEnterCriticalSection(void* lpCriticalSection) {
    THETAZ_EnterCriticalSection(static_cast<PCRITICAL_SECTION>(lpCriticalSection));
}

void WINAPI_CALL NTDLL_RtlLeaveCriticalSection(void* lpCriticalSection) {
    THETAZ_LeaveCriticalSection(static_cast<PCRITICAL_SECTION>(lpCriticalSection));
}

BOOL WINAPI_CALL NTDLL_RtlTryEnterCriticalSection(void* lpCriticalSection) {
    return THETAZ_TryEnterCriticalSection(static_cast<PCRITICAL_SECTION>(lpCriticalSection));
}

BOOL WINAPI_CALL NTDLL_RtlInitializeCriticalSectionAndSpinCount(void* lpCriticalSection, ULONG dwSpinCount) {
    (void)dwSpinCount;
    THETAZ_InitializeCriticalSection(static_cast<PCRITICAL_SECTION>(lpCriticalSection));
    return TRUE;
}

BOOL WINAPI_CALL NTDLL_RtlSetCriticalSectionSpinCount(void* lpCriticalSection, ULONG dwSpinCount) {
    (void)lpCriticalSection; (void)dwSpinCount;
    return 0;
}

void WINAPI_CALL NTDLL_RtlAcquireSRWLockExclusive(void* SRWLock) {
    THETAZ_AcquireSRWLockExclusive(SRWLock);
}

void WINAPI_CALL NTDLL_RtlAcquireSRWLockShared(void* SRWLock) {
    THETAZ_AcquireSRWLockShared(SRWLock);
}

void WINAPI_CALL NTDLL_RtlReleaseSRWLockExclusive(void* SRWLock) {
    THETAZ_ReleaseSRWLockExclusive(SRWLock);
}

void WINAPI_CALL NTDLL_RtlReleaseSRWLockShared(void* SRWLock) {
    THETAZ_ReleaseSRWLockShared(SRWLock);
}

void* WINAPI_CALL NTDLL_RtlGetCurrentPeb() {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(guest_peb_addr()));
}

void* WINAPI_CALL NTDLL_RtlGetCurrentTeb() {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(guest_teb_addr()));
}

void WINAPI_CALL NTDLL_RtlGetLastWin32Error() {
    // On the real ntdll this is the same as kernel32!GetLastError (both read
    // the TEB LastErrorValue). Our kernel32 keeps it in a thread-local.
    return;
}

void WINAPI_CALL NTDLL_RtlSetLastWin32Error(DWORD dwErrCode) {
    THETAZ_SetLastError(dwErrCode);
}

NTSTATUS WINAPI_CALL NTDLL_RtlGetVersion(PRTL_OSVERSIONINFOW lpVersionInformation) {
    if (!lpVersionInformation) return STATUS_INVALID_PARAMETER;
    PRTL_OSVERSIONINFOEXW v = reinterpret_cast<PRTL_OSVERSIONINFOEXW>(lpVersionInformation);
    memset(v, 0, 0x108);
    v->dwOSVersionInfoSize = 0x10C;
    v->dwMajorVersion = 10;
    v->dwMinorVersion = 0;
    v->dwBuildNumber = 19045;
    v->dwPlatformId = 2; // VER_PLATFORM_WIN32_NT
    v->wProductType = 1; // VER_NT_WORKSTATION
    v->wSuiteMask = 0x100;
    return STATUS_SUCCESS;
}

ULONGLONG WINAPI_CALL NTDLL_RtlGetSystemTimePrecise() {
    // 100ns since 1601 (same epoch as GetSystemTimeAsFileTime).
    using namespace std::chrono;
    auto now = system_clock::now();
    auto since_epoch = now.time_since_epoch();
    int64_t ns = duration_cast<std::chrono::nanoseconds>(since_epoch).count();
    return static_cast<ULONGLONG>((ns / 100) + 116444736000000000LL);
}

NTSTATUS WINAPI_CALL NTDLL_RtlNtStatusToDosError(NTSTATUS Status) {
    switch (Status) {
    case STATUS_SUCCESS: return 0;
    case STATUS_INVALID_PARAMETER: return 87;   // ERROR_INVALID_PARAMETER
    case STATUS_ACCESS_DENIED: return 5;        // ERROR_ACCESS_DENIED
    case STATUS_NO_MEMORY: return 8;            // ERROR_NOT_ENOUGH_MEMORY
    case STATUS_OBJECT_NAME_NOT_FOUND:
    case STATUS_NOT_FOUND: return 2;            // ERROR_FILE_NOT_FOUND
    case STATUS_OBJECT_PATH_NOT_FOUND: return 3; // ERROR_PATH_NOT_FOUND
    case STATUS_BUFFER_TOO_SMALL:
    case STATUS_INFO_LENGTH_MISMATCH: return 122; // ERROR_INSUFFICIENT_BUFFER
    case STATUS_INVALID_HANDLE: return 6;       // ERROR_INVALID_HANDLE
    default: return static_cast<NTSTATUS>(Status & 0xFFFF) ? static_cast<NTSTATUS>(Status & 0xFFFF) : 0;
    }
}

void WINAPI_CALL NTDLL_RtlZeroMemory(void* dst, SIZE_T len) {
    if (dst) memset(dst, 0, len);
}

void WINAPI_CALL NTDLL_RtlFillMemory(void* dst, SIZE_T len, BYTE fill) {
    if (dst) memset(dst, fill, len);
}

void WINAPI_CALL NTDLL_RtlCopyMemory(void* dst, const void* src, SIZE_T len) {
    if (dst && src) memmove(dst, src, len);
}

void WINAPI_CALL NTDLL_RtlMoveMemory(void* dst, const void* src, SIZE_T len) {
    if (dst && src) memmove(dst, src, len);
}

SIZE_T WINAPI_CALL NTDLL_RtlCompareMemory(const void* a, const void* b, SIZE_T len) {
    const uint8_t* pa = static_cast<const uint8_t*>(a);
    const uint8_t* pb = static_cast<const uint8_t*>(b);
    SIZE_T i = 0;
    while (i < len && pa[i] == pb[i]) i++;
    return i;
}

BOOL WINAPI_CALL NTDLL_RtlEqualMemory(const void* a, const void* b, SIZE_T len) {
    return (a == b) || (len == 0) || (memcmp(a, b, len) == 0);
}

void WINAPI_CALL NTDLL_RtlInitUnicodeString(PUNICODE_STRING dest, const WCHAR* src) {
    if (!dest) return;
    if (!src) {
        write_guest_unicode_string(dest, 0, 0, nullptr);
        return;
    }
    size_t n = 0;
    while (src[n]) n++;
    write_guest_unicode_string(dest, static_cast<USHORT>(n * 2),
                               static_cast<USHORT>((n + 1) * 2),
                               const_cast<WCHAR*>(src));
}

void WINAPI_CALL NTDLL_RtlInitAnsiString(PSTRING dest, const char* src) {
    if (!dest) return;
    if (!src) {
        write_guest_unicode_string(dest, 0, 0, nullptr);
        return;
    }
    size_t n = strlen(src);
    write_guest_unicode_string(dest, static_cast<USHORT>(n),
                               static_cast<USHORT>(n + 1),
                               const_cast<char*>(src));
}

BOOL WINAPI_CALL NTDLL_RtlCreateUnicodeStringFromAsciiz(PUNICODE_STRING dest, const char* src) {
    if (!dest || !src) return FALSE;
    std::u16string w;
    utf8_to_utf16le(src, w);
    void* buf = alloc_guest_shared((w.size() + 1) * 2);
    if (!buf) return FALSE;
    memcpy(buf, w.c_str(), (w.size() + 1) * 2);
    write_guest_unicode_string(dest, static_cast<USHORT>(w.size() * 2),
                               static_cast<USHORT>((w.size() + 1) * 2), buf);
    return TRUE;
}

void WINAPI_CALL NTDLL_RtlFreeUnicodeString(PUNICODE_STRING str) {
    if (str) write_guest_unicode_string(str, 0, 0, nullptr);
}

void WINAPI_CALL NTDLL_RtlFreeAnsiString(PSTRING str) {
    if (str) write_guest_unicode_string(str, 0, 0, nullptr);
}

NTSTATUS WINAPI_CALL NTDLL_RtlUnicodeStringToAnsiString(PSTRING dest, const PUNICODE_STRING src, BOOL alloc) {
    if (!dest || !src) return STATUS_INVALID_PARAMETER;
    std::string utf8 = unicode_string_to_utf8(src);
    void* buf = nullptr;
    if (alloc) {
        buf = alloc_guest_shared(utf8.size() + 1);
        if (!buf) return STATUS_NO_MEMORY;
    } else {
        buf = reinterpret_cast<void*>(static_cast<uintptr_t>(
            *reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(dest) +
                                               (x86_dispatch_active() ? 4 : 8))));
        if (!buf) return STATUS_NO_MEMORY;
    }
    memcpy(buf, utf8.c_str(), utf8.size() + 1);
    write_guest_unicode_string(dest, static_cast<USHORT>(utf8.size()),
                               static_cast<USHORT>(utf8.size() + 1), buf);
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_RtlAnsiStringToUnicodeString(PUNICODE_STRING dest, const PSTRING src, BOOL alloc) {
    if (!dest || !src) return STATUS_INVALID_PARAMETER;
    std::string a = ansi_string_to_utf8(src);
    std::u16string w;
    utf8_to_utf16le(a, w);
    void* buf = nullptr;
    if (alloc) {
        buf = alloc_guest_shared((w.size() + 1) * 2);
        if (!buf) return STATUS_NO_MEMORY;
    } else {
        buf = reinterpret_cast<void*>(static_cast<uintptr_t>(
            *reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(dest) +
                                               (x86_dispatch_active() ? 4 : 8))));
        if (!buf) return STATUS_NO_MEMORY;
    }
    memcpy(buf, w.c_str(), (w.size() + 1) * 2);
    write_guest_unicode_string(dest, static_cast<USHORT>(w.size() * 2),
                               static_cast<USHORT>((w.size() + 1) * 2), buf);
    return STATUS_SUCCESS;
}

void WINAPI_CALL NTDLL_RtlCopyUnicodeString(PUNICODE_STRING dest, const PUNICODE_STRING src) {
    if (!dest || !src) return;
    std::string s = unicode_string_to_utf8(src);
    uintptr_t existing = x86_dispatch_active()
        ? static_cast<uintptr_t>(*reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(dest) + 4))
        : reinterpret_cast<uintptr_t>(*reinterpret_cast<void* const*>(reinterpret_cast<const uint8_t*>(dest) + 8));
    if (!existing) return;
    memcpy(reinterpret_cast<void*>(existing), s.c_str(), s.size() + 1);
    write_guest_unicode_string(dest, static_cast<USHORT>(s.size()),
                               static_cast<USHORT>(s.size() + 1),
                               reinterpret_cast<void*>(existing));
}

void WINAPI_CALL NTDLL_RtlGenRandom(void* buffer, ULONG length) {
    if (!buffer || length == 0) return;
    FILE* f = fopen("/dev/urandom", "rb");
    if (f) {
        size_t got = fread(buffer, 1, length, f);
        fclose(f);
        if (got == length) return;
    }
    // Fallback: xorshift seeded from time+pid.
    uint64_t s = static_cast<uint64_t>(time(nullptr) ^ getpid() ^ 0x9E3779B97F4A7C15ull);
    uint8_t* p = static_cast<uint8_t*>(buffer);
    for (ULONG i = 0; i < length; i++) {
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        p[i] = static_cast<uint8_t>(s);
    }
}

ULONG WINAPI_CALL NTDLL_RtlRandom(unsigned long* seed) {
    if (!seed) return 0;
    *seed = *seed * 1103515245u + 12345u;
    return ((*seed >> 16) & 0x7FFF) | ((*seed >> 8) & 0xFFFF8000u);
}

USHORT WINAPI_CALL NTDLL_RtlDowncaseUnicodeChar(USHORT ch) {
    if (ch >= 'A' && ch <= 'Z') return static_cast<USHORT>(ch + 32);
    if (ch >= 0xC0 && ch <= 0xDE && ch != 0xD7) return static_cast<USHORT>(ch + 32);
    return ch;
}

NTSTATUS WINAPI_CALL NTDLL_RtlImageNtHeader(void* BaseAddress, void** NtHeader) {
    const uint8_t* base = static_cast<const uint8_t*>(BaseAddress);
    if (!base) return STATUS_INVALID_PARAMETER;
    const uint8_t* dos = base;
    uint32_t pe_off = *reinterpret_cast<const uint32_t*>(dos + 0x3C);
    if (NtHeader) *NtHeader = const_cast<void*>(static_cast<const void*>(dos + pe_off));
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_RtlImageDirectoryEntryToData(void* BaseAddress, BOOL mapped, USHORT directory, ULONG* size) {
    (void)mapped;
    const uint8_t* base = static_cast<const uint8_t*>(BaseAddress);
    if (!base) return STATUS_INVALID_PARAMETER;
    uint32_t pe_off = *reinterpret_cast<const uint32_t*>(base + 0x3C);
    const uint8_t* opt = base + pe_off + 24;
    uint16_t magic = *reinterpret_cast<const uint16_t*>(opt);
    const uint8_t* dirs = (magic == 0x20B) ? opt + 112 : opt + 96;
    const uint8_t* e = dirs + directory * 8;
    uint32_t rva = *reinterpret_cast<const uint32_t*>(e);
    uint32_t sz = *reinterpret_cast<const uint32_t*>(e + 4);
    if (size) *size = sz;
    return rva ? static_cast<NTSTATUS>(reinterpret_cast<uintptr_t>(base + rva)) : STATUS_NOT_FOUND;
}

NTSTATUS WINAPI_CALL NTDLL_RtlFindExportedRoutineByName(void* imageBase, const char* name, void** routine) {
    PELoader& loader = PELoader::instance();
    LoadedModule* m = loader.find_module_by_base(reinterpret_cast<uint64_t>(imageBase));
    if (!m) return STATUS_NOT_FOUND;
    void* addr = loader.get_export_address(*m, 0, name);
    if (!addr) return STATUS_NOT_FOUND;
    if (routine) *routine = addr;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_RtlRaiseException(void* ExceptionRecord) {
    // The x86 EXCEPTION_RECORD begins with the exception code.
    uint32_t code = 0;
    if (ExceptionRecord) code = *reinterpret_cast<uint32_t*>(ExceptionRecord);
    THETAZ_RaiseException(code, 0, 0, nullptr);
    return STATUS_SUCCESS;
}

void WINAPI_CALL NTDLL_RtlRaiseStatus(NTSTATUS Status) {
    THETAZ_RaiseException(static_cast<DWORD>(Status), 0, 0, nullptr);
}

// ---------------------------------------------------------------------------
// Nt* system calls
// ---------------------------------------------------------------------------

NTSTATUS WINAPI_CALL NTDLL_NtClose(void* handle) {
    return THETAZ_CloseHandle(handle) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

NTSTATUS WINAPI_CALL NTDLL_NtQuerySystemTime(ULONGLONG* SystemTime) {
    if (!SystemTime) return STATUS_INVALID_PARAMETER;
    *SystemTime = NTDLL_RtlGetSystemTimePrecise();
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_NtQueryPerformanceCounter(void* lpPerformanceCount, void* lpFrequency) {
    if (!lpPerformanceCount) return STATUS_INVALID_PARAMETER;
    LARGE_INTEGER* c = static_cast<LARGE_INTEGER*>(lpPerformanceCount);
    THETAZ_QueryPerformanceCounter(c);
    if (lpFrequency) THETAZ_QueryPerformanceFrequency(static_cast<LARGE_INTEGER*>(lpFrequency));
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_NtGetTickCount() {
    return THETAZ_GetTickCount();
}

NTSTATUS WINAPI_CALL NTDLL_NtDelayExecution(BOOL alertable, void* delayInterval) {
    (void)alertable;
    if (!delayInterval) return STATUS_SUCCESS;
    DWORD ms = nt_timeout_to_ms(delayInterval);
    THETAZ_Sleep(ms);
    return ms == 0 ? STATUS_TIMEOUT : STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_NtYieldExecution() {
    sched_yield();
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_NtTerminateProcess(void* processHandle, LONG exitStatus) {
    (void)processHandle;
    THETAZ_ExitProcess(static_cast<uint32_t>(exitStatus));
    return STATUS_SUCCESS; // not reached
}

NTSTATUS WINAPI_CALL NTDLL_NtTerminateThread(void* threadHandle, LONG exitStatus) {
    (void)threadHandle;
    THETAZ_ExitThread(static_cast<DWORD>(exitStatus));
    return STATUS_SUCCESS; // not reached
}

NTSTATUS WINAPI_CALL NTDLL_NtCurrentTeb() {
    return guest_teb_addr();
}

NTSTATUS WINAPI_CALL NTDLL_NtQueryInformationProcess(void* processHandle, ULONG infoClass,
                                                    void* info, ULONG len, ULONG* retLen) {
    (void)processHandle;
    if (!info) return STATUS_INVALID_PARAMETER;
    const bool x86 = x86_dispatch_active();
    uint8_t* p = static_cast<uint8_t*>(info);
    switch (infoClass) {
    case 0: { // ProcessBasicInformation
        if (len < (x86 ? 24u : 48u)) return STATUS_INFO_LENGTH_MISMATCH;
        memset(p, 0, x86 ? 24 : 48);
        uint64_t pid = ThreadManager::instance().get_current_guest_pid();
        if (pid == 0) pid = static_cast<uint64_t>(getpid());
        if (x86) {
            *reinterpret_cast<uint32_t*>(p + 4) = guest_peb_addr();
            *reinterpret_cast<uint32_t*>(p + 8) = 1;            // AffinityMask
            *reinterpret_cast<int32_t*>(p + 12) = 0;            // BasePriority
            *reinterpret_cast<uint32_t*>(p + 16) = static_cast<uint32_t>(pid);
            *reinterpret_cast<uint32_t*>(p + 20) = static_cast<uint32_t>(getppid());
        } else {
            *reinterpret_cast<void**>(p + 8) = reinterpret_cast<void*>(static_cast<uintptr_t>(guest_peb_addr()));
            *reinterpret_cast<uint64_t*>(p + 16) = 1;
            *reinterpret_cast<int32_t*>(p + 24) = 0;
            *reinterpret_cast<uint64_t*>(p + 32) = pid;
            *reinterpret_cast<uint64_t*>(p + 40) = static_cast<uint64_t>(getppid());
        }
        if (retLen) *retLen = x86 ? 24 : 48;
        return STATUS_SUCCESS;
    }
    case 1: // ProcessIoCounters
        if (len < (x86 ? 48u : 56u)) return STATUS_INFO_LENGTH_MISMATCH;
        memset(p, 0, x86 ? 48 : 56);
        if (retLen) *retLen = x86 ? 48 : 56;
        return STATUS_SUCCESS;
    case 2: { // ProcessVmCounters
        if (len < (x86 ? 48u : 56u)) return STATUS_INFO_LENGTH_MISMATCH;
        memset(p, 0, x86 ? 48 : 56);
        if (retLen) *retLen = x86 ? 48 : 56;
        return STATUS_SUCCESS;
    }
    case 5: // ProcessWow64Information (0 for 32-bit, non-zero for 64-bit)
        if (len < (x86 ? 4u : 8u)) return STATUS_INFO_LENGTH_MISMATCH;
        memset(p, 0, x86 ? 4 : 8);
        if (retLen) *retLen = x86 ? 4 : 8;
        return STATUS_SUCCESS;
    case 0x1F: // ProcessDebugObjectHandle
        if (len < (x86 ? 4u : 8u)) return STATUS_INFO_LENGTH_MISMATCH;
        memset(p, 0, x86 ? 4 : 8);
        if (retLen) *retLen = x86 ? 4 : 8;
        return STATUS_SUCCESS;
    default:
        return STATUS_INVALID_PARAMETER;
    }
}

NTSTATUS WINAPI_CALL NTDLL_NtQuerySystemInformation(ULONG infoClass, void* info,
                                                    ULONG len, ULONG* retLen) {
    if (!info) return STATUS_INVALID_PARAMETER;
    const bool x86 = x86_dispatch_active();
    uint8_t* p = static_cast<uint8_t*>(info);
    switch (infoClass) {
    case 0: { // SystemBasicInformation
        if (len < (x86 ? 44u : 48u)) return STATUS_INFO_LENGTH_MISMATCH;
        memset(p, 0, x86 ? 44 : 48);
        long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
        if (nprocs < 1) nprocs = 1;
        uint32_t* u = reinterpret_cast<uint32_t*>(p);
        u[0] = 0;               // Reserved
        u[1] = 156250;          // TimerResolution (100ns units)
        u[2] = 0x1000;          // PageSize
        u[3] = 1;               // NumberOfPhysicalPages
        u[4] = 0;               // LowestPhysicalAddress
        u[5] = 0x7FFE0000;      // HighestPhysicalAddress
        u[6] = 0x10000;         // AllocationGranularity
        if (x86) {
            *reinterpret_cast<uint32_t*>(p + 28) = 0x10000;   // LowestUserAddress
            *reinterpret_cast<uint32_t*>(p + 32) = 0x7FFE0000; // HighestUserAddress
            *reinterpret_cast<uint32_t*>(p + 36) = 1;          // ActiveProcessors
            *reinterpret_cast<uint8_t*>(p + 40) = static_cast<uint8_t>(nprocs);
        } else {
            *reinterpret_cast<uint64_t*>(p + 28) = 0x10000;
            *reinterpret_cast<uint64_t*>(p + 36) = 0x7FFE0000ull;
            *reinterpret_cast<uint64_t*>(p + 44) = 1;
            *reinterpret_cast<uint8_t*>(p + 52) = static_cast<uint8_t>(nprocs);
        }
        if (retLen) *retLen = x86 ? 44 : 48;
        return STATUS_SUCCESS;
    }
    case 1: { // SystemProcessorInformation
        if (len < (x86 ? 32u : 48u)) return STATUS_INFO_LENGTH_MISMATCH;
        memset(p, 0, x86 ? 32 : 48);
        if (retLen) *retLen = x86 ? 32 : 48;
        return STATUS_SUCCESS;
    }
    case 3: { // SystemTimeOfDayInformation
        if (len < 48) return STATUS_INFO_LENGTH_MISMATCH;
        memset(p, 0, 48);
        if (retLen) *retLen = 48;
        return STATUS_SUCCESS;
    }
    case 8: { // SystemProcessorPerformanceInformation
        if (len < (x86 ? 48u : 64u)) return STATUS_INFO_LENGTH_MISMATCH;
        memset(p, 0, x86 ? 48 : 64);
        if (retLen) *retLen = x86 ? 48 : 64;
        return STATUS_SUCCESS;
    }
    case 5: // SystemProcessInformation (variable; return not-supported rather than crash)
        return STATUS_INFO_LENGTH_MISMATCH;
    default:
        return STATUS_INVALID_PARAMETER;
    }
}

NTSTATUS WINAPI_CALL NTDLL_NtQueryInformationThread(void* threadHandle, ULONG infoClass,
                                                    void* info, ULONG len, ULONG* retLen) {
    (void)threadHandle;
    if (!info) return STATUS_INVALID_PARAMETER;
    const bool x86 = x86_dispatch_active();
    uint8_t* p = static_cast<uint8_t*>(info);
    switch (infoClass) {
    case 0: { // ThreadBasicInformation
        if (len < (x86 ? 24u : 48u)) return STATUS_INFO_LENGTH_MISMATCH;
        memset(p, 0, x86 ? 24 : 48);
        uint64_t tid = ThreadManager::instance().get_current_thread();
        if (x86) {
            *reinterpret_cast<uint32_t*>(p + 0) = 0; // ExitStatus
            *reinterpret_cast<uint32_t*>(p + 4) = guest_teb_addr();
            *reinterpret_cast<uint32_t*>(p + 8) = 1; // AffinityMask
            *reinterpret_cast<int32_t*>(p + 12) = 0; // BasePriority
            *reinterpret_cast<uint32_t*>(p + 16) = static_cast<uint32_t>(tid);
        } else {
            *reinterpret_cast<void**>(p + 8) = reinterpret_cast<void*>(static_cast<uintptr_t>(guest_teb_addr()));
            *reinterpret_cast<uint64_t*>(p + 16) = 1;
            *reinterpret_cast<int32_t*>(p + 24) = 0;
            *reinterpret_cast<uint64_t*>(p + 32) = tid;
        }
        if (retLen) *retLen = x86 ? 24 : 48;
        return STATUS_SUCCESS;
    }
    case 1: // ThreadTimes
        if (len < (x86 ? 32u : 32u)) return STATUS_INFO_LENGTH_MISMATCH;
        memset(p, 0, 32);
        if (retLen) *retLen = 32;
        return STATUS_SUCCESS;
    case 2: // ThreadPriority
        if (len < 4) return STATUS_INFO_LENGTH_MISMATCH;
        *reinterpret_cast<int32_t*>(p) = 0;
        if (retLen) *retLen = 4;
        return STATUS_SUCCESS;
    case 3: // ThreadAffinityMask
        if (len < (x86 ? 4u : 8u)) return STATUS_INFO_LENGTH_MISMATCH;
        memset(p, 0, x86 ? 4 : 8);
        if (retLen) *retLen = x86 ? 4 : 8;
        return STATUS_SUCCESS;
    default:
        return STATUS_INVALID_PARAMETER;
    }
}

NTSTATUS WINAPI_CALL NTDLL_NtSetInformationThread(void* threadHandle, ULONG infoClass,
                                                  void* info, ULONG len) {
    (void)threadHandle; (void)info; (void)len;
    switch (infoClass) {
    case 0x11: // ThreadHideFromDebugger
        return STATUS_SUCCESS;
    case 0x17: // ThreadIsIoPending
    case 0x26: // ThreadSetThreadInformation
    case 0x20: // ThreadNameInformation
    default:
        return STATUS_SUCCESS;
    }
}

NTSTATUS WINAPI_CALL NTDLL_NtSuspendThread(void* threadHandle, ULONG* previousCount) {
    if (previousCount) *previousCount = 1;
    (void)threadHandle;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_NtResumeThread(void* threadHandle, ULONG* previousCount) {
    if (previousCount) *previousCount = 0;
    (void)threadHandle;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_NtQueryVirtualMemory(void* processHandle, const void* baseAddr,
                                                ULONG infoClass, void* info, SIZE_T len, SIZE_T* retLen) {
    (void)processHandle;
    if (!info) return STATUS_INVALID_PARAMETER;
    // class 0 = MemoryBasicInformation (mirrors VirtualQueryEx; memory_api.cpp).
    const bool x86 = x86_dispatch_active();
    uint8_t* p = static_cast<uint8_t*>(info);
    SIZE_T need = x86 ? 28 : 48;
    if (len < need) {
        if (retLen) *retLen = need;
        return STATUS_INFO_LENGTH_MISMATCH;
    }
    memset(p, 0, need);
    // Simplify: report the region as a committed RW page.
    if (x86) {
        *reinterpret_cast<uint32_t*>(p + 0) = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(baseAddr) & ~0xFFFull);
        *reinterpret_cast<uint32_t*>(p + 4) = 0x1000;
        *reinterpret_cast<uint32_t*>(p + 8) = 0x4000;    // MEM_COMMIT
        *reinterpret_cast<uint32_t*>(p + 12) = 0x04;     // PAGE_READWRITE
    } else {
        *reinterpret_cast<uint64_t*>(p + 0) = reinterpret_cast<uint64_t>(baseAddr) & ~0xFFFull;
        *reinterpret_cast<uint64_t*>(p + 8) = 0x1000;
        *reinterpret_cast<uint32_t*>(p + 16) = 0x4000;
        *reinterpret_cast<uint32_t*>(p + 20) = 0x04;
    }
    if (retLen) *retLen = need;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_NtProtectVirtualMemory(void* processHandle, void** baseAddr,
                                                  SIZE_T* regionSize, ULONG newProtect, ULONG* oldProtect) {
    (void)processHandle;
    if (!baseAddr || !*baseAddr || !regionSize || !oldProtect) return STATUS_INVALID_PARAMETER;
    *oldProtect = 0x04;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_NtAllocateVirtualMemory(void* processHandle, void** baseAddr,
                                                   ULONG_PTR zeroBits, SIZE_T* regionSize,
                                                   ULONG allocType, ULONG protect) {
    (void)processHandle; (void)zeroBits; (void)allocType; (void)protect;
    if (!baseAddr || !regionSize || *regionSize == 0) return STATUS_INVALID_PARAMETER;
    void* p = alloc_guest_shared(*regionSize);
    if (!p) return STATUS_NO_MEMORY;
    if (baseAddr) *baseAddr = p;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_NtFreeVirtualMemory(void* processHandle, void** baseAddr,
                                               SIZE_T* regionSize, ULONG freeType) {
    (void)processHandle; (void)baseAddr; (void)regionSize; (void)freeType;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_NtCreateEvent(void* eventHandle, ULONG access, void* attrs,
                                         ULONG type, BOOLEAN initialState, const WCHAR* name) {
    (void)access; (void)attrs;
    if (!eventHandle) return STATUS_INVALID_PARAMETER;
    HANDLE h = THETAZ_CreateEventW(nullptr, type != 0, initialState != 0, name);
    if (!h) return STATUS_UNSUCCESSFUL;
    *reinterpret_cast<void**>(eventHandle) = h;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_NtOpenEvent(void* eventHandle, ULONG access, void* attrs, void* name) {
    (void)access; (void)attrs;
    if (!eventHandle) return STATUS_INVALID_PARAMETER;
    std::string n = unicode_string_to_utf8(name);
    if (n.empty()) return STATUS_INVALID_PARAMETER;
    std::u16string w;
    utf8_to_utf16le(n, w);
    HANDLE h = THETAZ_OpenEventW(0x80000000, FALSE, reinterpret_cast<LPCWSTR>(w.data()));
    if (!h) return STATUS_OBJECT_NAME_NOT_FOUND;
    *reinterpret_cast<void**>(eventHandle) = h;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_NtSetEvent(void* eventHandle, ULONG* previousState) {
    if (previousState) *previousState = 0;
    return THETAZ_SetEvent(static_cast<HANDLE>(eventHandle)) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

NTSTATUS WINAPI_CALL NTDLL_NtResetEvent(void* eventHandle, ULONG* previousState) {
    if (previousState) *previousState = 0;
    return THETAZ_ResetEvent(static_cast<HANDLE>(eventHandle)) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

NTSTATUS WINAPI_CALL NTDLL_NtPulseEvent(void* eventHandle, ULONG* previousState) {
    if (previousState) *previousState = 0;
    return THETAZ_PulseEvent(static_cast<HANDLE>(eventHandle)) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

NTSTATUS WINAPI_CALL NTDLL_NtWaitForSingleObject(void* handle, BOOL alertable, void* timeout) {
    (void)alertable;
    DWORD ms = nt_timeout_to_ms(timeout);
    DWORD r = THETAZ_WaitForSingleObject(static_cast<HANDLE>(handle), ms);
    if (r == 0xFFFFFFFF) return STATUS_INVALID_HANDLE;
    return (r == 0x102) ? STATUS_TIMEOUT : STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_NtWaitForMultipleObjects(ULONG count, void** handles, ULONG waitType,
                                                    BOOL alertable, void* timeout) {
    (void)alertable;
    DWORD ms = nt_timeout_to_ms(timeout);
    DWORD r = THETAZ_WaitForMultipleObjects(count, handles, waitType == 0, ms);
    if (r == 0xFFFFFFFF) return STATUS_INVALID_HANDLE;
    if (r == 0x102) return STATUS_TIMEOUT;
    return r;
}

NTSTATUS WINAPI_CALL NTDLL_NtCreateMutant(void* mutantHandle, ULONG access, void* attrs, BOOLEAN initial) {
    (void)access; (void)attrs;
    if (!mutantHandle) return STATUS_INVALID_PARAMETER;
    HANDLE h = THETAZ_CreateMutexW(nullptr, initial != 0, nullptr);
    if (!h) return STATUS_UNSUCCESSFUL;
    *reinterpret_cast<void**>(mutantHandle) = h;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_NtOpenMutant(void* mutantHandle, ULONG access, void* attrs, void* name) {
    (void)access; (void)attrs;
    if (!mutantHandle) return STATUS_INVALID_PARAMETER;
    std::string n = unicode_string_to_utf8(name);
    if (n.empty()) return STATUS_INVALID_PARAMETER;
    std::u16string w;
    utf8_to_utf16le(n, w);
    HANDLE h = THETAZ_OpenMutexW(0x1F0001, FALSE, reinterpret_cast<LPCWSTR>(w.data()));
    if (!h) return STATUS_OBJECT_NAME_NOT_FOUND;
    *reinterpret_cast<void**>(mutantHandle) = h;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_NtReleaseMutant(void* mutantHandle, LONG* previousCount) {
    if (previousCount) *previousCount = 0;
    return THETAZ_ReleaseMutex(static_cast<HANDLE>(mutantHandle)) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

NTSTATUS WINAPI_CALL NTDLL_NtCreateSemaphore(void* semHandle, ULONG access, void* attrs,
                                             LONG initial, LONG maximum, const WCHAR* name) {
    (void)access; (void)attrs;
    if (!semHandle) return STATUS_INVALID_PARAMETER;
    std::string n;
    if (name) n = utf16le_to_utf8(reinterpret_cast<const uint16_t*>(name));
    void* h = reinterpret_cast<void*>(ThetaServer::instance().create_semaphore(
        n, initial, maximum, ThreadManager::instance().get_current_guest_pid()));
    if (!h) return STATUS_UNSUCCESSFUL;
    *reinterpret_cast<void**>(semHandle) = h;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_NtOpenSemaphore(void* semHandle, ULONG access, void* attrs, void* name) {
    (void)access; (void)attrs;
    if (!semHandle) return STATUS_INVALID_PARAMETER;
    *reinterpret_cast<void**>(semHandle) = reinterpret_cast<void*>(
        ThetaServer::instance().create_semaphore("", 1, 1,
                                                 ThreadManager::instance().get_current_guest_pid()));
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_NtReleaseSemaphore(void* semHandle, LONG releaseCount, LONG* prevCount) {
    if (prevCount) *prevCount = 0;
    return ThetaServer::instance().release_semaphore(reinterpret_cast<uint64_t>(semHandle), releaseCount)
        ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

NTSTATUS WINAPI_CALL NTDLL_NtCreateTimer(void* timerHandle, ULONG access, void* attrs, ULONG type) {
    (void)access; (void)attrs;
    if (!timerHandle) return STATUS_INVALID_PARAMETER;
    bool manual = (type & 0x1) != 0; // 0x1 = TIMER_SYNCHRONIZE... manual if 0x1? keep manual default
    void* h = reinterpret_cast<void*>(ThetaServer::instance().create_timer(
        "", manual, ThreadManager::instance().get_current_guest_pid()));
    if (!h) return STATUS_UNSUCCESSFUL;
    *reinterpret_cast<void**>(timerHandle) = h;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_NtOpenTimer(void* timerHandle, ULONG access, void* attrs, void* name) {
    (void)access; (void)attrs; (void)name;
    if (!timerHandle) return STATUS_INVALID_PARAMETER;
    *reinterpret_cast<void**>(timerHandle) = reinterpret_cast<void*>(
        ThetaServer::instance().create_timer("", false,
                                             ThreadManager::instance().get_current_guest_pid()));
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_NtSetTimer(void* timerHandle, void* dueTime, void* timerApc,
                                      void* timerContext, BOOL resume, LONG period, void* state) {
    (void)timerApc; (void)timerContext; (void)resume; (void)state;
    if (!dueTime) return STATUS_INVALID_PARAMETER;
    int64_t d = *static_cast<int64_t*>(dueTime);
    DWORD ms = d < 0 ? static_cast<DWORD>(((-d) + 9999) / 10000) : 0;
    return ThetaServer::instance().set_timer(reinterpret_cast<uint64_t>(timerHandle), ms, period)
        ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

NTSTATUS WINAPI_CALL NTDLL_NtCancelTimer(void* timerHandle, BOOL* state) {
    if (state) *state = FALSE;
    return ThetaServer::instance().cancel_timer(reinterpret_cast<uint64_t>(timerHandle))
        ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

NTSTATUS WINAPI_CALL NTDLL_NtQueryObject(void* objectHandle, ULONG infoClass, void* info,
                                         ULONG len, ULONG* retLen) {
    (void)objectHandle; (void)info; (void)len;
    if (retLen) *retLen = 0;
    // ObjectBasicInformation (0) and ObjectNameInformation (1) are the common
    // probes; apps often ignore the failure and continue.
    return STATUS_INVALID_PARAMETER;
}

NTSTATUS WINAPI_CALL NTDLL_NtCreateFile(void* fileHandle, ULONG access, void* objAttrs,
                                        void* ioStatus, void* allocSize, ULONG fileAttrs,
                                        ULONG shareAccess, ULONG openDisposition, ULONG createOptions,
                                        void* eaBuffer, ULONG eaLength) {
    (void)allocSize; (void)shareAccess; (void)createOptions; (void)eaBuffer; (void)eaLength;
    if (!fileHandle) return STATUS_INVALID_PARAMETER;
    std::string path = unicode_string_to_utf8(objAttrs);
    if (path.empty()) return STATUS_OBJECT_NAME_NOT_FOUND;

    // Map an NT path like \??\C:\... to a host path via the kernel32 helper.
    std::string host;
    if (path.compare(0, 4, "\\??\\") == 0 || path.compare(0, 8, "\\SystemRoot\\") == 0) {
        if (path.compare(0, 4, "\\??\\") == 0) path = path.substr(4);
        else path = std::string("C:\\windows\\system32") + path.substr(12);
        // Strip \??\ drive prefix and lowercase drive for windows_to_linux_path.
        std::string win_path = path;
        host = windows_to_linux_path(win_path);
    } else {
        host = windows_to_linux_path(path);
    }
    if (host.empty()) host = path;

    DWORD disposition = 0;
    switch (openDisposition) {
    case kFileOpen: disposition = 3; break;       // OPEN_EXISTING
    case kFileCreate: disposition = 2; break;     // CREATE_ALWAYS
    case kFileOpenIf: disposition = 4; break;     // OPEN_ALWAYS
    case kFileOverwrite: disposition = 5; break;  // TRUNCATE_EXISTING
    case kFileOverwriteIf: disposition = 4; break;
    default: disposition = 3; break;
    }
    DWORD desired = 0;
    if (access & (kGenericRead | kFileReadData)) desired |= 0x80000000u;
    if (access & (kGenericWrite | kFileWriteData | kFileAppendData)) desired |= 0x40000000u;
    if (!desired) desired = 0x80000000u;

    HANDLE h = THETAZ_CreateFileA(host.c_str(), desired, 1 /*FILE_SHARE_READ*/, nullptr,
                                  disposition, 0x80 /*FILE_ATTRIBUTE_NORMAL*/, nullptr);
    if (h == reinterpret_cast<HANDLE>(-1)) {
        h = nullptr;
    }
    *reinterpret_cast<void**>(fileHandle) = h;
    if (!h) return STATUS_OBJECT_NAME_NOT_FOUND;
    // Fill IO_STATUS_BLOCK {Status@0, Information@8}.
    if (ioStatus) {
        *reinterpret_cast<int64_t*>(ioStatus) = STATUS_SUCCESS;
        *reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(ioStatus) + 8) = 0;
    }
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_NtOpenFile(void* fileHandle, ULONG access, void* objAttrs,
                                      void* ioStatus, ULONG shareAccess, ULONG openOptions) {
    (void)access; (void)shareAccess; (void)openOptions;
    return NTDLL_NtCreateFile(fileHandle, access, objAttrs, ioStatus, nullptr, 0,
                              shareAccess, kFileOpen, 0, nullptr, 0);
}

NTSTATUS WINAPI_CALL NTDLL_NtReadFile(void* fileHandle, void* event, void* apc, void* apcCtx,
                                      void* ioStatus, void* buffer, ULONG length, void* byteOffset, void* key) {
    (void)event; (void)apc; (void)apcCtx; (void)key;
    if (byteOffset) {
        int64_t off = *static_cast<int64_t*>(byteOffset);
        LONG hi = static_cast<LONG>(off >> 32);
        THETAZ_SetFilePointer(static_cast<HANDLE>(fileHandle), static_cast<LONG>(off), &hi, 0);
    }
    DWORD read = 0;
    BOOL ok = THETAZ_ReadFile(static_cast<HANDLE>(fileHandle), buffer, length, &read, nullptr);
    if (ioStatus) {
        *reinterpret_cast<int64_t*>(ioStatus) = STATUS_SUCCESS;
        *reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(ioStatus) + 8) = read;
    }
    return ok ? STATUS_SUCCESS : (read == 0 ? STATUS_END_OF_FILE : STATUS_UNSUCCESSFUL);
}

NTSTATUS WINAPI_CALL NTDLL_NtWriteFile(void* fileHandle, void* event, void* apc, void* apcCtx,
                                       void* ioStatus, void* buffer, ULONG length, void* byteOffset, void* key) {
    (void)event; (void)apc; (void)apcCtx; (void)key;
    if (byteOffset) {
        int64_t off = *static_cast<int64_t*>(byteOffset);
        LONG hi = static_cast<LONG>(off >> 32);
        THETAZ_SetFilePointer(static_cast<HANDLE>(fileHandle), static_cast<LONG>(off), &hi, 0);
    }
    DWORD written = 0;
    BOOL ok = THETAZ_WriteFile(static_cast<HANDLE>(fileHandle), buffer, length, &written, nullptr);
    if (ioStatus) {
        *reinterpret_cast<int64_t*>(ioStatus) = STATUS_SUCCESS;
        *reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(ioStatus) + 8) = written;
    }
    return ok ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

NTSTATUS WINAPI_CALL NTDLL_NtQueryInformationFile(void* fileHandle, void* ioStatus,
                                                  void* info, ULONG len, ULONG infoClass) {
    (void)fileHandle; (void)info; (void)len;
    if (ioStatus) {
        *reinterpret_cast<int64_t*>(ioStatus) = STATUS_SUCCESS;
        *reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(ioStatus) + 8) = 0;
    }
    // FileStandardInformation (5), FilePositionInformation (14), etc.
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_NtQueryAttributesFile(void* objAttrs, void* fileInfo) {
    std::string path = unicode_string_to_utf8(objAttrs);
    if (path.empty()) return STATUS_OBJECT_NAME_NOT_FOUND;
    std::string host = windows_to_linux_path(path);
    if (host.empty()) host = path;
    struct stat st;
    if (::stat(host.c_str(), &st) != 0) return STATUS_OBJECT_NAME_NOT_FOUND;
    if (fileInfo) memset(fileInfo, 0, 40);
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_NtQueryFullAttributesFile(void* objAttrs, void* fileInfo) {
    std::string path = unicode_string_to_utf8(objAttrs);
    if (path.empty()) return STATUS_OBJECT_NAME_NOT_FOUND;
    std::string host = windows_to_linux_path(path);
    if (host.empty()) host = path;
    struct stat st;
    if (::stat(host.c_str(), &st) != 0) return STATUS_OBJECT_NAME_NOT_FOUND;
    if (fileInfo) memset(fileInfo, 0, 40);
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_NtDeviceIoControlFile(void* fileHandle, void* event, void* apc, void* apcCtx,
                                                 void* ioStatus, ULONG ioControlCode, void* inBuf,
                                                 ULONG inLen, void* outBuf, ULONG outLen) {
    (void)fileHandle; (void)event; (void)apc; (void)apcCtx; (void)ioControlCode;
    (void)inBuf; (void)inLen; (void)outBuf; (void)outLen;
    if (ioStatus) {
        *reinterpret_cast<int64_t*>(ioStatus) = STATUS_SUCCESS;
        *reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(ioStatus) + 8) = 0;
    }
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_NtFlushBuffersFile(void* fileHandle, void* ioStatus) {
    (void)fileHandle;
    if (ioStatus) {
        *reinterpret_cast<int64_t*>(ioStatus) = STATUS_SUCCESS;
        *reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(ioStatus) + 8) = 0;
    }
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_NtDuplicateObject(void* sourceProcess, void* sourceHandle,
                                             void* targetProcess, void** targetHandle,
                                             ULONG access, ULONG attrs, ULONG options) {
    (void)sourceProcess; (void)access; (void)attrs;
    if (!targetHandle) return STATUS_INVALID_PARAMETER;
    if (options & 0x2) { // DUPLICATE_CLOSE_SOURCE
        THETAZ_CloseHandle(static_cast<HANDLE>(sourceHandle));
    }
    *targetHandle = targetProcess ? sourceHandle : nullptr;
    return targetProcess ? STATUS_SUCCESS : STATUS_INVALID_HANDLE;
}

NTSTATUS WINAPI_CALL NTDLL_NtOpenProcess(void* processHandle, ULONG access, void* attrs, void* cid) {
    (void)access; (void)attrs;
    if (!processHandle) return STATUS_INVALID_PARAMETER;
    uint32_t pid = 0;
    if (cid) pid = *reinterpret_cast<const uint32_t*>(cid);
    // Process IDs in ThetaZ map 1:1 to handles (handle == pid).
    *reinterpret_cast<void**>(processHandle) = pid ? reinterpret_cast<void*>(static_cast<uintptr_t>(pid)) : nullptr;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_NtOpenThread(void* threadHandle, ULONG access, void* attrs, void* cid) {
    (void)access; (void)attrs;
    if (!threadHandle) return STATUS_INVALID_PARAMETER;
    uint32_t tid = 0;
    if (cid) tid = *reinterpret_cast<const uint32_t*>(cid);
    *reinterpret_cast<void**>(threadHandle) = tid ? reinterpret_cast<void*>(static_cast<uintptr_t>(tid)) : nullptr;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_NtGetContextThread(void* threadHandle, void* context) {
    (void)threadHandle; (void)context;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_NtSetContextThread(void* threadHandle, void* context) {
    (void)threadHandle; (void)context;
    return STATUS_SUCCESS;
}

// ---------------------------------------------------------------------------
// Ldr* loader functions
// ---------------------------------------------------------------------------

NTSTATUS WINAPI_CALL NTDLL_LdrLoadDll(const WCHAR* pathToFile, void* flags, const PUNICODE_STRING moduleName, void** moduleHandle) {
    (void)pathToFile; (void)flags;
    if (!moduleHandle) return STATUS_INVALID_PARAMETER;
    if (!moduleName) return STATUS_INVALID_PARAMETER;
    std::string name = unicode_string_to_utf8(moduleName);
    if (name.empty()) return STATUS_INVALID_PARAMETER;
    if (name.find('.') == std::string::npos) name += ".dll";
    std::u16string w;
    utf8_to_utf16le(name, w);
    HMODULE h = THETAZ_LoadLibraryW(reinterpret_cast<LPCWSTR>(w.data()));
    if (!h) return STATUS_DLL_NOT_FOUND;
    *moduleHandle = h;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_LdrUnloadDll(void* moduleHandle) {
    if (moduleHandle) THETAZ_FreeLibrary(static_cast<HMODULE>(moduleHandle));
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_LdrGetDllHandle(const WCHAR* baseName, void* flags, const PUNICODE_STRING moduleName, void** moduleHandle) {
    (void)flags;
    if (!moduleHandle) return STATUS_INVALID_PARAMETER;
    std::string name;
    if (moduleName) {
        name = unicode_string_to_utf8(moduleName);
    } else if (baseName) {
        size_t n = 0;
        while (baseName[n]) n++;
        name = utf16le_to_utf8(reinterpret_cast<const uint16_t*>(baseName));
    } else {
        return STATUS_INVALID_PARAMETER;
    }
    if (name.empty()) return STATUS_INVALID_PARAMETER;
    if (name.find('.') == std::string::npos) name += ".dll";
    std::u16string w;
    utf8_to_utf16le(name, w);
    HMODULE h = THETAZ_GetModuleHandleW(reinterpret_cast<LPCWSTR>(w.data()));
    if (!h) return STATUS_DLL_NOT_FOUND;
    *moduleHandle = h;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_LdrGetProcedureAddress(void* ModuleHandle, void* procName, USHORT ordinal, void** procAddress) {
    (void)ordinal;
    if (!procAddress) return STATUS_INVALID_PARAMETER;
    void* f = nullptr;
    uintptr_t pn = reinterpret_cast<uintptr_t>(procName);
    if (pn & 0x1) {
        char ord_buf[16];
        snprintf(ord_buf, sizeof(ord_buf), "#%u", static_cast<unsigned>(pn >> 16));
        f = THETAZ_GetProcAddress(static_cast<HMODULE>(ModuleHandle), ord_buf);
    } else {
        f = THETAZ_GetProcAddress(static_cast<HMODULE>(ModuleHandle),
                                  static_cast<const char*>(procName));
    }
    if (!f) return STATUS_NOT_FOUND;
    *procAddress = f;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_LdrAddRefDll(ULONG flags, void* moduleHandle) {
    (void)flags; (void)moduleHandle;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_LdrDisableThreadCalloutsForDll(void* moduleHandle) {
    (void)moduleHandle;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_LdrLockLoaderLock(ULONG flags, ULONG* disposed, void** cookie) {
    (void)flags;
    if (disposed) *disposed = 0;
    if (cookie) *cookie = nullptr;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_LdrUnlockLoaderLock(ULONG flags, void* cookie) {
    (void)flags; (void)cookie;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_LdrGetDllFullName(void* moduleHandle, PUNICODE_STRING fullName) {
    if (!fullName) return STATUS_INVALID_PARAMETER;
    WCHAR buf[260];
    DWORD n = THETAZ_GetModuleFileNameW(static_cast<HMODULE>(moduleHandle), buf, 260);
    if (n == 0) return STATUS_NOT_FOUND;
    void* b = alloc_guest_shared((n + 1) * 2);
    if (!b) return STATUS_NO_MEMORY;
    memcpy(b, buf, (n + 1) * 2);
    write_guest_unicode_string(fullName, static_cast<USHORT>(n * 2),
                               static_cast<USHORT>((n + 1) * 2), b);
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_LdrGetDllPath(const WCHAR* name, ULONG flags, PUNICODE_STRING path) {
    (void)name; (void)flags;
    if (path) write_guest_unicode_string(path, 0, 0, nullptr);
    return STATUS_NOT_FOUND;
}

NTSTATUS WINAPI_CALL NTDLL_LdrSetDllDirectory(const WCHAR* path) {
    (void)path;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_LdrGetDllDirectory(PUNICODE_STRING path) {
    if (path) write_guest_unicode_string(path, 0, 0, nullptr);
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_LdrResolveDelayLoadedAPI(void* parent, void* delayload, void* failureDllHook,
                                                    void* failureSystemHook, void* thunkAddress, void* flags) {
    (void)parent; (void)delayload; (void)failureDllHook; (void)failureSystemHook; (void)flags;
    // Return the thunk address unchanged (already resolved) or NULL to indicate
    // "let the caller handle it". Returning NULL with no hook lets the app's
    // own delay-load machinery take over.
    return thunkAddress ? static_cast<NTSTATUS>(reinterpret_cast<uintptr_t>(thunkAddress)) : 0;
}

NTSTATUS WINAPI_CALL NTDLL_LdrAddDllDirectory(const WCHAR* path, void** cookie) {
    (void)path;
    if (cookie) *cookie = reinterpret_cast<void*>(1);
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_LdrRemoveDllDirectory(void* cookie) {
    (void)cookie;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_LdrSetDefaultDllDirectories(ULONG flags) {
    (void)flags;
    return STATUS_SUCCESS;
}

// ---------------------------------------------------------------------------
// Second-wave ntdll coverage: registry, environment, strings, IPv4/IPv6.
// ---------------------------------------------------------------------------

NTSTATUS WINAPI_CALL NTDLL_NtOpenKey(void* keyHandle, ULONG access, void* objAttrs) {
    (void)access;
    if (keyHandle) *reinterpret_cast<void**>(keyHandle) = nullptr;
    return STATUS_OBJECT_NAME_NOT_FOUND;
}

NTSTATUS WINAPI_CALL NTDLL_NtCreateKey(void* keyHandle, ULONG access, void* objAttrs, ULONG titleIndex,
                                       void* classInfo, ULONG createOptions, void* disposition) {
    (void)access; (void)objAttrs; (void)titleIndex; (void)classInfo; (void)createOptions;
    if (keyHandle) *reinterpret_cast<void**>(keyHandle) = nullptr;
    if (disposition) *reinterpret_cast<ULONG*>(disposition) = 1; // REG_CREATED_NEW_KEY
    return STATUS_OBJECT_NAME_NOT_FOUND;
}

NTSTATUS WINAPI_CALL NTDLL_NtFsControlFile(void* fileHandle, void* event, void* apc, void* apcCtx,
                                           void* ioStatus, ULONG fsctl, void* inBuf, ULONG inLen,
                                           void* outBuf, ULONG outLen) {
    (void)fileHandle; (void)event; (void)apc; (void)apcCtx; (void)fsctl;
    (void)inBuf; (void)inLen; (void)outBuf; (void)outLen;
    if (ioStatus) memset(ioStatus, 0, 16);
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_NtCancelIoFileEx(void* fileHandle, void* ioStatusBlock, void* ioStatus) {
    (void)fileHandle; (void)ioStatusBlock;
    if (ioStatus) memset(ioStatus, 0, 16);
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_NtPowerInformation(ULONG infoLevel, void* inBuf, ULONG inLen,
                                              void* outBuf, ULONG outLen) {
    (void)inBuf; (void)inLen;
    if (infoLevel == 11 /*SystemBatteryState*/ && outBuf && outLen >= 36) {
        memset(outBuf, 0, outLen);
    }
    return STATUS_SUCCESS;
}

void WINAPI_CALL NTDLL_RtlAcquirePebLock() { }
void WINAPI_CALL NTDLL_RtlReleasePebLock() { }

NTSTATUS WINAPI_CALL NTDLL_RtlPcToFileHeader(void* pc, void** base) {
    (void)pc;
    if (base) *base = nullptr;
    return STATUS_NOT_FOUND;
}

NTSTATUS WINAPI_CALL NTDLL_RtlCompareString(const PSTRING s1, const PSTRING s2, BOOLEAN caseInsensitive) {
    std::string a = ansi_string_to_utf8(s1);
    std::string b = ansi_string_to_utf8(s2);
    if (caseInsensitive) {
        std::string la = a, lb = b;
        for (char& c : la) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        for (char& c : lb) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        a = la; b = lb;
    }
    int c = a.compare(b);
    return (c == 0) ? 0 : (c < 0) ? 1 : 2;
}

NTSTATUS WINAPI_CALL NTDLL_RtlEqualUnicodeString(const PUNICODE_STRING s1, const PUNICODE_STRING s2,
                                                 BOOLEAN caseInsensitive) {
    std::string a = unicode_string_to_utf8(s1);
    std::string b = unicode_string_to_utf8(s2);
    if (caseInsensitive) {
        std::string la = a, lb = b;
        for (char& c : la) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        for (char& c : lb) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        a = la; b = lb;
    }
    return a == b;
}

void WINAPI_CALL NTDLL_RtlInitString(PSTRING dest, const char* src) {
    if (!dest) return;
    if (!src) {
        write_guest_unicode_string(dest, 0, 0, nullptr);
        return;
    }
    size_t n = strlen(src);
    write_guest_unicode_string(dest, static_cast<USHORT>(n),
                               static_cast<USHORT>(n + 1),
                               const_cast<char*>(src));
}

BOOL WINAPI_CALL NTDLL_RtlCreateUnicodeString(PUNICODE_STRING dest, const WCHAR* src) {
    if (!dest) return FALSE;
    size_t n = 0;
    if (src) while (src[n]) n++;
    void* buf = alloc_guest_shared((n + 1) * 2);
    if (!buf) return FALSE;
    if (src) memcpy(buf, src, (n + 1) * 2);
    write_guest_unicode_string(dest, static_cast<USHORT>(n * 2),
                               static_cast<USHORT>((n + 1) * 2), buf);
    return TRUE;
}

void WINAPI_CALL NTDLL_RtlCreateEnvironment(BOOLEAN inherit, void** env) {
    (void)inherit;
    if (env) *env = alloc_guest_shared(4096);
}

void WINAPI_CALL NTDLL_RtlDestroyEnvironment(void* env) {
    (void)env;
}

NTSTATUS WINAPI_CALL NTDLL_RtlQueryEnvironmentVariable_U(void* env, const PUNICODE_STRING name,
                                                         PUNICODE_STRING value) {
    (void)env;
    if (!name) return STATUS_INVALID_PARAMETER;
    std::string key = unicode_string_to_utf8(name);
    const char* v = getenv(key.c_str());
    if (!v) return STATUS_VARIABLE_NOT_FOUND;
    std::u16string w;
    utf8_to_utf16le(v, w);
    void* buf = alloc_guest_shared((w.size() + 1) * 2);
    if (!buf) return STATUS_NO_MEMORY;
    memcpy(buf, w.c_str(), (w.size() + 1) * 2);
    if (value) write_guest_unicode_string(value, static_cast<USHORT>(w.size() * 2),
                                          static_cast<USHORT>((w.size() + 1) * 2), buf);
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_RtlSetEnvironmentVariable(void* env, const PUNICODE_STRING name,
                                                     const PUNICODE_STRING value) {
    (void)env;
    if (!name || !value) return STATUS_INVALID_PARAMETER;
    std::string key = unicode_string_to_utf8(name);
    std::string val = unicode_string_to_utf8(value);
    setenv(key.c_str(), val.c_str(), 1);
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_RtlExpandEnvironmentStrings_U(PUNICODE_STRING src, PUNICODE_STRING dest,
                                                         ULONG* len) {
    (void)src; (void)dest;
    if (len) *len = 0;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_RtlGUIDFromString(const PUNICODE_STRING s, void* guid) {
    (void)s;
    if (guid) memset(guid, 0, 16);
    return STATUS_INVALID_PARAMETER;
}

static void ipv4_to_string(uint32_t ip, uint16_t* w, char* a) {
    uint8_t b[4] = { static_cast<uint8_t>(ip & 0xFF), static_cast<uint8_t>((ip >> 8) & 0xFF),
                     static_cast<uint8_t>((ip >> 16) & 0xFF), static_cast<uint8_t>((ip >> 24) & 0xFF) };
    char tmp[24];
    snprintf(tmp, sizeof(tmp), "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
    if (a) { strcpy(a, tmp); return; }
    size_t n = strlen(tmp);
    for (size_t i = 0; i < n; i++) w[i] = static_cast<uint16_t>(tmp[i]);
    w[n] = 0;
}

NTSTATUS WINAPI_CALL NTDLL_RtlIpv4AddressToStringA(void* addr, char* s) {
    ipv4_to_string(*reinterpret_cast<const uint32_t*>(addr), nullptr, s);
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_RtlIpv4AddressToStringW(void* addr, WCHAR* s) {
    ipv4_to_string(*reinterpret_cast<const uint32_t*>(addr), s, nullptr);
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_RtlIpv4StringToAddressA(const char* s, BOOLEAN strict, void* addr, const char** end) {
    (void)strict; (void)end;
    uint32_t a = inet_addr(s);
    if (a == 0xFFFFFFFFu) return STATUS_INVALID_PARAMETER;
    if (addr) *reinterpret_cast<uint32_t*>(addr) = a;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_RtlIpv4StringToAddressW(const WCHAR* s, BOOLEAN strict, void* addr, const WCHAR** end) {
    (void)strict; (void)end;
    std::string a = utf16le_to_utf8(reinterpret_cast<const uint16_t*>(s));
    uint32_t ip = inet_addr(a.c_str());
    if (ip == 0xFFFFFFFFu) return STATUS_INVALID_PARAMETER;
    if (addr) *reinterpret_cast<uint32_t*>(addr) = ip;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_RtlIpv4StringToAddressExW(const WCHAR* s, BOOLEAN strict, void* addr, void* port) {
    (void)strict; (void)port;
    std::string a = utf16le_to_utf8(reinterpret_cast<const uint16_t*>(s));
    uint32_t ip = inet_addr(a.c_str());
    if (ip == 0xFFFFFFFFu) return STATUS_INVALID_PARAMETER;
    if (addr) *reinterpret_cast<uint32_t*>(addr) = ip;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL NTDLL_RtlIpv6StringToAddressA(const char* s, const char** end, void* addr) {
    (void)s; (void)end;
    if (addr) memset(addr, 0, 16);
    return STATUS_INVALID_PARAMETER;
}

NTSTATUS WINAPI_CALL NTDLL_RtlIpv6StringToAddressW(const WCHAR* s, const WCHAR** end, void* addr) {
    (void)s; (void)end;
    if (addr) memset(addr, 0, 16);
    return STATUS_INVALID_PARAMETER;
}

NTSTATUS WINAPI_CALL NTDLL_RtlIpv6StringToAddressExW(const WCHAR* s, void* addr, void* scopeId, void* port) {
    (void)s; (void)scopeId; (void)port;
    if (addr) memset(addr, 0, 16);
    return STATUS_INVALID_PARAMETER;
}

NTSTATUS WINAPI_CALL NTDLL_RtlIpv6AddressToStringExW(void* addr, ULONG scopeId, void* port, WCHAR* s, ULONG* size) {
    (void)addr; (void)scopeId; (void)port; (void)size;
    if (s) s[0] = 0;
    return STATUS_INVALID_PARAMETER;
}


// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void initialize_ntdll_api() {
    // Rtl*
    register_api("ntdll.dll", "RtlAllocateHeap", (void*)NTDLL_RtlAllocateHeap);
    register_api("ntdll.dll", "RtlFreeHeap", (void*)NTDLL_RtlFreeHeap);
    register_api("ntdll.dll", "RtlReAllocateHeap", (void*)NTDLL_RtlReAllocateHeap);
    register_api("ntdll.dll", "RtlSizeHeap", (void*)NTDLL_RtlSizeHeap);
    register_api("ntdll.dll", "RtlCreateHeap", (void*)NTDLL_RtlCreateHeap);
    register_api("ntdll.dll", "RtlDestroyHeap", (void*)NTDLL_RtlDestroyHeap);
    register_api("ntdll.dll", "RtlCompactHeap", (void*)NTDLL_RtlCompactHeap);
    register_api("ntdll.dll", "RtlGetProcessHeaps", (void*)NTDLL_RtlGetProcessHeaps);

    register_api("ntdll.dll", "RtlInitializeCriticalSection", (void*)NTDLL_RtlInitializeCriticalSection);
    register_api("ntdll.dll", "RtlDeleteCriticalSection", (void*)NTDLL_RtlDeleteCriticalSection);
    register_api("ntdll.dll", "RtlEnterCriticalSection", (void*)NTDLL_RtlEnterCriticalSection);
    register_api("ntdll.dll", "RtlLeaveCriticalSection", (void*)NTDLL_RtlLeaveCriticalSection);
    register_api("ntdll.dll", "RtlTryEnterCriticalSection", (void*)NTDLL_RtlTryEnterCriticalSection);
    register_api("ntdll.dll", "RtlInitializeCriticalSectionAndSpinCount", (void*)NTDLL_RtlInitializeCriticalSectionAndSpinCount);
    register_api("ntdll.dll", "RtlSetCriticalSectionSpinCount", (void*)NTDLL_RtlSetCriticalSectionSpinCount);
    register_api("ntdll.dll", "RtlAcquireSRWLockExclusive", (void*)NTDLL_RtlAcquireSRWLockExclusive);
    register_api("ntdll.dll", "RtlAcquireSRWLockShared", (void*)NTDLL_RtlAcquireSRWLockShared);
    register_api("ntdll.dll", "RtlReleaseSRWLockExclusive", (void*)NTDLL_RtlReleaseSRWLockExclusive);
    register_api("ntdll.dll", "RtlReleaseSRWLockShared", (void*)NTDLL_RtlReleaseSRWLockShared);

    register_api("ntdll.dll", "RtlGetCurrentPeb", (void*)NTDLL_RtlGetCurrentPeb);
    register_api("ntdll.dll", "RtlGetCurrentTeb", (void*)NTDLL_RtlGetCurrentTeb);
    register_api("ntdll.dll", "RtlGetVersion", (void*)NTDLL_RtlGetVersion);
    register_api("ntdll.dll", "RtlGetSystemTimePrecise", (void*)NTDLL_RtlGetSystemTimePrecise);
    register_api("ntdll.dll", "RtlNtStatusToDosError", (void*)NTDLL_RtlNtStatusToDosError);
    register_api("ntdll.dll", "RtlGetLastWin32Error", (void*)NTDLL_RtlGetLastWin32Error);
    register_api("ntdll.dll", "RtlSetLastWin32Error", (void*)NTDLL_RtlSetLastWin32Error);

    register_api("ntdll.dll", "RtlZeroMemory", (void*)NTDLL_RtlZeroMemory);
    register_api("ntdll.dll", "RtlFillMemory", (void*)NTDLL_RtlFillMemory);
    register_api("ntdll.dll", "RtlCopyMemory", (void*)NTDLL_RtlCopyMemory);
    register_api("ntdll.dll", "RtlMoveMemory", (void*)NTDLL_RtlMoveMemory);
    register_api("ntdll.dll", "RtlCompareMemory", (void*)NTDLL_RtlCompareMemory);
    register_api("ntdll.dll", "RtlEqualMemory", (void*)NTDLL_RtlEqualMemory);

    register_api("ntdll.dll", "RtlInitUnicodeString", (void*)NTDLL_RtlInitUnicodeString);
    register_api("ntdll.dll", "RtlInitAnsiString", (void*)NTDLL_RtlInitAnsiString);
    register_api("ntdll.dll", "RtlCreateUnicodeStringFromAsciiz", (void*)NTDLL_RtlCreateUnicodeStringFromAsciiz);
    register_api("ntdll.dll", "RtlFreeUnicodeString", (void*)NTDLL_RtlFreeUnicodeString);
    register_api("ntdll.dll", "RtlFreeAnsiString", (void*)NTDLL_RtlFreeAnsiString);
    register_api("ntdll.dll", "RtlUnicodeStringToAnsiString", (void*)NTDLL_RtlUnicodeStringToAnsiString);
    register_api("ntdll.dll", "RtlAnsiStringToUnicodeString", (void*)NTDLL_RtlAnsiStringToUnicodeString);
    register_api("ntdll.dll", "RtlCopyUnicodeString", (void*)NTDLL_RtlCopyUnicodeString);

    register_api("ntdll.dll", "RtlGenRandom", (void*)NTDLL_RtlGenRandom);
    register_api("ntdll.dll", "SystemFunction036", (void*)NTDLL_RtlGenRandom);
    register_api("ntdll.dll", "RtlRandom", (void*)NTDLL_RtlRandom);
    register_api("ntdll.dll", "RtlDowncaseUnicodeChar", (void*)NTDLL_RtlDowncaseUnicodeChar);

    register_api("ntdll.dll", "RtlImageNtHeader", (void*)NTDLL_RtlImageNtHeader);
    register_api("ntdll.dll", "RtlImageDirectoryEntryToData", (void*)NTDLL_RtlImageDirectoryEntryToData);
    register_api("ntdll.dll", "RtlFindExportedRoutineByName", (void*)NTDLL_RtlFindExportedRoutineByName);
    register_api("ntdll.dll", "RtlRaiseException", (void*)NTDLL_RtlRaiseException);
    register_api("ntdll.dll", "RtlRaiseStatus", (void*)NTDLL_RtlRaiseStatus);

    // Nt*
    register_api("ntdll.dll", "NtClose", (void*)NTDLL_NtClose);
    register_api("ntdll.dll", "NtQuerySystemTime", (void*)NTDLL_NtQuerySystemTime);
    register_api("ntdll.dll", "NtQueryPerformanceCounter", (void*)NTDLL_NtQueryPerformanceCounter);
    register_api("ntdll.dll", "NtGetTickCount", (void*)NTDLL_NtGetTickCount);
    register_api("ntdll.dll", "NtDelayExecution", (void*)NTDLL_NtDelayExecution);
    register_api("ntdll.dll", "NtYieldExecution", (void*)NTDLL_NtYieldExecution);
    register_api("ntdll.dll", "NtTerminateProcess", (void*)NTDLL_NtTerminateProcess);
    register_api("ntdll.dll", "NtTerminateThread", (void*)NTDLL_NtTerminateThread);
    register_api("ntdll.dll", "NtCurrentTeb", (void*)NTDLL_NtCurrentTeb);
    register_api("ntdll.dll", "NtQueryInformationProcess", (void*)NTDLL_NtQueryInformationProcess);
    register_api("ntdll.dll", "NtQuerySystemInformation", (void*)NTDLL_NtQuerySystemInformation);
    register_api("ntdll.dll", "NtQueryInformationThread", (void*)NTDLL_NtQueryInformationThread);
    register_api("ntdll.dll", "NtSetInformationThread", (void*)NTDLL_NtSetInformationThread);
    register_api("ntdll.dll", "NtSuspendThread", (void*)NTDLL_NtSuspendThread);
    register_api("ntdll.dll", "NtResumeThread", (void*)NTDLL_NtResumeThread);
    register_api("ntdll.dll", "NtQueryVirtualMemory", (void*)NTDLL_NtQueryVirtualMemory);
    register_api("ntdll.dll", "NtProtectVirtualMemory", (void*)NTDLL_NtProtectVirtualMemory);
    register_api("ntdll.dll", "NtAllocateVirtualMemory", (void*)NTDLL_NtAllocateVirtualMemory);
    register_api("ntdll.dll", "NtFreeVirtualMemory", (void*)NTDLL_NtFreeVirtualMemory);
    register_api("ntdll.dll", "NtCreateEvent", (void*)NTDLL_NtCreateEvent);
    register_api("ntdll.dll", "NtOpenEvent", (void*)NTDLL_NtOpenEvent);
    register_api("ntdll.dll", "NtSetEvent", (void*)NTDLL_NtSetEvent);
    register_api("ntdll.dll", "NtResetEvent", (void*)NTDLL_NtResetEvent);
    register_api("ntdll.dll", "NtPulseEvent", (void*)NTDLL_NtPulseEvent);
    register_api("ntdll.dll", "NtWaitForSingleObject", (void*)NTDLL_NtWaitForSingleObject);
    register_api("ntdll.dll", "NtWaitForMultipleObjects", (void*)NTDLL_NtWaitForMultipleObjects);
    register_api("ntdll.dll", "NtCreateMutant", (void*)NTDLL_NtCreateMutant);
    register_api("ntdll.dll", "NtOpenMutant", (void*)NTDLL_NtOpenMutant);
    register_api("ntdll.dll", "NtReleaseMutant", (void*)NTDLL_NtReleaseMutant);
    register_api("ntdll.dll", "NtCreateSemaphore", (void*)NTDLL_NtCreateSemaphore);
    register_api("ntdll.dll", "NtOpenSemaphore", (void*)NTDLL_NtOpenSemaphore);
    register_api("ntdll.dll", "NtReleaseSemaphore", (void*)NTDLL_NtReleaseSemaphore);
    register_api("ntdll.dll", "NtCreateTimer", (void*)NTDLL_NtCreateTimer);
    register_api("ntdll.dll", "NtOpenTimer", (void*)NTDLL_NtOpenTimer);
    register_api("ntdll.dll", "NtSetTimer", (void*)NTDLL_NtSetTimer);
    register_api("ntdll.dll", "NtCancelTimer", (void*)NTDLL_NtCancelTimer);
    register_api("ntdll.dll", "NtQueryObject", (void*)NTDLL_NtQueryObject);
    register_api("ntdll.dll", "NtCreateFile", (void*)NTDLL_NtCreateFile);
    register_api("ntdll.dll", "NtOpenFile", (void*)NTDLL_NtOpenFile);
    register_api("ntdll.dll", "NtReadFile", (void*)NTDLL_NtReadFile);
    register_api("ntdll.dll", "NtWriteFile", (void*)NTDLL_NtWriteFile);
    register_api("ntdll.dll", "NtQueryInformationFile", (void*)NTDLL_NtQueryInformationFile);
    register_api("ntdll.dll", "NtQueryAttributesFile", (void*)NTDLL_NtQueryAttributesFile);
    register_api("ntdll.dll", "NtQueryFullAttributesFile", (void*)NTDLL_NtQueryFullAttributesFile);
    register_api("ntdll.dll", "NtDeviceIoControlFile", (void*)NTDLL_NtDeviceIoControlFile);
    register_api("ntdll.dll", "NtFlushBuffersFile", (void*)NTDLL_NtFlushBuffersFile);
    register_api("ntdll.dll", "NtDuplicateObject", (void*)NTDLL_NtDuplicateObject);
    register_api("ntdll.dll", "NtOpenProcess", (void*)NTDLL_NtOpenProcess);
    register_api("ntdll.dll", "NtOpenThread", (void*)NTDLL_NtOpenThread);
    register_api("ntdll.dll", "NtGetContextThread", (void*)NTDLL_NtGetContextThread);
    register_api("ntdll.dll", "NtSetContextThread", (void*)NTDLL_NtSetContextThread);

    // Ldr*
    register_api("ntdll.dll", "LdrLoadDll", (void*)NTDLL_LdrLoadDll);
    register_api("ntdll.dll", "LdrUnloadDll", (void*)NTDLL_LdrUnloadDll);
    register_api("ntdll.dll", "LdrGetDllHandle", (void*)NTDLL_LdrGetDllHandle);
    register_api("ntdll.dll", "LdrGetProcedureAddress", (void*)NTDLL_LdrGetProcedureAddress);
    register_api("ntdll.dll", "LdrAddRefDll", (void*)NTDLL_LdrAddRefDll);
    register_api("ntdll.dll", "LdrDisableThreadCalloutsForDll", (void*)NTDLL_LdrDisableThreadCalloutsForDll);
    register_api("ntdll.dll", "LdrLockLoaderLock", (void*)NTDLL_LdrLockLoaderLock);
    register_api("ntdll.dll", "LdrUnlockLoaderLock", (void*)NTDLL_LdrUnlockLoaderLock);
    register_api("ntdll.dll", "LdrGetDllFullName", (void*)NTDLL_LdrGetDllFullName);
    register_api("ntdll.dll", "LdrGetDllPath", (void*)NTDLL_LdrGetDllPath);
    register_api("ntdll.dll", "LdrSetDllDirectory", (void*)NTDLL_LdrSetDllDirectory);
    register_api("ntdll.dll", "LdrGetDllDirectory", (void*)NTDLL_LdrGetDllDirectory);
    register_api("ntdll.dll", "LdrResolveDelayLoadedAPI", (void*)NTDLL_LdrResolveDelayLoadedAPI);
    register_api("ntdll.dll", "LdrAddDllDirectory", (void*)NTDLL_LdrAddDllDirectory);
    register_api("ntdll.dll", "LdrRemoveDllDirectory", (void*)NTDLL_LdrRemoveDllDirectory);
    register_api("ntdll.dll", "LdrSetDefaultDllDirectories", (void*)NTDLL_LdrSetDefaultDllDirectories);

    // Second wave
    register_api("ntdll.dll", "NtOpenKey", (void*)NTDLL_NtOpenKey);
    register_api("ntdll.dll", "NtCreateKey", (void*)NTDLL_NtCreateKey);
    register_api("ntdll.dll", "NtFsControlFile", (void*)NTDLL_NtFsControlFile);
    register_api("ntdll.dll", "NtCancelIoFileEx", (void*)NTDLL_NtCancelIoFileEx);
    register_api("ntdll.dll", "NtPowerInformation", (void*)NTDLL_NtPowerInformation);
    register_api("ntdll.dll", "RtlAcquirePebLock", (void*)NTDLL_RtlAcquirePebLock);
    register_api("ntdll.dll", "RtlReleasePebLock", (void*)NTDLL_RtlReleasePebLock);
    register_api("ntdll.dll", "RtlPcToFileHeader", (void*)NTDLL_RtlPcToFileHeader);
    register_api("ntdll.dll", "RtlCompareString", (void*)NTDLL_RtlCompareString);
    register_api("ntdll.dll", "RtlEqualUnicodeString", (void*)NTDLL_RtlEqualUnicodeString);
    register_api("ntdll.dll", "RtlInitString", (void*)NTDLL_RtlInitString);
    register_api("ntdll.dll", "RtlCreateUnicodeString", (void*)NTDLL_RtlCreateUnicodeString);
    register_api("ntdll.dll", "RtlCreateEnvironment", (void*)NTDLL_RtlCreateEnvironment);
    register_api("ntdll.dll", "RtlDestroyEnvironment", (void*)NTDLL_RtlDestroyEnvironment);
    register_api("ntdll.dll", "RtlQueryEnvironmentVariable_U", (void*)NTDLL_RtlQueryEnvironmentVariable_U);
    register_api("ntdll.dll", "RtlSetEnvironmentVariable", (void*)NTDLL_RtlSetEnvironmentVariable);
    register_api("ntdll.dll", "RtlExpandEnvironmentStrings_U", (void*)NTDLL_RtlExpandEnvironmentStrings_U);
    register_api("ntdll.dll", "RtlGUIDFromString", (void*)NTDLL_RtlGUIDFromString);
    register_api("ntdll.dll", "RtlIpv4AddressToStringA", (void*)NTDLL_RtlIpv4AddressToStringA);
    register_api("ntdll.dll", "RtlIpv4AddressToStringW", (void*)NTDLL_RtlIpv4AddressToStringW);
    register_api("ntdll.dll", "RtlIpv4StringToAddressA", (void*)NTDLL_RtlIpv4StringToAddressA);
    register_api("ntdll.dll", "RtlIpv4StringToAddressW", (void*)NTDLL_RtlIpv4StringToAddressW);
    register_api("ntdll.dll", "RtlIpv4StringToAddressExW", (void*)NTDLL_RtlIpv4StringToAddressExW);
    register_api("ntdll.dll", "RtlIpv6StringToAddressA", (void*)NTDLL_RtlIpv6StringToAddressA);
    register_api("ntdll.dll", "RtlIpv6StringToAddressW", (void*)NTDLL_RtlIpv6StringToAddressW);
    register_api("ntdll.dll", "RtlIpv6StringToAddressExW", (void*)NTDLL_RtlIpv6StringToAddressExW);
    register_api("ntdll.dll", "RtlIpv6AddressToStringExW", (void*)NTDLL_RtlIpv6AddressToStringExW);
}

} // namespace ThetaZ
