// ThetaZ window/message-pump test: real class registry, CreateWindowEx, WM_CREATE
// dispatch, SendMessage, paint cycle, and a GetMessage loop with WM_QUIT.
#include <windows.h>
#include <stdio.h>

static int g_create_count = 0;
static int g_custom_msg = 0;
static int g_destroyed = 0;

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    (void)hwnd;
    (void)lparam;
    switch (msg) {
        case WM_CREATE:
            g_create_count++;
            return 0;
        case WM_USER + 7:
            g_custom_msg = (int)wparam;
            return 0;
        case WM_USER + 8:
            g_custom_msg = (int)wparam;
            return 0;
        case WM_DESTROY:
            g_destroyed = 1;
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

int main(void) {
    WNDCLASSA wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "TestClass";
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(0, "TestClass", "ThetaZ Window", 0,
                                10, 20, 300, 200, NULL, NULL, wc.hInstance, NULL);
    printf("hwnd=%p\n", hwnd);
    printf("create_count=%d\n", g_create_count);

    SendMessageA(hwnd, WM_USER + 7, 42, 0);
    printf("custom_msg=%d\n", g_custom_msg);

    PAINTSTRUCT ps;
    InvalidateRect(hwnd, NULL, TRUE);
    HDC hdc = BeginPaint(hwnd, &ps);
    printf("paint_hdc=%p\n", hdc);
    EndPaint(hwnd, &ps);

    PostMessageA(hwnd, WM_USER + 8, 77, 0);
    PostMessageA(hwnd, WM_CLOSE, 0, 0);

    MSG msg;
    int loop_rounds = 0;
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        loop_rounds++;
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    printf("posted_msg=%d\n", g_custom_msg);
    printf("destroyed=%d\n", g_destroyed);
    printf("loop_rounds=%d\n", loop_rounds);
    printf("win_done\n");
    return 0;
}
