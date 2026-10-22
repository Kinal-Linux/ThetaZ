// ThetaZ LoadLibrary test: load a guest DLL, resolve its exports, call them.
#include <windows.h>
#include <stdio.h>

typedef int (__stdcall* fn_t)(int, int);

int main(void) {
    HMODULE lib = LoadLibraryA("mylib.dll");
    if (!lib) { printf("dll_load_fail\n"); return 1; }

    fn_t add = (fn_t)GetProcAddress(lib, "add");
    fn_t sub = (fn_t)GetProcAddress(lib, "sub");
    if (!add || !sub) { printf("dll_proc_fail\n"); return 2; }

    printf("dll_add=%d\n", add(2, 3));
    printf("dll_sub=%d\n", sub(10, 4));

    HMODULE again = GetModuleHandleA("mylib.dll");
    printf("dll_handle_match=%d\n", again == lib);
    return 0;
}
