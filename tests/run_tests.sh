#!/bin/bash
# ThetaZ PE test harness.
#
# Compiles tiny test .exe files with MinGW (no CRT, raw entry), runs them
# through the ThetaZ loader, and asserts on the observable behavior:
#   - the process exits via our ExitProcess stub with the expected code, or
#   - the entry point returns the expected value (no imports).
#
# Usage: tests/run_tests.sh [path-to-ThetaZ-binary]

set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${1:-$ROOT/build/ThetaZ}"
# Resolve to absolute path so (cd $WORK && $BIN ...) works
case "$BIN" in
  /*) ;;
  *) BIN="$PWD/$BIN" ;;
esac
CC="x86_64-w64-mingw32-gcc"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

PASS=0
FAIL=0

check() {
    local name="$1"
    if eval "$2"; then
        PASS=$((PASS + 1))
        echo "PASS: $name"
    else
        FAIL=$((FAIL + 1))
        echo "FAIL: $name"
    fi
}

build_pe() { # name testdefine -> writes $WORK/$name.exe
    local name="$1" def="$2"
    "$CC" -c -D_WIN32_WINNT=0x0600 -D"$def" "$ROOT/tests/test_pe.c" -o "$WORK/$name.o" || return 1
    "$CC" -nostdlib -nostartfiles -Wl,-e,entry -Wl,--subsystem,console \
        "$WORK/$name.o" -o "$WORK/$name.exe" -lkernel32 || return 1
}

run_pe() { # exe [child_exe] -> OUT contains captured stdout
    if [ $# -ge 2 ]; then
        OUT="$(timeout -k 1 5 stdbuf -oL "$BIN" "$1" "$2" 2>&1)"
    else
        OUT="$(timeout -k 1 5 stdbuf -oL "$BIN" "$1" 2>&1)"
    fi
}

# --- Test 1: entry point return value (no imports) -------------------------
build_pe "return" "TEST_RETURN"
run_pe "$WORK/return.exe"
check "return: ExitProcess(42) called" 'echo "$OUT" | grep -q "ExitProcess(42) called"'

# --- Test 2: GetTickCount + ExitProcess -------------------------------------
build_pe "ticks" "TEST_TICKS"
run_pe "$WORK/ticks.exe"
check "ticks: ExitProcess(5) called" 'echo "$OUT" | grep -q "ExitProcess(5) called"'

# --- Test 3: import resolution recorded --------------------------------------
check "ticks: GetTickCount import resolved" 'echo "$OUT" | grep -q "GetTickCount -> "'

# --- Test 4: VirtualAlloc / VirtualFree --------------------------------------
build_pe "memory" "TEST_MEMORY"
run_pe "$WORK/memory.exe"
check "memory: VirtualAlloc+Free -> ExitProcess(17)" 'echo "$OUT" | grep -q "ExitProcess(17) called"'

# --- Test 5: CreateFileA / WriteFile / CloseHandle ----------------------------
build_pe "file" "TEST_FILE"
(cd "$WORK" && timeout -k 1 5 stdbuf -oL "$BIN" "file.exe" > file.out 2>&1)
check "file: ExitProcess(0x22) called" 'grep -q "ExitProcess(34) called" "$WORK/file.out"'
check "file: wrote thetaz_test_out.txt" 'grep -q "Hello from ThetaZ" "$WORK/thetaz_test_out.txt"'

# --- Test 6: full CRT main()-based program -------------------------------------
"$CC" "$ROOT/tests/hello_main.c" -o "$WORK/hello_main.exe"
(cd "$WORK" && timeout -k 1 8 stdbuf -oL "$BIN" "hello_main.exe" > hello_main.out 2>&1)
check "hello_main: Hello, World!" 'grep -q "Hello, World!" "$WORK/hello_main.out"'
check "hello_main: printf %d works" 'grep -q "6 \* 7 = 42" "$WORK/hello_main.out"'
check "hello_main: printf %s/%x works" 'grep -q "name: thetaz, hex: abcdef" "$WORK/hello_main.out"'

# --- Test 7: SEH dispatch (guest handler fixes up RIP) --------------------------
build_pe "seh" "TEST_SEH"
run_pe "$WORK/seh.exe"
check "seh: handler resumed after fault" 'echo "$OUT" | grep -q "ExitProcess(81) called"'

# --- Test 8: CRT startup (argc/argv, atexit LIFO, math, locale) ----------------
"$CC" "$ROOT/tests/hello_crt.c" -o "$WORK/hello_crt.exe"
(cd "$WORK" && timeout -k 1 8 stdbuf -oL "$BIN" "hello_crt.exe" first_arg > hello_crt.out 2>&1)
check "crt: argc from real command line" 'grep -q "argc=2" "$WORK/hello_crt.out"'
check "crt: argv[1] passed through" 'grep -q "arg1=first_arg" "$WORK/hello_crt.out"'
check "crt: sqrt(16)=4" 'grep -q "sqrt(16)=4" "$WORK/hello_crt.out"'
check "crt: sqrt(2) real value" 'grep -q "sqrt2=1414" "$WORK/hello_crt.out"'
check "crt: locale is C" 'grep -q "locale=C" "$WORK/hello_crt.out"'
check "crt: atexit ran on ExitProcess" 'grep -q "atexit-b" "$WORK/hello_crt.out" && grep -q "atexit-a" "$WORK/hello_crt.out"'
check "crt: atexit LIFO order" '[ "$(grep -n "atexit-a" "$WORK/hello_crt.out" | cut -d: -f1)" -gt "$(grep -n "atexit-b" "$WORK/hello_crt.out" | cut -d: -f1)" ]'

# --- Test 9: real threads (CreateThread, join, events, semaphore, mutex) --------
"$CC" "$ROOT/tests/hello_threads.c" -o "$WORK/hello_threads.exe"
(cd "$WORK" && timeout -k 1 12 stdbuf -oL "$BIN" "hello_threads.exe" > hello_threads.out 2>&1)
check "threads: created worker ran and joined" 'grep -q "join_wait=0" "$WORK/hello_threads.out"'
check "threads: exit code returned from proc" 'grep -q "exit_code=7" "$WORK/hello_threads.out"'
check "threads: interlocked counter incremented" 'grep -q "counter=100" "$WORK/hello_threads.out"'
check "threads: worker has its own thread id" 'grep -q "worker_tid=2" "$WORK/hello_threads.out"'
check "threads: main thread id is 1" 'grep -q "main_tid=1" "$WORK/hello_threads.out"'
check "threads: event set from another thread" 'grep -q "event_wait=0" "$WORK/hello_threads.out"'
check "threads: semaphore wait timed out" 'grep -q "sem_timeout=258" "$WORK/hello_threads.out"'
check "threads: semaphore acquired after release" 'grep -q "sem_acquire=0 prev=0" "$WORK/hello_threads.out"'
check "threads: mutex wait blocked then timed out" 'grep -q "mutex_timeout=258" "$WORK/hello_threads.out"'
check "threads: mutex acquired after release" 'grep -q "mutex_acquire=0" "$WORK/hello_threads.out"'
check "threads: completed cleanly" 'grep -q "threads_done" "$WORK/hello_threads.out"'

# --- Test 10: LoadLibraryA / GetProcAddress of a guest DLL --------------------
"$CC" -shared "$ROOT/tests/mylib.c" -o "$WORK/mylib.dll"
"$CC" "$ROOT/tests/hello_dll.c" -o "$WORK/hello_dll.exe"
(cd "$WORK" && timeout -k 1 8 stdbuf -oL "$BIN" "hello_dll.exe" > hello_dll.out 2>&1)
check "dll: loaded guest DLL" 'grep -q "PELoader: loading mylib.dll" "$WORK/hello_dll.out"'
check "dll: resolved and called export add" 'grep -q "dll_add=5" "$WORK/hello_dll.out"'
check "dll: resolved and called export sub" 'grep -q "dll_sub=6" "$WORK/hello_dll.out"'
check "dll: GetModuleHandleA finds loaded DLL" 'grep -q "dll_handle_match=1" "$WORK/hello_dll.out"'

# --- Test 11: per-thread TLS (TlsAlloc/Set/Get across threads) -----------------
"$CC" "$ROOT/tests/hello_tls.c" -o "$WORK/hello_tls.exe"
(cd "$WORK" && timeout -k 1 8 stdbuf -oL "$BIN" "hello_tls.exe" > hello_tls.out 2>&1)
check "tls: slot allocated" 'grep -q "slot=0" "$WORK/hello_tls.out"'
check "tls: main thread value" 'grep -q "main_tls=abcd" "$WORK/hello_tls.out"'
check "tls: worker thread value is its own" 'grep -q "worker_tls=1234" "$WORK/hello_tls.out"'
check "tls: main value not clobbered by worker" 'grep -q "main_tls_after=abcd" "$WORK/hello_tls.out"'

# --- Test 12: IPC socket round-trip (robust send_data/receive_data) -------------
"$CC" "$ROOT/tests/hello_sleep.c" -o "$WORK/hello_sleep.exe"
(cd "$WORK" && timeout -k 1 20 "$BIN" "hello_sleep.exe" > hello_sleep.log 2>&1) &
THETAZ_PID=$!
sleep 1.5
check "ipc: GetTickCount round-trip over socket" 'python3 "$ROOT/tests/ipc_client.py" 5'
wait "$THETAZ_PID"

# --- Test 13: SRW locks (shared/exclusive semantics) ---------------------------
"$CC" "$ROOT/tests/hello_srw.c" -o "$WORK/hello_srw.exe"
(cd "$WORK" && timeout -k 1 8 stdbuf -oL "$BIN" "hello_srw.exe" > hello_srw.out 2>&1)
check "srw: two shared holders coexist" 'grep -q "two shared holders" "$WORK/hello_srw.out"'
check "srw: exclusive blocked while shared held" 'grep -q "srw_try_excl_blocked=0" "$WORK/hello_srw.out"'
check "srw: shared blocked while exclusive held" 'grep -q "srw_try_shared_blocked=0" "$WORK/hello_srw.out"'
check "srw: writer thread got exclusive" 'grep -q "srw_writer_ok=1" "$WORK/hello_srw.out"'
check "srw: completed cleanly" 'grep -q "srw_done" "$WORK/hello_srw.out"'

# --- Test 14: unhandled-exception top-level filter (CONTINUE_SEARCH) -----------
"$CC" "$ROOT/tests/hello_fault.c" -o "$WORK/hello_fault.exe"
run_pe "$WORK/hello_fault.exe"
check "fault: top-level filter called with exception code" 'echo "$OUT" | grep -q "uef_code=c0000094"'
check "fault: filter CONTINUE_SEARCH reported" 'echo "$OUT" | grep -q "Top-level filter returned 0x0"'
check "fault: unhandled exception reported before exit" 'echo "$OUT" | grep -q "Unhandled exception: 0xC0000094"'

# --- Test 15: top-level filter fixes up context and resumes --------------------
"$CC" "$ROOT/tests/hello_fault_resume.c" -o "$WORK/hello_fault_resume.exe"
run_pe "$WORK/hello_fault_resume.exe"
check "fault: filter fixed up RIP (CONTINUE_EXECUTION)" 'echo "$OUT" | grep -q "Top-level filter returned 0xFFFFFFFF"'
check "fault: execution resumed past fault" 'echo "$OUT" | grep -q "fault_resumed"'

# --- Test 16: window/class registry + message pump ----------------------------
"$CC" "$ROOT/tests/hello_win.c" -o "$WORK/hello_win.exe"
run_pe "$WORK/hello_win.exe"
check "win: real HWND allocated" 'echo "$OUT" | grep -q "hwnd=0x1001"'
check "win: WM_CREATE dispatched to guest wndproc" 'echo "$OUT" | grep -q "create_count=1"'
check "win: SendMessage reached wndproc" 'echo "$OUT" | grep -q "custom_msg=42"'
check "win: BeginPaint returned a DC" 'echo "$OUT" | grep -q "paint_hdc=0x1"'
check "win: posted message dispatched" 'echo "$OUT" | grep -q "posted_msg=77"'
check "win: WM_DESTROY -> PostQuitMessage" 'echo "$OUT" | grep -q "destroyed=1"'
check "win: message loop ended on WM_QUIT" 'echo "$OUT" | grep -q "loop_rounds=2"'
check "win: completed cleanly" 'echo "$OUT" | grep -q "win_done"'

# --- Test 17: CreateProcessA spawns a child process ---------------------------
"$CC" "$ROOT/tests/hello_child.c" -o "$WORK/hello_child.exe"
"$CC" "$ROOT/tests/hello_process.c" -o "$WORK/hello_process.exe"
run_pe "$WORK/hello_process.exe" "$WORK/hello_child.exe"
check "process: CreateProcessA succeeded" 'echo "$OUT" | grep -q "create_ok=1"'
check "process: child pid allocated" 'echo "$OUT" | grep -q "child_pid=65536"'
check "process: child tid allocated" 'echo "$OUT" | grep -q "child_tid=2"'
check "process: hProcess = child pid" 'echo "$OUT" | grep -q "hProcess=0x10000"'
check "process: child process started" 'echo "$OUT" | grep -q "child_running"'
check "process: child_pid=65536" 'echo "$OUT" | grep -q "child_pid=65536"'
check "process: child_exit=7" 'echo "$OUT" | grep -q "child_exit=7"'
check "process: WaitForSingleObject waited (object signaled)" 'echo "$OUT" | grep -q "wait_result=0"'
check "process: GetExitCodeProcess succeeded" 'echo "$OUT" | grep -q "got_exit_code=1"'
check "process: child exit code is 7" 'echo "$OUT" | grep -q "exit_code=7"'
check "process: parent done cleanly" 'echo "$OUT" | grep -q "parent_done"'

# --- Test 18: GDI object model + DC (minimal) --------------------------------
"$CC" -D_WIN32_WINNT=0x0600 -DTEST_GDI "$ROOT/tests/test_pe.c" -o "$WORK/hello_gdi.exe" -lkernel32 -lgdi32
run_pe "$WORK/hello_gdi.exe"
check "gdi: GetStockObject returns stock handles" 'echo "$OUT" | grep -q "gdi: black_brush="'
check "gdi: SelectObject returns previous object" 'echo "$OUT" | grep -q "gdi: prev_brush="'
check "gdi: CreateSolidBrush returns custom handle" 'echo "$OUT" | grep -q "gdi: custom_brush="'
check "gdi: DeleteObject on stock returns FALSE" 'echo "$OUT" | grep -q "gdi: stock_delete=0"'
check "gdi: DeleteObject on custom returns TRUE" 'echo "$OUT" | grep -q "custom_delete=1"'
check "gdi: TextOutA prints text" 'echo "$OUT" | grep -q "Hello from GDI"'
check "gdi: TextOutA prints ThetaZ" 'echo "$OUT" | grep -q "ThetaZ"'
check "gdi: completed" 'echo "$OUT" | grep -q "gdi_done"'

# --- Test 19: predefined control classes (BUTTON/STATIC, BM_*, WM_COMMAND) ----
"$CC" "$ROOT/tests/ctrl_test.c" -o "$WORK/ctrl_test.exe" -lkernel32 -luser32 -lgdi32
run_pe "$WORK/ctrl_test.exe"
check "ctrl: BM_GETCHECK/SETCHECK roundtrip" 'echo "$OUT" | grep -q "CTRL PASS"'

# --- Test 20: EDIT/LISTBOX/COMBOBOX + dialog accessors ------------------------
"$CC" "$ROOT/tests/ctrl2_test.c" -o "$WORK/ctrl2_test.exe" -lkernel32 -luser32 -lgdi32
run_pe "$WORK/ctrl2_test.exe"
check "ctrl2: EDIT/LISTBOX/COMBOBOX messages + dialog accessors" 'echo "$OUT" | grep -q "CTRL2 PASS"'

echo
echo "=== $PASS passed, $FAIL failed ==="
[ "$FAIL" -eq 0 ]
