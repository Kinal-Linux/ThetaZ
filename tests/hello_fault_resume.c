// ThetaZ unhandled-exception filter resume test: the top-level filter may fix
// up the CONTEXT (advance RIP past the faulting `idiv`) and return
// EXCEPTION_CONTINUE_EXECUTION, and execution must resume there.
#include <windows.h>
#include <stdio.h>

static LONG WINAPI uef_filter(PEXCEPTION_POINTERS pep) {
    printf("uef_code=%lx\n",
           (unsigned long)pep->ExceptionRecord->ExceptionCode);
    fflush(stdout);

    unsigned char* ip = (unsigned char*)pep->ExceptionRecord->ExceptionAddress;
    // F7 /6 is `div`, F7 /7 is `idiv`; decode a ModRM length so we can skip it.
    if (ip[0] == 0xF7) {
        int reg = (ip[1] >> 3) & 7;
        if (reg == 6 || reg == 7) {
            int mod = ip[1] >> 6;
            int rm = ip[1] & 7;
            size_t len = 2;
            if (mod != 3) {
                if (rm == 4) len += 1;
                if (mod == 0 && rm == 5) len += 4;
                else if (mod == 1) len += 1;
                else if (mod == 2) len += 4;
            }
            pep->ContextRecord->Rip = (DWORD64)(ip + len);
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

int main(void) {
    SetUnhandledExceptionFilter(uef_filter);
    volatile int a = 1, b = 0;
    volatile int c = a / b;
    printf("after_fault=%d\n", c);
    printf("fault_resumed\n");
    return 0;
}
