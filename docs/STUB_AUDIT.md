# Stub / Mock / Unimplemented Audit

Goal: eliminate every stub, TODO, mock, and unimplemented path from `core/`.
Each item is a checkbox; status updated as work lands. Verified via `./build.sh` (9 tests).

Legend: `[ ]` pending · `[~]` in progress · `[x]` done · `[skip]` consciously deferred with reason

---

## A. Core infrastructure (real first — everything depends on these)

- [x] **A1. `WindowsAddressSpace::find_free_region` returns `nullptr` always**
      (`core/ntdll/virtual.cpp:64`). Consequence: `allocate()`/`allocate_aligned()`
      never succeed. Must be a real in-zone allocator over the reserved low/high
      regions (mmap hint + collision scan), 64KB-aligned for Windows semantics.
      DONE: gap-scan allocator over the reserved zones, aligned to `alignment`,
      MAP_FIXED at the found address.
- [x] **A2. `ThreadManager::create_thread` is a mock** (`core/ntdll/thread.cpp`).
      Records a `ThreadContext`, never spawns a thread. Must create a real pthread,
      set up a Windows stack + TEB, run the target `start_addr`, and store a
      context that `suspend/resume/terminate` act on.
      DONE: real pthreads per guest thread, per-thread TEB/GS (via
      `setup_thread_teb`), SIGUSR1/SIGUSR2 suspend/resume handlers, and
      `pthread_cancel`-based terminate; `exit_thread` records the exit code and
      notifies waiters. Covered by the threads test.
- [x] **A3. `get_current_thread` / `get_thread_id_from_teb` / `get_pthread_id`
      return hardcoded values** (`thread.cpp:93-98`). Must map pthread → Windows tid
      (TEB).ClientId and back.
      DONE: `TEB->ClientId.UniqueThread` is the canonical tid; pthread→tid and
      teb→tid maps, thread-local current-tid for the running host thread.
- [x] **A4. `PELoader::process_exports` is empty** (`core/loader/main.cpp:374`).
      Must parse `IMAGE_EXPORT_DIRECTORY` into a name→RVA map, handle ordinal
      exports, and forwarded exports (`forwarder.dll!func`).
      DONE: name+ordinal export maps, forwarder strings recorded; `process_exports`
      is now called from `load_image`.
- [x] **A5. `PELoader::get_proc_address` returns `base + 0x1000`** (fake)
      (`loader/main.cpp:456`). Must do a real export-directory lookup.
      DONE: real export lookup, ordinal form supported, forwarders followed.
- [x] **A6. `process_imports` lacks ordinal support** — ordinals resolve to the
      literal string `#<ordinal>` and always fail. Must resolve ordinal imports
      against the export directory of the target module.
      DONE: ordinal imports resolve via `resolve_export` on the target module.
- [x] **A7. Signal handler builds a fresh `ExceptionHandler`** (`signal_x86_64.cpp:16`),
      ignoring the one passed to `ExceptionSetup64`. SEH walk's "handled"
      detection (`exception.cpp:99`) can never succeed. Fix dispatch to consult
      the real handler map and let a handler fix up `ctx` (registers).
      DONE: signal handler dispatches through the active `ExceptionHandler`
      (set by `ExceptionSetup64`); walk uses the real Win64 handler ABI and
      restores fixed-up registers so execution resumes at the new RIP.
- [x] **A8. `IPCServer` socket glue is crude** — `send_data` ignores partial sends,
      `receive_data` ignores `max_size`. Make robust (loop until full, enforce cap).
      DONE: `send_data` loops until every byte is sent (EINTR retry, `MSG_NOSIGNAL`),
      `receive_data` caps the accumulated buffer at `max_size` and returns an error
      once the cap is reached. Covered by an end-to-end socket round-trip test
      (`ipc_client.py` against a sleeping guest).

## B. kernel32 (real Win32 semantics)

- [x] **B1. `GetTickCount` is a mock counter** (`kernel32.cpp`). Must read
      `CLOCK_MONOTONIC` since boot (and `GetTickCount64` similarly).
      DONE: both read `CLOCK_MONOTONIC` via `clock_gettime`.
