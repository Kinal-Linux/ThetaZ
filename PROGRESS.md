# ThetaZ — Work Progress & Handoff

**Date:** 2026-08-01 (session interrupted; continuation on Windows)
**Project:** ThetaZ — a Windows-compatibility layer / PE emulator for running
real Windows x64 executables (and eventually big apps like WebView2/Edge) on Linux.
**Location:** `/mnt/hdd/Desktop/Projects/C++/ThetaZ`
**Build:** `cmake -B build -S . && cmake --build build -j$(nproc)` then run `./build/ThetaZ <pe.exe>`
**Tests:** `tests/run_tests.sh` (hello_*.c guests compiled to PE + loaded through the emulator)

---

## 1. THE GOAL (from the user, verbatim intent)

> "i want all of it implemented do not find a simpler app i want to target the big guns"

Implement ALL critical Windows APIs needed to run real, complex Windows executables
(WebView2/msedgewebview2.exe, Office, etc.), not just hello-world test guests.
Do NOT give up on the hard target. The acceptance test is: WebView2's
`msedgewebview2.exe` should load, resolve its imports, and not crash at RIP 0.

---

## 2. STATUS AT A GLANCE

- **E1 (window layer), E3 (CreateProcessA), E4 (GDI), batches 6-7 (Loader/TLS/IPC/SRW/exceptions): DONE** and committed in earlier sessions.
- **CURRENT BUILD STATE: DOES NOT COMPILE.** The last session started adding a big
  batch of critical kernel32 APIs but left the tree in a broken half-state.
  Everything below in §4 must be fixed before the project builds again.
- **WebView2 current behavior:** loads as a 64-bit PE, entry point is called, some
  imports resolve, then crashes with `Unhandled exception: 0xC0000005 at RIP: 0x0000000000000000`
  (access violation at NULL — almost certainly calling an **unresolved import** that
  was left as a null IAT entry by `process_imports`).

---

## 3. WHAT WAS DONE (completed & committed in earlier sessions)

| Item | Where | Status |
|------|-------|--------|
| Windows address space (map_sections, relocations, final protections) | `core/loader/main.cpp` (PELoader) | Done |
| Import resolution via `ApiRegistry` (dll!func), forwarders, ordinal imports | `core/loader/main.cpp` `process_imports`/`resolve_export`/`resolve_forwarder` | Done |
| Thread manager (create_thread, current pid/tid, exit_thread) | `core/ntdll/thread.cpp` | Done |
| Virtual memory (`virtual.cpp`) | `core/ntdll/virtual.cpp` | Done |
| Sync primitives / SRW locks | `core/ntdll/sync.cpp` | Done |
| TEB/PEB setup (gs base) | `core/ntdll/teb.cpp` | Done |
| IPC server + θserver (object manager) | `core/ipc/server.cpp`, `core/ipc/thetaserver.cpp` | Done |
| SEH/exception signal delivery | `core/exception/signal_x86_64.cpp` (needs exception.cpp impl — see §4) | Partial |
| E1: `window.cpp` (RegisterClassA, CreateWindowExA, GetMessageA, DispatchMessageA, DefWindowProcA, ShowWindow, etc.) | `core/api/window.cpp`, test `tests/hello_win.c` | Done, 56 tests pass |
| E3: `CreateProcessA` + child-process exit routing | `core/api/process.cpp`, tests `hello_child.c`/`hello_process.c` | Done, 64 tests pass |
| E4: GDI object model (GetStockObject, SelectObject, CreateSolidBrush, DeleteObject, TextOutA, etc.) | `core/api/user32.cpp`, test `tests/hello_gdi.c` | Mostly done; test still has a couple failures |
| UCRT layer | `core/api/ucrt.cpp` | Done |

---

## 4. CURRENT (BROKEN) STATE — exact files to fix

### 4.1 `core/api/kernel32.h` — NEW header, mostly good, ONE blocker
- Includes `core/api/api.h` (gives DWORD, BOOL, SIZE_T, HANDLE? NO — gives WinHandle,
  TRUE_VALUE/FALSE_VALUE, WINAPI_CALL, register_api, initialize_api_layer decls).
- Adds: HANDLE, ULONG, USHORT, INT, PVOID, VOID, LPDWORD/PDWORD/PBOOL/LPBOOL/PULONG/
  ULONG_PTR, LPOVERLAPPED, LPSECURITY_ATTRIBUTES, MEMORY_BASIC_INFORMATION (+MEM_/PAGE_
  flags), PROCESSINFOCLASS, THREADINFOCLASS, CONDITION_VARIABLE, CRITICAL_SECTION,
  SRWLOCK, LARGE_INTEGER, CONSOLE_READCONSOLE_CONTROL, WOW64CONTEXT.
