// ThetaZ — Window/GUI subsystem interface for cross-file access
// Copyright (c) 2026 ThetaZ Project. MIT License.

#pragma once

#include "core/api/api.h"
#include "core/ntdll/thread.h"
#include "core/x86/x86.h"

#include <cstdint>
#include <mutex>
#include <condition_variable>
#include <map>
#include <deque>
#include <unordered_map>
#include <string>
#include <thread>
#include <atomic>

#include <X11/Xlib.h>

namespace ThetaZ {

// Forward declarations
struct Window;
struct WindowClass;

// Guest MSG layout (x86)
enum {
    MSG_HWND = 0x00,
    MSG_MESSAGE = 0x04,
    MSG_WPARAM = 0x08,
    MSG_LPARAM = 0x0C,
    MSG_TIME = 0x10,
    MSG_PT = 0x14,
    MSG_SIZE = 0x1C,
};

// Window messages
enum {
    WM_CREATE = 0x0001,
    WM_DESTROY = 0x0002,
    WM_PAINT = 0x000F,
    WM_QUIT = 0x0012,
    WM_ERASEBKGND = 0x0014,
    WM_CLOSE = 0x0010,
    WM_TIMER = 0x0113,
    WM_COMMAND = 0x0111,
    WM_SETTEXT = 0x000C,
    WM_GETTEXT = 0x000D,
    WM_GETTEXTLENGTH = 0x000E,
    WM_KEYDOWN = 0x0100,
    WM_CHAR = 0x0102,
};

// Window class styles
enum {
    CS_HREDRAW = 0x0002,
    CS_VREDRAW = 0x0001,
    CS_DBLCLKS = 0x0008,
    CS_OWNDC = 0x0020,
    CS_CLASSDC = 0x0040,
    CS_PARENTDC = 0x0080,
    CS_NOCLOSE = 0x0200,
    CS_SAVEBITS = 0x0800,
    CS_BYTEALIGNCLIENT = 0x1000,
    CS_BYTEALIGNWINDOW = 0x2000,
    CS_GLOBALCLASS = 0x4000,
    CS_IME = 0x00010000,
};

// Window styles
enum {
    WS_OVERLAPPED = 0x00000000,
    WS_POPUP = 0x80000000,
    WS_CHILD = 0x40000000,
    WS_MINIMIZE = 0x20000000,
    WS_VISIBLE = 0x10000000,
    WS_DISABLED = 0x08000000,
    WS_CLIPSIBLINGS = 0x04000000,
    WS_CLIPCHILDREN = 0x02000000,
    WS_MAXIMIZE = 0x01000000,
    WS_CAPTION = 0x00C00000,
    WS_BORDER = 0x00800000,
    WS_DLGFRAME = 0x00400000,
    WS_VSCROLL = 0x00200000,
    WS_HSCROLL = 0x00100000,
    WS_SYSMENU = 0x00080000,
    WS_THICKFRAME = 0x00040000,
    WS_GROUP = 0x00020000,
    WS_TABSTOP = 0x00010000,
    WS_MINIMIZEBOX = 0x00020000,
    WS_MAXIMIZEBOX = 0x00010000,
    WS_TILED = WS_OVERLAPPED,
    WS_ICONIC = WS_MINIMIZE,
    WS_SIZEBOX = WS_THICKFRAME,
    WS_OVERLAPPEDWINDOW = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
    WS_TILEDWINDOW = WS_OVERLAPPEDWINDOW,
    WS_POPUPWINDOW = WS_POPUP | WS_BORDER | WS_SYSMENU,
    WS_CHILDWINDOW = WS_CHILD,
};

// Extended window styles
enum {
    WS_EX_DLGMODALFRAME = 0x00000001,
    WS_EX_NOPARENTNOTIFY = 0x00000004,
    WS_EX_TOPMOST = 0x00000008,
    WS_EX_ACCEPTFILES = 0x00000010,
    WS_EX_TRANSPARENT = 0x00000020,
    WS_EX_MDICHILD = 0x00000040,
    WS_EX_TOOLWINDOW = 0x00000080,
    WS_EX_WINDOWEDGE = 0x00000100,
    WS_EX_CLIENTEDGE = 0x00000200,
    WS_EX_CONTEXTHELP = 0x00000400,
    WS_EX_RIGHT = 0x00001000,
    WS_EX_LEFT = 0x00000000,
    WS_EX_RTLREADING = 0x00002000,
    WS_EX_LTRREADING = 0x00000000,
    WS_EX_LEFTSCROLLBAR = 0x00004000,
    WS_EX_RIGHTSCROLLBAR = 0x00000000,
    WS_EX_CONTROLPARENT = 0x00010000,
    WS_EX_STATICEDGE = 0x00020000,
    WS_EX_APPWINDOW = 0x00040000,
    WS_EX_OVERLAPPEDWINDOW = WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE,
    WS_EX_PALETTEWINDOW = WS_EX_WINDOWEDGE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
    WS_EX_LAYERED = 0x00080000,
    WS_EX_NOINHERITLAYOUT = 0x00100000,
    WS_EX_LAYOUTRTL = 0x00400000,
    WS_EX_COMPOSITED = 0x02000000,
    WS_EX_NOACTIVATE = 0x08000000,
};

// Button styles
enum {
    BS_PUSHBUTTON = 0x00000000,
    BS_DEFPUSHBUTTON = 0x00000001,
    BS_CHECKBOX = 0x00000002,
    BS_AUTOCHECKBOX = 0x00000003,
    BS_RADIOBUTTON = 0x00000004,
    BS_3STATE = 0x00000005,
    BS_AUTO3STATE = 0x00000006,
    BS_GROUPBOX = 0x00000007,
    BS_USERBUTTON = 0x00000008,
    BS_AUTORADIOBUTTON = 0x00000009,
    BS_PUSHBOX = 0x0000000A,
    BS_OWNERDRAW = 0x0000000B,
    BS_TYPEMASK = 0x0000000F,
    BS_LEFTTEXT = 0x00000020,
    BS_TEXT = 0x00000000,
    BS_ICON = 0x00000040,
    BS_BITMAP = 0x00000080,
    BS_LEFT = 0x00000100,
    BS_RIGHT = 0x00000200,
    BS_CENTER = 0x00000300,
    BS_TOP = 0x00000400,
    BS_BOTTOM = 0x00000800,
    BS_VCENTER = 0x00000C00,
    BS_PUSHLIKE = 0x00001000,
    BS_MULTILINE = 0x00002000,
    BS_NOTIFY = 0x00004000,
    BS_FLAT = 0x00008000,
    BS_RIGHTBUTTON = BS_LEFTTEXT,
};

// Button messages
enum {
    BM_CLICK = 0x00F5,
    BM_GETCHECK = 0x00F0,
    BM_SETCHECK = 0x00F1,
    BM_GETSTATE = 0x00F2,
    BM_SETSTATE = 0x00F3,
    BM_SETSTYLE = 0x00F4,
};

// Control IDs
enum {
    IDC_STATIC = -1,
    IDC_BUTTON = 0x0080,
    IDC_EDIT = 0x0081,
    IDC_STATIC_TEXT = 0x0082,
};
// Type definitions
// =========================================================================

using INT_PTR = intptr_t;
using SIZE_T = size_t;

// Guest MSG layout (x86)
struct GuestMsg {
    uint64_t hwnd = 0;
    uint32_t message = 0;
    uint64_t wparam = 0;
    uint64_t lparam = 0;
};

// Guest timer info
struct GuestTimerInfo {
    uint64_t hwnd = 0;
    uint32_t id = 0;
    uint32_t period_ms = 0;
    uint64_t due_ms = 0;
    uint32_t func = 0;
};

// Window class
struct WindowClass {
    std::string name;
    uint32_t style = 0;
    uint32_t lpfnWndProc = 0;
    int cbClsExtra = 0;
    int cbWndExtra = 0;
    uint32_t hInstance = 0;
    uint32_t hIcon = 0;
    uint32_t hCursor = 0;
    uint32_t hbrBackground = 0;
    const char* lpszMenuName = nullptr;
    const char* lpszClassName = nullptr;
};

// Window structure
struct Window {
    uint64_t hwnd = 0;
    uint64_t wnd_proc = 0;
    std::string class_name;
    std::string title;
    uint32_t style = 0;
    uint32_t ex_style = 0;
    int x = 0, y = 0, w = 0, h = 0;
    bool visible = false;
    bool dirty = false;
    bool enabled = true;
    int inv_left = 0, inv_top = 0, inv_right = 0, inv_bottom = 0;
    uint64_t parent = 0;      // control: owning window hwnd
    uint32_t id = 0;          // control id (menu/ hmenu param low word)
    uint32_t ctrl_type = 0;   // kCtrlNone = 0, etc.
    bool checked = false;     // checkbox state
    // EDIT: buffer + cursor.
    std::string text;          // control text (EDIT buffer / combo box field)
    int sel_start = 0, sel_end = 0; // EDIT selection (char offsets, [start,end))
    bool read_only = false;
    size_t max_text = 0x7FFFFFFF;  // EM_SETLIMITTEXT / default
    // LISTBOX / COMBOBOX: items + selection.
    std::vector<std::string> items;
    std::vector<uint32_t> item_data;
    int cur_sel = -1;          // LB/CB current selection (-1 = none)
    int top_index = 0;         // first visible item
    bool multi_sel = false;    // LBS_EXTENDEDSEL / LBS_MULTIPLESEL
    std::vector<bool> sel_bits; // per-item selection state for multi-select
    bool dropped = false;      // COMBOBOX dropdown open
    int list_h = 0;            // dropdown height (from CreateWindowEx h)
    bool list_visible = false; // dropdown window shown
    bool has_focus = false;
    ::Window x11_window = 0;
    ::Window x11_parent = 0;
    int x11_x = 0, x11_y = 0;
    // GDI DC
    uint32_t hdc = 0;
    int bk_mode = 1;
    uint32_t back_color = 0x00FFFFFF;
    uint32_t fore_color = 0x00000000;
};

// GDI DC
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

// GDI object
struct GdiObject {
    uint32_t type;
    uint32_t width;
    uint32_t height;
    void* data;
};

// Global state (defined in window.cpp)
extern std::mutex g_window_mutex;
extern std::condition_variable g_queue_cond;
extern std::condition_variable g_timer_cv;
extern std::map<std::string, WindowClass> g_classes;
extern std::map<uint64_t, Window> g_windows;
extern std::deque<GuestMsg> g_queue;
extern std::unordered_map<uint64_t, GuestTimerInfo> g_timers;
extern uint64_t g_next_hwnd;
extern bool g_quit_pending;
extern DWORD g_quit_exit_code;
extern bool g_dialog_result_active;
extern int g_dialog_result;
extern std::unordered_map<uint64_t, ::Window> g_x11_windows;
extern std::unordered_map<uint64_t, ::Window> g_x11_hwnds;
extern ::Display* g_x11_display;
extern std::mutex g_x11_mutex;
extern uint64_t g_next_timer_id;
extern const uint64_t kDesktopHwnd;

// GDI/Atom globals (defined in wine_compat.cpp)
extern std::mutex g_gdi_mutex;
extern std::unordered_map<uint32_t, GdiDc> g_gdi_dcs;
extern std::unordered_map<uint32_t, GdiObject> g_gdi_objects;
extern std::unordered_map<std::wstring, uint16_t> g_atom_table;

// Forward declarations
Window* find_window(uint64_t hwnd);
bool is_control_hwnd(uint64_t hwnd);
void call_wnd_proc(Window* win, uint32_t msg, uint64_t wparam, uint64_t lparam);
void x11_paint_window(::Window xw);
void post_button_command(uint64_t hwnd, uint32_t id, uint32_t notify_code);
void post_control_notify(uint64_t ctrl_hwnd, uint16_t code);
uint64_t steady_clock_ms();
void write_guest_msg(uint8_t* dst, const GuestMsg& msg);
uint64_t THETAZ_GetDlgItem(uint64_t hdlg, int id);
BOOL WINAPI_CALL THETAZ_CheckDlgButton(uint64_t hdlg, int id, uint32_t check);
uint64_t THETAZ_SendMessageA(uint64_t hwnd, uint32_t message, uint64_t wparam, uint64_t lparam);
uint64_t THETAZ_PostMessageA(uint64_t hwnd, uint32_t message, uint64_t wparam, uint64_t lparam);

// =========================================================================
// Wrapper functions for 64-bit dispatch
// =========================================================================
uint64_t WINAPI64_CallWindowProcA(void* proc, uint64_t hwnd, uint32_t msg, uint64_t wparam, uint64_t lparam);
uint64_t WINAPI64_CallWindowProcW(void* proc, uint64_t hwnd, uint32_t msg, uint64_t wparam, uint64_t lparam);
BOOL WINAPI64_DestroyWindow(uint64_t hwnd);
BOOL WINAPI64_DialogBoxParamA(uint32_t hInstance, const char* template_name, uint64_t hwnd_parent, void* dlg_proc, intptr_t param);
int WINAPI_CALL THETAZ_DialogBoxParamA(uint32_t hinst, const char* dlg, uint32_t parent, void* proc, LPARAM init);
BOOL WINAPI64_CheckDlgButton(uint64_t hdlg, int id, uint32_t check);
INT_PTR WINAPI64_EndDialog(uint64_t hwnd, INT_PTR result);
BOOL WINAPI64_EnableWindow(uint64_t hwnd, BOOL enable);
BOOL WINAPI64_EndPaint(uint64_t hwnd, const void* ps);
int WINAPI64_GetDlgCtrlID(uint64_t hwnd);
BOOL WINAPI64_GetMessageA(void* msg, uint64_t hwnd, uint32_t filter_min, uint32_t filter_max);
BOOL WINAPI64_TranslateMessage(const void* msg);
uint64_t WINAPI64_DispatchMessageA(const void* msg);
BOOL WINAPI64_PeekMessageA(void* msg, uint64_t hwnd, uint32_t filter_min, uint32_t filter_max, uint32_t remove);
BOOL WINAPI64_PostMessageA(uint64_t hwnd, uint32_t message, uint64_t wparam, uint64_t lparam);
uint64_t WINAPI64_SendMessageA(uint64_t hwnd, uint32_t message, uint64_t wparam, uint64_t lparam);
uint64_t WINAPI64_DefWindowProcA(uint64_t hwnd, uint32_t message, uint64_t wparam, uint64_t lparam);
BOOL WINAPI64_UpdateWindow(uint64_t hwnd);
int WINAPI64_GetDlgCtrlID(uint64_t hwnd);
BOOL WINAPI64_EnableWindow(uint64_t hwnd, BOOL enable);
BOOL WINAPI64_IsWindow(uint64_t hwnd);
BOOL WINAPI64_IsWindowEnabled(uint64_t hwnd);
BOOL WINAPI64_IsWindowVisible(uint64_t hwnd);

BOOL WINAPI64_GetWindowTextA(uint64_t hwnd, char* buf, int max);
int WINAPI64_GetWindowTextLengthA(uint64_t hwnd);
BOOL WINAPI64_GetDlgItemTextA(uint64_t hdlg, int id, char* buf, int max);
BOOL WINAPI64_InvalidateRect(uint64_t hwnd, const void* rect, BOOL erase);
BOOL WINAPI64_KillTimer(uint64_t hwnd, uintptr_t nIDEvent);
BOOL WINAPI64_MessageBoxA(uint64_t hwnd, const char* text, const char* caption, uint32_t type);
BOOL WINAPI64_PeekMessageA(void* msg, uint64_t hwnd, uint32_t filter_min, uint32_t filter_max, uint32_t remove);
BOOL WINAPI64_PostMessageA(uint64_t hwnd, uint32_t message, uint64_t wparam, uint64_t lparam);
void WINAPI64_PostQuitMessage(int exit_code);
BOOL WINAPI64_SetDlgItemTextA(uint64_t hdlg, int id, const char* text);
BOOL WINAPI64_SetWindowTextA(uint64_t hwnd, const char* text);
BOOL WINAPI64_ShowWindow(uint64_t hwnd, int cmd_show);
BOOL WINAPI64_TranslateMessage(const void* msg);
BOOL WINAPI64_UpdateWindow(uint64_t hwnd);
BOOL WINAPI64_SetWindowTextA(uint64_t hwnd, const char* text);

// Timer/threadpool synchronization globals
extern std::mutex g_window_mutex;
extern std::unordered_map<uint64_t, GuestTimerInfo> g_timers;
extern std::condition_variable g_timer_cv;

} // namespace ThetaZ