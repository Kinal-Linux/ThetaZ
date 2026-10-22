# ThetaZ API Coverage Plan — 64-bit Office Apps

## Executive Summary

The Click-to-Run Office apps (WINWORD, EXCEL, POWERPNT, OUTLOOK, MSACCESS, MSPUB, ONENOTE) are **PE64** (64-bit). ThetaZ's 64-bit path loads the image and calls entry directly via host ms_abi, but the API surface is nearly empty. WINWORD exits with `ExitProcess(1)` because:
1. Only `kernel32.dll` has ~346 functions in the API registry
2. `kernelbase.dll`, `ntdll.dll`, `user32.dll`, `gdi32.dll`, `advapi32.dll`, `ole32.dll`, `shell32.dll`, `msvcrt.dll`, `ucrtbase.dll`, `vcruntime140.dll` are either missing or have minimal coverage
3. WINWORD imports 1,300+ functions — most unresolved → app fails early

This plan documents the exact file edits needed to implement the missing API surface. Each section lists: target DLL, functions to add, exact file locations, registration calls, signature-table entries, and test commands.

---

## Phase 1: Core Kernel32 (Priority: WINWORD early init)

**Target**: Complete kernel32.dll coverage for WINWORD's early init path (~14 unresolved after builtin-DLL fix)

### Files to Edit:
1. `core/api/kernel32_runtime.cpp` — add function implementations
2. `core/api/kernel32.h` — add declarations
3. `core/api/kernel32_runtime.cpp` — register in `initialize_kernel32_runtime()`
4. `core/x86/api_dispatch.cpp` — add signature-table entries

### Functions to Add (from WINWORD's unresolved list):

| Function | Args | Calling Convention | Return | Location |
|----------|------|-------------------|--------|----------|
| `Beep` | 2 (DWORD, DWORD) | stdcall | BOOL | kernel32_runtime.cpp |
| `DuplicateHandle` | 7 | stdcall | BOOL | kernel32_runtime.cpp |
| `FindFirstFileA` | 2 | stdcall | HANDLE | kernel32_runtime.cpp |
| `FindNextFileA` | 2 | stdcall | BOOL | kernel32_runtime.cpp |
| `FreeEnvironmentStringsA` | 1 | stdcall | BOOL | kernel32_runtime.cpp |
| `GetEnvironmentStringsA` | 0 | stdcall | LPSTR | kernel32_runtime.cpp |
| `GetLocaleInfoA` | 4 | stdcall | int | kernel32_runtime.cpp |
| `GetNumberOfConsoleInputEvents` | 1 | stdcall | BOOL | kernel32_runtime.cpp |
| `GetStringTypeExA` | 4 | stdcall | BOOL | kernel32_runtime.cpp |
| `HeapWalk` | 3 | stdcall | BOOL | kernel32_runtime.cpp |
| `RtlUnwindEx` | 6 | stdcall | void | kernel32_runtime.cpp (forward to ntdll impl) |
| `CreateEventW` | 4 | stdcall | HANDLE | kernel32_runtime.cpp |
| `CreateThread` | 6 | stdcall | HANDLE | kernel32_runtime.cpp |
| `DeleteCriticalSection` | 1 | stdcall | void | kernel32_runtime.cpp |
| `EnterCriticalSection` | 1 | stdcall | void | kernel32_runtime.cpp |
| `FileTimeToSystemTime` | 2 | stdcall | BOOL | kernel32_runtime.cpp |
| `FindResourceW` | 3 | stdcall | HRSRC | kernel32_runtime.cpp |
| `FreeResource` | 1 | stdcall | BOOL | kernel32_runtime.cpp |
| `GetDateFormatW` | 5 | stdcall | int | kernel32_runtime.cpp |
| `GetLocalTime` | 1 | stdcall | void | kernel32_runtime.cpp |
| `GetLocaleInfoW` | 4 | stdcall | int | kernel32_runtime.cpp |
| `GetProcAddress` | 2 | stdcall | FARPROC | kernel32_runtime.cpp (already exists, check) |
| `GetThreadLocale` | 0 | stdcall | LCID | kernel32_runtime.cpp |
| `GetTickCount` | 0 | stdcall | DWORD | kernel32_runtime.cpp (already exists) |
| `GlobalAlloc` | 2 | stdcall | HGLOBAL | kernel32_runtime.cpp |
| `GlobalFree` | 1 | stdcall | HGLOBAL | kernel32_runtime.cpp |
| `HeapAlloc` | 3 | stdcall | LPVOID | kernel32_runtime.cpp |
| `HeapFree` | 3 | stdcall | BOOL | kernel32_runtime.cpp |
| `HeapReAlloc` | 4 | stdcall | LPVOID | kernel32_runtime.cpp |
| `InitializeCriticalSectionEx` | 3 | stdcall | void | kernel32_runtime.cpp |
| `LeaveCriticalSection` | 1 | stdcall | void | kernel32_runtime.cpp |
| `LoadLibraryW` | 1 | stdcall | HMODULE | kernel32_runtime.cpp (already exists) |
| `LoadResource` | 2 | stdcall | HGLOBAL | kernel32_runtime.cpp |
| `LocalAlloc` | 2 | stdcall | HLOCAL | kernel32_runtime.cpp |
| `LocalFree` | 1 | stdcall | HLOCAL | kernel32_runtime.cpp |
| `LocalReAlloc` | 3 | stdcall | HLOCAL | kernel32_runtime.cpp |
| `LockResource` | 1 | stdcall | LPVOID | kernel32_runtime.cpp |
| `MulDiv` | 3 | stdcall | int | kernel32_runtime.cpp |
| `MultiByteToWideChar` | 6 | stdcall | int | kernel32_runtime.cpp |
| `SetEvent` | 1 | stdcall | BOOL | kernel32_runtime.cpp |
| `SizeofResource` | 2 | stdcall | DWORD | kernel32_runtime.cpp |
| `StrCmpIW` | 2 | stdcall | int | kernel32_runtime.cpp (shlwapi forward) |
| `StrCmpNIW` | 3 | stdcall | int | kernel32_runtime.cpp (shlwapi forward) |
| `SystemTimeToFileTime` | 2 | stdcall | BOOL | kernel32_runtime.cpp |
| `WaitForSingleObject` | 2 | stdcall | DWORD | kernel32_runtime.cpp |
| `WideCharToMultiByte` | 8 | stdcall | int | kernel32_runtime.cpp |
| `lstrcmpW` | 2 | stdcall | int | kernel32_runtime.cpp (shlwapi forward) |
| `lstrcmpiW` | 2 | stdcall | int | kernel32_runtime.cpp (shlwapi forward) |

### Signature-Table Entries (core/x86/api_dispatch.cpp):

```cpp
// Add to kSignatures array in api_dispatch.cpp
ST("kernel32.dll", "Beep", 2),
ST("kernel32.dll", "DuplicateHandle", 7),
ST("kernel32.dll", "FindFirstFileA", 2),
ST("kernel32.dll", "FindNextFileA", 2),
ST("kernel32.dll", "FreeEnvironmentStringsA", 1),
ST("kernel32.dll", "GetEnvironmentStringsA", 0),
ST("kernel32.dll", "GetLocaleInfoA", 4),
ST("kernel32.dll", "GetNumberOfConsoleInputEvents", 1),
ST("kernel32.dll", "GetStringTypeExA", 4),
ST("kernel32.dll", "HeapWalk", 3),
ST("kernel32.dll", "RtlUnwindEx", 6),
// ... continue for all above
```

