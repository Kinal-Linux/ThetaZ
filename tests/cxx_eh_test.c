/* __CxxFrameHandler end-to-end test (i686-w64-mingw32, no CRT C++).
 * Builds MSVC-style V4 (0x19930522) C++ EH registration records + FuncInfo
 * tables on the guest, then throws via the imported _CxxThrowException.
 * Verifies:
 *   - pass 1: inner frame's catch type ("WrongType") does NOT match, the
 *     outer frame's catch of a BASE class of the thrown type DOES match
 *     (via the throw's CatchableTypeArray), with the PMD subobject
 *     displacement (Base is at thrown_obj + 4);
 *   - the catch funclet is entered via context resume with EDX = the
 *     adjusted exception object and EBP = the saved frame ebp (frameless
 *     x86 funclet ABI), and exits by JUMPING to the continuation;
 *   - pass 2: the inner frame's unwind funclet (destructor cleanup) ran.
 */
#include <windows.h>
#include <stdio.h>

volatile unsigned int g_caught = 0;
volatile int g_unwound = 0;

struct FuncInfo {
    unsigned int magic, maxState;
    void* pUnwindMap;
    unsigned int nTryBlocks;
    void* pTryBlockMap;
    unsigned int nIPMapEntries;
    void* pIPToStateMap;
    unsigned int nCatchHandlers;
    void* pCatchHandlerArray;
    unsigned int flags;
    void* pUnwindMapForUnexpected;
};
struct UnwindMapEntry { unsigned int toState; void* action; };
struct TryBlockMapEntry { int tryLow, tryHigh, catchHigh; unsigned int nCatches; void* pHandlerArray; };
struct CatchHandlerType { unsigned int adjectives; void* pType; void* handler; };
struct IPToState { unsigned int ip, state; };
struct TypeDescriptor { void* pVFTable; void* spare; char name[32]; };
struct PMD { int mdisp, pdisp, vdisp; };
struct CatchableType { unsigned int properties; void* pType; struct PMD disp; unsigned int sizeOrOffset; void* copyFunction; };
struct CatchableTypeArray { int n; void* pCatchableTypes[2]; };
struct ThrowInfo { unsigned int attributes; void* pmfnUnwind; void* pForwardCompat; void* pCatchableTypeArray; };
struct EH4Reg { void* prev; void* handler; void* ebp; unsigned int cookie; void* pFuncInfo; unsigned int tryLevel; void* xpointers; };

static int thrown_obj[2] = { 0x11111111, 0x22222222 };

static struct TypeDescriptor tdDerived = { 0, 0, ".?AVDerived@@" };
static struct TypeDescriptor tdBase    = { 0, 0, ".?AVBase@@" };
static struct TypeDescriptor tdWrong   = { 0, 0, ".?AVWrongType@@" };

static struct CatchableType ctDerived = { 0, &tdDerived, { 0, -1, 0 }, 4, 0 };
static struct CatchableType ctBase    = { 0, &tdBase,    { 4, -1, 0 }, 4, 0 };
static struct CatchableTypeArray cta  = { 2, { &ctDerived, &ctBase } };
static struct ThrowInfo throwInfo     = { 0, 0, 0, &cta };

static struct CatchHandlerType catchB  = { 0, &tdBase, 0 };   /* handler patched below */
static struct CatchHandlerType catchA  = { 0, &tdWrong, 0 };
static struct TryBlockMapEntry tryB    = { 1, 2, 0, 1, &catchB };
static struct TryBlockMapEntry tryA    = { 1, 2, 0, 1, &catchA };
static struct IPToState ipMapB[2]      = { { 0, 0 }, { 0, 1 } };
static struct IPToState ipMapA[2]      = { { 0, 0 }, { 0, 1 } };
static struct UnwindMapEntry uwA[2]    = { { 0, 0 }, { 0, 0 } };
static struct UnwindMapEntry uwB[2]    = { { 0, 0 }, { 0, 0 } };
static struct FuncInfo fiA, fiB;

static void catch_funclet(void);
static void unwind_funclet(void);
void continuation(void);
static void do_throw(void);

/* Pass-2 cleanup funclet for the inner frame: plain function, returns. */
static void unwind_funclet(void) {
    g_unwound = 1;
}

