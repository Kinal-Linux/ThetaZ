# AGENTS.md — ThetaZ Master Reference

> This file (plus `docs/AGENTS.md`, the deep technical reference) is the COMPLETE
> memory of the ThetaZ project. Read both fully before touching any code. Between
> them they capture the entire architecture, every fixed address, every invariant,
> every known bug, and the full session history — so you can fix any file without
> re-reading the source.

---

## 1. WHAT THIS PROJECT IS

**ThetaZ** is an original, from-scratch C++17 **Windows compatibility layer / PE
emulator** for Linux x86-64. It loads real Windows `.exe`/`.dll` files (both PE32
32-bit and PE32+ 64-bit) and runs them **in-process** (no VM, no ptrace). Its
primary current target is running **32-bit (PE32) Windows apps** — specifically
the Office/Adobe installers — via a built-in **x86 interpreter**.

Key facts:
- **GPL-2.0 licensed, original code.** The `Wine/` and `ProtonGE/` directories in the
  tree are **study references only** — they are NOT compiled, linked, or included.
  Do not copy from them; stay original.
- **C++17, CMake ≥3.10**, GCC 14+ / Clang 17+. Only external deps: **libffi**
  (via pkg-config) and **libX11** (via pkg-config). pthread + mmap from libc.
- Guest memory is **identity-mapped below 4GB**: the host address of a low
  allocation IS its 32-bit guest address. This is the single most important
  invariant of the whole design.
- There are TWO execution models:
  - **64-bit PE guests**: host function pointers called directly with
    `__attribute__((ms_abi))` (guest import thunks are real host function addresses).
  - **32-bit PE guests**: executed by the built-in x86 interpreter
    (`core/x86/interp.cpp`). Imports resolve to fake "thunk" addresses in a
    reserved low region; the interpreter detects a call/jump to a thunk address and
    marshals the guest stack args into a host call via libffi.
- The whole pipeline for 32-bit guests now works end-to-end including an **X11
  GUI backend** (windows, event loop, GDI text rendering).

---

## 2. BUILD & RUN

```bash
cd /mnt/hdd/Desktop/Projects/C++/ThetaZ
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j8
./build/ThetaZ /path/to/app.exe
```

- Debug build adds `-g -O0 -Wall -Wextra`. Clean build = **0 errors**.
- Two artifacts: `ThetaZ` (executable) and `ThetaZLib` (shared lib, links first).
- Requires `libffi` and `x11` pkg-config modules.
- Runtime env vars (debug):
  - `THETAZ_TRACE_INSNS=1` — per-instruction trace + a 128-entry EIP ring buffer
    dumped on guest fault (`[Thetax86] eip=...`).
  - `THETAZ_TRACE_API=1` — logs every thunk-dispatched API call
    (`[api] dll!name argc=N args=[...] -> ret`).
- Regression test: `/tmp/seh_test.exe` (MinGW 32-bit SEH two-frame test) must print
  `resumed: caught=1 unwound=1` and exit 0. There is also `tests/` with MinGW
  fixtures and `build.sh` (older suite).
- **IPC port 12345**: a stale ThetaZ process (or a runaway child) holds port
  12345 and makes the next run die with "Failed to initialize IPC server". Clean up
  with `for p in $(pgrep -x ThetaZ); do kill -9 $p; done`. NEVER use
  `pkill -f ThetaZ` or `pkill -f "build/ThetaZ"` from a shell whose command line
  contains the word ThetaZ — it kills the shell itself (matches the path).

---

## 3. HIGH-LEVEL ARCHITECTURE

```
main_entry.cpp               # host main(): init order, loads argv[1], runs guest
core/loader/main.cpp         # PELoader: PE32/PE32+ parse, map, relocate, imports, exports
core/x86/interp.cpp          # 32-bit x86 interpreter (3442 lines) + fault handler
core/x86/api_dispatch.cpp    # thunk region, signature table, x86_dispatch_api, SEH resume
core/x86/guest_env.cpp       # 32-bit stack/TEB/PEB/params/TLS + per-thread env
core/ntdll/                  # ThreadManager, WindowsAddressSpace, sync, TEB/GS(64-bit)
core/api/                    # ApiRegistry + ~200 kernel32 + ucrt + ws2_32 + user32/gdi32 + window.cpp (X11 GUI)
core/exception/              # 64-bit host SEH: SIGSEGV/FPE/ILL -> guest dispatch
core/ipc/                    # IPCServer (TCP:12345) + ThetaServer object manager
```

### Boot order (main_entry.cpp)
1. `WindowsAddressSpace` init + layout print.
2. `ThreadManager` singleton.
3. `ExceptionHandler` + `ExceptionSetup64` (signal handlers).
4. `IPCServer` (callbacks for GetTickCount→0 and QueryInformationProcess→1).
5. API layer init: `initialize_api_layer()` (kernel32 + oleaut32 + ws2_32),
   `initialize_ucrt_layer()`, `initialize_user32_layer()` (print only),
   `initialize_window_layer()` (user32 window/message + X11 + event thread),
   `initialize_process_layer()`.
6. `set_crt_command_line(argc, argv)`, `theta_record_command_line`,
   `theta_record_environment()`, `initialize_kernel32_runtime()`.
7. `setenv` a battery of WINEPREFIX paths
   (`/home/eronic/Prefixes/ThetaZ/drive_c/...`).
8. `ThetaServer::register_process(getpid(), 0)`.
9. `ipc_server.initialize(12345)`.
10. `PELoader::load_image(argv[1])`; if `is_32bit`:
    - `init_api_dispatch()` (reserve thunk region),
    - `x86_setup_environment(base, 0, &stack_top, &teb_addr)`,
    - `x86_run_entry((uint32_t)entry_point)` and return its rc.
    Otherwise call the 64-bit entry point directly as `uint32_t (*)()`.
11. Fall through: `ipc_server.run()` blocks forever (join on the accept thread).

### The 32-bit guest world (identity-mapped, host addr == guest addr)
```
0x00400000           main image base (PE32 default; loader may pick 0x10000000/0x08000000/...)
0x00C00000+          fallback low image candidates
0x00E00000..0x00F00000  per-thread guest stacks (allocated downward from 0x00E00000)
0x00F00000           main guest stack top (1MB, grows down); esp starts (top-0x40)&~0xF
0x60000000..0x76000000  guest HEAP  (352MB, first-fit free-list, see docs/AGENTS.md)
0x76000000..0x76100000  API THUNK region (1MB, 32-byte stride, 32768 thunks)
0x7D000000..0x7DF00000  shared low-memory bump allocator (15MB): CRT ptrs, cmdline,
                        env block, FILE tokens, SEH record/ctx, argv, mapped views
0x7FFDF000           PEB (x86 layout)
0x7FFE0000           RTL_USER_PROCESS_PARAMETERS
0x7FFE1000           command line (UNICODE)
0x7FFE2000           TLS data block
0x7FFE3000           TLS array
0x7FFE8000..0x7FFF0000  per-thread TEBs (allocated downward from 0x7FFE8000, 0x1000 each;
                        TLS array = teb-0x3000, TLS data = teb-0x2000)
0x7FFF0000           main thread TEB (also kGuestTeb, the fs:0 SEH base)
0x78000000..         builtin module handles (+0x1000 per dll)
```
`x86_alloc_guest(addr,size)` = `mmap(addr, size, RW, MAP_PRIVATE|MAP_ANONYMOUS|
MAP_FIXED_NOREPLACE)`; returns 0 on failure.

---

## 4. THE THREE EXECUTION PATHS (understand this before everything)

1. **Main 32-bit guest thread** — `x86_setup_environment` then `x86_run_entry`:
   `X86CpuState{eip=entry, esp=(stack_top-0x40)&~0xF, fs_base=0x7FFF0000}`.
2. **Child process / spawned 32-bit threads** — `ThreadManager::create_thread`
   → `thread_entry` → `x86_run_guest_thread(entry, stack_base, stack_size, tid)`:
   fresh per-thread TEB (fs_base), own TLS array/data, low stack. On child
   (pid!=0) exit: `ThetaServer::terminate_process(pid, rc)` then `exit_thread`.
3. **Guest callbacks invoked from host** — `x86_call_guest(fn)` /
   `x86_call_guest_args(fn, args, nargs)`: copy the CURRENT interpreter state
   (`*g_fault_state`), push a sentinel return address (0xCA11B000/0xCA11B001),
   run a nested `x86_run`; RET to the sentinel halts. Used for SEH handler
   dispatch, atexit callbacks, enumeration callbacks, WNDPROC dispatch
   (`call_wnd_proc`).

Every 32-bit API call goes through `x86_dispatch_api` (see docs/AGENTS.md §3).

---

## 5. MODULE-BY-MODULE (short form; details in docs/AGENTS.md)

### core/loader/main.cpp — PELoader
- Parses DOS/COFF/optional headers for **PE32 (0x10B) and PE32+ (0x20B)**.
- `map_sections`: mmap PROT_NONE size_of_image; for 32-bit tries low candidate
  bases (0x10000000, 0x08000000, ... 0x00040000) with MAP_FIXED_NOREPLACE, then
  falls back to any high address. Writes headers + sections RW (exec for code).
- `apply_relocations`: type 3 = HIGHLOW 32-bit (add delta), type 10 = 64-bit.
- `process_imports`: for 32-bit images writes **thunk addresses** (`alloc_api_thunk`)
  into IAT slots (never host pointers); 64-bit images get host pointers. Ordinal
  imports resolved via ApiRegistry `find_function_ordinal`. Prints
  `!! unresolved: NAME` for missing (this is the primary debugging signal).
- `process_exports` / `resolve_export` / `resolve_forwarder`: name/ordinal
  tables, forwarder strings (`dll!func`, `dll.dll.#123`) followed recursively
  (depth cap 32), fall back to ApiRegistry.
- `load_image(path, preferred_base)`: thread-safe (mutex), detects base conflicts.
  Also used to load the FRESH child copy in CreateProcess self-relaunch.
- `set_final_protections`: **keeps EVERYTHING RW (code RWX)** — guest code patches
  IAT slots and data at runtime (delay-load, CRT self-modification).

