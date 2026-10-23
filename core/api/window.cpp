// ThetaZ — Copyright (c) 2026 ThetaZ Project. MIT License.
//
// Real user32 window/class registry + message queue.
//
// Windows GUI code registers window classes (WNDCLASSA), creates windows
// (CreateWindowExA) whose WNDPROC is a guest function, and runs a message loop
// (GetMessageA / TranslateMessage / DispatchMessageA). This layer models all of
// that in-process so guest window procedures actually receive real messages.
// There is no display; painting is recorded (BeginPaint/EndPaint) but drawing
// calls still no-op until the GDI object model lands (E4).

#include "core/api/api.h"
#include "core/ntdll/thread.h"
#include "core/x86/x86.h"
#include "core/x86/api_dispatch.h"

#include <cstdio>
#include <cstring>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <string>
#include <thread>
#include <unordered_map>
#include <unistd.h>
#include <vector>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

namespace ThetaZ {

// --- Guest-visible structure layouts (x86 / 32-bit) -------------------------

// MSG (x86)
enum {
    MSG_HWND = 0x00,
    MSG_MESSAGE = 0x04,
    MSG_WPARAM = 0x08,
    MSG_LPARAM = 0x0C,
    MSG_TIME = 0x10,
    MSG_PT = 0x14,
    MSG_SIZE = 0x1C,
};

// WNDCLASSA (x86)
enum {
    WNDCLASSA_STYLE = 0x00,
    WNDCLASSA_WNDPROC = 0x04,
    WNDCLASSA_CBCLSEXTRA = 0x08,
    WNDCLASSA_CBWNDEXTRA = 0x0C,
    WNDCLASSA_HINSTANCE = 0x10,
    WNDCLASSA_HICON = 0x14,
    WNDCLASSA_HCURSOR = 0x18,
    WNDCLASSA_HBRBACKGROUND = 0x1C,
    WNDCLASSA_LPSZMENUNAME = 0x20,
    WNDCLASSA_LPSZCLASSNAME = 0x24,
    WNDCLASSA_SIZE = 0x28,
};

// CREATESTRUCTA (x86)
enum {
    CREATESTRUCT_PARAMS = 0x00,
    CREATESTRUCT_INSTANCE = 0x04,
    CREATESTRUCT_MENU = 0x08,
    CREATESTRUCT_PARENT = 0x0C,
    CREATESTRUCT_CY = 0x10,
    CREATESTRUCT_CX = 0x14,
    CREATESTRUCT_Y = 0x18,
    CREATESTRUCT_X = 0x1C,
    CREATESTRUCT_STYLE = 0x20,
    CREATESTRUCT_NAME = 0x24,
    CREATESTRUCT_CLASS = 0x28,
    CREATESTRUCT_EXSTYLE = 0x2C,
    CREATESTRUCT_SIZE = 0x30,
};

// PAINTSTRUCT (x86)
enum {
    PAINTSTRUCT_HDC = 0x00,
    PAINTSTRUCT_FERASE = 0x04,
    PAINTSTRUCT_RCPAINT = 0x08,
    PAINTSTRUCT_SIZE = 0x40,
};

// --- Guest-visible structure layouts (x64 / 64-bit) -------------------------

// MSG (x64) - 48 bytes
enum {
    MSG64_HWND = 0x00,
    MSG64_MESSAGE = 0x08,
    MSG64_WPARAM = 0x10,
    MSG64_LPARAM = 0x18,
    MSG64_TIME = 0x20,
    MSG64_PT = 0x28,
    MSG64_SIZE = 0x38,
};

// WNDCLASSA (x64) - 72 bytes (80 with alignment)
enum {
    WNDCLASSA64_STYLE = 0x00,
    WNDCLASSA64_WNDPROC = 0x08,
    WNDCLASSA64_CBCLSEXTRA = 0x10,
    WNDCLASSA64_CBWNDEXTRA = 0x14,
    WNDCLASSA64_HINSTANCE = 0x18,
    WNDCLASSA64_HICON = 0x20,
    WNDCLASSA64_HCURSOR = 0x28,
    WNDCLASSA64_HBRBACKGROUND = 0x30,
    WNDCLASSA64_LPSZMENUNAME = 0x38,
    WNDCLASSA64_LPSZCLASSNAME = 0x40,
    WNDCLASSA64_SIZE = 0x48,
};

// CREATESTRUCTA (x64) - 80 bytes
enum {
    CREATESTRUCT64_PARAMS = 0x00,
    CREATESTRUCT64_INSTANCE = 0x08,
    CREATESTRUCT64_MENU = 0x10,
    CREATESTRUCT64_PARENT = 0x18,
    CREATESTRUCT64_CY = 0x20,
    CREATESTRUCT64_CX = 0x24,
    CREATESTRUCT64_Y = 0x28,
    CREATESTRUCT64_X = 0x2C,
    CREATESTRUCT64_STYLE = 0x30,
    CREATESTRUCT64_NAME = 0x38,
    CREATESTRUCT64_CLASS = 0x40,
    CREATESTRUCT64_EXSTYLE = 0x48,
    CREATESTRUCT64_SIZE = 0x50,
};

// PAINTSTRUCT (x64) - 56 bytes (struct has 8-byte alignment)
enum {
    PAINTSTRUCT64_HDC = 0x00,
    PAINTSTRUCT64_FERASE = 0x08,
    PAINTSTRUCT64_RCPAINT = 0x10,
    PAINTSTRUCT64_SIZE = 0x38,
};

// Window messages used by this layer.
enum {
    WM_CREATE = 0x0001,
    WM_DESTROY = 0x0002,
    WM_SETTEXT = 0x000C,
    WM_GETTEXT = 0x000D,
    WM_GETTEXTLENGTH = 0x000E,
    WM_PAINT = 0x000F,
    WM_CLOSE = 0x0010,
    WM_QUIT = 0x0012,
    WM_SHOWWINDOW = 0x0018,
    WM_ENABLE = 0x000A,
    WM_SETFOCUS = 0x0007,
    WM_KILLFOCUS = 0x0008,
    WM_GETDLGCODE = 0x0087,
    WM_KEYDOWN = 0x0100,
    WM_CHAR = 0x0102,
    WM_TIMER = 0x0113,
    WM_COMMAND = 0x0111,
    WM_ERASEBKGND = 0x0014,
    WM_LBUTTONDOWN = 0x0201,
    WM_LBUTTONUP = 0x0202,
    WM_USER = 0x0400,
    // button control messages / notifications
    BM_GETCHECK = 0x00F0,
    BM_SETCHECK = 0x00F1,
    BM_CLICK = 0x00F5,
    // edit control messages
    EM_GETSEL = 0x00B0,
    EM_SETSEL = 0x00B1,
    EM_GETRECT = 0x00B2,
    EM_REPLACESEL = 0x00C2,
    EM_GETLINE = 0x00C4,
    EM_GETLINECOUNT = 0x00BA,
    EM_GETMODIFY = 0x0008,
    EM_SETMODIFY = 0x0009,
    EM_GETTEXT = 0x000D,   // == WM_GETTEXT
    EM_GETTEXTLEN = 0x000E, // == WM_GETTEXTLENGTH
    EM_SETLIMITTEXT = 0x00C5,
    EM_SETREADONLY = 0x00CF,
    EM_SCROLL = 0x00B5,
    EM_LINEFROMCHAR = 0x00C9,
    // edit notifications
    EN_CHANGE = 0x0300,
    EN_UPDATE = 0x0400,
    EN_SETFOCUS = 0x0100,
    EN_KILLFOCUS = 0x0200,
    // listbox messages
    LB_ADDSTRING = 0x0180,
    LB_INSERTSTRING = 0x0181,
    LB_DELETESTRING = 0x0182,
    LB_RESETCONTENT = 0x0184,
    LB_GETCOUNT = 0x018B,
    LB_GETCURSEL = 0x0188,
    LB_SETCURSEL = 0x0186,
    LB_GETTEXT = 0x0189,
    LB_GETTEXTLEN = 0x018A,
    LB_GETSEL = 0x0187,
    LB_SETSEL = 0x0185,
    LB_GETSELCOUNT = 0x0190,
    LB_GETSELITEMS = 0x0191,
    LB_FINDSTRING = 0x018F,
    LB_GETITEMDATA = 0x0199,
    LB_SETITEMDATA = 0x019A,
    LB_SETTOPINDEX = 0x0197,
    LB_GETTOPINDEX = 0x018E,
    LB_SELECTSTRING = 0x018C,
    LB_ITEMFROMPOINT = 0x01A9,
    // listbox notifications
    LBN_SELCHANGE = 0x0001,
    LBN_DBLCLK = 0x0002,
    // combobox messages
    CB_ADDSTRING = 0x0143,
    CB_DELETESTRING = 0x0144,
    CB_GETCOUNT = 0x0146,
    CB_GETCURSEL = 0x0147,
    CB_GETLBTEXT = 0x0148,
    CB_GETLBTEXTLEN = 0x0149,
    CB_INSERTSTRING = 0x014A,
    CB_RESETCONTENT = 0x014B,
    CB_FINDSTRING = 0x014C,
    CB_SELECTSTRING = 0x014D,
    CB_SETCURSEL = 0x014E,
    CB_SHOWDROPDOWN = 0x014F,
    CB_GETDROPPEDSTATE = 0x0157,
    CB_GETITEMDATA = 0x0150,
    CB_SETITEMDATA = 0x0151,
    CB_SETEDITSEL = 0x0142,
    CB_GETEDITSEL = 0x0140,
    CB_GETTOPINDEX = 0x015B,
    CB_SETTOPINDEX = 0x015C,
    // combobox notifications
    CBN_SELCHANGE = 0x0001,
    CBN_DBLCLK = 0x0002,
    CBN_EDITCHANGE = 0x0005,
};

// Predefined control classes ("BUTTON"/"STATIC"/... from user32, no guest
// WNDPROC). ctrl_type values stored in Window::ctrl_type.
enum {
    kCtrlNone = 0,
    kCtrlButton = 1,
    kCtrlStatic = 2,
    kCtrlEdit = 3,
    kCtrlListbox = 4,
    kCtrlCombobox = 5,
};

enum {
    WS_CHILD = 0x40000000,
    WS_VISIBLE = 0x10000000,
    WS_DISABLED = 0x08000000,
    BS_DEFPUSHBUTTON = 0x00000001,
    BS_CHECKBOX = 0x00000002,
    BS_AUTOCHECKBOX = 0x00000003,
    ES_MULTILINE = 0x0004,
    ES_PASSWORD = 0x0020,
    ES_READONLY = 0x0800,
    LBS_NOTIFY = 0x0001,
    LBS_MULTIPLESEL = 0x0008,
    LBS_EXTENDEDSEL = 0x0800,
    CBS_DROPDOWN = 0x0002,
    CBS_DROPDOWNLIST = 0x0003,
    CBS_AUTOHSCROLL = 0x0040,
    CBS_HASSTRINGS = 0x0200,
    LB_ERR = -1,
    LB_OKAY = 0,
    CB_ERR = -1,
    CB_OKAY = 0,
};

// --- Internal structure definitions (must precede globals that use them) ---
struct WindowClass {
    std::string name;
    uint64_t wnd_proc = 0;
    uint32_t style = 0;
    uint64_t hinstance = 0;
};

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
    uint32_t ctrl_type = kCtrlNone;
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
};

struct GuestMsg {
    uint64_t hwnd = 0;
    uint32_t message = 0;
    uint64_t wparam = 0;
    uint64_t lparam = 0;
};

// Forward declarations for functions used before definition
Window* find_window(uint64_t hwnd);
bool is_control_hwnd(uint64_t hwnd);
void post_control_notify(uint64_t ctrl_hwnd, uint16_t code);
void post_button_command(uint64_t btn_hwnd);
int h_for_combobox(uint64_t hwnd);

// --- Globals (moved from anonymous namespace for shared library visibility) ---
std::mutex g_window_mutex;
std::condition_variable g_queue_cond;

std::map<std::string, WindowClass> g_classes;
std::map<uint64_t, Window> g_windows;
std::deque<GuestMsg> g_queue;

uint64_t g_next_hwnd = 0x1001;
bool g_quit_pending = false;
DWORD g_quit_exit_code = 0;
// DialogBoxParam/EndDialog handshake (modal loop result).
bool g_dialog_result_active = false;
int g_dialog_result = 0;

// --- Timers (SetTimer/KillTimer) ------------------------------------------
struct GuestTimerInfo {
    uint64_t hwnd = 0;
    uint32_t id = 0;
    uint32_t period_ms = 0;
    uint64_t due_ms = 0;
    uint32_t func = 0;
};

uint64_t steady_clock_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::unordered_map<uint64_t, GuestTimerInfo> g_timers; // key = (hwnd << 32) | id
std::condition_variable g_timer_cv;
std::thread g_timer_thread;
std::atomic<bool> g_timer_stop{false};
uint32_t g_next_timer_id = 0;

// --- X11 display backend globals ---------------------------------------
Display* g_x11_display = nullptr;
std::mutex g_x11_mutex;
std::thread g_x11_thread;
bool g_x11_started = false;
std::map<uint64_t, ::Window> g_x11_windows;   // guest hwnd -> X11 Window
std::map<::Window, uint64_t> g_x11_hwnds;     // X11 Window -> guest hwnd

namespace {

constexpr uint64_t kDesktopHwnd = 0x1000;

// X11 globals (defined in ThetaZ namespace, brought into scope for internal linkage)
using ThetaZ::g_x11_display;
using ThetaZ::g_x11_windows;
using ThetaZ::g_x11_hwnds;
using ThetaZ::g_x11_mutex;
using ThetaZ::g_x11_thread;
using ThetaZ::g_x11_started;

bool x11_display_init() {
    g_x11_display = XOpenDisplay(nullptr);
    if (!g_x11_display) {
        fprintf(stderr, "[x11] no display available; GUI disabled\n");
        return false;
    }
    g_x11_started = true;
    return true;
}

void timer_thread_main() {
    std::unique_lock<std::mutex> lock(g_window_mutex);
    while (!g_timer_stop) {
        uint64_t now = steady_clock_ms();
        uint64_t next_due = 0;
        bool fired = false;
        for (auto it = g_timers.begin(); it != g_timers.end();) {
            if (it->second.due_ms <= now) {
                GuestMsg m;
                m.hwnd = it->second.hwnd;
                m.message = WM_TIMER;
                m.wparam = it->second.id;
                m.lparam = it->second.func;
                g_queue.push_back(m);
                if (it->second.period_ms) {
                    it->second.due_ms = now + it->second.period_ms;
                } else {
                    it = g_timers.erase(it); // one-shot
                    continue;
                }
                fired = true;
            }
            uint64_t d = it->second.due_ms;
            if (!next_due || d < next_due) next_due = d;
            ++it;
        }
        if (fired) g_queue_cond.notify_all();
        if (g_timers.empty()) {
            g_timer_cv.wait(lock);
        } else {
            uint64_t wait_ms = next_due > now ? next_due - now : 0;
            g_timer_cv.wait_for(lock, std::chrono::milliseconds(wait_ms));
        }
    }
}

void x11_post_guest_msg(uint64_t hwnd, uint32_t message, uint64_t wparam, uint64_t lparam) {
    GuestMsg m;
    m.hwnd = hwnd;
    m.message = message;
    m.wparam = wparam;
    m.lparam = lparam;
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        g_queue.push_back(m);
    }
    g_queue_cond.notify_all();
}

