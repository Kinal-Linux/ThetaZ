/* fuzz_ops.c — differential fuzz of the ThetaZ x86 interpreter.
 *
 * The same C file is compiled two ways:
 *   1. native host gcc (x86-64, -fno-pie): produces the GOLDEN output
 *   2. i686-w64-mingw32-gcc: 32-bit PE run under the ThetaZ interpreter
 *
 * Every probe executes hand-emitted raw bytes (.byte/.long) so both
 * binaries run identical instruction streams. Output lines must be
 * byte-identical; the driver diffs them.
 *
 * Flag masks follow Intel semantics:
 *   arithmetic/logic: CF PF AF ZF SF OF all defined
 *   shifts:           CF PF ZF SF (+OF only when count==1)
 *   rotates:          CF, OF(only count==1); others PRESERVED (pre-set
 *                     deterministically via cmpl)
 *   RCL/RCR:          CF (+OF only count==1); others preserved
 *   mul/div/imul:     CF OF only
 *   bsf/bsr:          ZF only; dest unchanged when src==0
 *   test:             CF ZF SF OF (AF undefined)
 *   inc/dec:          PF AF ZF SF OF (CF preserved)
 *   mov/movzx/movsx/xchg/bswap: no flags
 */
#include <stdio.h>
#include <stdint.h>

static uint32_t seed = 0x12345678u;
static uint32_t rnd(void) {
    seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
    return seed;
}

/* memory probe cell: defined in pure asm so both i686-mingw (underscore
 * prefixing) and native x86-64 reference the same symbol name. */
extern uint32_t thetaz_cell[] __asm__("thetaz_cell");
__asm__(".bss\n.p2align 12\n.global thetaz_cell\nthetaz_cell:\n.space 4096\n.text");

#define F_CF 0x001u
#define F_PF 0x004u
#define F_AF 0x010u
#define F_ZF 0x040u
#define F_SF 0x080u
#define F_OF 0x800u
#define M_ALL    (F_CF|F_PF|F_AF|F_ZF|F_SF|F_OF)      /* 0x8D5 */
#define M_NOAF   (F_CF|F_PF|F_ZF|F_SF|F_OF)           /* 0x8C5 */
#define M_SHIFT  (F_CF|F_PF|F_ZF|F_SF)                /* 0x0C5 */
#define M_SHIFT1 (F_CF|F_PF|F_ZF|F_SF|F_OF)           /* 0x8C5 */
#define M_MULDIV (F_CF|F_OF)                          /* 0x801 */
#define M_BSF    (F_ZF)                               /* 0x040 */
#define M_TEST   (F_CF|F_ZF|F_SF|F_OF)                /* 0x8C1 */
#define M_INC    (F_PF|F_AF|F_ZF|F_SF|F_OF)           /* 0x8D4 */
#define M_CF     (F_CF)
#define M_NONE   0

/* reg/reg + imm probes: inputs eax=a, ecx=b, edx=d0; prints a b eax ecx edx flags */
#define PROBE(name, PRE, OPS, imm, MASK) do { \
    uint32_t eax_, ecx_, edx_, fl_; \
    __asm__ volatile ( \
        "movl %4, %%eax; movl %5, %%ecx; movl %6, %%edx;\n\t" \
        PRE \
        OPS \
        "movl %%edx, %3;\n\t" \
        ".byte 0x9C, 0x5A; movl %%edx, %0;\n\t" \
        "movl %%eax, %1; movl %%ecx, %2;" \
        : "=m"(fl_), "=m"(eax_), "=m"(ecx_), "=m"(edx_) \
        : "m"(a), "m"(b), "m"(d0), "i"((uint32_t)(imm)) \
        : "eax", "ecx", "edx", "cc", "memory"); \
    printf("%-15s %08x %08x %08x %08x %08x %08x\n", (name), (a), (b), \
           eax_, ecx_, edx_, fl_ & (uint32_t)(MASK)); \
} while (0)

/* memory probes: cell preloaded with d0; prints a b eax(old cell) ecx(new cell) 0 flags */
#define PROBE_C(name, PRE, OPS, MASK) do { \
    uint32_t eax_, ecx_, fl_; \
    __asm__ volatile ( \
        "movl %3, %%eax; movl %4, %%ecx; movl %5, %%edx; movl %%edx, thetaz_cell;\n\t" \
        PRE \
        OPS \
        "movl thetaz_cell, %%ecx;\n\t" \
        ".byte 0x9C, 0x5A; movl %%edx, %0;\n\t" \
        "movl %%eax, %1; movl %%ecx, %2;" \
        : "=m"(fl_), "=m"(eax_), "=m"(ecx_) \
        : "m"(a), "m"(b), "m"(d0), "i"(0u) \
        : "eax", "ecx", "edx", "cc", "memory"); \
    printf("%-15s %08x %08x %08x %08x %08x %08x\n", (name), (a), (b), \
           eax_, ecx_, 0u, fl_ & (uint32_t)(MASK)); \
} while (0)