- Declares ~40 `WINAPI_*` functions (thread ctx, vectored exceptions, NtQueryInfo*,
  VirtualQueryEx/ProtectEx/Write/ReadProcessMemory, WideCharToMultiByte,
  IsDebuggerPresent, condition variables, tick/QPC, WriteFile/ReadFile/WriteConsoleW/
  ReadConsoleW, named pipes) + `initialize_kernel32_api()`.
- **BLOCKER:** the line
  `typedef LONG (*PVECTORED_EXCEPTION_HANDLER)(EXCEPTION_POINTERS* ExceptionInfo);`
  uses `EXCEPTION_POINTERS`, which is never declared anywhere in kernel32.h.
  **FIX:** add before it:
  ```cpp
  struct _EXCEPTION_POINTERS;
  typedef struct _EXCEPTION_POINTERS* PEXCEPTION_POINTERS;
  typedef LONG (WINAPI_CALL *PVECTORED_EXCEPTION_HANDLER)(struct _EXCEPTION_POINTERS*);
  ```
  (or a full 2-field struct with `struct _EXCEPTION_RECORD*` + `void* ContextRecord`).

### 4.2 `core/api/api.cpp` — MUST be reverted to original + one addition
Current file wrongly ADDED placeholder bodies for functions that are **really
defined in other translation units** → duplicate-symbol link errors:
- `initialize_ucrt_layer()` → real one is in `core/api/ucrt.cpp:494`
- `initialize_user32_layer()` → real one is in `core/api/user32.cpp:347`
- `initialize_window_layer()` → real one is in `core/api/window.cpp:492`
- `initialize_process_layer()` → real one is in `core/api/process.cpp:146`
- `terminate_current_process()` → real one is in `core/api/process.cpp:130`
- `set_main_module_base()`, `load_guest_module()`, `set_crt_command_line()`,
  `run_crt_exit_callbacks()` → real ones are elsewhere (ucrt layer)

**FIX:** `core/api/api.cpp` must contain ONLY:
- `normalize_dll` helper
- `ApiRegistry::instance/register_function/find_function/find_function_by_name/list_dll_functions`
- `register_api(dll, name, func)`
- `initialize_api_layer()` — which should call:
  `initialize_kernel32_api(); initialize_user32_layer(); initialize_window_layer(); initialize_process_layer(); initialize_ucrt_layer();`
  (All the sub-layer init functions use `std::call_once`/idempotent registration,
  so calling them from both here and main_entry.cpp is harmless.)
- NO other function definitions.

### 4.3 `core/exception/exception.cpp` — BROKEN, needs full rewrite
Current file references symbols that don't exist: `NULL_VALUE`, `g_exception_mutex`,
`g_exception_handlers`, `VectoredHandlerEntry`, `EXCEPTION_POINTERS`,
`EXCEPTION_CONTINUE_SEARCH`. It also **deleted the entire `ExceptionHandler` class
implementation** that `signal_x86_64.cpp` requires (the build needs these symbols):
- `ExceptionHandler::ExceptionHandler()`
- `ExceptionHandler::register_handler(ExceptionCode, ExceptionHandlerFunc, void*)`
- `ExceptionHandler::get_current_context()` / `set_current_context()`
- `ExceptionHandler::unhandled_exception(ExceptionCode, const CPUContext64&, void*)`
- `ExceptionHandler::dispatch(ExceptionCode, CPUContext64&, void*)`
- `ExceptionHandler::walk_exception_chain(ExceptionCode, CPUContext64&)`
- `ExceptionSetup64::ExceptionSetup64(ExceptionHandler&)` (constructor — the signal
  handler wiring lives in signal_x86_64.cpp; the ctor here must at least exist or
  the class can be defined inline there — check which file owns it)
- `set_active_exception_handler(ExceptionHandler*)` / `get_active_exception_handler()`
- `set_unhandled_exception_filter(void*)` / `get_unhandled_exception_filter()`

**FIX — rewrite `core/exception/exception.cpp` to:**
1. Implement all of the above per the declarations in `core/exception/exception.h`
   (keep the guest-visible `WinContext`/`WinExceptionRecord` layouts intact — do not change exception.h).
