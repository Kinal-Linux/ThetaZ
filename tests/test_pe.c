// Minimal test PE entry point for the ThetaZ loader test suite.
// Each test overrides behavior via one of the entry functions below;
// the runner selects which by -D entry symbol.

#if defined(TEST_GDI)
#include <stdio.h>
#include <string.h>

typedef void* WinHandle;
typedef int BOOL;
#define FALSE_VALUE 0
#define TRUE_VALUE 1
#define NULL_BRUSH ((WinHandle)0x8001)
#define BLACK_BRUSH ((WinHandle)0x8002)
#define WHITE_BRUSH ((WinHandle)0x8003)
#define NULL_PEN ((WinHandle)0x8004)
#define BLACK_PEN ((WinHandle)0x8005)
#define WHITE_PEN ((WinHandle)0x8006)

extern WinHandle GetStockObject(int index);
extern WinHandle SelectObject(void* dc, WinHandle obj);
extern WinHandle CreateSolidBrush(unsigned int color);
extern BOOL DeleteObject(WinHandle handle);
extern void TextOutA(void* hdc, int x, int y, const char* text, int count);
extern void ExitProcess(int code);

int main(void) {
    WinHandle black_brush = GetStockObject(11);  // BLACK_BRUSH
    WinHandle white_pen = GetStockObject(29);    // WHITE_PEN
    printf("gdi: black_brush=%p white_pen=%p\n", black_brush, white_pen);

    WinHandle prev_brush = SelectObject(NULL, black_brush);
    WinHandle prev_pen = SelectObject(NULL, white_pen);
    printf("gdi: prev_brush=%p prev_pen=%p\n", prev_brush, prev_pen);

    WinHandle custom_brush = CreateSolidBrush(0xFF0000);
    printf("gdi: custom_brush=%p\n", custom_brush);

    BOOL stock_del = DeleteObject(BLACK_BRUSH);
    BOOL custom_del = DeleteObject(custom_brush);
    printf("gdi: stock_delete=%d custom_delete=%d\n", (int)stock_del, (int)custom_del);

    TextOutA(NULL, 10, 20, "Hello from GDI", 17);
    TextOutA(NULL, 30, 40, "ThetaZ", 6);

    printf("gdi_done\n");
    ExitProcess(0);
}
#else
#include <windows.h>

#if defined(TEST_RETURN)
void entry(void) {
    ExitProcess(42);
}
#elif defined(TEST_TICKS)
void entry(void) {
    DWORD t1 = GetTickCount();
    DWORD t2 = GetTickCount();
    DWORD t3 = GetTickCount64();
    if (t1 == 0) ExitProcess(1);
    if (t2 < t1) ExitProcess(2);
    if (t3 < t1) ExitProcess(3);
    ExitProcess(5);
}
#elif defined(TEST_MEMORY)
void entry(void) {
    char* r = (char*)VirtualAlloc(0, 0x4000, 0x2000, 0x04);
    if (!r) ExitProcess(1);
    void* c = VirtualAlloc(r, 0x1000, 0x1000, 0x04);
    if (c != (void*)r) ExitProcess(2);
    ((char*)c)[0] = 0x42;
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(c, &mbi, sizeof(mbi)) == 0) ExitProcess(3);
    if (mbi.State != 0x1000) ExitProcess(4);
    if (mbi.Protect != 0x04) ExitProcess(5);
    if (mbi.BaseAddress != c) ExitProcess(6);
    DWORD old = 0;
    if (!VirtualProtect(c, 0x1000, 0x02, &old)) ExitProcess(7);
    if (old != 0x04) ExitProcess(8);
    if (VirtualQuery(c, &mbi, sizeof(mbi)) == 0) ExitProcess(9);
    if (mbi.Protect != 0x02) ExitProcess(10);
    if (!VirtualFree(c, 0x1000, 0x4000)) ExitProcess(11);
    if (VirtualQuery(c, &mbi, sizeof(mbi)) == 0) ExitProcess(12);
    if (mbi.State != 0x2000) ExitProcess(13);
    if (!VirtualFree(r, 0, 0x8000)) ExitProcess(14);
    ExitProcess(0x11);
}
#elif defined(TEST_FILE)
#include <stdio.h>
void entry(void) {
    HANDLE f = CreateFileA("thetaz_test_out.txt", 0x40000000, 0, 0, 2, 0, 0);
    if (f == (HANDLE)-1) { ExitProcess(1); }
    DWORD written = 0;
    const char* msg = "Hello from ThetaZ\n";
    WriteFile(f, msg, 18, &written, 0);
    CloseHandle(f);
    ExitProcess(0x22);
}
#elif defined(TEST_SEH)
#include <stdint.h>