### Registration Calls (kernel32_runtime.cpp:initialize_kernel32_runtime):

```cpp
register_api("kernel32.dll", "Beep", (void*)WINAPI_Beep);
register_api("kernel32.dll", "DuplicateHandle", (void*)WINAPI_DuplicateHandle);
register_api("kernel32.dll", "FindFirstFileA", (void*)WINAPI_FindFirstFileA);
// ... continue for all above
```

### Header Declarations (kernel32.h):

```cpp
BOOL WINAPI_CALL WINAPI_Beep(DWORD dwFreq, DWORD dwDuration);
BOOL WINAPI_CALL WINAPI_DuplicateHandle(HANDLE hSourceProcessHandle, HANDLE hSourceHandle,
                                        HANDLE hTargetProcessHandle, LPHANDLE lpTargetHandle,
                                        DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwOptions);
// ... etc for all functions
```

---

## Phase 2: Kernelbase.dll (Priority: WINWORD imports heavily from here)

**Target**: Add kernelbase.dll as builtin DLL with API registry coverage

### Files to Edit:
1. `core/api/kernel32_runtime.cpp` — add implementations (can forward to kernel32 where appropriate)
2. `core/api/kernel32.h` — add declarations
3. `core/loader/loader.h` — add `kernelbase.dll` to builtin_dlls set in `try_load_from_thetaz_prefix`
3. `core/loader/main.cpp` — add `kernelbase.dll` to builtin_dlls set
4. `core/api/kernel32_runtime.cpp` — register in `initialize_kernel32_runtime()` with dll="kernelbase.dll"

### Key Functions (from WINWORD unresolved):
| Function | Forward To |
|----------|------------|
| `CreateCompatibleBitmap` | gdi32 |
| `CreateDIBSection` | gdi32 |
| `CreateFontIndirectW` | gdi32 |
| `ExtTextOutW` | gdi32 |
| `LineTo` | gdi32 |
| `MoveToEx` | gdi32 |
| `Polygon` | gdi32 |
| `Rectangle` | gdi32 |
| `StretchBlt` | gdi32 |
| `DelayLoadFailureHook` | kernel32 (stub) |
| `FindResourceA` | kernel32 |
| `GlobalAddAtomW` | kernel32 |
| `GlobalDeleteAtom` | kernel32 |
| `IsBadCodePtr` | kernel32 |
| `IsBadStringPtrA` | kernel32 |
| `IsBadWritePtr` | kernel32 |
| `CloseHandle` | kernel32 |
| `CompareStringA` | kernel32 |
| `CreateEventW` | kernel32 |
| `CreateThread` | kernel32 |
| `DeleteCriticalSection` | kernel32 |
| `EnterCriticalSection` | kernel32 |
| `FileTimeToSystemTime` | kernel32 |
| `FindResourceW` | kernel32 |
| `FreeResource` | kernel32 |
| `GetDateFormatW` | kernel32 |
| `GetLocalTime` | kernel32 |
| `GetLocaleInfoW` | kernel32 |
| `GetProcAddress` | kernel32 |
| `GetThreadLocale` | kernel32 |
| `GetTickCount` | kernel32 |
| `GlobalAlloc` | kernel32 |
| `GlobalFree` | kernel32 |
| `HeapAlloc` | kernel32 |
| `HeapFree` | kernel32 |
| `HeapReAlloc` | kernel32 |
| `InitializeCriticalSectionEx` | kernel32 |
| `LeaveCriticalSection` | kernel32 |
| `LoadLibraryW` | kernel32 |
| `LoadResource` | kernel32 |
| `LocalAlloc` | kernel32 |
| `LocalFree` | kernel32 |
| `LocalReAlloc` | kernel32 |
| `LockResource` | kernel32 |
| `MulDiv` | kernel32 |
| `MultiByteToWideChar` | kernel32 |
| `SetEvent` | kernel32 |
| `SizeofResource` | kernel32 |
| `StrCmpIW` | shlwapi |
| `StrCmpNIW` | shlwapi |
| `SystemTimeToFileTime` | kernel32 |
| `WaitForSingleObject` | kernel32 |
| `WideCharToMultiByte` | kernel32 |
| `lstrcmpW` | shlwapi |
| `lstrcmpiW` | shlwapi |

**Implementation Strategy**: For kernelbase.dll, register the SAME function pointers as kernel32.dll but with dll="kernelbase.dll". This is correct because on Windows, kernelbase.dll forwards to kernel32.dll.

```cpp
// In initialize_kernel32_runtime():
register_api("kernelbase.dll", "Beep", (void*)WINAPI_Beep);
register_api("kernelbase.dll", "DuplicateHandle", (void*)WINAPI_DuplicateHandle);
// ... etc
```

Also add to builtin_dlls set in:
- `core/loader/main.cpp:try_load_from_thetaz_prefix` (the `builtin_dlls` unordered_set)
- `core/loader/main.cpp:process_imports` (the `is_builtin_dll` lambda)

---

## Phase 3: Ntdll.dll (Priority: WINWORD core loader)

**Target**: Add ntdll.dll as builtin DLL

### Files to Edit:
1. `core/api/ntdll_api.cpp` — NEW FILE for ntdll implementations
2. `core/api/ntdll_api.h` — NEW FILE for declarations
3. `core/api/api.h` — add `initialize_ntdll_layer()` declaration
4. `core/api/api.cpp` — implement `initialize_ntdll_layer()`, call from `initialize_api_layer()`
5. `core/loader/main.cpp` — add `ntdll.dll` to builtin_dlls set
6. `core/loader/loader.h` — add `ntdll.dll` to builtin_dlls set in `try_load_from_thetaz_prefix`

### Key Functions (from WINWORD unresolved):
| Function | Args | Convention |
|----------|------|------------|
| `RtlUnwindEx` | 6 | stdcall |
| `NtClose` | 1 | stdcall |
| `NtCreateKey` | 7 | stdcall |
| `NtOpenKey` | 3 | stdcall |
| `RtlInitUnicodeString` | 2 | stdcall |
| `RtlNtStatusToDosError` | 1 | stdcall |
| `RtlCreateUnicodeStringFromAsciiz` | 2 | stdcall |
| `RtlFreeUnicodeString` | 1 | stdcall |
| `RtlGUIDFromString` | 2 | stdcall |
| `RtlIpv4StringToAddressA` | 2 | stdcall |
| `RtlIpv6StringToAddressExW` | 2 | stdcall |
| `RtlIpv6StringToAddressW` | 2 | stdcall |
| `RtlNtStatusToDosError` | 1 | stdcall |
| `NtQuerySystemTime` | 1 | stdcall |
| `NtQueryVirtualMemory` | 6 | stdcall |
| `NtCreateFile` | 11 | stdcall |
| `NtReadFile` | 9 | stdcall |
| `NtWriteFile` | 9 | stdcall |
| `NtAllocateVirtualMemory` | 6 | stdcall |
| `NtFreeVirtualMemory` | 4 | stdcall |
| `NtQueryInformationFile` | 5 | stdcall |
| `NtQuerySystemInformation` | 4 | stdcall |
| `LdrGetDllHandle` | 4 | stdcall |
| `LdrLoadDll` | 4 | stdcall |
| `LdrUnloadDll` | 1 | stdcall |
| `LdrGetProcedureAddress` | 4 | stdcall |
| `RtlAllocateHeap` | 3 | stdcall |
| `RtlFreeHeap` | 3 | stdcall |
| `RtlReAllocateHeap` | 4 | stdcall |
| `RtlCreateUnicodeString` | 2 | stdcall |
| `RtlFreeUnicodeString` | 1 | stdcall |
| `RtlInitAnsiString` | 2 | stdcall |
| `RtlAnsiStringToUnicodeString` | 3 | stdcall |
| `RtlUnicodeStringToAnsiString` | 3 | stdcall |
| `RtlCompareString` | 3 | stdcall |
| `RtlEqualString` | 3 | stdcall |
| `RtlCopyUnicodeString` | 2 | stdcall |
| `RtlAppendUnicodeStringToString` | 2 | stdcall |
| `RtlAppendUnicodeToString` | 2 | stdcall |
| `RtlInitUnicodeString` | 2 | stdcall |
| `RtlFreeUnicodeString` | 1 | stdcall |
| `RtlCreateUnicodeStringFromAsciiz` | 2 | stdcall |
| `RtlNtStatusToDosError` | 1 | stdcall |