// Minimal paint: clear the window and draw its title text so windows are visible.
// Controls draw their label too (button centered, checkbox with [x]/[ ] marker).
// Lock order: g_window_mutex (copy data) then g_x11_mutex (display calls);
// never the reverse, and never nested.
void x11_paint_window(uint64_t hwnd) {
    if (!g_x11_display) return;
    ::Window xw = 0;
    int w = 800, h = 600;
    std::string title = "ThetaZ";
    std::string text;                 // EDIT buffer / combo field
    std::vector<std::string> items;   // LISTBOX / COMBOBOX items
    uint32_t ctrl_type = kCtrlNone;
    bool checked = false;
    bool is_checkbox = false;
    int cur_sel = -1;
    int top_index = 0;
    bool has_focus = false;
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        auto it = g_x11_windows.find(hwnd);
        if (it == g_x11_windows.end()) return;
        xw = it->second;
        if (Window* win = find_window(hwnd)) {
            w = win->w; h = win->h;
            title = win->title.empty() ? "ThetaZ" : win->title;
            ctrl_type = win->ctrl_type;
            checked = win->checked;
            text = win->text;
            items = win->items;
            cur_sel = win->cur_sel;
            top_index = win->top_index;
            has_focus = win->has_focus;
            uint32_t bs = win->style & 0x3;
            is_checkbox = (bs >= BS_CHECKBOX && bs <= BS_AUTOCHECKBOX);
        }
    }
    std::lock_guard<std::mutex> lock(g_x11_mutex);
    unsigned long bg = BlackPixel(g_x11_display, DefaultScreen(g_x11_display));
    unsigned long fg = WhitePixel(g_x11_display, DefaultScreen(g_x11_display));
    XSetForeground(g_x11_display, DefaultGC(g_x11_display, 0), bg);
    XFillRectangle(g_x11_display, xw, DefaultGC(g_x11_display, 0), 0, 0, w, h);
    XSetForeground(g_x11_display, DefaultGC(g_x11_display, 0), fg);
    switch (ctrl_type) {
        case kCtrlButton:
            if (is_checkbox) {
                std::string label = checked ? "[x] " : "[ ] ";
                label += title;
                XDrawString(g_x11_display, xw, DefaultGC(g_x11_display, 0), 8, h / 2,
                            label.c_str(), static_cast<int>(label.size()));
            } else {
                int tx = (w - static_cast<int>(title.size()) * 7) / 2;
                if (tx < 4) tx = 4;
                XDrawString(g_x11_display, xw, DefaultGC(g_x11_display, 0), tx, h / 2,
                            title.c_str(), static_cast<int>(title.size()));
            }
            break;
        case kCtrlEdit: {
            // Border + single-line text buffer.
            XDrawRectangle(g_x11_display, xw, DefaultGC(g_x11_display, 0), 0, 0, w - 1, h - 1);
            int cy = h / 2;
            std::string shown = text.empty() ? " " : text;
            XDrawString(g_x11_display, xw, DefaultGC(g_x11_display, 0), 4, cy,
                        shown.c_str(), static_cast<int>(shown.size()));
            if (has_focus) {
                XDrawLine(g_x11_display, xw, DefaultGC(g_x11_display, 0),
                          4 + static_cast<int>(shown.size()) * 7, cy - 4,
                          4 + static_cast<int>(shown.size()) * 7, cy + 3);
            }
            break;
        }
        case kCtrlListbox: {
            // Vertical item list, top_index first, highlight cur_sel.
            XDrawRectangle(g_x11_display, xw, DefaultGC(g_x11_display, 0), 0, 0, w - 1, h - 1);
            int line_h = 16;
            int y = 14;
            size_t idx = static_cast<size_t>(top_index);
            while (idx < items.size() && y < h) {
                if (static_cast<int>(idx) == cur_sel) {
                    XSetForeground(g_x11_display, DefaultGC(g_x11_display, 0), fg);
                    XFillRectangle(g_x11_display, xw, DefaultGC(g_x11_display, 0),
                                   1, y - 10, w - 2, line_h);
                    XSetForeground(g_x11_display, DefaultGC(g_x11_display, 0), bg);
                }
                XDrawString(g_x11_display, xw, DefaultGC(g_x11_display, 0), 4, y,
                            items[idx].c_str(), static_cast<int>(items[idx].size()));
                if (static_cast<int>(idx) == cur_sel)
                    XSetForeground(g_x11_display, DefaultGC(g_x11_display, 0), fg);
                y += line_h;
                ++idx;
            }
            break;
        }
        case kCtrlCombobox: {
            // Field showing current selection text + a drop arrow.
            XDrawRectangle(g_x11_display, xw, DefaultGC(g_x11_display, 0), 0, 0, w - 1, h - 1);
            std::string shown;
            if (cur_sel >= 0 && cur_sel < static_cast<int>(items.size()))
                shown = items[static_cast<size_t>(cur_sel)];
            if (shown.empty()) shown = text;
            XDrawString(g_x11_display, xw, DefaultGC(g_x11_display, 0), 4, h / 2,
                        shown.c_str(), static_cast<int>(shown.size()));
            // Drop arrow on the right.
            int ax = w - 14;
            XDrawLine(g_x11_display, xw, DefaultGC(g_x11_display, 0), ax, h / 2 - 3,
                      ax + 8, h / 2 - 3);
            XDrawLine(g_x11_display, xw, DefaultGC(g_x11_display, 0), ax + 2, h / 2,
                      ax + 6, h / 2);
            XDrawLine(g_x11_display, xw, DefaultGC(g_x11_display, 0), ax + 3, h / 2 + 2,
                      ax + 5, h / 2 + 2);
            break;
        }
        default:
            XDrawString(g_x11_display, xw, DefaultGC(g_x11_display, 0), 10, 20,
                        title.c_str(), static_cast<int>(title.size()));
            break;
    }
    XFlush(g_x11_display);
}

void x11_event_loop() {
    while (g_x11_display) {
        while (XPending(g_x11_display) == 0) {
            usleep(5000);
        }
        XEvent ev;
        XNextEvent(g_x11_display, &ev);
        uint64_t hwnd = 0;
        {
            std::lock_guard<std::mutex> lock(g_window_mutex);
            auto hit = g_x11_hwnds.find(ev.xany.window);
            if (hit != g_x11_hwnds.end()) hwnd = hit->second;
        }
        if (!hwnd) continue;
        switch (ev.type) {
            case Expose:
                if (ev.xexpose.count == 0) {
                    x11_paint_window(hwnd);
                    x11_post_guest_msg(hwnd, WM_PAINT, 0, 0);
                }
                break;
            case ButtonPress:
                // Controls handle their own mouse input: buttons/checkboxes
                // act on ButtonRelease. Raw LBUTTONDOWN only goes to real
                // (guest WNDPROC) windows.
                if (is_control_hwnd(hwnd)) break;
                x11_post_guest_msg(hwnd, WM_LBUTTONDOWN,
                                   (ev.xbutton.button == 3) ? 2u : 1u,
                                   (static_cast<uint64_t>(ev.xbutton.y) << 16) | ev.xbutton.x);
                break;
            case ButtonRelease: {
                if (is_control_hwnd(hwnd)) {
                    uint32_t style = 0;
                    uint32_t ctrl_type = kCtrlNone;
                    {
                        std::lock_guard<std::mutex> lock(g_window_mutex);
                        Window* win = find_window(hwnd);
                        if (win) {
                            style = win->style;
                            ctrl_type = win->ctrl_type;
                            if (ctrl_type == kCtrlButton &&
                                (style & 0x3) >= BS_CHECKBOX &&
                                (style & 0x3) <= BS_AUTOCHECKBOX) {
                                win->checked = !win->checked;
                            }
                        }
                    }
                    if (ctrl_type == kCtrlButton) {
                        if ((style & 0x3) >= BS_CHECKBOX && (style & 0x3) <= BS_AUTOCHECKBOX) {
                            x11_paint_window(hwnd); // refresh [x]/[ ] marker
                        }
                        post_button_command(hwnd);
                        break;
                    }
                    if (ctrl_type == kCtrlListbox) {
                        // Hit-test the clicked row against items.
                        int clicked = -1;
                        {
                            std::lock_guard<std::mutex> lock(g_window_mutex);
                            Window* win = find_window(hwnd);
                            if (win) {
                                int row = (ev.xbutton.y - 4) / 16;
                                int idx = win->top_index + row;
                                if (row >= 0 && idx < static_cast<int>(win->items.size()))
                                    clicked = idx;
                            }
                        }
                        if (clicked >= 0) {
                            bool notify = false;
                            {
                                std::lock_guard<std::mutex> lock(g_window_mutex);
                                Window* win = find_window(hwnd);
                                if (win) {
                                    if (win->cur_sel != clicked) {
                                        win->cur_sel = clicked;
                                        notify = true;
                                    }
                                }
                            }
                            x11_paint_window(hwnd);
                            if (notify && (style & LBS_NOTIFY))
                                post_control_notify(hwnd, LBN_SELCHANGE);
                        }
                        break;
                    }
                    if (ctrl_type == kCtrlCombobox) {
                        // Click anywhere toggles the dropdown; clicking a row
                        // while open selects it.
                        bool was_dropped = false;
                        {
                            std::lock_guard<std::mutex> lock(g_window_mutex);
                            Window* win = find_window(hwnd);
                            if (win) {
                                was_dropped = win->dropped;
                                if (!win->dropped) win->dropped = true;
                            }
                        }
                        if (was_dropped) {
                            int row = (ev.xbutton.y - (h_for_combobox(hwnd))) / 16;
                            int picked = -1;
                            {
                                std::lock_guard<std::mutex> lock(g_window_mutex);
                                Window* win = find_window(hwnd);
                                if (win) {
                                    if (row >= 0 && row < static_cast<int>(win->items.size())) {
                                        picked = row;
                                        win->dropped = false;
                                        win->cur_sel = picked;
                                    } else {
                                        win->dropped = false;
                                    }
                                }
                            }
                            x11_paint_window(hwnd);
                            if (picked >= 0) post_control_notify(hwnd, CBN_SELCHANGE);
                        } else {
                            x11_paint_window(hwnd);
                        }
                        break;
                    }
                    break;
                }
                x11_post_guest_msg(hwnd, WM_LBUTTONUP,
                                   (ev.xbutton.button == 3) ? 2u : 1u,
                                   (static_cast<uint64_t>(ev.xbutton.y) << 16) | ev.xbutton.x);
                break;
            }
            case KeyPress: {
                char buf[2];
                KeySym ks;
                int n = XLookupString(&ev.xkey, buf, sizeof(buf), &ks, nullptr);
                // Route printable input to a focused EDIT control.
                if (n > 0 && static_cast<unsigned char>(buf[0]) >= 32) {
                    uint64_t edit_target = 0;
                    {
                        std::lock_guard<std::mutex> lock(g_window_mutex);
                        Window* win = find_window(hwnd);
                        if (win && win->ctrl_type == kCtrlEdit) edit_target = hwnd;
                    }
                    if (edit_target) {
                        bool en_change = false;
                        {
                            std::lock_guard<std::mutex> lock(g_window_mutex);
                            Window* win = find_window(edit_target);
                            if (win && !win->read_only) {
                                if (win->text.size() < win->max_text) {
                                    win->text.push_back(static_cast<char>(buf[0]));
                                    win->sel_start = win->sel_end =
                                        static_cast<int>(win->text.size());
                                    en_change = true;
                                }
                            }
                        }
                        x11_paint_window(edit_target);
                        if (en_change) post_control_notify(edit_target, EN_CHANGE);
                        break;
                    }
                }
                x11_post_guest_msg(hwnd, WM_KEYDOWN, static_cast<uint64_t>(ks), 0);
                if (n > 0 && static_cast<unsigned char>(buf[0]) >= 32) {
                    x11_post_guest_msg(hwnd, WM_CHAR, static_cast<unsigned char>(buf[0]), 0);
                }
                break;
            }
            case ClientMessage:
                x11_post_guest_msg(hwnd, WM_CLOSE, 0, 0);
                break;
            case ConfigureNotify:
                {
                    std::lock_guard<std::mutex> lock(g_window_mutex);
                    Window* win = find_window(hwnd);
                    if (win) { win->w = ev.xconfigure.width; win->h = ev.xconfigure.height; }
                }
                break;
            default:
                break;
        }
    }
}

// Guest WNDPROC: LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM).
// 32-bit guests call this through the interpreter; 64-bit via the host ABI.
uint64_t call_wnd_proc(uint64_t wnd_proc, uint64_t hwnd, uint32_t message,
                       uint64_t wparam, uint64_t lparam) {
    if (!wnd_proc) return 0;
    if (wnd_proc < 0x100000000ull) {
        uint32_t args[4] = { static_cast<uint32_t>(hwnd), message,
                             static_cast<uint32_t>(wparam), static_cast<uint32_t>(lparam) };
        return x86_call_guest_args(static_cast<uint32_t>(wnd_proc), args, 4);
    }
    typedef uint64_t (__attribute__((ms_abi)) *WndProcFn)(
        uint64_t hwnd, uint32_t message, uint64_t wparam, uint64_t lparam);
    WndProcFn fn = reinterpret_cast<WndProcFn>(wnd_proc);
    return fn(hwnd, message, wparam, lparam);
}

bool is_control_hwnd(uint64_t hwnd) {
    Window* win = find_window(hwnd);
    return win && win->ctrl_type != kCtrlNone;
}

// A (push)button click or checkbox toggle posts WM_COMMAND to the parent:
// wparam = MAKEWPARAM(id, BN_CLICKED=0), lparam = control hwnd.
void post_button_command(uint64_t btn_hwnd) {
    uint64_t parent = 0;
    uint32_t id = 0;
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        Window* win = find_window(btn_hwnd);
        if (!win) return;
        parent = win->parent;
        id = win->id;
    }
    if (!parent) return;
    GuestMsg m;
    m.hwnd = parent;
    m.message = WM_COMMAND;
    m.wparam = id;
    m.lparam = btn_hwnd;
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        g_queue.push_back(m);
    }
    g_queue_cond.notify_all();
}

int h_for_combobox(uint64_t hwnd) {
    std::lock_guard<std::mutex> lock(g_window_mutex);
    Window* win = find_window(hwnd);
    return win ? (win->h ? win->h : 24) : 24;
}

// A control notification (LBN_SELCHANGE, CBN_SELCHANGE, EN_CHANGE, ...) posts
// WM_COMMAND to the parent: wparam = MAKEWPARAM(id, code), lparam = control.
void post_control_notify(uint64_t ctrl_hwnd, uint16_t code) {
    uint64_t parent = 0;
    uint32_t id = 0;
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        Window* win = find_window(ctrl_hwnd);
        if (!win) return;
        parent = win->parent;
        id = win->id;
    }
    if (!parent) return;
    GuestMsg m;
    m.hwnd = parent;
    m.message = WM_COMMAND;
    m.wparam = (static_cast<uint64_t>(code) << 16) | id;
    m.lparam = ctrl_hwnd;
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        g_queue.push_back(m);
    }
    g_queue_cond.notify_all();
}

uint64_t get_current_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
}

