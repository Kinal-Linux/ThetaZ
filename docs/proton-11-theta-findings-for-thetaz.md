# Proton-11-Theta Findings for ThetaZ Implementation

> **Audience:** ThetaZ DeepSeek Agent
> **Trigger:** This document contains implementation findings from proton-11-theta research that can be applied to ThetaZ's Windows compatibility layer.
> **Priority:** Non-blocking — apply when current ThetaZ tasks allow.

---

## Executive Summary

Built **proton-11-theta** (Valve proton_11.0 + SpookySkeletons RTSP 11.0 + theta WMF fixes) with complete GStreamer pipeline for VRChat AVPro video player support. Key finding: **ThetaZ can implement equivalent AVPro support by adding GStreamer scheme handlers and media source infrastructure.**

---

## What proton-11-theta Contains

### Base Components
| Component | Version | Purpose |
|-----------|---------|---------|
| Proton | proton_11.0 | Steam compatibility layer base |
| RTSP Patchset | 67 patches (rtsp-11.0-20260609-4) | Media Foundation, winegstreamer fixes |
| Wine (theta-11-wmf) | e92981ddbf2 | MF/V4L2, qcap webcam fixes |
| GStreamer | 1.22.5 | Core multimedia framework |
| gst-plugins-base | 1.22.5 | Core plugins (ogg, opus, vorbis, theora) |
| gst-plugins-good | 1.22.5 | soup (HTTPS), rtsp, rtp, isomp4, matroska, avi |
| gst-plugins-bad | 1.22.5 | hls, mpegtsdemux, audiobuffersplit, videoparsers |
| gst-libav | 1.22.5 | ffmpeg codecs (H.264, H.265, VP8/9, AV1, AAC, MP3) |
| gst-plugins-rs | dav1d | AV1 decoder |
| **gst-plugins-ugly** | **NEW** | **asfdemux (ASF/WMV) — CRITICAL for VRChat** |

### Critical Patches for AVPro Video Players

1. **GStreamer Scheme Handler** (commit `4d9083e70bc`)
   - Registers `GStreamerSchemePlugin` for `http:`, `https:`, `rtsp:` URIs
   - Uses `uridecodebin` for streaming (not download-then-play)

2. **libsoup/souphttpsrc Hooks** (commit `eef101d3783`)
   - Forces HTTP/1.1 (`soup_message_set_force_http1`)
   - Sets native User-Agent: `NSPlayer/12.00.19041.4894 WMFSDK/12.00.19041.4894`

3. **RTSP URI Variants** (commit `86a3f95c535`)
   - Registers `rtspu:`, `rtspt:`, `rtsph:`, `rtsp-sdp:`, `rtsps:`, `rtspsu:`, `rtspst:`, `rtspsh:`

4. **mfmediaengine Fixes** (commits `f3f25735b3c`, `bea1097638b`)
   - Sample-pump fix
   - Video frame sink queue size 2→5

5. **media-converter Refactor** (commit `881a9dc8332`)
   - Videoconv stream discarding helper

6. **qcap Webcam Fixes** (commit `3b4a5f40`)
   - Smart Tee RGB32 + v4l wow64 marshaling

---

## What ThetaZ Can Implement

### 1. GStreamer Scheme Handler for ThetaZ

**Current ThetaZ State:** No GStreamer integration. Uses custom X11 + minimal Win32 API.

**Required Addition:**
```cpp
// core/api/gstreamer_scheme_handler.cpp
// Implement IMFSchemeHandler for http/https/rtsp URIs
// Route to GStreamer uridecodebin via winegstreamer bridge
```

**Key Interfaces:**
- `IMFSchemeHandler` — BeginCreateObject/EndCreateObject for URI resolution
- `IMFAsyncCallback` — Async completion
- Registration in registry: `HKLM\Software\Microsoft\Windows Media Foundation\SchemeHandlers\http:` → CLSID

### 2. Media Foundation Media Source Bridge

**Current ThetaZ State:** Minimal MF support in `kernel32_runtime.cpp`/`ucrt.cpp`.

**Required Addition:**
```cpp
// core/api/winegstreamer_bridge.cpp
// Bridge MF IMFMediaSource → GStreamer pipeline
// Implements:
//   - IMFMediaSource
//   - IMFMediaSourceEx
//   - IMFPresentationDescriptor
//   - IMFStreamDescriptor
//   - IMFMediaTypeHandler
```

### 3. GStreamer Pipeline Integration

**Required Libraries (link into ThetaZLib):**
- `gstreamer-1.0`
- `gstbase-1.0`
- `gstpbutils-1.0` (for `uridecodebin`)
- `libsoup-3.0` (HTTPS)
- `gstreamer-plugins-bad` (hls, mpegtsdemux)

### 4. Launch Options Support

