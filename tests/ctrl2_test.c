/* Controls test 2: EDIT / LISTBOX / COMBOBOX message behavior plus dialog
 * accessors (GetDlgItem / GetDlgItemTextA / SetDlgItemTextA / SendDlgItemMessageA /
 * CheckDlgButton / IsDlgButtonChecked / EnableWindow / IsWindowEnabled). */
#include <windows.h>
#include <stdio.h>

static int g_cmd_id = -1;
static int g_cmd_code = -1;

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_COMMAND:
            g_cmd_id = (int)LOWORD(wparam);
            g_cmd_code = (int)HIWORD(wparam);
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
    wc.lpszClassName = "ControlsTest2";
    if (!RegisterClassA(&wc)) { printf("FAIL: RegisterClassA\n"); fails++; }

    HWND main = CreateWindowExA(0, "ControlsTest2", "Controls2", WS_OVERLAPPEDWINDOW,
                                100, 100, 500, 400, NULL, NULL, hinst, NULL);
    if (!main) { printf("FAIL: CreateWindowExA main\n"); fails++; }

    /* --- EDIT --- */
    HWND edit = CreateWindowExA(0, "EDIT", "initial", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                20, 20, 200, 24, main, (HMENU)10, hinst, NULL);
    if (!edit) { printf("FAIL: CreateWindowExA EDIT\n"); fails++; }
    else {
        if (SendMessageA(edit, WM_GETTEXTLENGTH, 0, 0) != 7)
            { printf("FAIL: EDIT WM_GETTEXTLENGTH=%d\n", (int)SendMessageA(edit, WM_GETTEXTLENGTH, 0, 0)); fails++; }
        char buf[64];
        if (SendMessageA(edit, WM_GETTEXT, (WPARAM)sizeof(buf), (LPARAM)buf) != 7 ||
            strcmp(buf, "initial") != 0)
            { printf("FAIL: EDIT WM_GETTEXT=\"%s\"\n", buf); fails++; }
        SendMessageA(edit, WM_SETTEXT, 0, (LPARAM)"hello world");
        if (SendMessageA(edit, WM_GETTEXTLENGTH, 0, 0) != 11)
            { printf("FAIL: EDIT WM_SETTEXT len\n"); fails++; }
        SendMessageA(edit, EM_SETSEL, 0, 5);
        SendMessageA(edit, EM_REPLACESEL, 0, (LPARAM)"HELLO");
        if (SendMessageA(edit, WM_GETTEXTLENGTH, 0, 0) != 11)
            { printf("FAIL: EDIT EM_REPLACESEL len=%d\n", (int)SendMessageA(edit, WM_GETTEXTLENGTH, 0, 0)); fails++; }
        SendMessageA(edit, WM_GETTEXT, (WPARAM)sizeof(buf), (LPARAM)buf);
        if (strcmp(buf, "HELLO world") != 0)
            { printf("FAIL: EDIT EM_REPLACESEL=\"%s\"\n", buf); fails++; }
        DWORD sel = (DWORD)SendMessageA(edit, EM_GETSEL, 0, 0);
        if (LOWORD(sel) != 5 || HIWORD(sel) != 5)
            { printf("FAIL: EDIT EM_GETSEL=%lx\n", sel); fails++; }
    }

    /* --- LISTBOX --- */
    HWND lb = CreateWindowExA(0, "LISTBOX", NULL, WS_CHILD | WS_VISIBLE | LBS_NOTIFY,
                              20, 60, 180, 120, main, (HMENU)11, hinst, NULL);
    if (!lb) { printf("FAIL: CreateWindowExA LISTBOX\n"); fails++; }
    else {
        int i0 = (int)SendMessageA(lb, LB_ADDSTRING, 0, (LPARAM)"alpha");
        int i1 = (int)SendMessageA(lb, LB_ADDSTRING, 0, (LPARAM)"beta");
        int i2 = (int)SendMessageA(lb, LB_ADDSTRING, 0, (LPARAM)"gamma");
        if (i0 != 0 || i1 != 1 || i2 != 2)
            { printf("FAIL: LB_ADDSTRING idx %d,%d,%d\n", i0, i1, i2); fails++; }
        if (SendMessageA(lb, LB_GETCOUNT, 0, 0) != 3)
            { printf("FAIL: LB_GETCOUNT=%d\n", (int)SendMessageA(lb, LB_GETCOUNT, 0, 0)); fails++; }
        SendMessageA(lb, LB_SETCURSEL, 1, 0);
        if (SendMessageA(lb, LB_GETCURSEL, 0, 0) != 1)
            { printf("FAIL: LB_SET/GETCURSEL=%d\n", (int)SendMessageA(lb, LB_GETCURSEL, 0, 0)); fails++; }
        char lbbuf[64];
        if (SendMessageA(lb, LB_GETTEXT, 1, (LPARAM)lbbuf) != 4 || strcmp(lbbuf, "beta") != 0)
            { printf("FAIL: LB_GETTEXT=\"%s\"\n", lbbuf); fails++; }
        if (SendMessageA(lb, LB_GETTEXTLEN, 2, 0) != 5)
            { printf("FAIL: LB_GETTEXTLEN=%d\n", (int)SendMessageA(lb, LB_GETTEXTLEN, 2, 0)); fails++; }
        SendMessageA(lb, LB_SETITEMDATA, 2, 0x12345678);
        if (SendMessageA(lb, LB_GETITEMDATA, 2, 0) != 0x12345678)
            { printf("FAIL: LB_SET/GETITEMDATA\n"); fails++; }
        SendMessageA(lb, LB_RESETCONTENT, 0, 0);
        if (SendMessageA(lb, LB_GETCOUNT, 0, 0) != 0)
            { printf("FAIL: LB_RESETCONTENT count=%d\n", (int)SendMessageA(lb, LB_GETCOUNT, 0, 0)); fails++; }
    }

    /* --- COMBOBOX --- */
    HWND cb = CreateWindowExA(0, "COMBOBOX", NULL, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                              20, 200, 200, 200, main, (HMENU)12, hinst, NULL);
    if (!cb) { printf("FAIL: CreateWindowExA COMBOBOX\n"); fails++; }
    else {
        SendMessageA(cb, CB_ADDSTRING, 0, (LPARAM)"red");
        SendMessageA(cb, CB_ADDSTRING, 0, (LPARAM)"green");
        SendMessageA(cb, CB_ADDSTRING, 0, (LPARAM)"blue");
        if (SendMessageA(cb, CB_GETCOUNT, 0, 0) != 3)
            { printf("FAIL: CB_GETCOUNT=%d\n", (int)SendMessageA(cb, CB_GETCOUNT, 0, 0)); fails++; }
        SendMessageA(cb, CB_SETCURSEL, 2, 0);
        if (SendMessageA(cb, CB_GETCURSEL, 0, 0) != 2)
            { printf("FAIL: CB_SET/GETCURSEL=%d\n", (int)SendMessageA(cb, CB_GETCURSEL, 0, 0)); fails++; }
        char cbbuf[64];
        if (SendMessageA(cb, CB_GETLBTEXT, 2, (LPARAM)cbbuf) != 4 || strcmp(cbbuf, "blue") != 0)
            { printf("FAIL: CB_GETLBTEXT=\"%s\"\n", cbbuf); fails++; }
        SendMessageA(cb, CB_SETCURSEL, 0, 0);
        if (SendMessageA(cb, CB_GETLBTEXTLEN, 0, 0) != 3)
            { printf("FAIL: CB_GETLBTEXTLEN=%d\n", (int)SendMessageA(cb, CB_GETLBTEXTLEN, 0, 0)); fails++; }
        if (SendMessageA(cb, CB_GETDROPPEDSTATE, 0, 0) != 0)
            { printf("FAIL: CB_GETDROPPEDSTATE init\n"); fails++; }
        SendMessageA(cb, CB_SHOWDROPDOWN, TRUE, 0);
        if (SendMessageA(cb, CB_GETDROPPEDSTATE, 0, 0) != 1)
            { printf("FAIL: CB_SHOWDROPDOWN\n"); fails++; }
        SendMessageA(cb, CB_RESETCONTENT, 0, 0);
        if (SendMessageA(cb, CB_GETCOUNT, 0, 0) != 0)
            { printf("FAIL: CB_RESETCONTENT\n"); fails++; }
    }

    /* --- Dialog accessors (using 'main' as the dialog owner) --- */
    if (GetDlgItem(main, 10) != edit)
        { printf("FAIL: GetDlgItem 10\n"); fails++; }
    if (GetDlgItem(main, 11) != lb)
        { printf("FAIL: GetDlgItem 11\n"); fails++; }
    if (GetDlgCtrlID(edit) != 10)
        { printf("FAIL: GetDlgCtrlID=%d\n", GetDlgCtrlID(edit)); fails++; }

    char dlgbuf[64];
    SetDlgItemTextA(main, 10, "dlg text");
    if (GetDlgItemTextA(main, 10, dlgbuf, sizeof(dlgbuf)) != 8 || strcmp(dlgbuf, "dlg text") != 0)
        { printf("FAIL: Set/GetDlgItemTextA=\"%s\"\n", dlgbuf); fails++; }

    SendDlgItemMessageA(main, 10, WM_SETTEXT, 0, (LPARAM)"via dlg msg");
    if (GetDlgItemTextA(main, 10, dlgbuf, sizeof(dlgbuf)) != 11 || strcmp(dlgbuf, "via dlg msg") != 0)
        { printf("FAIL: SendDlgItemMessageA WM_SETTEXT=\"%s\"\n", dlgbuf); fails++; }

    /* checkbox via dialog accessors */
    HWND chk = CreateWindowExA(0, "BUTTON", "opt", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                               20, 330, 120, 24, main, (HMENU)13, hinst, NULL);
    if (!chk) { printf("FAIL: CreateWindowExA checkbox2\n"); fails++; }
    else {
        CheckDlgButton(main, 13, BST_CHECKED);
        if (IsDlgButtonChecked(main, 13) != BST_CHECKED)
            { printf("FAIL: Check/IsDlgButtonChecked=%d\n", IsDlgButtonChecked(main, 13)); fails++; }
    }

    /* enable/disable */
    if (!IsWindowEnabled(edit))
        { printf("FAIL: IsWindowEnabled init\n"); fails++; }
    EnableWindow(edit, FALSE);
    if (IsWindowEnabled(edit))
        { printf("FAIL: EnableWindow(FALSE)\n"); fails++; }
    EnableWindow(edit, TRUE);
    if (!IsWindowEnabled(edit))
        { printf("FAIL: EnableWindow(TRUE)\n"); fails++; }
    if (!IsWindow(main) || IsWindow((HWND)0x12345))
        { printf("FAIL: IsWindow\n"); fails++; }

    /* LBN_SELCHANGE notification via SendMessage BM_CLICK-ish path is not
     * synchronous; verify GetWindowTextA on the edit instead. */
    SetWindowTextA(edit, "setwin");
    if (GetWindowTextA(edit, dlgbuf, sizeof(dlgbuf)) != 6 || strcmp(dlgbuf, "setwin") != 0)
        { printf("FAIL: Set/GetWindowTextA=\"%s\"\n", dlgbuf); fails++; }
    if (GetWindowTextLengthA(edit) != 6)
        { printf("FAIL: GetWindowTextLengthA=%d\n", GetWindowTextLengthA(edit)); fails++; }

    if (fails == 0) {
        printf("CTRL2 PASS\n");
        ExitProcess(0);
    }
    ExitProcess(1);
}