void write_guest_msg(uint8_t* dst, const GuestMsg& m) {
    *reinterpret_cast<uint32_t*>(dst + MSG_HWND) = static_cast<uint32_t>(m.hwnd);
    *reinterpret_cast<uint32_t*>(dst + MSG_MESSAGE) = m.message;
    *reinterpret_cast<uint32_t*>(dst + MSG_WPARAM) = static_cast<uint32_t>(m.wparam);
    *reinterpret_cast<uint32_t*>(dst + MSG_LPARAM) = static_cast<uint32_t>(m.lparam);
    *reinterpret_cast<uint32_t*>(dst + MSG_TIME) =
        static_cast<uint32_t>(get_current_ms() & 0xFFFFFFFF);
    *reinterpret_cast<uint32_t*>(dst + MSG_PT) = 0;     // pt.x
    *reinterpret_cast<uint32_t*>(dst + MSG_PT + 4) = 0; // pt.y
}

// 64-bit version: writes to x64 MSG structure layout
void write_guest_msg64(uint8_t* dst, const GuestMsg& m) {
    *reinterpret_cast<uint64_t*>(dst + MSG64_HWND) = m.hwnd;
    *reinterpret_cast<uint32_t*>(dst + MSG64_MESSAGE) = m.message;
    *reinterpret_cast<uint32_t*>(dst + MSG64_MESSAGE + 4) = 0; // padding
    *reinterpret_cast<uint64_t*>(dst + MSG64_WPARAM) = m.wparam;
    *reinterpret_cast<uint64_t*>(dst + MSG64_LPARAM) = m.lparam;
    *reinterpret_cast<uint32_t*>(dst + MSG64_TIME) = static_cast<uint32_t>(get_current_ms() & 0xFFFFFFFF);
    *reinterpret_cast<uint32_t*>(dst + MSG64_TIME + 4) = 0; // padding
    *reinterpret_cast<int64_t*>(dst + MSG64_PT) = 0;        // pt.x
    *reinterpret_cast<int64_t*>(dst + MSG64_PT + 8) = 0;    // pt.y
}

} // namespace

// --- Outer ThetaZ helpers (visible to both inner and outer) ----------------
Window* find_window(uint64_t hwnd) {
    auto it = g_windows.find(hwnd);
    return it == g_windows.end() ? nullptr : &it->second;
}

void theta_register_dc_hwnd(uintptr_t dc, uint64_t hwnd) {
    (void)dc; (void)hwnd;
}

bool is_control_hwnd(uint64_t hwnd) {
    Window* w = find_window(hwnd);
    return w && w->ctrl_type != 0;
}

void post_control_notify(uint64_t ctrl_hwnd, uint16_t code) {
    Window* ctrl = find_window(ctrl_hwnd);
    if (!ctrl) return;
    uint64_t parent = ctrl->parent;
    if (!parent) return;
    uint32_t id = ctrl->id;
    GuestMsg m;
    m.hwnd = parent;
    m.message = WM_COMMAND;
    m.wparam = (static_cast<uint64_t>(code) << 16) | id;
    m.lparam = ctrl_hwnd;
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        g_queue.push_back(m);
    }
    g_queue_cond.notify_all();
}

void post_button_command(uint64_t btn_hwnd) {
    Window* w = find_window(btn_hwnd);
    if (!w) return;
    uint64_t parent = w->parent;
    if (!parent) return;
    uint32_t id = w->id;
    GuestMsg m;
    m.hwnd = parent;
    m.message = WM_COMMAND;
    m.wparam = id;
    m.lparam = btn_hwnd;
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        g_queue.push_back(m);
    }
    g_queue_cond.notify_all();
}

int h_for_combobox(uint64_t hwnd) {
    (void)hwnd; return 0;
}

// --- user32.dll: window management and messages -----------------------------

static uint16_t WINAPI_CALL THETAZ_RegisterClassA(const void* wc) {
    if (!wc) return 0;
    const uint8_t* c = static_cast<const uint8_t*>(wc);
    // 32-bit guest struct: pointer fields are 4 bytes, never host 8-byte
    // pointer reads (would slurp the neighbouring dword).
    uint32_t name_addr = *reinterpret_cast<const uint32_t*>(c + WNDCLASSA_LPSZCLASSNAME);
    LPCSTR name = reinterpret_cast<LPCSTR>(static_cast<uintptr_t>(name_addr));
    if (!name || name[0] == '\0') return 0;

    WindowClass cls;
    cls.name = name;
    cls.wnd_proc = *reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(c + WNDCLASSA_WNDPROC));
    cls.style = *reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(c + WNDCLASSA_STYLE));
    cls.hinstance = *reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(c + WNDCLASSA_HINSTANCE));

    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        g_classes[cls.name] = cls;
    }
    fprintf(stderr, "[user32] RegisterClassA: \"%s\" wndproc=0x%lX\n", name, cls.wnd_proc);
    return static_cast<uint16_t>(g_classes.size());
}

static std::string wcls_to_a(const uint16_t* ws) {
    std::string r;
    if (!ws) return r;
    for (int i = 0; i < 256 && ws[i]; i++) r.push_back(static_cast<char>(ws[i] & 0xFF));
    return r;
}

static uint16_t WINAPI_CALL THETAZ_RegisterClassW(const void* wc) {
    if (!wc) return 0;
    const uint8_t* c = static_cast<const uint8_t*>(wc);
    uint32_t name_addr = *reinterpret_cast<const uint32_t*>(c + WNDCLASSA_LPSZCLASSNAME);
    const uint16_t* wname = reinterpret_cast<const uint16_t*>(static_cast<uintptr_t>(name_addr));
    std::string name = wcls_to_a(wname);
    if (name.empty()) return 0;
    WindowClass cls;
    cls.name = name;
    cls.wnd_proc = *reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(c + WNDCLASSA_WNDPROC));
    cls.style = *reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(c + WNDCLASSA_STYLE));
    cls.hinstance = *reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(c + WNDCLASSA_HINSTANCE));
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        g_classes[cls.name] = cls;
    }
    fprintf(stderr, "[user32] RegisterClassW: \"%s\" wndproc=0x%lX\n", name.c_str(), cls.wnd_proc);
    return static_cast<uint16_t>(g_classes.size());
}

static uint16_t WINAPI_CALL THETAZ_RegisterClassExA(const void* wc) {
    if (!wc) return 0;
    const uint8_t* c = static_cast<const uint8_t*>(wc);
    uint32_t name_addr = *reinterpret_cast<const uint32_t*>(c + WNDCLASSA_LPSZCLASSNAME + 4);
    LPCSTR name = reinterpret_cast<LPCSTR>(static_cast<uintptr_t>(name_addr));
    if (!name || name[0] == '\0') return 0;
    WindowClass cls;
    cls.name = name;
    cls.wnd_proc = *reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(c + WNDCLASSA_WNDPROC + 4));
    cls.style = *reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(c + WNDCLASSA_STYLE + 4));
    cls.hinstance = *reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(c + WNDCLASSA_HINSTANCE + 4));
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        g_classes[cls.name] = cls;
    }
    fprintf(stderr, "[user32] RegisterClassExA: \"%s\" wndproc=0x%lX\n", name, cls.wnd_proc);
    return static_cast<uint16_t>(g_classes.size());
}

static uint16_t WINAPI_CALL THETAZ_RegisterClassExW(const void* wc) {
    if (!wc) return 0;
    const uint8_t* c = static_cast<const uint8_t*>(wc);
    uint32_t name_addr = *reinterpret_cast<const uint32_t*>(c + WNDCLASSA_LPSZCLASSNAME + 4);
    const uint16_t* wname = reinterpret_cast<const uint16_t*>(static_cast<uintptr_t>(name_addr));
    std::string name = wcls_to_a(wname);
    if (name.empty()) return 0;
    WindowClass cls;
    cls.name = name;
    cls.wnd_proc = *reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(c + WNDCLASSA_WNDPROC + 4));
    cls.style = *reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(c + WNDCLASSA_STYLE + 4));
    cls.hinstance = *reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(c + WNDCLASSA_HINSTANCE + 4));
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        g_classes[cls.name] = cls;
    }
    fprintf(stderr, "[user32] RegisterClassExW: \"%s\" wndproc=0x%lX\n", name.c_str(), cls.wnd_proc);
    return static_cast<uint16_t>(g_classes.size());
}

static void* WINAPI_CALL THETAZ_GetDesktopWindow() {
    return reinterpret_cast<WinHandle>(kDesktopHwnd);
}

// Predefined user32 control classes are case-insensitive and have NO guest
// WNDPROC; their behavior lives in this layer (buttons post WM_COMMAND, etc).
static uint32_t lookup_control_class(const char* name) {
    if (!name) return kCtrlNone;
    std::string n(name);
    for (auto& c : n) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    if (n == "BUTTON") return kCtrlButton;
    if (n == "STATIC") return kCtrlStatic;
    if (n == "EDIT") return kCtrlEdit;
    if (n == "LISTBOX") return kCtrlListbox;
    if (n == "COMBOBOX") return kCtrlCombobox;
    return kCtrlNone;
}

static WinHandle WINAPI_CALL THETAZ_CreateWindowExA(DWORD ex_style, LPCSTR class_name, LPCSTR window_name,
                                                    DWORD style, int x, int y, int w, int h,
                                                    WinHandle parent, WinHandle menu, HINSTANCE instance, LPVOID param) {
    (void)parent;
    (void)menu;
    if (!class_name) return nullptr;

    uint32_t ctrl_type = lookup_control_class(class_name);
    WindowClass* cls = nullptr;
    uint64_t wnd_proc = 0;
    if (!ctrl_type) {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        auto it = g_classes.find(class_name);
        if (it != g_classes.end()) {
            cls = &it->second;
            wnd_proc = cls->wnd_proc;
        }
        if (!cls) {
            fprintf(stderr, "[user32] CreateWindowExA: class \"%s\" not registered\n", class_name);
            return nullptr;
        }
    }

    uint64_t hwnd;
    uint64_t parent_hwnd = parent ? reinterpret_cast<uint64_t>(parent) : 0;
    uint32_t ctrl_id = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(menu)) & 0xFFFF;
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        hwnd = g_next_hwnd++;
        Window win;
        win.hwnd = hwnd;
        win.wnd_proc = wnd_proc;
        win.class_name = ctrl_type ? class_name : cls->name;
        win.title = window_name ? window_name : "";
        win.style = style;
        win.ex_style = ex_style;
        win.x = x;
        win.y = y;
        win.w = w;
        win.h = h;
        win.parent = parent_hwnd;
        win.id = ctrl_id;
        win.ctrl_type = ctrl_type;
        win.visible = (style & WS_VISIBLE) != 0;
        if (ctrl_type == kCtrlEdit || ctrl_type == kCtrlCombobox) {
            win.text = window_name ? window_name : "";
        }
        if (ctrl_type == kCtrlListbox) {
            win.multi_sel = (style & (LBS_MULTIPLESEL | LBS_EXTENDEDSEL)) != 0;
        }
        if (ctrl_type == kCtrlEdit) {
            win.read_only = (style & ES_READONLY) != 0;
        }
        g_windows[hwnd] = win;
    }

    // Create a real X11 window so the GUI is visible and interactive.
    if (x11_display_init() && g_x11_display) {
        ::Window parent_xw = DefaultRootWindow(g_x11_display);
        if (parent_hwnd) {
            std::lock_guard<std::mutex> lock(g_window_mutex);
            auto pit = g_x11_windows.find(parent_hwnd);
            if (pit != g_x11_windows.end()) parent_xw = pit->second;
        }
        ::Window xw = XCreateSimpleWindow(
            g_x11_display, parent_xw,
            x, y, w > 0 ? w : 800, h > 0 ? h : 600, 0,
            BlackPixel(g_x11_display, DefaultScreen(g_x11_display)),
            WhitePixel(g_x11_display, DefaultScreen(g_x11_display)));
        XSelectInput(g_x11_display, xw,
                     ExposureMask | ButtonPressMask | ButtonReleaseMask |
                     (ctrl_type ? 0 : KeyPressMask | StructureNotifyMask));
        if (!ctrl_type) {
            XStoreName(g_x11_display, xw, window_name ? window_name : "ThetaZ");
            Atom wm_delete = XInternAtom(g_x11_display, "WM_DELETE_WINDOW", False);
            XSetWMProtocols(g_x11_display, xw, &wm_delete, 1);
        }
        {
            std::lock_guard<std::mutex> lock(g_window_mutex);
            g_x11_windows[hwnd] = xw;
            g_x11_hwnds[xw] = hwnd;
        }
    }

    fprintf(stderr, "[user32] CreateWindowExA: hwnd=0x%lX \"%s\" (%s) [%d,%d %dx%d]%s\n",
           hwnd, class_name, window_name ? window_name : "", x, y, w, h,
           ctrl_type ? " [control]" : "");

    // CREATESTRUCT the guest WNDPROC sees during WM_CREATE.
    thread_local uint8_t cs[CREATESTRUCT_SIZE];
    thread_local char title_buf[256];
    thread_local char class_buf[256];
    memset(cs, 0, sizeof(cs));
    strncpy(title_buf, window_name ? window_name : "", sizeof(title_buf) - 1);
    strncpy(class_buf, class_name, sizeof(class_buf) - 1);

    *reinterpret_cast<uint32_t*>(cs + CREATESTRUCT_PARAMS) = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(param));
    *reinterpret_cast<uint32_t*>(cs + CREATESTRUCT_INSTANCE) = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(instance));
    *reinterpret_cast<uint32_t*>(cs + CREATESTRUCT_MENU) = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(menu));
    *reinterpret_cast<uint32_t*>(cs + CREATESTRUCT_PARENT) = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(parent));
    *reinterpret_cast<int*>(cs + CREATESTRUCT_CY) = h;
    *reinterpret_cast<int*>(cs + CREATESTRUCT_CX) = w;
    *reinterpret_cast<int*>(cs + CREATESTRUCT_Y) = y;
    *reinterpret_cast<int*>(cs + CREATESTRUCT_X) = x;
    *reinterpret_cast<uint32_t*>(cs + CREATESTRUCT_STYLE) = style;
    *reinterpret_cast<uint32_t*>(cs + CREATESTRUCT_NAME) = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(title_buf));
    *reinterpret_cast<uint32_t*>(cs + CREATESTRUCT_CLASS) = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(class_buf));
    *reinterpret_cast<uint32_t*>(cs + CREATESTRUCT_EXSTYLE) = ex_style;

    uint64_t result = call_wnd_proc(wnd_proc, hwnd, WM_CREATE, 0, reinterpret_cast<uint64_t>(cs));
    if (result == static_cast<uint64_t>(-1)) {
        // WM_CREATE returned -1: creation failed, window is destroyed.
        std::lock_guard<std::mutex> lock(g_window_mutex);
        g_windows.erase(hwnd);
        return nullptr;
    }

    return reinterpret_cast<WinHandle>(hwnd);
}

static BOOL WINAPI_CALL THETAZ_DestroyWindow(WinHandle hwnd) {
    uint64_t h = reinterpret_cast<uint64_t>(hwnd);
    uint64_t wnd_proc = 0;
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        Window* win = find_window(h);
        if (!win) return FALSE_VALUE;
        wnd_proc = win->wnd_proc;
    }
    call_wnd_proc(wnd_proc, h, WM_DESTROY, 0, 0);
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        // Destroy children (X11 subwindows die with the parent, but our
        // registry needs cleaning).
        for (auto it = g_windows.begin(); it != g_windows.end();) {
            if (it->second.parent == h) {
                auto xit = g_x11_windows.find(it->first);
                if (xit != g_x11_windows.end()) {
                    g_x11_hwnds.erase(xit->second);
                    g_x11_windows.erase(xit);
                }
                it = g_windows.erase(it);
            } else {
                ++it;
            }
        }
        g_windows.erase(h);
        auto xit = g_x11_windows.find(h);
        if (xit != g_x11_windows.end()) {
            g_x11_hwnds.erase(xit->second);
            g_x11_windows.erase(xit);
        }
    }
    return TRUE_VALUE;
}

