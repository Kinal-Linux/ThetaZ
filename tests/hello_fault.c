// ThetaZ unhandled-exception filter test: a fault no SEH frame claims must
// reach the SetUnhandledExceptionFilter top-level filter, which is called with
// an EXCEPTION_POINTERS* and may inspect the exception record.
#include <windows.h>
#include <stdio.h>

static LONG WINAPI uef_filter(PEXCEPTION_POINTERS pep) {
    printf("uef_code=%lx\n",
           (unsigned long)pep->ExceptionRecord->ExceptionCode);
    fflush(stdout);
    return EXCEPTION_CONTINUE_SEARCH;
}

int main(void) {
    SetUnhandledExceptionFilter(uef_filter);
    volatile int a = 1, b = 0;
    volatile int c = a / b;
    printf("after_fault=%d\n", c);
    return 0;
}