### core/x86/interp.cpp — the interpreter
- `x86_run(state, opts)`: single `for(;;)` + `switch(op)`; prefixes (66/67/segment/
  F0/F2/F3) decoded per instruction. ModRM/SIB, 16-bit addressing under 0x67.
- Implemented: full arithmetic/logic/tests, shifts/rotates, string ops with REP,
  Jcc/LOOP, CALL/JMP rel and r/m, PUSHAD/POPAD, group3 mul/div, x87 (D8–DF with
  memory and register pseudo-ops, fpu_st[8] doubles, FCOMI/FCOMIP flags), and a
  large 0F two-byte + SSE/SSE2 set (MOVUPS/PS/PD/SD, pack/unpack, PADD/PSUB/PCMP,
  PCMPGTB/W/D, PUNPCKL/H, PACK*, PSLL/PSRL/PSRA imm8 71-73, PMOVMSKB, 0F AE
  FXSAVE/FXRSTOR/LDMXCSR/STMXCSR, fences, RDTSC, XADD 8/32, CMPXCHG, SHLD/SHRD,
  BSWAP, TZCNT/LZCNT/POPCNT/CRC32, MOVMSKPS, RCPPS/RSQRTPS, CMPPS, MOVNTI,
  MOVNTPS, PALIGNR/PBLENDW/PBLENDVB, CVT* etc.). See docs/AGENTS.md §2 for the
  exact list and known gaps.
- **Fault handling**: `interp_fault_handler` (SIGSEGV/SIGBUS/SIGFPE) — if
  `g_interp_active`, prints fault, dumps regs + trace ring, `siglongjmp` back into
  the CURRENT run's `sigjmp_buf`. Per-run state (`g_fault_jmp_ptr`,
  `g_interp_active`, `g_fault_state`, `g_callback_sentinel`) is saved/restored so
  NESTED interpreter runs (SEH dispatch, callbacks) fault into their own handler
  instead of unwinding host frames.
- **Thunk detection**: CALL rel32 (0xE8), JMP rel32 (0xE9), CALL/JMP r/m (0xFF
  opf 2/4) check `is_api_thunk_addr(target)`; if so route to
  `x86_dispatch_api`. JMP pops the return address already pushed by the call.
- Memory helpers: `mr8/16/32/64`, `mw8/16/32/64` (direct deref of identity-mapped
  addr), `fetch8/16/32`, `push16/32`, `pop16/32`, `seg_base` (FS→fs_base, GS→gs_base).
- `interp_fault(s, reason, ...)`: prints `[Thetax86] unhandled instruction/fault`
  and halts.

### core/x86/api_dispatch.cpp — thunks + signature table (see docs/AGENTS.md §3)
- **CRITICAL ABI FACTS** (each caused a real bug):
  - Host WINAPI functions are SysV on Linux (`WINAPI_CALL` is empty), so the ffi
    cif must use **FFI_DEFAULT_ABI** — NOT FFI_WIN64 (wrong argument registers).
  - Every guest arg is passed to the host as **`ffi_type_uint64`** — never
    `ffi_type_uint32` (truncates the upper half of pointer args; corrupts 64-bit
    results and pointer args).
  - Unknown-signature thunks must read `argc` 32-bit stack words themselves.
- Signature table (`kSignatures`): hand-authored dll!name→argc + flags (bit0
  cdecl) + `wide` bitmask (8-byte slots). Macros ST (stdcall), STW (stdcall wide),
  CT (cdecl), CTW. Covers kernel32, ws2_32, user32, gdi32, ntdll, oleaut32,
  ucrtbase/api-ms-win-crt/msvcrt/vcruntime140. `sig_dll_name` maps
  `api-ms-win-crt-*`→`ucrtbase.dll`, msvcrt/vcruntime→`ucrtbase.dll`.
- `alloc_api_thunk(fn, argc, stdcall, dll, name)`: consults the sig table for the
  real argc; builds the ffi cif (uint64 args); returns `kThunkBase + i*32`.
- `x86_dispatch_api`: reads args from `[esp]` (interpreter did not push a ret
  addr), calls host fn, returns rvalue in EAX (EDX = high 32 for 64-bit results),
  stdcall pops `4*argc`. **SEH resume**: after the call, if
  `x86_seh_resume_state().active` → apply {eip,esp,ebp} to the interpreter state
  (guest SEH handler rewrote context during RaiseException).
- `x86_alloc_proc_thunk(fn, name)` for GetProcAddress runtime bindings: guards
  ordinals passed as fake string pointers; consults sig table by name; default
  = stdcall 1-arg.

### core/x86/guest_env.cpp — 32-bit env (see docs/AGENTS.md §1 for offsets)

### core/api/ — the API surface
- `api.cpp`: `ApiRegistry` (lowercased dll→name/ordinal→void*), `alloc_guest_heap`
  (0x60000000, first-fit, coalescing), `alloc_guest_shared` (0x7D000000 bump),
  `load_guest_module`, `get/set_main_module_base`, `initialize_api_layer`
  (kernel32+oleaut32+ws2_32). **Duplicates/symbol collisions are a known hazard** —
  `kernel32_api_additions.cpp` is DEAD CODE that would collide with kernel32.cpp
  (never compiled).
- `kernel32.cpp` (base 30+): ExitProcess, Virtual*, CreateFile/WriteFile,
  critical sections (recursive_mutex map keyed by address), TLS (64 slots),
  GetTickCount/QPC, string conversion, console, condition variables, CreateThread
  (ThreadManager-backed), named pipes (dummy handles), locale/time. `g_last_error`
  is thread-local. `GetCurrentProcess`=(HANDLE)-1, `GetCurrentThread`=(HANDLE)-2.
- `kernel32_runtime.cpp` (~200 APIs + minimal user32/gdi32) — the workhorse. Key
  behaviors and magic constants in docs/AGENTS.md §4. Contains:
  - `RaiseException` **two-pass SEH dispatch** (docs/AGENTS.md §5).
  - `GetProcAddress` with api-set routing + ordinal→name mapping.
  - `EncodePointer/DecodePointer` — REAL encoding using the guest `__security_cookie`
    (docs/AGENTS.md §4.10).
  - user32/gdi32 minimal layer: GetDC (fake DCs from 0x10), GetStockObject
    (0x1000+n), TextOutA/W (routes via `theta_gdi_text` to X11), BitBlt, fonts,
    brushes, pens, GetClientRect, GetSystemMetrics, SystemParametersInfoW.
- `process.cpp`: CreateProcessA/W (self-relaunch + fresh child image copy), the
  **x86 PROCESS_INFORMATION layout** {hProcess@0, hThread@4, pid@8, tid@0xC},
  ExitProcess, GetExitCodeProcess (STILL_ACTIVE=0x103).
- `ws2_32.cpp`: Winsock2 over POSIX sockets, WSAData exactly 400 bytes,
  errno→WSA error table, socket handles 0x100+, REAL `select` (guest 260-byte
  fd_set → host fd_sets → rewrite ready sockets), `gethostbyname` builds a
  guest-visible hostent copy in shared low memory (no >4GB leak).
- `oleaut32.cpp`: BSTR (4-byte length prefix + data), VARIANT (16-byte opaque),
  error info. BSTRs now on the guest heap (alloc_guest_heap) — the old host-malloc
  >4GB pointer bug is FIXED.
- `ucrt.cpp`: printf family (own formatter; `g_vararg_slot` 8 for 64-bit, 4 for
  32-bit), malloc/free/calloc/realloc on the guest heap, FILE token registry
  (glibc FILE* ↔ guest token), CRT startup (`_set_app_type`,
  `_configure_narrow_argv`, `_initterm`, `_initterm_e`), `__p___argc/__p___argv/
  __p__acmdln/__p__environ` (shared low memory), atexit LIFO queue (guest
  callbacks via `x86_call_guest`), **`__C_specific_handler` — full table-driven
  SEH dispatch** (scope tables at [frame+8], ImageBase-relative RVAs, filter
  calls, `__finally` unwinds, DISPATCHER_CONTEXT; see docs/AGENTS.md §5).
- `window.cpp` — the X11 GUI layer (docs/AGENTS.md §6).
- `path_resolver.cpp` / `environment.cpp` — mostly scaffolds; the REAL path
  translation is `wine_to_host` in kernel32_runtime.cpp (maps drive letters/UNC
  to `$WINEPREFIX/drive_c/`, i.e. `/home/eronic/Prefixes/ThetaZ/drive_c/`).
  NOTE there are TWO prefix concepts: WINEPREFIX (paths) vs `~/.ThetaZ`
  (environment.cpp).

### core/ntdll/
- `thread.cpp/h`: `ThreadManager` singleton. `thread_entry` routes 32-bit guest
  threads to `x86_run_guest_thread`. tids start at 1, never recycled. Suspend/
  resume via SIGUSR1/SIGUSR2. TEB+0x048 = ClientId.UniqueThread.
- `virtual.cpp/h`: `WindowsAddressSpace` — two 8GB PROT_NONE zones (low + high
  shifted by 0x7FFFFFFE00000000); reserve/commit/decommit/release/protect/query.
  `page_to_prot` masks GUARD/NOCACHE/WRITECOMBINE.
- `sync.cpp/h`: Semaphore/Event/CriticalSection/Mutex/SpinLock/ReaderWriterLock
  (recursive, owner = Windows tid). ThetaServer has a SECOND parallel set.
- `teb.cpp/h`: **64-bit guest** TEB/PEB via GS (`arch_prctl(ARCH_SET_GS)`). Linux
  libc uses FS, so GS is free. x64 offsets: TEB.Self=0x30, PEB=0x60,
  ExceptionList=0x0, StackBase=0x8, TlsSlots=0x1800, TEB_SIZE=0x2000;
  PEB.ImageBase=0x10, ProcessParameters=0x20; params.ImagePathName=0x60,
  CommandLine=0x70. (The 32-bit guest uses FS-based TEB — different, see
  docs/AGENTS.md §1.)