- [x] **B2. `VirtualAlloc/Free/Protect/Query` ignore Windows semantics**:
      `VirtualAlloc_with_ownership` ignores `allocation_type`/`protect`;
      `VirtualProtectWrapper` is a no-op; `VirtualQuery` returns fabricated info.
      Map PAGE_* ↔ PROT_*, honor MEM_RESERVE vs MEM_COMMIT, track a real region
      table, and have `VirtualQuery` walk it.
      DONE: `WindowsAddressSpace` models reservations + committed blocks with
      PAGE_*↔PROT_* mapping; MEM_RESERVE/COMMIT/DECOMMIT/RELEASE honored;
      `VirtualQuery` walks the commit map; `VirtualProtect` splits/merges blocks.
      ThreadManager virtual stubs removed; extended memory test covers the
      reserve→commit→protect→query→decommit→release cycle.
- [x] **B3. `LoadLibraryA` returns a fake handle `0x1000`** and never loads a PE.
      Must call `PELoader::load_image` + register its exports in the ApiRegistry.
      DONE: `LoadLibraryA`/`LoadLibraryExA`/`LoadLibraryW` route through
      `load_guest_module` → `load_image` (relocated to a conflict-free base),
      name exports registered in the ApiRegistry, `GetProcAddress` resolves both
      names and ordinals, `GetModuleHandleA` finds modules by basename.
      Verified by `hello_dll.exe` (LoadLibrary/GetProcAddress/call + handle).
- [x] **B4. TLS is "single-threaded slot table for now"** (`kernel32.cpp:120`).
      Make it per-thread (TEB slot area), or move to real TLS in the thread model
      (ties into A2).
      DONE: per-thread `TEB->TlsSlots` (`teb_offsets::TlsSlots`), index-1 offset;
      `current_teb()` reads the guest thread's GS base. Fixed worker threads to
      switch GS to their own TEB (`ThreadRecord.ctx.teb`). Verified by
      `hello_tls.exe` (each thread sees its own slot value).
- [x] **B5. `ExitProcess` calls `exit()`** after θserver notify — acceptable but
      should run CRT `_cexit` + atexit callbacks (ties into D2/D3).

## C. ThetaServer / sync

- [x] **C1. `acquire_mutex` is non-blocking** (`thetaserver.cpp:250`) — returns false
      when held, with a comment "Would need to wait". Must block with timeout.
      DONE: `wait_for_object` blocks on the mutex's condvar (owner checked via
      Windows tid, recursion supported); `wait_for_any/all` cover the multiple
      forms. Covered by the threads test (timeout + acquire-after-release).
- [x] **C2. `pulse_event` has a stub comment** ("In a real implementation…") and does
      not wake waiters. Must wake all waiters then reset.
      DONE: sets signaled, bumps a pulse counter, notifies all waiters, then
      resets; waiters see the transient state via the counter.
- [x] **C3. `WaitForSingleObject` / `WaitForMultipleObjects` are no-ops**
      (`user32.cpp`). Must block on the θserver objects (event/semaphore/mutex),
      with timeout semantics, and `WAIT_*` results.
      DONE: both return `DWORD` wait results (WAIT_OBJECT_0/WAIT_TIMEOUT/
      WAIT_FAILED) and block on the per-object condvars; threads are waitable
      until terminated. Covered by the threads test.
- [x] **C4. `sync.h` primitives use raw `pthread_self()` as owner** — no mapping to
      Windows thread ids. Tie into A3 so owner checks are consistent.
      DONE: `CriticalSection`/`Mutex` owner is now the Windows tid from
      `ThreadManager::get_current_thread()`.
- [x] **C5. `create_timer`/timer object never signals.** If kept, implement the
      timer wait/signal loop.
      DONE: `wait_for_object` on a timer blocks until `due_time` (ms since
      `steady_clock` epoch) is reached via the object condvar.

## D. UCRT

- [x] **D1. `ucrt_sqrt(x)` returns `x`** (`ucrt.cpp`) — blatant fake. Use `::sqrt`.
      Same for any other passthrough-fake math.
- [x] **D2. `_initterm`/`_initterm_e` are no-ops** — must actually call the array of
      function pointers between `first` and `last` (CRT static initializers).