**Implementation**: Stub implementations returning appropriate NTSTATUS (e.g., `STATUS_NOT_IMPLEMENTED` = 0xC0000002) for now. Real implementations can be added incrementally.

```cpp
// ntdll_api.cpp
NTSTATUS WINAPI_CALL WINAPI_RtlUnwindEx(
    PVOID TargetFrame, PVOID TargetIp, PEXCEPTION_RECORD ExceptionRecord,
    PVOID ReturnValue, PCONTEXT ContextRecord, PUNWIND_HISTORY_TABLE HistoryTable) {
    return STATUS_NOT_IMPLEMENTED;
}
// ... etc
```

```cpp
// ntdll_api.h
NTSTATUS WINAPI_CALL WINAPI_RtlUnwindEx(...);
// ... etc
```

```cpp
// api.cpp
void initialize_ntdll_layer() {
    register_api("ntdll.dll", "RtlUnwindEx", (void*)WINAPI_RtlUnwindEx);
    register_api("ntdll.dll", "NtClose", (void*)WINAPI_NtClose);
    // ... etc
}
```

```cpp
// In initialize_api_layer():
initialize_ntdll_layer();
```

---

## Phase 4: User32.dll / Gdi32.dll / Advapi32.dll / Ole32.dll / Shell32.dll (Priority: WINWORD GUI)

These are massive DLLs. Implement incrementally by category:

### 4a: User32 - Window/Message/Input
**Priority Functions** (from WINWORD unresolved):
```
CreateWindowExW, DefWindowProcW, DispatchMessageW, GetMessageW, PeekMessageW, PostMessageW,
RegisterClassW, RegisterClassExW, RegisterWindowMessageW, SendMessageW, SetWindowLongPtrW,
SetWindowPos, ShowWindow, UpdateWindow, GetWindowRect, GetClientRect, GetWindowLongPtrW,
GetWindowLongW, SetWindowTextW, GetWindowTextW, GetWindowTextLengthW, GetWindow,
GetParent, GetAncestor, GetTopWindow, GetNextDlgTabItem, GetDlgItem, GetDlgItemInt,
GetDlgItemTextW, SetDlgItemInt, SetDlgItemTextW, GetClassInfoW, GetClassLongPtrW,
GetClassNameW, GetCursorPos, SetCursor, LoadCursorW, LoadIconW, LoadImageW,
CreateDialogIndirectParamW, DialogBoxParamW, DialogBoxIndirectParamW, EndDialog,
GetDesktopWindow, GetShellWindow, GetActiveWindow, GetForegroundWindow, SetForegroundWindow,
GetFocus, SetFocus, GetCapture, SetCapture, ReleaseCapture, GetAsyncKeyState, GetKeyState,
GetKeyboardState, SetKeyboardState, MapVirtualKeyW, MapVirtualKeyExW,
GetSystemMetrics, SystemParametersInfoW, GetDoubleClickTime, GetMessagePos, GetMessageTime,
MessageBoxW, MsgWaitForMultipleObjects, MsgWaitForMultipleObjectsEx,
EnableWindow, IsWindowEnabled, IsWindowVisible, ShowWindowAsync, UpdateWindow,
InvalidateRect, ValidateRect, RedrawWindow, InvalidateRgn, UpdateWindow,
BeginPaint, EndPaint, GetDC, GetDCEx, GetWindowDC, ReleaseDC, GetDCOrgEx,
DrawTextW, DrawTextExW, TextOutW, ExtTextOutW, TabbedTextOutW,
GetTextExtentPoint32W, GetTextExtentPointW, GetTextMetricsW, GetCharWidth32W,
GetCharABCWidthsW, GetTextFaceW, GetTextFaceA, GetFontData, GetOutlineTextMetricsW,
CreateFontW, CreateFontIndirectW, DeleteObject, SelectObject, GetStockObject,
CreateSolidBrush, CreateHatchBrush, CreatePatternBrush, CreateDIBPatternBrushPt,
CreateBrushIndirect, CreatePen, CreatePenIndirect, ExtCreatePen, DeleteObject,
SetBkMode, SetBkColor, SetTextColor, SetTextAlign, SetMapMode, SetWindowExtEx,
SetViewportExtEx, SetWindowOrgEx, SetViewportOrgEx, SetBkMode, SetBkColor,
SetTextColor, SetTextAlign, SetTextCharacterExtra, GetTextCharacterExtra,
GetTextAlign, GetBkMode, GetBkColor, GetTextColor, GetMapMode, GetWindowExtEx,
GetViewportExtEx, GetWindowOrgEx, GetViewportOrgEx, GetDeviceCaps,
CreateCompatibleDC, CreateCompatibleBitmap, CreateDIBSection, DeleteDC, DeleteObject,
GetObjectW, GetObjectA, GetBitmapBits, SetBitmapBits, GetDIBits, SetDIBits,
StretchBlt, StretchDIBits, BitBlt, PatBlt, Rectangle, RoundRect, Ellipse, Chord, Pie,
PolyBezier, Polyline, Polygon, PolyPolygon, MoveToEx, LineTo, PolylineTo, Polyline,
Arc, ArcTo, AngleArc, PolyDraw, PolyPolyline, PolyBezierTo, ExtFloodFill,
FillRect, FillRgn, FrameRect, FrameRgn, InvertRect, InvertRgn, PaintRgn,
ScrollWindow, ScrollWindowEx, ScrollDC, GetScrollInfo, SetScrollInfo, GetScrollPos,
SetScrollPos, GetScrollRange, SetScrollRange, ShowScrollBar, EnableScrollBar,
GetScrollBarInfo, DrawEdge, DrawFocusRect, DrawFrameControl, DrawIcon, DrawIconEx,
DrawStateW, DrawTextW, DrawTextExW, TabbedTextOutW, ExtTextOutW, GrayStringW,
TabbedTextOutW, GetTabbedTextExtentW, DrawTextExW, DrawTextW, ExtTextOutW
```

### 4b: Gdi32 - Graphics
See above (many overlap with user32)