2. ADD (new this session) vectored exception handlers with **file-local static state**:
   ```cpp
   namespace {
   std::mutex g_veh_mutex;
   std::vector<PVECTORED_EXCEPTION_HANDLER> g_veh_handlers;      // first=0
   std::vector<PVECTORED_EXCEPTION_HANDLER> g_veh_continue_handlers;
   }
   ```
   - `WINAPI_AddVectoredExceptionHandler(ULONG first, handler)` → insert at front if
     first==1 else push back; return the handler ptr as the opaque "handle".
   - `WINAPI_AddVectoredContinueHandler` → same, separate list.
   - `WINAPI_RemoveVectoredExceptionHandler(PVOID handle)` → erase matching ptr.
   - `WINAPI_RemoveVectoredContinueHandler` → same for continue list.
   - Optionally a `dispatch_vectored_handlers(EXCEPTION_POINTERS*)` that walks the
     lists and returns EXCEPTION_CONTINUE_EXECUTION(1)/SEARCH(0).
3. Include `core/api/kernel32.h` for PVECTORED_EXCEPTION_HANDLER/ULONG/PVOID/BOOL.
   (Note: the header currently has the EXCEPTION_POINTERS blocker from §4.1 — fix that first.)

### 4.4 `core/api/thread_api.cpp` — small fixes needed
- Currently implements Wow64GetThreadContext, SetThreadContext, SuspendThread,
  ResumeThread, GetThreadId.
- **BUGS:** (a) it does NOT include `core/api/kernel32.h`, so HANDLE/WOW64CONTEXT/
  TRUE_VALUE/FALSE_VALUE/DWORD are undeclared; (b) `GetThreadId` calls
  `ThetaServer::instance().get_thread_mutex()` / `.get_thread()` which don't exist
  (wrong API / wrong header).
- **FIX:** include kernel32.h; rewrite GetThreadId to a stub:
  ```cpp
  DWORD WINAPI_CALL WINAPI_GetThreadId(HANDLE hThread) {
      return hThread ? static_cast<DWORD>(reinterpret_cast<uintptr_t>(hThread) & 0xFFFFFFFF) : 0;
  }
  ```
  (keep Wow64GetThreadContext returning FALSE, SetThreadContext returning TRUE,
  SuspendThread returning 0, ResumeThread returning 1).

### 4.5 `core/api/kernel32.cpp` — INCOMPLETE (the main work)
Only implements + registers 5 APIs. Must implement **all** APIs declared in
kernel32.h and register each in `initialize_kernel32_api()`. Recommended simple
implementations (each `WINAPI_CALL`, in namespace ThetaZ):

**Process info:**
- `WINAPI_NtQueryInformationProcess(handle, cls, out, len, retlen)` — for
  `ProcessBasicInformation(0)` fill a
  `PROCESS_BASIC_INFORMATION {ExitStatus; PebBaseAddress; AffinityMask; BasePriority;
  UniqueProcessId; InheritedFromUniqueProcessId}` (all zeros / pid = GetCurrentProcessId
  semantics) and return STATUS_SUCCESS (0); otherwise return 0xC0000004.
- `WINAPI_NtQueryInformationThread(...)` → return 0xC0000004 (stub).
- `WINAPI_GetProcessId(HANDLE)` → return handle value cast to DWORD (already there).

**Memory:**
- `WINAPI_VirtualQueryEx(hProcess, addr, mbi, len)` → if hProcess is NULL (own process)
  zero `mbi`, set BaseAddress=addr, AllocationBase=addr, AllocationProtect=PAGE_READWRITE,
  RegionSize=0x1000, State=MEM_COMMIT|MEM_RESERVE, Protect=PAGE_READWRITE, Type=MEM_PRIVATE;
  return sizeof(MEMORY_BASIC_INFORMATION).
- `WINAPI_VirtualProtectEx` → return TRUE (no-op).
- `WINAPI_WriteProcessMemory`/`ReadProcessMemory` → if hProcess is NULL/self, do
  `memcpy(base, buffer, n)` / `memcpy(buffer, base, n)` and set bytes written/read; TRUE.

**Strings (UTF-8 ⇄ UTF-16):**
- `WINAPI_WideCharToMultiByte` → encode; respect cch==-1 (strlen). Must treat
  WCHAR (uint16_t) as UTF-16 code unit.
- `WINAPI_MultiByteToWideChar` → decode; if cchWideChar==0 return required length.
  FIX NOTE: previous version had a typo `if (!lpWideCharStr || lpMultiByteStr)` (wrong
  null check) — write it correctly.