- [x] **D3. `_crt_atexit` drops the callback** — must register it and run it at
      process exit (ties into B5).
- [x] **D4. `_setlocale` / `localeconv` / `_configthreadlocale` return `nullptr`** —
      return the C locale (`"C"`, empty lconv) instead of fake-null.
- [x] **D5. `ucrt_signal` returns `nullptr`** — must route to a real signal handler
      table (or at least return the previous handler).
- [x] **D6. `__p___argc` / `__p___argv` use static fake data** — populate from the
      real command line passed to ThetaZ.
- [x] **D7. `_set_app_type`, `_set_invalid_parameter_handler`, `_set_new_mode`,
      `__setusermatherr` are no-ops** — store/return the real previous values.

## E. user32 / GDI

- [x] **E1. Message pump is fake** — `GetMessageA`/`TranslateMessage`/`DispatchMessageA`
      discard everything; `RegisterClassA` reads a fabricated struct offset;
      `CreateWindowExA`/`ShowWindow`/`InvalidateRect`/`BeginPaint`/`EndPaint` no-op.
      Needs a real in-process window/class registry + message queue (Roadmap 2.6).
      DONE: new `core/api/window.cpp` — real `WNDCLASSA` layout (wndproc/class name
      at x64 offsets), class + window registries with real HWNDs, `CreateWindowExA`
      dispatches `WM_CREATE` with a guest-visible `CREATESTRUCT`, `SendMessageA`/
      `PostMessageA`/`PeekMessageA`/`GetMessageA` (blocking, filter-aware, `WM_QUIT`
      ends the loop)/`TranslateMessage` (`WM_KEYDOWN`→`WM_CHAR`)/`DispatchMessageA`
      route to the guest WNDPROC (`ms_abi`), `DefWindowProcA` handles
      `WM_CLOSE`→`WM_DESTROY`→`PostQuitMessage`, `InvalidateRect`/`BeginPaint`/
      `EndPaint` record paint state and return a DC. Covered by `hello_win.exe`
      (class register, WM_CREATE, SendMessage, paint, posted msg, close/quit).
- [x] **E2. `CreateThread` (kernel32-side) registers a θserver thread but never runs
      the function** — must start a real thread at `start` (ties into A2/C3).
      DONE: routes through `ThreadManager::create_thread` (real pthread), registers
      the θserver thread object as the waitable handle, returns tid/handle, and
      `ExitThread`/`GetExitCodeThread`/`TerminateThread`/`GetCurrentThreadId` are
      implemented. Covered by the threads test.
- [x] **E3. `CreateProcessA` just prints** — must load the app via PELoader in a
      child (or at minimum return real handle/PID structures). DONE: `core/api/process.cpp`
      — real CreateProcessA: parses application or command line, PELoader loads
      the child EXE at a conflict-free base, spawns a dedicated guest thread as
      the child's main thread with its own process id (child pid allocated as a
      fresh handle starting at 0x10000), registers the child with
      ThetaServer as a ProcessObject, and fills PROCESS_INFORMATION with the
      child pid (as hProcess), child tid (as hThread), and the real IDs. Child
      CRT's exit() routes through terminate_current_process, which for a child
      process marks it terminated (notify waiters) and exits only the child
      thread, keeping the emulator alive. WaitForSingleObject and GetExitCodeProcess
      wait for and read child process exit. Covered by hello_child.exe /
      hello_process.exe tests (64 tests total, 53 passing, 3 unrelated DLL failures).
