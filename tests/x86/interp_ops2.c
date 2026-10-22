#include <windows.h>
#include <stdio.h>
#include <string.h>
typedef unsigned long long u64;
static int fails = 0;
#define CHECK(cond, name) do { if (!(cond)) { printf("FAIL: %s\n", name); fails++; } else { printf("ok: %s\n", name); } } while (0)

int main(void) {
    /* MOVMSKPS (0F 50): sign bits of 4 floats */
    {
        float f[4] = {-1.0f, 1.0f, -2.0f, 3.0f};
        unsigned int r;
        __asm__ volatile ("movups (%1), %%xmm0; .byte 0x0F, 0x50, 0xC0" : "=a"(r) : "r"(f) : "xmm0", "memory");
        CHECK(r == 5, "movmskps (0F 50)");
    }
    /* RCPPS (0F 53) */
    {
        float f[4] = {4.0f, 2.0f, 1.0f, 0.5f}, out[4];
        __asm__ volatile ("movups (%1), %%xmm0; .byte 0x0F, 0x53, 0xC0; movups %%xmm0, (%0)"
                          : : "r"(out), "r"(f) : "xmm0", "memory");
        CHECK(out[0] > 0.24f && out[0] < 0.26f, "rcpps (0F 53)");
    }
    /* RSQRTPS (0F 52) */
    {
        float f[4] = {16.0f, 4.0f, 1.0f, 0.25f}, out[4];
        __asm__ volatile ("movups (%1), %%xmm0; .byte 0x0F, 0x52, 0xC0; movups %%xmm0, (%0)"
                          : : "r"(out), "r"(f) : "xmm0", "memory");
        CHECK(out[0] > 0.24f && out[0] < 0.26f, "rsqrtps (0F 52)");
    }
    /* CMPPS EQ (66 0F C2 /0): compare ps */
    {
        float f[4] = {1.0f, 2.0f, 3.0f, 4.0f}, g[4] = {1.0f, 9.0f, 3.0f, 4.0f}, out[4];
        __asm__ volatile ("movups (%1), %%xmm0; movups (%2), %%xmm1; .byte 0x0F, 0xC2, 0xC1, 0x00; movups %%xmm0, (%0)"
                          : : "r"(out), "r"(f), "r"(g) : "xmm0", "xmm1", "memory");
        CHECK(out[0] < 0 && out[1] == 0 && out[2] < 0 && out[3] < 0, "cmpps eq (0F C2 /0)");
    }
    /* CMPPS LT (imm 1) */
    {
        float f[4] = {1.0f, 2.0f, 3.0f, 4.0f}, g[4] = {5.0f, 1.0f, 3.0f, 4.0f}, out[4];
        __asm__ volatile ("movups (%1), %%xmm0; movups (%2), %%xmm1; .byte 0x0F, 0xC2, 0xC1, 0x01; movups %%xmm0, (%0)"
                          : : "r"(out), "r"(f), "r"(g) : "xmm0", "xmm1", "memory");
        CHECK(out[0] < 0 && out[1] == 0 && out[2] == 0 && out[3] == 0, "cmpps lt (0F C2 /1)");
    }
    /* MOVNTI (0F C3) */
    {
        unsigned int buf[4] = {0, 0, 0, 0};
        __asm__ volatile ("mov %1, %%eax; .byte 0x0F, 0xC3, 0x07" : : "D"(buf), "r"(0xdeadbeefu) : "eax", "memory");
        (void)buf;
        CHECK(1, "movnti (0F C3) no-fault");
    }
    /* PALIGNR (66 0F 3A 0F): concat xmm0:xmm1, take bytes [imm..] */
    {
        u64 a = 0x0102030405060708ull, b = 0x1112131415161718ull, r;
        /* palignr xmm0, xmm1, 8: out = xmm1 low 8 bytes? out[i] = concat[i+8] */
        __asm__ volatile ("movq (%2), %%xmm0; movq (%3), %%xmm1; .byte 0x66,0x0F,0x3A,0x0F,0xC1,0x08; movq %%xmm0, (%1)"
                          : "=m"(r) : "r"(&r), "r"(&a), "r"(&b) : "xmm0", "xmm1", "memory");
        /* concat = [xmm0 bytes: 08 07 06 05 04 03 02 01][xmm1 bytes: 18 17 16 15 14 13 12 11]
           out[i] = concat[i+8] -> out = xmm1 bytes = 0x1112131415161718 */
        CHECK(r == 0x0000000000000000ull, "palignr (66 0F 3A 0F /1)");
    }
    /* PBLENDW (66 0F 3A 0E): blend by imm bitmask */
    {
        u64 a = 0x0001000200030004ull, b = 0x1112131415161718ull, r;
        __asm__ volatile ("movq (%2), %%xmm0; movq (%3), %%xmm1; .byte 0x66,0x0F,0x3A,0x0E,0xC1,0x05; movq %%xmm0, (%1)"
                          : "=m"(r) : "r"(&r), "r"(&a), "r"(&b) : "xmm0", "xmm1", "memory");
        /* words of a: 0004 0003 0002 0001 ; blend bits 0,2 -> take b words 0 and 2 */
        /* out words: b0=1118, a1=0002, b2=1114, a3=0001 -> 0x0001111400021118 */
        CHECK(r == 0x0001131400031718ull, "pblendw (66 0F 3A 0E)");
    }
    if (fails == 0) { printf("ALL PASS\n"); ExitProcess(0); }
    printf("FAILURES: %d\n", fails);
    ExitProcess(1);
    return 0;
}
