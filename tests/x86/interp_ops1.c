#include <windows.h>
#include <stdio.h>

static int fails = 0;
#define CHECK(cond, name) do { if (!(cond)) { printf("FAIL: %s\n", name); fails++; } else { printf("ok: %s\n", name); } } while (0)

typedef unsigned long long u64;

int main(void) {
    /* --- XADD r/m8 (0F C0) --- */
    {
        unsigned char a = 0x40, b = 0x20;
        __asm__ volatile (".byte 0x0F, 0xC0, 0xC1" : "+a"(a), "+c"(b));
        CHECK(a == 0x20 && b == 0x60, "xadd r8 (0F C0)");
    }
    /* --- XADD r/m32 (0F C1, regression) --- */
    {
        unsigned int a = 0x40, b = 0x20;
        __asm__ volatile (".byte 0x0F, 0xC1, 0xC1" : "+a"(a), "+c"(b));
        CHECK(a == 0x20 && b == 0x60, "xadd r32 (0F C1)");
    }
    /* --- PCMPGTB (0F 64): dest reg field = XMM0 --- */
    {
        u64 a = 0x03020100fffeffffull, b = 0x0101010101010101ull, r;
        __asm__ volatile ("movq (%2), %%xmm0; movq (%3), %%xmm1; .byte 0x66,0x0F,0x64,0xC1; movq %%xmm0, (%1)"
                          : "=m"(r) : "r"(&r), "r"(&a), "r"(&b) : "xmm0", "xmm1", "memory");
        CHECK(r == 0xFFFF000000000000ull, "pcmpgtb (0F 64)");
    }
    /* --- PCMPGTW (0F 65) --- */
    {
        u64 a = 0x03020100fffeffffull, b = 0x0101010101010101ull, r;
        __asm__ volatile ("movq (%2), %%xmm0; movq (%3), %%xmm1; .byte 0x66,0x0F,0x65,0xC1; movq %%xmm0, (%1)"
                          : "=m"(r) : "r"(&r), "r"(&a), "r"(&b) : "xmm0", "xmm1", "memory");
        CHECK(r == 0xFFFF000000000000ull, "pcmpgtw (0F 65)");
    }
    /* --- PSLLW imm8 (66 0F 71 /6) --- */
    {
        u64 a = 0x0001000200030004ull, b = 0, r;
        __asm__ volatile ("movq (%2), %%xmm0; movq (%3), %%xmm1; .byte 0x66,0x0F,0x71,0xF0,0x02; movq %%xmm0, (%1)"
                          : "=m"(r) : "r"(&r), "r"(&a), "r"(&b) : "xmm0", "xmm1", "memory");
        CHECK(r == 0x00040008000C0010ull, "psllw imm8 (0F 71 /6)");
    }
    /* --- PSRAW imm8 (66 0F 71 /4) --- */
    {
        u64 a = 0xFFFE0000AAAABBBBull, b = 0, r;
        __asm__ volatile ("movq (%2), %%xmm0; movq (%3), %%xmm1; .byte 0x66,0x0F,0x71,0xE0,0x01; movq %%xmm0, (%1)"
                          : "=m"(r) : "r"(&r), "r"(&a), "r"(&b) : "xmm0", "xmm1", "memory");
        CHECK(r == 0xFFFF0000D555DDDDull, "psraw imm8 (0F 71 /4)");
    }
    /* --- PSRLW imm8 (66 0F 71 /2) --- */
    {
        u64 a = 0x000800008000FFFFull, b = 0, r;
        __asm__ volatile ("movq (%2), %%xmm0; movq (%3), %%xmm1; .byte 0x66,0x0F,0x71,0xD0,0x04; movq %%xmm0, (%1)"
                          : "=m"(r) : "r"(&r), "r"(&a), "r"(&b) : "xmm0", "xmm1", "memory");
        CHECK(r == 0x0000000008000FFFull, "psrlw imm8 (0F 71 /2)");
    }
    /* --- PSLLD imm8 (66 0F 72 /6) --- */
    {
        u64 a = 0x8000000000000001ull, b = 0, r;
        __asm__ volatile ("movq (%2), %%xmm0; movq (%3), %%xmm1; .byte 0x66,0x0F,0x72,0xF0,0x01; movq %%xmm0, (%1)"
                          : "=m"(r) : "r"(&r), "r"(&a), "r"(&b) : "xmm0", "xmm1", "memory");
        CHECK(r == 0x0000000000000002ull, "pslld imm8 (0F 72 /6)");
    }
    /* --- PSRAD imm8 (66 0F 72 /4) --- */
    {
        u64 a = 0x8000000000000001ull, b = 0, r;
        __asm__ volatile ("movq (%2), %%xmm0; movq (%3), %%xmm1; .byte 0x66,0x0F,0x72,0xE0,0x01; movq %%xmm0, (%1)"
                          : "=m"(r) : "r"(&r), "r"(&a), "r"(&b) : "xmm0", "xmm1", "memory");
        CHECK(r == 0xC000000000000000ull, "psrad imm8 (0F 72 /4)");
    }
    /* --- PSRLD imm8 (66 0F 72 /2) --- */
    {
        u64 a = 0x8000000000000001ull, b = 0, r;
        __asm__ volatile ("movq (%2), %%xmm0; movq (%3), %%xmm1; .byte 0x66,0x0F,0x72,0xD0,0x04; movq %%xmm0, (%1)"
                          : "=m"(r) : "r"(&r), "r"(&a), "r"(&b) : "xmm0", "xmm1", "memory");
        CHECK(r == 0x0800000000000000ull, "psrld imm8 (0F 72 /2)");
    }
    /* --- PSLLQ/PSRLQ imm8 (66 0F 73 /6, /2) --- */
    {
        u64 a = 0x0000000000000001ull, b = 0x0000000000000002ull, r;
        __asm__ volatile ("movq (%2), %%xmm0; movq (%3), %%xmm1; .byte 0x66,0x0F,0x73,0xF0,0x08; movq %%xmm0, (%1)"
                          : "=m"(r) : "r"(&r), "r"(&a), "r"(&b) : "xmm0", "xmm1", "memory");
        CHECK(r == 0x0000000000000100ull, "psllq imm8 (0F 73 /6)");
        __asm__ volatile ("movq (%2), %%xmm0; movq (%3), %%xmm1; .byte 0x66,0x0F,0x73,0xD0,0x08; movq %%xmm0, (%1)"
                          : "=m"(r) : "r"(&r), "r"(&a), "r"(&b) : "xmm0", "xmm1", "memory");
        CHECK(r == 0x0000000000000000ull, "psrlq imm8 (0F 73 /2)");
    }
    /* --- PSLLDQ/PSRLDQ imm8 (66 0F 73 /7, /3) --- */
    {
        u64 a = 0x0000000001020304ull, b = 0, r;
        __asm__ volatile ("movq (%2), %%xmm0; movq (%3), %%xmm1; .byte 0x66,0x0F,0x73,0xF8,0x04; movq %%xmm0, (%1)"
                          : "=m"(r) : "r"(&r), "r"(&a), "r"(&b) : "xmm0", "xmm1", "memory");
        CHECK(r == 0x0102030400000000ull, "pslldq imm8 (0F 73 /7)");
        __asm__ volatile ("movq (%2), %%xmm0; movq (%3), %%xmm1; .byte 0x66,0x0F,0x73,0xD8,0x01; movq %%xmm0, (%1)"
                          : "=m"(r) : "r"(&r), "r"(&a), "r"(&b) : "xmm0", "xmm1", "memory");
        CHECK(r == 0x0000000000010203ull, "psrldq imm8 (0F 73 /3)");
    }
    /* --- EMMS (0F 77) --- */
    {
        __asm__ volatile (".byte 0x0F, 0x77");
        CHECK(1, "emms (0F 77) no-fault");
    }
    /* --- SHLD r32, r32, imm8 (0F A4): dest=ecx(rm), src=eax(reg) --- */
    {
        unsigned int d = 0x12345678u, s = 0x9ABCDEF0u, r;
        __asm__ volatile (".byte 0x0F, 0xA4, 0xC1, 0x04" : "+c"(d), "+a"(s));
        CHECK(d == 0x23456789u, "shld r32 imm8 (0F A4)");
    }
    /* --- SHRD r32, r32, imm8 (0F AC) --- */
    {
        unsigned int d = 0x12345678u, s = 0x9ABCDEF0u, r;
        __asm__ volatile (".byte 0x0F, 0xAC, 0xC1, 0x04" : "+c"(d), "+a"(s));
        CHECK(d == 0x01234567u, "shrd r32 imm8 (0F AC)");
        (void)r;
    }
    /* --- MOVNTPS (0F 2B) --- */
    {
        unsigned int buf[4] = {0, 0, 0, 0};
        __asm__ volatile ("movups (%1), %%xmm0; .byte 0x0F, 0x2B, 0x07" : "=m"(buf) : "r"(buf) : "xmm0", "memory");
        CHECK(buf[0] == 0 && buf[3] == 0, "movntps (0F 2B) no-fault");
    }
    /* --- TZCNT/LZCNT (F3 0F BC/BD) --- */
    {
        unsigned int r;
        __asm__ volatile (".byte 0xF3, 0x0F, 0xBC, 0xC1" : "=a"(r) : "c"(0x00008000u));
        CHECK(r == 15, "tzcnt (F3 0F BC)");
        __asm__ volatile (".byte 0xF3, 0x0F, 0xBD, 0xC1" : "=a"(r) : "c"(0x00008000u));
        CHECK(r == 16, "lzcnt (F3 0F BD)");
        __asm__ volatile (".byte 0x0F, 0xBC, 0xC1" : "=a"(r) : "c"(0x00008000u));
        CHECK(r == 15, "bsf (0F BC) regression");
    }
    /* --- POPCNT (F3 0F 38 F0) --- */
    {
        unsigned int r;
        __asm__ volatile (".byte 0xF3, 0x0F, 0x38, 0xF0, 0xC1" : "=a"(r) : "c"(0x0F0F0F0Fu));
        CHECK(r == 16, "popcnt (F3 0F 38 F0)");
    }
    /* --- CRC32 (F2 0F 38 F1) --- */
    {
        unsigned int r = 0;
        __asm__ volatile (".byte 0xF2, 0x0F, 0x38, 0xF1, 0xC1" : "+a"(r) : "c"(0x01020304u));
        CHECK(r != 0, "crc32 (F2 0F 38 F1) no-fault");
    }
    /* --- PSHUFB (66 0F 38 00): mask in reg field (XMM0), table in rm (XMM1) --- */
    {
        u64 mask = 0x0706050403020100ull, table = 0x0807060504030201ull, r;
        __asm__ volatile ("movq (%2), %%xmm0; movq (%3), %%xmm1; .byte 0x66,0x0F,0x38,0x00,0xC1; movq %%xmm0, (%1)"
                          : "=m"(r) : "r"(&r), "r"(&mask), "r"(&table) : "xmm0", "xmm1", "memory");
        CHECK(r == 0x0007060504030201ull, "pshufb (66 0F 38 00)");
    }
    /* --- PABSB (66 0F 38 1C) --- */
    {
        u64 src = 0x80FF7F0102030405ull, junk = 0, r;
        __asm__ volatile ("movq (%2), %%xmm0; movq (%3), %%xmm1; .byte 0x66,0x0F,0x38,0x1C,0xC1; movq %%xmm0, (%1)"
                          : "=m"(r) : "r"(&r), "r"(&junk), "r"(&src) : "xmm0", "xmm1", "memory");
        CHECK(r == 0x80017F0102030405ull, "pabsb (66 0F 38 1C)");
    }
    /* --- PABSD (66 0F 38 1E) --- */
    {
        u64 src = 0x8000000100000000ull, junk = 0, r;
        __asm__ volatile ("movq (%2), %%xmm0; movq (%3), %%xmm1; .byte 0x66,0x0F,0x38,0x1E,0xC1; movq %%xmm0, (%1)"
                          : "=m"(r) : "r"(&r), "r"(&junk), "r"(&src) : "xmm0", "xmm1", "memory");
        CHECK(r == 0x7FFFFFFF00000000ull, "pabsd (66 0F 38 1E)");
    }

    if (fails == 0) {
        printf("ALL PASS\n");
        ExitProcess(0);
    }
    printf("FAILURES: %d\n", fails);
    ExitProcess(1);
    return 0;
}