**Console / file I/O:**
- `WINAPI_WriteConsoleW` → loop `putwchar(str[i])` for `nNumberOfCharsToWrite` wchars,
  set `*lpNumberOfCharsWritten`, return TRUE. (If the host console is ASCII, fall back
  to `printf("%ls", ...)`.)
- `WINAPI_ReadConsoleW` / `WINAPI_WriteFile` / `WINAPI_ReadFile` → stubs returning
  FALSE/TRUE with counts = 0 (do NOT fault).

**Condition variables:** back with per-address state OR just stub:
- `InitConditionVariable`/`DeleteConditionVariable` → no-op.
- `WakeAllConditionVariable`/`WakeConditionVariable` → no-op.
- `SleepConditionVariableCS`/`SleepConditionVariableSRW` → return TRUE immediately
  (or `std::this_thread::sleep_for` for dwMilliseconds if you want to be closer to real).

**Timing:**
- `GetTickCount` (already), `GetTickCount64` → `steady_clock` ms cast to ULONG.
- `QueryPerformanceCounter` → `high_resolution_clock` ns into `LARGE_INTEGER::QuadPart`.
- `QueryPerformanceFrequency` → 1'000'000'000.

**Debug:**
- `IsDebuggerPresent` → FALSE; `CheckRemoteDebuggerPresent` → set *pb=FALSE, return FALSE.

**Named pipes (all stubs, but must NOT crash):**
- `WaitNamedPipeW/A` → FALSE.
- `CreateNamedPipeW/A` → return `(HANDLE)0xDEADPIPE` style dummy (non-NULL).
- `SetNamedPipeHandleState`, `GetNamedPipeInfo`, `CallNamedPipeW/A` → TRUE / zeros.

**Registration:** `initialize_kernel32_api()` must register every implemented function,
e.g.:
```cpp
register_api("kernel32.dll", "WriteConsoleW", (void*)WINAPI_WriteConsoleW);
register_api("kernel32.dll", "NtQueryInformationProcess", (void*)WINAPI_NtQueryInformationProcess);
// ... one line per function ...
```
Functions implemented in OTHER files (thread_api.cpp, exception.cpp) can be declared in
kernel32.h and registered here via `extern` linkage (same namespace — just reference them).

### 4.6 Files already deleted this session (do not recreate)
`core/api/wrappers.cpp`, `core/api/process.h`, `core/api/pc.rs/` (a directory containing
one misnamed file), `core/ntdll/ntapi.cpp`, `core/ipc/cache.cpp`, `core/ipc/message.cpp`,
`core/ipc/message.h`. `CMakeLists.txt` has already been cleaned to match.

---

## 5. IMMEDIATE NEXT STEPS (in order)

1. Fix `kernel32.h` EXCEPTION_POINTERS blocker (§4.1).
2. Revert `core/api/api.cpp` to original content + `initialize_api_layer()` (§4.2).
3. Rewrite `core/exception/exception.cpp` fully (§4.3).
4. Fix `core/api/thread_api.cpp` includes + GetThreadId stub (§4.4).
5. Fill in all remaining implementations + registrations in `core/api/kernel32.cpp` (§4.5).
6. `cmake -B build -S . && cmake --build build -j$(nproc)` → iterate until it links.
7. Run the existing test suite `tests/run_tests.sh` (or the individual hello_*.c
   tests through the emulator) to confirm nothing regressed.