static BOOL WINAPI_CALL THETAZ_ShowWindow(WinHandle hwnd, int cmd_show) {
    uint64_t h = reinterpret_cast<uint64_t>(hwnd);
    ::Window xw = 0;
    bool was_visible;
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        Window* win = find_window(h);
        if (!win) return FALSE_VALUE;
        was_visible = win->visible;
        win->visible = (cmd_show != 0);
        auto it = g_x11_windows.find(h);
        if (it != g_x11_windows.end()) xw = it->second;
    }
    if (g_x11_display && xw) {
        std::lock_guard<std::mutex> lock(g_x11_mutex);
        if (cmd_show == 0) {
            XUnmapWindow(g_x11_display, xw);
        } else {
            XMapRaised(g_x11_display, xw);
        }
        XFlush(g_x11_display);
    }
    return was_visible ? TRUE_VALUE : FALSE_VALUE;
}

static BOOL WINAPI_CALL THETAZ_UpdateWindow(WinHandle hwnd) {
    (void)hwnd;
    return TRUE_VALUE;
}

static BOOL WINAPI_CALL THETAZ_InvalidateRect(WinHandle hwnd, const void* rect, BOOL erase) {
    (void)erase;
    std::lock_guard<std::mutex> lock(g_window_mutex);
    Window* win = find_window(reinterpret_cast<uint64_t>(hwnd));
    if (!win) return FALSE_VALUE;
    win->dirty = true;
    if (rect) {
        const int* r = static_cast<const int*>(rect);
        win->inv_left = r[0];
        win->inv_top = r[1];
        win->inv_right = r[2];
        win->inv_bottom = r[3];
    } else {
        win->inv_left = 0;
        win->inv_top = 0;
        win->inv_right = win->w;
        win->inv_bottom = win->h;
    }
    return TRUE_VALUE;
}

// Returns a (fake) DC so the guest can paint; actual drawing no-ops until the
// GDI object model exists (E4). Fills PAINTSTRUCT like a real BeginPaint.
static void* WINAPI_CALL THETAZ_BeginPaint(WinHandle hwnd, void* ps) {
    if (!ps) return reinterpret_cast<void*>(0x1);
    uint8_t* out = static_cast<uint8_t*>(ps);
    std::lock_guard<std::mutex> lock(g_window_mutex);
    Window* win = find_window(reinterpret_cast<uint64_t>(hwnd));
    int left = 0, top = 0, right = 0, bottom = 0;
    if (win) {
        left = win->inv_left;
        top = win->inv_top;
        right = win->inv_right ? win->inv_right : win->w;
        bottom = win->inv_bottom ? win->inv_bottom : win->h;
        win->dirty = false;
    }
    theta_register_dc_hwnd(0x1, reinterpret_cast<uint64_t>(hwnd));
    *reinterpret_cast<uint32_t*>(out + PAINTSTRUCT_HDC) = 0x1; // fake DC
    *reinterpret_cast<uint32_t*>(out + PAINTSTRUCT_FERASE) = 1;
    int* rc = reinterpret_cast<int*>(out + PAINTSTRUCT_RCPAINT);
    rc[0] = left;
    rc[1] = top;
    rc[2] = right;
    rc[3] = bottom;
    return reinterpret_cast<void*>(0x1);
}

static BOOL WINAPI_CALL THETAZ_EndPaint(WinHandle hwnd, const void* ps) {
    (void)hwnd;
    (void)ps;
    return TRUE_VALUE;
}

static void WINAPI_CALL THETAZ_PostQuitMessage(int exit_code) {
    printf("[user32] PostQuitMessage(%d)\n", exit_code);
    std::lock_guard<std::mutex> lock(g_window_mutex);
    g_quit_pending = true;
    g_quit_exit_code = static_cast<DWORD>(exit_code);
    GuestMsg m;
    m.hwnd = 0;
    m.message = WM_QUIT;
    m.wparam = static_cast<uint64_t>(static_cast<uint32_t>(exit_code));
    g_queue.push_back(m);
    g_queue_cond.notify_all();
}

static BOOL WINAPI_CALL THETAZ_PostMessageA(WinHandle hwnd, DWORD message, uint64_t wparam, uint64_t lparam) {
    GuestMsg m;
    m.hwnd = hwnd ? reinterpret_cast<uint64_t>(hwnd) : kDesktopHwnd;
    m.message = message;
    m.wparam = wparam;
    m.lparam = lparam;
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        g_queue.push_back(m);
    }
    g_queue_cond.notify_all();
    return TRUE_VALUE;
}

static uint64_t WINAPI_CALL THETAZ_SendMessageA(WinHandle hwnd, DWORD message, uint64_t wparam, uint64_t lparam) {
    uint64_t h = hwnd ? reinterpret_cast<uint64_t>(hwnd) : kDesktopHwnd;
    uint64_t wnd_proc = 0;
    bool repaint = false;
    uint64_t click_parent = 0;
    uint32_t click_id = 0;

    // Handle control-class messages synchronously in this layer.
    uint32_t ctrl_type = kCtrlNone;
    uint64_t result = 0;
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        Window* win = find_window(h);
        if (win) {
            wnd_proc = win->wnd_proc;
            ctrl_type = win->ctrl_type;
            // Generic text/enable messages apply to every window.
            switch (message) {
                case WM_SETTEXT: {
                    const char* s = reinterpret_cast<const char*>(
                        static_cast<uintptr_t>(lparam));
                    win->text = s ? s : "";
                    win->title = win->text;
                    if (win->ctrl_type == kCtrlCombobox && win->cur_sel >= 0 &&
                        win->cur_sel < static_cast<int>(win->items.size()))
                        win->items[static_cast<size_t>(win->cur_sel)] = win->text;
                    repaint = true;
                    result = 1;
                    break;
                }
                case WM_GETTEXT: {
                    char* dst = reinterpret_cast<char*>(
                        static_cast<uintptr_t>(lparam));
                    uint32_t n = static_cast<uint32_t>(wparam);
                    if (dst && n) {
                        const std::string& src = win->text.empty() ? win->title : win->text;
                        size_t len = std::min(src.size(), static_cast<size_t>(n - 1));
                        memcpy(dst, src.c_str(), len);
                        dst[len] = '\0';
                        result = len;
                    } else result = 0;
                    break;
                }
                case WM_GETTEXTLENGTH: {
                    const std::string& src = win->text.empty() ? win->title : win->text;
                    result = src.size();
                    break;
                }
                case WM_ENABLE:
                    win->enabled = (wparam != 0);
                    repaint = true;
                    result = TRUE_VALUE;
                    break;
                case WM_SETFOCUS:
                    if (win->ctrl_type != kCtrlNone) {
                        win->has_focus = true;
                        repaint = true;
                    }
                    result = 0;
                    break;
                case WM_KILLFOCUS:
                    if (win->ctrl_type != kCtrlNone) {
                        win->has_focus = false;
                        repaint = true;
                    }
                    result = 0;
                    break;
                default:
                    break;
            }
            if (ctrl_type != kCtrlNone) {
                switch (ctrl_type) {
                    case kCtrlButton:
                        switch (message) {
                            case BM_GETCHECK:
                                return win->checked ? 1 : 0;
                            case BM_SETCHECK:
                                win->checked = (wparam != 0);
                                repaint = true;
                                break;
                            case BM_CLICK:
                                click_parent = win->parent;
                                click_id = win->id;
                                break;
                            default:
                                break;
                        }
                        break;
                    case kCtrlEdit:
                        switch (message) {
                            case EM_GETTEXTLEN:
                                return win->text.size();
                            case EM_GETTEXT: {
                                char* dst = reinterpret_cast<char*>(
                                    static_cast<uintptr_t>(lparam));
                                uint32_t n = static_cast<uint32_t>(wparam);
                                if (dst && n) {
                                    size_t len = std::min(win->text.size(), static_cast<size_t>(n - 1));
                                    memcpy(dst, win->text.c_str(), len);
                                    dst[len] = '\0';
                                    return len;
                                }
                                return 0;
                            }
                            case EM_SETLIMITTEXT:
                                win->max_text = wparam;
                                return 1;
                            case EM_SETREADONLY:
                                win->read_only = (wparam != 0);
                                return 1;
                            case EM_SETSEL: {
                                int start = static_cast<int>(wparam & 0xFFFF);
                                int end = static_cast<int>(lparam & 0xFFFF);
                                if (wparam == 0xFFFFFFFF && lparam == 0xFFFFFFFF) {
                                    start = end = static_cast<int>(win->text.size());
                                }
                                win->sel_start = start;
                                win->sel_end = end;
                                return 1;
                            }
                            case EM_GETSEL: {
                                uint32_t* sel = reinterpret_cast<uint32_t*>(
                                    static_cast<uintptr_t>(lparam));
                                if (sel) {
                                    sel[0] = static_cast<uint32_t>(win->sel_start);
                                    sel[1] = static_cast<uint32_t>(win->sel_end);
                                }
                                return (static_cast<uint64_t>(win->sel_end) << 16) |
                                       static_cast<uint64_t>(win->sel_start);
                            }
                            case EM_REPLACESEL: {
                                if (win->read_only) return 0;
                                const char* s = reinterpret_cast<const char*>(
                                    static_cast<uintptr_t>(lparam));
                                if (!s) return 0;
                                std::string repl(s);
                                int a = std::max(0, win->sel_start);
                                int b = std::max(a, win->sel_end);
                                if (a < 0) a = 0;
                                b = std::min(static_cast<int>(win->text.size()), b);
                                std::string before = win->text.substr(0, static_cast<size_t>(a));
                                std::string after = win->text.substr(static_cast<size_t>(b));
                                win->text = before + repl + after;
                                win->sel_start = win->sel_end =
                                    static_cast<int>(a + repl.size());
                                repaint = true;
                                result = 1;
                                break;
                            }
                            case EM_GETLINECOUNT:
                                return 1; // single-line
                            case EM_GETMODIFY:
                                return 0;
                            case EM_SETMODIFY:
                                return 1;
                            case EM_LINEFROMCHAR:
                                return 0;
                            default:
                                break;
                        }
                        break;
                    case kCtrlListbox:
                        switch (message) {
                            case LB_ADDSTRING:
                            case LB_INSERTSTRING: {
                                const char* s = reinterpret_cast<const char*>(
                                    static_cast<uintptr_t>(lparam));
                                int ins_at = (message == LB_INSERTSTRING)
                                    ? static_cast<int>(static_cast<uint32_t>(wparam))
                                    : static_cast<int>(win->items.size());
                                if (ins_at < 0 || ins_at > static_cast<int>(win->items.size()))
                                    ins_at = static_cast<int>(win->items.size());
                                win->items.insert(win->items.begin() + ins_at, s ? s : "");
                                win->item_data.insert(win->item_data.begin() + ins_at, 0);
                                if (win->multi_sel)
                                    win->sel_bits.insert(win->sel_bits.begin() + ins_at, false);
                                repaint = true;
                                result = ins_at;
                                break;
                            }
                            case LB_DELETESTRING: {
                                int idx = static_cast<int>(static_cast<uint32_t>(wparam));
                                if (idx < 0 || idx >= static_cast<int>(win->items.size()))
                                    return LB_ERR;
                                win->items.erase(win->items.begin() + idx);
                                win->item_data.erase(win->item_data.begin() + idx);
                                if (win->multi_sel)
                                    win->sel_bits.erase(win->sel_bits.begin() + idx);
                                if (win->cur_sel == idx) win->cur_sel = -1;
                                else if (win->cur_sel > idx) --win->cur_sel;
                                repaint = true;
                                result = static_cast<int>(win->items.size());
                                break;
                            }
                            case LB_RESETCONTENT:
                                win->items.clear();
                                win->item_data.clear();
                                win->sel_bits.clear();
                                win->cur_sel = -1;
                                win->top_index = 0;
                                repaint = true;
                                return LB_OKAY;
                            case LB_GETCOUNT:
                                return static_cast<int>(win->items.size());
                            case LB_GETCURSEL:
                                return (win->cur_sel >= 0) ? win->cur_sel : LB_ERR;
                            case LB_SETCURSEL: {
                                int idx = static_cast<int>(static_cast<uint32_t>(wparam));
                                if (idx == -1 || idx >= static_cast<int>(win->items.size())) {
                                    win->cur_sel = -1;
                                    repaint = true;
                                    return LB_ERR;
                                }
                                win->cur_sel = idx;
                                if (idx < win->top_index) win->top_index = idx;
                                if (idx >= win->top_index + (win->h ? (win->h / 16) : 8))
                                    win->top_index = std::max(0, idx - (win->h ? (win->h / 16) : 8) + 1);
                                repaint = true;
                                result = LB_OKAY;
                                break;
                            }
                            case LB_GETTEXT: {
                                int idx = static_cast<int>(static_cast<uint32_t>(wparam));
                                if (idx < 0 || idx >= static_cast<int>(win->items.size()))
                                    return LB_ERR;
                                char* dst = reinterpret_cast<char*>(
                                    static_cast<uintptr_t>(lparam));
                                if (dst) strcpy(dst, win->items[static_cast<size_t>(idx)].c_str());
                                result = static_cast<int>(win->items[static_cast<size_t>(idx)].size());
                                break;
                            }
                            case LB_GETTEXTLEN: {
                                int idx = static_cast<int>(static_cast<uint32_t>(wparam));
                                if (idx < 0 || idx >= static_cast<int>(win->items.size()))
                                    return LB_ERR;
                                return static_cast<int>(win->items[static_cast<size_t>(idx)].size());
                            }
                            case LB_GETSEL: {
                                int idx = static_cast<int>(static_cast<uint32_t>(wparam));
                                if (idx < 0 || idx >= static_cast<int>(win->items.size()))
                                    return LB_ERR;
                                if (win->multi_sel)
                                    return win->sel_bits[static_cast<size_t>(idx)] ? 1 : 0;
                                return (win->cur_sel == idx) ? 1 : 0;
                            }
                            case LB_SETSEL: {
                                int idx = static_cast<int>(static_cast<uint32_t>(wparam));
                                if (win->multi_sel) {
                                    if (idx == -1) {
                                        for (size_t bi = 0; bi < win->sel_bits.size(); ++bi)
                                            win->sel_bits[bi] = (lparam != 0);
                                    } else if (idx >= 0 && idx < static_cast<int>(win->sel_bits.size())) {
                                        win->sel_bits[static_cast<size_t>(idx)] = (lparam != 0);
                                    }
                                    repaint = true;
                                    return LB_OKAY;
                                }
                                if (idx >= 0 && idx < static_cast<int>(win->items.size())) {
                                    win->cur_sel = idx;
                                    repaint = true;
                                }
                                return LB_OKAY;
                            }
                            case LB_GETSELCOUNT:
                                if (!win->multi_sel) return LB_ERR;
                                {
                                    int n = 0;
                                    for (auto it = win->sel_bits.begin(); it != win->sel_bits.end(); ++it)
                                        if (*it) ++n;
                                    return n;
                                }
                            case LB_GETSELITEMS: {
                                int* dst = reinterpret_cast<int*>(
                                    static_cast<uintptr_t>(lparam));
                                int cap = static_cast<int>(static_cast<uint32_t>(wparam));
                                if (!dst) return 0;
                                int n = 0;
                                for (size_t i = 0; i < win->sel_bits.size() && n < cap; ++i)
                                    if (win->sel_bits[i]) dst[n++] = static_cast<int>(i);
                                return n;
                            }
                            case LB_FINDSTRING:
                            case LB_SELECTSTRING: {
                                const char* needle = reinterpret_cast<const char*>(
                                    static_cast<uintptr_t>(lparam));
                                if (!needle) return LB_ERR;
                                int start = static_cast<int>(static_cast<uint32_t>(wparam));
                                int n = static_cast<int>(win->items.size());
                                for (int k = 0; k < n; ++k) {
                                    int idx = (start + 1 + k) % n;
                                    if (strncmp(needle, win->items[static_cast<size_t>(idx)].c_str(),
                                                strlen(needle)) == 0) {
                                        if (message == LB_SELECTSTRING) {
                                            win->cur_sel = idx;
                                            repaint = true;
                                        }
                                        result = idx;
                                        break;
                                    }
                                    if (message == LB_SELECTSTRING && k == n - 1) {
                                        if (win->cur_sel < 0) result = LB_ERR;
                                    }
                                }
                                if (message == LB_SELECTSTRING && result == 0 &&
                                    win->cur_sel < 0 && n == 0)
                                    result = LB_ERR;
                                break;
                            }
                            case LB_GETITEMDATA: {
                                int idx = static_cast<int>(static_cast<uint32_t>(wparam));
                                if (idx < 0 || idx >= static_cast<int>(win->item_data.size()))
                                    return LB_ERR;
                                return win->item_data[static_cast<size_t>(idx)];
                            }
                            case LB_SETITEMDATA: {
                                int idx = static_cast<int>(static_cast<uint32_t>(wparam));
                                if (idx < 0 || idx >= static_cast<int>(win->item_data.size()))
                                    return LB_ERR;
                                win->item_data[static_cast<size_t>(idx)] = static_cast<uint32_t>(lparam);
                                return LB_OKAY;
                            }
                            case LB_SETTOPINDEX:
                                win->top_index = std::max(0, static_cast<int>(static_cast<uint32_t>(wparam)));
                                repaint = true;
                                return LB_OKAY;
                            case LB_GETTOPINDEX:
                                return win->top_index;
                            case LB_ITEMFROMPOINT:
                                return LB_ERR;
                            default:
                                break;
                        }
                        break;
                    case kCtrlCombobox:
                        switch (message) {
                            case CB_ADDSTRING:
                            case CB_INSERTSTRING: {
                                const char* s = reinterpret_cast<const char*>(
                                    static_cast<uintptr_t>(lparam));
                                int ins_at = (message == CB_INSERTSTRING)
                                    ? static_cast<int>(static_cast<uint32_t>(wparam))
                                    : static_cast<int>(win->items.size());
                                if (ins_at < 0 || ins_at > static_cast<int>(win->items.size()))
                                    ins_at = static_cast<int>(win->items.size());
                                win->items.insert(win->items.begin() + ins_at, s ? s : "");
                                win->item_data.insert(win->item_data.begin() + ins_at, 0);
                                repaint = true;
                                result = ins_at;
                                break;
                            }
                            case CB_DELETESTRING: {
                                int idx = static_cast<int>(static_cast<uint32_t>(wparam));
                                if (idx < 0 || idx >= static_cast<int>(win->items.size()))
                                    return CB_ERR;
                                win->items.erase(win->items.begin() + idx);
                                win->item_data.erase(win->item_data.begin() + idx);
                                if (win->cur_sel == idx) win->cur_sel = -1;
                                else if (win->cur_sel > idx) --win->cur_sel;
                                repaint = true;
                                result = static_cast<int>(win->items.size());
                                break;
                            }
                            case CB_RESETCONTENT:
                                win->items.clear();
                                win->item_data.clear();
                                win->cur_sel = -1;
                                repaint = true;
                                return CB_OKAY;
                            case CB_GETCOUNT:
                                return static_cast<int>(win->items.size());
                            case CB_GETCURSEL:
                                return (win->cur_sel >= 0) ? win->cur_sel : CB_ERR;
                            case CB_SETCURSEL: {
                                int idx = static_cast<int>(static_cast<uint32_t>(wparam));
                                if (idx == -1 || idx >= static_cast<int>(win->items.size())) {
                                    win->cur_sel = -1;
                                    win->text.clear();
                                    repaint = true;
                                    return CB_ERR;
                                }
                                win->cur_sel = idx;
                                win->text = win->items[static_cast<size_t>(idx)];
                                repaint = true;
                                result = CB_OKAY;
                                break;
                            }
                            case CB_GETLBTEXT: {
                                int idx = static_cast<int>(static_cast<uint32_t>(wparam));
                                if (idx < 0 || idx >= static_cast<int>(win->items.size()))
                                    return CB_ERR;
                                char* dst = reinterpret_cast<char*>(
                                    static_cast<uintptr_t>(lparam));
                                if (dst) strcpy(dst, win->items[static_cast<size_t>(idx)].c_str());
                                result = static_cast<int>(win->items[static_cast<size_t>(idx)].size());
                                break;
                            }
                            case CB_GETLBTEXTLEN: {
                                int idx = static_cast<int>(static_cast<uint32_t>(wparam));
                                if (idx < 0 || idx >= static_cast<int>(win->items.size()))
                                    return CB_ERR;
                                return static_cast<int>(win->items[static_cast<size_t>(idx)].size());
                            }
                            case CB_FINDSTRING:
                            case CB_SELECTSTRING: {
                                const char* needle = reinterpret_cast<const char*>(
                                    static_cast<uintptr_t>(lparam));
                                if (!needle) return CB_ERR;
                                int start = static_cast<int>(static_cast<uint32_t>(wparam));
                                int n = static_cast<int>(win->items.size());
                                int found = CB_ERR;
                                for (int k = 0; k < n; ++k) {
                                    int idx = (start + 1 + k) % n;
                                    if (strncmp(needle, win->items[static_cast<size_t>(idx)].c_str(),
                                                strlen(needle)) == 0) {
                                        found = idx;
                                        break;
                                    }
                                }
                                if (message == CB_SELECTSTRING && found != CB_ERR) {
                                    win->cur_sel = found;
                                    win->text = win->items[static_cast<size_t>(found)];
                                    repaint = true;
                                }
                                result = found;
                                break;
                            }
                            case CB_GETITEMDATA: {
                                int idx = static_cast<int>(static_cast<uint32_t>(wparam));
                                if (idx < 0 || idx >= static_cast<int>(win->item_data.size()))
                                    return CB_ERR;
                                return win->item_data[static_cast<size_t>(idx)];
                            }
                            case CB_SETITEMDATA: {
                                int idx = static_cast<int>(static_cast<uint32_t>(wparam));
                                if (idx < 0 || idx >= static_cast<int>(win->item_data.size()))
                                    return CB_ERR;
                                win->item_data[static_cast<size_t>(idx)] = static_cast<uint32_t>(lparam);
                                return CB_OKAY;
                            }
                            case CB_SHOWDROPDOWN:
                                win->dropped = (wparam != 0);
                                repaint = true;
                                return CB_OKAY;
                            case CB_GETDROPPEDSTATE:
                                return win->dropped ? TRUE_VALUE : FALSE_VALUE;
                            case CB_GETTOPINDEX:
                                return win->top_index;
                            case CB_SETTOPINDEX:
                                win->top_index = std::max(0, static_cast<int>(static_cast<uint32_t>(wparam)));
                                repaint = true;
                                return CB_OKAY;
                            case CB_SETEDITSEL:
                                win->sel_start = static_cast<int>(static_cast<uint32_t>(wparam) & 0xFFFF);
                                win->sel_end = static_cast<int>(static_cast<uint32_t>(lparam) & 0xFFFF);
                                return TRUE_VALUE;
                            case CB_GETEDITSEL: {
                                uint32_t* sel = reinterpret_cast<uint32_t*>(
                                    static_cast<uintptr_t>(lparam));
                                if (sel) {
                                    sel[0] = static_cast<uint32_t>(win->sel_start);
                                    sel[1] = static_cast<uint32_t>(win->sel_end);
                                }
                                return (static_cast<uint64_t>(win->sel_end) << 16) |
                                       static_cast<uint64_t>(win->sel_start);
                            }
                            default:
                                break;
                        }
                        break;
                    default:
                        break;
                }
            }
        }
    }
    if (repaint) x11_paint_window(h);
    if (click_parent) {
        uint64_t parent_proc = 0;
        {
            std::lock_guard<std::mutex> lock(g_window_mutex);
            Window* pwin = find_window(click_parent);
            if (pwin) parent_proc = pwin->wnd_proc;
        }
        if (parent_proc) return call_wnd_proc(parent_proc, click_parent, WM_COMMAND, click_id, h);
        return 0;
    }
    if (ctrl_type != kCtrlNone) return result;
    return call_wnd_proc(wnd_proc, h, message, wparam, lparam);
}

