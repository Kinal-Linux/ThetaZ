// ThetaZ — Wine DLL compatibility layer.
// Copyright (c) 2026 ThetaZ Project. MIT License.
//
// When OfficeSetup.exe (or other apps) delay-load Wine DLLs like comctl32.dll
// from the host filesystem, those DLLs import functions from user32.dll,
// gdi32.dll, kernel32.dll, shlwapi.dll, and advapi32.dll. This file provides
// the implementations those DLLs need so their IATs resolve cleanly.

#include "core/api/api.h"
#include "core/api/kernel32.h"
#include "core/x86/api_dispatch.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cwchar>
#include <cctype>
#include <wctype.h>
#include <algorithm>
#include <map>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

namespace ThetaZ {

// ---------------------------------------------------------------------------
// GDI object model — minimal state tracking for Wine DLL compatibility.
// comctl32.dll and gdiplus.dll import drawing functions; they run during
// DllMain or control initialization. Most are never called by the app
// itself. We track enough state that the calls succeed without crashing.
// ---------------------------------------------------------------------------

struct GdiObject {
    enum Type { NONE, PEN, BRUSH, FONT, BITMAP, REGION, DC, PALETTE };
    Type type = NONE;
    uint32_t style = 0;
    uint32_t color = 0;
    int32_t width = 0;
};

static uint32_t g_next_gdi_handle = 0x1000;
static std::mutex g_gdi_mutex;
static std::unordered_map<uint32_t, GdiObject> g_gdi_objects;

static uint32_t alloc_gdi_handle(GdiObject::Type type) {
    std::lock_guard<std::mutex> lock(g_gdi_mutex);
    uint32_t h = g_next_gdi_handle++;
    g_gdi_objects[h] = GdiObject{type, 0, 0, 0};
    return h;
}

struct GdiDc {
    uint32_t fore_color = 0x00000000;
    uint32_t back_color = 0x00FFFFFF;
    int32_t bk_mode = 1;
    int32_t text_align = 0;
    uint32_t pen = 0;
    uint32_t brush = 0;
    uint32_t font = 0;
    int32_t rop2 = 13;
    int32_t viewport_x = 0;
    int32_t viewport_y = 0;
    int32_t window_x = 0;
    int32_t window_y = 0;
    struct { int32_t left, top, right, bottom; } clip = {0, 0, 32767, 32767};
};

static std::unordered_map<uint32_t, GdiDc> g_gdi_dcs;
static std::unordered_map<uint32_t, uint32_t> g_dc_hwnd;

// ---------------------------------------------------------------------------
// atom table for GlobalAddAtomW / GlobalDeleteAtom
// ---------------------------------------------------------------------------
static std::unordered_map<std::wstring, uint16_t> g_atom_table;
static uint16_t g_next_atom = 0xC000;

// ---------------------------------------------------------------------------
// user32: W→A charset conversion helpers (UTF-16LE → UTF-8)
// ---------------------------------------------------------------------------
static std::string w_to_a(const uint16_t* wstr) {
    if (!wstr) return "";
    std::string out;
    const uint16_t* p = wstr;
    while (*p) {
        uint32_t cp = *p++;
        if (cp >= 0xD800 && cp <= 0xDBFF && *p >= 0xDC00 && *p <= 0xDFFF) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (*p - 0xDC00);
            p++;
        }
        if (cp < 0x80) { out += (char)cp; }
        else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) {
            out += (char)(0xE0 | (cp >> 12));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        } else {
            out += (char)(0xF0 | (cp >> 18));
            out += (char)(0x80 | ((cp >> 12) & 0x3F));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        }
    }
    return out;
}

static int wstrlen(const uint16_t* s) {
    if (!s) return 0;
    int n = 0;
    while (s[n]) n++;
    return n;
}

static int wstrcmp(const uint16_t* a, const uint16_t* b) {
    if (!a || !b) return a == b ? 0 : (a ? 1 : -1);
    while (*a && *a == *b) { a++; b++; }
    return (int)*a - (int)*b;
}

static int wstrcmpi(const uint16_t* a, const uint16_t* b) {
    if (!a || !b) return a == b ? 0 : (a ? 1 : -1);
    while (*a && *a != 0) {
        int d = (int)towlower(*a) - (int)towlower(*b);
        if (d) return d;
        a++; b++;
    }
    return (int)towlower(*a) - (int)towlower(*b);
}

static int wstrncmp(const uint16_t* a, const uint16_t* b, int n) {
    if (!a || !b) return 0;
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) return (int)a[i] - (int)b[i];
        if (a[i] == 0) break;
    }
    return 0;
}

static void wstrcpy(uint16_t* dst, const uint16_t* src) {
    if (!dst || !src) return;
    while (*src) *dst++ = *src++;
    *dst = 0;
}