/* Catch funclet: frameless MSVC x86 ABI — exception object in EDX, no return
 * address (entered via context resume), exits by jumping to the
 * continuation inside the "function". */
__attribute__((naked)) static void catch_funclet(void) {
    __asm__ volatile(
        "movl %edx, %eax\n\t"
        "movl %eax, _g_caught\n\t"
        "jmp _continuation\n\t");
}

void continuation(void) {
    unsigned int expected = (unsigned int)(unsigned long)&thrown_obj + 4; /* Base subobject */
    int ok = (g_caught == expected) && g_unwound;
    printf("CXX %s caught=0x%x expected=0x%x unwound=%d\n",
           ok ? "PASS" : "FAIL", g_caught, expected, g_unwound);
    ExitProcess(ok ? 0 : 2);
}

static void do_throw(void) {
    void* h = LoadLibraryA("vcruntime140.dll");
    void* throwfn = (void*)GetProcAddress(h, "_CxxThrowException");
    if (!throwfn) { printf("no _CxxThrowException thunk\n"); ExitProcess(3); }
    ((void (__cdecl*)(void*, void*))throwfn)(&thrown_obj, &throwInfo);
    printf("fell through (unhandled)\n");
    ExitProcess(4);
}

static unsigned int get_ebp(void) {
    unsigned int ebp;
    __asm__("movl %%ebp, %0" : "=r"(ebp));
    return ebp;
}

int main(void) {
    HMODULE h = LoadLibraryA("vcruntime140.dll");
    void* handler = (void*)GetProcAddress(h, "__CxxFrameHandler3");
    if (!handler) { printf("no __CxxFrameHandler3 thunk\n"); return 3; }

    unsigned int code = (unsigned int)(unsigned long)&do_throw;
    unsigned int ebp = get_ebp();

    /* Outer frame B (matching catch Base), installed first. */
    fiB.magic = 0x19930522;
    fiB.maxState = 1;
    fiB.pUnwindMap = &uwB;
    fiB.nTryBlocks = 1;
    fiB.pTryBlockMap = &tryB;
    fiB.nIPMapEntries = 2;
    fiB.pIPToStateMap = &ipMapB;
    fiB.nCatchHandlers = 1;
    fiB.pCatchHandlerArray = &catchB;
    fiB.flags = 0;
    fiB.pUnwindMapForUnexpected = 0;
    uwB[1].toState = 0; uwB[1].action = 0;
    ipMapB[1].ip = code; ipMapB[1].state = 1;
    catchB.handler = (void*)&catch_funclet;

    /* Inner frame A (non-matching catch WrongType), installed second. */
    fiA.magic = 0x19930522;
    fiA.maxState = 1;
    fiA.pUnwindMap = &uwA;
    fiA.nTryBlocks = 1;
    fiA.pTryBlockMap = &tryA;
    fiA.nIPMapEntries = 2;
    fiA.pIPToStateMap = &ipMapA;
    fiA.nCatchHandlers = 1;
    fiA.pCatchHandlerArray = &catchA;
    fiA.flags = 0;
    fiA.pUnwindMapForUnexpected = 0;
    uwA[1].toState = 0; uwA[1].action = (void*)&unwind_funclet;
    ipMapA[1].ip = code; ipMapA[1].state = 1;
    catchA.handler = (void*)&catch_funclet;

    struct EH4Reg regB, regA;
    __asm__ volatile("mov %%fs:0, %0" : "=r"(regB.prev));
    regB.handler = handler;
    regB.ebp = (void*)ebp;
    regB.cookie = 0;
    regB.pFuncInfo = &fiB;
    regB.tryLevel = 1;
    regB.xpointers = 0;
    __asm__ volatile("mov %0, %%fs:0" :: "r"(&regB));

    regA.prev = &regB;
    regA.handler = handler;
    regA.ebp = (void*)ebp;
    regA.cookie = 0;
    regA.pFuncInfo = &fiA;
    regA.tryLevel = 1;
    regA.xpointers = 0;
    __asm__ volatile("mov %0, %%fs:0" :: "r"(&regA));

    do_throw();
    printf("fell through main\n");
    return 5;
}