static uint64_t WINAPI_CALL THETAZ_SetTimer(WinHandle hwnd, uintptr_t nIDEvent, DWORD uElapse, void* lpTimerFunc) {
    uint64_t h = hwnd ? reinterpret_cast<uint64_t>(hwnd) : kDesktopHwnd;
    uint32_t elapse = uElapse ? uElapse : 1; // Windows clamps 0 to "soon"
    std::lock_guard<std::mutex> lock(g_window_mutex);
    uint32_t id = static_cast<uint32_t>(nIDEvent);
    if (id == 0) {
        do { id = ++g_next_timer_id; } while (!id || g_timers.count((h << 32) | id));
    }
    GuestTimerInfo t;
    t.hwnd = h;
    t.id = id;
    t.period_ms = elapse;
    t.due_ms = steady_clock_ms() + elapse;
    t.func = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(lpTimerFunc));
    g_timers[(h << 32) | id] = t;
    g_timer_cv.notify_all();
    printf("[user32] SetTimer hwnd=0x%llx id=%lu elapse=%lu\n",
           static_cast<unsigned long long>(h), static_cast<unsigned long>(id),
           static_cast<unsigned long>(elapse));
    return id;
}

static BOOL WINAPI_CALL THETAZ_KillTimer(WinHandle hwnd, uintptr_t nIDEvent) {
    uint64_t h = hwnd ? reinterpret_cast<uint64_t>(hwnd) : kDesktopHwnd;
    std::lock_guard<std::mutex> lock(g_window_mutex);
    size_t erased = g_timers.erase((h << 32) | static_cast<uint32_t>(nIDEvent));
    g_timer_cv.notify_all();
    return erased ? TRUE_VALUE : FALSE_VALUE;
}

static LONG WINAPI_CALL THETAZ_GetMessageA(void* msg, WinHandle hwnd, DWORD filter_min, DWORD filter_max) {
    (void)hwnd;
    if (!msg) return -1;

    std::unique_lock<std::mutex> lock(g_window_mutex);
    while (true) {
        if (!g_queue.empty()) {
            GuestMsg m = g_queue.front();
            bool matches_filter = (filter_min == 0 && filter_max == 0) ||
                                  (m.message >= filter_min && m.message <= filter_max);
            if (m.message == WM_QUIT) {
                // WM_QUIT is never filtered and ends the loop.
                g_queue.pop_front();
                g_quit_pending = false;
                write_guest_msg(static_cast<uint8_t*>(msg), m);
                printf("[user32] GetMessageA -> WM_QUIT (%lu)\n", (unsigned long)m.wparam);
                return 0;
            }
            if (matches_filter) {
                g_queue.pop_front();
                write_guest_msg(static_cast<uint8_t*>(msg), m);
                return 1;
            }
            // Doesn't match: leave it queued and wait for a matching message.
        }
        g_queue_cond.wait(lock);
    }
}

static BOOL WINAPI_CALL THETAZ_PeekMessageA(void* msg, WinHandle hwnd, DWORD filter_min, DWORD filter_max, DWORD remove) {
    (void)hwnd;
    if (!msg) return FALSE_VALUE;
    std::lock_guard<std::mutex> lock(g_window_mutex);
    for (auto it = g_queue.begin(); it != g_queue.end(); ++it) {
        bool matches_filter = (filter_min == 0 && filter_max == 0) ||
                              (it->message >= filter_min && it->message <= filter_max);
        if (it->message == WM_QUIT || matches_filter) {
            GuestMsg m = *it;
            if (remove) {
                g_queue.erase(it);
                if (m.message == WM_QUIT) g_quit_pending = false;
            }
            write_guest_msg(static_cast<uint8_t*>(msg), m);
            return TRUE_VALUE;
        }
    }
    return FALSE_VALUE;
}

static BOOL WINAPI_CALL THETAZ_TranslateMessage(const void* msg) {
    if (!msg) return FALSE_VALUE;
    const uint8_t* m = static_cast<const uint8_t*>(msg);
    uint32_t message = *reinterpret_cast<const uint32_t*>(m + MSG_MESSAGE);
    if (message == WM_KEYDOWN) {
        uint64_t wparam = *reinterpret_cast<const uint32_t*>(m + MSG_WPARAM);
        // Synthesize WM_CHAR for printable-ish keys (no keyboard state model).
        THETAZ_PostMessageA(reinterpret_cast<WinHandle>(*reinterpret_cast<const uint32_t*>(m + MSG_HWND)),
                            WM_CHAR, wparam, 1);
        return TRUE_VALUE;
    }
    return FALSE_VALUE;
}

static uint64_t WINAPI_CALL THETAZ_DispatchMessageA(const void* msg) {
    if (!msg) return 0;
    const uint8_t* m = static_cast<const uint8_t*>(msg);
    uint64_t hwnd = *reinterpret_cast<const uint32_t*>(m + MSG_HWND);
    uint32_t message = *reinterpret_cast<const uint32_t*>(m + MSG_MESSAGE);
    uint64_t wparam = *reinterpret_cast<const uint32_t*>(m + MSG_WPARAM);
    uint64_t lparam = *reinterpret_cast<const uint32_t*>(m + MSG_LPARAM);

    uint64_t wnd_proc = 0;
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        Window* win = find_window(hwnd);
        if (win) wnd_proc = win->wnd_proc;
    }

    return call_wnd_proc(wnd_proc, hwnd, message, wparam, lparam);
}

static uint64_t WINAPI_CALL THETAZ_DefWindowProcA(WinHandle hwnd, DWORD message, uint64_t wparam, uint64_t lparam) {
    (void)wparam;
    (void)lparam;
    switch (message) {
        case WM_CLOSE:
            THETAZ_DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            THETAZ_PostQuitMessage(0);
            return 0;
        default:
            return 0;
    }
}

// --- Dialog and control-accessor APIs -----------------------------------------

// GetDlgItem(hDlg, id): find the direct child control with that id.
static WinHandle WINAPI_CALL THETAZ_GetDlgItem(WinHandle hdlg, int id) {
    uint64_t h = reinterpret_cast<uint64_t>(hdlg);
    std::lock_guard<std::mutex> lock(g_window_mutex);
    for (auto& kv : g_windows) {
        const Window& w = kv.second;
        if (w.parent == h && static_cast<int>(w.id) == id)
            return reinterpret_cast<WinHandle>(kv.first);
    }
    return nullptr;
}

