# ThetaZ — Office 365 Support Plan

## Scope

This document describes what it would take to run Microsoft Office 365
applications (Word, Excel, PowerPoint, Outlook, OneNote) and the Click-to-Run
bootstrap (`OfficeC2RClient.exe` / `OfficeClickToRun.exe`) under ThetaZ.

It is a planning document, not an implementation log. The work is divided
into phases that can be tackled in order, each landing with a small
regression test and a fresh app smoke-test.

## Why this is hard

The Office 365 family is a stack of PE64 binaries with a vast Windows API
surface. From the AGENTS.md §7 "INVESTIGATED — Prefix-app smoke tests"
investigation, a fresh `WINWORD.EXE` run hits ~14 unresolved imports
**after** the builtin-DLL blocklist removes Wine's `kernelbase` /
`ntdll` / `msvcrt` references. Unblocking the missing 14 was the goal of
that fix; making the app actually open a document requires the broader
coverage described below.

The challenge isn't any single API — it's the *breadth* of the surface and
the *depth* of integration required (COM, OLE, RPC, Direct2D, DirectWrite,
WRL, WinRT activation, etc.).

## Office 365 component inventory

The Office 365 install in `/home/eronic/Prefixes/ThetaZ/drive_c/`:

```
drive_c/Program Files/Microsoft Office/root/Office16/
    WINWORD.EXE, EXCEL.EXE, POWERPNT.EXE, OUTLOOK.EXE,
    ONENOTE.EXE, MSACCESS.EXE, MSPUB.EXE,
    ONEDRIVE.EXE, EQNEDT32.EXE, GRAPH.EXE,
    OfficeScrBroker.exe, capture.exe, excelcnv.exe, ...

drive_c/Program Files/Common Files/Microsoft Shared/ClickToRun/
    OfficeC2RClient.exe          # the bootstrap
    OfficeClickToRun.exe         # the launcher (per-product)
    OfficeC2RCom.dll             # COM + SOAP + update logic
    ApiClient.dll, ApiClient_arm64.dll
    c2r64werhandler.dll          # Windows Error Reporting bridge

drive_c/ProgramData/Microsoft/ClickToRun/ProductReleases/
    <product-guid>/x-none.16/stream.x64.x-none.man.dat  # install manifest
    <product-guid>/en-us.16/stream.x64.en-us.man.dat    # language pack
    <product-guid>/sd640.deltaNN.cab_extract/...        # CAB-packaged updates

drive_c/Windows/System32/
    WINWORD.EXE  ← typically absent; Office installs into Program Files
```

## API coverage map (current state)

Counts below come from `grep -r "register_api(\"<dll>\."` over `core/api/`.
"by name" = `register_api(dll, name, fn)`, "by ordinal" =
`register_api_ordinal(dll, ord, fn)`.

| DLL | by name | by ordinal | Office need | gap |
|---|---|---|---|---|
| kernel32.dll | 273 | many | critical | covered |
| kernelbase.dll | 162 | 0 | critical | partial — needs ~50 more |
| ntdll.dll | 147 | 0 | critical | partial — needs RtlUnwindEx, NtQueryVirtualMemory, etc. |
| user32.dll | 272 | 0 | critical | covered for windows/controls; need IME, DPI awareness |
| gdi32.dll | 73 | 0 | critical | partial — fonts, paths, regions |
| advapi32.dll | 123 | 0 | high | covered for registry/eventlog; need LSA, scm |
| ole32.dll | 31 | 0 | critical | needs CoCreateInstance, IStream, moniker binding |
| oleaut32.dll | 30 | 0 | critical | covered (BSTR/VARIANT); need SafeArray of records |
| combase.dll | 0 | 0 | critical | entire surface — needs ROAPI, activation factories |
| shell32.dll | 4 | 0 | medium | needs SHGetFolderPath, SHCreateItemFromIDList |
| crypt32.dll | 0 | 0 | critical | entire surface — code-signing + cert chain |
| wintrust.dll | 0 | 0 | critical | entire surface — Authenticode verification |
| bcrypt.dll | 0 | 0 | critical | entire surface — AES, SHA, ECDSA |
| wininet.dll | 0 | 0 | critical | entire surface — HTTP for C2R download |
| winhttp.dll | 10 | 0 | medium | partial — needs WinHttpOpen, etc. |
| webio.dll | 0 | 0 | medium | entire surface — webservices.dll backend |
| webservices.dll | 0 | 0 | medium | entire surface — SOAP/WCF for activation |
| msi.dll | 0 | 0 | critical | entire surface — Office is installed via MSI |
| msvcp140.dll | 4 | 0 | high | partial — needs ~600 more C++ STL symbols |
| vcruntime140.dll | 12 | 0 | high | partial |
| d3d11.dll | 0 | 0 | high | entire surface — needed for D2D rendering |
| dxgi.dll | 0 | 0 | high | entire surface — D3D swap chain |
| d2d1.dll | 0 | 0 | critical | entire surface — Office ribbon uses D2D |
| dwrite.dll | 0 | 0 | critical | entire surface — text rendering |
| dwmapi.dll | 0 | 0 | low | entire surface — DWM composition |
| uxtheme.dll | 0 | 0 | medium | entire surface — visual styles |
| mfplat.dll | 8 | 0 | low | skeleton (Phase A of MF workstream) |
| mfmediaengine.dll | 2 | 0 | low | skeleton (Phase A) |
| winegstreamer.dll | 0 | 0 | low | not yet (Phase B) |