### core/exception/ + core/ipc/
- exception: 64-bit host SEH (SIGSEGV/FPE/ILL → ExceptionCode → dispatch order:
  host handlers → vectored (AddVectored*) → guest SEH chain → UnhandledExceptionFilter).
  Vectored handlers are file-local linked list. `walk_exception_chain` uses
  x64 frame layout {Handler@0, Prev@8}.
- ipc: TCP 12345 server (framed [type,length,request_id] messages), plus
  `ThetaServer` object manager (handles from 0x1000; process/thread handles are
  pid/tid; `[θserver]` logs; wait_for_object by type; wait_for_any/all are 5ms
  polling loops).

---

## 6. KEY INVARIANTS & GOTCHAS (do not violate)

1. **Guest memory is identity-mapped**: a host pointer returned to a 32-bit guest
   MUST be < 4GB. Use `alloc_guest_heap` (0x60000000) or `alloc_guest_shared`
   (0x7D000000). Never return host `malloc`/stack pointers to 32-bit guests.
2. **32-bit struct layouts** are hand-written and OFTEN differ from the host
   structs: `PROCESS_INFORMATION` (16 bytes, all u32), `WSADATA` (400 bytes,
   static_assert'd), `STARTUPINFOA/W` (68 bytes; wShowWindow@48, hStdInput@56,
   hStdOutput@60, hStdError@64), x86 CONTEXT offsets (Eip=0xB8, Esp=0xC4,
   Ebp=0xB4), x86 TEB offsets. Writing a 64-bit layout corrupts the 32-bit caller.
3. **Thunk dispatch ABI**: FFI_DEFAULT_ABI + ffi_type_uint64 for ALL args.
4. **X11 locking** (window.cpp): never hold `g_window_mutex` while calling Xlib.
   `g_x11_mutex` serializes all Xlib calls; lock order is ALWAYS
   g_window_mutex → (copy data, release) → g_x11_mutex. ShowWindow's
   self-deadlock (re-locking g_window_mutex inside x11_paint_window) was a real bug.
5. **XNextEvent deadlock**: Xlib's blocking `XNextEvent` never wakes when another
   thread uses the display. The event loop MUST poll `XPending()` + `usleep(5000)`.
6. **EncodePointer key**: the guest `__security_cookie` at `image_base+0x66e780`
   is randomized per run. App decodes with `ror(ptr ^ cookie, cookie&0x1f)` so
   EncodePointer must be `rotl(ptr, cookie&0x1f) ^ cookie`.
7. **Stale IPC process** on port 12345 kills the next run (see §2).
8. **Never `pkill -f ...ThetaZ...`** from a shell whose own command line contains
   the pattern (it self-kills). Use `pgrep -x ThetaZ`.
9. **`gethostbyname`** leaks a host `hostent*` (>4GB) to 32-bit guests — latent.
10. **BSTR** (oleaut32) uses host malloc — latent (see above).
11. **Two prefix systems** (WINEPREFIX vs ~/.ThetaZ) can disagree.
12. `kernel32_api_additions.cpp` is dead code with duplicate symbols — never add
    it to the build.

---

## 7. SESSION HISTORY (the OfficeSetup / GUI push)

Target app: `/home/eronic/Downloads/OfficeSetup.exe` — PE32/32-bit, base 0x400000,
entry 0x7FA3CC, imports OLEAUT32 + KERNEL32 only, delay-imports incl.
GetSystemTimePreciseAsFileTime/GetTempPath2W; runtime-binds ws2_32 (ordinal 115
WSAStartup), user32, gdi32, api-sets.

What was fixed to get it through ALL of startup (do not regress):
- `TEST r/m,imm` (F7/0) was testing EAX instead of the r/m operand — the key bug
  that unblocked the app's resolver.
- SSE2: PUNPCKL/H BW/WD/DQ, PACKSSWB/USWB/SSDW, PCMPEQB/W/D, PMOVMSKB,
  0F AE (FXSAVE/FXRSTOR/LDMXCSR/STMXCSR/fences), 0F 1F NOP, 0F 31 RDTSC.
- `moffs` (A0–A3) displacement uses ADDRESS-size, not operand-size.
- `/GS cookie failure`: `WSAData` was 408 bytes (host `char* lpVendorInfo`) vs
  guest 400; WSAStartup memset clobbered the stack cookie at [esp+0x194]. Fixed
  with `uint32_t lpVendorInfo` + `static_assert(sizeof==400)`.
- `IsProcessorFeaturePresent(23)` (PF_FASTFAIL_AVAILABLE) returns FALSE.
- `GetProcAddress` ordinal path: ordinals must map to names BEFORE thunk alloc
  (an ordinal value used as a string pointer crashed at host addr 0x73).
- `GetModuleFileNameA/W` fall back to `get_main_module_base()` when hModule=0.
- `CommandLineToArgvW` always returns argv[0]="thetaz.exe".
- `CreateProcessA/W` self-relaunch ("RELAUNCHED" bootstrap): when the target path
  matches the main module's name prefix, **load a FRESH copy** of the image
  (own .data/relocations) so the child is a true process, then run it via
  `x86_run_guest_thread`. PROCESS_INFORMATION written as the x86 layout.
- 32-bit child emulation: `x86_run_guest_thread` (per-thread TEB below 0x7FFF0000,
  per-thread TLS array/data, low stack below 4GB).
- **GUI layer** (this push): X11 backend, guest WNDPROC via interpreter,
  32-bit struct layouts, GDI text rendering, ShowWindow deadlock, XPending polling.
  Verified end-to-end with `/tmp/opencode/gui_test.c` (MinGW 32-bit):
  RegisterClassA → CreateWindowExA (WM_CREATE via interpreter) → ShowWindow (X11
  mapped) → Expose→WM_PAINT→guest WNDPROC→BeginPaint/TextOutA → X11 text.
- New APIs: `IsDBCSLeadByte`, `GetStartupInfoA`, `__p__acmdln`.

### FIXED — OfficeSetup parent teardown crash (HeapSize returned 0)
After self-relaunch the child exits (code 0/1) and the PARENT's teardown crashed:
```
guest fault at eip=<garbage> e.g. 0xf61f8dcb / 0x47627b7a
esi=0x6002cca0 edi=0x6002d09c ebp=0x00effe48 esp=0x00effe24
```
**Root cause (found & fixed this session):** the app's grow path (0x8234ba-0x82353d)
calls its calloc-realloc `0x820603(old, count, size)`, which does:
`oldsize = HeapSize(heap,0,old)` → `newbuf = HeapReAlloc(old, newsize)` →
`memset(newbuf + oldsize, 0, newsize - oldsize)`. The emulator's `WINAPI_HeapSize`
**always returned 0**, so the memset became `memset(newbuf, 0, newsize)` — the app
**zeroed the entire freshly-realloc'd buffer**, destroying the old entries that
HeapReAlloc had just copied. The container's `{first, last, end}` (decoded from
0xa965f0, e.g. {0x6002cc78, 0x6002d154, 0x6002d478}) still spanned the full range,
so the teardown (0x823588) walked the zero region, decoded 0 →
`ror(0 ^ cookie, cl)` = rotation of the cookie → `call` → SIGSEGV (crash EIP always
a rotation of the cookie — confirmed entry==0).
Container anatomy (verified with fault-time dumps): the static container at
0xa965f0 = head node {encode(first), encode(write), encode(end)}; the heap buffer
at first = 4-byte slots, each an encoded function pointer (decodes into .text,
e.g. 0x007fb9f0/0x008728ab); the app's grow places the first new entry at
`newbuf + count*4` and cookie-fills `[newbuf+count*4, newbuf+newcap*4)`; the
teardown iterates `[first, write)` top-down, skipping cookie slots, marking each
processed slot with the cookie, and re-reads the head node after every call
(destructors may remove themselves).
**Fix:** `WINAPI_HeapSize` (and `LocalSize`/`GlobalSize`) now return the real
guest-heap block payload size via the new `ThetaZ::HeapBlockSize(ptr)` helper
(api.h/api.cpp; `b->size`, 8-aligned). The app's memset now only clears the tail
beyond the old allocation, the copied entries survive, and the teardown runs all
registered destructors cleanly.
**Result:** OfficeSetup now runs to completion of the parent teardown; the child
exits 0/1, no interpreter fault, and the app itself calls ExitProcess(0xC0000417)
(= STATUS_INVALID_CRUNTIME_PARAMETER — the app's own next failure path, a NEW
investigation item). SEH regression test still passes.

### FIXED — OfficeSetup ExitProcess(0xC0000417) (HEAP_ZERO_MEMORY not honored)
After the HeapSize fix, the parent's teardown hit the CRT `_invoke_watson` path:
`RaiseException(0xC0000417)` at eip 0x8070fe, then `UnhandledExceptionFilter(0)`
→ `ExitProcess(0xC0000417)` (exit code 23).
**Root cause (found & fixed this session):** `WINAPI_HeapAlloc` ignored
`dwFlags` — `HEAP_ZERO_MEMORY (0x8)` was never honored, so `calloc`-style
allocations returned blocks with stale contents. Sequence: the app's env-copy
helper 0x82c789 (`GetEnvironmentStringsW` → `malloc` copy of the block) got
0x60001520 (0x192e bytes); the app later freed it (0x8240ec) and its registry
`calloc(512,4)` (init 0x81c677 → 0x8258ff → `HeapAlloc(0x8, 0x800)`) REUSED the
same address — but the block was NOT zeroed, so the registry's tail (entries 3+)
still held the env block bytes (`/usr/bin/bash`, `SHELL=...`, `SESSION_MANAGER=...`
— the string content varied per run with the host env). The teardown
(0x81ee50 node release → 0x82b9bc handle lookup, lim@a96838=0x40) walked those
non-zero "entries" → garbage handle 0x60001d20 (read exactly one-past the 0x800
block = guest-heap chunk header) → `_invalid_parameter` (errno 0x9) →
`_invoke_watson`. On real Windows the calloc'd block is zeroed, so the walk sees
empty slots and exits cleanly.
**Fix:** `WINAPI_HeapAlloc` now memsets the block when `dwFlags & 0x8`
(HEAP_ZERO_MEMORY); `LocalAlloc`/`GlobalAlloc` zero when `uFlags & 0x40`
(LMEM_ZEROIN/GMEM_ZEROIN). Also removed dead vars `g_shared_env_block` and
`g_next_tls_slot` (unused-var warnings).
**Result:** OfficeSetup runs the FULL parent teardown to a clean
`ExitProcess(1)` — no 0xC0000417, no interpreter fault, no watson. Child exits
0/1 as before. SEH regression (`/tmp/seh_test.exe`) and the GUI test
(`/tmp/opencode/gui_test.c`, i686-w64-mingw32) still pass.