// GetDlgCtrlID(hwnd): returns the control's id.
static int WINAPI_CALL THETAZ_GetDlgCtrlID(WinHandle hwnd) {
    std::lock_guard<std::mutex> lock(g_window_mutex);
    Window* win = find_window(reinterpret_cast<uint64_t>(hwnd));
    return win ? static_cast<int>(win->id) : 0;
}

static int WINAPI_CALL THETAZ_GetDlgItemTextA(WinHandle hdlg, int id, char* buf, int max) {
    WinHandle ctrl = THETAZ_GetDlgItem(hdlg, id);
    if (!ctrl || !buf || max <= 0) return 0;
    uint64_t r = THETAZ_SendMessageA(ctrl, WM_GETTEXT, static_cast<uint64_t>(max),
                                     reinterpret_cast<uint64_t>(buf));
    return static_cast<int>(r);
}

static BOOL WINAPI_CALL THETAZ_SetDlgItemTextA(WinHandle hdlg, int id, const char* text) {
    WinHandle ctrl = THETAZ_GetDlgItem(hdlg, id);
    if (!ctrl) return FALSE_VALUE;
    THETAZ_SendMessageA(ctrl, WM_SETTEXT, 0, reinterpret_cast<uint64_t>(text));
    return TRUE_VALUE;
}

static uint64_t WINAPI_CALL THETAZ_SendDlgItemMessageA(WinHandle hdlg, int id, DWORD message,
                                                       uint64_t wparam, uint64_t lparam) {
    WinHandle ctrl = THETAZ_GetDlgItem(hdlg, id);
    if (!ctrl) return 0;
    return THETAZ_SendMessageA(ctrl, message, wparam, lparam);
}

static BOOL WINAPI_CALL THETAZ_CheckDlgButton(WinHandle hdlg, int id, uint32_t check) {
    WinHandle ctrl = THETAZ_GetDlgItem(hdlg, id);
    if (!ctrl) return FALSE_VALUE;
    return THETAZ_SendMessageA(ctrl, BM_SETCHECK, check ? 1 : 0, 0) ? TRUE_VALUE : FALSE_VALUE;
}

static uint32_t WINAPI_CALL THETAZ_IsDlgButtonChecked(WinHandle hdlg, int id) {
    WinHandle ctrl = THETAZ_GetDlgItem(hdlg, id);
    if (!ctrl) return 0;
    return static_cast<uint32_t>(THETAZ_SendMessageA(ctrl, BM_GETCHECK, 0, 0));
}

static BOOL WINAPI_CALL THETAZ_EnableWindow(WinHandle hwnd, BOOL enable) {
    uint64_t h = reinterpret_cast<uint64_t>(hwnd);
    std::lock_guard<std::mutex> lock(g_window_mutex);
    Window* win = find_window(h);
    if (!win) return FALSE_VALUE;
    BOOL was = win->enabled ? TRUE_VALUE : FALSE_VALUE;
    win->enabled = (enable != 0);
    return was;
}

static BOOL WINAPI_CALL THETAZ_IsWindowEnabled(WinHandle hwnd) {
    std::lock_guard<std::mutex> lock(g_window_mutex);
    Window* win = find_window(reinterpret_cast<uint64_t>(hwnd));
    return (win && win->enabled) ? TRUE_VALUE : FALSE_VALUE;
}

static BOOL WINAPI_CALL THETAZ_IsWindowVisible(WinHandle hwnd) {
    std::lock_guard<std::mutex> lock(g_window_mutex);
    Window* win = find_window(reinterpret_cast<uint64_t>(hwnd));
    return (win && win->visible) ? TRUE_VALUE : FALSE_VALUE;
}

static BOOL WINAPI_CALL THETAZ_IsWindow(WinHandle hwnd) {
    std::lock_guard<std::mutex> lock(g_window_mutex);
    return find_window(reinterpret_cast<uint64_t>(hwnd)) ? TRUE_VALUE : FALSE_VALUE;
}

static int WINAPI_CALL THETAZ_GetWindowTextA(WinHandle hwnd, char* buf, int max) {
    if (!buf || max <= 0) return 0;
    uint64_t r = THETAZ_SendMessageA(hwnd, WM_GETTEXT, static_cast<uint64_t>(max),
                                     reinterpret_cast<uint64_t>(buf));
    return static_cast<int>(r);
}

static int WINAPI_CALL THETAZ_GetWindowTextLengthA(WinHandle hwnd) {
    return static_cast<int>(THETAZ_SendMessageA(hwnd, WM_GETTEXTLENGTH, 0, 0));
}

static BOOL WINAPI_CALL THETAZ_SetWindowTextA(WinHandle hwnd, const char* text) {
    if (!hwnd) return FALSE_VALUE;
    return THETAZ_SendMessageA(hwnd, WM_SETTEXT, 0, reinterpret_cast<uint64_t>(text))
        ? TRUE_VALUE : FALSE_VALUE;
}

static void* WINAPI_CALL THETAZ_SetFocus(WinHandle hwnd) {
    if (!hwnd) return nullptr;
    // Track focus on controls (EDIT input routing + caret rendering).
    THETAZ_SendMessageA(hwnd, WM_SETFOCUS, 0, 0);
    return reinterpret_cast<void*>(hwnd);
}

static void* WINAPI_CALL THETAZ_GetFocus() {
    std::lock_guard<std::mutex> lock(g_window_mutex);
    for (auto& kv : g_windows)
        if (kv.second.has_focus) return reinterpret_cast<void*>(kv.first);
    return nullptr;
}

// DialogBoxParamA/W: we do not have resource template parsing yet; installers
// typically call this with an integer template id. We create a plain window of
// the (registered) main dialog class if any, else a top-level window, run a
// modal message loop dispatching the shared queue, and return the EndDialog code.
static int WINAPI_CALL THETAZ_DialogBoxParamA(WinHandle hInstance, const char* name,
                                              WinHandle hWndParent,
                                              void* lpDialogFunc, uint64_t dwInitParam) {
    (void)hInstance;
    (void)hWndParent;
    (void)name;
    // Find a registered class to host the dialog; default to a raw window.
    uint64_t proc = reinterpret_cast<uint64_t>(lpDialogFunc);
    uint64_t hwnd = 0;
    std::string cls_name;
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        hwnd = g_next_hwnd++;
        Window win;
        win.hwnd = hwnd;
        win.wnd_proc = proc;
        win.class_name = "#dialog";
        win.style = WS_CHILD | WS_VISIBLE;
        win.w = 640;
        win.h = 400;
        g_windows[hwnd] = win;
    }
    if (x11_display_init() && g_x11_display) {
        ::Window xw = XCreateSimpleWindow(g_x11_display, DefaultRootWindow(g_x11_display),
                                          100, 80, 640, 400, 0,
                                          BlackPixel(g_x11_display, DefaultScreen(g_x11_display)),
                                          WhitePixel(g_x11_display, DefaultScreen(g_x11_display)));
        XSelectInput(g_x11_display, xw,
                     ExposureMask | ButtonPressMask | ButtonReleaseMask |
                     KeyPressMask | StructureNotifyMask);
        XStoreName(g_x11_display, xw, "Dialog");
        Atom wm_delete = XInternAtom(g_x11_display, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(g_x11_display, xw, &wm_delete, 1);
        XMapRaised(g_x11_display, xw);
        {
            std::lock_guard<std::mutex> lock(g_window_mutex);
            g_x11_windows[hwnd] = xw;
            g_x11_hwnds[xw] = hwnd;
        }
    }
    // WM_INITDIALOG (0x0110) — return value 0 in a dialog means "give focus".
    call_wnd_proc(proc, hwnd, 0x0110, dwInitParam, 0);
    // Modal loop: dispatch messages until the dialog is destroyed.
    while (true) {
        {
            std::unique_lock<std::mutex> lock(g_window_mutex);
            if (g_windows.find(hwnd) == g_windows.end()) break;
        }
        GuestMsg m;
        bool got = false;
        {
            std::unique_lock<std::mutex> lock(g_window_mutex);
            for (auto it = g_queue.begin(); it != g_queue.end(); ++it) {
                if (it->hwnd == hwnd || it->message == WM_QUIT || it->message == WM_TIMER) {
                    m = *it;
                    g_queue.erase(it);
                    got = true;
                    break;
                }
            }
        }
        if (!got) {
            usleep(5000);
            continue;
        }
        if (m.message == WM_QUIT) {
            g_quit_pending = false;
            g_quit_exit_code = static_cast<DWORD>(m.wparam);
            break;
        }
        uint64_t r = call_wnd_proc(proc, m.hwnd ? m.hwnd : hwnd, m.message, m.wparam, m.lparam);
        if (g_dialog_result_active) {
            int rc = g_dialog_result;
            g_dialog_result_active = false;
            return rc;
        }
        (void)r;
    }
    return 0;
}

// EndDialog(hDlg, result): ends the running modal dialog loop.
static BOOL WINAPI_CALL THETAZ_EndDialog(WinHandle hdlg, int result) {
    uint64_t h = reinterpret_cast<uint64_t>(hdlg);
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        g_dialog_result_active = true;
        g_dialog_result = result;
        auto xit = g_x11_windows.find(h);
        if (xit != g_x11_windows.end()) {
            g_x11_hwnds.erase(xit->second);
            g_x11_windows.erase(xit);
        }
        g_windows.erase(h);
    }
    return TRUE_VALUE;
}

static int WINAPI_CALL THETAZ_MessageBoxA(WinHandle hwnd, const char* text,
                                          const char* caption, uint32_t type) {
    (void)hwnd;
    (void)type;
    fprintf(stderr, "[user32] MessageBoxA: \"%s\" title=\"%s\"\n",
            text ? text : "", caption ? caption : "");
    return 1; // IDOK
}

// --- 64-bit user32 API functions (for PE32+ guests) ---------------------------

// 64-bit GetMessageA: reads/writes x64 MSG structure
static LONG WINAPI_CALL WINAPI64_GetMessageA(void* msg, WinHandle hwnd, DWORD filter_min, DWORD filter_max) {
    (void)hwnd;
    if (!msg) return -1;

    std::unique_lock<std::mutex> lock(g_window_mutex);
    while (true) {
        if (!g_queue.empty()) {
            GuestMsg m = g_queue.front();
            bool matches_filter = (filter_min == 0 && filter_max == 0) ||
                                  (m.message >= filter_min && m.message <= filter_max);
            if (m.message == WM_QUIT) {
                g_queue.pop_front();
                g_quit_pending = false;
                write_guest_msg64(static_cast<uint8_t*>(msg), m);
                printf("[user32] GetMessageA (x64) -> WM_QUIT (%lu)\n", (unsigned long)m.wparam);
                return 0;
            }
            if (matches_filter) {
                g_queue.pop_front();
                write_guest_msg64(static_cast<uint8_t*>(msg), m);
                return 1;
            }
        }
        g_queue_cond.wait(lock);
    }
}

// 64-bit PeekMessageA
static BOOL WINAPI_CALL WINAPI64_PeekMessageA(void* msg, WinHandle hwnd, DWORD filter_min, DWORD filter_max, DWORD remove) {
    (void)hwnd;
    if (!msg) return FALSE_VALUE;
    std::lock_guard<std::mutex> lock(g_window_mutex);
    for (auto it = g_queue.begin(); it != g_queue.end(); ++it) {
        bool matches_filter = (filter_min == 0 && filter_max == 0) ||
                              (it->message >= filter_min && it->message <= filter_max);
        if (it->message == WM_QUIT || matches_filter) {
            GuestMsg m = *it;
            if (remove) {
                g_queue.erase(it);
                if (m.message == WM_QUIT) g_quit_pending = false;
            }
            write_guest_msg64(static_cast<uint8_t*>(msg), m);
            return TRUE_VALUE;
        }
    }
    return FALSE_VALUE;
}

// 64-bit TranslateMessage
static BOOL WINAPI_CALL WINAPI64_TranslateMessage(const void* msg) {
    if (!msg) return FALSE_VALUE;
    const uint8_t* m = static_cast<const uint8_t*>(msg);
    uint32_t message = *reinterpret_cast<const uint32_t*>(m + MSG64_MESSAGE);
    if (message == WM_KEYDOWN) {
        uint64_t wparam = *reinterpret_cast<const uint64_t*>(m + MSG64_WPARAM);
        THETAZ_PostMessageA(reinterpret_cast<WinHandle>(*reinterpret_cast<const uint64_t*>(m + MSG64_HWND)),
                            WM_CHAR, wparam, 1);
        return TRUE_VALUE;
    }
    return FALSE_VALUE;
}

// 64-bit DispatchMessageA
static uint64_t WINAPI_CALL WINAPI64_DispatchMessageA(const void* msg) {
    if (!msg) return 0;
    const uint8_t* m = static_cast<const uint8_t*>(msg);
    uint64_t hwnd = *reinterpret_cast<const uint64_t*>(m + MSG64_HWND);
    uint32_t message = *reinterpret_cast<const uint32_t*>(m + MSG64_MESSAGE);
    uint64_t wparam = *reinterpret_cast<const uint64_t*>(m + MSG64_WPARAM);
    uint64_t lparam = *reinterpret_cast<const uint64_t*>(m + MSG64_LPARAM);

    uint64_t wnd_proc = 0;
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        Window* win = find_window(hwnd);
        if (win) wnd_proc = win->wnd_proc;
    }

    return call_wnd_proc(wnd_proc, hwnd, message, wparam, lparam);
}

// 64-bit PostMessageA
static BOOL WINAPI_CALL WINAPI64_PostMessageA(WinHandle hwnd, DWORD message, uint64_t wparam, uint64_t lparam) {
    return THETAZ_PostMessageA(hwnd, message, wparam, lparam);
}

// 64-bit SendMessageA
static uint64_t WINAPI_CALL WINAPI64_SendMessageA(WinHandle hwnd, DWORD message, uint64_t wparam, uint64_t lparam) {
    return THETAZ_SendMessageA(hwnd, message, wparam, lparam);
}

// 64-bit RegisterClassA
static uint16_t WINAPI_CALL WINAPI64_RegisterClassA(const void* wc) {
    if (!wc) return 0;
    const uint8_t* c = static_cast<const uint8_t*>(wc);
    // 64-bit guest struct: pointer fields are 8 bytes
    uint64_t name_addr = *reinterpret_cast<const uint64_t*>(c + WNDCLASSA64_LPSZCLASSNAME);
    LPCSTR name = reinterpret_cast<LPCSTR>(name_addr);
    if (!name || name[0] == '\0') return 0;

    WindowClass cls;
    cls.name = name;
    cls.wnd_proc = *reinterpret_cast<const uint64_t*>(c + WNDCLASSA64_WNDPROC);
    cls.style = *reinterpret_cast<const uint32_t*>(c + WNDCLASSA64_STYLE);
    cls.hinstance = *reinterpret_cast<const uint64_t*>(c + WNDCLASSA64_HINSTANCE);

    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        g_classes[cls.name] = cls;
    }
    fprintf(stderr, "[user32] RegisterClassA (x64): \"%s\" wndproc=0x%lX\n", name, cls.wnd_proc);
    return static_cast<uint16_t>(g_classes.size());
}

