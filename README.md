# ThetaZ — Windows Compatibility Layer

A **from-scratch, original C++ implementation** of a Windows compatibility layer
for Linux x86-64. Licensed under GPL-2.0 — this is not derived from Wine, Proton,
or any other existing compatibility project.

The entire codebase is original C++17. The `Wine/` and `ProtonGE/` directories
in this tree are **study references** — they are not compiled, linked, or
included in ThetaZ in any way.

## Why a new implementation

Existing solutions (Wine, Proton) are 30+ year C codebases with architectural
constraints from dual PE/ELF builds, autoconf, and retrofitted features. ThetaZ
approaches the problem with modern C++: RAII, STL, type-safe namespaces, and a
single build system (CMake).

Key advantages:
- **No PE/ELF split** — pure C++ with one compiler, one build system.
- **`ms_abi` functions** — the Win32 layer uses GCC's `__attribute__((ms_abi))`
  so PE code calls API functions with the correct Microsoft x64 convention
  (RCX/RDX/R8/R9) instead of SysV (RDI/RSI/RDX/RCX).
- **Manual MS-x64 vararg extraction** — avoids the `va_list` ABI mismatch
  between SysV and Microsoft conventions that trips up cross-ABI `printf`.
- **GS-segment TEB** — the Windows TEB/PEB lives at `GS` on x86-64 Linux,
  which is free (glibc uses `FS` for its thread-local storage).
- **RAII memory management** — `std::unique_ptr`, containers, and mutexes
  replace raw pointer tracking.

## Requirements

- **C++17** compiler (GCC 14+, Clang 17+)
- **CMake** 3.10+
- **POSIX/Linux**: pthread, mmap, socketpair, epoll, ucontext, signal
- **MinGW-w64** (optional): for building test `.exe` fixtures
- **No external libraries** — everything else is self-contained

## Building

```bash
./build.sh            # build + run 9 automated tests
# or manually:
cmake -B build && cmake --build build
./build/ThetaZ <windows.exe>
```

## What works (tested)

The full pipeline is end-to-end functional:

| Layer | File | Description |
|-------|------|-------------|
| PE32+ loader | `core/loader/main.cpp` | Map, relocate, resolve imports (case-insensitive + api-set fallback), apply section protections, call entry point |
| TEB/PEB | `core/ntdll/teb.cpp` | GS→TEB, PEB→ImageBaseAddress, stack limits — safe (Linux FS-based TLS untouched) |
| Win32 API | `core/api/kernel32.cpp` | 30+ kernel32 functions: ExitProcess, VirtualAlloc/Free, CreateFileA/WriteFile, critical sections, TLS, env, etc. |
| UCRT/msvcrt | `core/api/ucrt.cpp` | `api-ms-win-crt-*` api-set surface: printf (full % formatting), malloc/free, str*, fopen/fprintf/fclose, CRT startup chain |
| Exceptions | `core/exception/` | SIGSEGV/SIGFPE/SIGILL → Windows exception codes → handler dispatch |
| Sync | `core/ntdll/sync.cpp` | Semaphore, Event, CriticalSection, Mutex, SpinLock, ReaderWriterLock |
| Threads | `core/ntdll/thread.cpp` | TEB/register context model |
| Memory | `core/ntdll/virtual.cpp` | 8GB low + 8GB high reservations, mmap-based |
| IPC foundation | `core/ipc/` | Socket server + message protocol for wineserver role |

### Test suite (9 assertions)

```bash
$ ./build.sh
PASS: return: ExitProcess(42) called          # raw PE, 1 import, API call
PASS: ticks: ExitProcess(5) called             # GetTickCount + ExitProcess
PASS: ticks: GetTickCount import resolved      # import resolution
PASS: memory: VirtualAlloc+Free                # memory API
PASS: file: CreateFileA/WriteFile              # file I/O
PASS: file: wrote thetaz_test_out.txt          # file contents verified
PASS: hello_main: Hello, World!                # full CRT main() program
PASS: hello_main: printf %d works              # integer formatting
PASS: hello_main: printf %s/%x works           # string + hex formatting
=== 9 passed, 0 failed ===
```

## Running a real Windows program

```bash
$ ./build/ThetaZ hello.exe
...
Calling entry point...
Hello, World!
6 * 7 = 42
name: thetaz, hex: abcdef
```

Real-world demo (malloc, fopen, fprintf, fclose, proper exit code):

```bash
$ ./build/ThetaZ demo.exe
ThetaZ heap+string works
wrote 24 bytes to file
$ echo $?
3
$ cat thetaz_demo.txt
ThetaZ heap+string works
```

## Architecture

```
main_entry.cpp          # Entry point, wires subsystems together
core/
  loader/               # PE32+ image loader (map, relocate, imports, exec)
  ntdll/                # Virtual memory, threads, sync, TEB/PEB
  api/                  # Win32 + UCRT function registry and implementations
  exception/            # Signal → Windows exception translation
  ipc/                  # Socket-based wineserver foundation
tests/                  # MinGW test .exe fixtures + harness
Wine/                   # Official Wine source (reference, NOT compiled)
ProtonGE/               # GE-Proton source (reference, NOT compiled)
docs/                   # Architecture study notes
```

## License

MIT — see [LICENSE](LICENSE). This is an original implementation not derived
from Wine, Proton, or any other compatibility-layer codebase.
