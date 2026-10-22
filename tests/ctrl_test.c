/* Controls test: BUTTON/STATIC predefined classes, BM_GETCHECK/SETCHECK/CLICK,
 * WM_COMMAND dispatch to the parent WNDPROC (32-bit guest). */
#include <windows.h>
#include <stdio.h>

static int g_commands[8];
static int g_ncmd;

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_COMMAND:
            if (g_ncmd < 8) g_commands[g_ncmd++] = (int)LOWORD(wparam);
            if (g_ncmd == 2) PostQuitMessage(0);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE hinst, HINSTANCE prev, LPSTR cmd, int show) {
    (void)prev; (void)cmd; (void)show;
    int fails = 0;
    WNDCLASSA wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = wndproc;
    wc.hInstance = hinst;
    wc.lpszClassName = "ControlsTest";
    if (!RegisterClassA(&wc)) { printf("FAIL: RegisterClassA\n"); fails++; }

    HWND main = CreateWindowExA(0, "ControlsTest", "Controls", WS_OVERLAPPEDWINDOW,
                                100, 100, 400, 300, NULL, NULL, hinst, NULL);
    if (!main) { printf("FAIL: CreateWindowExA main\n"); fails++; }

    HWND btn = CreateWindowExA(0, "BUTTON", "Click Me",
                               WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                               20, 20, 120, 32, main, (HMENU)1, hinst, NULL);
    if (!btn) { printf("FAIL: CreateWindowExA BUTTON\n"); fails++; }
    else {
        HWND chk = CreateWindowExA(0, "BUTTON", "Enable",
                                   WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                   20, 60, 120, 32, main, (HMENU)2, hinst, NULL);
        HWND stc = CreateWindowExA(0, "STATIC", "Hello static",
                                   WS_CHILD | WS_VISIBLE,
                                   20, 100, 200, 20, main, (HMENU)3, hinst, NULL);
        if (!chk) { printf("FAIL: CreateWindowExA checkbox\n"); fails++; }
        if (!stc) { printf("FAIL: CreateWindowExA STATIC\n"); fails++; }

        if (SendMessageA(chk, BM_GETCHECK, 0, 0) != 0) { printf("FAIL: BM_GETCHECK init\n"); fails++; }
        SendMessageA(chk, BM_SETCHECK, 1, 0);
        if (SendMessageA(chk, BM_GETCHECK, 0, 0) != 1) { printf("FAIL: BM_SETCHECK/GETCHECK\n"); fails++; }

        SendMessageA(btn, BM_CLICK, 0, 0);   /* -> WM_COMMAND id=1 to parent */
        SendMessageA(chk, BM_CLICK, 0, 0);   /* -> WM_COMMAND id=2 to parent */

        if (g_ncmd != 2 || g_commands[0] != 1 || g_commands[1] != 2) {
            printf("FAIL: WM_COMMAND ids=%d,%d n=%d\n",
                   g_ncmd > 0 ? g_commands[0] : -1,
                   g_ncmd > 1 ? g_commands[1] : -1, g_ncmd);
            fails++;
        }
    }

    if (fails == 0) {
        printf("CTRL PASS\n");
        ExitProcess(0);
    }
    ExitProcess(1);
}