static uint16_t WINAPI_CALL WINAPI64_RegisterClassW(const void* wc) {
    if (!wc) return 0;
    const uint8_t* c = static_cast<const uint8_t*>(wc);
    uint64_t name_addr = *reinterpret_cast<const uint64_t*>(c + WNDCLASSA64_LPSZCLASSNAME);
    const uint16_t* wname = reinterpret_cast<const uint16_t*>(name_addr);
    std::string name = wcls_to_a(wname);
    if (name.empty()) return 0;
    WindowClass cls;
    cls.name = name;
    cls.wnd_proc = *reinterpret_cast<const uint64_t*>(c + WNDCLASSA64_WNDPROC);
    cls.style = *reinterpret_cast<const uint32_t*>(c + WNDCLASSA64_STYLE);
    cls.hinstance = *reinterpret_cast<const uint64_t*>(c + WNDCLASSA64_HINSTANCE);
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        g_classes[cls.name] = cls;
    }
    fprintf(stderr, "[user32] RegisterClassW (x64): \"%s\" wndproc=0x%lX\n", name.c_str(), cls.wnd_proc);
    return static_cast<uint16_t>(g_classes.size());
}

static uint16_t WINAPI_CALL WINAPI64_RegisterClassExA(const void* wc) {
    if (!wc) return 0;
    const uint8_t* c = static_cast<const uint8_t*>(wc);
    uint64_t name_addr = *reinterpret_cast<const uint64_t*>(c + WNDCLASSA64_LPSZCLASSNAME);
    LPCSTR name = reinterpret_cast<LPCSTR>(name_addr);
    if (!name || name[0] == '\0') return 0;
    WindowClass cls;
    cls.name = name;
    cls.wnd_proc = *reinterpret_cast<const uint64_t*>(c + WNDCLASSA64_WNDPROC);
    cls.style = *reinterpret_cast<const uint32_t*>(c + WNDCLASSA64_STYLE);
    cls.hinstance = *reinterpret_cast<const uint64_t*>(c + WNDCLASSA64_HINSTANCE);
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        g_classes[cls.name] = cls;
    }
    fprintf(stderr, "[user32] RegisterClassExA (x64): \"%s\" wndproc=0x%lX\n", name, cls.wnd_proc);
    return static_cast<uint16_t>(g_classes.size());
}

static uint16_t WINAPI_CALL WINAPI64_RegisterClassExW(const void* wc) {
    if (!wc) return 0;
    const uint8_t* c = static_cast<const uint8_t*>(wc);
    uint64_t name_addr = *reinterpret_cast<const uint64_t*>(c + WNDCLASSA64_LPSZCLASSNAME);
    const uint16_t* wname = reinterpret_cast<const uint16_t*>(name_addr);
    std::string name = wcls_to_a(wname);
    if (name.empty()) return 0;
    WindowClass cls;
    cls.name = name;
    cls.wnd_proc = *reinterpret_cast<const uint64_t*>(c + WNDCLASSA64_WNDPROC);
    cls.style = *reinterpret_cast<const uint32_t*>(c + WNDCLASSA64_STYLE);
    cls.hinstance = *reinterpret_cast<const uint64_t*>(c + WNDCLASSA64_HINSTANCE);
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        g_classes[cls.name] = cls;
    }
    fprintf(stderr, "[user32] RegisterClassExW (x64): \"%s\" wndproc=0x%lX\n", name.c_str(), cls.wnd_proc);
    return static_cast<uint16_t>(g_classes.size());
}

// 64-bit CreateWindowExA
static WinHandle WINAPI_CALL WINAPI64_CreateWindowExA(DWORD ex_style, LPCSTR class_name, LPCSTR window_name,
                                                      DWORD style, int x, int y, int w, int h,
                                                      WinHandle parent, WinHandle menu, HINSTANCE instance, LPVOID param) {
    // Reuse the 32-bit implementation since it works with the same internal structures
    return THETAZ_CreateWindowExA(ex_style, class_name, window_name, style, x, y, w, h, parent, menu, instance, param);
}

// 64-bit DefWindowProcA
static uint64_t WINAPI_CALL WINAPI64_DefWindowProcA(WinHandle hwnd, DWORD message, uint64_t wparam, uint64_t lparam) {
    return THETAZ_DefWindowProcA(hwnd, message, wparam, lparam);
}

// 64-bit GetDesktopWindow
static void* WINAPI_CALL WINAPI64_GetDesktopWindow() {
    return THETAZ_GetDesktopWindow();
}

// 64-bit DestroyWindow
static BOOL WINAPI_CALL WINAPI64_DestroyWindow(WinHandle hwnd) {
    return THETAZ_DestroyWindow(hwnd);
}

// 64-bit ShowWindow
static BOOL WINAPI_CALL WINAPI64_ShowWindow(WinHandle hwnd, int cmd_show) {
    return THETAZ_ShowWindow(hwnd, cmd_show);
}

// 64-bit UpdateWindow
static BOOL WINAPI_CALL WINAPI64_UpdateWindow(WinHandle hwnd) {
    return THETAZ_UpdateWindow(hwnd);
}

// 64-bit InvalidateRect
static BOOL WINAPI_CALL WINAPI64_InvalidateRect(WinHandle hwnd, const void* rect, BOOL erase) {
    return THETAZ_InvalidateRect(hwnd, rect, erase);
}

// 64-bit BeginPaint
static void* WINAPI_CALL WINAPI64_BeginPaint(WinHandle hwnd, void* ps) {
    return THETAZ_BeginPaint(hwnd, ps);
}

// 64-bit EndPaint
static BOOL WINAPI_CALL WINAPI64_EndPaint(WinHandle hwnd, const void* ps) {
    return THETAZ_EndPaint(hwnd, ps);
}

// 64-bit SetTimer
static uint64_t WINAPI_CALL WINAPI64_SetTimer(WinHandle hwnd, uintptr_t nIDEvent, DWORD uElapse, void* lpTimerFunc) {
    return THETAZ_SetTimer(hwnd, nIDEvent, uElapse, lpTimerFunc);
}

// 64-bit KillTimer
static BOOL WINAPI_CALL WINAPI64_KillTimer(WinHandle hwnd, uintptr_t nIDEvent) {
    return THETAZ_KillTimer(hwnd, nIDEvent);
}

// 64-bit PostQuitMessage
static void WINAPI_CALL WINAPI64_PostQuitMessage(int exit_code) {
    THETAZ_PostQuitMessage(exit_code);
}

// 64-bit dialog/accessor APIs (all reuse the 32-bit implementations).
static WinHandle WINAPI_CALL WINAPI64_GetDlgItem(WinHandle hdlg, int id) {
    return THETAZ_GetDlgItem(hdlg, id);
}
static int WINAPI_CALL WINAPI64_GetDlgCtrlID(WinHandle hwnd) {
    return THETAZ_GetDlgCtrlID(hwnd);
}
static int WINAPI_CALL WINAPI64_GetDlgItemTextA(WinHandle hdlg, int id, char* buf, int max) {
    return THETAZ_GetDlgItemTextA(hdlg, id, buf, max);
}
static BOOL WINAPI_CALL WINAPI64_SetDlgItemTextA(WinHandle hdlg, int id, const char* text) {
    return THETAZ_SetDlgItemTextA(hdlg, id, text);
}
static uint64_t WINAPI_CALL WINAPI64_SendDlgItemMessageA(WinHandle hdlg, int id, DWORD message,
                                                         uint64_t wparam, uint64_t lparam) {
    return THETAZ_SendDlgItemMessageA(hdlg, id, message, wparam, lparam);
}
static BOOL WINAPI_CALL WINAPI64_CheckDlgButton(WinHandle hdlg, int id, uint32_t check) {
    return THETAZ_CheckDlgButton(hdlg, id, check);
}
static uint32_t WINAPI_CALL WINAPI64_IsDlgButtonChecked(WinHandle hdlg, int id) {
    return THETAZ_IsDlgButtonChecked(hdlg, id);
}
static BOOL WINAPI_CALL WINAPI64_EnableWindow(WinHandle hwnd, BOOL enable) {
    return THETAZ_EnableWindow(hwnd, enable);
}
static BOOL WINAPI_CALL WINAPI64_IsWindowEnabled(WinHandle hwnd) {
    return THETAZ_IsWindowEnabled(hwnd);
}
static BOOL WINAPI_CALL WINAPI64_IsWindowVisible(WinHandle hwnd) {
    return THETAZ_IsWindowVisible(hwnd);
}
static BOOL WINAPI_CALL WINAPI64_IsWindow(WinHandle hwnd) {
    return THETAZ_IsWindow(hwnd);
}
static int WINAPI_CALL WINAPI64_GetWindowTextA(WinHandle hwnd, char* buf, int max) {
    return THETAZ_GetWindowTextA(hwnd, buf, max);
}
static int WINAPI_CALL WINAPI64_GetWindowTextLengthA(WinHandle hwnd) {
    return THETAZ_GetWindowTextLengthA(hwnd);
}
static BOOL WINAPI_CALL WINAPI64_SetWindowTextA(WinHandle hwnd, const char* text) {
    return THETAZ_SetWindowTextA(hwnd, text);
}
static void* WINAPI_CALL WINAPI64_SetFocus(WinHandle hwnd) {
    return THETAZ_SetFocus(hwnd);
}
static void* WINAPI_CALL WINAPI64_GetFocus() {
    return THETAZ_GetFocus();
}
static int WINAPI_CALL WINAPI64_DialogBoxParamA(WinHandle hInstance, const char* name,
                                                WinHandle hWndParent, void* lpDialogFunc,
                                                uint64_t dwInitParam) {
    return THETAZ_DialogBoxParamA(hInstance, name, hWndParent, lpDialogFunc, dwInitParam);
}
static BOOL WINAPI_CALL WINAPI64_EndDialog(WinHandle hdlg, int result) {
    return THETAZ_EndDialog(hdlg, result);
}
static int WINAPI_CALL WINAPI64_MessageBoxA(WinHandle hwnd, const char* text,
                                            const char* caption, uint32_t type) {
    return THETAZ_MessageBoxA(hwnd, text, caption, type);
}

// --- Dispatch wrappers (choose 32-bit or 64-bit based on context) ------------

#include "core/x86/api_dispatch.h"

static LONG WINAPI_CALL THETAZ_GetMessageA_Wrapper(void* msg, WinHandle hwnd, DWORD filter_min, DWORD filter_max) {
    if (x86_dispatch_active()) {
        return THETAZ_GetMessageA(msg, hwnd, filter_min, filter_max);
    } else {
        return WINAPI64_GetMessageA(msg, hwnd, filter_min, filter_max);
    }
}

static BOOL WINAPI_CALL THETAZ_PeekMessageA_Wrapper(void* msg, WinHandle hwnd, DWORD filter_min, DWORD filter_max, DWORD remove) {
    if (x86_dispatch_active()) {
        return THETAZ_PeekMessageA(msg, hwnd, filter_min, filter_max, remove);
    } else {
        return WINAPI64_PeekMessageA(msg, hwnd, filter_min, filter_max, remove);
    }
}

static BOOL WINAPI_CALL THETAZ_TranslateMessage_Wrapper(const void* msg) {
    if (x86_dispatch_active()) {
        return THETAZ_TranslateMessage(msg);
    } else {
        return WINAPI64_TranslateMessage(msg);
    }
}

static uint64_t WINAPI_CALL THETAZ_DispatchMessageA_Wrapper(const void* msg) {
    if (x86_dispatch_active()) {
        return THETAZ_DispatchMessageA(msg);
    } else {
        return WINAPI64_DispatchMessageA(msg);
    }
}

static uint16_t WINAPI_CALL THETAZ_RegisterClassA_Wrapper(const void* wc) {
    if (x86_dispatch_active()) {
        return THETAZ_RegisterClassA(wc);
    } else {
        return WINAPI64_RegisterClassA(wc);
    }
}
static uint16_t WINAPI_CALL THETAZ_RegisterClassW_Wrapper(const void* wc) {
    if (x86_dispatch_active()) {
        return THETAZ_RegisterClassW(wc);
    } else {
        return WINAPI64_RegisterClassW(wc);
    }
}
static uint16_t WINAPI_CALL THETAZ_RegisterClassExA_Wrapper(const void* wc) {
    if (x86_dispatch_active()) {
        return THETAZ_RegisterClassExA(wc);
    } else {
        return WINAPI64_RegisterClassExA(wc);
    }
}
static uint16_t WINAPI_CALL THETAZ_RegisterClassExW_Wrapper(const void* wc) {
    if (x86_dispatch_active()) {
        return THETAZ_RegisterClassExW(wc);
    } else {
        return WINAPI64_RegisterClassExW(wc);
    }
}

static WinHandle WINAPI_CALL THETAZ_CreateWindowExA_Wrapper(DWORD ex_style, LPCSTR class_name, LPCSTR window_name,
                                                            DWORD style, int x, int y, int w, int h,
                                                            WinHandle parent, WinHandle menu, HINSTANCE instance, LPVOID param) {
    // Both versions use the same internal implementation
    return THETAZ_CreateWindowExA(ex_style, class_name, window_name, style, x, y, w, h, parent, menu, instance, param);
}

static BOOL WINAPI_CALL THETAZ_DestroyWindow_Wrapper(WinHandle hwnd) {
    if (x86_dispatch_active()) {
        return THETAZ_DestroyWindow(hwnd);
    } else {
        return WINAPI64_DestroyWindow(hwnd);
    }
}

static BOOL WINAPI_CALL THETAZ_ShowWindow_Wrapper(WinHandle hwnd, int cmd_show) {
    if (x86_dispatch_active()) {
        return THETAZ_ShowWindow(hwnd, cmd_show);
    } else {
        return WINAPI64_ShowWindow(hwnd, cmd_show);
    }
}

static BOOL WINAPI_CALL THETAZ_UpdateWindow_Wrapper(WinHandle hwnd) {
    if (x86_dispatch_active()) {
        return THETAZ_UpdateWindow(hwnd);
    } else {
        return WINAPI64_UpdateWindow(hwnd);
    }
}

static BOOL WINAPI_CALL THETAZ_InvalidateRect_Wrapper(WinHandle hwnd, const void* rect, BOOL erase) {
    if (x86_dispatch_active()) {
        return THETAZ_InvalidateRect(hwnd, rect, erase);
    } else {
        return WINAPI64_InvalidateRect(hwnd, rect, erase);
    }
}

static void* WINAPI_CALL THETAZ_BeginPaint_Wrapper(WinHandle hwnd, void* ps) {
    if (x86_dispatch_active()) {
        return THETAZ_BeginPaint(hwnd, ps);
    } else {
        return WINAPI64_BeginPaint(hwnd, ps);
    }
}

static BOOL WINAPI_CALL THETAZ_EndPaint_Wrapper(WinHandle hwnd, const void* ps) {
    if (x86_dispatch_active()) {
        return THETAZ_EndPaint(hwnd, ps);
    } else {
        return WINAPI64_EndPaint(hwnd, ps);
    }
}

static uint64_t WINAPI_CALL THETAZ_SetTimer_Wrapper(WinHandle hwnd, uintptr_t nIDEvent, DWORD uElapse, void* lpTimerFunc) {
    if (x86_dispatch_active()) {
        return THETAZ_SetTimer(hwnd, nIDEvent, uElapse, lpTimerFunc);
    } else {
        return WINAPI64_SetTimer(hwnd, nIDEvent, uElapse, lpTimerFunc);
    }
}

static BOOL WINAPI_CALL THETAZ_KillTimer_Wrapper(WinHandle hwnd, uintptr_t nIDEvent) {
    if (x86_dispatch_active()) {
        return THETAZ_KillTimer(hwnd, nIDEvent);
    } else {
        return WINAPI64_KillTimer(hwnd, nIDEvent);
    }
}