int main(void) {
    int i;

    setvbuf(stdout, NULL, _IOLBF, 0);
    printf("fuzz_ops seed=%08x\n", seed);
    for (i = 0; i < 8; i++) {
        uint32_t a = rnd(), b = rnd(), d0 = rnd();

        /* --- 32-bit reg-reg arithmetic (eax,ecx) --- */
        PROBE("add32", "", ".byte 0x01, 0xC8;", 0, M_ALL);
        PROBE("or32",  "", ".byte 0x09, 0xC8;", 0, M_ALL);
        PROBE("adc32cf0", "clc;\n\t", ".byte 0x11, 0xC8;", 0, M_ALL);
        PROBE("adc32cf1", "stc;\n\t", ".byte 0x11, 0xC8;", 0, M_ALL);
        PROBE("sbb32cf0", "clc;\n\t", ".byte 0x19, 0xC8;", 0, M_ALL);
        PROBE("sbb32cf1", "stc;\n\t", ".byte 0x19, 0xC8;", 0, M_ALL);
        PROBE("and32", "", ".byte 0x21, 0xC8;", 0, M_ALL);
        PROBE("sub32", "", ".byte 0x29, 0xC8;", 0, M_ALL);
        PROBE("xor32", "", ".byte 0x31, 0xC8;", 0, M_ALL);
        PROBE("cmp32", "", ".byte 0x39, 0xC8;", 0, M_ALL);

        /* --- 32-bit imm32 forms (81 /0..7) --- */
        PROBE("addi32", "", "addl %7, %%eax;", 0xABCD1234u, M_ALL);
        PROBE("ori32",  "", "orl %7, %%eax;", 0xF0F0F0F0u, M_ALL);
        PROBE("adci32", "stc;\n\t", "adcl %7, %%eax;", 0x5555AAAAu, M_ALL);
        PROBE("sbbi32", "clc;\n\t", "sbbl %7, %%eax;", 0x80000000u, M_ALL);
        PROBE("andi32", "", "andl %7, %%eax;", 0x0F0F0F0Fu, M_ALL);
        PROBE("subi32", "", "subl %7, %%eax;", 0x12345678u, M_ALL);
        PROBE("xori32", "", "xorl %7, %%eax;", 0xFFFFFFFFu, M_ALL);
        PROBE("cmpi32", "", "cmpl %7, %%eax;", 0xDEADBEEFu, M_ALL);

        /* --- 32-bit imm8 sign-extended forms (83 /0..7) --- */
        PROBE("addi8", "", "addl %7, %%eax;", 0x7F, M_ALL);
        PROBE("ori8",  "", "orl %7, %%eax;", 0x80, M_ALL);
        PROBE("adci8", "stc;\n\t", "adcl %7, %%eax;", 0xFF, M_ALL);
        PROBE("sbbi8", "clc;\n\t", "sbbl %7, %%eax;", 0x7F, M_ALL);
        PROBE("andi8", "", "andl %7, %%eax;", 0x80, M_ALL);
        PROBE("subi8", "", "subl %7, %%eax;", 0xFF, M_ALL);
        PROBE("xori8", "", "xorl %7, %%eax;", 0x05, M_ALL);
        PROBE("cmpi8", "", "cmpl %7, %%eax;", 0x80, M_ALL);

        /* --- shifts/rotates by cl (D3 /4../7), count = cl&31 --- */
        PROBE("shl_cl", "", ".byte 0xD3, 0xE0;", 0, M_SHIFT);
        PROBE("shr_cl", "", ".byte 0xD3, 0xE8;", 0, M_SHIFT);
        PROBE("sar_cl", "", ".byte 0xD3, 0xF8;", 0, M_SHIFT);
        PROBE("rol_cl", "cmpl $0, %5;\n\t", ".byte 0xD3, 0xC0;", 0, M_SHIFT);
        PROBE("ror_cl", "cmpl $0, %5;\n\t", ".byte 0xD3, 0xC8;", 0, M_SHIFT);
        PROBE("rcl_cl", "clc;\n\t", ".byte 0xD3, 0xD0;", 0, M_CF);
        PROBE("rcr_cl", "stc;\n\t", ".byte 0xD3, 0xD8;", 0, M_CF);

        /* --- shifts/rotates by 1 (D1) --- */
        PROBE("shl_1", "", ".byte 0xD1, 0xE0;", 0, M_SHIFT1);
        PROBE("shr_1", "", ".byte 0xD1, 0xE8;", 0, M_SHIFT1);
        PROBE("sar_1", "", ".byte 0xD1, 0xF8;", 0, M_SHIFT1);
        PROBE("rol_1", "cmpl $0, %5;\n\t", ".byte 0xD1, 0xC0;", 0, M_ALL);
        PROBE("ror_1", "cmpl $0, %5;\n\t", ".byte 0xD1, 0xC8;", 0, M_ALL);
        PROBE("rcl_1", "clc;\n\t", ".byte 0xD1, 0xD0;", 0, M_MULDIV);
        PROBE("rcr_1", "stc;\n\t", ".byte 0xD1, 0xD8;", 0, M_MULDIV);

        /* --- shifts/rotates by imm8 (C1) --- */
        PROBE("shl_i8", "", "shll %7, %%eax;", 3, M_SHIFT);
        PROBE("shr_i8", "", "shrl %7, %%eax;", 5, M_SHIFT);
        PROBE("sar_i8", "", "sarl %7, %%eax;", 9, M_SHIFT);
        PROBE("rol_i8", "cmpl $0, %5;\n\t", "roll %7, %%eax;", 1, M_ALL);
        PROBE("ror_i8", "cmpl $0, %5;\n\t", "rorl %7, %%eax;", 17, M_SHIFT);

        /* --- inc/dec (FF /0, FF /1 — the 1-byte 0x40/0x48 forms are REX
         * prefixes in 64-bit mode, so they are not encoding-identical) --- */
        PROBE("inc32", "cmpl $0, %5;\n\t", ".byte 0xFF, 0xC0;", 0, M_INC);
        PROBE("dec32", "cmpl $0, %5;\n\t", ".byte 0xFF, 0xC8;", 0, M_INC);
        PROBE("neg32", "", ".byte 0xF7, 0xD8;", 0, M_ALL);
        PROBE("not32", "", ".byte 0xF7, 0xD0;", 0, M_NONE);

        /* --- mul/div family --- */
        PROBE("mul32", "", ".byte 0xF7, 0xE1;", 0, M_MULDIV);
        PROBE("imul32", "", ".byte 0xF7, 0xE9;", 0, M_MULDIV);
        if (b != 0 && a / b <= 0xFFFFFFFFu)
            PROBE("div32", "xorl %%edx, %%edx;\n\t", ".byte 0xF7, 0xF1;", 0, M_MULDIV);
        {
            int32_t sa = (int32_t)a, sb = (int32_t)b;
            if (sb != 0 && !(sa == -2147483647 - 1 && sb == -1))
                PROBE("idiv32", "cdq;\n\t", ".byte 0xF7, 0xF9;", 0, M_MULDIV);
        }
        PROBE("imul_i32", "", "imull %7, %%ecx, %%eax;", 0x10001u, M_MULDIV);
        PROBE("imul_i8",  "", "imull %7, %%ecx, %%eax;", 0xFD, M_MULDIV);

        /* --- test family --- */
        PROBE("test32", "", ".byte 0x85, 0xC9;", 0, M_TEST);
        PROBE("testeax", "", ".byte 0x85, 0xC0;", 0, M_TEST);
        PROBE("testi32", "", "testl %7, %%eax;", 0x12345678u, M_TEST);

        /* --- bsf/bsr (dest unchanged when src==0) --- */
        PROBE("bsf", "", ".byte 0x0F, 0xBC, 0xC1;", 0, M_BSF);
        PROBE("bsr", "", ".byte 0x0F, 0xBD, 0xC1;", 0, M_BSF);

        /* --- shld/shrd --- */
        PROBE("shld_cl", "", ".byte 0x0F, 0xA5, 0xC1;", 0, M_SHIFT);
        PROBE("shld_i1", "", "shldl %7, %%eax, %%ecx;", 1, M_SHIFT1);
        PROBE("shrd_cl", "", ".byte 0x0F, 0xAD, 0xC1;", 0, M_SHIFT);
        PROBE("shrd_i1", "", "shrdl %7, %%eax, %%ecx;", 1, M_SHIFT1);

        /* --- mov/movzx/movsx/xchg/bswap (no flags) --- */
        PROBE("mov8", "", ".byte 0x8A, 0xC1;", 0, M_NONE);
        PROBE("movzx_b", "", ".byte 0x0F, 0xB6, 0xC1;", 0, M_NONE);
        PROBE("movsx_b", "", ".byte 0x0F, 0xBE, 0xC1;", 0, M_NONE);
        PROBE("movzx_w", "", ".byte 0x0F, 0xB7, 0xC1;", 0, M_NONE);
        PROBE("movsx_w", "", ".byte 0x0F, 0xBF, 0xC1;", 0, M_NONE);
        PROBE("xchg32", "", ".byte 0x87, 0xC1;", 0, M_NONE);
        PROBE("bswap", "", ".byte 0x0F, 0xC8;", 0, M_NONE);

        /* --- 8-bit reg-reg (al,cl) --- */
        PROBE("add8", "", ".byte 0x00, 0xC8;", 0, M_ALL);
        PROBE("or8",  "", ".byte 0x08, 0xC8;", 0, M_ALL);
        PROBE("adc8cf1", "stc;\n\t", ".byte 0x10, 0xC8;", 0, M_ALL);
        PROBE("sbb8cf0", "clc;\n\t", ".byte 0x18, 0xC8;", 0, M_ALL);
        PROBE("and8", "", ".byte 0x20, 0xC8;", 0, M_ALL);
        PROBE("sub8", "", ".byte 0x28, 0xC8;", 0, M_ALL);
        PROBE("xor8", "", ".byte 0x30, 0xC8;", 0, M_ALL);
        PROBE("cmp8", "", ".byte 0x38, 0xC8;", 0, M_ALL);
        PROBE("test8", "", ".byte 0x84, 0xC8;", 0, M_TEST);
        PROBE("neg8", "", ".byte 0xF6, 0xD8;", 0, M_ALL);
        PROBE("not8", "", ".byte 0xF6, 0xD0;", 0, M_NONE);

        /* --- 16-bit (66 prefix) --- */
        PROBE("add16", "", ".byte 0x66, 0x01, 0xC8;", 0, M_ALL);
        PROBE("or16",  "", ".byte 0x66, 0x09, 0xC8;", 0, M_ALL);
        PROBE("adc16cf1", "stc;\n\t", ".byte 0x66, 0x11, 0xC8;", 0, M_ALL);
        PROBE("sbb16cf0", "clc;\n\t", ".byte 0x66, 0x19, 0xC8;", 0, M_ALL);
        PROBE("and16", "", ".byte 0x66, 0x21, 0xC8;", 0, M_ALL);
        PROBE("sub16", "", ".byte 0x66, 0x29, 0xC8;", 0, M_ALL);
        PROBE("xor16", "", ".byte 0x66, 0x31, 0xC8;", 0, M_ALL);
        PROBE("cmp16", "", ".byte 0x66, 0x39, 0xC8;", 0, M_ALL);
        PROBE("addi16", "", "addw %7, %%ax;", 0x12345678u, M_ALL);
        PROBE("shl16_cl", "", ".byte 0x66, 0xD3, 0xE0;", 0, M_SHIFT);
        PROBE("sar16_cl", "", ".byte 0x66, 0xD3, 0xF8;", 0, M_SHIFT);
        PROBE("rol16_1", "cmpl $0, %5;\n\t", ".byte 0x66, 0xD1, 0xC0;", 0, M_ALL);
        PROBE("shl16_i8", "", "shlw %7, %%ax;", 3, M_SHIFT);
        PROBE("movzx_w16", "", ".byte 0x66, 0x0F, 0xB6, 0xC1;", 0, M_NONE);
        /* no 16-bit bswap probe: 66 0F C8 is undefined on x86-64 (the
         * 1-byte bswap is covered by the "bswap" probe) */
        PROBE("neg16", "", ".byte 0x66, 0xF7, 0xD8;", 0, M_ALL);
        if ((uint32_t)((a & 0xFFFF) * (b & 0xFFFF)) <= 0xFFFFu)
            PROBE("mul16", "", ".byte 0x66, 0xF7, 0xE1;", 0, M_MULDIV);
        if (b != 0 && (b & 0xFFFF) != 0 &&
            (uint16_t)((a & 0xFFFF) / (b & 0xFFFF)) == ((a & 0xFFFF) / (b & 0xFFFF)))
            PROBE("div16", "xorl %%edx, %%edx;\n\t", ".byte 0x66, 0xF7, 0xF1;", 0, M_MULDIV);

        /* --- memory probes --- */
        /* modrm 0x0C = reg field ECX (0x34 was reg=ESI, uninitialized) */
        PROBE_C("cmpxchg_c", "", ".byte 0x0F, 0xB1, 0x0C, 0x25; .long thetaz_cell;", M_ALL);
        PROBE_C("xadd_c", "", ".byte 0x0F, 0xC1, 0x0C, 0x25; .long thetaz_cell;", M_ALL);
    }
    printf("done\n");
    return 0;
}