### DONE — API completion push (5 unresolved APIs, real SEH handler, latent 4GB fixes, timers)
- **5 unresolved kernel32/ntdll APIs implemented**: `ExitThread`,
  `FreeLibraryAndExitThread` (process.cpp, primary-thread check via
  `ThreadManager::instance().get_current_guest_pid()`), `RtlUnwind` (kernel32_runtime:
  walk fs:0 chain, X86SehResume transfer), `CreateTimerQueueTimer`/
  `DeleteTimerQueueTimer`/`CreateTimerQueue`/`DeleteTimerQueue` (timer thread +
  `x86_run_guest_callback`), `RtlCaptureStackBackTrace`. OfficeSetup: 0 unresolved.
- **Real `__C_specific_handler`** (ucrt.cpp ~612): table-driven scope dispatch —
  scope table at `[frame+8]`, entries {begin,end,handler,jump} (16B, ImageBase-
  relative RVAs), `0xFFFFFFFF` = __except sentinel, filter calls, `__finally` unwinds
  (pass 2, rec flags & 0x6), TargetIp written to the guest DISPATCHER_CONTEXT
  (56B, offsets in docs §5). `build_guest_dispatch_context` +
  `call_seh_handler` (RaiseException + RtlUnwind); **SEH handler addresses can be
  IAT thunks** — resolved via `api_thunk_at`→host fn, never guest-called.
  `vcruntime140.dll`/`vcruntime140_1.dll` added to `is_builtin_dll`.
  Test: `/tmp/opencode/cs_handler_test.exe` (hand-built scope tables) — pass.
- **Latent >4GB pointer bugs fixed**: BSTR → guest heap; `gethostbyname` →
  guest-visible hostent in shared low memory; `RegisterClassA` read the class name
  as a host-pointer-width (slurped the neighbour dword).
- **Real ws2_32 `select`**: guest 260-byte fd_set (u32 count + 64 SOCKETs) →
  host fd_sets via wsock_fd → rewrite ready sockets.
- **GUI timers**: SetTimer/KillTimer + WM_TIMER background thread; signature
  entries SetTimer(4)/KillTimer(2). Test `/tmp/opencode/timer_test.exe`: 3 fires.
- **Nested callback sentinel bug FIXED**: a callback (WNDPROC) dispatching an API
  that dispatches another callback (DestroyWindow → WM_DESTROY) cleared
  `g_callback_sentinel`, so the outer RET executed sentinel bytes
  (fault eip=0xca11b002). All three callback sites now save/restore via
  `x86_prev_callback_sentinel()` — timer_test now exits cleanly with no fault.
- **Full regression green**: SEH (`resumed: caught=1 unwound=1`), cs_handler
  (`resumed flag=1`), GUI (window + text), timers (3 fires, exit 0, no fault),
  OfficeSetup (0 unresolved, 0 faults, clean `ExitProcess(1)`).

### DONE — INTERP completeness push (new opcodes, x87 float conversion, 3 real bugs)
- **CMPXCHG8B (0F C7 /1)** (interp.cpp, before BSWAP): regf==1 + memory operand; eq → ZF=1 + store ECX:EBX→m64; neq → ZF=0 + load m64→EDX:EAX. Body in inner `{}` (declarations in a switch case). Verified by interp_ops3 (success + mismatch).
- **`memcmp` missing-import fix**: MinGW imports it from `api-ms-win-crt-private-l1-1-0.dll`; `ucrt_memcmp` existed but was unregistered for the PRIVATE api-set → guest jumped to 0x1 → SIGSEGV. Added `register_ucrt_api("api-ms-win-crt-private-l1-1-0.dll", "memcmp", ...)` (ucrt.cpp).
- **interp_ops3.c test harness** (tests/x86, third fixture): CMPXCHG8B, x87 (21 checks), REP string ops, CMOVcc (16 + mem), SETcc, Jcc rel8/rel32, LOOP/LOOPE/LOOPNE/JECXZ, PUSHAD/POPAD, PUSHFD/POPFD — **ALL PASS; ZERO interpreter bugs found**. All 6 initial failures were test bugs: (1) `inc; E2` is do-while — body runs once on entry + after each jump (ecx=4 → 4 body runs, eax=4); (2) REPE stops at first unequal element — a "find" test needs REPNE; (3) SETGE/JGE expectations backwards for `mov $1,eax; cmp $2,eax` (1>=2 is false); (4) `"+c"(n)` writeback clobbers n between two rep tests — reset it; (5) `inc` body sets ZF=0 → LOOPE stops immediately — body must be LEA to preserve ZF; (6) AT&T traps: `sete %0`→`%b0`, `scasd`→`scasl`, `pushfd`→`pushfl`.
- **New 0F two-byte opcodes**: XADD r/m8 (0F C0), PCMPGTB/W (64/65), PSLLW/D/Q +
  PSRLW/D/Q + PSRAW/D + PSLLDQ/PSRLDQ by imm8 (71/72/73, reg-field op selector),
  EMMS (77), SHLD/SHRD r/m,reg,imm8/CL (A4/A5/AC/AD, 16/32-bit, CF/OF correct),
  MOVNTPS/MOVNTPD (2B), MOVNTI (C3), MOVMSKPS/PD (50), RCPPS (53) / RSQRTPS (52)
  PS/PD/SS/SD, CMPPS/CMPPD/CMPSS/CMPSD (C2, imm8 pred, NaN-aware), 0F 38 escape
  (PSHUFB 00, PABSB/W/D 1C/1D/1E, POPCNT F3-F0, CRC32 F2-F1), 0F 3A escape
  (PALIGNR 0F, PBLENDW 0E, PBLENDVB 40), TZCNT/LZCNT (F3 0F BC/BD; LZCNT count =
  31-pos, CF set when src==0).
- **Bug 1 — F3 0F 7E/6E (MOVQ xmm, r/m64 load)**: GCC emits `f3 0f 7e` for 64-bit
  xmm loads; 0F 7E was treated as MOVD store, so EVERY 64-bit SSE load wrote
  garbage. All 66-prefixed SSE2 ops "failed" until this was fixed.
- **Bug 2 — MOVUPS-family stores (0F 11/29/2B)**: the source was read from the
  DESTINATION memory (its own old contents) and reg-reg stores wrote to the reg
  field; stores now source `xmm[regf]` and write `rm`.
- **Bug 3 — x87 m32/m64 memory ops (D8/DC mem, D9/DD /0,/2,/3)**: FLD/FADD/etc
  reinterpreted the raw IEEE bits as integers (printf %f printed garbage like
  1082130432.0/0.0); now converted via f32_to_double/f64_to_double, FST/FSTP
  store double_to_f32/f64_bits. Also added FCOMI/FUCOMI (DB /6,/5), FCOMIP/
  FUCOMIP (DF /6,/5 with pop) setting ZF/PF/CF + C0/C2/C3.
- **Test harness started** (todo #3): `/tmp/opencode/newops_test.c` (26 checks:
  xadd, pcmpgt, all 71/72/73 shifts, emms, shld/shrd, movntps, tzcnt/lzcnt/bsf,
  popcnt, crc32, pshufb, pabs) + `/tmp/opencode/newops2_test.c` (8 checks:
  movmskps, rcpps/rsqrtps, cmpps, movnti, palignr, pblendw) — ALL PASS. Most
  "failures" during bring-up were test-side endianness mistakes, not emulator bugs.
- **Audit status**: 1-byte map 239/256 (17 missing = prefixes + 8086 relics);
  0F two-byte 177 implemented; remaining gaps are ring-0/priviledged + MMX-family
  (mm0-7 alias to fpu_st not done) + rare SSSE3/SSE4. 0F 2B/50/52/53/64/65/71-73/
  77/A4/A5/AC/AD/C2/C3/D6-rework + 0F 38/3A escapes verified against MinGW-GCC
  emitted encodings (F3 0F 7E loads, 66 0F D6 stores).
- **Regression**: SEH (`resumed: caught=1 unwound=1`), cs_handler (`resumed
  flag=1`), timer (3 fires, 0 faults), GUI (window+text), newops 26/26, newops2
  8/8, OfficeSetup (0 unresolved, 0 faults, clean `ExitProcess(1)`), build 0 errors.

### DONE — GUI CONTROLS (BUTTON/STATIC/EDIT/LISTBOX/COMBOBOX, WM_COMMAND)
- `lookup_control_class`: user32 predefined classes (case-insensitive) with NO
  guest WNDPROC — behavior is host-side in window.cpp. `Window` gained
  `parent`, `id` (hMenu low word), `ctrl_type`, `checked`.
- `CreateWindowExA` control classes → X11 **subwindows** of the parent's X11
  window (Exposure|Button input only, no WM_DELETE); WM_CREATE skipped (no proc).
- **WM_COMMAND (0x0111)** to parent: wParam=MAKEWPARAM(id,BN_CLICKED=0),
  lParam=ctrl hwnd. X11 ButtonRelease → `post_button_command` (posted);
  `SendMessageA(btn, BM_CLICK)` → synchronous parent call (lock discipline:
  NEVER call a guest WNDPROC while holding g_window_mutex — restructured
  SendMessageA so the BM_CLICK forward happens after the lock is released).
- **Checkboxes** (BS_CHECKBOX/BS_AUTOCHECKBOX): `checked` state; BM_GETCHECK/
  BM_SETCHECK roundtrip; X11 release toggles + repaints then posts BN_CLICKED.
- `x11_paint_window` renders controls: buttons centered, checkboxes with
  [x]/[ ] marker, statics left-aligned. DestroyWindow cleans children +
  X11 maps.