static void WINAPI_CALL THETAZ_PostQuitMessage_Wrapper(int exit_code) {
    if (x86_dispatch_active()) {
        THETAZ_PostQuitMessage(exit_code);
    } else {
        WINAPI64_PostQuitMessage(exit_code);
    }
}

static uint64_t WINAPI_CALL THETAZ_DefWindowProcA_Wrapper(WinHandle hwnd, DWORD message, uint64_t wparam, uint64_t lparam) {
    if (x86_dispatch_active()) {
        return THETAZ_DefWindowProcA(hwnd, message, wparam, lparam);
    } else {
        return WINAPI64_DefWindowProcA(hwnd, message, wparam, lparam);
    }
}

static void* WINAPI_CALL THETAZ_GetDesktopWindow_Wrapper() {
    if (x86_dispatch_active()) {
        return THETAZ_GetDesktopWindow();
    } else {
        return WINAPI64_GetDesktopWindow();
    }
}

static BOOL WINAPI_CALL THETAZ_PostMessageA_Wrapper(WinHandle hwnd, DWORD message, uint64_t wparam, uint64_t lparam) {
    if (x86_dispatch_active()) {
        return THETAZ_PostMessageA(hwnd, message, wparam, lparam);
    } else {
        return WINAPI64_PostMessageA(hwnd, message, wparam, lparam);
    }
}

static uint64_t WINAPI_CALL THETAZ_SendMessageA_Wrapper(WinHandle hwnd, DWORD message, uint64_t wparam, uint64_t lparam) {
    if (x86_dispatch_active()) {
        return THETAZ_SendMessageA(hwnd, message, wparam, lparam);
    } else {
        return WINAPI64_SendMessageA(hwnd, message, wparam, lparam);
    }
}

static WinHandle WINAPI_CALL THETAZ_GetDlgItem_Wrapper(WinHandle hdlg, int id) {
    if (x86_dispatch_active()) return THETAZ_GetDlgItem(hdlg, id);
    return WINAPI64_GetDlgItem(hdlg, id);
}
static int WINAPI_CALL THETAZ_GetDlgCtrlID_Wrapper(WinHandle hwnd) {
    if (x86_dispatch_active()) return THETAZ_GetDlgCtrlID(hwnd);
    return WINAPI64_GetDlgCtrlID(hwnd);
}
static int WINAPI_CALL THETAZ_GetDlgItemTextA_Wrapper(WinHandle hdlg, int id, char* buf, int max) {
    if (x86_dispatch_active()) return THETAZ_GetDlgItemTextA(hdlg, id, buf, max);
    return WINAPI64_GetDlgItemTextA(hdlg, id, buf, max);
}
static BOOL WINAPI_CALL THETAZ_SetDlgItemTextA_Wrapper(WinHandle hdlg, int id, const char* text) {
    if (x86_dispatch_active()) return THETAZ_SetDlgItemTextA(hdlg, id, text);
    return WINAPI64_SetDlgItemTextA(hdlg, id, text);
}
static uint64_t WINAPI_CALL THETAZ_SendDlgItemMessageA_Wrapper(WinHandle hdlg, int id, DWORD message,
                                                               uint64_t wparam, uint64_t lparam) {
    if (x86_dispatch_active()) return THETAZ_SendDlgItemMessageA(hdlg, id, message, wparam, lparam);
    return WINAPI64_SendDlgItemMessageA(hdlg, id, message, wparam, lparam);
}
static BOOL WINAPI_CALL THETAZ_CheckDlgButton_Wrapper(WinHandle hdlg, int id, uint32_t check) {
    if (x86_dispatch_active()) return THETAZ_CheckDlgButton(hdlg, id, check);
    return WINAPI64_CheckDlgButton(hdlg, id, check);
}
static uint32_t WINAPI_CALL THETAZ_IsDlgButtonChecked_Wrapper(WinHandle hdlg, int id) {
    if (x86_dispatch_active()) return THETAZ_IsDlgButtonChecked(hdlg, id);
    return WINAPI64_IsDlgButtonChecked(hdlg, id);
}
static BOOL WINAPI_CALL THETAZ_EnableWindow_Wrapper(WinHandle hwnd, BOOL enable) {
    if (x86_dispatch_active()) return THETAZ_EnableWindow(hwnd, enable);
    return WINAPI64_EnableWindow(hwnd, enable);
}
static BOOL WINAPI_CALL THETAZ_IsWindowEnabled_Wrapper(WinHandle hwnd) {
    if (x86_dispatch_active()) return THETAZ_IsWindowEnabled(hwnd);
    return WINAPI64_IsWindowEnabled(hwnd);
}
static BOOL WINAPI_CALL THETAZ_IsWindowVisible_Wrapper(WinHandle hwnd) {
    if (x86_dispatch_active()) return THETAZ_IsWindowVisible(hwnd);
    return WINAPI64_IsWindowVisible(hwnd);
}
static BOOL WINAPI_CALL THETAZ_IsWindow_Wrapper(WinHandle hwnd) {
    if (x86_dispatch_active()) return THETAZ_IsWindow(hwnd);
    return WINAPI64_IsWindow(hwnd);
}
static int WINAPI_CALL THETAZ_GetWindowTextA_Wrapper(WinHandle hwnd, char* buf, int max) {
    if (x86_dispatch_active()) return THETAZ_GetWindowTextA(hwnd, buf, max);
    return WINAPI64_GetWindowTextA(hwnd, buf, max);
}
static int WINAPI_CALL THETAZ_GetWindowTextLengthA_Wrapper(WinHandle hwnd) {
    if (x86_dispatch_active()) return THETAZ_GetWindowTextLengthA(hwnd);
    return WINAPI64_GetWindowTextLengthA(hwnd);
}
static BOOL WINAPI_CALL THETAZ_SetWindowTextA_Wrapper(WinHandle hwnd, const char* text) {
    if (x86_dispatch_active()) return THETAZ_SetWindowTextA(hwnd, text);
    return WINAPI64_SetWindowTextA(hwnd, text);
}
static void* WINAPI_CALL THETAZ_SetFocus_Wrapper(WinHandle hwnd) {
    if (x86_dispatch_active()) return THETAZ_SetFocus(hwnd);
    return WINAPI64_SetFocus(hwnd);
}
static void* WINAPI_CALL THETAZ_GetFocus_Wrapper() {
    if (x86_dispatch_active()) return THETAZ_GetFocus();
    return WINAPI64_GetFocus();
}
static int WINAPI_CALL THETAZ_DialogBoxParamA_Wrapper(WinHandle hInstance, const char* name,
                                                      WinHandle hWndParent, void* lpDialogFunc,
                                                      uint64_t dwInitParam) {
    if (x86_dispatch_active()) return THETAZ_DialogBoxParamA(hInstance, name, hWndParent, lpDialogFunc, dwInitParam);
    return WINAPI64_DialogBoxParamA(hInstance, name, hWndParent, lpDialogFunc, dwInitParam);
}
static BOOL WINAPI_CALL THETAZ_EndDialog_Wrapper(WinHandle hdlg, int result) {
    if (x86_dispatch_active()) return THETAZ_EndDialog(hdlg, result);
    return WINAPI64_EndDialog(hdlg, result);
}
static int WINAPI_CALL THETAZ_MessageBoxA_Wrapper(WinHandle hwnd, const char* text,
                                                  const char* caption, uint32_t type) {
    if (x86_dispatch_active()) return THETAZ_MessageBoxA(hwnd, text, caption, type);
    return WINAPI64_MessageBoxA(hwnd, text, caption, type);
}

// Keyboard layouts. The guest has no real input server here: one layout is
// exported (the process-wide en-US HKL), which is what a stock US-English
// desktop reports. GetKeyboardLayoutList fills the caller's buffer or, when
// lpList is NULL, just queries the count -- the standard contract.
static const uint32_t kGuestKeyboardLayout = 0x04090409u; // MAKELANGID(EN,US)

static int WINAPI_CALL THETAZ_GetKeyboardLayoutList_Wrapper(int n_buff,
                                                            void* lp_list) {
    if (lp_list && n_buff > 0) {
        *reinterpret_cast<uint32_t*>(lp_list) = kGuestKeyboardLayout;
    }
    return 1;
}

static uint32_t WINAPI_CALL THETAZ_GetKeyboardLayout_Wrapper(void* id_thread) {
    (void)id_thread;
    return kGuestKeyboardLayout;
}

// RegisterWindowMessageA is implemented in kernel32_winword.cpp but registered
// here alongside the rest of the user32 surface (the guest imports it as a
// user32 export, as real Windows does). Declared locally to avoid pulling in
// kernel32.h, whose declarations collide with this file's own definitions.
extern UINT WINAPI_CALL THETAZ_RegisterWindowMessageA(LPCSTR lpString);

static uint32_t THETAZ_RegisterWindowMessageA_Wrapper(const char* name) {
    if (x86_dispatch_active()) return THETAZ_RegisterWindowMessageA(name);
    return 1;
}

// --- Initialization ----------------------------------------------------------

static std::once_flag g_window_init_once;

void initialize_window_layer() {
    std::call_once(g_window_init_once, []() {
        // user32.dll
        register_api("user32.dll", "GetDesktopWindow", (void*)THETAZ_GetDesktopWindow_Wrapper);
        register_api("user32.dll", "RegisterClassA", (void*)THETAZ_RegisterClassA_Wrapper);
        register_api("user32.dll", "RegisterClassW", (void*)THETAZ_RegisterClassW_Wrapper);
        register_api("user32.dll", "RegisterClassExA", (void*)THETAZ_RegisterClassExA_Wrapper);
        register_api("user32.dll", "RegisterClassExW", (void*)THETAZ_RegisterClassExW_Wrapper);
        register_api("user32.dll", "CreateWindowExA", (void*)THETAZ_CreateWindowExA_Wrapper);
        register_api("user32.dll", "DestroyWindow", (void*)THETAZ_DestroyWindow_Wrapper);
        register_api("user32.dll", "ShowWindow", (void*)THETAZ_ShowWindow_Wrapper);
        register_api("user32.dll", "UpdateWindow", (void*)THETAZ_UpdateWindow_Wrapper);
        register_api("user32.dll", "InvalidateRect", (void*)THETAZ_InvalidateRect_Wrapper);
        register_api("user32.dll", "BeginPaint", (void*)THETAZ_BeginPaint_Wrapper);
        register_api("user32.dll", "EndPaint", (void*)THETAZ_EndPaint_Wrapper);
        register_api("user32.dll", "GetMessageA", (void*)THETAZ_GetMessageA_Wrapper);
        register_api("user32.dll", "PeekMessageA", (void*)THETAZ_PeekMessageA_Wrapper);
        register_api("user32.dll", "PostMessageA", (void*)THETAZ_PostMessageA_Wrapper);
        register_api("user32.dll", "SendMessageA", (void*)THETAZ_SendMessageA_Wrapper);
        register_api("user32.dll", "TranslateMessage", (void*)THETAZ_TranslateMessage_Wrapper);
        register_api("user32.dll", "DispatchMessageA", (void*)THETAZ_DispatchMessageA_Wrapper);
        register_api("user32.dll", "SetTimer", (void*)THETAZ_SetTimer_Wrapper);
        register_api("user32.dll", "KillTimer", (void*)THETAZ_KillTimer_Wrapper);
        register_api("user32.dll", "DefWindowProcA", (void*)THETAZ_DefWindowProcA_Wrapper);
        register_api("user32.dll", "PostQuitMessage", (void*)THETAZ_PostQuitMessage_Wrapper);
        register_api("user32.dll", "GetDlgItem", (void*)THETAZ_GetDlgItem_Wrapper);
        register_api("user32.dll", "GetDlgCtrlID", (void*)THETAZ_GetDlgCtrlID_Wrapper);
        register_api("user32.dll", "GetDlgItemTextA", (void*)THETAZ_GetDlgItemTextA_Wrapper);
        register_api("user32.dll", "SetDlgItemTextA", (void*)THETAZ_SetDlgItemTextA_Wrapper);
        register_api("user32.dll", "SendDlgItemMessageA", (void*)THETAZ_SendDlgItemMessageA_Wrapper);
        register_api("user32.dll", "CheckDlgButton", (void*)THETAZ_CheckDlgButton_Wrapper);
        register_api("user32.dll", "IsDlgButtonChecked", (void*)THETAZ_IsDlgButtonChecked_Wrapper);
        register_api("user32.dll", "EnableWindow", (void*)THETAZ_EnableWindow_Wrapper);
        register_api("user32.dll", "IsWindowEnabled", (void*)THETAZ_IsWindowEnabled_Wrapper);
        register_api("user32.dll", "IsWindowVisible", (void*)THETAZ_IsWindowVisible_Wrapper);
        register_api("user32.dll", "IsWindow", (void*)THETAZ_IsWindow_Wrapper);
        register_api("user32.dll", "GetWindowTextA", (void*)THETAZ_GetWindowTextA_Wrapper);
        register_api("user32.dll", "GetWindowTextLengthA", (void*)THETAZ_GetWindowTextLengthA_Wrapper);
        register_api("user32.dll", "SetWindowTextA", (void*)THETAZ_SetWindowTextA_Wrapper);
        register_api("user32.dll", "SetFocus", (void*)THETAZ_SetFocus_Wrapper);
        register_api("user32.dll", "GetFocus", (void*)THETAZ_GetFocus_Wrapper);
        register_api("user32.dll", "DialogBoxParamA", (void*)THETAZ_DialogBoxParamA_Wrapper);
        register_api("user32.dll", "EndDialog", (void*)THETAZ_EndDialog_Wrapper);
        register_api("user32.dll", "MessageBoxA", (void*)THETAZ_MessageBoxA_Wrapper);
        register_api("user32.dll", "GetKeyboardLayoutList",
                     (void*)THETAZ_GetKeyboardLayoutList_Wrapper);
        register_api("user32.dll", "GetKeyboardLayout",
                     (void*)THETAZ_GetKeyboardLayout_Wrapper);
        register_api("user32.dll", "RegisterWindowMessageA",
                     (void*)THETAZ_RegisterWindowMessageA_Wrapper);
        if (!g_timer_thread.joinable()) {
            g_timer_thread = std::thread(timer_thread_main);
            g_timer_thread.detach();
        }
        if (x11_display_init() && !g_x11_thread.joinable()) {
            g_x11_thread = std::thread(x11_event_loop);
            g_x11_thread.detach();
        }
    });
}

// Renders UTF-8 text into the X11 window backing the guest hwnd. Called from
// the gdi32 TextOutA/W implementations after the guest paints. Declared in
// api.h at ThetaZ scope, so it lives here outside any anonymous namespace.
void theta_gdi_text(uint64_t hwnd, int x, int y, const char* utf8_text) {
    if (!g_x11_display || !utf8_text) return;
    ::Window xw = 0;
    {
        std::lock_guard<std::mutex> lock(g_window_mutex);
        auto it = g_x11_windows.find(hwnd);
        if (it == g_x11_windows.end()) return;
        xw = it->second;
    }
    std::lock_guard<std::mutex> lock(g_x11_mutex);
    unsigned long fg = WhitePixel(g_x11_display, DefaultScreen(g_x11_display));
    XSetForeground(g_x11_display, DefaultGC(g_x11_display, 0), fg);
    XDrawString(g_x11_display, xw, DefaultGC(g_x11_display, 0),
                x, y, utf8_text, static_cast<int>(strlen(utf8_text)));
    XFlush(g_x11_display);
}

} // namespace ThetaZ