### 4c: Advapi32 - Registry/Security
```
RegOpenKeyA, RegOpenKeyW, RegOpenKeyExA, RegOpenKeyExW, RegCreateKeyA, RegCreateKeyW,
RegCreateKeyExA, RegCreateKeyExW, RegCloseKey, RegDeleteKeyA, RegDeleteKeyW,
RegDeleteTreeW, RegDeleteValueA, RegDeleteValueW, RegEnumKeyA, RegEnumKeyW,
RegEnumKeyExA, RegEnumKeyExW, RegEnumValueA, RegEnumValueW, RegQueryInfoKeyA,
RegQueryInfoKeyW, RegQueryValueA, RegQueryValueW, RegQueryValueExA, RegQueryValueExW,
RegSetValueA, RegSetValueW, RegSetValueExA, RegSetValueExW, RegGetValueW,
RegGetValueA, RegGetValueEx, RegOverridePredefKey, RegDisablePredefinedCache,
RegDisableReflectionKey, RegEnableReflectionKey, RegQueryReflectionKey,
RegConnectRegistryA, RegConnectRegistryW, RegLoadKeyA, RegLoadKeyW, RegUnLoadKey,
RegSaveKeyA, RegSaveKeyW, RegRestoreKeyA, RegRestoreKeyW, RegReplaceKeyA, RegReplaceKeyW,
RegNotifyChangeKeyValue, RegFlushKey, RegSetKeySecurity, RegGetKeySecurity,
GetSecurityDescriptorDacl, GetSecurityDescriptorSacl, GetSecurityDescriptorOwner,
GetSecurityDescriptorGroup, GetSecurityDescriptorControl, GetSecurityDescriptorLength,
SetSecurityDescriptorDacl, SetSecurityDescriptorSacl, SetSecurityDescriptorOwner,
SetSecurityDescriptorGroup, SetSecurityDescriptorControl, InitializeSecurityDescriptor,
IsValidSecurityDescriptor, GetFileSecurityA, GetFileSecurityW, SetFileSecurityA,
SetFileSecurityW, GetKernelObjectSecurity, SetKernelObjectSecurity,
GetPrivateObjectSecurity, SetPrivateObjectSecurity, GetUserObjectSecurity,
SetUserObjectSecurity, GetSecurityDescriptorDacl, GetSecurityDescriptorSacl,
InitializeAcl, AddAccessAllowedAce, AddAccessAllowedAceEx, AddAccessDeniedAce,
AddAccessDeniedAceEx, AddAccessAllowedObjectAce, AddAccessDeniedObjectAce,
DeleteAce, GetAce, GetAclInformation, SetAclInformation, MakeAbsoluteSD,
MakeSelfRelativeSD, GetSecurityDescriptorLength, GetSecurityDescriptorControl,
GetSecurityDescriptorOwner, GetSecurityDescriptorGroup, GetSecurityDescriptorDacl,
GetSecurityDescriptorSacl, SetSecurityDescriptorDacl, SetSecurityDescriptorSacl,
SetSecurityDescriptorOwner, SetSecurityDescriptorGroup, SetSecurityDescriptorControl,
InitializeSecurityDescriptor, IsValidSecurityDescriptor, GetSecurityDescriptorLength,
GetSecurityDescriptorControl, GetSecurityDescriptorOwner, GetSecurityDescriptorGroup,
GetSecurityDescriptorDacl, GetSecurityDescriptorSacl, SetSecurityDescriptorDacl,
SetSecurityDescriptorSacl, SetSecurityDescriptorOwner, SetSecurityDescriptorGroup,
SetSecurityDescriptorControl, InitializeAcl, AddAccessAllowedAce, AddAccessAllowedAceEx,
AddAccessDeniedAce, AddAccessDeniedAceEx, AddAccessAllowedObjectAce,
AddAccessDeniedObjectAce, DeleteAce, GetAce, GetAclInformation, SetAclInformation,
MakeAbsoluteSD, MakeSelfRelativeSD, GetSecurityDescriptorLength,
GetSecurityDescriptorControl, GetSecurityDescriptorOwner, GetSecurityDescriptorGroup,
GetSecurityDescriptorDacl, GetSecurityDescriptorSacl, SetSecurityDescriptorDacl,
SetSecurityDescriptorSacl, SetSecurityDescriptorOwner, SetSecurityDescriptorGroup,
SetSecurityDescriptorControl, InitializeAcl, AddAccessAllowedAce, AddAccessAllowedAceEx,
AddAccessDeniedAce, AddAccessDeniedAceEx, AddAccessAllowedObjectAce,
AddAccessDeniedObjectAce, DeleteAce, GetAce, GetAclInformation, SetAclInformation,
MakeAbsoluteSD, MakeSelfRelativeSD, GetSecurityDescriptorLength,
GetSecurityDescriptorControl, GetSecurityDescriptorOwner, GetSecurityDescriptorGroup,
GetSecurityDescriptorDacl, GetSecurityDescriptorSacl, SetSecurityDescriptorDacl,
SetSecurityDescriptorSacl, SetSecurityDescriptorOwner, SetSecurityDescriptorGroup,
SetSecurityDescriptorControl, OpenProcessToken, OpenThreadToken, OpenProcessTokenEx,
OpenThreadTokenEx, DuplicateToken, DuplicateTokenEx, GetTokenInformation,
SetTokenInformation, AdjustTokenPrivileges, AdjustTokenGroups, AdjustTokenDefaultDacl,
CheckTokenMembership, GetTokenInformation, GetTokenInformationEx, PrivilegeCheck,
PrivilegeCheckEx, AccessCheck, AccessCheckByType, AccessCheckByTypeResultList,
AccessCheckByTypeResultListAndAuditAlarm, AccessCheckByTypeResultListAndAuditAlarmByHandle,
ObjectOpenAuditAlarm, ObjectPrivilegeAuditAlarm, ObjectCloseAuditAlarm,
PrivilegedServiceAuditAlarm, GenerateAudit, GenerateAuditForHandle,
SetAuditPolicy, GetAuditPolicy, AuditEnumerateUsers, AuditEnumerateUserRights,
AuditAddUserRight, AuditRemoveUserRight, AuditSetSystemPolicy, AuditQuerySystemPolicy,
AuditSetPerUserPolicy, AuditQueryPerUserPolicy, AuditSetCategoryPolicy,
AuditQueryCategoryPolicy, AuditSetSubcategoryPolicy, AuditQuerySubcategoryPolicy,
AuditEnumerateCategories, AuditEnumerateSubcategories, AuditEnumeratePerUserPolicy,
AuditEnumeratePerUserPolicyForCategory, AuditQuerySystemPolicyEx,
AuditSetSystemPolicyEx, AuditQuerySystemPolicyEx, AuditEnumerateUsersEx,
AuditEnumerateUserRightsEx, AuditAddUserRightEx, AuditRemoveUserRightEx,
LookupPrivilegeNameA, LookupPrivilegeNameW, LookupPrivilegeValueA, LookupPrivilegeValueW,
LookupPrivilegeDisplayNameA, LookupPrivilegeDisplayNameW, GetAce, GetAclInformation,
SetAclInformation, AddAccessAllowedAce, AddAccessAllowedAceEx, AddAccessDeniedAce,
AddAccessDeniedAceEx, AddAccessAllowedObjectAce, AddAccessDeniedObjectAce,
DeleteAce, GetAce, GetAclInformation, SetAclInformation, AddAccessAllowedAce,
AddAccessAllowedAceEx, AddAccessDeniedAce, AddAccessDeniedAceEx, DeleteAce,
GetAce, GetAclInformation, SetAclInformation, AddAccessAllowedAce,
AddAccessAllowedAceEx, AddAccessDeniedAce, AddAccessDeniedAceEx, DeleteAce,
GetAce, GetAclInformation, SetAclInformation, InitializeAcl, GetAclInformation,
SetAclInformation, AddAccessAllowedAce, AddAccessAllowedAceEx, AddAccessDeniedAce,
AddAccessDeniedAceEx, DeleteAce, GetAce, GetAclInformation, SetAclInformation,
InitializeAcl, GetAclInformation, SetAclInformation, GetAce, GetAclInformation,
SetAclInformation, AddAccessAllowedAce, AddAccessAllowedAceEx, AddAccessDeniedAce,
AddAccessDeniedAceEx, DeleteAce, GetAce, GetAclInformation, SetAclInformation,
InitializeSecurityDescriptor, IsValidSecurityDescriptor, GetSecurityDescriptorLength,
GetSecurityDescriptorControl, GetSecurityDescriptorOwner, GetSecurityDescriptorGroup,
GetSecurityDescriptorDacl, GetSecurityDescriptorSacl, SetSecurityDescriptorDacl,
SetSecurityDescriptorSacl, SetSecurityDescriptorOwner, SetSecurityDescriptorGroup,
SetSecurityDescriptorControl, GetSecurityDescriptorDacl, GetSecurityDescriptorSacl,
GetSecurityDescriptorOwner, GetSecurityDescriptorGroup, GetSecurityDescriptorControl,
InitializeSecurityDescriptor, IsValidSecurityDescriptor, GetSecurityDescriptorLength,
GetSecurityDescriptorControl, GetSecurityDescriptorOwner, GetSecurityDescriptorGroup,
GetSecurityDescriptorDacl, GetSecurityDescriptorSacl, SetSecurityDescriptorDacl,
SetSecurityDescriptorSacl, SetSecurityDescriptorOwner, SetSecurityDescriptorGroup,
SetSecurityDescriptorControl, GetSecurityDescriptorDacl, GetSecurityDescriptorSacl,
GetSecurityDescriptorOwner, GetSecurityDescriptorGroup, GetSecurityDescriptorControl,
OpenProcessToken, OpenThreadToken, OpenProcessTokenEx, OpenThreadTokenEx,
DuplicateToken, DuplicateTokenEx, GetTokenInformation, SetTokenInformation,
AdjustTokenPrivileges, AdjustTokenGroups, AdjustTokenDefaultDacl, CheckTokenMembership,
GetTokenInformation, GetTokenInformationEx, PrivilegeCheck, PrivilegeCheckEx,
AccessCheck, AccessCheckByType, AccessCheckByTypeResultList,
AccessCheckByTypeResultListAndAuditAlarm, AccessCheckByTypeResultListAndAuditAlarmByHandle,
ObjectOpenAuditAlarm, ObjectPrivilegeAuditAlarm, ObjectCloseAuditAlarm,
PrivilegedServiceAuditAlarm, GenerateAudit, GenerateAuditForHandle,
SetAuditPolicy, GetAuditPolicy, AuditEnumerateUsers, AuditEnumerateUserRights,
AuditAddUserRight, AuditRemoveUserRight, AuditSetSystemPolicy, AuditQuerySystemPolicy,
AuditEnumerateUsers, AuditEnumerateUserRights, AuditAddUserRight, AuditRemoveUserRight,
AuditSetSystemPolicy, AuditQuerySystemPolicy, AuditEnumerateUsers, AuditEnumerateUserRights,
AuditAddUserRight, AuditRemoveUserRight, AuditSetSystemPolicy, AuditQuerySystemPolicy,
AuditEnumerateUsers, AuditEnumerateUserRights, AuditAddUserRight, AuditRemoveUserRight,
AuditSetSystemPolicy, AuditQuerySystemPolicy, AuditEnumerateUsers, AuditEnumerateUserRights,
AuditAddUserRight, AuditRemoveUserRight, AuditSetSystemPolicy, AuditQuerySystemPolicy,
AuditEnumerateUsers, AuditEnumerateUserRights, AuditAddUserRight, AuditRemoveUserRight,
AuditSetSystemPolicy, AuditQuerySystemPolicy, AuditEnumerateUsers, AuditEnumerateUserRights,
AuditAddUserRight, AuditRemoveUserRight, AuditSetSystemPolicy, AuditQuerySystemPolicy,
AuditEnumerateUsers, AuditEnumerateUserRights, AuditAddUserRight, AuditRemoveUserRight,
LookupPrivilegeNameA, LookupPrivilegeNameW, LookupPrivilegeValueA, LookupPrivilegeValueW,
LookupPrivilegeDisplayNameA, LookupPrivilegeDisplayNameW, GetAce, GetAclInformation,
SetAclInformation, AddAccessAllowedAce, AddAccessAllowedAceEx, AddAccessDeniedAce,
AddAccessDeniedAceEx, AddAccessAllowedObjectAce, AddAccessDeniedObjectAce, DeleteAce,
GetAce, GetAclInformation, SetAclInformation, AddAccessAllowedAce, AddAccessAllowedAceEx,
AddAccessDeniedAce, AddAccessDeniedAceEx, DeleteAce, GetAce, GetAclInformation,
SetAclInformation, AddAccessAllowedAce, AddAccessAllowedAceEx, AddAccessDeniedAce,
AddAccessDeniedAceEx, DeleteAce, GetAce, GetAclInformation, SetAclInformation,
InitializeAcl, GetAclInformation, SetAclInformation, AddAccessAllowedAce,
AddAccessAllowedAceEx, AddAccessDeniedAce, AddAccessDeniedAceEx, DeleteAce,
GetAce, GetAclInformation, SetAclInformation, InitializeAcl, GetAclInformation,
SetAclInformation, GetAce, GetAclInformation, SetAclInformation, AddAccessAllowedAce,
AddAccessAllowedAceEx, AddAccessDeniedAce, AddAccessDeniedAceEx, DeleteAce,
GetAce, GetAclInformation, SetAclInformation, InitializeAcl, GetAclInformation,
SetAclInformation, GetAce, GetAclInformation, SetAclInformation, AddAccessAllowedAce,
AddAccessAllowedAceEx, AddAccessDeniedAce, AddAccessDeniedAceEx, DeleteAce,
GetAce, GetAclInformation, SetAclInformation, InitializeAcl, GetAclInformation,
SetAclInformation, GetAce, GetAclInformation, SetAclInformation, AddAccessAllowedAce,
AddAccessAllowedAceEx, AddAccessDeniedAce, AddAccessDeniedAceEx, DeleteAce,
GetAce, GetAclInformation, SetAclInformation
```