- **New test**: `tests/ctrl_test.c` → "CTRL PASS" (creation of button+checkbox+
  static, BM_GETCHECK=0 → BM_SETCHECK(1) → BM_GETCHECK=1, BM_CLICK → WM_COMMAND
  ids 1 and 2 received by the parent in order).
- Regression green after controls: SEH, cs_handler, timer, GUI, ctrl,
  newops 26/26, newops2 8/8, OfficeSetup 0 unresolved / 0 faults / ExitProcess(1).

### DONE — C++ EH (`__CxxFrameHandler` V4 catch + unwind, `_CxxThrowException`)
- Full MSVC x86 C++ EH: `ucrt___CxxFrameHandler` (V2/V3/V4 FuncInfo, EH3 + EH4
  record probe by FuncInfo magic), `ucrt__CxxThrowException` (4-word
  RaiseException 0xE06D7363), registered on vcruntime140.dll only.
- Pass 1: IPToStateMap state → innermost-first TryBlockMapEntry scan → catch
  TypeDescriptor-name match against the throw's CatchableTypeArray (PMD
  mdisp/pdisp/vdisp incl. virtual-base vbtable indirection) → frameless catch
  funclet resume via context {Eip=funclet, Ebp=frame+8, Esp=frame, EDX=object+disp}.
- Pass 2 (UNWINDING|EXIT_UNWIND): destructor funclets via `cxx_unwind_frame`
  (states down to 0, action < 0x80000000) through `x86_call_guest_args_ebp`.
- **Three real bugs found & fixed this session**:
  1. FuncInfo field offsets were off by 4 (read nTryBlocks@+16 instead of +12,
     nIPMapEntries@+24 instead of +20) — every throw "fell through unhandled"
     (`state=1 ntry=0x4031e8` in debug). Fixed to the true ehhdr.h layout.
  2. `kCtxEdx` was 0xAC (the x86 CONTEXT Ecx slot) — the catch EDX was read from
     the wrong slot; 0xA8 is Edx. (Worked by accident before because both the
     handler write and the resume read used 0xAC.)
  3. **Sig-table rows keyed `"vcruntime140.dll"` never match `find_sig`** (it
     canonicalizes to ucrtbase.dll), so the `_CxxThrowException` thunk landed in
     the `!t.info.known` dispatch path — which had NO SEH resume check. The
     raise's resume was consumed by the NEXT api dispatch instead (the
     thrower's fallthrough `puts` got redirected to the catch funclet). Fixed:
     row is now `CT("ucrtbase.dll","_CxxThrowException",2)`, AND the `!known`
     path now applies the SEH resume after ffi_call (defense in depth).
- New test `tests/cxx_eh_test.c` (hand-built V4 FuncInfo + EH4 records, naked
  catch funclet, pass-2 unwind funclet): `CXX PASS caught=0x403024 expected=
  0x403024 unwound=1`, exit 0. Regression fully green (SEH, cs_handler, timer
  3 fires, GUI, CTRL PASS, newops 26/26 + 8/8, OfficeSetup 0 unresolved /
  no faults).

### DONE — Flag-exact shift/rotate pass + fuzz harness (802-row golden, ZERO diff vs native)
- New `tests/x86/fuzz_ops.c` (i686 MinGW guest + same source built natively
  `-no-pie`): 802 deterministic flag rows, one probe per row (first 16 rows
  single-op input sets: `not/xor/and/or/xadd/cmpxchg/shl/shr/sar/rol/ror/rcl/rcr/
  shld/shrd/shl1/sar1/.../shl_cl/shr_cl/sar_cl/rol_cl/ror_cl/rcl_cl/rcr_cl` — plus
  the legacy CMP/SUB/OR residue-chaining probes, which are NOT deterministic;
  golden regeneration requires clearing `.bss` state between runs). Row format
  `op a b eax ecx edx flags`; guest vs golden diff is the flag/value oracle.
  Run: build natively `gcc -O0 -no-pie` → `fuzz_golden.txt`; run in ThetaZ →
  filter rows; `diff`. **Current: 802/802 ZERO diff.** Regression tools:
  `/tmp/opencode/native_probe.c` (hand-written inline-asm ground truth).
- **Shift count masks**: x86 masks counts by 32 for ALL operand sizes. The
  emulator had `c &= 15` (16-bit) / `c &= 7` (8-bit) → e.g. `shl ax, cl` with
  `cl=0xf8` shifted by 8 instead of 24 (native ground truth: result 0x0000).
  All three `shift8/16/32` now `c &= 31`. The 16-bit result-row values
  (`0x3ab11100` vs native `0x3ab10000`) and CF were the visible symptom.
- **Logical-op AF leak**: OR/AND/XOR/NOT/TEST helpers preserved AF (Intel clears
  it for logical ops). Replaced all 38 occurrences of `~(F_ARITH & ~F_AF)` with
  `~F_ARITH`. This was ALSO the root cause of the earlier bogus "AF leak"
  signal: the OR-probe's captured flags (0x92 → M_ALL mask → 0x90) were
  definitively wrong vs native 0x80.
- **ROL/ROR**: CF now always written (was only on c==1); OF only for c==1.
  ROL1/ROR1 OF formula corrected (`MSB_new ^ MSB_old`; ROL1: `cf_final ^ d>>31`
  where `cf_final == MSB_old`).
- **RCL/RCR rotate-through-carry**: the loop fed back `flag_cf(s)` (the ORIGINAL
  CF, never updated) instead of the rotating local `cf` — RCL degraded to a
  plain shift (163 iterations → 0), RCR OR'd 0x80000000 every iteration
  (→ 0xffffffff). Now `cf = flag_cf(s)` once, feed local `cf`, keep
  `cf = nc` each iteration. **RCR1 OF** = `MSB(src) ^ MSB(result)` (same rule as
  ROR1; `MSB(src) ^ final-CF` was wrong — golden `rcr_1` rows showed OF=0 with
  `MSB(src)=1`).
- **SHL/SAR CF guards** (16/8-bit, count 16..31): `SHL: cf = (c <= w) ?
  (d >> (w-c)) & 1 : 0` (was shifting by negative/≥width — UB); `SAR: cf =
  (d >> ((c >= w) ? w-1 : c-1)) & 1`.
- **SHLD/SHRD OF**: `(res ^ dst)` (MSB of result vs ORIGINAL dst), was `res ^
  src` — shld_i1 rows showed OF missing (0x804 vs 0x04) and extra (0x881 vs
  0x81) on the same row-pair.
- **CMPXCHG** (0F B0/B1, all widths): replaced the legacy ZF-only logic with
  full `cmp8/16/32` — golden rows were `0x804` (OF+PF from the compare) vs guest
  `0x14`.
- **Fixture bug found**: `cmpxchg_c`/`xadd_c` probes used modrm `0x34` (reg=ESI,
  uninitialized) → value columns nondeterministic (`0x482104c4` vs
  `0x4820f4d8` for identical inputs). Fixed to `0x0C` (reg=ECX); golden
  regenerated. cmpxchg value/flag columns are deterministic (compare EAX vs
  mem); xadd value columns depend on the reg field, so reg must be initialized
  by the probe.
- Regression after the pass: SEH `resumed: caught=1 unwound=1`, cs_handler
  `resumed flag=1`, timer 3 fires, CTRL PASS, GUI hello window + text,
  OfficeSetup 0 unresolved / 0 faults / clean `ExitProcess(1)`, fuzz 802/802
  ZERO diff, build 0 errors (only the 2 pre-existing unused-var warnings at
  interp.cpp:770/3792).

### FIXED — OfficeSetup child-image corruption (name-keyed unload + relaunch-base fallback)
After the pid→base registry, TEB block allocator, and per-pid GetModuleFileName
fixes, child fresh copies were loading ON TOP of the parent at 0x400000 (callers
appeared at `0x6e0d81` = parent base + RVA 0x2e0d81), and the run died with a
host SIGSEGV. Two loader bugs, both fixed:
1. **`unload_image(const char* name)` unmapped the wrong copy.** The
   CreateProcess remap path called `loader.unload_image(name)` to discard the
   freshly-loaded high-address child copy — but parent and child copies share
   the same path, and the name-keyed lookup munmapped the FIRST match, i.e. the
   PARENT at 0x400000. That erased the parent from `modules_`, so the next child
   load saw 0x400000 free and mapped over it. Fix: `unload_image` now takes
   **uint64_t base** and munmaps exactly that module (loader.h:38,
   main.cpp:1084; call site process.cpp:110 passes the just-loaded `module->base`).
2. **`load_image(path, preferred_base)` silently fell back to a high address on
   conflict.** The relaunch loop tries `{0x20000000,0x30000000,0x40000000,
   0x50000000}`; when 0x20000000 was taken by a prior child, `map_sections`
   ignored the preferred base and fell through to low candidates/high — so
   children 2..4 landed at `0x7F2F...` instead of their relaunch bases. Fix:
   when `preferred_base != 0` and it conflicts with an existing module,
   `load_image` returns **nullptr** so the caller advances to the next relaunch
   base (main.cpp).
Result: parent stays at 0x400000; children chain-spawn at 0x20000000,
0x30000000, 0x40000000, 0x50000000 (pids 65536-65539, all `ppid=parent`), each
runs guest code from its own image, and ALL of parent + children exit code 0
(previously: host SIGSEGV / exit 139). Regression: cxx_eh (`CXX PASS
caught=0x403024 expected=0x403024 unwound=1`) and ctrl (`CTRL PASS`) still pass;
build 0 errors. NOTE the remaining lone guest fault `eip=0x500812af addr=0x14`
in child 65539 (x86_run siglongjmps to -1, app continues and exits 0) — it did
not block this run but is a new investigation item.