8. Run WebView2:
   `./build/ThetaZ /mnt/hdd/Downloads/Microsoft.WebView2.FixedVersionRuntime.150.0.4078.83.x64/msedgewebview2.exe`
   Watch `process_imports` output for `!! unresolved:` lines. Implement each missing
   import (from both the IAT scan and the previous run's output list), e.g.:
   - AddVectoredExceptionHandler ✓ (now in exception.cpp)
   - GetInstallDetailsPayload, IsBrowserProcess, SignalChromeElf, SignalInitializeCrashReporting
     (Chrome/Edge crashpad internal exports — can be stubs returning 0/NULL)
   - VirtualQueryEx ✓, WriteProcessMemory ✓, ReadProcessMemory ✓
   - WaitNamedPipeW ✓, WideCharToMultiByte ✓
   - WakeAllConditionVariable / WakeConditionVariable ✓
   - Wow64GetThreadContext ✓, NtQueryInformationProcess ✓
   - RtlInitUnicodeString, RtlFreeUnicodeString (ntdll)
   - GetCommandLineW, GetModuleHandleExW, SetUnhandledExceptionFilter, GetCurrentThreadId,
     GetSystemInfo, GetNativeSystemInfo, GetProcAddress, LoadLibraryExW, GetFileAttributesW,
     GetFileSizeEx, FindFirstFileW/FindNextFileW/FindClose, CreateFileW, ReadFile, WriteFile,
     GetCurrentDirectoryW, SetCurrentDirectoryW, GetEnvironmentStringsW, GetEnvironmentVariableW,
     SetEnvironmentVariableW, GetModuleFileNameW, GetStartupInfoW, ExitProcess, TerminateProcess,
     GetLastError/SetLastError, Sleep, WaitForSingleObject, WaitForMultipleObjectsEx,
     CreateEventW, SetEvent, ResetEvent, CreateMutexW, CreateSemaphoreW, ReleaseSemaphore,
     CreateThread, SetThreadPriority, GetCurrentThread, SuspendThread, ResumeThread,
     GetSystemTimeAsFileTime, GetLocalTime, GetUserDefaultLCID, GetACP, GetOEMCP,
     IsValidCodePage, MultiByteToWideChar, SetUnhandledExceptionFilter, RaiseException,
     GetProcessHeap, HeapAlloc, HeapFree, HeapReAlloc, GetCommandLineA, GetCommandLineW,
     RtlVirtualUnwind, RtlLookupFunctionEntry, RtlCaptureContext, RtlUnwindEx, RtlRaiseException,
     KiUserExceptionDispatcher... (add each as a stub as the IAT scan reports it).
9. Re-run WebView2 after each batch; the crash-at-RIP-0 should move further into
   startup each time as imports resolve. Iterate until entry returns or a real logic
   gap is hit.
10. When WebView2 is stable, re-check the bigger apps; keep an audit file of what is
    implemented vs. stubbed so remaining work is visible.

---

## 6. KEY FILE / SYMBOL REFERENCE (so work can resume quickly)

- `core/api/api.h` — WinHandle, DWORD/LONG/BOOL/.../SIZE_T, TRUE_VALUE/FALSE_VALUE,
  `WINAPI_CALL` (`__attribute__((ms_abi))`), `ApiRegistry`, `register_api(dll,name,func)`,
  `initialize_api_layer`, `terminate_current_process`, `set_main_module_base`,
  `load_guest_module`, `set_crt_command_line`, `run_crt_exit_callbacks`.
- `core/api/kernel32.h` — NEW: extended types + ~40 WINAPI_* declarations +
  `initialize_kernel32_api()`. ← fix §4.1.
- `core/api/kernel32.cpp` — implementations + registrations. ← main work §4.5.
- `core/api/thread_api.cpp` — Wow64GetThreadContext/SetThreadContext/Suspend/Resume/
  GetThreadId. ← fix §4.4.
- `core/exception/exception.h` — ExceptionHandler, CPUContext64, WinContext,
  WinExceptionRecord, EXCEPTION_CONTINUE_*, set/get_active_exception_handler,
  set/get_unhandled_exception_filter. ← do NOT edit; implement it.
- `core/exception/signal_x86_64.cpp` — installs SIGSEGV/SIGFPE/SIGILL handlers that
  call `handler->dispatch()` and restore registers. Needs exception.cpp's class impl.
- `core/loader/main.cpp` — PELoader. `process_imports()` prints `!! unresolved:` for
  missing imports (the primary driver of the RIP-0 crash). ApiRegistry lookups are
  case-normalized (lowercase dll name).
- `core/api/process.cpp` — CreateProcessA, ExitProcess, GetExitCodeProcess,
  GetCurrentProcessId, terminate_current_process. ← do NOT touch.
- `core/api/user32.cpp`, `core/api/window.cpp`, `core/api/ucrt.cpp` — each has its own
  `initialize_*_layer()`. ← do NOT touch (except adding missing funcs if needed).
- `main_entry.cpp` — calls `initialize_api_layer()` then each `initialize_*_layer()`
  individually; loads argv[1] as the guest PE and calls its entry point.
- `tests/run_tests.sh` + `tests/hello_*.c` — regression suite.

## 7. HOST ENVIRONMENT NOTES
- Linux, x86-64. The emulator runs guest PEs in-process (same host process) using
  ms_abi calls; imports are resolved via ApiRegistry into the IAT.
- WebView2 test binary path: `/mnt/hdd/Downloads/Microsoft.WebView2.FixedVersionRuntime.150.0.4078.83.x64/msedgewebview2.exe`
- No git repo at `/mnt/hdd/...` (git is unavailable there; `git status` fails with
  "not a git repository"). The working dir `/home/eronic/home-smol` is a git repo but
  unrelated to this project path. Do not rely on git for this project.
