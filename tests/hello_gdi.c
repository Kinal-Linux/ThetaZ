// ThetaZ GDI test: GetStockObject, SelectObject, CreateSolidBrush, DeleteObject, TextOutA
#include <stdio.h>
#include <string.h>

// ThetaZ-like type definitions to avoid MinGW windows.h conflicts
typedef void* WinHandle;
typedef int BOOL;
#define FALSE_VALUE 0
#define TRUE_VALUE 1

// Stock object constants (same as ThetaZ)
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

int main(void) {
    // Test GetStockObject
    WinHandle black_brush = GetStockObject(11);  // BLACK_BRUSH
    WinHandle white_pen = GetStockObject(29);    // WHITE_PEN
    printf("black_brush=%p white_pen=%p\n", black_brush, white_pen);

    // Test SelectObject
    WinHandle prev_brush = SelectObject(NULL, black_brush);
    WinHandle prev_pen = SelectObject(NULL, white_pen);
    printf("prev_brush=%p prev_pen=%p\n", prev_brush, prev_pen);

    // Test CreateSolidBrush
    WinHandle custom_brush = CreateSolidBrush(0xFF0000);
    printf("custom_brush=%p\n", custom_brush);

    // Test DeleteObject (should return FALSE for stock, TRUE for custom)
    BOOL stock_del = DeleteObject(BLACK_BRUSH);
    BOOL custom_del = DeleteObject(custom_brush);
    printf("stock_delete=%d custom_delete=%d\n", (int)stock_del, (int)custom_del);

    // Test TextOutA
    TextOutA(NULL, 10, 20, "Hello from GDI", 17);
    TextOutA(NULL, 30, 40, "ThetaZ", 6);

    printf("gdi_done\n");
    return 0;
}
