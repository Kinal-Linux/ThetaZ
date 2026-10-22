# ThetaZ / "C~" — Build system & how Wine is compiled

## Tree layout (official Wine HEAD)

```
aclocal.m4, configure.ac, configure   # autoconf (generates configure)
Makefile.in                           # top-level; per-dir Makefile.in
dlls/<name>/                          # one dir per DLL
  Makefile.in, <name>.spec, *.c, include/
programs/<name>/                      # executables (cmd, explorer, winecfg...)
server/                               # wineserver daemon (plain unix C)
loader/                               # wine, wine-preloader, winevdm
include/                              # public headers: windows.h, winternl.h, wine/*.h
  wine/  # unixlib.h, server_protocol.h, asm.h, exception.h, debug.h, ...
libs/                                 # helper libs (portability): libport, libwine
tools/                                # build tools: winebuild, makedep, wrc, widl, wmc,
                                      # make_requests, make_unicode, winegcc, winedump...
nls/, fonts/, documentation/, po/
```

## Makefile.in semantics (per DLL)

Example `dlls/ntdll/Makefile.in`:
```
MODULE    = ntdll.dll        # PE module name
UNIXLIB   = ntdll.so         # companion unix .so (built from `#pragma makedep unix` files)
IMPORTLIB = ntdll            # import library name (libntdll.a / ntdll.lib)
IMPORTS   = ...              # PE import libs (in order; resolved at link)
UNIX_CFLAGS / UNIX_LIBS      # flags/libs for the unix .so
EXTRADLLFLAGS = -nodefaultlibs
x86_64_EXTRADLLFLAGS = -Wl,--image-base,0x170000000   # per-arch load base
SOURCES   = ...              # PE-side C files
```

Directives (`#pragma makedep` in source files):
- `#pragma makedep unix` → compile this file into the unix `.so` (e.g. ntdll/unix/*.c).
- `#pragma makedep import` / `install` → install rules.
- `.spec` file: declares exports: `@ stdcall NtCreateEvent(...)`, ordinals,
  `-syscall=0xNNNN`, `-private`, `-arch=`, `-unix` markers.

## The dual build (PE + ELF)

Modern Wine 8+ builds almost everything as **PE files** (cross-compiled with MinGW,
`i686-w64-mingw32-gcc` / `x86_64-w64-mingw32-gcc`), not ELF .so. The old "builtin
ELF .so" model survives only for:
- ntdll.so (the bootstrap unix lib)
- the `__wine_unix_call` backend .so of some dlls (win32u, wined3d, etc.)

`tools/winebuild` is the key tool. From the `.spec` + import libs it generates:
- `*_stubs.c` — PE stubs for imports.
- syscall stubs / dispatcher glue for `-syscall=` entries.
- import libs (`libkernel32.a` etc.) linking PE files together.
- `.so` glue for unix libs (the `__wine_unix_call_funcs` table + dispatch).

`tools/makedep.c` computes dependencies and generates Makefiles; `winegcc` is the
driver that compiles/links PE and unix objects together.

`tools/make_requests` reads `server/protocol.def` → `include/wine/server_protocol.h`
(request structs + SERVER_START_REQ macros).

## Configure essentials

- `./configure --enable-win64` builds the 64-bit tree (default on x86_64).
- `--enable-archs=i386,x86_64` builds both (wow64-capable).
- `--with-mingw=<dir>` for the PE cross compiler; `--without-*` to drop unix deps
  (freetype, fontconfig, etc. — many optional).
- `make`, `make install` install into `/usr/local` (or `--prefix`).
- `make crosstest` runs the cross-compiled test suite.

## How a DLL is "builtin vs native"

At runtime, ntdll decides per-dll whether to load the builtin (our compiled PE)
or a native Windows dll, via the load order (`HKCU\Software\Wine\DllOverrides` +
default). Builtins are found under `dlls/<name>/<name>.dll` next to the tree or in
`lib/wine/...`. `loadorder.c` in ntdll/unix implements this; `WINEDLLOVERRIDES`
env var overrides (e.g. `winword=n`).

## The ntdll.so bootstrap & unix funcs

- `ntdll.so` is a normal ELF .so loaded by the loader (`loader/main.c`).
- It contains the unix-side of ntdll: `unix/loader.c` (module loading),
  `unix/virtual.c` (memory), `unix/thread.c` (threads), `unix/signal_*.c`
  (exceptions), `unix/server.c` (server IPC), `unix/sync.c`, etc.
- Its `unix_call_funcs[]` (see STUDY.md §2) is handed to the PE ntdll via the
  patched `__wine_unixlib_handle` export.

## Relevant files for reimplementation

- `tools/winebuild/spec32.c` — .spec parsing/export table generation.
- `tools/makedep.c` — the build rule engine.
- `tools/make_requests/*` — protocol codegen.
- `dlls/ntdll/unix/loader.c` — builtin dll search + map.
- `dlls/kernelbase`, `dlls/kernel32`, `dlls/ucrtbase` — PE-only, thin over ntdll.