### 4d: Ole32 - COM
```
CoInitialize, CoUninitialize, CoCreateInstance, CoGetClassObject, CoRegisterClassObject,
CoRevokeClassObject, CoGetClassObject, CoGetObject, CoCreateInstanceEx, CoGetInstanceFromFile,
CoCreateInstanceFromApp, CoGetInstanceFromIStorage, CoGetClassObjectFromURL,
OleInitialize, OleUninitialize, OleCreate, OleCreateEx, OleCreateFromData, OleCreateFromFile,
OleCreateLink, OleCreateLinkEx, OleCreateLinkFromData, OleCreateLinkFromFile,
OleCreateLinkFromFileEx, OleCreateLinkToFile, OleCreateLinkToFileEx, OleCreateLinkToFile,
OleCreateLinkToFileEx, OleCreateFromFile, OleCreateFromFileEx, OleCreateLinkFromFile,
OleCreateLinkFromFileEx, OleCreateLinkToFile, OleCreateLinkToFileEx, OleCreateLinkToFile,
OleCreateLinkToFileEx, OleCreateFromFile, OleCreateFromFileEx, OleCreateLinkFromFile,
OleCreateLinkFromFileEx, OleCreateLinkToFile, OleCreateLinkToFileEx, OleCreateLinkToFile,
OleCreateLinkToFileEx, OleCreateFromFile, OleCreateFromFileEx, OleCreateLinkFromFile,
OleCreateLinkFromFileEx, OleCreateLinkToFile, OleCreateLinkToFileEx, OleCreateLinkToFile,
OleCreateLinkToFileEx, OleCreateFromFile, OleCreateFromFileEx, OleCreateLinkFromFile,
OleCreateLinkFromFileEx, OleCreateLinkToFile, OleCreateLinkToFileEx, OleCreateLinkToFile,
OleCreateLinkToFileEx, OleCreateFromFile, OleCreateFromFileEx, OleCreateLinkFromFile,
OleCreateLinkFromFileEx, OleCreateLinkToFile, OleCreateLinkToFileEx, OleCreateLinkToFile,
OleCreateLinkToFileEx, OleCreateFromFile, OleCreateFromFileEx, OleCreateLinkFromFile,
OleCreateLinkFromFileEx, OleCreateLinkToFile, OleCreateLinkToFileEx, OleCreateLinkToFile,
OleCreateLinkToFileEx, OleCreateFromFile, OleCreateFromFileEx, OleCreateLinkFromFile,
OleCreateLinkFromFileEx, OleCreateLinkToFile, OleCreateLinkToFileEx, OleCreateLinkToFile,
OleCreateLinkToFileEx, OleCreateFromFile, OleCreateFromFileEx, OleCreateLinkFromFile,
OleCreateLinkFromFileEx, OleCreateLinkToFile, OleCreateLinkToFileEx, OleCreateLinkToFile,
OleCreateLinkToFileEx, OleCreateFromFile, OleCreateFromFileEx, OleCreateLinkFromFile,
OleCreateLinkFromFileEx, OleCreateLinkToFile, OleCreateLinkToFileEx
```

