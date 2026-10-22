#include <windows.h>
#include <stdio.h>

/* Interpreter harness part 3: CMPXCHG8B, x87, REP string ops, CMOVcc,
 * SETcc, Jcc rel32/short, LOOP family, PUSHAD/POPAD, PUSHFD/POPFD.
 * Compile: i686-w64-mingw32-gcc -msse2
 */

static int fails = 0;
#define CHECK(cond, name) do { if (!(cond)) { printf("FAIL: %s\n", name); fails++; } else { printf("ok: %s\n", name); } } while (0)

typedef unsigned long long u64;

int main(void) {
    /* --- CMPXCHG8B (0F C7 /1): success --- */
    {
        u64 m = 0x1122334455667788ull;
        unsigned int eax_r, edx_r, zf;
        __asm__ volatile (
            ".byte 0x0F, 0xC7, 0x0F\n"
            "mov %%eax, %0\n"
            "mov %%edx, %1\n"
            "setz %b2\n"
            : "=r"(eax_r), "=r"(edx_r), "=q"(zf)
            : "a"((unsigned int)m), "d"((unsigned int)(m >> 32)),
              "b"(0xAABBCCDDu), "c"(0xEEFF0011u), "D"(&m)
            : "memory");
        CHECK(eax_r == 0x55667788u && edx_r == 0x11223344u && (zf & 1) == 1 &&
              m == 0xEEFF0011AABBCCDDull, "cmpxchg8b success (0F C7 /1)");
    }
    /* --- CMPXCHG8B: mismatch (ZF=0, EDX:EAX loaded from mem) --- */
    {
        u64 m = 0x1122334455667788ull;
        unsigned int eax_r, edx_r, zf;
        __asm__ volatile (
            ".byte 0x0F, 0xC7, 0x0F\n"
            "mov %%eax, %0\n"
            "mov %%edx, %1\n"
            "setz %b2\n"
            : "=r"(eax_r), "=r"(edx_r), "=q"(zf)
            : "a"(0xDEADBEEFu), "d"(0x12345678u),
              "b"(0xAABBCCDDu), "c"(0xEEFF0011u), "D"(&m)
            : "memory");
        CHECK(eax_r == 0x55667788u && edx_r == 0x11223344u && (zf & 1) == 0 &&
              m == 0x1122334455667788ull, "cmpxchg8b mismatch");
    }

    /* --- x87: FLD/FSTP m64 roundtrip (DD /0, /3) --- */
    {
        double a = 3.14159265358979, out = 0.0;
        __asm__ volatile ("fldl %1; fstpl %0" : "=m"(out) : "m"(a) : "st");
        CHECK(out == a, "fld/fstp m64 (DD /0 /3)");
    }
    /* --- x87: FADD/FMUL/FSUB/FDIV m64 (D8 /0 /1 /4 /6 /7 /5) --- */
    {
        double a = 10.0, b = 4.0, out = 0.0;
        __asm__ volatile ("fldl %2; faddl %3; fstpl %1" : "=m"(out) : "m"(out), "m"(a), "m"(b) : "st");
        CHECK(out == 14.0, "fadd m64 (D8 /0)");
        __asm__ volatile ("fldl %2; fmull %3; fstpl %1" : "=m"(out) : "m"(out), "m"(a), "m"(b) : "st");
        CHECK(out == 40.0, "fmul m64 (D8 /1)");
        __asm__ volatile ("fldl %2; fsubl %3; fstpl %1" : "=m"(out) : "m"(out), "m"(a), "m"(b) : "st");
        CHECK(out == 6.0, "fsub m64 (D8 /4)");
        __asm__ volatile ("fldl %2; fdivl %3; fstpl %1" : "=m"(out) : "m"(out), "m"(a), "m"(b) : "st");
        CHECK(out == 2.5, "fdiv m64 (D8 /6)");
        __asm__ volatile ("fldl %2; fdivrl %3; fstpl %1" : "=m"(out) : "m"(out), "m"(a), "m"(b) : "st");
        CHECK(out == 0.4, "fdivr m64 (D8 /7)");
        __asm__ volatile ("fldl %2; fsubrl %3; fstpl %1" : "=m"(out) : "m"(out), "m"(a), "m"(b) : "st");
        CHECK(out == -6.0, "fsubr m64 (D8 /5)");
    }
    /* --- x87: FADDP reg-reg (DE C1), FILD/FISTP m32, FCHS/FABS/FLDPI/FLDZ --- */
    {
        double a = 1.5, b = 2.5, out = 0.0;
        __asm__ volatile ("fldl %2; fldl %3; faddp; fstpl %1" : "=m"(out) : "m"(out), "m"(a), "m"(b) : "st");
        CHECK(out == 4.0, "faddp (DE C1)");
        int iv = -12345, io = 0;
        __asm__ volatile ("fildl %1; fistpl %0" : "=m"(io) : "m"(iv) : "st");
        CHECK(io == -12345, "fild/fistp m32 (DB /0 /3)");
        double d;
        __asm__ volatile ("fldl %1; fchs; fabs; fstpl %0" : "=m"(d) : "m"(a) : "st");
        CHECK(d == 1.5, "fchs/fabs (D9 E0/E1)");
        __asm__ volatile ("fldpi; fstpl %0" : "=m"(d) : : "st");
        CHECK(d > 3.1415 && d < 3.1416, "fldpi (D9 EB)");
        __asm__ volatile ("fldz; fstpl %0" : "=m"(d) : : "st");
        CHECK(d == 0.0, "fldz (D9 EE)");
    }
    /* --- x87: FCOMI/FCOMIP/FUCOMI/FUCOMIP flags (DB F0/F7, DB E8, DF F0/E8) --- */
    {
        double a = 1.0, b = 2.0;
        unsigned int cf = 0, zf = 0;
        __asm__ volatile ("fldl %2; fldl %1; fcomi %%st(1), %%st; setb %b0"
                          : "=q"(cf) : "m"(a), "m"(b) : "st", "cc");
        CHECK((cf & 1) == 1, "fcomi st0<st1 -> CF (DB F0)");
        __asm__ volatile ("fldl %1; fldl %2; fcomi %%st(1), %%st; setb %b0"
                          : "=q"(cf) : "m"(a), "m"(b) : "st", "cc");
        CHECK((cf & 1) == 0, "fcomi st0>st1 -> CF=0");
        __asm__ volatile ("fldl %2; fldl %1; fcomip %%st(1), %%st; setb %b0"
                          : "=q"(cf) : "m"(a), "m"(b) : "st", "cc");
        CHECK((cf & 1) == 1, "fcomip a<b -> CF, pops (DF F0)");
        __asm__ volatile ("fldl %1; fldl %2; fcomi %%st(1), %%st; sete %b0"
                          : "=q"(zf) : "m"(a), "m"(b) : "st", "cc");
        CHECK((zf & 1) == 0, "fcomi unequal -> ZF=0");
        __asm__ volatile ("fldl %1; fldl %1; fcomi %%st(1), %%st; sete %b0"
                          : "=q"(zf) : "m"(a) : "st", "cc");
        CHECK((zf & 1) == 1, "fcomi equal -> ZF=1");
        __asm__ volatile ("fldl %2; fldl %1; fucomi %%st(1), %%st; setb %b0"
                          : "=q"(cf) : "m"(a), "m"(b) : "st", "cc");
        CHECK((cf & 1) == 1, "fucomi a<b -> CF (DB E8)");
        __asm__ volatile ("fldl %1; fldl %2; fucomip %%st(1), %%st; sete %b0"
                          : "=q"(zf) : "m"(a), "m"(b) : "st", "cc");
        CHECK((zf & 1) == 0, "fucomip unequal -> ZF=0 (DF E8)");
    }

    /* --- REP MOVSB forward (F3 A4) --- */
    {
        char src[9] = "abcdefgh", dst[9] = "00000000";
        unsigned int n = 8;
        __asm__ volatile ("cld; rep movsb" : : "S"(src), "D"(dst), "c"(n) : "memory");
        CHECK(memcmp(dst, "abcdefgh", 8) == 0, "rep movsb (F3 A4)");
    }
    /* --- REP MOVSD 4 dwords (F3 A5) --- */
    {
        unsigned int src[4] = {1, 2, 3, 4}, dst[4] = {0, 0, 0, 0};
        unsigned int n = 4;
        __asm__ volatile ("cld; rep movsl" : : "S"(src), "D"(dst), "c"(n) : "memory");
        CHECK(dst[0] == 1 && dst[3] == 4, "rep movsl (F3 A5)");
    }
    /* --- REP MOVSW 2 words --- */
    {
        unsigned short src[2] = {0x1111, 0x2222}, dst[2] = {0, 0};
        unsigned int n = 2;
        __asm__ volatile ("cld; rep movsw" : : "S"(src), "D"(dst), "c"(n) : "memory");
        CHECK(dst[0] == 0x1111 && dst[1] == 0x2222, "rep movsw (F3 A5 66)");
    }
    /* --- STD + REP MOVSB backward --- */
    {
        char src[6] = "ABCDEF", dst[6] = "000000";
        unsigned int n = 6;
        __asm__ volatile ("std; lea 5(%%esi), %%esi; lea 5(%%edi), %%edi; rep movsb; cld"
                          : : "S"(src), "D"(dst), "c"(n) : "memory");
        CHECK(memcmp(dst, "ABCDEF", 6) == 0, "std rep movsb backward (FD F3 A4)");
    }
    /* --- REP STOSB / STOSD (F3 AA / F3 AB) --- */
    {
        char buf[8] = {1, 1, 1, 1, 1, 1, 1, 1};
        unsigned int v = 0x00, n = 8;
        __asm__ volatile ("cld; rep stosb" : : "D"(buf), "a"(v), "c"(n) : "memory");
        CHECK(buf[0] == 0 && buf[7] == 0, "rep stosb (F3 AA)");
        unsigned int dw[4] = {1, 1, 1, 1};
        v = 0xCAFEBABE; n = 4;
        __asm__ volatile ("cld; rep stosl" : : "D"(dw), "a"(v), "c"(n) : "memory");
        CHECK(dw[0] == 0xCAFEBABE && dw[3] == 0xCAFEBABE, "rep stosl (F3 AB)");
    }
    /* --- LODSD (AD) --- */
    {
        unsigned int src = 0xDEADBEEF, eax_r = 0;
        __asm__ volatile ("cld; lodsl" : "=a"(eax_r) : "S"(&src) : "memory");
        CHECK(eax_r == 0xDEADBEEF, "lodsd (AD)");
    }
    /* --- REPZ CMPSD equal / unequal (F3 A7) --- */
    {
        unsigned int a[4] = {10, 20, 30, 40}, b[4] = {10, 20, 30, 40};
        unsigned int zf = 0, ecx_r = 9, n = 4;
        __asm__ volatile ("cld; repz cmpsl; sete %b0; mov %%ecx, %1"
                          : "=q"(zf), "=r"(ecx_r), "+c"(n)
                          : "S"(a), "D"(b) : "cc", "memory");
        CHECK((zf & 1) == 1 && ecx_r == 0, "repz cmpsl equal (F3 A7)");
        b[2] = 99; n = 4;
        __asm__ volatile ("cld; repz cmpsl; sete %b0; mov %%ecx, %1"
                          : "=q"(zf), "=r"(ecx_r), "+c"(n)
                          : "S"(a), "D"(b) : "cc", "memory");
        CHECK((zf & 1) == 0 && ecx_r == 1, "repz cmpsl unequal stops at idx2");
    }
    /* --- REPNE CMPSB (F2 A6) --- */
    {
        char a[6] = "abcXef", b[6] = "abcXef";
        unsigned int zf = 0, n = 6;
        __asm__ volatile ("cld; repne cmpsb; sete %b0"
                          : "=q"(zf), "+c"(n) : "S"(a), "D"(b) : "cc", "memory");
        CHECK((zf & 1) == 1, "repne cmpsb equal -> ZF (F2 A6)");
    }
    /* --- REPNE SCASB found / missing (F2 AE) --- */
    {
        char buf[8] = "abcdefgh";
        unsigned int zf = 0, ecx_r = 9, n = 8;
        unsigned int needle = 'd';
        __asm__ volatile ("cld; repne scasb; sete %b0; mov %%ecx, %1"
                          : "=q"(zf), "=r"(ecx_r), "+c"(n)
                          : "D"(buf), "a"(needle) : "cc", "memory");
        CHECK((zf & 1) == 1 && ecx_r == 4, "repne scasb found 'd' at idx3 (ecx=4)");
        needle = 'Z';
        __asm__ volatile ("cld; repne scasb; sete %b0"
                          : "=q"(zf), "+c"(n) : "D"(buf), "a"(needle) : "cc", "memory");
        CHECK((zf & 1) == 0, "repne scasb not found -> ZF=0");
    }
    /* --- REPZ SCASD (F3 AF) --- */
    {
        unsigned int buf[4] = {1, 2, 3, 4};
        unsigned int zf = 0, ecx_r = 9, n = 4;
        __asm__ volatile ("cld; repne scasl; sete %b0; mov %%ecx, %1"
                          : "=q"(zf), "=r"(ecx_r), "+c"(n)
                          : "D"(buf), "a"(3u) : "cc", "memory");
        CHECK((zf & 1) == 1 && ecx_r == 1, "repne scasl finds 3 (ecx=1)");
            }

    /* --- CMOVcc reg-reg (0F 44/45/42/47, dest EAX rm=EBX -> modrm C3) --- */
    {
        unsigned int r, s = 0xAA55AA55u;
        __asm__ volatile ("mov $0, %%eax; cmp $0, %%eax; .byte 0x0F, 0x44, 0xC3" : "=a"(r) : "b"(s) : "cc");
        CHECK(r == 0xAA55AA55u, "cmove taken (0F 44)");
        __asm__ volatile ("mov $1, %%eax; cmp $0, %%eax; .byte 0x0F, 0x44, 0xC3" : "=a"(r) : "b"(s) : "cc");
        CHECK(r == 1, "cmove not taken");
        __asm__ volatile ("mov $1, %%eax; cmp $0, %%eax; .byte 0x0F, 0x45, 0xC3" : "=a"(r) : "b"(s) : "cc");
        CHECK(r == 0xAA55AA55u, "cmovne taken (0F 45)");
        __asm__ volatile ("mov $1, %%eax; cmp $2, %%eax; .byte 0x0F, 0x42, 0xC3" : "=a"(r) : "b"(s) : "cc");
        CHECK(r == 0xAA55AA55u, "cmovb taken (0F 42)");
        __asm__ volatile ("mov $2, %%eax; cmp $2, %%eax; .byte 0x0F, 0x47, 0xC3" : "=a"(r) : "b"(s) : "cc");
        CHECK(r == 2, "cmova not taken (CF&&ZF) (0F 47)");
        __asm__ volatile ("mov $2, %%eax; cmp $1, %%eax; .byte 0x0F, 0x47, 0xC3" : "=a"(r) : "b"(s) : "cc");
        CHECK(r == 0xAA55AA55u, "cmova taken (0F 47)");
        __asm__ volatile ("mov $2, %%eax; cmp $1, %%eax; .byte 0x0F, 0x4F, 0xC3" : "=a"(r) : "b"(s) : "cc");
        CHECK(r == 0xAA55AA55u, "cmovg taken (0F 4F)");
        __asm__ volatile ("mov $1, %%eax; cmp $1, %%eax; .byte 0x0F, 0x4F, 0xC3" : "=a"(r) : "b"(s) : "cc");
        CHECK(r == 1, "cmovg not taken (ZF)");
        __asm__ volatile ("mov $2, %%eax; cmp $1, %%eax; .byte 0x0F, 0x4C, 0xC3" : "=a"(r) : "b"(s) : "cc");
        CHECK(r == 2, "cmovl not taken (0F 4C)");
        __asm__ volatile ("mov $1, %%eax; cmp $2, %%eax; .byte 0x0F, 0x4C, 0xC3" : "=a"(r) : "b"(s) : "cc");
        CHECK(r == 0xAA55AA55u, "cmovl taken (signed) (0F 4C)");
        __asm__ volatile ("mov $1, %%eax; cmp $2, %%eax; .byte 0x0F, 0x4D, 0xC3" : "=a"(r) : "b"(s) : "cc");
        CHECK(r == 1, "cmovge not taken (0F 4D)");
        __asm__ volatile ("mov $2, %%eax; cmp $1, %%eax; .byte 0x0F, 0x4E, 0xC3" : "=a"(r) : "b"(s) : "cc");
        CHECK(r == 2, "cmovle not taken (0F 4E)");
        __asm__ volatile ("mov $0x80000000, %%eax; test %%eax, %%eax; .byte 0x0F, 0x48, 0xC3" : "=a"(r) : "b"(s) : "cc");
        CHECK(r == 0xAA55AA55u, "cmovs taken (0F 48)");
        __asm__ volatile ("mov $0x7FFFFFFF, %%eax; add $1, %%eax; .byte 0x0F, 0x40, 0xC3" : "=a"(r) : "b"(s) : "cc");
        CHECK(r == 0xAA55AA55u, "cmovo taken (0F 40)");
        __asm__ volatile ("mov $3, %%eax; test %%eax, %%eax; .byte 0x0F, 0x4A, 0xC3" : "=a"(r) : "b"(s) : "cc");
        CHECK(r == 0xAA55AA55u, "cmovp taken (0F 4A)");
    }
    /* --- CMOVcc from memory (modrm 03 = [EBX]) --- */
    {
        unsigned int r, m = 0x12345678u;
        __asm__ volatile ("mov $0, %%eax; cmp $0, %%eax; .byte 0x0F, 0x44, 0x03" : "=a"(r) : "b"(&m) : "cc", "memory");
        CHECK(r == 0x12345678u, "cmove [mem] (0F 44 /r mod=00)");
    }
    /* --- SETcc (0F 94-9F) --- */
    {
        unsigned int r;
        __asm__ volatile ("mov $0, %%eax; cmp $0, %%eax; .byte 0x0F, 0x94, 0xC0" : "=q"(r) : : "cc");
        CHECK((r & 1) == 1, "setz (0F 94)");
        __asm__ volatile ("mov $1, %%eax; cmp $0, %%eax; .byte 0x0F, 0x94, 0xC0" : "=q"(r) : : "cc");
        CHECK((r & 1) == 0, "setz not taken");
        __asm__ volatile ("mov $1, %%eax; cmp $0, %%eax; .byte 0x0F, 0x95, 0xC0" : "=q"(r) : : "cc");
        CHECK((r & 1) == 1, "setnz (0F 95)");
        __asm__ volatile ("mov $1, %%eax; cmp $2, %%eax; .byte 0x0F, 0x92, 0xC0" : "=q"(r) : : "cc");
        CHECK((r & 1) == 1, "setb (0F 92)");
        __asm__ volatile ("mov $2, %%eax; cmp $2, %%eax; .byte 0x0F, 0x93, 0xC0" : "=q"(r) : : "cc");
        CHECK((r & 1) == 1, "setae (0F 93)");
        __asm__ volatile ("mov $2, %%eax; cmp $1, %%eax; .byte 0x0F, 0x9F, 0xC0" : "=q"(r) : : "cc");
        CHECK((r & 1) == 1, "setg (0F 9F)");
        __asm__ volatile ("mov $2, %%eax; cmp $1, %%eax; .byte 0x0F, 0x9C, 0xC0" : "=q"(r) : : "cc");
        CHECK((r & 1) == 0, "setl (0F 9C)");
        __asm__ volatile ("mov $2, %%eax; cmp $1, %%eax; .byte 0x0F, 0x9D, 0xC0" : "=q"(r) : : "cc");
        CHECK((r & 1) == 1, "setge (0F 9D)");
        __asm__ volatile ("mov $2, %%eax; cmp $1, %%eax; .byte 0x0F, 0x9E, 0xC0" : "=q"(r) : : "cc");
        CHECK((r & 1) == 0, "setle (0F 9E)");
        __asm__ volatile ("mov $2, %%eax; cmp $1, %%eax; .byte 0x0F, 0x97, 0xC0" : "=q"(r) : : "cc");
        CHECK((r & 1) == 1, "seta (0F 97)");
        __asm__ volatile ("mov $0x80000000, %%eax; test %%eax, %%eax; .byte 0x0F, 0x98, 0xC0" : "=q"(r) : : "cc");
        CHECK((r & 1) == 1, "sets (0F 98)");
        __asm__ volatile ("mov $1, %%eax; test %%eax, %%eax; .byte 0x0F, 0x99, 0xC0" : "=q"(r) : : "cc");
        CHECK((r & 1) == 1, "setns (0F 99)");
        __asm__ volatile ("mov $3, %%eax; test %%eax, %%eax; .byte 0x0F, 0x9A, 0xC0" : "=q"(r) : : "cc");
        CHECK((r & 1) == 1, "setp (0F 9A)");
        __asm__ volatile ("mov $2, %%eax; test %%eax, %%eax; .byte 0x0F, 0x9B, 0xC0" : "=q"(r) : : "cc");
        CHECK((r & 1) == 1, "setnp (0F 9B)");
        __asm__ volatile ("mov $0x7FFFFFFF, %%eax; add $1, %%eax; .byte 0x0F, 0x90, 0xC0" : "=q"(r) : : "cc");
        CHECK((r & 1) == 1, "seto (0F 90)");
    }

    /* --- Jcc rel32 (0F 80-8F) --- */
    {
        unsigned int r;
        __asm__ volatile ("xor %%eax, %%eax; .byte 0x0F, 0x84; .long 5; mov $1, %%eax" : "=a"(r));
        CHECK(r == 0, "jz rel32 taken (0F 84)");
        __asm__ volatile ("mov $1, %%eax; cmp $0, %%eax; .byte 0x0F, 0x84; .long 5; mov $2, %%eax" : "=a"(r));
        CHECK(r == 2, "jz rel32 not taken");
        __asm__ volatile ("xor %%eax, %%eax; .byte 0x0F, 0x85; .long 5; mov $1, %%eax" : "=a"(r));
        CHECK(r == 1, "jnz rel32 not taken (0F 85)");
        __asm__ volatile ("mov $1, %%eax; cmp $0, %%eax; .byte 0x0F, 0x85; .long 5; mov $2, %%eax" : "=a"(r));
        CHECK(r == 1, "jnz rel32 taken");
        __asm__ volatile ("mov $1, %%eax; cmp $2, %%eax; .byte 0x0F, 0x82; .long 5; mov $2, %%eax" : "=a"(r));
        CHECK(r == 1, "jb rel32 taken (0F 82)");
        __asm__ volatile ("mov $2, %%eax; cmp $2, %%eax; .byte 0x0F, 0x83; .long 5; mov $2, %%eax" : "=a"(r));
        CHECK(r == 2, "jae rel32 taken (0F 83)");
        __asm__ volatile ("mov $2, %%eax; cmp $1, %%eax; .byte 0x0F, 0x87; .long 5; mov $2, %%eax" : "=a"(r));
        CHECK(r == 2, "ja rel32 taken (0F 87)");
        __asm__ volatile ("mov $2, %%eax; cmp $1, %%eax; .byte 0x0F, 0x8F; .long 5; mov $2, %%eax" : "=a"(r));
        CHECK(r == 2, "jg rel32 taken (0F 8F)");
        __asm__ volatile ("mov $2, %%eax; cmp $1, %%eax; .byte 0x0F, 0x8C; .long 5; mov $2, %%eax" : "=a"(r));
        CHECK(r == 2, "jl rel32 not taken (0F 8C)");
        __asm__ volatile ("mov $1, %%eax; cmp $2, %%eax; .byte 0x0F, 0x8D; .long 5; mov $2, %%eax" : "=a"(r));
        CHECK(r == 2, "jge rel32 taken (0F 8D)");
        __asm__ volatile ("mov $1, %%eax; cmp $2, %%eax; .byte 0x0F, 0x8E; .long 5; mov $2, %%eax" : "=a"(r));
        CHECK(r == 1, "jle rel32 taken (0F 8E)");
        __asm__ volatile ("mov $0x80000000, %%eax; test %%eax, %%eax; .byte 0x0F, 0x88; .long 5; mov $2, %%eax" : "=a"(r));
        CHECK(r == 0x80000000u, "js rel32 taken (0F 88)");
        __asm__ volatile ("mov $1, %%eax; test %%eax, %%eax; .byte 0x0F, 0x89; .long 5; mov $2, %%eax" : "=a"(r));
        CHECK(r == 1, "jns rel32 taken (0F 89)");
        __asm__ volatile ("mov $3, %%eax; test %%eax, %%eax; .byte 0x0F, 0x8A; .long 5; mov $2, %%eax" : "=a"(r));
        CHECK(r == 3, "jp rel32 taken (0F 8A)");
        __asm__ volatile ("mov $2, %%eax; test %%eax, %%eax; .byte 0x0F, 0x8B; .long 5; mov $2, %%eax" : "=a"(r));
        CHECK(r == 2, "jnp rel32 taken (0F 8B)");
        __asm__ volatile ("mov $0x7FFFFFFF, %%eax; add $1, %%eax; .byte 0x0F, 0x80; .long 5; mov $2, %%eax" : "=a"(r));
        CHECK(r == 0x80000000u, "jo rel32 taken (0F 80)");
    }
    /* --- short Jcc (74/75/7F/7C/7D) --- */
    {
        unsigned int r;
        __asm__ volatile ("xor %%eax, %%eax; .byte 0x74, 0x05; mov $1, %%eax" : "=a"(r));
        CHECK(r == 0, "jz rel8 taken (74)");
        __asm__ volatile ("mov $1, %%eax; cmp $0, %%eax; .byte 0x74, 0x05; mov $2, %%eax" : "=a"(r));
        CHECK(r == 2, "jz rel8 not taken");
        __asm__ volatile ("xor %%eax, %%eax; .byte 0x75, 0x05; mov $1, %%eax" : "=a"(r));
        CHECK(r == 1, "jnz rel8 not taken (75)");
        __asm__ volatile ("mov $2, %%eax; cmp $1, %%eax; .byte 0x7F, 0x05; mov $2, %%eax" : "=a"(r));
        CHECK(r == 2, "jg rel8 taken (7F)");
        __asm__ volatile ("mov $2, %%eax; cmp $1, %%eax; .byte 0x7C, 0x05; mov $2, %%eax" : "=a"(r));
        CHECK(r == 2, "jl rel8 not taken (7C)");
        __asm__ volatile ("mov $1, %%eax; cmp $2, %%eax; .byte 0x7D, 0x05; mov $2, %%eax" : "=a"(r));
        CHECK(r == 2, "jge rel8 taken (7D)");
    }
    /* --- JECXZ (E3), LOOP/LOOPE/LOOPNE (E2/E1/E0) --- */
    {
        unsigned int r;
        __asm__ volatile ("xor %%ecx, %%ecx; mov $9, %%eax; .byte 0xE3, 0x05; mov $1, %%eax" : "=a"(r) : : "ecx");
        CHECK(r == 9, "jecxz taken (E3)");
        __asm__ volatile ("mov $3, %%ecx; mov $9, %%eax; .byte 0xE3, 0x05; mov $1, %%eax" : "=a"(r) : : "ecx");
        CHECK(r == 1, "jecxz not taken");
        __asm__ volatile ("mov $4, %%ecx; xor %%eax, %%eax; inc %%eax; .byte 0xE2, 0xFD" : "=a"(r) : : "ecx");
        CHECK(r == 4, "loop ecx=4 body-4x (E2)");
        __asm__ volatile ("mov $4, %%ecx; xor %%eax, %%eax; lea 1(%%eax), %%eax; .byte 0xE1, 0xFB" : "=a"(r) : : "ecx");
        CHECK(r == 4, "loope ZF=1 body-4x (E1)");
        __asm__ volatile ("mov $4, %%ecx; mov $1, %%eax; cmp $0, %%eax; inc %%eax; .byte 0xE0, 0xFD" : "=a"(r) : : "ecx", "cc");
        CHECK(r == 5, "loopne ZF=0 body-4x (E0)");
    }
    /* --- PUSHAD/POPAD (60/61) --- */
    {
        unsigned int a = 1, b = 2, c = 3, d = 4, si = 5, di = 6;
        __asm__ volatile (
            "movl $1, %%eax\nmovl $2, %%ebx\nmovl $3, %%ecx\nmovl $4, %%edx\n"
            "movl $5, %%esi\nmovl $6, %%edi\n"
            ".byte 0x60\n"
            "xorl %%eax, %%eax\n"
            ".byte 0x61\n"
            "movl %%eax, %0\nmovl %%ebx, %1\nmovl %%ecx, %2\nmovl %%edx, %3\n"
            "movl %%esi, %4\nmovl %%edi, %5\n"
            : "=m"(a), "=m"(b), "=m"(c), "=m"(d), "=m"(si), "=m"(di)
            :
            : "eax", "ebx", "ecx", "edx", "esi", "edi", "memory");
        CHECK(a == 1 && b == 2 && c == 3 && d == 4 && si == 5 && di == 6,
              "pushad/popad roundtrip (60/61)");
    }
    /* --- PUSHFD/POPFD (9C/9D) --- */
    {
        unsigned int f;
        __asm__ volatile ("mov $0, %%eax; cmp $0, %%eax; pushfl; pop %%ecx; and $0x40, %%ecx; mov %%ecx, %0"
                          : "=r"(f) : : "eax", "ecx", "cc");
        CHECK((f & 0x40) == 0x40, "pushfd captures ZF (9C/9D)");
    }

    if (fails == 0) {
        printf("ALL PASS\n");
        ExitProcess(0);
    }
    printf("FAILURES: %d\n", fails);
    ExitProcess(1);
    return 0;
}
