// ThetaZ parent process test: spawns a child via CreateProcessA,
// waits for it, and checks exit code.
#include <windows.h>
#include <stdio.h>

int main(int argc, char** argv) {
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);

    const char* child_path = (argc >= 2) ? argv[1] : "tests/hello_child.exe";
    printf("spawning %s\n", child_path);
    BOOL ok = CreateProcessA(child_path, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    printf("create_ok=%d\n", (int)ok);
    if (!ok) { printf("CreateProcessA failed\n"); return 1; }
    printf("child_pid=%lu child_tid=%lu\n", (unsigned long)pi.dwProcessId, (unsigned long)pi.dwThreadId);
    printf("hProcess=%p hThread=%p\n", pi.hProcess, pi.hThread);

    printf("waiting...\n");
    DWORD wait = WaitForSingleObject(pi.hProcess, 5000);
    printf("wait_result=%lu\n", (unsigned long)wait);

    DWORD code = 0;
    BOOL got = GetExitCodeProcess(pi.hProcess, &code);
    printf("got_exit_code=%d exit_code=%lu\n", (int)got, (unsigned long)code);

    DWORD wt = WaitForSingleObject(pi.hThread, 5000);
    printf("wait_thread=%lu\n", (unsigned long)wt);

    printf("parent_done\n");
    return 0;
}