### FIXED — 89→125 processes: global thunk table exhausted (NULL IAT slot jump)
After the FLS per-process fix, the run reached 89 processes but a late child
faulted with `eip=0x00000001 addr=(nil)` — a **call through a NULL IAT slot**.
Root cause: `alloc_api_thunk` hands out slots from ONE global 1MB region
(`kThunkBase 0x76000000`, `kThunkStride 32`, **32768 slots**, never freed).
Every child process re-loads the SAME main image, and `process_imports` +
`process_delay_load_imports` allocate a fresh ~300 thunks per process. With 89
processes the table exhausted (`[Thetax86] thunk table exhausted`, `thunk alloc
failed: #116`), so `process_imports` wrote `thunk[i]=0` into late children's IAT
slots — the child's `__security_init_cookie`/init (OfficeSetup RVA 0xfb559,
which does `mov 0x873058,%edi; ... call *%edi` = KERNEL32 IAT slot index 14 =
**GetProcAddress**) jumped to NULL.
Fix (core/x86/api_dispatch.cpp `alloc_api_thunk`):
1. **Dedup identical thunks.** Thunks are immutable after creation and dispatch
   only on `(fn, argc, stdcall, known)`, so before allocating, scan for an
   in-use thunk matching all four and return its address. One slot is now shared
   by every process's identical import. (Known changes the dispatch path — the
   `!known` branch reads argc args with no wide handling and applies SEH resume;
   the `known` branch consults `sig->wide`. Keep it in the key.)
2. **Thunk-owned string copies.** `t.info.dll`/`t.info.name` previously pointed
   into the allocating process's image (unmapped when that process exits) or a
   caller's stack buffer (`ordinal_buf` in the loader). Dedup made those
   dangling pointers referenced again. `Thunk` now carries `char dll_buf[64]` /
   `name_buf[128]` and `info.dll/name` point into the owned buffers.
Result (api19): **125 processes registered, 124 `ExitProcess(0)`, ZERO thunk
failures, run exit 0** (was 89 processes / NULL-IAT fault). Only the
pre-existing tolerated lone fault remains (now at `eip=0x5f1812af addr=0x14`,
same RVA 0x812af NULL `[ebx+0x14]` walk in a late child). Regressions green:
cxx_eh (`CXX PASS ... unwound=1`), ctrl (`CTRL PASS`). The `!! unresolved`
count per run is dominated by the known delay-load set (wintrust/setupapi/
gdiplus/uxtheme/advapi32/comctl32), logged per process — not new.

### INVESTIGATED — Prefix-app smoke tests & WINWORD analysis
Ran the Click-to-Run Office apps in `/home/eronic/Prefixes/ThetaZ` (the `$WINEPREFIX`
the emulator boots with). Key findings:

- **Main Office apps (WINWORD/EXCEL/POWERPNT/OUTLOOK/MSACCESS/MSPUB/ONENOTE) = PE64**.
  Load via 64-bit direct-call path, reach `ExitProcess(1)` cleanly — but **1341+
  unresolved imports** (mostly C++ vcruntime/msvcp mangled exports + kernel32/user32/
  gdi32/advapi32/ole32 surface). The 64-bit path is bare beyond entry call.

- **32-bit Office pieces** (wordpad, MSOHTMED, ai.exe, notepad) run under interpreter
  but fault immediately at `eip=0x1` via unresolved IAT slots (40–188 missing each):
  - wordpad: 40 unresolved (gdi32 drawing, comdlg32, ole32)
  - MSOHTMED: 80 unresolved, fault at `0x438b70`
  - ai.exe: 188 unresolved (crypt32/bcrypt/wintrust code-signing), dies via `ExitProcess(0xC0000417)` watson
  - notepad.exe: 41 unresolved, fault at `eip=0x1`

- **WINWORD ExitProcess(1) root cause**: The 64-bit loader resolves imports by loading
  Wine's `kernelbase.dll` / `ntdll.dll` / `msvcrt.dll` from the prefix, then uses
  THEIR exports. Our API registry only covers `kernel32.dll` (346 functions).
  WINWORD imports from `kernelbase.dll` (not in registry), `ntdll.dll` (4 funcs),
  `user32.dll`/`gdi32.dll`/`advapi32.dll`/`ole32.dll` (minimal), `msvcrt.dll` (Wine
  DLL, 100s of CRT functions missing). The app runs its entry, hits an unresolved
  import or logic path that returns failure, and exits 1.

- **Prefix IS correctly referenced**: `$WINEPREFIX` = `/home/eronic/Prefixes/ThetaZ`
  (boot sets it, `wine_to_host` maps drives, Wine DLLs load from prefix's
  `windows/system32` and `syswow64`). The failure is import coverage, not paths.

### FIXED — Thunk table recursion & builtin-DLL import fix
The delay-load fallback was recursively reloading `ntdll.dll` (30+ times). Fixed:
1. `try_load_from_thetaz_prefix` now checks `find_module_by_dll_name` first to avoid
   reloading.
2. Added builtin-DLL blocklist (`kernel32.dll`, `kernelbase.dll`, `ntdll.dll`,
   `user32.dll`, `gdi32.dll`, `advapi32.dll`, `ole32.dll`, `oleaut32.dll`,
   `shell32.dll`, `ws2_32.dll`, `version.dll`, `shlwapi.dll`, `msvcrt.dll`,
   `ucrtbase.dll`, `vcruntime140.dll`) — these ONLY use the API registry.
3. Import resolution for builtin DLLs now ONLY queries the API registry — never
   loads Wine DLLs.

