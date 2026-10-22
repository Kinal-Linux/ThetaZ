// ThetaZ child process test: prints child pid and exits with code 7.
#include <windows.h>
#include <stdio.h>

int main(void) {
    printf("child_running\n");
    printf("child_pid=%lu\n", (unsigned long)GetCurrentProcessId());
    printf("child_exit=7\n");
    return 7;
}