### 4e: Shell32
```
SHGetFolderPathW, SHGetSpecialFolderPathW, SHGetSpecialFolderLocation, SHGetPathFromIDListW,
SHGetDesktopFolder, SHCreateItemFromIDList, SHCreateShellItemArray, SHCreateShellItemArrayFromDataObject,
SHGetFileInfoW, SHParseDisplayName, PathAddBackslashW, PathCanonicalizeW, PathFileExistsW,
PathFindExtensionA, PathFindExtensionW, PathFindFileNameA, PathFindFileNameW,
PathFindNextComponentW, PathGetDriveNumberW, PathIsDirectoryW, PathIsRelativeW, PathMatchSpecW,
PathRemoveExtensionW, PathSkipRootW, StrChrW, StrDupW, StrRetToBufW
```

---

## Phase 5: CRT / UCRT / VCRuntime (Priority: WINWORD C++ runtime)

**Target**: msvcrt.dll, ucrtbase.dll, vcruntime140.dll

### Key Functions (from WINWORD unresolved):
```
__acrt_iob_func, __stdio_common_vsprintf, __stdio_common_vswprintf, _assert, _strdup,
_wcsicmp, _wcsnicmp, _wcsdup, _recalloc, _errno, _ltow, _stricmp, _time64, _wgetenv,
atoi, atol, calloc, free, fwrite, getenv, iswalpha, iswcntrl, iswspace, iswupper,
malloc, memchr, memcmp, memcpy, memmove, memset, qsort, rand, realloc, round, strchr,
strcmp, strcpy, strcspn, strlen, strncmp, strnlen, strtok, system, towlower, wcschr,
wcscmp, wcscpy, wcscspn, wcslen, wcsncmp, wcsnlen, wcsrchr, wcsspn, wcsstr, wcstol,
wcstoul, strrchr, tolower, toupper, strcat, strcspn, strdup, strerror, strncat, strncpy,
strpbrk, strrchr, strspn, strstr, strtod, strtof, strtol, strtoll, strtoul, strtoull,
wcsncat, wcsncpy, wcsxfrm, wmemcmp, wmemcpy, wmemmove, wmemset, wcscat, wcscpy, wcsxfrm,
wmemcmp, wmemcpy, wmemmove, wmemset
```

**Implementation**: These are in `core/api/ucrt.cpp`. Add missing ones and register under:
- `msvcrt.dll` (for backward compat)
- `ucrtbase.dll` (modern CRT)
- `vcruntime140.dll` (VC++ runtime)

```cpp
// In initialize_ucrt_layer() or new initialize_vcruntime_layer():
register_api("msvcrt.dll", "calloc", (void*)WINAPI_calloc);
register_api("ucrtbase.dll", "calloc", (void*)WINAPI_calloc);
register_api("vcruntime140.dll", "calloc", (void*)WINAPI_calloc);
// ... etc
```

---

## Phase 6: RPC / COM / Network (Priority: WINWORD advanced features)

### Rpcrt4.dll
```
RpcStringBindingComposeW, RpcBindingFromStringBindingW, RpcBindingFree,
RpcStringFreeW, RpcBindingSetAuthInfoExW, RpcMgmtInqDefaultProtectLevel,
RpcBindingInqAuthClientExW, RpcServerUseProtseqEpW, RpcServerRegisterIfEx,
RpcServerUnregisterIfEx, RpcServerListen, RpcMgmtStopServerListening,
RpcMgmtWaitServerListen, RpcBindingCopy, RpcBindingVectorFree, RpcEpRegisterW,
RpcEpUnregisterW, RpcNsBindingLookupBegin, RpcNsBindingLookupNext, RpcNsBindingLookupDone
```