After fix: WINWORD unresolved dropped from 1341 → 14 (still missing: `RtlUnwindEx`,
`Beep`, `DuplicateHandle`, `FindFirstFileA`, `FindNextFileA`, `FreeEnvironmentStringsA`,
`GetEnvironmentStringsA`, `GetLocaleInfoA`, `GetNumberOfConsoleInputEvents`,
`GetStringTypeExA`, `HeapWalk`, `CreateCompatibleBitmap`, `CreateDIBSection`,
`CreateFontIndirectW`, `ExtTextOutW`, `LineTo`, `MoveToEx`, `Polygon`, `Rectangle`,
`StretchBlt`, `DelayLoadFailureHook`, `FindResourceA`, `GlobalAddAtomW`,
`GlobalDeleteAtom`, `IsBadCodePtr`, `IsBadStringPtrA`, `IsBadWritePtr`, `CloseHandle`,
`CompareStringA`, `CreateEventW`, `CreateThread`, `DeleteCriticalSection`,
`EnterCriticalSection`, `FileTimeToSystemTime`, `FindResourceW`, `FreeResource`,
`GetDateFormatW`, `GetLocalTime`, `GetLocaleInfoW`, `GetProcAddress`, `GetThreadLocale`,
`GetTickCount`, `GlobalAlloc`, `GlobalFree`, `HeapAlloc`, `HeapFree`, `HeapReAlloc`,
`InitializeCriticalSectionEx`, `LeaveCriticalSection`, `LoadLibraryW`, `LoadResource`,
`LocalAlloc`, `LocalFree`, `LocalReAlloc`, `LockResource`, `MulDiv`,
`MultiByteToWideChar`, `SetEvent`, `SizeofResource`, `StrCmpIW`, `StrCmpNIW`,
`SystemTimeToFileTime`, `WaitForSingleObject`, `WideCharToMultiByte`, `lstrcmpW`,
`lstrcmpiW`, `__acrt_iob_func`, `__stdio_common_vsprintf`, `_assert`, `_strdup`,
`_wcsicmp`, `_wcsnicmp`, `floor`, `free`, `fwrite`, `getenv`, `isprint`,
`iswalnum`, `memcmp`, `memcpy`, `memmove`, `memset`, `qsort`, `strcat`, `strchr`,
`strcmp`, `strcpy`, `strcspn`, `strlen`, `wcschr`, `wcsncmp`, `wcsstr`, `wcstol`,
`AdjustWindowRectEx`, `AppendMenuW`, `BeginDeferWindowPos`, `CallWindowProcA`,
`CallWindowProcW`, `CheckMenuItem`, `ClientToScreen`, `CreateDialogIndirectParamA`,
`CreateDialogIndirectParamW`, `CreateIconIndirect`, `CreatePopupMenu`,
`CreateWindowExW`, `DefWindowProcW`, `DeferWindowPos`, `DestroyCaret`,
`DestroyCursor`, `DestroyIcon`, `DialogBoxParamW`, `DispatchMessageW`,
`DrawEdge`, `DrawFocusRect`, `DrawFrameControl`, `DrawIcon`, `DrawIconEx`,
`DrawTextA`, `DrawTextW`, `EnableScrollBar`, `EndDeferWindowPos`, `FillRect`,
`FrameRect`, `GetActiveWindow`, `GetAsyncKeyState`, `GetCapture`, `GetClassLongPtrW`,
`GetClassNameW`, `GetCursorPos`, `GetDCEx`, `GetDoubleClickTime`, `GetDpiForWindow`,
`GetIconInfo`, `GetKeyNameTextW`, `GetKeyState`, `GetMenuState`, `GetMessagePos`,
`GetMessageTime`, `GetMessageW`, `GetMonitorInfoW`, `GetNextDlgTabItem`, `GetParent`,
`GetPropW`, `GetScrollInfo`, `GetScrollPos`, `GetScrollRange`, `GetSysColor`,
`GetSysColorBrush`, `GetTopWindow`, `GetWindow`, `GetWindowLongPtrW`,
`GetWindowLongW`, `GetWindowRect`, `GetWindowRgn`, `GetWindowTextLengthW`,
`GetWindowTextW`, `IntersectRect`, `InvalidateRgn`, `IsChild`, `IsDialogMessageW`,
`IsWindowUnicode`, `LoadBitmapW`, `LoadCursorW`, `LoadIconW`, `LoadImageA`,
`LoadImageW`, `LoadStringW`, `MapDialogRect`, `MapVirtualKeyW`, `MapWindowPoints`,
`MonitorFromRect`, `MoveWindow`, `NotifyWinEvent`, `PeekMessageW`, `PostMessageW`,
`RedrawWindow`, `RegisterClassW`, `RegisterWindowMessageW`, `ReleaseCapture`,
`RemovePropW`, `ScreenToClient`, `ScrollWindow`, `ScrollWindowEx`,
`SendDlgItemMessageW`, `SendMessageW`, `SetCapture`, `SetCaretPos`, `SetCursor`,
`SetParent`, `SetPropW`, `SetScrollInfo`, `SetScrollPos`, `SetScrollRange`,
`SetWindowLongPtrA`, `SetWindowLongPtrW`, `SetWindowLongW`, `SetWindowPos`,
`SetWindowRgn`, `SetWindowTextW`, `ShowCaret`, `ShowScrollBar`, `SubtractRect`,
`TrackMouseEvent`, `TrackPopupMenu`, `UnionRect`, `UnregisterClassW`,
`ValidateRect`, `WindowFromPoint`, `wsprintfW`, `RegOpenKeyA`, `CreateCompatibleBitmap`,
`CreateDCA`, `CreateDCW`, `CreateFontIndirectW`, `CreateICA`, `CreateICW`,
`CreateRectRgnIndirect`, `EnumFontFamiliesExW`, `ExtTextOutW`, `GdiConvertToDevmodeW`,
`GetNearestColor`, `GetObjectW`, `GetTextExtentPoint32W`, `GetTextMetricsW`,
`LineTo`, `MoveToEx`, `Polygon`, `Rectangle`, `SelectClipRgn`, `ActivateActCtx`,
`CreateActCtxW`, `DeactivateActCtx`, `DelayLoadFailureHook`, `EnumResourceNamesW`,
`FindResourceA`, `GetCurrentActCtx`, `GetDriveTypeA`, `GlobalFlags`, `GlobalLock`,
`GlobalUnlock`, `IsBadStringPtrA`, `ReleaseActCtx`, `lstrcmpA`,
`RtlCreateUnicodeStringFromAsciiz`, `RtlFreeUnicodeString`, `#17`, `#18`, `#21`,
`#25`, `#152`, `#153`, `#155`, `#162`, `SHCreateItemFromIDList`,
`SHCreateShellItemArray`, `SHCreateShellItemArrayFromDataObject`,
`SHGetDesktopFolder`, `SHGetFileInfoW`, `SHGetFolderPathW`, `SHGetIDListFromObject`,
`SHGetItemFromObject`, `SHGetSpecialFolderLocation`, `SHParseDisplayName`,
`PathAddBackslashW`, `PathCanonicalizeW`, `PathFileExistsW`, `PathFindExtensionA`,
`PathFindExtensionW`, `PathFindFileNameA`, `PathFindFileNameW`,
`PathFindNextComponentW`, `PathGetDriveNumberW`, `PathIsDirectoryW`,
`PathIsRelativeW`, `PathMatchSpecW`, `PathRemoveExtensionW`, `PathSkipRootW`,
`StrChrW`, `StrDupW`, `StrRetToBufW`, `__acrt_iob_func`, `__stdio_common_vsprintf`,
`__stdio_common_vswprintf`, `_strdup`, `_wcsdup`, `_wcsicmp`, `atoi`, `calloc`,
`free`, `fwrite`, `getenv`, `iswdigit`, `malloc`, `memcmp`, `memcpy`, `memmove`,
`memset`, `qsort`, `strcat`, `strchr`, `strcmp`, `strcpy`, `strcspn`, `strlen`,
`wcschr`, `wcsncmp`, `wcsstr`, `wcstol`, `AdjustWindowRectEx`, `AppendMenuW`,
`BeginDeferWindowPos`, `CallWindowProcA`, `CallWindowProcW`, `CharLowerW`,
`CheckRadioButton`, `ClientToScreen`, `CreateDialogIndirectParamA`,
`CreateDialogIndirectParamW`, `CreatePopupMenu`, `CreateWindowExW`,
`DefWindowProcW`, `DeferWindowPos`, `DestroyCaret`, `DestroyCursor`,
`DestroyIcon`, `DialogBoxIndirectParamA`, `DialogBoxIndirectParamW`,
`DialogBoxParamW`, `DlgDirListComboBoxW`, `DlgDirListW`, `DrawEdge`,
`DrawFocusRect`, `DrawFrameControl`, `DrawIcon`, `DrawIconEx`, `DrawTextW`,
`EnableMenuItem`, `EndDeferWindowPos`, `FillRect`, `GetClassInfoW`,
`GetClassLongPtrW`, `GetDialogBaseUnits`, `GetDlgItemInt`, `GetDlgItemTextW`,
`GetMenuItemInfoW`, `GetParent`, `GetPropW`, `GetScrollInfo`, `GetScrollPos`,
`GetScrollRange`, `GetSysColor`, `GetSysColorBrush`, `GetWindow`,
`GetWindowInfo`, `GetWindowLongPtrW`, `GetWindowLongW`, `GetWindowRect`,
`GetWindowRgn`, `GetWindowTextLengthW`, `GetWindowTextW`, `InsertMenuItemW`,
`IsChild`, `LoadCursorA`, `LoadCursorW`, `LoadIconA`, `LoadIconW`,
`LoadImageA`, `LoadImageW`, `LoadStringA`, `LoadStringW`, `MapDialogRect`,
`MapWindowPoints`, `MessageBoxW`, `MoveWindow`, `PeekMessageW`, `PostMessageW`,
`RedrawWindow`, `RegisterClassW`, `RegisterClipboardFormatA`,
`RegisterWindowMessageA`, `RegisterWindowMessageW`, `ReleaseCapture`,
`RemovePropA`, `RemovePropW`, `ScreenToClient`, `ScrollWindow`, `ScrollWindowEx`,
`SendDlgItemMessageW`, `SendMessageW`, `SetCapture`, `SetCaretPos`, `SetCursor`,
`SetParent`, `SetPropW`, `SetScrollInfo`, `SetScrollPos`, `SetScrollRange`,
`SetWindowLongPtrA`, `SetWindowLongPtrW`, `SetWindowLongW`, `SetWindowPos`,
`SetWindowRgn`, `SetWindowTextW`, `TrackPopupMenu`, `wsprintfW`,
`CreateFontW`, `CompareFileTime`, `DelayLoadFailureHook`, `EnumResourceNamesW`,
`CreateBindCtx`, `OleCreateDefaultHandler`, `CryptUIDlgSelectCertificateFromStore`,
`__stdio_common_vsprintf`, `__stdio_common_vswprintf`, `_strdup`, `_wcsdup`,
`_wcsicmp`, `calloc`, `free`, `fwrite`, `getenv`, `iswprint`, `iswspace`,
`malloc`, `memcmp`, `memcpy`, `memmove`, `memset`, `realloc`, `strchr`,
`strcmp`, `strcspn`, `strlen`, `wcslen`, `wcsrchr`, `DialogBoxParamW`,
`EnumChildWindows`, `GetClassLongW`, `GetParent`, `GetWindowLongPtrW`,
`GetWindowRect`, `LoadBitmapW`, `LoadImageW`, `LoadStringW`, `MapWindowPoints`,
`MessageBoxW`, `PostMessageW`, `SendMessageW`, `SetWindowLongPtrW`,
`SetWindowPos`, `ShowScrollBar`, `RegOpenKeyA`, `CompareStringOrdinal`,
`DelayLoadFailureHook`, `GetComputerNameExA`, `GetComputerNameExW`,
`IsBadStringPtrA`, `lstrcmpiA`, `LdrGetDllHandle`, `NtQueryVirtualMemory`,
`RtlFindExportedRoutineByName`, `RtlIpv4StringToAddressA`,
`RtlIpv6StringToAddressA`, `__acrt_iob_func`, `__stdio_common_vsprintf`,
`__stdio_common_vswprintf`, `_assert`, `_strdup`, `calloc`, `free`, `fwrite`,
`getenv`, `malloc`, `memcmp`, `memcpy`, `memmove`, `strchr`, `strcmp`,
`strcspn`, `strlen`, `wcschr`, `wcscmp`, `wcscpy`, `SystemFunction036`,
`GetComputerNameExA`, `IsBadStringPtrA`, `__acrt_iob_func`,
`__stdio_common_vsprintf`, `__stdio_common_vswprintf`, `_strdup`, `free`,
`fwrite`, `getenv`, `malloc`, `memcmp`, `memmove`, `strchr`, `strcmp`,
`strcspn`, `strlen`, `freeaddrinfo`, `getaddrinfo`, `DelayLoadFailureHook`,
`EnumResourceNamesW`, `FreeLibraryWhenCallbackReturns`, `GetComputerNameExW`,
`IsBadStringPtrA`, `WinHttpReceiveResponse`, `InternetGetCookieExW`,
`InetNtopW`, `SetFileCompletionNotificationModes`, `TrySubmitThreadpoolCallback`,
`__acrt_iob_func`, `__stdio_common_vsprintf`, `__stdio_common_vswprintf`,
`_assert`, `_strdup`, `_strnicmp`, `_wcsdup`, `_wcsicmp`, `_wcslwr`,
`_wcsnicmp`, `_wgetenv`, `calloc`, `free`, `fwrite`, `getenv`, `iswalpha`,
`malloc`, `memcmp`, `memcpy`, `memmove`, `memset`, `realloc`, `strchr`,
`strcmp`, `strcspn`, `strlen`, `towlower`, `wcscat`, `wcschr`, `wcscmp`,
`wcscpy`, `wcslen`, `wcsncmp`, `wcsrchr`, `wcsstr`, `wcstol`, `wcstoul`,
`wcstoull`, `DispatchMessageW`, `MsgWaitForMultipleObjects`, `PeekMessageW`,
`__WSAFDIsSet`, `FreeAddrInfoW`, `GetAddrInfoW`, `GetNameInfoW`, `WSASend`,
`WSASocketW`, `inet_ntop`, `__acrt_iob_func`, `__stdio_common_vsprintf`,
`CreateXmlReaderInputWithEncodingName`, `WerReportCloseHandle`,
`#0`, `@`, `#0`, `_assert`, `_strdup`, `_wcsicmp`, `_wcsnicmp`, `calloc`,
`free`, `fwrite`, `getenv`, `malloc`, `memcmp`, `memcpy`, `memmove`, `memset`,
`realloc`, `strchr`, `strcmp`, `strcspn`, `strlen`, `wcscmp`, `wcsstr`,
`__acrt_iob_func`, `__stdio_common_vsprintf`, `_strdup`, `calloc`, `free`,
`fwrite`, `getenv`, `memcmp`, `memcpy`, `memmove`, `strchr`, `strcmp`,
`strcspn`, `strlen`, `wcsrchr`).