ThetaZ should honor these VRChat launch options:
```bash
WINEDLLOVERRIDES="iyuv_32=" %command%
PROTON_ENABLE_WAYLAND=1 %command%
--enable-avpro-in-proton %command%
```

---

## Implementation Priority for ThetaZ

| Priority | Component | Effort | Blockers |
|----------|-----------|--------|----------|
| **P0** | GStreamer scheme handler registration | Medium | None |
| **P0** | uridecodebin-based media source | High | GStreamer dep |
| **P1** | libsoup HTTPS hooks | Medium | libsoup dep |
| **P1** | RTSP URI scheme variants | Low | Registry only |
| **P2** | mfmediaengine sample-pump logic | Medium | MF impl needed |
| **P2** | qcap webcam (DirectShow) | High | quartz/qcap/qedit |

---

## Code Locations in ThetaZ to Extend

```
core/
├── api/
│   ├── gstreamer_scheme_handler.cpp    # NEW: IMFSchemeHandler for http/https/rtsp
│   ├── winegstreamer_bridge.cpp        # NEW: MF ↔ GStreamer bridge
│   ├── mf_media_source.cpp             # EXTEND: Add GStreamer backend
│   └── kernel32_runtime.cpp            # EXTEND: Handle launch options
├── loader/
│   └── main.cpp                        # EXTEND: Load GStreamer plugins
├── ntdll/
│   └── virtual.cpp                     # EXTEND: GStreamer plugin paths
└── exception/
    └── exception.cpp                   # EXTEND: GStreamer error handling
```

---

## Dependencies to Add to CMakeLists.txt

```cmake
# GStreamer for AVPro video playback
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0>=1.22
                              gstbase-1.0>=1.22
                              gstpbutils-1.0>=1.22
                              libsoup-3.0)
pkg_check_modules(GST_PLUGINS_BAD REQUIRED gstreamer-plugins-bad-1.0)

target_link_libraries(ThetaZLib PRIVATE
    ${GSTREAMER_LIBRARIES}
    ${GST_PLUGINS_BAD_LIBRARIES}
)
```

---

## Testing Verification

### Proton-11-Theta Test Matrix (Manual Verification Needed)

| Test | Command | Expected |
|------|---------|----------|
| YouTube playback | VRChat world with YouTube URL | Video plays via souphttpsrc |
| Twitch stream | VRChat world with Twitch URL | Live stream plays |
| RTSP stream | VRChat world with rtsp:// | Stream plays via rtspsrc |
| Local file | VRChat world with file:// | file:// scheme handler works |
| MPEG-TS (VRCDN) | VRChat world with VRCDN URL | mpegtsdemux handles |

### ThetaZ Test Additions

```c
// tests/gstreamer_avpro_test.c
// Test: IMFSourceResolver resolves http:// URL → IMFMediaSource
// Test: Media source plays H.264/AAC stream
// Test: RTSP URI resolves and plays
```

---

## Non-Blocking Integration Notes

1. **GStreamer is optional** — guard with `#ifdef THETAZ_HAVE_GSTREAMER`
2. **Fallback to existing behavior** if GStreamer unavailable
3. **No changes to x86 interpreter** — purely Win32 API layer
4. **Minimal runtime overhead** — only activates when MF media source created

---

## Files Created in proton-11-theta

```
/mnt/hdd/.proton-build/proton-11-theta/
├── Makefile.in                    # Modified: +gst-plugins-ugly build rules
├── gstreamer/subprojects/gst-plugins-ugly/  # Submodule (existing in GStreamer)
└── .tiv/                          # tiv VCS repo (commits 0+1, 0+2, 0+3)
```

### Key Commits
- `0de3753` (v0+1): Initial proton-11-theta base
- `bab8de1` (v0+2): Added gst-plugins-ugly build rules (ASF, MPEG-2, AC3)
- `b91e387` (v0+3): Set GPL plugins to `auto` (graceful dep handling)

---

## Recommendation for ThetaZ

**Implement GStreamer scheme handler first** — it's the highest-impact, lowest-effort path to unblock VRChat AVPro video players (TVs, YouTube, Twitch, VRCDN streams).

The webcam/face-tracking path (qcap/quartz/qedit) is separate and requires DirectShow implementation — can be deferred.

---

## Questions for ThetaZ Team

1. **GStreamer dependency acceptable?** (LGPL, runtime-dep only)
2. **Target ThetaZLib or separate plugin DLL?** (Recommend ThetaZLib for simplicity)
3. **Wayland/X11 integration needed?** (GStreamer handles both via gl/videosink)

---

*Generated: 2026-09-11 from proton-11-theta research session*
*Author: Sisyphus (Orchestrator)*
*For: ThetaZ DeepSeek Agent — apply when current task permits*