### Oleaut32.dll
```
SysAllocString, SysFreeString, SysStringLen, SysStringByteLen, SysReAllocString,
SysReAllocStringLen, SysAllocStringByteLen, SysAllocStringLen, VariantInit, VariantClear,
VariantCopy, VariantCopyInd, VariantChangeType, VariantChangeTypeEx, VariantTimeToDosDateTime,
VariantTimeToSystemTime, DosDateTimeToVariantTime, SystemTimeToVariantTime,
VarI2FromUI1, VarI2FromI4, VarI2FromR4, VarI2FromR8, VarI2FromCy, VarI2FromDate,
VarI2FromStr, VarI2FromDisp, VarI2FromBool, VarI2FromI1, VarI2FromUI2, VarI2FromUI4,
VarI2FromUI8, VarI2FromDec, VarI4FromUI1, VarI4FromI2, VarI4FromR4, VarI4FromR8,
VarI4FromCy, VarI4FromDate, VarI4FromStr, VarI4FromDisp, VarI4FromBool, VarI4FromI1,
VarI4FromUI2, VarI4FromUI4, VarI4FromUI8, VarI4FromDec, VarR4FromUI1, VarR4FromI2,
VarR4FromI4, VarR4FromCy, VarR4FromDate, VarR4FromStr, VarR4FromDisp, VarR4FromBool,
VarR4FromI1, VarR4FromUI2, VarR4FromUI4, VarR4FromUI8, VarR4FromDec, VarR8FromUI1,
VarR8FromI2, VarR8FromI4, VarR8FromCy, VarR8FromDate, VarR8FromStr, VarR8FromDisp,
VarR8FromBool, VarR8FromI1, VarR8FromUI2, VarR8FromUI4, VarR8FromUI8, VarR8FromDec,
VarCyFromUI1, VarCyFromI2, VarCyFromI4, VarCyFromR4, VarCyFromR8, VarCyFromDate,
VarCyFromStr, VarCyFromDisp, VarCyFromBool, VarCyFromI1, VarCyFromUI2, VarCyFromUI4,
VarCyFromUI8, VarCyFromDec, VarDateFromUI1, VarDateFromI2, VarDateFromI4, VarDateFromR4,
VarDateFromR8, VarDateFromCy, VarDateFromStr, VarDateFromDisp, VarDateFromBool,
VarDateFromI1, VarDateFromUI2, VarDateFromUI4, VarDateFromUI8, VarDateFromDec,
VarBstrFromUI1, VarBstrFromI2, VarBstrFromI4, VarBstrFromR4, VarBstrFromR8, VarBstrFromCy,
VarBstrFromDate, VarBstrFromStr, VarBstrFromDisp, VarBstrFromBool, VarBstrFromI1,
VarBstrFromUI2, VarBstrFromUI4, VarBstrFromUI8, VarBstrFromDec, VarBoolFromUI1,
VarBoolFromI2, VarBoolFromI4, VarBoolFromR4, VarBoolFromR8, VarBoolFromCy, VarBoolFromDate,
VarBoolFromStr, VarBoolFromDisp, VarBoolFromI1, VarBoolFromUI2, VarBoolFromUI4,
VarBoolFromUI8, VarBoolFromDec, VarI1FromUI1, VarI1FromI2, VarI1FromI4, VarI1FromR4,
VarI1FromR8, VarI1FromCy, VarI1FromDate, VarI1FromStr, VarI1FromDisp, VarI1FromBool,
VarI1FromUI2, VarI1FromUI4, VarI1FromUI8, VarI1FromDec, VarUI1FromUI1, VarUI1FromI2,
VarUI1FromI4, VarUI1FromR4, VarUI1FromR8, VarUI1FromCy, VarUI1FromDate, VarUI1FromStr,
VarUI1FromDisp, VarUI1FromBool, VarUI1FromI1, VarUI1FromUI2, VarUI1FromUI4,
VarUI1FromUI8, VarUI1FromDec, VarUI2FromUI1, VarUI2FromI2, VarUI2FromI4, VarUI2FromR4,
VarUI2FromR8, VarUI2FromCy, VarUI2FromDate, VarUI2FromStr, VarUI2FromDisp, VarUI2FromBool,
VarUI2FromI1, VarUI2FromUI2, VarUI2FromUI4, VarUI2FromUI8, VarUI2FromDec, VarUI4FromUI1,
VarUI4FromI2, VarUI4FromI4, VarUI4FromR4, VarUI4FromR8, VarUI4FromCy, VarUI4FromDate,
VarUI4FromStr, VarUI4FromDisp, VarUI4FromBool, VarUI4FromI1, VarUI4FromUI2, VarUI4FromUI4,
VarUI4FromUI8, VarUI4FromDec, VarUI8FromUI1, VarUI8FromI2, VarUI8FromI4, VarUI8FromR4,
VarUI8FromR8, VarUI8FromCy, VarUI8FromDate, VarUI8FromStr, VarUI8FromDisp, VarUI8FromBool,
VarUI8FromI1, VarUI8FromUI2, VarUI8FromUI4, VarUI8FromUI8, VarUI8FromDec, VarDecFromUI1,
VarDecFromI2, VarDecFromI4, VarDecFromR4, VarDecFromR8, VarDecFromCy, VarDecFromDate,
VarDecFromStr, VarDecFromDisp, VarDecFromBool, VarDecFromI1, VarDecFromUI2, VarDecFromUI4,
VarDecFromUI8, VarDecFromDec, VarBstrFromUI1, VarBstrFromI2, VarBstrFromI4, VarBstrFromR4,
VarBstrFromR8, VarBstrFromCy, VarBstrFromDate, VarBstrFromStr, VarBstrFromDisp,
VarBstrFromBool, VarBstrFromI1, VarBstrFromUI2, VarBstrFromUI4, VarBstrFromUI8,
VarBstrFromDec, VarI8FromUI1, VarI8FromI2, VarI8FromI4, VarI8FromR4, VarI8FromR8,
VarI8FromCy, VarI8FromDate, VarI8FromStr, VarI8FromDisp, VarI8FromBool, VarI8FromI1,
VarI8FromUI2, VarUI8FromUI4, VarUI8FromUI8, VarUI8FromDec, VarR4FromUI1, VarR4FromI2,
VarR4FromI4, VarR4FromCy, VarR4FromDate, VarR4FromStr, VarR4FromDisp, VarR4FromBool,
VarR4FromI1, VarR4FromUI2, VarR4FromUI4, VarR4FromUI8, VarR4FromDec, VarR8FromUI1,
VarR8FromI2, VarR8FromI4, VarR8FromCy, VarR8FromDate, VarR8FromStr, VarR8FromDisp,
VarR8FromBool, VarR8FromI1, VarR8FromUI2, VarR8FromUI4, VarR8FromUI8, VarR8FromDec
```

### Crypt32.dll
```
CertOpenStore, CertCloseStore, CertFindCertificateInStore, CertFreeCertificateContext,
CertGetCertificateChain, CertFreeCertificateChain, CertVerifyCertificateChainPolicy,
CertGetCertificateContextProperty, CertSetCertificateContextProperty,
CertCompareCertificateName, CertDuplicateCertificateContext, CertDuplicateStore,
CertAddStoreToCollection, CertRemoveStoreFromCollection, CertSaveStore,
CertSerializeCertificateStoreElement, CertAddEncodedCertificateToStore,
CertAddCertificateContextToStore, CertDeleteCertificateFromStore,
CertCreateCertificateContext, CertCreateSelfSignCertificate, CertStrToNameW,
CertNameToStrW, CertGetPublicKeyLength, CryptMsgOpenToDecode, CryptMsgClose,
CryptMsgGetParam, CryptMsgUpdate, CryptMsgControl, CryptMsgVerifyCountersignature,
CryptSignMessage, CryptVerifyMessageSignature, CryptEncodeObjectEx, CryptDecodeObjectEx,
CryptEncodeObject, CryptDecodeObject, CryptFormatObject, CryptImportPublicKeyInfoEx,
CryptExportPublicKeyInfoEx, CryptSignCertificate, CryptVerifyCertificateSignature,
CryptSignAndEncodeCertificate, CryptDecodeAndVerifyCertificate, CryptHashData,
CryptHashData2, CryptCreateHash, CryptDestroyHash, CryptGetHashParam, CryptSetHashParam,
CryptDeriveKey, CryptGenKey, CryptDestroyKey, CryptExportKey, CryptImportKey,
CryptGetKeyParam, CryptSetKeyParam, CryptEncrypt, CryptDecrypt, CryptSignHash,
CryptVerifySignature, CryptGetUserKey, CryptSetProviderEx, CryptSetProvider,
CryptGetProvParam, CryptReleaseContext, CryptAcquireContextW, CryptAcquireContextA,
CryptSetProviderU, CryptGetDefaultProvider, CryptGetDefaultProviderU,
CryptEnumProviders, CryptEnumProviderTypes, CryptGetProvParam, CryptSetProvider,
CryptSetProviderEx, CryptGetDefaultProvider, CryptGetDefaultProviderU,
CryptEnumProviders, CryptEnumProviderTypes, CryptContextAddRef, CryptContextRelease,
CryptSetKeyParam, CryptGetKeyParam, CryptDestroyKey, CryptGenKey, CryptExportKey,
CryptImportKey, CryptDestroyHash, CryptGetHashParam, CryptSetHashParam, CryptCreateHash,
CryptHashData, CryptHashData2, CryptEncrypt, CryptDecrypt, CryptSignHash,
CryptVerifySignature
```