Result: WINWORD now loads using ONLY the API registry (no Wine DLLs for builtin
DLLs), unresolved count ~14 for core kernel32, but still missing the massive
surface for `kernelbase.dll`/`ntdll.dll`/`user32.dll`/`gdi32.dll`/`advapi32.dll`/
`ole32.dll`/`shell32.dll`/`msvcrt.dll`/`ucrtbase.dll`/`vcruntime140.dll` — a
separate 64-bit API coverage workstream.

---

### DONE — OfficeSetup API-coverage push (Tiers 1-6) + 16-call death-point BREAKTHROUGH
Sessions focused on shrinking OfficeSetup.exe's unresolved surface AND moving its
16-call CRT death point. (OfficeSetup = PE32/32-bit bootstrap installer at 0x400000,
target app per user directive.)

**Tier 1 (thread/env/str)** — DisableThreadLibraryCalls (module flag), IsBadStringPtrW
(/proc/self/maps), GetEnvironmentVariableW (host getenv + UTF-16), ResolveDelayLoadedAPI
(descriptor parse + load_guest_module + thunk patch), RtlUnwind (fs:0 walk).

**Tier 2 (registry)** — in-memory hive (g_reg_handles, RegKey tree, predefined HKEYs
0x80000000-5). RegOpen/Create/Close/Query/Set/Delete + A-variants + RegGetValueW.
Multiplication of "!!! unresolved" dropped by ~25.

**Tier 3 (GDI DC)** — GetDC/ReleaseDC/GetDCEx (per-window DC via g_dc_hwnd),
GetClientRect, SelectObject (pen/brush/font), GetDeviceCaps, SetTextColor/BkColor/BkMode,
TextOutW/ExtTextOutW (UTF-16→UTF-8 → theta_gdi_text), GetStockObject, CreateSolidBrush.

**Tier 4 (file)** — FindFirstFileW/Next/Close (opendir/readdir + */? match, WIN32_FIND_DATAW),
CopyFileW (byte-identical). Fixed malformed WIN32_FIND_DATAW struct (FILETIME merged).

**Tier 5 (strings)** — lstrcmpW/lstrcmpiW/CompareStringW (1/2/3 semantics + NORM_IGNORECASE),
GetLocaleInfoW/GetDateFormatW/GetTimeFormatW.

**Tier 6 (crypto, real OpenSSL-backed)** — BCrypt (Open/GetProperty/CreateHash/HashData/
FinishHash/DestroyHash/DestroyKey/ImportKeyPair ECC-P256+RSA/VerifySignature), full X509 cert
store/context/chain (CertOpenStore/AddEncoded/find/getproperty/CertGetCertificateChain via
X509_verify_cert/CertVerifyCertificateChainPolicy), CryptMsg PKCS#7, CryptDecodeObject,
CryptMemAlloc/Free, CryptSIP, WinVerifyTrustEx (REAL PE Authenticode: security-dir → PKCS7 →
signer → chain; verified against Microsoft's OWN OfficeSetup.exe signature + injected
self-signed → trusted). SHA1 fingerprint matches `openssl x509` exactly. CRITICAL: fixed
CERT_FIND_* shift encoding (SUBJECT_STR_A=0x70007 not 0x80007). LIMITATION: does not
recompute SpcIndirectDataContent image digest.

**BREAKTHROUGH (the 16-call death point)**: the fault was `call *%esi` with esi =
decode(0) = ror(__security_cookie@0xa6e780, cookie&0x1f) — a ZERO encoded function-pointer
slot, caused by **LoadLibraryExW("api-ms-win-core-synch-l1-2-0", 0, 0x800) returning null**
(api-set names have no backing .dll file; load_guest_module couldn't map them). FIX:
`get_builtin_module_handle()` (api.cpp/api.h) synthesizes `0x78000000+i*0x1000` handles;
`ApiRegistry::has_dll()`; LoadLibraryW/A/ExW route api-set/builtin names to the synthetic
handle. Also implemented+registered (were generic_api_stub `s`): FlsAlloc/Free/Get/SetValue,
InitOnceExecuteOnce, QueryPerformanceCounter/Frequency, GetProcessHeap, GetSystemTime+
GetLocalTime+GetSystemTimeAsFileTime+FileTimeToSystemTime+SystemTimeToFileTime,
OutputDebugStringA/W, GetCPInfo/GetCPInfoExW/IsValidCodePage, GetUserDefaultLocaleName,
AllocConsole, LocalFree, K32GetProcessMemoryInfo, OpenProcess.

**RESULT: OfficeSetup death point moved 16 → 39,357 API calls.** The app now runs its
entire CRT static-init + DLL load phase and reaches its real installer body. Unresolved
imports 931 → 603. Regression 76/76 green throughout.

**NEW BLOCKER (open)**: after ~39k calls the app stalls in an allocation loop at
RVA 0x825943 `call *[0x873238]` = **HeapAlloc** (confirmed via IAT slot resolution, NOT the
misleading thunk-dedup trace name). HeapAlloc returns NULL → `test eax; je 0x825926` retry
loop → infinite recursion → stack overflow at 0x823fdd (esp=0xdffffc). Hypothesis: guest-heap
exhaustion/fragmentation after 39k allocations (352MB heap 0x60000000-0x76000000). Next:
instrument alloc_guest_heap failure, check heap exhaustion vs allocator bug.

---

### Todo workstreams
- SEH — done for MSVC `__C_specific_handler`, classic `__except_handler3`/
  `__except_handler4_common` (frame layout {prev@0, handler@4, scopetable@8,
  trylevel@0xC, _ebp@0x10, xpointers@0x14}, 12-byte scope entries
  {prev, filter, handler}, TRYLEVEL_END -1/-2, /GS cookie-XOR for handler4,
  `_local_unwind2/4` for longjmp, `x86_call_guest_args_ebp` EBP override for
  filters/finally; `except3_test.c` two-frame decline/accept/finally test
  passes), AND `__CxxFrameHandler` V2/V3/V4 (catch + pass-2 unwind,
  `tests/cxx_eh_test.c`). Remaining: `__CxxFrameHandler4` (x64-style
  fully-digested tables) + deeper unwind-scope coverage.
- GUI — X11 backend + message loop + GDI text + timers + controls (BUTTON/
  STATIC, WM_COMMAND, checkboxes) done. Remaining: EDIT/LISTBOX/COMBOBOX
  behavior, dialogs, fuller gdi32 object model, ole32/oleaut32/comdlg32/shell32.
- INTERP — opcode audit done (gaps documented in docs/AGENTS.md §2), SSE2 +
  common SSE/SSSE3 + x87 completeness done, per-opcode test harness started
  (newops + newops2 + interp_ops3 — cmpxchg8b/x87/rep/string/cmov/setcc/jcc/loop/
  pushad/pushfd — all pass). Remaining: broaden harness (fuzz, 16-bit, MMX),

---

## 8. DEBUGGING RECIPES

- Unresolved import: look for `!! unresolved: NAME` in the loader output, then add
  the function to `kernel32_runtime.cpp` (or ucrt.cpp) + register + add a
  signature-table entry (arg count) in `api_dispatch.cpp`.
- Guest SIGSEGV: run with `THETAZ_TRACE_INSNS=1`; the fault dump includes the
  last 128 EIPs. The fault EIP being a rotation of the __security_cookie means a
  zero entry was decoded (EncodePointer/teardown path).
- Stack corruption after an API call: check the signature-table argc for that
  function (wrong argc ⇒ stdcall pops wrong bytes ⇒ [esp] garbage). Also check
  the guest struct layout (32-bit vs 64-bit).
- A hang in the GUI app: likely a lock issue (g_window_mutex/g_x11_mutex order)
  or XNextEvent blocking (must poll XPending).
- "Failed to initialize IPC server": stale process holds port 12345 (see §2).
- `std::terminate`/Aborted on exit: a joinable `std::thread` (e.g. g_x11_thread)
  destroyed while joinable — detach it.
- Always `cmake --build build` in Debug and check for 0 errors after changes.

---

## 9. FILE MAP (quick index)

| Path | Role |
|------|------|
| `main_entry.cpp` | host main / boot order / entry dispatch |
| `core/loader/main.cpp` | PELoader (map/reloc/imports/exports) |
| `core/x86/interp.cpp` | x86 interpreter + fault handler |
| `core/x86/api_dispatch.cpp` | thunks, signature table, dispatch, SEH resume |
| `core/x86/guest_env.cpp` | 32-bit stack/TEB/PEB/TLS setup |
| `core/api/api.cpp` | ApiRegistry + guest heap + shared alloc |
| `core/api/kernel32.cpp` | base kernel32 (30+) |
| `core/api/kernel32_runtime.cpp` | ~200 APIs, user32/gdi32, RaiseException SEH, GetProcAddress, EncodePointer |
| `core/api/kernel32.h` | extended Windows types + WINAPI_* declarations |
| `core/api/ucrt.cpp` | CRT/MSVCRT layer |
| `core/api/process.cpp` | CreateProcess/ExitProcess |
| `core/api/ws2_32.cpp` | Winsock2 |
| `core/api/oleaut32.cpp` | BSTR/VARIANT |
| `core/api/window.cpp` | X11 GUI + user32 window/message layer |
| `core/ntdll/thread.cpp` | ThreadManager |
| `core/ntdll/virtual.cpp` | WindowsAddressSpace |
| `core/ntdll/sync.cpp` | sync primitives |
| `core/ntdll/teb.cpp` | 64-bit TEB/PEB via GS |
| `core/exception/exception.cpp` | ExceptionHandler + vectored handlers |
| `core/exception/signal_x86_64.cpp` | SIGSEGV→guest dispatch (64-bit host) |
| `core/ipc/server.cpp` / `thetaserver.cpp` | IPC + object manager |
| `tests/`, `build.sh` | test fixtures |

Full exact constants/offsets/structs: see **`docs/AGENTS.md`**.
DO NOT ASSUME THE LICENSE IS MIT AS THIS CAUSES DISCREPANCIES IN TIV COMMITS
