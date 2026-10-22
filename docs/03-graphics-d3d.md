# ThetaZ / "C~" — Graphics & D3D stack (win32u / user32 / d3d / dcomp)

Relevant to the Word/Mso40 investigation: Word's D3D11 render thread crashed in
Mso40 (a queue drain), but the pipeline is: WINWORD → d3d11.dll → wined3d → Vulkan.

## win32u — the "win32k" replacement

- `dlls/win32u/` is a single PE DLL that implements the NtUser*/NtGdi* syscalls
  that real Windows puts in win32k.sys. user32.dll and gdi32.dll are thin wrappers
  over it (`IMPORTS = ... win32u`).
- Files: message.c (GetMessage/PeekMessage, window proc dispatch), class.c, window.c
  (in `mapping.c`?), input.c/rawinput.c, font.c/freetype.c (TrueType via FreeType),
  dc.c/dib.c/bitblt.c/dibdrv/ (GDI drawing), emfdrv.c/printdrv.c (EMF+print),
  cursoricon.c, menu.c, hook.c, clipboard.c, imm.c, opengl.c, spy.c.
- It also has a small unix backend for what needs raw X/Wayland: `driver.c` +
  `dlls/win32u/unix/` (X11: the `winex11` driver in `dlls/winex11.drv`).
- Window manager integration: `dlls/winex11.drv` (X11 backend) implements the
  `driver` interface (create window, draw, input) and talks to the wineserver
  window objects; there is also `winewayland.drv`.
- The DCE (device context) batching: GDI operations can be batched in the TEB
  (`TEB->GdiBatchCount`, `GdiTebBatch`), flushed by `NtGdiFlush`.
- Input: the rawinput/input threads convert X/Wayland events → win32 messages via
  the wineserver queue; `GetRawInputData` etc.

## Direct3D pipeline

- `d3d11.dll` (PE) implements ID3D11Device/DeviceContext/swapchain via
  `IMPORTS = dxguid uuid dxgi wined3d`.
- `d3d12.dll` similarly over `wined3d` + dxgi.
- `dxgi.dll` (PE) implements IDXGI* (factory, adapter, swapchain) over wined3d.
- `wined3d` is the real engine: `dlls/wined3d/` (PE + unix). It has backends:
  - `unix/vulkan.c` — the Vulkan backend (preferred; used by dxvk and native).
  - GL backend via `dlls/wined3d/` unix + `winex11.drv`/`winewayland.drv` OpenGL.
  - Adapter selection: `d3d12`/`d3d11` can also be implemented entirely by dxvk /
    vkd3d-proton on Proton (native dlls shipped by Proton, via DLL overrides).
- CSMT: the wined3d multithreaded command stream (`csmt`), controlled by
  `HKCU\Software\Wine\Direct3D\csmt` (0/1). With csmt=1 a separate wined3d thread
  processes command lists; csmt=0 processes on the calling thread (the change that
  stabilized Word).

## dcomp (DirectComposition)

- `dlls/dcomp/` implements IDCompositionDevice/Target/Visual on top of the
  swapchain/win32u (`device.c`, `dcomp.spec`). Word/Mso40 uses it for window
  content composition on modern Office builds.

## Other graphics-related dlls

- `dwrite` (DirectWrite, FreeType-backed), `d2d1` (Direct2D over wined3d),
  `gdiplus` (over gdi32 + freetype), `mshtml`/`winebrowser` for WebView2 fallback.

## Notes for Word/O365 (64-bit, C2R)

- WINWORD.EXE is pure x64; it loads d3d11/dxgi/dcomp/dwrite/d2d1 builtins.
- The crash thread was the D3D11 device render thread; no d3d/dxgi errors appeared
  before the fault — the fault was inside Office's own Mso40UIwin32client.dll
  (a queue CS never initialized), fixed by patching that dll (see prior work).
- `wined3d` uses Vulkan by default on recent Wine (radv LLVM/ACO). The log line
  "radv is not a conformant Vulkan implementation" is benign.
