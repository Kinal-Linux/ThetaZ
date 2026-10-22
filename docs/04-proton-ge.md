# ThetaZ / "C~" — Proton GE specifics (GE-Proton11-3)

## What GE-Proton is

GE-Proton = Valve's Proton + GloriousEggroll's extra patches, built into a
self-contained distribution (`files/` dir) that Steam's compatibility tool
mechanism (or our own launcher) can invoke.

## Directory map

- `proton` — the big Python entry script (runs wine, creates prefix, applies fixes).
- `configure.sh` — build config (enables x86_64/i386, generates Makefile with
  build flags from distro).
- `Makefile` — top-level build; targets `help`, `install`, `deploy`, `redist`.
- `patches/` — patch sources applied during build:
  - `protonprep-valve-staging.sh` — resets submodules and applies patches to
    dxvk, vkd3d-proton, dxvk-nvapi, protonfixes, wineopenxr, steam_helper.
  - `wine-hotfixes/` — GE wine fixes: `wine-staging/` (applied to wine),
    `wine-wayland/`, `qcap-dshow-fixes`, `pending`, `disabled`.
  - `game-patches/` — per-game: battlenet, EAC timeout, maple story, PSO2,
    star citizen OS check, win32u WM_ACTIVATEAPP, layered-overlay, etc.
  - `proton/`, `protonfixes/`, `dxvk/`, `discordrpc`, `video-rework.sh`,
    `ge-video-rework/`, `proton-ds5-haptic`.
- `wine/` submodule — Valve's Wine fork (GE-Proton11 is Wine 11.0 based).
- `wine-staging/` — staging patchset (source).
- `dxvk/`, `dxvk-nvapi/`, `vkd3d-proton/`, `d7vk/`, `vklayers/`, `OpenXR-SDK/`,
  `wineopenxr/`, `openvr/`, `vrclient_x64/`, `lsteamclient/`, `steam_helper/`,
  `umu_helper/`, `FEX/`, `discord-rpc-bridge/`, `ffmpeg/`, `dav1d/`, `icu/`,
  `kaldi/`, `vosk-api/`, `openfst/`, `graphene/`, `libsoup/`, `libxkbcommon/`,
  `libxml2/`, `libpcap/`, `glslang/`, `meson/`, `SPIRV-Headers/`, `Vulkan-Headers/`,
  `nvidia-libs/`, `default_pfx.py`, `user_settings.sample.py`, `VERSION`,
  `compatibilitytool.vdf.template`, `steampipe_fixups.py`.

## proton script flow (session)

`Session.__init__`:
- dlloverrides: always builtin `steam.exe`; `beclient*.dll` native,builtin;
  `winebth.sys` disabled (crashes winedevice); opencl n,d except a few games.
- parse `STEAM_COMPAT_CONFIG` → `compat_config` (cmdlineappend, toggles).
- force `forcelgadd` by default; per-appid env (CPU topology, WINE_HIDE_AMD_GPU...).

`init_wine`:
- restore real `LC_ALL` from `HOST_LC_ALL`; set XLOCALEDIR; WINEARCH=wow64 when
  `PROTON_USE_WOW64=1`; prepend `lib_dir/*-linux-gnu` to LD_LIBRARY_PATH;
  `WINEDLLPATH=lib/vkd3d:lib/wine`; media transcoding env; per-game tweaks
  (dxvk HUD, latency flex, fsr, etc.).

`CompatData.setup_prefix`:
- create `compatdata/<appid>/pfx`; `wineboot -u` on first run; copy/symlink
  builtin dlls (vkd3d, dxvk, lsteamclient, vrclient) into the prefix;
  setup ddraw/d7vk; merge user dirs; registry fixups (e.g. protonfixes);
  create `drive_c/users/steamuser`; fonts symlinks; migrate old paths.

`run_proc`:
- finally spawns the game exe under the wine tree (`wine <exe> <args>`), with
  `WINEPREFIX`, steam env (SteamAppId, SteamGameId, STEAM_COMPAT_DATA_PATH...).

## GE fixes of note (wine-hotfixes/wine-staging)

- `winex11-Fixed-scancodes`, `winex11-ime-check-thread-data` — X11 driver fixes
  (IME/input correctness — relevant to Office IME window issues).
- `comctl32_animate_avi`, `windowscodecs-TIFF_Support`, `d3drm-starwars`,
  `mmsystem.dll16-MIDIHDR_Refcount`.

## FEX (arm64)

- `FEX/` — the x86_64/32 emulator used for running x86 Windows code on ARM64
  Steam Decks; Proton generates a FEX app config (`generate_fex_app_config`) and
  routes x86/32-bit executables through it while native ARM64 pieces run directly.

## How a game is launched (diagram)

```
Steam (compat tool = proton)
  proton <options> <exe> <args>
    g_proton = Proton(base_dir)         # finds files/, wine, libs
    session = Session()                 # env, dlloverrides, compat config
    session.init_wine()                 # env fixes, LD_LIBRARY_PATH, WINEDLLPATH
    setup_prefix()                      # pfx via wineboot, builtin libs, fixes
    session.run_proc()                  # wine <exe> under WINEPREFIX
```
