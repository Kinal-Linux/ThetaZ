# ThetaZ — Proton Fork Pattern Reimplementation

## Why this document exists

The `proton-rtsp-full` fork at `/mnt/hdd/.proton-build/proton-rtsp-full` is a
from-Wine-up Proton build that the user patches to add:
- AVPro video support (IMFMediaEngine extension)
- RTSP camera source (qcap)
- Media Foundation frame queue fixes

This is a productive pattern: **track upstream + cherry-pick fixes + rebuild
+ install**. But ThetaZ is GPL-2.0, original code only — we cannot embed
Wine or Proton. So this document describes the **equivalent pattern inside
ThetaZ**: how ThetaZ reproduces the same fork-then-rebuild cycle **without
importing upstream code**.

## The Proton fork pattern, abstracted

```
Proton:  Wine (submodule) ──► wine-build/ (rsync from wine/) ──► DLLs ──► dist/
                                              ▲
                                              │ patches applied
                                              │
                                        proton fork commit

ThetaZ:  (no submodule — we ARE the implementation)
         core/api/*.cpp ──► CMakeLists.txt ──► ThetaZLib.so ──► binary
                            ▲
                            │ fixes
                            │
                       this git tree
```

The functional equivalence:

| Proton step | ThetaZ equivalent |
|---|---|
| Bump `wine` submodule pointer to a new upstream commit | No bump — we don't have an upstream to bump. Instead, *expand coverage*: identify a new API class the user needs and design an original implementation. |
| Cherry-pick a fix from `wine-staging` or upstream wine | Apply the equivalent fix in our own `core/api/`, `core/x86/`, or `core/ntdll/` code — but **rewrite, don't copy**. |
| Run `make` (wine rebuild is ~30-60 min) | `cmake --build build -j8` (~30 sec for incremental). |
| `cp -a dist/* ~/.local/share/Steam/compatibilitytools.d/proton-fork/` | No install step. The ThetaZ binary is rebuilt and that's the deliverable. |
| Launch a game with `Steam Launch Options` `PROTON_USE_NEW_PROTON=...` | Run `./build/ThetaZ app.exe` directly. |

## What "rewriting, not copying" looks like

Concrete example: the AVPro TV fix that patched `IMFMediaEngineEx` to call
the extension's texture sink on every frame.

**Wine's patch (study-only):** added a `do_get_sample` path in
`mfmediaengine/media_engine.c` that, when the extension's
`OnVideoSample` callback is set, bypasses the normal sample-queue delivery
and instead calls the extension's `BeginGetSample` directly.

**ThetaZ equivalent (when we get to Phase C):** in `core/api/mfmediaengine.cpp`,
the `IMFMediaEngineEx::Play` implementation walks the topology, and when it
finds an `IMFMediaEngineExtension` attached, calls its
`OnVideoSample(nv12_buffer, width, height, stride, pts)` directly. The
extension's `OnVideoSample` then either:
- Blits NV12 into an OpenGL texture (for our X11+OpenGL renderer), or
- Posts a WM_APP message to a guest window via `PostMessageW` so the
  app's WNDPROC can repaint.

The *behavior* matches Wine's patch. The *code* is 100% original.

## The "fork" mental model inside ThetaZ

When you need to fix a ThetaZ bug, the workflow is:

1. **Reproduce** — run a real .exe through ThetaZ that hits the bug.
   Use `THETAZ_TRACE_INSNS=1 THETAZ_TRACE_API=1` for diagnostics.
2. **Locate** — find the offending code in `core/`. The user-reported bug
   usually names the API (`HeapAlloc` zero-init, `gethostbyname` 64-bit ptr,
   etc.). `grep -rn` from the project root.
3. **Fix minimally** — change the smallest amount of code that fixes it.
   Don't refactor while fixing. Add a regression test if the bug is subtle
   (it usually is).
4. **Verify** — run the regression suite. Make sure `cmake --build build`
   produces 0 errors.
5. **Document** — add a one-paragraph note to `AGENTS.md` §7
   "SESSION HISTORY" describing what was fixed and why. This is the
   memory that the next session will read.

## When the pattern doesn't fit: large additions

Sometimes the fix is too big for one commit. The Office 365 workstream
(see `docs/OFFICE_365.md`) is a six-month effort. For work like this:

1. **Create a planning document** at `docs/OFFICE_365.md` with phases,
   API coverage tables, and dependency graph.