### WinHttp.dll / WinInet.dll / UrlMon.dll
```
WinHttpOpen, WinHttpConnect, WinHttpOpenRequest, WinHttpSetOption, WinHttpSendRequest,
WinHttpReceiveResponse, WinHttpQueryHeaders, WinHttpReadData, WinHttpWriteData,
WinHttpCloseHandle, WinHttpSetStatusCallback, WinHttpGetProxyForUrl, WinHttpGetIEProxyConfigForCurrentUser,
InternetOpenW, InternetCloseHandle, InternetConnectW, InternetOpenUrlW, InternetReadFile,
InternetWriteFile, InternetSetOptionW, InternetQueryOptionW, InternetGetConnectedState,
InternetGetConnectedStateEx, InternetSetStatusCallback, InternetSetStatusCallbackA,
UrlDownloadToFileW, UrlDownloadToCacheFileW, URLOpenBlockingStreamW, URLOpenStreamW,
URLDownloadToCacheFileW, URLOpenBlockingStreamW, URLOpenStreamW
```

---

## Testing Commands

After each phase, run these tests:

```bash
# Clean and rebuild
for p in $(pgrep -x ThetaZ); do kill -9 $p 2>/dev/null; done
cmake --build build -j8

# Test WINWORD (64-bit)
timeout 60 ./build/ThetaZ "/home/eronic/Prefixes/ThetaZ/drive_c/Program Files/Microsoft Office/root/Office16/WINWORD.EXE" > /tmp/winword_test.log 2>&1
echo "exit=$?"
grep -c "!! unresolved" /tmp/winword_test.log

# Test 32-bit wordpad (should still work)
timeout 60 ./build/ThetaZ "/home/eronic/Prefixes/ThetaZ/drive_c/Program Files (x86)/Windows NT/Accessories/wordpad.exe" > /tmp/wordpad_test.log 2>&1
echo "exit=$?"
grep -c "!! unresolved" /tmp/wordpad_test.log

# Regression: OfficeSetup (32-bit target)
timeout 120 ./build/ThetaZ "/home/eronic/Downloads/OfficeSetup.exe" > /tmp/officesetup_test.log 2>&1
echo "exit=$?"
grep "ExitProcess" /tmp/officesetup_test.log

# Regression: SEH test
/tmp/seh_test.exe
```

---

## File List Summary

### New Files:
- `core/api/ntdll_api.cpp` — ntdll implementations
- `core/api/ntdll_api.h` — ntdll declarations

### Modified Files:
- `core/api/kernel32_runtime.cpp` — add kernel32 + kernelbase functions
- `core/api/kernel32.h` — declarations
- `core/api/kernel32_runtime.cpp` — register kernelbase.dll functions
- `core/api/ucrt.cpp` — add missing CRT functions
- `core/api/ucrt.cpp` — register msvcrt/ucrtbase/vcruntime
- `core/api/api.h` — add `initialize_ntdll_layer()`
- `core/api/api.cpp` — implement `initialize_ntdll_layer()`, call from `initialize_api_layer()`
- `core/loader/main.cpp` — add `kernelbase.dll`, `ntdll.dll` to builtin_dlls sets
- `core/loader/loader.h` — add to `try_load_from_thetaz_prefix` builtin set
- `core/x86/api_dispatch.cpp` — add signature-table entries for all new functions
- `core/api/user32.cpp` / `user32.h` — NEW or extend
- `core/api/gdi32.cpp` / `gdi32.h` — NEW or extend
- `core/api/advapi32.cpp` / `advapi32.h` — NEW or extend
- `core/api/ole32.cpp` / `ole32.h` — NEW or extend
- `core/api/shell32.cpp` / `shell32.h` — NEW or extend
- `core/api/rpcrt4.cpp` / `rpcrt4.h` — NEW
- `core/api/oleaut32.cpp` — extend
- `core/api/crypt32.cpp` / `crypt32.h` — NEW
- `core/api/winhttp.cpp` / `winhttp.h` — NEW

---

## Estimated Effort

| Phase | Functions | Files | Est. Days |
|-------|-----------|-------|-----------|
| 1: Core Kernel32 | ~50 | 4 | 3 |
| 2: Kernelbase | ~100 (forwards) | 3 | 2 |
| 3: Ntdll | ~60 | 5 | 4 |
| 4: User32/GDI/Advapi/Ole/Shell | 500+ | 10+ | 15+ |
| 5: CRT/UCRT | ~80 | 3 | 3 |
| 6: RPC/COM/Net | 200+ | 6 | 10+ |

**Total**: ~1000+ functions across 30+ files, ~40+ days for full coverage.

---

## Quick Start — Phase 1 Only (Minimal WINWORD)

If you only want WINWORD to get past early init, implement Phase 1 + Phase 2 (kernel32 + kernelbase). That's ~150 functions and will reduce WINWORD's unresolved from 14 to near-zero for kernel32/kernelbase.

```bash
# After Phase 1+2, run:
./build/ThetaZ "/home/eronic/Prefixes/ThetaZ/drive_c/Program Files/Microsoft Office/root/Office16/WINWORD.EXE"
# Should get further than ExitProcess(1)
```

---

## Notes for Lowest-End Model

1. **Each function follows the same pattern**: declaration in `.h`, implementation in `.cpp`, registration in `initialize_*_layer()`, signature in `api_dispatch.cpp:kSignatures`.

2. **Signature format in api_dispatch.cpp**:
   ```cpp
   // Macro: ST(dll, name, argc) = stdcall
   //        STW(dll, name, argc) = stdcall with wide (8-byte) args
   //        CT(dll, name, argc) = cdecl
   ST("kernel32.dll", "Beep", 2),
   ```

3. **Registration pattern**:
   ```cpp
   register_api("kernel32.dll", "Beep", (void*)WINAPI_Beep);
   ```

4. **Implementation pattern** (stub first):
   ```cpp
   BOOL WINAPI_CALL WINAPI_Beep(DWORD dwFreq, DWORD dwDuration) {
       (void)dwFreq; (void)dwDuration;
       return TRUE;  // or appropriate stub return
   }
   ```

5. **Builtin DLL list** (in two places):
   - `core/loader/main.cpp:process_imports` (the `is_builtin_dll` lambda)
   - `core/loader/main.cpp:try_load_from_thetaz_prefix` (the `builtin_dlls` unordered_set)

6. **Never modify Wine DLL loading logic** — only add to API registry and builtin lists.

7. **Test after each 10-20 functions** to catch regressions early.

8. **Use `grep "!! unresolved" log`** to track remaining missing imports.