typedef struct { void* handler; void* prev; } SEH_FRAME;

static uintptr_t g_resume;

static DWORD WINAPI seh_handler(PEXCEPTION_RECORD rec, void* frame,
                                PCONTEXT ctx, void* disp) {
    (void)frame; (void)disp;
    if (rec->ExceptionCode != 0xC0000005) return 0;
    ctx->Rip = (DWORD64)g_resume;
    return 0xFFFFFFFF; /* EXCEPTION_CONTINUE_EXECUTION */
}

void entry(void) {
    SEH_FRAME frame;
    frame.prev = 0;
    frame.handler = (void*)seh_handler;
    __asm__ __volatile__("mov %0, %%gs:0" : : "r"(&frame) : "memory");

    g_resume = (uintptr_t)&&after_fault;
    volatile int* p = (volatile int*)0;
    *p = 1; /* SIGSEGV -> SEH dispatch */

after_fault:
    ExitProcess(0x51);
}
#elif defined(TEST_GDI)
// GDI test: use our own types to avoid MinGW conflicts
typedef void* WinHandle;
typedef int BOOL;
#define FALSE_VALUE 0
#define TRUE_VALUE 1
#define NULL_BRUSH ((WinHandle)0x8001)
#define BLACK_BRUSH ((WinHandle)0x8002)
#define WHITE_BRUSH ((WinHandle)0x8003)
#define NULL_PEN ((WinHandle)0x8004)
#define BLACK_PEN ((WinHandle)0x8005)
#define WHITE_PEN ((WinHandle)0x8006)

// Import GDI functions from ThetaZ's gdi32 (implemented in user32.cpp)
extern WinHandle GetStockObject(int index);
extern WinHandle SelectObject(void* dc, WinHandle obj);
extern WinHandle CreateSolidBrush(unsigned int color);
extern BOOL DeleteObject(WinHandle handle);
extern void TextOutA(void* hdc, int x, int y, const char* text, int count);
extern void ExitProcess(int code);

void entry(void) {
    // Test GetStockObject
    WinHandle black_brush = GetStockObject(11);  // BLACK_BRUSH
    WinHandle white_pen = GetStockObject(29);    // WHITE_PEN
    printf("gdi: black_brush=%p white_pen=%p\n", black_brush, white_pen);

    // Test SelectObject
    WinHandle prev_brush = SelectObject(NULL, black_brush);
    WinHandle prev_pen = SelectObject(NULL, white_pen);
    printf("gdi: prev_brush=%p prev_pen=%p\n", prev_brush, prev_pen);

    // Test CreateSolidBrush
    WinHandle custom_brush = CreateSolidBrush(0xFF0000);
    printf("gdi: custom_brush=%p\n", custom_brush);

    // Test DeleteObject
    BOOL stock_del = DeleteObject(BLACK_BRUSH);
    BOOL custom_del = DeleteObject(custom_brush);
    printf("gdi: stock_delete=%d custom_delete=%d\n", (int)stock_del, (int)custom_del);

    // Test TextOutA
    TextOutA(NULL, 10, 20, "Hello from GDI", 17);
    TextOutA(NULL, 30, 40, "ThetaZ", 6);

    printf("gdi_done\n");
    ExitProcess(0);
}
#else
#error "Define a TEST_* symbol"
#endif
#endif