2. **Land Phase A** as one commit (skeleton, no-op APIs).
3. **Land each subsequent phase** as its own commit, with regression tests
   for the newly-supported APIs.
4. **Update ROADMAP.md** at the end of each phase with what shipped.

This matches the cadence of the Proton fork — many small commits, each
with a clear purpose.

## The ThetaZ ↔ Proton mapping (current state)

| Proton subsystem | ThetaZ equivalent | Status |
|---|---|---|
| Wine kernel32.dll | `core/api/kernel32.cpp` + `core/api/kernel32_runtime.cpp` + `core/api/kernel32_winword.cpp` + `core/api/kernel32_batch.cpp` | Substantial coverage |
| Wine user32.dll / gdi32.dll | `core/api/window.cpp` (X11 backend) | Working for windows, buttons, checkboxes, edit/listbox/combobox, timers |
| Wine ntdll.dll | `core/ntdll/{thread,virtual,sync,teb}.cpp` | Working |
| Wine ws2_32.dll | `core/api/ws2_32.cpp` | Working (real `select`, `gethostbyname` fixed) |
| Wine ucrtbase.dll / msvcrt.dll | `core/api/ucrt.cpp` | Substantial (printf, malloc, FILE, CRT startup, SEH, C++ EH) |
| Wine oleaut32.dll | `core/api/oleaut32.cpp` | BSTR + VARIANT |
| Wine combase.dll | `core/api/wine_compat.cpp`, `core/api/ole32_apis.cpp` | Stub |
| Wine advapi32.dll | `core/api/crypto_runtime.cpp` (crypt subset) | Partial |
| Wine mfplat.dll | `core/api/mfplat.{h,cpp}` (Phase A stub) | New — skeleton only |
| Wine mfmediaengine.dll | (not yet) | Phase B/C |
| Wine winegstreamer.dll | (not yet) | Phase B — needs GStreamer integration |
| Wine mfreadwrite.dll | (not yet) | Phase B |
| Wine d3d11.dll / dxgi.dll / d2d1.dll / dwrite.dll | (not yet) | Separate workstream (see Phase D) |
| Wine crypt32.dll / wintrust.dll / bcrypt.dll | (not yet) | Office 365 workstream |
| Wine wininet.dll / winhttp.dll | (not yet) | Office 365 workstream |
| Proton DXVK | (not yet) | d3d11-on-vulkan; separate workstream |
| Proton vkd3d-proton | (not yet) | d3d12-on-vulkan; separate workstream |
| Proton media stack (gstreamer plugins) | (not yet) | Phase B — needs libgstreamer pkg-config link |

## Fork hygiene: rules for original code

Per `AGENTS.md` §1 (WHAT THIS PROJECT IS):
> The `Wine/` and `ProtonGE/` directories in the tree are **study
> references only** — they are NOT compiled, linked, or included. Do not
> copy from them; stay original.

When you find a Wine patch that fixes a bug you're hitting:

1. **Understand** the bug. Read the Wine patch's commit message + the
   function it changes. Map the Wine data structures to your data
   structures (they're different — Wine uses `struct list`, ThetaZ uses
   `std::vector`).
2. **Translate** the fix. Don't line-by-line port — rewrite using the
   idioms of the host codebase. The fix is a *concept*, not *code*.
3. **Add a regression test** before checking in. The bug existed
   silently because we didn't have coverage for the corner case.
4. **Note the source** in the commit message ("Fixes the same bug as
   upstream wine commit 1c84c378466, rewritten for our heap model") —
   this makes provenance traceable without copying code.

## What this document is NOT

This document does not say we should re-implement Wine. ThetaZ is
**not** a Wine clone. It is an original implementation of Windows
compatibility using C++17 idioms. Wine is a study reference for behavior,
not a code source.

If you find yourself wanting to copy 50+ lines from Wine, **stop**.
Re-architect the fix to fit the existing code instead.

## Summary

The Proton fork pattern — track + cherry-pick + rebuild + install —
applies to ThetaZ as: **expand + fix + verify + document**, with
"submodule bump" replaced by "API coverage expansion" and "cherry-pick"
replaced by "translate the fix into our own code". The cycle is
identical; the source of patches is different.

When working on the Proton fork itself (e.g. a TV-breaking bug),
use the actual Proton workflow. When working on ThetaZ, use this
document as the equivalent.