static void wstrncpy(uint16_t* dst, const uint16_t* src, int n) {
    if (!dst || !src || n <= 0) return;
    int i = 0;
    for (; i < n - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = 0;
}

// ---------------------------------------------------------------------------
// gdi32.dll — all functions needed by comctl32.dll and gdiplus.dll
// ---------------------------------------------------------------------------

uint32_t WINAPI_CALL THETAZ_CreateCompatibleBitmap(uint32_t hdc, int cx, int cy) {
    return alloc_gdi_handle(GdiObject::BITMAP);
}

uint32_t WINAPI_CALL THETAZ_CreateBitmap(int w, int h, uint32_t planes, uint32_t bpp, const void* bits) {
    return alloc_gdi_handle(GdiObject::BITMAP);
}

uint32_t WINAPI_CALL THETAZ_CreateDIBSection(uint32_t hdc, const void* bmi, uint32_t usage, void** bits, uint32_t section, uint32_t offset) {
    if (bits) *bits = nullptr;
    return alloc_gdi_handle(GdiObject::BITMAP);
}

uint32_t WINAPI_CALL THETAZ_CreateFontIndirectW(const void* lf) {
    return alloc_gdi_handle(GdiObject::FONT);
}

uint32_t WINAPI_CALL THETAZ_CreatePatternBrush(uint32_t hbm) {
    return alloc_gdi_handle(GdiObject::BRUSH);
}

uint32_t WINAPI_CALL THETAZ_CreatePenIndirect(const void* logpen) {
    return alloc_gdi_handle(GdiObject::PEN);
}

uint32_t WINAPI_CALL THETAZ_CreatePolygonRgn(const void* points, int count, int mode) {
    return alloc_gdi_handle(GdiObject::REGION);
}

uint32_t WINAPI_CALL THETAZ_CreateRectRgn(int l, int t, int r, int b) {
    return alloc_gdi_handle(GdiObject::REGION);
}

uint32_t WINAPI_CALL THETAZ_CreateRoundRectRgn(int l, int t, int r, int b, int w, int h) {
    return alloc_gdi_handle(GdiObject::REGION);
}

uint32_t WINAPI_CALL THETAZ_CreateRectRgnIndirect(const void* rect) {
    return alloc_gdi_handle(GdiObject::REGION);
}

uint32_t WINAPI_CALL THETAZ_CreateRoundRectRgnIndirect(const void* rect, int w, int h) {
    return alloc_gdi_handle(GdiObject::REGION);
}

int WINAPI_CALL THETAZ_CombineRgn(uint32_t hdest, uint32_t hsrc1, uint32_t hsrc2, int mode) {
    if (!hdest) return 0;
    return 1;
}

int WINAPI_CALL THETAZ_DPtoLP(uint32_t hdc, void* pt, int count) {
    return 1;
}

int WINAPI_CALL THETAZ_ExcludeClipRect(uint32_t hdc, int l, int t, int r, int b) {
    return 1;
}

uint32_t WINAPI_CALL THETAZ_ExtCreatePen(uint32_t style, uint32_t width, const void* brush, uint32_t count, const uint32_t* style_array) {
    return alloc_gdi_handle(GdiObject::PEN);
}

uint32_t WINAPI_CALL THETAZ_ExtCreateRegion(const void* xform, uint32_t count, const void* rgn_data) {
    return alloc_gdi_handle(GdiObject::REGION);
}

int WINAPI_CALL THETAZ_ExtTextOutW(uint32_t hdc, int x, int y, uint32_t options, const void* rect, const uint16_t* str, uint32_t count, const int32_t* dx) {
    (void)options; (void)rect; (void)dx;
    if (!str || !count) return 1;
    std::string utf8 = w_to_a(str);
    if (utf8.size() > count) utf8.resize(count);
    uint32_t hwnd = 0;
    {
        std::lock_guard<std::mutex> lock(g_gdi_mutex);
        auto it = g_dc_hwnd.find(hdc);
        if (it != g_dc_hwnd.end()) hwnd = it->second;
    }
    if (hwnd) theta_gdi_text(hwnd, x, y, utf8.c_str());
    else printf("%s\n", utf8.c_str());
    fflush(stdout);
    return 1;
}

int WINAPI_CALL THETAZ_FillRgn(uint32_t hdc, uint32_t hrgn, uint32_t hbr) {
    return 1;
}

int WINAPI_CALL THETAZ_FrameRgn(uint32_t hdc, uint32_t hrgn, uint32_t hbr, int w, int h) {
    return 1;
}

int WINAPI_CALL THETAZ_GdiAlphaBlend(uint32_t hdcDst, int xdst, int ydst, int wdst, int hdst, uint32_t hdcSrc, int xsrc, int ysrc, int wsrc, int hsrc, uint32_t blend) {
    return 1;
}

uint32_t WINAPI_CALL THETAZ_GetBkColor(uint32_t hdc) {
    auto it = g_gdi_dcs.find(hdc);
    return it != g_gdi_dcs.end() ? it->second.back_color : 0x00FFFFFF;
}

int WINAPI_CALL THETAZ_GetBkMode(uint32_t hdc) {
    auto it = g_gdi_dcs.find(hdc);
    return it != g_gdi_dcs.end() ? it->second.bk_mode : 1;
}

int WINAPI_CALL THETAZ_GetClipBox(uint32_t hdc, void* rect) {
    if (rect) memset(rect, 0, 16);
    return 1;
}

int WINAPI_CALL THETAZ_GetClipRgn(uint32_t hdc, uint32_t hrgn) {
    return 0;
}

uint32_t WINAPI_CALL THETAZ_GetCurrentObject(uint32_t hdc, uint32_t type) {
    auto it = g_gdi_dcs.find(hdc);
    if (it == g_gdi_dcs.end()) return 0;
    switch (type) {
        case 1: return it->second.pen;
        case 2: return it->second.brush;
        case 6: return it->second.font;
        default: return 0;
    }
}

uint32_t WINAPI_CALL THETAZ_GetDIBColorTable(uint32_t hdc, uint32_t start, uint32_t count, void* entries) {
    return 0;
}

int WINAPI_CALL THETAZ_GetDIBits(uint32_t hdc, uint32_t hbm, uint32_t start, uint32_t cLines, void* lpvBits, void* lpbmi, uint32_t usage) {
    return 0;
}

uint32_t WINAPI_CALL THETAZ_GetLayout(uint32_t hdc) {
    return 0;
}

uint32_t WINAPI_CALL THETAZ_GetNearestColor(uint32_t hdc, uint32_t color) {
    return color;
}

int WINAPI_CALL THETAZ_GetObjectW(uint32_t hobj, int count, void* buf) {
    if (!buf) return 0;
    memset(buf, 0, count);
    auto it = g_gdi_objects.find(hobj);
    if (it == g_gdi_objects.end()) return 0;
    return count;
}

uint32_t WINAPI_CALL THETAZ_GetPixel(uint32_t hdc, int x, int y) {
    return 0x00FFFFFF;
}

uint32_t WINAPI_CALL THETAZ_GetTextColor(uint32_t hdc) {
    auto it = g_gdi_dcs.find(hdc);
    return it != g_gdi_dcs.end() ? it->second.fore_color : 0x00000000;
}

int WINAPI_CALL THETAZ_GetTextExtentPoint32W(uint32_t hdc, const uint16_t* str, int len, void* size) {
    if (size) {
        int32_t* s = (int32_t*)size;
        s[0] = len * 8;
        s[1] = 16;
    }
    return 1;
}

int WINAPI_CALL THETAZ_GetTextExtentPoint32A(uint32_t hdc, const char* str, int len, void* size) {
    if (size) {
        int32_t* s = (int32_t*)size;
        s[0] = len * 8;
        s[1] = 16;
    }
    return 1;
}

int WINAPI_CALL THETAZ_GetTextExtentPointW(uint32_t hdc, const uint16_t* str, int len, void* size) {
    return THETAZ_GetTextExtentPoint32W(hdc, str, len, size);
}

int WINAPI_CALL THETAZ_GetTextExtentPointA(uint32_t hdc, const char* str, int len, void* size) {
    return THETAZ_GetTextExtentPoint32A(hdc, str, len, size);
}

int WINAPI_CALL THETAZ_GetTextMetricsW(uint32_t hdc, void* tm) {
    if (tm) memset(tm, 0, 60);
    return 1;
}

int WINAPI_CALL THETAZ_IntersectClipRect(uint32_t hdc, int l, int t, int r, int b) {
    return 1;
}

int WINAPI_CALL THETAZ_LineTo(uint32_t hdc, int x, int y) {
    return 1;
}

int WINAPI_CALL THETAZ_MoveToEx(uint32_t hdc, int x, int y, void* pt) {
    return 1;
}

int WINAPI_CALL THETAZ_OffsetWindowOrgEx(uint32_t hdc, int x, int y, void* pt) {
    return 1;
}

int WINAPI_CALL THETAZ_PatBlt(uint32_t hdc, int x, int y, int w, int h, uint32_t rop) {
    return 1;
}

int WINAPI_CALL THETAZ_PolyPolyline(uint32_t hdc, const void* pt, const uint32_t* counts, uint32_t polys) {
    return 1;
}

int WINAPI_CALL THETAZ_Polygon(uint32_t hdc, const void* pt, int count) {
    return 1;
}

int WINAPI_CALL THETAZ_RectVisible(uint32_t hdc, const void* rect) {
    return 0;
}

int WINAPI_CALL THETAZ_Rectangle(uint32_t hdc, int l, int t, int r, int b) {
    return 1;
}

int WINAPI_CALL THETAZ_SelectClipRgn(uint32_t hdc, uint32_t hrgn) {
    return 1;
}

uint32_t WINAPI_CALL THETAZ_SelectPalette(uint32_t hdc, uint32_t hpal, int force) {
    return 0;
}

uint32_t WINAPI_CALL THETAZ_SetDIBColorTable(uint32_t hdc, uint32_t start, uint32_t count, const void* entries) {
    return 0;
}

int WINAPI_CALL THETAZ_SetDIBits(uint32_t hdc, uint32_t hbm, uint32_t start, uint32_t cLines, const void* bits, const void* bmi, uint32_t usage) {
    return 0;
}

uint32_t WINAPI_CALL THETAZ_SetPixel(uint32_t hdc, int x, int y, uint32_t color) {
    return 0xFFFFFFFF;
}

int WINAPI_CALL THETAZ_SetPolyFillMode(uint32_t hdc, int mode) {
    return 0;
}

int WINAPI_CALL THETAZ_SetTextAlign(uint32_t hdc, uint32_t align) {
    return 0;
}

int WINAPI_CALL THETAZ_SetViewportOrgEx(uint32_t hdc, int x, int y, void* pt) {
    return 1;
}

int WINAPI_CALL THETAZ_SetWindowOrgEx(uint32_t hdc, int x, int y, void* pt) {
    return 1;
}

int WINAPI_CALL THETAZ_StretchBlt(uint32_t hdcDst, int dstx, int dsty, int dstw, int dsth, uint32_t hdcSrc, int srcx, int srcy, int srcw, int srch, uint32_t rop) {
    return 1;
}

int WINAPI_CALL THETAZ_StretchDIBits(uint32_t hdc, int dstx, int dsty, int dstw, int dsth, int srcx, int srcy, int srcw, int srch, const void* bits, const void* bmi, uint32_t usage, uint32_t rop) {
    return 1;
}

// ---------------------------------------------------------------------------
// user32.dll — W-suffix functions + missing A-suffix functions
// ---------------------------------------------------------------------------

int WINAPI_CALL THETAZ_AdjustWindowRect(void* rect, uint32_t style, int menu) { return 1; }
int WINAPI_CALL THETAZ_AdjustWindowRectEx(void* rect, uint32_t style, int menu, uint32_t exstyle) { return 1; }

int WINAPI_CALL THETAZ_AppendMenuW(uint32_t hmenu, uint32_t flags, uint32_t id, const uint16_t* str) { return 1; }

uint32_t WINAPI_CALL THETAZ_BeginDeferWindowPos(int n) { return 0; }

LRESULT WINAPI_CALL THETAZ_CallWindowProcA(void* proc, uint32_t hwnd, uint32_t msg, WPARAM wParam, LPARAM lParam) {
    if (!proc) return 0;
    return ((LRESULT(WINAPI_CALL*)(uint32_t, uint32_t, WPARAM, LPARAM))proc)(hwnd, msg, wParam, lParam);
}

LRESULT WINAPI_CALL THETAZ_CallWindowProcW(void* proc, uint32_t hwnd, uint32_t msg, WPARAM wParam, LPARAM lParam) {
    return THETAZ_CallWindowProcA(proc, hwnd, msg, wParam, lParam);
}

int WINAPI_CALL THETAZ_CheckMenuItem(uint32_t hmenu, uint32_t id, uint32_t flags) { return 0; }

uint32_t WINAPI_CALL THETAZ_ChildWindowFromPoint(uint32_t parent, int x, int y) { return 0; }

int WINAPI_CALL THETAZ_ClientToScreen(uint32_t hwnd, void* pt) { return 1; }

uint32_t WINAPI_CALL THETAZ_CopyIcon(uint32_t hicon) { return hicon; }

uint32_t WINAPI_CALL THETAZ_CopyImage(uint32_t h, uint32_t type, int cx, int cy, uint32_t flags) { return h; }

int WINAPI_CALL THETAZ_CopyRect(void* dst, const void* src) {
    if (dst && src) memcpy(dst, src, 16);
    return 1;
}

uint32_t WINAPI_CALL THETAZ_CreateCaret(uint32_t hwnd, uint32_t hbm, int w, int h) { return 1; }

uint32_t WINAPI_CALL THETAZ_CreateDialogIndirectParamA(uint32_t hinst, const void* dlg, uint32_t parent, void* proc, LPARAM init) { return 0; }
uint32_t WINAPI_CALL THETAZ_CreateDialogIndirectParamW(uint32_t hinst, const void* dlg, uint32_t parent, void* proc, LPARAM init) { return 0; }

uint32_t WINAPI_CALL THETAZ_CreateIconIndirect(void* icon) { return alloc_gdi_handle(GdiObject::NONE); }

uint32_t WINAPI_CALL THETAZ_CreatePopupMenu() { return 0; }

uint32_t WINAPI_CALL THETAZ_CreateWindowExW(uint32_t exstyle, const uint16_t* cls, const uint16_t* title, uint32_t style, int x, int y, int w, int h, uint32_t parent, uint32_t menu, uint32_t hinst, void* param) {
    using CreateWindowExA_fn = uint32_t(WINAPI_CALL*)(uint32_t, const char*, const char*, uint32_t, int, int, int, int, uint32_t, uint32_t, uint32_t, void*);
    auto* fn = (CreateWindowExA_fn)ApiRegistry::instance().find_function("user32.dll", "CreateWindowExA");
    if (!fn) return 0;
    std::string cls_a = cls ? w_to_a(cls) : "";
    std::string title_a = title ? w_to_a(title) : "";
    return fn(exstyle, cls_a.c_str(), title_a.c_str(), style, x, y, w, h, parent, menu, hinst, param);
}

LRESULT WINAPI_CALL THETAZ_DefWindowProcW(uint32_t hwnd, uint32_t msg, WPARAM wParam, LPARAM lParam) {
    using DefWindowProcA_fn = LRESULT(WINAPI_CALL*)(uint32_t, uint32_t, WPARAM, LPARAM);
    auto* fn = (DefWindowProcA_fn)ApiRegistry::instance().find_function("user32.dll", "DefWindowProcA");
    if (!fn) return 0;
    return fn(hwnd, msg, wParam, lParam);
}

uint32_t WINAPI_CALL THETAZ_DeferWindowPos(uint32_t hwinpos, uint32_t hwnd, uint32_t insertafter, int x, int y, int cx, int cy, uint32_t flags) { return hwinpos; }

int WINAPI_CALL THETAZ_DestroyCaret() { return 1; }
uint32_t WINAPI_CALL THETAZ_DestroyCursor(uint32_t hcur) { return 1; }
int WINAPI_CALL THETAZ_DestroyIcon(uint32_t hicon) { return 1; }

int WINAPI_CALL THETAZ_DialogBoxParamA(uint32_t hinst, const char* dlg, uint32_t parent, void* proc, LPARAM init) { return -1; }
int WINAPI_CALL THETAZ_DialogBoxParamW(uint32_t hinst, const uint16_t* dlg, uint32_t parent, void* proc, LPARAM init) { return -1; }

int WINAPI_CALL THETAZ_DispatchMessageW(const void* msg) {
    if (!msg) return 0;
    uint32_t hwnd = *(const uint32_t*)((const uint8_t*)msg);
    if (!hwnd) return 0;
    return 0;
}

int WINAPI_CALL THETAZ_DrawEdge(uint32_t hdc, void* rect, uint32_t edge, uint32_t flags) { return 1; }
int WINAPI_CALL THETAZ_DrawFocusRect(uint32_t hdc, const void* rect) { return 1; }
int WINAPI_CALL THETAZ_DrawFrameControl(uint32_t hdc, void* rect, uint32_t type, uint32_t state) { return 1; }
int WINAPI_CALL THETAZ_DrawIcon(uint32_t hdc, int x, int y, uint32_t hicon) { return 1; }
int WINAPI_CALL THETAZ_DrawIconEx(uint32_t hdc, int x, int y, uint32_t hicon, int w, int h, uint32_t step, uint32_t hbr, uint32_t flags) { return 1; }

int WINAPI_CALL THETAZ_DrawTextA(uint32_t hdc, const char* str, int len, void* rect, uint32_t format) {
    if (len == -1) len = str ? (int)strlen(str) : 0;
    return len;
}

int WINAPI_CALL THETAZ_DrawTextW(uint32_t hdc, const uint16_t* str, int len, void* rect, uint32_t format) {
    if (len == -1) len = str ? wstrlen(str) : 0;
    return len;
}

int WINAPI_CALL THETAZ_EnableScrollBar(uint32_t hwnd, uint32_t bar, uint32_t flags) { return 1; }
uint32_t WINAPI_CALL THETAZ_EndDeferWindowPos(uint32_t hwinpos) { return hwinpos; }

int WINAPI_CALL THETAZ_FillRect(uint32_t hdc, const void* rect, uint32_t hbr) { return 1; }
int WINAPI_CALL THETAZ_FrameRect(uint32_t hdc, const void* rect, uint32_t hbr) { return 1; }

uint32_t WINAPI_CALL THETAZ_GetActiveWindow() { return 0; }

int16_t WINAPI_CALL THETAZ_GetAsyncKeyState(int key) { return 0; }

uint32_t WINAPI_CALL THETAZ_GetCapture() { return 0; }

uint32_t WINAPI_CALL THETAZ_GetClassLongW(uint32_t hwnd, int index) { return 0; }

int WINAPI_CALL THETAZ_GetClassNameW(uint32_t hwnd, uint16_t* buf, int max) {
    if (buf && max > 0) buf[0] = 0;
    return 0;
}

int WINAPI_CALL THETAZ_GetCursorPos(void* pt) {
    if (pt) memset(pt, 0, 8);
    return 1;
}

uint32_t WINAPI_CALL THETAZ_GetDCEx(uint32_t hwnd, uint32_t hrgn, uint32_t flags) {
    uint32_t hdc = alloc_gdi_handle(GdiObject::DC);
    g_gdi_dcs[hdc] = GdiDc{};
    return hdc;
}

void* WINAPI_CALL THETAZ_GetDC(void* hwnd) {
    uint32_t hdc = alloc_gdi_handle(GdiObject::DC);
    g_gdi_dcs[hdc] = GdiDc{};
    if (hwnd) {
        std::lock_guard<std::mutex> lock(g_gdi_mutex);
        g_dc_hwnd[hdc] = (uint32_t)(uintptr_t)hwnd;
    }
    return (void*)(uintptr_t)hdc;
}

int WINAPI_CALL THETAZ_ReleaseDC(void* hwnd, void* hdc) {
    (void)hwnd;
    uint32_t h = (uint32_t)(uintptr_t)hdc;
    std::lock_guard<std::mutex> lock(g_gdi_mutex);
    g_gdi_dcs.erase(h);
    g_dc_hwnd.erase(h);
    return 1;
}

BOOL WINAPI_CALL THETAZ_GetClientRect(void* hwnd, void* rect) {
    (void)hwnd;
    if (!rect) return FALSE;
    int32_t* r = (int32_t*)rect;
    r[0] = 0; r[1] = 0; r[2] = 800; r[3] = 600;
    return TRUE;
}

void* WINAPI_CALL THETAZ_SelectObject(void* hdc, void* obj) {
    uint32_t dc = (uint32_t)(uintptr_t)hdc;
    uint32_t o = (uint32_t)(uintptr_t)obj;
    std::lock_guard<std::mutex> lock(g_gdi_mutex);
    auto dcit = g_gdi_dcs.find(dc);
    if (dcit == g_gdi_dcs.end()) return nullptr;
    auto oit = g_gdi_objects.find(o);
    uint32_t prev = 0;
    if (oit == g_gdi_objects.end()) {
        if (o >= 0x8000 && o <= 0x8020) return (void*)(uintptr_t)o;
        return nullptr;
    }
    switch (oit->second.type) {
    case GdiObject::PEN: prev = dcit->second.pen; dcit->second.pen = o; break;
    case GdiObject::BRUSH: prev = dcit->second.brush; dcit->second.brush = o; break;
    case GdiObject::FONT: prev = dcit->second.font; dcit->second.font = o; break;
    default: return nullptr;
    }
    return (void*)(uintptr_t)(prev ? prev : o);
}

int WINAPI_CALL THETAZ_GetDeviceCaps(void* hdc, int index) {
    (void)hdc;
    switch (index) {
    case 4: return 1920;
    case 8: return 1080;
    case 10: return 32;
    case 12:
    case 90: return 96;
    default: return 0;
    }
}

DWORD WINAPI_CALL THETAZ_SetTextColor(void* hdc, DWORD color) {
    uint32_t dc = (uint32_t)(uintptr_t)hdc;
    std::lock_guard<std::mutex> lock(g_gdi_mutex);
    auto it = g_gdi_dcs.find(dc);
    if (it == g_gdi_dcs.end()) return 0xFFFFFFFFu;
    DWORD prev = it->second.fore_color;
    it->second.fore_color = color;
    return prev;
}

DWORD WINAPI_CALL THETAZ_SetBkColor(void* hdc, DWORD color) {
    uint32_t dc = (uint32_t)(uintptr_t)hdc;
    std::lock_guard<std::mutex> lock(g_gdi_mutex);
    auto it = g_gdi_dcs.find(dc);
    if (it == g_gdi_dcs.end()) return 0xFFFFFFFFu;
    DWORD prev = it->second.back_color;
    it->second.back_color = color;
    return prev;
}

int WINAPI_CALL THETAZ_SetBkMode(void* hdc, int mode) {
    uint32_t dc = (uint32_t)(uintptr_t)hdc;
    std::lock_guard<std::mutex> lock(g_gdi_mutex);
    auto it = g_gdi_dcs.find(dc);
    if (it == g_gdi_dcs.end()) return 0;
    int prev = it->second.bk_mode;
    it->second.bk_mode = mode;
    return prev;
}

BOOL WINAPI_CALL THETAZ_TextOutW(void* hdc, int x, int y, const uint16_t* s, int c) {
    if (!s || c <= 0) return FALSE;
    std::string utf8 = w_to_a(s);
    if ((int)utf8.size() > c) utf8.resize(c);
    uint32_t dc = (uint32_t)(uintptr_t)hdc;
    uint32_t hwnd = 0;
    {
        std::lock_guard<std::mutex> lock(g_gdi_mutex);
        auto it = g_dc_hwnd.find(dc);
        if (it != g_dc_hwnd.end()) hwnd = it->second;
    }
    if (hwnd) theta_gdi_text(hwnd, x, y, utf8.c_str());
    else printf("%s\n", utf8.c_str());
    fflush(stdout);
    return TRUE;
}

void* WINAPI_CALL THETAZ_GetStockObject(int index) {
    if (index < 0 || index > 31) return nullptr;
    return (void*)(uintptr_t)(0x8000u + (uint32_t)index);
}

void* WINAPI_CALL THETAZ_CreateSolidBrush(DWORD color) {
    uint32_t h = alloc_gdi_handle(GdiObject::BRUSH);
    std::lock_guard<std::mutex> lock(g_gdi_mutex);
    g_gdi_objects[h].color = color;
    return (void*)(uintptr_t)h;
}

uint32_t WINAPI_CALL THETAZ_GetDoubleClickTime() { return 500; }

uint32_t WINAPI_CALL THETAZ_GetDpiForWindow(uint32_t hwnd) { return 96; }

int WINAPI_CALL THETAZ_GetIconInfo(uint32_t hicon, void* info) {
    if (info) memset(info, 0, 20);
    return 1;
}

int WINAPI_CALL THETAZ_GetKeyNameTextW(int32_t lParam, uint16_t* buf, int max) {
    if (buf && max > 0) buf[0] = 0;
    return 0;
}

int16_t WINAPI_CALL THETAZ_GetKeyState(int key) { return 0; }

uint32_t WINAPI_CALL THETAZ_GetMenuState(uint32_t hmenu, uint32_t id, uint32_t flags) { return 0xFFFFFFFF; }

uint32_t WINAPI_CALL THETAZ_GetMessagePos() { return 0; }
uint32_t WINAPI_CALL THETAZ_GetMessageTime() { return 0; }

int WINAPI_CALL THETAZ_GetMessageW(void* msg, uint32_t hwnd, uint32_t min, uint32_t max) {
    if (msg) memset(msg, 0, 28);
    return 0;
}

int WINAPI_CALL THETAZ_GetMonitorInfoW(uint32_t hmon, void* info) { return 0; }

uint32_t WINAPI_CALL THETAZ_GetNextDlgTabItem(uint32_t hwnd, uint32_t ctrl, int prev) { return 0; }

uint32_t WINAPI_CALL THETAZ_GetParent(uint32_t hwnd) { return 0; }

void* WINAPI_CALL THETAZ_GetPropW(uint32_t hwnd, const uint16_t* str) { return nullptr; }

int WINAPI_CALL THETAZ_GetScrollInfo(uint32_t hwnd, int bar, void* si) { return 0; }
int WINAPI_CALL THETAZ_GetScrollPos(uint32_t hwnd, int bar) { return 0; }
int WINAPI_CALL THETAZ_GetScrollRange(uint32_t hwnd, int bar, int* min, int* max) {
    if (min) *min = 0;
    if (max) *max = 0;
    return 1;
}

uint32_t WINAPI_CALL THETAZ_GetSysColor(int index) { return 0x00FFFFFF; }
uint32_t WINAPI_CALL THETAZ_GetSysColorBrush(int index) {
    return alloc_gdi_handle(GdiObject::BRUSH);
}

uint32_t WINAPI_CALL THETAZ_GetTopWindow(uint32_t hwnd) { return 0; }

uint32_t WINAPI_CALL THETAZ_GetWindow(uint32_t hwnd, uint32_t cmd) { return 0; }

uint32_t WINAPI_CALL THETAZ_GetWindowLongW(uint32_t hwnd, int index) {
    return 0;
}

uint32_t WINAPI_CALL THETAZ_GetWindowLongPtrW(uint32_t hwnd, int index) {
    return 0;
}

int WINAPI_CALL THETAZ_GetWindowRect(uint32_t hwnd, void* rect) {
    if (rect) memset(rect, 0, 16);
    return 1;
}

int WINAPI_CALL THETAZ_GetWindowRgn(uint32_t hwnd, uint32_t hrgn) { return 0; }

int WINAPI_CALL THETAZ_GetWindowTextLengthW(uint32_t hwnd) { return 0; }

int WINAPI_CALL THETAZ_GetWindowTextW(uint32_t hwnd, uint16_t* buf, int max) {
    if (buf && max > 0) buf[0] = 0;
    return 0;
}

int WINAPI_CALL THETAZ_IntersectRect(void* dst, const void* r1, const void* r2) {
    if (dst) memset(dst, 0, 16);
    return 0;
}

int WINAPI_CALL THETAZ_InvalidateRgn(uint32_t hwnd, uint32_t hrgn, int erase) { return 1; }

int WINAPI_CALL THETAZ_IsChild(uint32_t parent, uint32_t child) { return 0; }

int WINAPI_CALL THETAZ_IsDialogMessageW(uint32_t hwnd, void* msg) { return 0; }

int WINAPI_CALL THETAZ_IsWindowUnicode(uint32_t hwnd) { return 1; }

uint32_t WINAPI_CALL THETAZ_LoadBitmapW(uint32_t hinst, const uint16_t* name) { return 0; }

uint32_t WINAPI_CALL THETAZ_LoadCursorW(uint32_t hinst, const uint16_t* name) { return 0; }

uint32_t WINAPI_CALL THETAZ_LoadIconW(uint32_t hinst, const uint16_t* name) { return 0; }

uint32_t WINAPI_CALL THETAZ_LoadImageA(uint32_t hinst, const char* name, uint32_t type, int cx, int cy, uint32_t flags) { return 0; }

uint32_t WINAPI_CALL THETAZ_LoadImageW(uint32_t hinst, const uint16_t* name, uint32_t type, int cx, int cy, uint32_t flags) { return 0; }

int WINAPI_CALL THETAZ_LoadStringW(uint32_t hinst, uint32_t id, uint16_t* buf, int max) {
    if (buf && max > 0) buf[0] = 0;
    return 0;
}

int WINAPI_CALL THETAZ_MapDialogRect(uint32_t hwnd, void* rect) { return 1; }

uint32_t WINAPI_CALL THETAZ_MapVirtualKeyW(uint32_t code, uint32_t maptype) { return 0; }

int WINAPI_CALL THETAZ_MapWindowPoints(uint32_t from, uint32_t to, void* pts, int count) { return 1; }

uint32_t WINAPI_CALL THETAZ_MonitorFromRect(void* rect, uint32_t flags) { return 0; }

int WINAPI_CALL THETAZ_MoveWindow(uint32_t hwnd, int x, int y, int w, int h, int repaint) { return 1; }

void WINAPI_CALL THETAZ_NotifyWinEvent(uint32_t event, uint32_t hwnd, int idObject, int idChild) {}

int WINAPI_CALL THETAZ_PeekMessageW(void* msg, uint32_t hwnd, uint32_t min, uint32_t max, uint32_t remove) {
    if (msg) memset(msg, 0, 28);
    return 0;
}

int WINAPI_CALL THETAZ_PostMessageW(uint32_t hwnd, uint32_t msg, WPARAM wParam, LPARAM lParam) { return 1; }

int WINAPI_CALL THETAZ_RedrawWindow(uint32_t hwnd, const void* lprcUpdate, uint32_t hrgnUpdate, uint32_t flags) { return 1; }

uint32_t WINAPI_CALL THETAZ_RegisterClassW(const void* wc) { return 0x0001; }

uint32_t WINAPI_CALL THETAZ_RegisterWindowMessageW(const uint16_t* str) { return 0xC000; }

int WINAPI_CALL THETAZ_ReleaseCapture() { return 1; }

void* WINAPI_CALL THETAZ_RemovePropW(uint32_t hwnd, const uint16_t* str) { return nullptr; }

int WINAPI_CALL THETAZ_ScreenToClient(uint32_t hwnd, void* pt) { return 1; }

int WINAPI_CALL THETAZ_ScrollWindow(uint32_t hwnd, int dx, int dy, const void* rect, const void* clip) { return 1; }
int WINAPI_CALL THETAZ_ScrollWindowEx(uint32_t hwnd, int dx, int dy, const void* rect, const void* clip, uint32_t hrgnUpdate, void* update, uint32_t flags) { return 1; }

int WINAPI_CALL THETAZ_SendDlgItemMessageW(uint32_t hwnd, int id, uint32_t msg, WPARAM wParam, LPARAM lParam) { return 0; }

LRESULT WINAPI_CALL THETAZ_SendMessageW(uint32_t hwnd, uint32_t msg, WPARAM wParam, LPARAM lParam) {
    using SendMessageA_fn = LRESULT(WINAPI_CALL*)(uint32_t, uint32_t, WPARAM, LPARAM);
    auto* fn = (SendMessageA_fn)ApiRegistry::instance().find_function("user32.dll", "SendMessageA");
    if (!fn) return 0;
    return fn(hwnd, msg, wParam, lParam);
}

int WINAPI_CALL THETAZ_SetCapture(uint32_t hwnd) { return 1; }

int WINAPI_CALL THETAZ_SetCaretPos(int x, int y) { return 1; }

uint32_t WINAPI_CALL THETAZ_SetCursor(uint32_t hcur) { return 0; }

uint32_t WINAPI_CALL THETAZ_SetParent(uint32_t child, uint32_t parent) { return 0; }

void* WINAPI_CALL THETAZ_SetPropW(uint32_t hwnd, const uint16_t* str, void* data) { return data; }

int WINAPI_CALL THETAZ_SetScrollInfo(uint32_t hwnd, int bar, const void* si, int redraw) { return 0; }
int WINAPI_CALL THETAZ_SetScrollPos(uint32_t hwnd, int bar, int pos, int redraw) { return pos; }
int WINAPI_CALL THETAZ_SetScrollRange(uint32_t hwnd, int bar, int min, int max, int redraw) { return 1; }

int WINAPI_CALL THETAZ_SetWindowLongA(uint32_t hwnd, int index, int value) { return 0; }

int WINAPI_CALL THETAZ_SetWindowLongW(uint32_t hwnd, int index, int value) {
    return THETAZ_SetWindowLongA(hwnd, index, value);
}

int64_t WINAPI_CALL THETAZ_SetWindowLongPtrA(uint32_t hwnd, int index, int64_t value) { return 0; }

int64_t WINAPI_CALL THETAZ_SetWindowLongPtrW(uint32_t hwnd, int index, int64_t value) {
    return THETAZ_SetWindowLongPtrA(hwnd, index, value);
}

int WINAPI_CALL THETAZ_SetWindowPos(uint32_t hwnd, uint32_t insertafter, int x, int y, int cx, int cy, uint32_t flags) { return 1; }

int WINAPI_CALL THETAZ_SetWindowRgn(uint32_t hwnd, uint32_t hrgn, int redraw) { return 1; }

int WINAPI_CALL THETAZ_SetWindowTextW(uint32_t hwnd, const uint16_t* str) { return 1; }

int WINAPI_CALL THETAZ_ShowCaret(uint32_t hwnd) { return 1; }
int WINAPI_CALL THETAZ_ShowScrollBar(uint32_t hwnd, int bar, int show) { return 1; }

int WINAPI_CALL THETAZ_SubtractRect(void* dst, const void* r1, const void* r2) {
    if (dst) memset(dst, 0, 16);
    return 0;
}

int WINAPI_CALL THETAZ_TrackMouseEvent(void* tme) { return 1; }
int WINAPI_CALL THETAZ_TrackPopupMenu(uint32_t hmenu, uint32_t flags, int x, int y, int reserved, uint32_t hwnd, const void* rect) { return 1; }

int WINAPI_CALL THETAZ_UnionRect(void* dst, const void* r1, const void* r2) {
    if (dst) memset(dst, 0, 16);
    return 1;
}

int WINAPI_CALL THETAZ_UnregisterClassW(const uint16_t* cls, uint32_t hinst) { return 1; }
int WINAPI_CALL THETAZ_ValidateRect(uint32_t hwnd, const void* rect) { return 1; }
uint32_t WINAPI_CALL THETAZ_WindowFromPoint(int64_t pt) { return 0; }

int WINAPI_CALL THETAZ_wsprintfW(uint16_t* buf, const uint16_t* fmt, ...) {
    if (buf) buf[0] = 0;
    return 0;
}

// ---------------------------------------------------------------------------
// kernel32.dll — functions needed by comctl32, shlwapi, credui
// ---------------------------------------------------------------------------

int WINAPI_CALL THETAZ_CompareFileTime(const FILETIME* ft1, const FILETIME* ft2) {
    if (!ft1 || !ft2) return -1;
    if (ft2->dwHighDateTime > ft1->dwHighDateTime) return -1;
    if (ft2->dwHighDateTime < ft1->dwHighDateTime) return 1;
    if (ft2->dwLowDateTime > ft1->dwLowDateTime) return -1;
    if (ft2->dwLowDateTime < ft1->dwLowDateTime) return 1;
    return 0;
}

int WINAPI_CALL THETAZ_CompareStringA(uint32_t lcid, uint32_t flags, const char* s1, int l1, const char* s2, int l2) {
    if (!s1 || !s2) return 2;
    if (l1 == -1) l1 = (int)strlen(s1);
    if (l2 == -1) l2 = (int)strlen(s2);
    int r = strncmp(s1, s2, std::min(l1, l2));
    if (r != 0) return r < 0 ? 1 : 3;
    if (l1 < l2) return 1;
    if (l1 > l2) return 3;
    return 2;
}

int WINAPI_CALL THETAZ_CompareStringOrdinal(const uint16_t* s1, int l1, const uint16_t* s2, int l2, int ignore) {
    if (!s1 || !s2) return 0;
    if (l1 == -1) l1 = wstrlen(s1);
    if (l2 == -2) l2 = wstrlen(s2);
    int r = wstrncmp(s1, s2, std::min(l1, l2));
    if (r != 0) return r < 0 ? 1 : 3;
    if (l1 < l2) return 1;
    if (l1 > l2) return 3;
    return 2;
}

BOOL WINAPI_CALL THETAZ_IsBadCodePtr(const void* ptr) {
    return ptr ? 0 : 1;
}

BOOL WINAPI_CALL THETAZ_IsBadStringPtrA(const char* ptr, uint32_t len) {
    return ptr ? 0 : 1;
}

BOOL WINAPI_CALL THETAZ_IsBadWritePtr(void* ptr, uint32_t len) {
    return ptr ? 0 : 1;
}

uint32_t WINAPI_CALL THETAZ_GlobalAddAtomW(const uint16_t* name) {
    if (!name) return 0;
    std::wstring ws(name, name + wstrlen(name));
    std::lock_guard<std::mutex> lock(g_gdi_mutex);
    auto it = g_atom_table.find(ws);
    if (it != g_atom_table.end()) return it->second;
    uint16_t atom = g_next_atom++;
    g_atom_table[ws] = atom;
    return atom;
}

int WINAPI_CALL THETAZ_GlobalDeleteAtom(uint16_t atom) {
    std::lock_guard<std::mutex> lock(g_gdi_mutex);
    for (auto it = g_atom_table.begin(); it != g_atom_table.end(); ++it) {
        if (it->second == atom) { g_atom_table.erase(it); break; }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// shlwapi.dll — functions needed by comctl32.dll
// ---------------------------------------------------------------------------

int WINAPI_CALL THETAZ_StrCmpIW(const uint16_t* s1, const uint16_t* s2) {
    return wstrcmpi(s1, s2);
}

int WINAPI_CALL THETAZ_StrCmpNIW(const uint16_t* s1, const uint16_t* s2, int n) {
    for (int i = 0; i < n; i++) {
        int d = (int)towlower(s1 ? s1[i] : 0) - (int)towlower(s2 ? s2[i] : 0);
        if (d) return d;
        if (!s1[i]) break;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// advapi32.dll — RegOpenKeyA (comctl32.dll import)
// ---------------------------------------------------------------------------

static uint32_t g_fake_hkey = 0x80000001;

uint32_t WINAPI_CALL THETAZ_RegOpenKeyA(uint32_t hkey, const char* subkey, void* result) {
    if (result) *(uint32_t*)result = g_fake_hkey++;
    return 0;
}

// ---------------------------------------------------------------------------
// CRT functions — needed by comctl32.dll (links against ucrtbase/msvcrt)
// ---------------------------------------------------------------------------

double WINAPI_CALL THETAZ_floor(double x) { return floor(x); }

int WINAPI_CALL THETAZ_isprint(int c) { return isprint(c); }

int WINAPI_CALL THETAZ_iswalnum(int c) { return iswalnum(c); }

void WINAPI_CALL THETAZ_qsort(void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*)) {
    if (base && nmemb > 0 && size > 0 && compar) {
        ::qsort(base, nmemb, size, compar);
    }
}

const uint16_t* WINAPI_CALL THETAZ_wcschr(const uint16_t* s, uint16_t c) {
    if (!s) return nullptr;
    while (*s) {
        if (*s == c) return s;
        s++;
    }
    return c == 0 ? s : nullptr;
}

int WINAPI_CALL THETAZ_wcsncmp(const uint16_t* a, const uint16_t* b, size_t n) {
    return wstrncmp(a, b, (int)n);
}

long WINAPI_CALL THETAZ_wcstol(const uint16_t* str, uint16_t** end, int base) {
    if (!str) { if (end) *end = (uint16_t*)str; return 0; }
    std::string s = w_to_a(str);
    char* e = nullptr;
    long v = strtol(s.c_str(), &e, base);
    if (end) {
        if (e) {
            int offset = (int)(e - s.c_str());
            *end = (uint16_t*)str + offset;
        } else {
            *end = (uint16_t*)str;
        }
    }
    return v;
}

// ---------------------------------------------------------------------------
// credui.dll — credential functions (imported but never called)
// ---------------------------------------------------------------------------

int WINAPI_CALL THETAZ_CredEnumerateW(const uint16_t* filter, uint32_t flags, uint32_t* count, void** credentials) {
    if (count) *count = 0;
    if (credentials) *credentials = nullptr;
    return 0;
}

int WINAPI_CALL THETAZ_CredFree(void* buffer) { return 1; }

int WINAPI_CALL THETAZ_CredWriteW(const void* cred, uint32_t flags) { return 0; }

// ---------------------------------------------------------------------------
// Other functions
// ---------------------------------------------------------------------------

uint32_t WINAPI_CALL THETAZ_DelayLoadFailureHook() { return 0; }

uint32_t WINAPI_CALL THETAZ_FindResourceA(uint32_t hmod, const char* name, const char* type) { return 0; }

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void initialize_wine_compat_layer() {
    // --- gdi32.dll ---
    register_api("gdi32.dll", "CreateCompatibleBitmap", (void*)THETAZ_CreateCompatibleBitmap);
    register_api("gdi32.dll", "CreateBitmap", (void*)THETAZ_CreateBitmap);
    register_api("gdi32.dll", "CreateDIBSection", (void*)THETAZ_CreateDIBSection);
    register_api("gdi32.dll", "CreateFontIndirectW", (void*)THETAZ_CreateFontIndirectW);
    register_api("gdi32.dll", "CreatePatternBrush", (void*)THETAZ_CreatePatternBrush);
    register_api("gdi32.dll", "CreatePenIndirect", (void*)THETAZ_CreatePenIndirect);
    register_api("gdi32.dll", "CreatePolygonRgn", (void*)THETAZ_CreatePolygonRgn);
    register_api("gdi32.dll", "CreateRectRgn", (void*)THETAZ_CreateRectRgn);
    register_api("gdi32.dll", "CreateRoundRectRgn", (void*)THETAZ_CreateRoundRectRgn);
    register_api("gdi32.dll", "CreateRectRgnIndirect", (void*)THETAZ_CreateRectRgnIndirect);
    register_api("gdi32.dll", "CombineRgn", (void*)THETAZ_CombineRgn);
    register_api("gdi32.dll", "DPtoLP", (void*)THETAZ_DPtoLP);
    register_api("gdi32.dll", "ExcludeClipRect", (void*)THETAZ_ExcludeClipRect);
    register_api("gdi32.dll", "ExtCreatePen", (void*)THETAZ_ExtCreatePen);
    register_api("gdi32.dll", "ExtCreateRegion", (void*)THETAZ_ExtCreateRegion);
    register_api("gdi32.dll", "ExtTextOutW", (void*)THETAZ_ExtTextOutW);
    register_api("gdi32.dll", "FillRgn", (void*)THETAZ_FillRgn);
    register_api("gdi32.dll", "FrameRgn", (void*)THETAZ_FrameRgn);
    register_api("gdi32.dll", "GdiAlphaBlend", (void*)THETAZ_GdiAlphaBlend);
    register_api("gdi32.dll", "GetBkColor", (void*)THETAZ_GetBkColor);
    register_api("gdi32.dll", "GetBkMode", (void*)THETAZ_GetBkMode);
    register_api("gdi32.dll", "GetClipBox", (void*)THETAZ_GetClipBox);
    register_api("gdi32.dll", "GetClipRgn", (void*)THETAZ_GetClipRgn);
    register_api("gdi32.dll", "GetCurrentObject", (void*)THETAZ_GetCurrentObject);
    register_api("gdi32.dll", "GetDIBColorTable", (void*)THETAZ_GetDIBColorTable);
    register_api("gdi32.dll", "GetDIBits", (void*)THETAZ_GetDIBits);
    register_api("gdi32.dll", "GetLayout", (void*)THETAZ_GetLayout);
    register_api("gdi32.dll", "GetNearestColor", (void*)THETAZ_GetNearestColor);
    register_api("gdi32.dll", "GetObjectW", (void*)THETAZ_GetObjectW);
    register_api("gdi32.dll", "GetPixel", (void*)THETAZ_GetPixel);
    register_api("gdi32.dll", "GetTextColor", (void*)THETAZ_GetTextColor);
    register_api("gdi32.dll", "GetTextExtentPoint32A", (void*)(int(*)(uint32_t, const char*, int, void*))THETAZ_GetTextExtentPoint32A);
    register_api("gdi32.dll", "GetTextExtentPoint32W", (void*)(int(*)(uint32_t, const uint16_t*, int, void*))THETAZ_GetTextExtentPoint32W);
    register_api("gdi32.dll", "GetTextExtentPointA", (void*)(int(*)(uint32_t, const char*, int, void*))THETAZ_GetTextExtentPointA);
    register_api("gdi32.dll", "GetTextExtentPointW", (void*)(int(*)(uint32_t, const uint16_t*, int, void*))THETAZ_GetTextExtentPointW);
    register_api("gdi32.dll", "GetTextMetricsW", (void*)THETAZ_GetTextMetricsW);
    register_api("gdi32.dll", "IntersectClipRect", (void*)THETAZ_IntersectClipRect);
    register_api("gdi32.dll", "LineTo", (void*)THETAZ_LineTo);
    register_api("gdi32.dll", "MoveToEx", (void*)THETAZ_MoveToEx);
    register_api("gdi32.dll", "OffsetWindowOrgEx", (void*)THETAZ_OffsetWindowOrgEx);
    register_api("gdi32.dll", "PatBlt", (void*)THETAZ_PatBlt);
    register_api("gdi32.dll", "PolyPolyline", (void*)THETAZ_PolyPolyline);
    register_api("gdi32.dll", "Polygon", (void*)THETAZ_Polygon);
    register_api("gdi32.dll", "RectVisible", (void*)THETAZ_RectVisible);
    register_api("gdi32.dll", "Rectangle", (void*)THETAZ_Rectangle);
    register_api("gdi32.dll", "SelectClipRgn", (void*)THETAZ_SelectClipRgn);
    register_api("gdi32.dll", "SelectPalette", (void*)THETAZ_SelectPalette);
    register_api("gdi32.dll", "SetDIBColorTable", (void*)THETAZ_SetDIBColorTable);
    register_api("gdi32.dll", "SetDIBits", (void*)THETAZ_SetDIBits);
    register_api("gdi32.dll", "SetPixel", (void*)THETAZ_SetPixel);
    register_api("gdi32.dll", "SetPolyFillMode", (void*)THETAZ_SetPolyFillMode);
    register_api("gdi32.dll", "SetTextAlign", (void*)THETAZ_SetTextAlign);
    register_api("gdi32.dll", "SetViewportOrgEx", (void*)THETAZ_SetViewportOrgEx);
    register_api("gdi32.dll", "SetWindowOrgEx", (void*)THETAZ_SetWindowOrgEx);
    register_api("gdi32.dll", "StretchBlt", (void*)THETAZ_StretchBlt);
    register_api("gdi32.dll", "StretchDIBits", (void*)THETAZ_StretchDIBits);

    // --- user32.dll (W-suffix and missing A-suffix) ---
    register_api("user32.dll", "AdjustWindowRect", (void*)THETAZ_AdjustWindowRect);
    register_api("user32.dll", "AdjustWindowRectEx", (void*)THETAZ_AdjustWindowRectEx);
    register_api("user32.dll", "AppendMenuW", (void*)THETAZ_AppendMenuW);
    register_api("user32.dll", "BeginDeferWindowPos", (void*)THETAZ_BeginDeferWindowPos);
    register_api("user32.dll", "CallWindowProcA", (void*)THETAZ_CallWindowProcA);
    register_api("user32.dll", "CallWindowProcW", (void*)THETAZ_CallWindowProcW);
    register_api("user32.dll", "CheckMenuItem", (void*)THETAZ_CheckMenuItem);
    register_api("user32.dll", "ChildWindowFromPoint", (void*)THETAZ_ChildWindowFromPoint);
    register_api("user32.dll", "ClientToScreen", (void*)THETAZ_ClientToScreen);
    register_api("user32.dll", "CopyIcon", (void*)THETAZ_CopyIcon);
    register_api("user32.dll", "CopyImage", (void*)THETAZ_CopyImage);
    register_api("user32.dll", "CopyRect", (void*)THETAZ_CopyRect);
    register_api("user32.dll", "CreateCaret", (void*)THETAZ_CreateCaret);
    register_api("user32.dll", "CreateDialogIndirectParamA", (void*)THETAZ_CreateDialogIndirectParamA);
    register_api("user32.dll", "CreateDialogIndirectParamW", (void*)THETAZ_CreateDialogIndirectParamW);
    register_api("user32.dll", "CreateIconIndirect", (void*)THETAZ_CreateIconIndirect);
    register_api("user32.dll", "CreatePopupMenu", (void*)THETAZ_CreatePopupMenu);
    register_api("user32.dll", "CreateWindowExW", (void*)THETAZ_CreateWindowExW);
    register_api("user32.dll", "DefWindowProcW", (void*)THETAZ_DefWindowProcW);
    register_api("user32.dll", "DeferWindowPos", (void*)THETAZ_DeferWindowPos);
    register_api("user32.dll", "DestroyCaret", (void*)THETAZ_DestroyCaret);
    register_api("user32.dll", "DestroyCursor", (void*)THETAZ_DestroyCursor);
    register_api("user32.dll", "DestroyIcon", (void*)THETAZ_DestroyIcon);
    register_api("user32.dll", "DialogBoxParamW", (void*)THETAZ_DialogBoxParamW);
    register_api("user32.dll", "DispatchMessageW", (void*)THETAZ_DispatchMessageW);
    register_api("user32.dll", "DrawEdge", (void*)THETAZ_DrawEdge);
    register_api("user32.dll", "DrawFocusRect", (void*)THETAZ_DrawFocusRect);
    register_api("user32.dll", "DrawFrameControl", (void*)THETAZ_DrawFrameControl);
    register_api("user32.dll", "DrawIcon", (void*)THETAZ_DrawIcon);
    register_api("user32.dll", "DrawIconEx", (void*)THETAZ_DrawIconEx);
    register_api("user32.dll", "DrawTextA", (void*)THETAZ_DrawTextA);
    register_api("user32.dll", "DrawTextW", (void*)THETAZ_DrawTextW);
    register_api("user32.dll", "EnableScrollBar", (void*)THETAZ_EnableScrollBar);
    register_api("user32.dll", "EndDeferWindowPos", (void*)THETAZ_EndDeferWindowPos);
    register_api("user32.dll", "FillRect", (void*)THETAZ_FillRect);
    register_api("user32.dll", "FrameRect", (void*)THETAZ_FrameRect);
    register_api("user32.dll", "GetActiveWindow", (void*)THETAZ_GetActiveWindow);
    register_api("user32.dll", "GetAsyncKeyState", (void*)THETAZ_GetAsyncKeyState);
    register_api("user32.dll", "GetCapture", (void*)THETAZ_GetCapture);
    register_api("user32.dll", "GetClassLongW", (void*)THETAZ_GetClassLongW);
    register_api("user32.dll", "GetClassNameW", (void*)THETAZ_GetClassNameW);
    register_api("user32.dll", "GetCursorPos", (void*)THETAZ_GetCursorPos);
    register_api("user32.dll", "GetDCEx", (void*)THETAZ_GetDCEx);
    register_api("user32.dll", "GetDC", (void*)THETAZ_GetDC);
    register_api("user32.dll", "ReleaseDC", (void*)THETAZ_ReleaseDC);
    register_api("user32.dll", "GetClientRect", (void*)THETAZ_GetClientRect);
    register_api("gdi32.dll", "SelectObject", (void*)THETAZ_SelectObject);
    register_api("gdi32.dll", "GetDeviceCaps", (void*)THETAZ_GetDeviceCaps);
    register_api("gdi32.dll", "SetTextColor", (void*)THETAZ_SetTextColor);
    register_api("gdi32.dll", "SetBkColor", (void*)THETAZ_SetBkColor);
    register_api("gdi32.dll", "SetBkMode", (void*)THETAZ_SetBkMode);
    register_api("gdi32.dll", "TextOutW", (void*)THETAZ_TextOutW);
    register_api("gdi32.dll", "GetStockObject", (void*)THETAZ_GetStockObject);
    register_api("gdi32.dll", "CreateSolidBrush", (void*)THETAZ_CreateSolidBrush);
    register_api("user32.dll", "GetDoubleClickTime", (void*)THETAZ_GetDoubleClickTime);
    register_api("user32.dll", "GetDpiForWindow", (void*)THETAZ_GetDpiForWindow);
    register_api("user32.dll", "GetIconInfo", (void*)THETAZ_GetIconInfo);
    register_api("user32.dll", "GetKeyNameTextW", (void*)THETAZ_GetKeyNameTextW);
    register_api("user32.dll", "GetKeyState", (void*)THETAZ_GetKeyState);
    register_api("user32.dll", "GetMenuState", (void*)THETAZ_GetMenuState);
    register_api("user32.dll", "GetMessagePos", (void*)THETAZ_GetMessagePos);
    register_api("user32.dll", "GetMessageTime", (void*)THETAZ_GetMessageTime);
    register_api("user32.dll", "GetMessageW", (void*)THETAZ_GetMessageW);
    register_api("user32.dll", "GetMonitorInfoW", (void*)THETAZ_GetMonitorInfoW);
    register_api("user32.dll", "GetNextDlgTabItem", (void*)THETAZ_GetNextDlgTabItem);
    register_api("user32.dll", "GetParent", (void*)THETAZ_GetParent);
    register_api("user32.dll", "GetPropW", (void*)THETAZ_GetPropW);
    register_api("user32.dll", "GetScrollInfo", (void*)THETAZ_GetScrollInfo);
    register_api("user32.dll", "GetScrollPos", (void*)THETAZ_GetScrollPos);
    register_api("user32.dll", "GetScrollRange", (void*)THETAZ_GetScrollRange);
    register_api("user32.dll", "GetSysColor", (void*)THETAZ_GetSysColor);
    register_api("user32.dll", "GetSysColorBrush", (void*)THETAZ_GetSysColorBrush);
    register_api("user32.dll", "GetTopWindow", (void*)THETAZ_GetTopWindow);
    register_api("user32.dll", "GetWindow", (void*)THETAZ_GetWindow);
    register_api("user32.dll", "GetWindowLongW", (void*)THETAZ_GetWindowLongW);
    register_api("user32.dll", "GetWindowLongPtrW", (void*)THETAZ_GetWindowLongPtrW);
    register_api("user32.dll", "GetWindowRect", (void*)THETAZ_GetWindowRect);
    register_api("user32.dll", "GetWindowRgn", (void*)THETAZ_GetWindowRgn);
    register_api("user32.dll", "GetWindowTextLengthW", (void*)THETAZ_GetWindowTextLengthW);
    register_api("user32.dll", "GetWindowTextW", (void*)THETAZ_GetWindowTextW);
    register_api("user32.dll", "IntersectRect", (void*)THETAZ_IntersectRect);
    register_api("user32.dll", "InvalidateRgn", (void*)THETAZ_InvalidateRgn);
    register_api("user32.dll", "IsChild", (void*)THETAZ_IsChild);
    register_api("user32.dll", "IsDialogMessageW", (void*)THETAZ_IsDialogMessageW);
    register_api("user32.dll", "IsWindowUnicode", (void*)THETAZ_IsWindowUnicode);
    register_api("user32.dll", "LoadBitmapW", (void*)THETAZ_LoadBitmapW);
    register_api("user32.dll", "LoadCursorW", (void*)THETAZ_LoadCursorW);
    register_api("user32.dll", "LoadIconW", (void*)THETAZ_LoadIconW);
    register_api("user32.dll", "LoadImageA", (void*)THETAZ_LoadImageA);
    register_api("user32.dll", "LoadImageW", (void*)THETAZ_LoadImageW);
    register_api("user32.dll", "LoadStringW", (void*)THETAZ_LoadStringW);
    register_api("user32.dll", "MapDialogRect", (void*)THETAZ_MapDialogRect);
    register_api("user32.dll", "MapVirtualKeyW", (void*)THETAZ_MapVirtualKeyW);
    register_api("user32.dll", "MapWindowPoints", (void*)THETAZ_MapWindowPoints);
    register_api("user32.dll", "MonitorFromRect", (void*)THETAZ_MonitorFromRect);
    register_api("user32.dll", "MoveWindow", (void*)THETAZ_MoveWindow);
    register_api("user32.dll", "NotifyWinEvent", (void*)THETAZ_NotifyWinEvent);
    register_api("user32.dll", "PeekMessageW", (void*)THETAZ_PeekMessageW);
    register_api("user32.dll", "PostMessageW", (void*)THETAZ_PostMessageW);
    register_api("user32.dll", "RedrawWindow", (void*)THETAZ_RedrawWindow);
    register_api("user32.dll", "RegisterClassW", (void*)THETAZ_RegisterClassW);
    register_api("user32.dll", "RegisterWindowMessageW", (void*)THETAZ_RegisterWindowMessageW);
    register_api("user32.dll", "ReleaseCapture", (void*)THETAZ_ReleaseCapture);
    register_api("user32.dll", "RemovePropW", (void*)THETAZ_RemovePropW);
    register_api("user32.dll", "ScreenToClient", (void*)THETAZ_ScreenToClient);
    register_api("user32.dll", "ScrollWindow", (void*)THETAZ_ScrollWindow);
    register_api("user32.dll", "ScrollWindowEx", (void*)THETAZ_ScrollWindowEx);
    register_api("user32.dll", "SendDlgItemMessageW", (void*)THETAZ_SendDlgItemMessageW);
    register_api("user32.dll", "SendMessageW", (void*)THETAZ_SendMessageW);
    register_api("user32.dll", "SetCapture", (void*)THETAZ_SetCapture);
    register_api("user32.dll", "SetCaretPos", (void*)THETAZ_SetCaretPos);
    register_api("user32.dll", "SetCursor", (void*)THETAZ_SetCursor);
    register_api("user32.dll", "SetParent", (void*)THETAZ_SetParent);
    register_api("user32.dll", "SetPropW", (void*)THETAZ_SetPropW);
    register_api("user32.dll", "SetScrollInfo", (void*)THETAZ_SetScrollInfo);
    register_api("user32.dll", "SetScrollPos", (void*)THETAZ_SetScrollPos);
    register_api("user32.dll", "SetScrollRange", (void*)THETAZ_SetScrollRange);
    register_api("user32.dll", "SetWindowLongA", (void*)THETAZ_SetWindowLongA);
    register_api("user32.dll", "SetWindowLongW", (void*)THETAZ_SetWindowLongW);
    register_api("user32.dll", "SetWindowLongPtrA", (void*)THETAZ_SetWindowLongPtrA);
    register_api("user32.dll", "SetWindowLongPtrW", (void*)THETAZ_SetWindowLongPtrW);
    register_api("user32.dll", "SetWindowPos", (void*)THETAZ_SetWindowPos);
    register_api("user32.dll", "SetWindowRgn", (void*)THETAZ_SetWindowRgn);
    register_api("user32.dll", "SetWindowTextW", (void*)THETAZ_SetWindowTextW);
    register_api("user32.dll", "ShowCaret", (void*)THETAZ_ShowCaret);
    register_api("user32.dll", "ShowScrollBar", (void*)THETAZ_ShowScrollBar);
    register_api("user32.dll", "SubtractRect", (void*)THETAZ_SubtractRect);
    register_api("user32.dll", "TrackMouseEvent", (void*)THETAZ_TrackMouseEvent);
    register_api("user32.dll", "TrackPopupMenu", (void*)THETAZ_TrackPopupMenu);
    register_api("user32.dll", "UnionRect", (void*)THETAZ_UnionRect);
    register_api("user32.dll", "UnregisterClassW", (void*)THETAZ_UnregisterClassW);
    register_api("user32.dll", "ValidateRect", (void*)THETAZ_ValidateRect);
    register_api("user32.dll", "WindowFromPoint", (void*)THETAZ_WindowFromPoint);
    register_api("user32.dll", "wsprintfW", (void*)THETAZ_wsprintfW);

    // --- kernel32.dll (additional) ---
    register_api("kernel32.dll", "CompareFileTime", (void*)THETAZ_CompareFileTime);
    register_api("kernel32.dll", "CompareStringA", (void*)THETAZ_CompareStringA);
    register_api("kernel32.dll", "CompareStringOrdinal", (void*)THETAZ_CompareStringOrdinal);
    register_api("kernel32.dll", "IsBadCodePtr", (void*)THETAZ_IsBadCodePtr);
    register_api("kernel32.dll", "IsBadStringPtrA", (void*)THETAZ_IsBadStringPtrA);
    register_api("kernel32.dll", "IsBadWritePtr", (void*)THETAZ_IsBadWritePtr);
    register_api("kernel32.dll", "GlobalAddAtomW", (void*)THETAZ_GlobalAddAtomW);
    register_api("kernel32.dll", "GlobalDeleteAtom", (void*)THETAZ_GlobalDeleteAtom);

    // --- shlwapi.dll ---
    register_api("shlwapi.dll", "StrCmpIW", (void*)THETAZ_StrCmpIW);
    register_api("shlwapi.dll", "StrCmpNIW", (void*)THETAZ_StrCmpNIW);

    // kernelbase.dll is also in the builtin list; Wine DLLs import from it
    register_api("kernelbase.dll", "StrCmpIW", (void*)THETAZ_StrCmpIW);
    register_api("kernelbase.dll", "StrCmpNIW", (void*)THETAZ_StrCmpNIW);

    // --- advapi32.dll (additional) ---
    register_api("advapi32.dll", "RegOpenKeyA", (void*)THETAZ_RegOpenKeyA);

    // credui.dll imports CredEnumerateW/CredFree/CredWriteW from advapi32.dll
    register_api("advapi32.dll", "CredEnumerateW", (void*)THETAZ_CredEnumerateW);
    register_api("advapi32.dll", "CredFree", (void*)THETAZ_CredFree);
    register_api("advapi32.dll", "CredWriteW", (void*)THETAZ_CredWriteW);

    // --- CRT (ucrtbase.dll / msvcrt.dll) ---
    register_api("ucrtbase.dll", "floor", (void*)THETAZ_floor);
    register_api("ucrtbase.dll", "isprint", (void*)THETAZ_isprint);
    register_api("ucrtbase.dll", "iswalnum", (void*)THETAZ_iswalnum);
    register_api("ucrtbase.dll", "qsort", (void*)THETAZ_qsort);
    register_api("ucrtbase.dll", "wcschr", (void*)THETAZ_wcschr);
    register_api("ucrtbase.dll", "wcsncmp", (void*)THETAZ_wcsncmp);
    register_api("ucrtbase.dll", "wcstol", (void*)THETAZ_wcstol);
    register_api("msvcrt.dll", "floor", (void*)THETAZ_floor);
    register_api("msvcrt.dll", "isprint", (void*)THETAZ_isprint);
    register_api("msvcrt.dll", "iswalnum", (void*)THETAZ_iswalnum);
    register_api("msvcrt.dll", "qsort", (void*)THETAZ_qsort);
    register_api("msvcrt.dll", "wcschr", (void*)THETAZ_wcschr);
    register_api("msvcrt.dll", "wcsncmp", (void*)THETAZ_wcsncmp);
    register_api("msvcrt.dll", "wcstol", (void*)THETAZ_wcstol);

    // --- credui.dll ---
    register_api("credui.dll", "CredEnumerateW", (void*)THETAZ_CredEnumerateW);
    register_api("credui.dll", "CredFree", (void*)THETAZ_CredFree);
    register_api("credui.dll", "CredWriteW", (void*)THETAZ_CredWriteW);

    // --- DelayLoadFailureHook ---
    register_api("kernel32.dll", "DelayLoadFailureHook", (void*)THETAZ_DelayLoadFailureHook);

    // --- FindResourceA (comctl32 imports it) ---
    register_api("kernel32.dll", "FindResourceA", (void*)THETAZ_FindResourceA);

    fprintf(stderr, "[wine-compat] initialized: gdi32/user32/shlwapi/advapi32/CRT/credui stubs registered\n");
}

} // namespace ThetaZ