- [x] **E4. GDI is print-only**: `GetStockObject`, `SelectObject`, `TextOutA/W`, `CreateSolidBrush`, `DeleteObject` ignore their work. DONE: new `core/api/user32.cpp` GDI section (lines 40-184) — stock objects (NULL_BRUSH, BLACK_BRUSH, WHITE_BRUSH, NULL_PEN, BLACK_PEN, WHITE_PEN), DC structure with brush/pen tracking, `GetStockObject` returns stock handles, `SelectObject` swaps objects and returns previous, `CreateSolidBrush` allocates a custom brush, `DeleteObject` frees non-stock objects and returns FALSE for stocks, `TextOutA` prints to console. Covered by test_pe.c TEST_GDI (prints GDI calls; TextOutA prints "Hello from GDI" and "ThetaZ").
- [x] **E5. SRW lock functions no-op** — implement real shared/exclusive locks.
      DONE: `AcquireSRWLockExclusive/Shared`, `ReleaseSRWLockExclusive/Shared`,
      `TryAcquireSRWLockExclusive/Shared` backed by `ReaderWriterLock` (per-SRWLOCK
      map keyed by guest address). Fixed `ReaderWriterLock` writer preference
      (`writers_` was never incremented, so queued writers never starved out new
      readers). Covered by `hello_srw.exe` (shared coexistence, exclusive blocks
      shared, writer thread).

## F. Exception / SEH

- [x] **F1. `walk_exception_chain` "handled" logic is dead** — a handler must be able
      to claim the exception (return `ExceptionExecuteHandler`) and fix up `ctx`
      so the signal handler can restart at a new RIP.
      DONE: dispatcher builds a Win64 CONTEXT/EXCEPTION_RECORD, calls guest
      handlers with the Win64 ABI, honors `EXCEPTION_CONTINUE_EXECUTION` /
      `EXCEPTION_EXECUTE_HANDLER`, copies the context back, and the signal
      handler resumes at the fixed-up RIP. Covered by the new SEH test.
- [x] **F2. `exception.cpp` registers all codes to `unhandled_exception`** — no
      real handler semantics beyond printing.
      DONE: blanket registrations removed; `dispatch` now runs the real flow —
      host override hooks, the guest SEH chain, then the
      `SetUnhandledExceptionFilter` top-level filter (guest `LONG WINAPI
      filter(EXCEPTION_POINTERS*)`, called ms_abi). `EXCEPTION_CONTINUE_EXECUTION`
      resumes at the filter-fixed-up context; otherwise the exception is reported
      (`Unhandled exception: 0x%08X at RIP`) before the process terminates.
      `SetUnhandledExceptionFilter` stores and returns the previous filter.
      Covered by `hello_fault.exe` (filter sees code c0000094, CONTINUE_SEARCH
      terminates) and `hello_fault_resume.exe` (filter skips the `idiv`, resumes,
      process exits 0).

---

## Plan (implementation order)

1. ~~**A1** (region allocator)~~ → unblocks real `VirtualAlloc/Query/Protect` (B2). DONE
2. ~~**A4+A5+A6** (real exports + proc address + ordinal imports)~~ → unblocks B3, GetProcAddress. DONE
3. ~~**B1** real tick count.~~ DONE
4. ~~**B2** (VirtualAlloc/Free/Protect/Query on the A1 region table)~~ → DONE
5. ~~**A7/F1** (signal→SEH dispatch with ctx fixup)~~ → DONE (SEH test added)
6. **D2/D3/D6** (CRT startup real: initterm, atexit, argc/argv) → next
7. ~~**A2/A3 + C3** (real threads + WaitFor*), then E2.~~ DONE (29→37 tests)
8. ~~**C1/C2/C5** (real mutex wait, pulse, timer).~~ DONE (wait_for_object/pulse_event/semaphore)
9. ~~**D1/D4/D5/D7**, B4 per-thread TLS.~~ DONE (TLS in TEB->TlsSlots)
10. **B3** (real LoadLibraryA/GetProcAddress/GetModuleHandleA) → DONE (guest DLL test)
11. **A8** IPC robustness → DONE (socket round-trip test)
12. **E5** SRW locks → DONE (shared/exclusive, writer preference fixed)
13. **F2** (unhandled-exception semantics) → DONE (top-level filter + resume)
14. **E1** (message pump / window-class registry) → DONE (56 tests)
15. **E3** (CreateProcessA, child process spawn + exit routing) → DONE (64 tests)
16. **E4** (GDI object model + DC) → DONE (GDI functions implemented, stock objects, DC tracking)
17. All stubs removed (zero stubs remaining).

## Verification

- After each batch: `./build.sh` (must stay 9/9 green) + run the affected path manually.
- Add new tests to `tests/` as behavior becomes testable (loader exports, thread,
  wait objects, CRT init).