The "Office need" column is the importance to running Office apps. "Critical"
= the app will not open at all without it. "High" = the app will crash on a
specific common action (save, print, ribbon paint). "Medium" = a feature
won't work. "Low" = a polish item.

## Phased plan

### Phase 1 — OfficeC2RClient smoke-test (4-6 weeks)

**Goal**: `OfficeC2RClient.exe` (or `OfficeClickToRun.exe`) loads, parses
its manifest, and reports its update status. Does NOT have to actually
download or install anything yet.

Required additions:
- `core/api/crypt32.cpp` — CertOpenSystemStore, CertGetCertificateChain,
  CertVerifyCertificateChainPolicy, CertFreeCertificateChainEngine (~30 APIs).
- `core/api/wintrust.cpp` — WinVerifyTrustEx (~5 APIs, just enough to
  verify Microsoft's signature on a CAB header).
- `core/api/bcrypt.cpp` — BCryptOpenAlgorithmProvider, BCryptHash,
  BCryptGetProperty (~15 APIs).
- `core/api/wininet.cpp` — InternetOpenA/W, InternetOpenUrlA/W,
  InternetReadFile, InternetCloseHandle, HttpSendRequestA/W
  (~30 APIs).
- `core/api/combase.cpp` — CoCreateInstance, CoInitializeEx,
  CoTaskMemFree, RoGetActivationFactory (~30 APIs). This is a LARGE
  surface; the stub can return E_NOTIMPL for most, but the
  activation-factory path (RoGetActivationFactory) needs a real
  implementation that maps a Windows.Activation.ActivationFactory
  class name to a host-side implementation.

Test: `tests/c2r_smoke.c` (or just run
`./build/ThetaZ "C:/Program Files/Common Files/Microsoft Shared/ClickToRun/OfficeClickToRun.exe" platform=x64 scenario=install`).

### Phase 2 — WINWORD opens a blank document (8-10 weeks)

**Goal**: `WINWORD.EXE` launches, creates a blank document, paints the
ribbon (D2D) and canvas, accepts input via keyboard/mouse, saves a `.docx`.

Required additions:
- `core/api/d2d1.cpp` — Direct2D: D2D1CreateFactory, ID2D1HwndRenderTarget,
  DrawRectangle, FillRectangle, DrawTextLayout (~40 APIs). Needs
  `IDWriteFactory` and `IDWriteTextFormat` from dwrite.
- `core/api/dwrite.cpp` — DirectWrite: DWriteCreateFactory,
  CreateTextFormat, CreateTextLayout, DrawText (~25 APIs).
- `core/api/d3d11.cpp` — D3D11 minimal: D3D11CreateDevice,
  ID3D11Device::CreateTexture2D, CreateRenderTargetView (~30 APIs).
  Without DXVK, the device can use a stub software rasterizer OR
  leverage the existing X11 + OpenGL backend for swap-chain present.
- `core/api/dxgi.cpp` — DXGI: CreateDXGIFactory, EnumAdapters,
  CreateSwapChain (~15 APIs).
- `core/api/dwmapi.cpp` — DwmSetWindowAttribute, DwmGetWindowAttribute
  (~10 APIs; can stub most).
- `core/api/uxtheme.cpp` — OpenThemeData, DrawThemeBackground (~20 APIs;
  most can return E_NOTIMPL or no-op).
- `core/api/gdi32_full.cpp` — Path building (BeginPath, EndPath,
  StrokePath), font enumeration, regions (CombineRgn).
- `core/api/ole32_full.cpp` — OleInitialize, CoCreateInstance (full),
  IStream implementations (CreateStreamOnHGlobal), IStorage for
  compound document files (`.doc`).
- `core/api/kernelbase_extended.cpp` — The remaining 50 functions WINWORD
  needs (mostly CreateFile2, InitializeCriticalSectionEx, etc.).

Test: `./build/ThetaZ "C:/Program Files/Microsoft Office/root/Office16/WINWORD.EXE" /new`.

### Phase 3 — Document save/load + .docx parsing (4-6 weeks)

**Goal**: WINWORD can save a `.docx` (which is a ZIP+XML) and re-open it.

Required additions:
- `core/api/cabinet.cpp` — Cabinet API: Extract, FCIAddFile (or use
  host-side `libzip`/`libarchive` directly).
- ZIP support — use host-side `libzip` via pkg-config (analogous to the
  way we use libffi/libX11).
- XML parsing — `libxml2` via pkg-config.
- `core/api/compression.cpp` — RtlDecompressBuffer, RtlCompressBuffer.

Test: open WINWORD, type text, save as `.docx`, close, re-open, verify text.

### Phase 4 — PowerPoint / Excel / OneNote (parallel workstreams)

Once WINWORD is stable, the same MF/D2D surface supports the other apps.
PowerPoint adds slide-transition effects (more MF surface); Excel adds
calc engine (mostly independent of our work); OneNote adds note storage
(uses WinRT).

Each app gets a 2-3 week implementation slice using the existing
WINWORD infrastructure.

### Phase 5 — Outlook (separate, 6-8 weeks)

Outlook is the hardest because it pulls in:
- MAPI (mapi32.dll, ~200 APIs)
- Active Directory / LDAP (wldap32.dll, ~80 APIs)
- Exchange Web Services (EWS, SOAP)
- WinRT for live tiles

It is the only Office app that *requires* network connectivity for
basic functionality. Plan it last.

### Phase 6 — Activation (Office 365 license check)

Office 365 apps validate the subscription via:
- `webservices.dll` SOAP calls to `https://login.microsoftonline.com/`
- Cryptographic signature of the response using Microsoft's public key
- Local registry check (HKLM\Software\Microsoft\Office\16.0\Common\Online)
- Telemetry (`wuaueng.dll`, `msvcrt` rand)

For a C2R-only test we don't need real activation — return a fake
"subscribed" status from our license API and Office apps will work in
"unlicensed" mode (still opens documents, just shows a banner).

## Architectural decisions

### A1. No DXVK — original D3D11/DXGI

DXVK is a Vulkan-based D3D9/10/11 implementation that ships with Proton.
We could embed it as a submodule + ship its DLLs alongside ThetaZLib, but
that violates "original code only". The ThetaZ-equivalent is an original
D3D11 → Vulkan (or OpenGL) layer using existing
`core/api/d3d11.cpp` interfaces.

This is months of work. For the Office workstream, start with a
**software rasterizer stub** for D3D11 — enough to make D2D's GPU-accel
fall back to WARP and paint correctly. DXVK-quality hardware acceleration
is a separate workstream.

### A2. No COM-on-host — original COM runtime

COM is fundamental to Office. The ThetaZ-equivalent needs to implement:
- `CoCreateInstance` / `CoCreateInstanceEx` — class factories registered
  via the registry.
- `IUnknown`, `IStream`, `IStorage`, `IDispatch` — base interfaces.
- `IRegistry` / Activation contexts — for DLL loading by COM CLSID.
- WinRT activation (`RoGetActivationFactory`) — maps WinRT class names to
  factories.

Wine implements all of this. We need to write our own version, but it
can be smaller (Office uses a subset of COM).

### A3. WinRT is a separate workstream

WinRT is the modern Windows runtime that Office uses for notifications,
live tiles, share contracts, etc. It is **a different ABI from COM**
(RoGetActivationFactory returns WinRT objects with a different vtable
layout). Implementing it is a 6-month effort. For Phase 1-4 of Office,
WinRT is not required — the apps degrade gracefully when WinRT APIs are
missing.

### A4. Treat download/signing as optional for now

The C2R bootstrap needs network + crypto to actually update Office. For
*running already-installed Office*, none of this is needed. Phase 1 is
about parsing the local manifest + reporting status; network calls return
ERROR_INTERNET_CANNOT_CONNECT and the app gracefully degrades.

### A5. Accept native hosting for some DLLs

For DLLs with 1000+ exports and no behavioral variation between Windows
versions (msvcp140, vcruntime140, ucrtbase), the cleanest answer is to
use the host's libstdc++/glibc directly. ThetaZ already does this for
ucrt (the CRT calls go through the host libc). For msvcp140 the work is
mapping `std::string` → `std::string`, etc. — direct ABI compat if the
host uses libstdc++ with `_GLIBCXX_USE_CXX11_ABI=0`.

The risk: Microsoft ABI differs from Itanium ABI (the name mangling,
exception handling). For Office apps, MSVC's C++ exceptions are critical.
We have `__CxxFrameHandler` working in ucrt.cpp; it just needs to be
extended to handle the much larger number of catch blocks in real apps.

## Test strategy

### Unit tests
- `tests/crypt32_basic.c` — CertOpenSystemStore, CertEnumCertificatesInStore
- `tests/bcrypt_hash.c` — BCryptHash with SHA256, verify against
  `sha256sum` on host
- `tests/wininet_basic.c` — InternetOpen + read from a known URL
  (only if we can reach one — gate the test on internet connectivity)
- `tests/com_init.c` — CoInitializeEx, CoCreateInstance on a dummy CLSID

### App smoke tests
- `tests/office/c2r_smoke.sh` — run OfficeC2RClient.exe, expect
  "Office is up to date" or similar
- `tests/office/winword_new.sh` — run WINWORD.EXE /new, expect window
  appears, no crashes for 30s
- `tests/office/winword_save.sh` — type text, save, re-open, verify

### Regression coverage
- Each new API added gets a one-line smoke test
- The current `tests/run_tests.sh` suite stays green
- `cmake --build build` produces 0 errors

## Risk register

| Risk | Probability | Impact | Mitigation |
|---|---|---|---|
| Direct2D needs hardware GPU acceleration we can't provide | high | high | Provide WARP software fallback (ID3D11Device with `D3D_DRIVER_TYPE_WARP`) |
| Office uses undocumented API surface beyond what we can enumerate | high | medium | Add `!! unresolved` tracking, prioritize by appearance frequency |
| WinRT activation is required by Office for basic UI | low | high | Survey with `dumpbin /imports WINWORD.EXE` — if no WinRT imports, this is not a blocker for Phase 1-2 |
| msvcp140 ABI differences cause crashes in std::string/std::vector | medium | high | Use the host's libstdc++ with `_GLIBCXX_USE_CXX11_ABI=0` to match MSVC's ABI |
| Office updates itself and breaks our assumptions | high | low | Pin to a specific Office version (16.0.x) for testing |
| Network/COM/Crypt code paths cause flaky tests | high | medium | All tests run with a 30-second timeout; offline-friendly fallbacks |

## Effort estimate (very rough)

| Phase | New code (LOC) | Calendar time | Confidence |
|---|---|---|---|
| 1. C2R smoke | 3000 | 4-6 weeks | medium |
| 2. WINWORD open | 8000 | 8-10 weeks | low |
| 3. Save/load .docx | 2000 | 4-6 weeks | high |
| 4. PowerPoint/Excel/OneNote | 5000 each | 6-9 weeks each | low |
| 5. Outlook | 4000 | 6-8 weeks | low |
| 6. Activation | 1000 | 1-2 weeks | medium |
| **Total** | **~28000** | **~9-12 months** | |

**Realistic delivery for a usable Office 365 in ThetaZ: 6-9 months of
focused development, assuming the dev is the same person doing the
interpreter work.**

## What NOT to do

- **Do not** try to ship a Wine fork and call it ThetaZ. The license
  forbids it, and the design philosophy (single C++17 build, original
  code) would be violated.
- **Do not** write a Phase 2 implementation that assumes DXVK. Write the
  software rasterizer fallback and document DXVK-equivalent as a
  separate workstream.
- **Do not** try to support Office 2019/2021 LTSC alongside Office 365.
  They use slightly different activation and update paths. Pin to one
  version (Office 365 16.0.x as installed in
  `/home/eronic/Prefixes/ThetaZ/`).
- **Do not** add APIs speculatively. Each API added without a real app
  calling it is dead code that gets stale. Add it when an app complains.

## Tracking

This document is the source of truth for the Office 365 workstream.
When a phase starts, create a new sub-doc:
`docs/OFFICE_365_PHASE1.md`, etc., with detailed API lists and
regression test names.

When a phase completes, update the API coverage table above with new
counts.
