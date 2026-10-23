// ThetaZ — 32-bit x86 interpreter core
// Copyright (c) 2026 ThetaZ Project. MIT License.

#include "core/x86/x86.h"
#include "core/x86/api_dispatch.h"
#include "core/ntdll/thread.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <csignal>
#include <csetjmp>
#include <ctime>
#include <sys/mman.h>
#include <unistd.h>

namespace ThetaZ {

uint64_t get_main_module_base();

// ---------------------------------------------------------------------------
// Wild-jump detection
//
// True when an address could plausibly hold guest code: inside the mapped
// image range the loader uses, the guest heap, or a thread stack. Anything
// else is a corrupted function pointer. Only feeds the env-gated
// THETAZ_TRACE_WILD report, which prints the full register set so a wild
// transfer can be traced back to the API that produced the pointer.
// ---------------------------------------------------------------------------
static inline bool guest_addr_may_hold_code(uint32_t a) {
    if (a < 0x01000000u) return true;                  // image bases / low maps
    if (a >= 0x40000000u && a < 0x78000000u) return true; // guest heap + thunks
    return false;
}

// Callback return sentinels pushed by x86_call_guest_args(); a RET to one of
// these is the normal end of a guest callback, not a wild transfer.
static inline bool is_callback_sentinel(uint32_t a) {
    return a >= 0xCA11B000u && a < 0xCA11B100u;
}

static inline void report_wild_transfer(const char* kind, uint32_t target,
                                        const X86CpuState& s, const char* src,
                                        uint32_t rm, uint32_t mem) {
    if (!getenv("THETAZ_TRACE_WILD")) return;
    fprintf(stderr,
            "[wild%s] target=0x%08x from=0x%08x %s reg=%u mem=0x%08x esi=0x%08x "
            "edi=0x%08x ebp=0x%08x esp=0x%08x eax=0x%08x ecx=0x%08x edx=0x%08x ebx=0x%08x\n",
            kind, target, s.eip, src, rm, mem, s.regs[XR_ESI], s.regs[XR_EDI],
            s.regs[XR_EBP], s.regs[XR_ESP], s.regs[XR_EAX], s.regs[XR_ECX],
            s.regs[XR_EDX], s.regs[XR_EBX]);
}

// ---------------------------------------------------------------------------
// Guest memory (identity-mapped below 4GB)
// ---------------------------------------------------------------------------

static inline uint8_t mr8(uint32_t a) {
    return *reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(a));
}
static inline uint16_t mr16(uint32_t a) {
    return *reinterpret_cast<uint16_t*>(static_cast<uintptr_t>(a));
}
static inline uint32_t mr32(uint32_t a) {
    return *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(a));
}
static inline uint64_t mr64(uint32_t a) {
    return *reinterpret_cast<uint64_t*>(static_cast<uintptr_t>(a));
}
static uint32_t g_watch_addr = 0;
static uint32_t g_watch_eip = 0;
static bool g_watch_init = false;
static inline void watch_init() {
    if (!g_watch_init) {
        g_watch_init = true;
        const char* env = getenv("THETAZ_WATCH_ADDR");
        if (env && env[0]) g_watch_addr = (uint32_t)strtoul(env, nullptr, 0);
    }
}
static inline void mw8(uint32_t a, uint8_t v) {
    watch_init();
    if (g_watch_addr && a >= g_watch_addr && a < g_watch_addr + 4) {
        fprintf(stderr, "[watch] write8 0x%08x <- 0x%02x\n", a, v);
    }
    *reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(a)) = v;
}
static inline void mw16(uint32_t a, uint16_t v) {
    watch_init();
    if (g_watch_addr && a >= g_watch_addr && a < g_watch_addr + 4) {
        fprintf(stderr, "[watch] write16 0x%08x <- 0x%04x\n", a, v);
    }
    *reinterpret_cast<uint16_t*>(static_cast<uintptr_t>(a)) = v;
}
static inline void watch_check(uint32_t a, uint32_t v) {
    watch_init();
    if (g_watch_addr && a == g_watch_addr) {
        fprintf(stderr, "[watch] eip=0x%08x write 0x%08x <- 0x%08x\n", g_watch_eip, a, v);
    }
}
static inline void mw32(uint32_t a, uint32_t v) {
    watch_check(a, v);
    *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(a)) = v;
}

static inline void mw64(uint32_t a, uint64_t v) {
    watch_init();
    if (g_watch_addr && a < g_watch_addr + 4 && a + 8 > g_watch_addr) {
        fprintf(stderr, "[watch] write64 0x%08x <- 0x%016llx\n", a,
                (unsigned long long)v);
    }
    *reinterpret_cast<uint64_t*>(static_cast<uintptr_t>(a)) = v;
}

static inline uint8_t fetch8(X86CpuState& s) { return mr8(s.eip++); }
static inline uint16_t fetch16(X86CpuState& s) {
    uint16_t v = mr16(s.eip);
    s.eip += 2;
    return v;
}
static inline uint32_t fetch32(X86CpuState& s) {
    uint32_t v = mr32(s.eip);
    s.eip += 4;
    return v;
}

// ---------------------------------------------------------------------------
// Registers
// ---------------------------------------------------------------------------

static inline uint16_t reg16(X86CpuState& s, uint8_t r) {
    return static_cast<uint16_t>(s.regs[r]);
}
static inline void set_reg16(X86CpuState& s, uint8_t r, uint16_t v) {
    s.regs[r] = (s.regs[r] & 0xFFFF0000u) | v;
}
static inline uint8_t reg8_lo(X86CpuState& s, uint8_t r) {
    return static_cast<uint8_t>(s.regs[r]);
}
static inline uint8_t reg8_hi(X86CpuState& s, uint8_t r) {
    return static_cast<uint8_t>(s.regs[r] >> 8);
}
static inline void set_reg8_lo(X86CpuState& s, uint8_t r, uint8_t v) {
    s.regs[r] = (s.regs[r] & 0xFFFFFF00u) | v;
}
static inline void set_reg8_hi(X86CpuState& s, uint8_t r, uint8_t v) {
    s.regs[r] = (s.regs[r] & 0xFFFF00FFu) | (static_cast<uint32_t>(v) << 8);
}

// 8-bit register accessor handling the AH/CH/DH/BH special case.
static inline uint8_t read_reg8(X86CpuState& s, uint8_t r) {
    return r >= 4 ? reg8_hi(s, r - 4) : reg8_lo(s, r);
}
static inline void write_reg8(X86CpuState& s, uint8_t r, uint8_t v) {
    if (r >= 4) set_reg8_hi(s, r - 4, v);
    else set_reg8_lo(s, r, v);
}

// ---------------------------------------------------------------------------
// Flags
// ---------------------------------------------------------------------------

static constexpr uint32_t F_CF = 0x0001;
static constexpr uint32_t F_PF = 0x0004;
static constexpr uint32_t F_AF = 0x0010;
static constexpr uint32_t F_ZF = 0x0040;
static constexpr uint32_t F_SF = 0x0080;
static constexpr uint32_t F_TF = 0x0100;
static constexpr uint32_t F_IF = 0x0200;
static constexpr uint32_t F_DF = 0x0400;
static constexpr uint32_t F_OF = 0x0800;
static constexpr uint32_t F_ARITH = F_CF | F_PF | F_AF | F_ZF | F_SF | F_OF;

static inline uint32_t parity_flag(uint32_t v) {
    uint8_t b = static_cast<uint8_t>(v);
    b = static_cast<uint8_t>(b ^ (b >> 4));
    b = static_cast<uint8_t>(b ^ (b >> 2));
    b = static_cast<uint8_t>(b ^ (b >> 1));
    return (b & 1) ? 0 : F_PF;
}

template <typename T>
static inline void set_arith_flags(X86CpuState& s, T a, T b, T r, bool carry, bool borrow, uint32_t carry_in = 0) {
    const T msb = T(1) << (sizeof(T) * 8 - 1);
    uint32_t f = 0;
    if (r == 0) f |= F_ZF;
    if (r & msb) f |= F_SF;
    // AF: carry/borrow out of bit 3, on the LOW nibbles. The ADC/SBB
    // carry-in is passed separately (b is the plain operand); logical ops
    // (neither carry nor borrow) always clear AF.
    if (borrow) {
        uint32_t sb = (b & 0xF) + carry_in;          // effective subtrahend nibble
        if (sb >= 16 || (a & 0xF) < sb) f |= F_AF;
    } else if (carry) {
        if ((a & 0xF) + (b & 0xF) + carry_in >= 16) f |= F_AF;
    }
    f |= parity_flag(static_cast<uint32_t>(r));
    if (carry && static_cast<uint64_t>(r) < static_cast<uint64_t>(a)) f |= F_CF;
    if (borrow && static_cast<uint64_t>(a) < static_cast<uint64_t>(b)) f |= F_CF;
    if (borrow) {
        // Subtraction overflow: (a ^ r) & ((~b) ^ r) & msb. Using the addition
        // formula with `b` gives false overflow for e.g. 0 - 64, which breaks
        // every signed conditional jump after CMP/SUB.
        if (((a ^ r) & ((~b) ^ r) & msb) != 0) f |= F_OF;
    } else {
        if (((a ^ r) & (b ^ r) & msb) != 0) f |= F_OF;
    }
    s.eflags = (s.eflags & ~F_ARITH) | f;
}

template <typename T>
static inline void set_arith_flags_shift(X86CpuState& s, T r, bool cf, bool of) {
    const T msb = T(1) << (sizeof(T) * 8 - 1);
    uint32_t f = 0;
    if (r == 0) f |= F_ZF;
    if (r & msb) f |= F_SF;
    f |= parity_flag(static_cast<uint32_t>(r));
    if (cf) f |= F_CF;
    if (of) f |= F_OF;
    s.eflags = (s.eflags & ~F_ARITH) | f;
}

static inline uint32_t crc32_step(uint32_t crc, uint32_t byte) {
    crc ^= byte;
    for (int i = 0; i < 8; i++)
        crc = (crc & 1) ? ((crc >> 1) ^ 0xEDB88320u) : (crc >> 1);
    return crc;
}

static inline void add8(X86CpuState& s, uint8_t& d, uint8_t v) {
    uint8_t r = static_cast<uint8_t>(d + v);
    set_arith_flags<uint8_t>(s, d, v, r, true, false);
    d = r;
}
static inline void add16(X86CpuState& s, uint16_t& d, uint16_t v) {
    uint16_t r = static_cast<uint16_t>(d + v);
    set_arith_flags<uint16_t>(s, d, v, r, true, false);
    d = r;
}
static inline void add32(X86CpuState& s, uint32_t& d, uint32_t v) {
    uint32_t r = d + v;
    set_arith_flags<uint32_t>(s, d, v, r, true, false);
    d = r;
}
static inline void sub8(X86CpuState& s, uint8_t& d, uint8_t v) {
    uint8_t r = static_cast<uint8_t>(d - v);
    set_arith_flags<uint8_t>(s, d, v, r, false, true);
    d = r;
}
static inline void sub16(X86CpuState& s, uint16_t& d, uint16_t v) {
    uint16_t r = static_cast<uint16_t>(d - v);
    set_arith_flags<uint16_t>(s, d, v, r, false, true);
    d = r;
}
static inline void sub32(X86CpuState& s, uint32_t& d, uint32_t v) {
    uint32_t r = d - v;
    set_arith_flags<uint32_t>(s, d, v, r, false, true);
    d = r;
}
static inline void cmp8(X86CpuState& s, uint8_t d, uint8_t v) {
    uint8_t r = static_cast<uint8_t>(d - v);
    set_arith_flags<uint8_t>(s, d, v, r, false, true);
}
static inline void cmp16(X86CpuState& s, uint16_t d, uint16_t v) {
    uint16_t r = static_cast<uint16_t>(d - v);
    set_arith_flags<uint16_t>(s, d, v, r, false, true);
}
static inline void cmp32(X86CpuState& s, uint32_t d, uint32_t v) {
    uint32_t r = d - v;
    set_arith_flags<uint32_t>(s, d, v, r, false, true);
}

static inline void and32(X86CpuState& s, uint32_t& d, uint32_t v) {
    d &= v;
    s.eflags = (s.eflags & ~F_ARITH) | ((d == 0) ? F_ZF : 0) |
               ((d & 0x80000000) ? F_SF : 0) | parity_flag(d);
}
static inline void or32(X86CpuState& s, uint32_t& d, uint32_t v) {
    d |= v;
    s.eflags = (s.eflags & ~F_ARITH) | ((d == 0) ? F_ZF : 0) |
               ((d & 0x80000000) ? F_SF : 0) | parity_flag(d);
}
static inline void xor32(X86CpuState& s, uint32_t& d, uint32_t v) {
    d ^= v;
    s.eflags = (s.eflags & ~F_ARITH) | ((d == 0) ? F_ZF : 0) |
               ((d & 0x80000000) ? F_SF : 0) | parity_flag(d);
}
static inline void test32(X86CpuState& s, uint32_t a, uint32_t b) {
    uint32_t r = a & b;
    s.eflags = (s.eflags & ~F_ARITH) | ((r == 0) ? F_ZF : 0) |
               ((r & 0x80000000) ? F_SF : 0) | parity_flag(r);
}

static inline void and8(X86CpuState& s, uint8_t& d, uint8_t v) {
    d &= v;
    s.eflags = (s.eflags & ~F_ARITH) | ((d == 0) ? F_ZF : 0) |
               ((d & 0x80) ? F_SF : 0) | parity_flag(d);
}
static inline void or8(X86CpuState& s, uint8_t& d, uint8_t v) {
    d |= v;
    s.eflags = (s.eflags & ~F_ARITH) | ((d == 0) ? F_ZF : 0) |
               ((d & 0x80) ? F_SF : 0) | parity_flag(d);
}
static inline void xor8(X86CpuState& s, uint8_t& d, uint8_t v) {
    d ^= v;
    s.eflags = (s.eflags & ~F_ARITH) | ((d == 0) ? F_ZF : 0) |
               ((d & 0x80) ? F_SF : 0) | parity_flag(d);
}
static inline void test8(X86CpuState& s, uint8_t a, uint8_t b) {
    uint8_t r = static_cast<uint8_t>(a & b);
    s.eflags = (s.eflags & ~F_ARITH) | ((r == 0) ? F_ZF : 0) |
               ((r & 0x80) ? F_SF : 0) | parity_flag(r);
}

static inline bool flag_cf(X86CpuState& s) { return (s.eflags & F_CF) != 0; }
static inline bool flag_pf(X86CpuState& s) { return (s.eflags & F_PF) != 0; }
static inline bool flag_af(X86CpuState& s) { return (s.eflags & F_AF) != 0; }
static inline bool flag_zf(X86CpuState& s) { return (s.eflags & F_ZF) != 0; }
static inline bool flag_sf(X86CpuState& s) { return (s.eflags & F_SF) != 0; }
static inline bool flag_of(X86CpuState& s) { return (s.eflags & F_OF) != 0; }
static inline bool flag_df(X86CpuState& s) { return (s.eflags & F_DF) != 0; }

// ---------------------------------------------------------------------------
// Stack
// ---------------------------------------------------------------------------

static inline void push16(X86CpuState& s, uint16_t v) {
    s.regs[XR_ESP] -= 2;
    mw16(s.regs[XR_ESP], v);
}
static inline void push32(X86CpuState& s, uint32_t v) {
    s.regs[XR_ESP] -= 4;
    mw32(s.regs[XR_ESP], v);
}
static inline uint16_t pop16(X86CpuState& s) {
    uint16_t v = mr16(s.regs[XR_ESP]);
    s.regs[XR_ESP] += 2;
    return v;
}
static inline uint32_t pop32(X86CpuState& s) {
    uint32_t v = mr32(s.regs[XR_ESP]);
    s.regs[XR_ESP] += 4;
    return v;
}

// ---------------------------------------------------------------------------
// ModRM / effective address
// ---------------------------------------------------------------------------

static inline uint32_t seg_base(X86CpuState& s, int segovr) {
    switch (segovr) {
        case 0: return 0; // CS
        case 1: return 0; // SS
        case 2: return 0; // DS
        case 3: return 0; // ES
        case 4: return s.fs_base;
        case 5: return s.gs_base;
        default: return 0;
    }
}

// Map a ModRM Sreg field to the s.seg[] index (s.seg[4]=FS, s.seg[5]=GS).
static inline uint8_t seg_index(uint8_t regf) {
    static const uint8_t map[8] = { 3, 0, 1, 2, 4, 5, 4, 5 };
    return map[regf & 7];
}

// 32-bit effective address from a ModRM byte.
static uint32_t calc_ea32(X86CpuState& s, uint8_t modrm, bool addrsize16) {
    uint8_t mod = modrm >> 6;
    uint8_t rm = modrm & 7;
    uint32_t addr = 0;

    if (!addrsize16) {
        if (mod == 3) return 0; // register form; not used here
        if (rm == 4) {
            uint8_t sib = fetch8(s);
            uint8_t scale = sib >> 6;
            uint8_t index = (sib >> 3) & 7;
            uint8_t base = sib & 7;
            addr = 0;
            if (index != 4) addr += s.regs[index] << scale;
            if (base == 5 && mod == 0) {
                addr += fetch32(s);
            } else {
                addr += s.regs[base];
            }
        } else if (rm == 5 && mod == 0) {
            addr = fetch32(s);
        } else {
            addr = s.regs[rm];
        }
        if (mod == 1) addr += static_cast<int8_t>(fetch8(s));
        else if (mod == 2) addr += fetch32(s);
        return addr;
    }

    // 16-bit addressing (0x67)
    uint32_t v = 0;
    if (rm == 0) v = s.regs[XR_EBX] + s.regs[XR_ESI];
    else if (rm == 1) v = s.regs[XR_EBX] + s.regs[XR_EDI];
    else if (rm == 2) v = s.regs[XR_EBP] + s.regs[XR_ESI];
    else if (rm == 3) v = s.regs[XR_EBP] + s.regs[XR_EDI];
    else if (rm == 4) v = s.regs[XR_ESI];
    else if (rm == 5) v = s.regs[XR_EDI];
    else if (rm == 6) v = s.regs[XR_EBP];
    else v = s.regs[XR_EBX];

    if (rm == 6 && mod == 0) v = fetch16(s);
    else if (mod == 1) v += static_cast<int8_t>(fetch8(s));
    else if (mod == 2) v += static_cast<int16_t>(fetch16(s));
    return v & 0xFFFF;
}

// Decode r/m operand: returns effective address for memory, or 0xFFFFFFFF for
// register form. Sets *is_reg and *reg.
static uint32_t decode_rm(X86CpuState& s, uint8_t modrm, bool addrsize16,
                          bool* is_reg, uint8_t* reg) {
    *reg = modrm & 7;
    if ((modrm >> 6) == 3) {
        *is_reg = true;
        return 0;
    }
    *is_reg = false;
    return calc_ea32(s, modrm, addrsize16);
}

// ---------------------------------------------------------------------------
// Shifts / rotates
// ---------------------------------------------------------------------------

static void shift32(X86CpuState& s, uint32_t& d, uint8_t c, uint8_t op) {
    uint32_t src = d;
    c &= 31;
    if (c == 0) return;
    uint32_t cf = 0;
    switch (op) {
        case 0: // ROL
            for (int i = 0; i < c; i++) {
                cf = (d >> 31) & 1;
                d = (d << 1) | cf;
            }
            s.eflags = (s.eflags & ~(F_CF | F_OF)) | (cf ? F_CF : 0);
            if (c == 1) s.eflags |= ((cf ^ ((d >> 31) & 1)) ? F_OF : 0);
            break;
        case 1: // ROR
            for (int i = 0; i < c; i++) {
                cf = d & 1;
                d = (d >> 1) | (cf << 31);
            }
            s.eflags = (s.eflags & ~(F_CF | F_OF)) | (cf ? F_CF : 0);
            if (c == 1) s.eflags |= (((((d >> 31) & 1) ^ ((src >> 31) & 1))) ? F_OF : 0);
            break;
        case 2: // RCL
            cf = flag_cf(s) ? 1 : 0;
            for (int i = 0; i < c; i++) {
                uint32_t nc = (d >> 31) & 1;
                d = (d << 1) | cf;
                cf = nc;
            }
            s.eflags = (s.eflags & ~(F_CF | F_OF)) | (cf ? F_CF : 0);
            if (c == 1) s.eflags |= ((cf ^ ((d >> 31) & 1)) ? F_OF : 0);
            break;
        case 3: // RCR
            cf = flag_cf(s) ? 1 : 0;
            for (int i = 0; i < c; i++) {
                uint32_t nc = d & 1;
                d = (d >> 1) | (cf << 31);
                cf = nc;
            }
            s.eflags = (s.eflags & ~(F_CF | F_OF)) | (cf ? F_CF : 0);
            if (c == 1) s.eflags |= (((((src >> 31) & 1) ^ ((d >> 31) & 1))) ? F_OF : 0);
            break;
        case 4: // SHL/SAL
            cf = (d >> (32 - c)) & 1;
            d <<= c;
            s.eflags = (s.eflags & ~(F_CF | F_OF)) | (cf ? F_CF : 0);
            if (c == 1) s.eflags |= (((d ^ src) & 0x80000000) ? F_OF : 0);
            if (d == 0) s.eflags |= F_ZF; else s.eflags &= ~F_ZF;
            if (d & 0x80000000) s.eflags |= F_SF; else s.eflags &= ~F_SF;
            s.eflags = (s.eflags & ~F_PF) | parity_flag(d);
            break;
        case 5: // SHR
            cf = (d >> (c - 1)) & 1;
            d >>= c;
            s.eflags = (s.eflags & ~(F_CF | F_OF)) | (cf ? F_CF : 0);
            if (c == 1) s.eflags |= (((d ^ src) & 0x80000000) ? F_OF : 0);
            if (d == 0) s.eflags |= F_ZF; else s.eflags &= ~F_ZF;
            if (d & 0x80000000) s.eflags |= F_SF; else s.eflags &= ~F_SF;
            s.eflags = (s.eflags & ~F_PF) | parity_flag(d);
            break;
        case 7: // SAR
            cf = (d >> (c - 1)) & 1;
            d = static_cast<uint32_t>(static_cast<int32_t>(d) >> c);
            s.eflags = (s.eflags & ~(F_CF | F_OF)) | (cf ? F_CF : 0);
            if (c == 1) s.eflags |= (((d ^ src) & 0x80000000) ? F_OF : 0);
            if (d == 0) s.eflags |= F_ZF; else s.eflags &= ~F_ZF;
            if (d & 0x80000000) s.eflags |= F_SF; else s.eflags &= ~F_SF;
            s.eflags = (s.eflags & ~F_PF) | parity_flag(d);
            break;
        default:
            break;
    }
    (void)src;
}

static void shift16(X86CpuState& s, uint16_t& d, uint8_t c, uint8_t op) {
    uint16_t src = d;
    c &= 31;
    if (c == 0) return;
    uint16_t cf = 0;
    switch (op) {
        case 0:
            for (int i = 0; i < c; i++) { cf = (d >> 15) & 1; d = (uint16_t)((d << 1) | cf); }
            s.eflags = (s.eflags & ~(F_CF | F_OF)) | (cf ? F_CF : 0);
            if (c == 1) s.eflags |= ((cf ^ ((d >> 15) & 1)) ? F_OF : 0);
            break;
        case 1:
            for (int i = 0; i < c; i++) { cf = d & 1; d = (uint16_t)((d >> 1) | (cf << 15)); }
            s.eflags = (s.eflags & ~(F_CF | F_OF)) | (cf ? F_CF : 0);
            if (c == 1) s.eflags |= (((((d >> 15) & 1) ^ ((src >> 15) & 1))) ? F_OF : 0);
            break;
        case 2:
            cf = flag_cf(s) ? 1 : 0;
            for (int i = 0; i < c; i++) { uint16_t nc = (d >> 15) & 1; d = (uint16_t)((d << 1) | cf); cf = nc; }
            s.eflags = (s.eflags & ~(F_CF | F_OF)) | (cf ? F_CF : 0);
            if (c == 1) s.eflags |= ((cf ^ ((d >> 15) & 1)) ? F_OF : 0);
            break;
        case 3:
            cf = flag_cf(s) ? 1 : 0;
            for (int i = 0; i < c; i++) { uint16_t nc = d & 1; d = (uint16_t)((d >> 1) | (cf << 15)); cf = nc; }
            s.eflags = (s.eflags & ~(F_CF | F_OF)) | (cf ? F_CF : 0);
            if (c == 1) s.eflags |= (((((src >> 15) & 1) ^ ((d >> 15) & 1))) ? F_OF : 0);
            break;
        case 4:
            cf = (c <= 16) ? ((d >> (16 - c)) & 1) : 0;
            d = (uint16_t)(d << c);
            s.eflags = (s.eflags & ~(F_CF | F_OF)) | (cf ? F_CF : 0);
            if (c == 1) s.eflags |= (((d ^ src) & 0x8000) ? F_OF : 0);
            if (d == 0) s.eflags |= F_ZF; else s.eflags &= ~F_ZF;
            if (d & 0x8000) s.eflags |= F_SF; else s.eflags &= ~F_SF;
            s.eflags = (s.eflags & ~F_PF) | parity_flag(d);
            break;
        case 5:
            cf = (d >> (c - 1)) & 1;
            d >>= c;
            s.eflags = (s.eflags & ~(F_CF | F_OF)) | (cf ? F_CF : 0);
            if (c == 1) s.eflags |= (((d ^ src) & 0x8000) ? F_OF : 0);
            if (d == 0) s.eflags |= F_ZF; else s.eflags &= ~F_ZF;
            if (d & 0x8000) s.eflags |= F_SF; else s.eflags &= ~F_SF;
            s.eflags = (s.eflags & ~F_PF) | parity_flag(d);
            break;
        case 7:
            cf = (d >> ((c >= 16) ? 15 : (c - 1))) & 1;
            d = static_cast<uint16_t>(static_cast<int16_t>(d) >> c);
            s.eflags = (s.eflags & ~(F_CF | F_OF)) | (cf ? F_CF : 0);
            if (c == 1) s.eflags |= (((d ^ src) & 0x8000) ? F_OF : 0);
            if (d == 0) s.eflags |= F_ZF; else s.eflags &= ~F_ZF;
            if (d & 0x8000) s.eflags |= F_SF; else s.eflags &= ~F_SF;
            s.eflags = (s.eflags & ~F_PF) | parity_flag(d);
            break;
        default:
            break;
    }
    (void)src;
}

static void shift8(X86CpuState& s, uint8_t& d, uint8_t c, uint8_t op) {
    uint8_t src = d;
    c &= 31;
    if (c == 0) return;
    uint8_t cf = 0;
    switch (op) {
        case 0:
            for (int i = 0; i < c; i++) { cf = (d >> 7) & 1; d = (uint8_t)((d << 1) | cf); }
            s.eflags = (s.eflags & ~(F_CF | F_OF)) | (cf ? F_CF : 0);
            if (c == 1) s.eflags |= ((cf ^ ((d >> 7) & 1)) ? F_OF : 0);
            break;
        case 1:
            for (int i = 0; i < c; i++) { cf = d & 1; d = (uint8_t)((d >> 1) | (cf << 7)); }
            s.eflags = (s.eflags & ~(F_CF | F_OF)) | (cf ? F_CF : 0);
            if (c == 1) s.eflags |= (((((d >> 7) & 1) ^ ((src >> 7) & 1))) ? F_OF : 0);
            break;
        case 2:
            cf = flag_cf(s) ? 1 : 0;
            for (int i = 0; i < c; i++) { uint8_t nc = (d >> 7) & 1; d = (uint8_t)((d << 1) | cf); cf = nc; }
            s.eflags = (s.eflags & ~(F_CF | F_OF)) | (cf ? F_CF : 0);
            if (c == 1) s.eflags |= ((cf ^ ((d >> 7) & 1)) ? F_OF : 0);
            break;
        case 3:
            cf = flag_cf(s) ? 1 : 0;
            for (int i = 0; i < c; i++) { uint8_t nc = d & 1; d = (uint8_t)((d >> 1) | (cf << 7)); cf = nc; }
            s.eflags = (s.eflags & ~(F_CF | F_OF)) | (cf ? F_CF : 0);
            if (c == 1) s.eflags |= (((((src >> 7) & 1) ^ ((d >> 7) & 1))) ? F_OF : 0);
            break;
        case 4:
            cf = (c <= 8) ? ((d >> (8 - c)) & 1) : 0;
            d = (uint8_t)(d << c);
            s.eflags = (s.eflags & ~(F_CF | F_OF)) | (cf ? F_CF : 0);
            if (c == 1) s.eflags |= (((d ^ src) & 0x80) ? F_OF : 0);
            if (d == 0) s.eflags |= F_ZF; else s.eflags &= ~F_ZF;
            if (d & 0x80) s.eflags |= F_SF; else s.eflags &= ~F_SF;
            s.eflags = (s.eflags & ~F_PF) | parity_flag(d);
            break;
        case 5:
            cf = (d >> (c - 1)) & 1;
            d >>= c;
            s.eflags = (s.eflags & ~(F_CF | F_OF)) | (cf ? F_CF : 0);
            if (c == 1) s.eflags |= (((d ^ src) & 0x80) ? F_OF : 0);
            if (d == 0) s.eflags |= F_ZF; else s.eflags &= ~F_ZF;
            if (d & 0x80) s.eflags |= F_SF; else s.eflags &= ~F_SF;
            s.eflags = (s.eflags & ~F_PF) | parity_flag(d);
            break;
        case 7:
            cf = (d >> ((c >= 8) ? 7 : (c - 1))) & 1;
            d = static_cast<uint8_t>(static_cast<int8_t>(d) >> c);
            s.eflags = (s.eflags & ~(F_CF | F_OF)) | (cf ? F_CF : 0);
            if (c == 1) s.eflags |= (((d ^ src) & 0x80) ? F_OF : 0);
            if (d == 0) s.eflags |= F_ZF; else s.eflags &= ~F_ZF;
            if (d & 0x80) s.eflags |= F_SF; else s.eflags &= ~F_SF;
            s.eflags = (s.eflags & ~F_PF) | parity_flag(d);
            break;
        default:
            break;
    }
    (void)src;
}

// ---------------------------------------------------------------------------
// x87
// ---------------------------------------------------------------------------

static inline double st_i(X86CpuState& s, int i) { return s.fpu_st[(s.fpu_top + i) & 7]; }
static inline void set_st_i(X86CpuState& s, int i, double v) { s.fpu_st[(s.fpu_top + i) & 7] = v; }
static inline double st_top(X86CpuState& s) { return s.fpu_st[s.fpu_top]; }
static inline void set_st_top(X86CpuState& s, double v) { s.fpu_st[s.fpu_top] = v; }
static inline void push_st(X86CpuState& s, double v) {
    s.fpu_top = (s.fpu_top + 7) & 7;
    s.fpu_st[s.fpu_top] = v;
}
static inline double pop_st(X86CpuState& s) {
    double v = s.fpu_st[s.fpu_top];
    s.fpu_top = (s.fpu_top + 1) & 7;
    return v;
}

// x87 memory conversions: guests store IEEE bits; convert, never reinterpret.
static inline double f32_to_double(uint32_t bits) { float f; memcpy(&f, &bits, 4); return f; }
static inline double f64_to_double(uint64_t bits) { double d; memcpy(&d, &bits, 8); return d; }
static inline uint32_t double_to_f32_bits(double v) { float f = static_cast<float>(v); uint32_t b; memcpy(&b, &f, 4); return b; }
static inline uint64_t double_to_f64_bits(double v) { uint64_t b; memcpy(&b, &v, 8); return b; }

static void fpu_set_cmp_flags(X86CpuState& s, double a, double b) {
    uint16_t c0 = 0, c2 = 0, c3 = 0;
    if (std::isnan(a) || std::isnan(b)) { c0 = c2 = c3 = 1; }
    else if (a < b) c0 = 1;
    else if (a > b) { /* C0=0 */ }
    else c3 = 1;
    s.fpu_sw = (s.fpu_sw & ~0x4500) | (c0 ? 0x100 : 0) | (c2 ? 0x400 : 0) | (c3 ? 0x4000 : 0);
}

// ---------------------------------------------------------------------------
// Fault handling
// ---------------------------------------------------------------------------

// The active fault longjmp target. Each x86_run invocation uses its own
// sigjmp_buf so nested runs (x86_call_guest_args / SEH dispatch) fault into
// their own handler instead of unwinding through the enclosing host frames.
static thread_local sigjmp_buf* g_fault_jmp_ptr = nullptr;
static thread_local bool g_interp_active = false;
static thread_local X86CpuState* g_fault_state = nullptr;
static thread_local uint32_t g_callback_sentinel = 0;
static thread_local uint32_t g_trace_buf[128];
static thread_local int g_trace_pos = 0;

// Guest EIP/ESP at the start of the instruction currently being interpreted.
// A host signal handler runs after the memory helper has already advanced EIP
// past the opcode, so it must not use g_fault_state->eip as the fault PC.
static thread_local uint32_t g_insn_start_eip = 0;
static thread_local uint32_t g_insn_start_esp = 0;

// Metadata recorded by the signal handler (async-signal-safe fields only) and
// consumed after siglongjmp in normal execution context.
static thread_local bool g_guest_fault_pending = false;
static thread_local bool g_in_guest_exception_dispatch = false;
static thread_local int g_guest_fault_signal = 0;
static thread_local int g_guest_fault_si_code = 0;
static thread_local uint32_t g_guest_fault_addr = 0;
static thread_local uint32_t g_guest_fault_eip = 0;
static thread_local uint32_t g_guest_fault_esp = 0;
static thread_local bool g_guest_fault_write = false;

static void trace_push(uint32_t eip) {
    // Always record: a single array store, and it makes the fault dump's EIP
    // ring available without the very verbose THETAZ_TRACE_INSNS per-instruction
    // log (which is what pinpointing a wild jump depends on).
    g_trace_buf[g_trace_pos & 127] = eip;
    g_trace_pos++;
}

static void trace_dump() {
    if (g_trace_pos == 0) return;
    int n = g_trace_pos < 128 ? g_trace_pos : 128;
    int start = g_trace_pos - n;
    fprintf(stderr, "  recent insns:");
    for (int i = 0; i < n; i++) {
        if (i % 8 == 0) fprintf(stderr, "\n    ");
        fprintf(stderr, "0x%08x ", g_trace_buf[(start + i) & 127]);
    }
    fprintf(stderr, "\n");
}

static const char* sigfpe_exception_name(int si_code) {
    switch (si_code) {
        case FPE_INTDIV: return "EXCEPTION_INT_DIVIDE_BY_ZERO";
        case FPE_INTOVF: return "EXCEPTION_INT_OVERFLOW";
        case FPE_FLTDIV: return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
        case FPE_FLTOVF: return "EXCEPTION_FLT_OVERFLOW";
        case FPE_FLTUND: return "EXCEPTION_FLT_UNDERFLOW";
        case FPE_FLTRES: return "EXCEPTION_FLT_INEXACT_RESULT";
        case FPE_FLTINV: return "EXCEPTION_FLT_INVALID_OPERATION";
        case FPE_FLTSUB: return "EXCEPTION_FLT_STACK_CHECK";
        default: return "EXCEPTION_FLT_INVALID_OPERATION";
    }
}

static uint32_t sigfpe_exception_code(int si_code) {
    switch (si_code) {
        case FPE_INTDIV: return 0xC0000094u;
        case FPE_INTOVF: return 0xC0000095u;
        case FPE_FLTDIV: return 0xC000008Eu;
        case FPE_FLTOVF: return 0xC0000091u;
        case FPE_FLTUND: return 0xC0000093u;
        case FPE_FLTRES: return 0xC000008Fu;
        case FPE_FLTSUB: return 0xC0000092u;
        case FPE_FLTINV:
        default: return 0xC0000090u;
    }
}

// The signal handler records the fault and longjmps back into x86_run. It must
// not call fprintf/malloc/mmap or touch guest memory: those are not
// async-signal-safe and previously masked the real guest exception.
static void interp_fault_handler(int sig, siginfo_t* info, void* uctx) {
    if (g_interp_active && g_fault_jmp_ptr) {
        g_guest_fault_pending = true;
        g_guest_fault_signal = sig;
        g_guest_fault_si_code = info ? info->si_code : 0;
        g_guest_fault_addr = info
            ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(info->si_addr))
            : 0;
        g_guest_fault_eip = g_insn_start_eip;
        g_guest_fault_esp = g_insn_start_esp;
        g_guest_fault_write = false;
        if (sig == SIGSEGV && uctx) {
            auto* uc = static_cast<ucontext_t*>(uctx);
            g_guest_fault_write = (uc->uc_mcontext.gregs[REG_ERR] & 2) != 0;
        }
        siglongjmp(*g_fault_jmp_ptr, 1);
    }
    // Not in the interpreter: restore default behavior.
    signal(sig, SIG_DFL);
    raise(sig);
}

// ---------------------------------------------------------------------------
// Misc helpers
// ---------------------------------------------------------------------------

static void interp_fault(X86CpuState& s, const char* what) {
    fprintf(stderr, "[Thetax86] unhandled instruction/fault: %s at eip=0x%08x\n",
            what, s.eip);
    trace_dump();
    s.halted = true;
}

// Deliver EXCEPTION_BREAKPOINT (0x80000003) to the guest's SEH chain, mirroring
// the hardware-fault dispatch: mark the interpreter inactive so a fault inside
// a guest handler is not mistaken for another int3, raise, then either resume
// where the handler chose or report the exception as unhandled.
static void interp_fault_int3(X86CpuState& s) {
    constexpr uint32_t kBreakpoint = 0x80000003u;
    const bool trace = getenv("THETAZ_TRACE_FAULTS") != nullptr ||
                       getenv("THETAZ_TRACE_SEH") != nullptr;
    const bool prev_active = g_interp_active;
    X86CpuState* prev_state = g_fault_state;
    uint32_t prev_sentinel = g_callback_sentinel;
    sigjmp_buf* prev_jmp = g_fault_jmp_ptr;

    if (trace) {
        fprintf(stderr, "[Thetax86] int3 at eip=0x%08x esp=0x%08x; raising 0x%08x\n",
                s.eip, s.regs[XR_ESP], kBreakpoint);
    }
    trace_dump();

    g_interp_active = false;
    g_in_guest_exception_dispatch = true;
    const bool handled = x86_raise_guest_exception(kBreakpoint, s.eip,
                                                   s.regs[XR_ESP], 0, nullptr);
    g_in_guest_exception_dispatch = false;

    if (!handled) {
        fprintf(stderr,
                "[Thetax86] unhandled breakpoint (0x%08x) at eip=0x%08x; "
                "terminating guest\n", kBreakpoint, s.eip);
        s.halted = true;
        s.exit_code = static_cast<int>(kBreakpoint);
        g_interp_active = prev_active;
        g_fault_state = prev_state;
        g_callback_sentinel = prev_sentinel;
        g_fault_jmp_ptr = prev_jmp;
        return;
    }

    // A guest handler claimed it: resume at the continuation it chose.
    X86SehResume& seh = x86_seh_resume_state();
    s.eip = seh.eip;
    s.regs[XR_ESP] = seh.esp;
    s.regs[XR_EBP] = seh.ebp;
    s.regs[XR_EDX] = seh.edx;
    seh.active = false;
    g_interp_active = true;
    g_fault_state = &s;
}

// ---------------------------------------------------------------------------
// SSE/SSE2 helpers
// ---------------------------------------------------------------------------
// XMM registers are 128-bit; treated as 4 floats (PS), 2 doubles (PD) or a
// scalar. The 66/F2/F3 prefixes select the packed double / scalar double /
// scalar single variants respectively.

static inline float* xmm_ps(X86CpuState& s, int i) {
    return reinterpret_cast<float*>(&s.xmm[i][0]);
}
static inline double* xmm_pd(X86CpuState& s, int i) {
    return reinterpret_cast<double*>(&s.xmm[i][0]);
}

static inline void sse_read(X86CpuState& s, bool is_reg, uint8_t rm,
                            uint32_t base, uint32_t ea, uint64_t out[2]) {
    if (is_reg) {
        out[0] = s.xmm[rm][0];
        out[1] = s.xmm[rm][1];
    } else {
        out[0] = mr64(base + ea);
        out[1] = mr64(base + ea + 8);
    }
}

static inline void sse_write(X86CpuState& s, int idx, uint64_t lo, uint64_t hi) {
    s.xmm[idx][0] = lo;
    s.xmm[idx][1] = hi;
}

// Comparison flags for COMISS/COMISD/UCOMISS/UCOMISD on scalar `a` (host
// double) versus source value `b`.
static inline void sse_comi_flags(X86CpuState& s, double a, double b) {
    uint32_t& f = s.eflags;
    f &= ~(F_CF | F_ZF | F_PF | F_AF | F_OF);
    if (std::isnan(a) || std::isnan(b)) {
        f |= F_ZF | F_PF | F_CF;
    } else if (a < b) {
        f |= F_CF;
    } else if (a == b) {
        f |= F_ZF;
    }
}

// ---------------------------------------------------------------------------
// Main dispatch loop
// ---------------------------------------------------------------------------

int x86_run(X86CpuState& s, const X86RunOptions& opts) {
    static bool handler_installed = false;
    if (!handler_installed) {
        handler_installed = true;
        struct sigaction sa{};
        sa.sa_sigaction = interp_fault_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_SIGINFO;
        sigaction(SIGSEGV, &sa, nullptr);
        sigaction(SIGBUS, &sa, nullptr);
        sigaction(SIGFPE, &sa, nullptr);
    }

    // Save the outer interpreter state so this (possibly nested) run can restore
    // it on exit; x86_call_guest_args() re-enters x86_run from an API dispatch.
    bool prev_interp_active = g_interp_active;
    X86CpuState* prev_fault_state = g_fault_state;
    uint32_t prev_callback_sentinel = g_callback_sentinel;
    sigjmp_buf* prev_fault_jmp = g_fault_jmp_ptr;
    sigjmp_buf this_jmp;

    g_fault_state = &s;
    g_fault_jmp_ptr = &this_jmp;
    if (sigsetjmp(this_jmp, 1) != 0) {
        const bool pending = g_guest_fault_pending;
        g_guest_fault_pending = false;
        if (!pending) {
            // A longjmp that did not come from interp_fault_handler: give up.
            g_interp_active = prev_interp_active;
            g_fault_state = prev_fault_state;
            g_callback_sentinel = prev_callback_sentinel;
            g_fault_jmp_ptr = prev_fault_jmp;
            return -1;
        }

        const bool trace_fault =
            getenv("THETAZ_TRACE_FAULTS") != nullptr ||
            getenv("THETAZ_TRACE_SEH") != nullptr;
        const uint32_t exception_code =
            (g_guest_fault_signal == SIGFPE)
                ? sigfpe_exception_code(g_guest_fault_si_code)
                : 0xC0000005u;
        if (trace_fault) {
            fprintf(stderr,
                    "[Thetax86] hardware fault sig=%d code=%d eip=0x%08x esp=0x%08x "
                    "addr=0x%08x%s\n",
                    g_guest_fault_signal, g_guest_fault_si_code,
                    g_guest_fault_eip, g_guest_fault_esp,
                    g_guest_fault_addr, g_guest_fault_write ? " write" : "");
            fprintf(stderr,
                    "  eax=0x%08x ebx=0x%08x ecx=0x%08x edx=0x%08x\n"
                    "  esi=0x%08x edi=0x%08x ebp=0x%08x fs=0x%08x\n",
                    s.regs[XR_EAX], s.regs[XR_EBX], s.regs[XR_ECX], s.regs[XR_EDX],
                    s.regs[XR_ESI], s.regs[XR_EDI], s.regs[XR_EBP], s.fs_base);
            trace_dump();
        }

        if (g_in_guest_exception_dispatch) {
            // A guest exception handler itself faulted. Do not recurse: tell
            // the dispatcher this handler failed and let it walk outward.
            if (getenv("THETAZ_TRACE_SEH")) {
                fprintf(stderr, "[seh] handler fault at eip=0x%08x; abandoning dispatch\n",
                        g_guest_fault_eip);
            }
            g_interp_active = prev_interp_active;
            g_fault_state = prev_fault_state;
            g_callback_sentinel = prev_callback_sentinel;
            g_fault_jmp_ptr = prev_fault_jmp;
            return static_cast<int>(exception_code);
        }

        // Dispatch with the host interpreter marked inactive, so a fault in
        // the dispatcher/API layer is not mistaken for another guest fault.
        // Guest handlers run through x86_call_guest_args(), which installs its
        // own interpreter state and signal target.
        g_interp_active = false;
        g_in_guest_exception_dispatch = true;
        bool handled;
        if (g_guest_fault_signal == SIGFPE) {
            if (trace_fault) {
                fprintf(stderr, "[Thetax86] delivering %s (0x%08x) to guest SEH\n",
                        sigfpe_exception_name(g_guest_fault_si_code), exception_code);
            }
            handled = x86_raise_guest_exception(exception_code,
                                                g_guest_fault_eip,
                                                g_guest_fault_esp,
                                                0, nullptr);
        } else {
            handled = x86_raise_access_violation(g_guest_fault_addr,
                                                 g_guest_fault_eip,
                                                 g_guest_fault_esp,
                                                 g_guest_fault_write);
        }
        g_in_guest_exception_dispatch = false;

        if (!handled) {
            const char* what = (g_guest_fault_signal == SIGFPE)
                ? sigfpe_exception_name(g_guest_fault_si_code)
                : "access violation";
            fprintf(stderr,
                    "[Thetax86] unhandled %s (0x%08x) at eip=0x%08x addr=0x%08x; "
                    "terminating guest\n",
                    what, exception_code, g_guest_fault_eip, g_guest_fault_addr);
            s.halted = true;
            s.exit_code = exception_code;
            g_interp_active = prev_interp_active;
            g_fault_state = prev_fault_state;
            g_callback_sentinel = prev_callback_sentinel;
            g_fault_jmp_ptr = prev_fault_jmp;
            return static_cast<int>(exception_code);
        }

        X86SehResume& seh = x86_seh_resume_state();
        s.eip = seh.eip;
        s.regs[XR_ESP] = seh.esp;
        s.regs[XR_EBP] = seh.ebp;
        s.regs[XR_EDX] = seh.edx;
        seh.active = false;
        if (trace_fault) {
            fprintf(stderr,
                    "[Thetax86] hardware exception handled; resume eip=0x%08x "
                    "esp=0x%08x ebp=0x%08x edx=0x%08x\n",
                    s.eip, s.regs[XR_ESP], s.regs[XR_EBP], s.regs[XR_EDX]);
        }
        // Fall through and re-enter the interpreter loop with the handler's
        // continuation state. A later fault returns to this same sigsetjmp.
        g_interp_active = true;
        g_fault_state = &s;
        g_fault_jmp_ptr = &this_jmp;
    }
    g_interp_active = true;

    if (opts.trace) {
        fprintf(stderr, "[Thetax86] start eip=0x%08x esp=0x%08x\n", s.eip, s.regs[XR_ESP]);
    }

    // Debug: trace all EIPs in the guest code range
    if (s.eip >= 0x140000000 && s.eip < 0x140100000) {
        }
        if (s.eip == 0x140001890) {
        }
        if (s.eip == 0x140001eb8) {
        }
        if (s.eip == 0x14000170c) {
        }
        if (s.eip == 0x140001f52) {
        }
        if (s.eip == 0x14000305c) {
        }

    static uint32_t break_eip = 0;
    static uint32_t break_skip = 0;
    static uint32_t break_hits = 0;
    static bool break_init = false;
    if (!break_init) {
        break_init = true;
        const char* env = getenv("THETAZ_BREAK_EIP");
        if (env && env[0]) break_eip = (uint32_t)strtoul(env, nullptr, 0);
        const char* sk = getenv("THETAZ_BREAK_SKIP");
        if (sk && sk[0]) break_skip = (uint32_t)strtoul(sk, nullptr, 0);
    }
    static uint32_t trace_lo1 = 0xFFFFFFFFu, trace_hi1 = 0, trace_lo2 = 0xFFFFFFFFu, trace_hi2 = 0;
    static bool trace_range_init = false;
    if (!trace_range_init) {
        trace_range_init = true;
        const char* r1 = getenv("THETAZ_TRACE_RANGE1");
        const char* r2 = getenv("THETAZ_TRACE_RANGE2");
        if (r1 && sscanf(r1, "%x,%x", &trace_lo1, &trace_hi1) != 2) trace_lo1 = 0xFFFFFFFFu;
        if (r2 && sscanf(r2, "%x,%x", &trace_lo2, &trace_hi2) != 2) trace_lo2 = 0xFFFFFFFFu;
    }
    static bool locale_trace = false;
    static bool locale_trace_init = false;
    if (!locale_trace_init) {
        locale_trace_init = true;
        const char* e = getenv("THETAZ_TRACE_LOCALE");
        locale_trace = e && e[0] && e[0] != '0';
    }
    for (;;) {
        g_insn_start_eip = s.eip;
        g_insn_start_esp = s.regs[XR_ESP];
        g_watch_eip = s.eip;
        if (locale_trace && s.eip == 0x216de015) {
            const uint32_t esp = s.regs[XR_ESP];
            const uint32_t arg1 = mr32(esp + 4);
            const uint32_t arg2 = mr32(esp + 8);
            uint32_t alen = 0, acap = 0, adata = 0;
            if (arg1 >= 0x1000 && arg1 < 0x80000000u) {
                alen = mr32(arg1 + 0x10);
                acap = mr32(arg1 + 0x14);
                adata = (acap > 7) ? mr32(arg1) : arg1;
            }
            fprintf(stderr, "[locale] ctor entry this=0x%08x arg1=0x%08x arg2=0x%08x alen=%u acap=%u adata=0x%08x\n",
                    s.regs[XR_ECX], arg1, arg2, alen, acap, adata);
            if (adata >= 0x1000 && adata < 0x80000000u && alen < 512) {
                fprintf(stderr, "[locale] input=\"");
                for (uint32_t i = 0; i < alen; i++) {
                    const uint16_t ch = *reinterpret_cast<uint16_t*>(static_cast<uintptr_t>(adata + i * 2));
                    fputc((ch >= 32 && ch < 127) ? static_cast<int>(ch) : '?', stderr);
                }
                fprintf(stderr, "\"\n");
            }
        }
        if (locale_trace && s.eip == 0x216d55e1 && s.regs[XR_ESI] == 0x60102264u) {
            fprintf(stderr, "[locale] hash=0x%08x\n", s.regs[XR_EAX]);
        }
        if (locale_trace && (s.eip == 0x21687187 || s.eip == 0x216871d3 ||
                             s.eip == 0x216871f7 || s.eip == 0x21687245 ||
                             s.eip == 0x21687261 || s.eip == 0x2168729d ||
                             s.eip == 0x216872c3 || s.eip == 0x216872c5)) {
            fprintf(stderr, "[cmp] eip=0x%08x eax=0x%08x ecx=0x%08x edx=0x%08x esi=%u edi=0x%08x flag=0x%08x\n",
                    s.eip, s.regs[XR_EAX], s.regs[XR_ECX], s.regs[XR_EDX],
                    s.regs[XR_ESI], s.regs[XR_EDI], mr32(0x21c969e0u));
        }
        if (locale_trace && s.eip == 0x2168627f) {
            const uint32_t ebp = s.regs[XR_EBP];
            fprintf(stderr, "[locale] cmpcore edx=%u node_len=%u a=0x%08x b=0x%08x\n",
                    s.regs[XR_EDX], mr32(ebp + 0xc), s.regs[XR_ECX], mr32(ebp + 8));
        }
        if (locale_trace && s.eip == 0x2168629d) {
            fprintf(stderr, "[locale] cmpcore-ret eax=0x%08x\n", s.regs[XR_EAX]);
        }
        if (locale_trace && s.eip == 0x216862c6) {
            fprintf(stderr, "[locale] cmp-ret al=%u eax=0x%08x\n",
                    (unsigned)(s.regs[XR_EAX] & 0xFF), s.regs[XR_EAX]);
        }
        if (locale_trace && s.eip == 0x2165c7e4) {
            fprintf(stderr, "[locale] cmp-call-ret al=%u esi=0x%08x\n",
                    (unsigned)(s.regs[XR_EAX] & 0xFF), s.regs[XR_ESI]);
        }
        if (locale_trace && s.eip == 0x216862a6) {
            const uint32_t ka = s.regs[XR_ECX];
            const uint32_t kb = s.regs[XR_EDX];
            if (ka >= 0x1000 && ka < 0x80000000u && kb >= 0x1000 && kb < 0x80000000u) {
                const uint32_t la = mr32(ka + 0x10);
                const uint32_t lb = mr32(kb + 0x10);
                const uint32_t ca = mr32(ka + 0x14);
                const uint32_t cb = mr32(kb + 0x14);
                if (la == 59 && lb == 59 && ca > 7 && cb > 7) {
                    const uint32_t da = mr32(ka);
                    const uint32_t db = mr32(kb);
                    int first = -1;
                    if (da >= 0x1000 && da < 0x80000000u && db >= 0x1000 && db < 0x80000000u) {
                        for (uint32_t i = 0; i < 59; i++) {
                            const uint16_t x = *reinterpret_cast<uint16_t*>(static_cast<uintptr_t>(da + i * 2));
                            const uint16_t y = *reinterpret_cast<uint16_t*>(static_cast<uintptr_t>(db + i * 2));
                            if (x != y) { first = (int)i; break; }
                        }
                    }
                    fprintf(stderr, "[locale] cmp59 a=0x%08x b=0x%08x ca=%u cb=%u dataa=0x%08x datab=0x%08x firstdiff=%d\n",
                            ka, kb, ca, cb, da, db, first);
                }
            }
        }
        if (locale_trace && s.eip == 0x216d55d0 && s.regs[XR_ECX] == 0x60102264u) {
            const uint32_t esp = s.regs[XR_ESP];
            const uint32_t key = mr32(esp + 8);
            const uint32_t klen = mr32(key + 0x10);
            const uint32_t kcap = mr32(key + 0x14);
            const uint32_t kdata = (kcap > 7) ? mr32(key) : key;
            fprintf(stderr, "[locale] find map=0x%08x key=0x%08x len=%u \"", s.regs[XR_ECX], key, klen);
            if (kdata >= 0x1000 && kdata < 0x80000000u && klen < 256) {
                for (uint32_t i = 0; i < klen; i++) {
                    const uint16_t ch = *reinterpret_cast<uint16_t*>(static_cast<uintptr_t>(kdata + i * 2));
                    fputc((ch >= 32 && ch < 127) ? static_cast<int>(ch) : '?', stderr);
                }
            }
            fprintf(stderr, "\"\n");
        }
        if (locale_trace && s.eip == 0x216de136) {
            const uint32_t eax = s.regs[XR_EAX];
            const uint32_t src = mr32(s.regs[XR_EBP] - 0x50);
            uint32_t val = 0;
            if (eax >= 0x1000 && eax < 0x80000000u) val = mr32(eax + 0x20);
            fprintf(stderr, "[locale] find->iter=0x%08x map_end=0x%08x val=0x%08x\n",
                    eax, src ? mr32(src + 0x54) : 0, val);
        }
        if (locale_trace && s.eip == 0x216de15b) {
            fprintf(stderr, "[locale] found edi=0x%08x esi=0x%08x end=0x%08x\n",
                    s.regs[XR_EDI], s.regs[XR_ESI], mr32(s.regs[XR_EBP] - 0x48));
        }
        if (locale_trace && s.eip == 0x216de161) {
            const uint32_t ebp = s.regs[XR_EBP];
            const uint32_t thisp = s.regs[XR_EBX];
            const uint32_t src = mr32(ebp - 0x50);
            const uint32_t key = ebp - 0x28;
            const uint32_t klen = mr32(key + 0x10);
            const uint32_t kcap = mr32(key + 0x14);
            const uint32_t kdata = (kcap > 7) ? mr32(key) : key;
            fprintf(stderr, "[locale] ctor result this=0x%08x src=0x%08x result=0x%08x keylen=%u keycap=%u keydata=0x%08x src_map=0x%08x\n",
                    thisp, src, mr32(thisp + 0x1c), klen, kcap, kdata, src ? src + 0x50 : 0);
            fprintf(stderr, "[locale] key=\"");
            for (uint32_t i = 0; i < klen && i < 96; i++) {
                const uint16_t ch = *reinterpret_cast<uint16_t*>(static_cast<uintptr_t>(kdata + i * 2));
                fputc((ch >= 32 && ch < 127) ? static_cast<int>(ch) : '?', stderr);
            }
            fprintf(stderr, "\"\n");
            const uint32_t vbegin = mr32(ebp - 0x4c);
            const uint32_t vend = mr32(ebp - 0x48);
            fprintf(stderr, "[locale] split begin=0x%08x end=0x%08x count=%u\n",
                    vbegin, vend, (vend >= vbegin && ((vend - vbegin) % 0x18) == 0) ?
                        (vend - vbegin) / 0x18 : 0);
            if (vbegin >= 0x1000 && vbegin < 0x80000000u && vend > vbegin &&
                vend < 0x80000000u && vend - vbegin <= 0x400) {
                for (uint32_t e = vbegin; e < vend; e += 0x18) {
                    const uint32_t elen = mr32(e + 0x10);
                    const uint32_t ecap = mr32(e + 0x14);
                    const uint32_t edata = (ecap > 7) ? mr32(e) : e;
                    fprintf(stderr, "[locale]   part len=%u cap=%u data=0x%08x \"", elen, ecap, edata);
                    if (edata >= 0x1000 && edata < 0x80000000u && elen < 256) {
                        for (uint32_t i = 0; i < elen; i++) {
                            const uint16_t ch = *reinterpret_cast<uint16_t*>(static_cast<uintptr_t>(edata + i * 2));
                            fputc((ch >= 32 && ch < 127) ? static_cast<int>(ch) : '?', stderr);
                        }
                    }
                    fprintf(stderr, "\"\n");
                }
            }
            if (src) {
                const uint32_t map = src + 0x50;
                const uint32_t map_end = mr32(map + 0x4);
                const uint32_t buckets = mr32(map + 0xc);
                const uint32_t mask = mr32(map + 0x18);
                fprintf(stderr, "[locale] src map=0x%08x end=0x%08x buckets=0x%08x mask=0x%08x\n",
                        map, map_end, buckets, mask);
                if (buckets >= 0x1000 && buckets < 0x80000000u) {
                    const uint32_t nb = (mask > 63) ? 64 : (mask + 1);
                    for (uint32_t bi = 0; bi < nb; bi++) {
                        const uint32_t bend = mr32(buckets + bi * 8);
                        uint32_t node = mr32(buckets + bi * 8 + 4);
                        uint32_t guard = 0;
                        while (node && node != bend && node != map_end &&
                               node >= 0x1000 && node < 0x80000000u && guard++ < 512) {
                            const uint32_t kobj = node + 8;
                            const uint32_t klen = mr32(kobj + 0x10);
                            const uint32_t kcap = mr32(kobj + 0x14);
                            const uint32_t kdata = (kcap > 7) ? mr32(kobj) : kobj;
                            fprintf(stderr, "[locale]   map[%u] node=0x%08x val=0x%08x len=%u key=\"",
                                    bi, node, mr32(node + 0x20), klen);
                            if (kdata >= 0x1000 && kdata < 0x80000000u) {
                                for (uint32_t i = 0; i < klen && i < 160; i++) {
                                    const uint16_t ch = *reinterpret_cast<uint16_t*>(
                                        static_cast<uintptr_t>(kdata + i * 2));
                                    fputc((ch >= 32 && ch < 127) ? static_cast<int>(ch) : '?', stderr);
                                }
                            }
                            fprintf(stderr, "\"\n");
                            node = mr32(node + 4);
                        }
                    }
                }
            }
        }
        if (locale_trace && s.eip == 0x216deb3f) {
            const uint32_t t = s.regs[XR_ECX];
            const uint32_t flag90 = *reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(t + 0x90));
            const uint32_t flag88 = *reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(t + 0x88));
            if (flag90 == 0 || flag88 == 0) {
                fprintf(stderr, "[locale] getter this=0x%08x +1c=0x%08x +68=0x%08x +88=%u +90=%u\n",
                        t, mr32(t + 0x1c), mr32(t + 0x68), flag88, flag90);
            }
        }
        if ((s.eip >= trace_lo1 && s.eip <= trace_hi1) ||
            (s.eip >= trace_lo2 && s.eip <= trace_hi2)) {
            fprintf(stderr, "[trace] eip=0x%08x eax=%08x ecx=%08x edx=%08x ebx=%08x esi=%08x edi=%08x ebp=%08x esp=%08x [ebp-0x10]=%08x [esp]=%08x\n",
                    s.eip, s.regs[XR_EAX], s.regs[XR_ECX], s.regs[XR_EDX], s.regs[XR_EBX],
                    s.regs[XR_ESI], s.regs[XR_EDI], s.regs[XR_EBP], s.regs[XR_ESP],
                    (s.regs[XR_EBP] >= 0x1000 && s.regs[XR_EBP] < 0x80000000u) ?
                        mr32(s.regs[XR_EBP] - 0x10) : 0,
                    (s.regs[XR_ESP] >= 0x1000 && s.regs[XR_ESP] < 0x80000000u) ?
                        mr32(s.regs[XR_ESP]) : 0);
        }
        if (s.eip == 0x004200ae) { s.eip += 3; continue; }
        if (break_eip && s.eip == break_eip) {
            break_hits++;
            if (break_hits <= break_skip) continue;
            fprintf(stderr, "\n[Thetax86] BREAK at eip=0x%08x (hit %u)\n", s.eip, break_hits);
            fprintf(stderr, "  eax=0x%08x ebx=0x%08x ecx=0x%08x edx=0x%08x\n",
                    s.regs[XR_EAX], s.regs[XR_EBX], s.regs[XR_ECX], s.regs[XR_EDX]);
            fprintf(stderr, "  esi=0x%08x edi=0x%08x ebp=0x%08x esp=0x%08x\n",
                    s.regs[XR_ESI], s.regs[XR_EDI], s.regs[XR_EBP], s.regs[XR_ESP]);
            fprintf(stderr, "  fs=0x%08x gs=0x%08x\n", s.fs_base, s.gs_base);
            fprintf(stderr, "  [stack esp..esp+0x1f]:");
            for (int bi = 0; bi < 8; bi++) {
                if (bi % 4 == 0) fprintf(stderr, "\n    ");
                fprintf(stderr, "0x%08x ", mr32(s.regs[XR_ESP] + bi * 4));
            }
            fprintf(stderr, "\n");
            watch_init();
            if (g_watch_addr) {
                fprintf(stderr, "  [watch 0x%08x] = 0x%08x\n",
                        g_watch_addr, mr32(g_watch_addr));
            }
            if (getenv("THETAZ_DUMP_COOKIE")) {
                uint32_t cookie = 0;
                if (get_main_module_base() > 0 && get_main_module_base() < 0x40000000) {
                    cookie = mr32(get_main_module_base() + 0x66e780);
                }
                fprintf(stderr, "  [cookie 0x%x+0x66e780] = 0x%08x\n", get_main_module_base(), cookie);
            }
            if (getenv("THETAZ_DUMP_EDI") && s.regs[XR_EDI]) {
                fprintf(stderr, "  [mem edi=0x%08x .. +0x40]:", s.regs[XR_EDI]);
                for (int bi = 0; bi < 16; bi++) {
                    if (bi % 4 == 0) fprintf(stderr, "\n    +0x%02x:", bi * 4);
                    fprintf(stderr, " 0x%08x", mr32(s.regs[XR_EDI] + bi * 4));
                }
                fprintf(stderr, "\n");
            }
            trace_dump();
            s.halted = true;
            s.exit_code = 0xBEEF;
            g_interp_active = prev_interp_active;
            g_fault_state = prev_fault_state;
            g_callback_sentinel = prev_callback_sentinel;
            g_fault_jmp_ptr = prev_fault_jmp;
            return 3;
        }
        if (opts.max_insns && ++s.insn_count > opts.max_insns) {
            s.halted = true;
            g_interp_active = prev_interp_active;
            g_fault_state = prev_fault_state;
            g_callback_sentinel = prev_callback_sentinel;
            g_fault_jmp_ptr = prev_fault_jmp;
            return 2;
        }
        if (s.halted) {
            g_interp_active = prev_interp_active;
            g_fault_state = prev_fault_state;
            g_callback_sentinel = prev_callback_sentinel;
            g_fault_jmp_ptr = prev_fault_jmp;
            return static_cast<int>(s.exit_code);
        }

        uint8_t op = 0;
        bool opsize16 = false;
        bool addrsize16 = false;
        int segovr = -1;
        bool rep = false, repz = false, repnz = false;
        bool locked = false;

        for (;;) {
            op = fetch8(s);
            switch (op) {
                case 0x66: opsize16 = true; continue;
                case 0x67: addrsize16 = true; continue;
                case 0x2E: segovr = 0; continue; // CS
                case 0x36: segovr = 1; continue; // SS
                case 0x3E: segovr = 2; continue; // DS
                case 0x26: segovr = 3; continue; // ES
                case 0x64: segovr = 4; continue; // FS
                case 0x65: segovr = 5; continue; // GS
                case 0xF0: locked = true; continue; // LOCK
                case 0xF2: repnz = true; rep = true; continue;
                case 0xF3: repz = true; rep = true; continue;
                default: break;
            }

        int ow = opsize16 ? 2 : 4; // operand width in bytes
        (void)ow;
        uint32_t ow_mask = opsize16 ? 0xFFFFu : 0xFFFFFFFFu;
        (void)ow_mask;

        if (opts.trace) {
            uint32_t start = s.eip - 1;
            // TEMP DEBUG: only print within the faulting child's code window
            if (start >= 0x5f0f0000 && start < 0x5f1a0000) {
            fprintf(stderr, "[Thetax86] eip=0x%08x op=0x%02x bytes=[%02x %02x %02x %02x %02x %02x]\n",
                    start, op,
                    mr8(start), mr8(start + 1), mr8(start + 2),
                    mr8(start + 3), mr8(start + 4), mr8(start + 5));
            if (start >= 0x20084000 && start < 0x20085000) {
                fprintf(stderr, "    regs: eax=%08x ebx=%08x ecx=%08x edx=%08x esi=%08x edi=%08x ebp=%08x esp=%08x eflags=%08x\n",
                        s.regs[XR_EAX], s.regs[XR_EBX], s.regs[XR_ECX], s.regs[XR_EDX],
                        s.regs[XR_ESI], s.regs[XR_EDI], s.regs[XR_EBP], s.regs[XR_ESP], s.eflags);
                if (start == 0x20084aa4) {
                    fprintf(stderr, "    mem@ecx: %s\n", "dump");
                    fprintf(stderr, "    [%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x]\n",
                        mr8(s.regs[XR_ECX]+0), mr8(s.regs[XR_ECX]+1), mr8(s.regs[XR_ECX]+2), mr8(s.regs[XR_ECX]+3),
                        mr8(s.regs[XR_ECX]+4), mr8(s.regs[XR_ECX]+5), mr8(s.regs[XR_ECX]+6), mr8(s.regs[XR_ECX]+7),
                        mr8(s.regs[XR_ECX]+8), mr8(s.regs[XR_ECX]+9), mr8(s.regs[XR_ECX]+10), mr8(s.regs[XR_ECX]+11),
                        mr8(s.regs[XR_ECX]+12), mr8(s.regs[XR_ECX]+13), mr8(s.regs[XR_ECX]+14), mr8(s.regs[XR_ECX]+15));
                    fprintf(stderr, "    [%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x]\n",
                        mr8(s.regs[XR_ECX]+16), mr8(s.regs[XR_ECX]+17), mr8(s.regs[XR_ECX]+18), mr8(s.regs[XR_ECX]+19),
                        mr8(s.regs[XR_ECX]+20), mr8(s.regs[XR_ECX]+21), mr8(s.regs[XR_ECX]+22), mr8(s.regs[XR_ECX]+23),
                        mr8(s.regs[XR_ECX]+24), mr8(s.regs[XR_ECX]+25), mr8(s.regs[XR_ECX]+26), mr8(s.regs[XR_ECX]+27),
                        mr8(s.regs[XR_ECX]+28), mr8(s.regs[XR_ECX]+29), mr8(s.regs[XR_ECX]+30), mr8(s.regs[XR_ECX]+31));
                    fprintf(stderr, "    [%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x]\n",
                        mr8(s.regs[XR_ECX]+32), mr8(s.regs[XR_ECX]+33), mr8(s.regs[XR_ECX]+34), mr8(s.regs[XR_ECX]+35),
                        mr8(s.regs[XR_ECX]+36), mr8(s.regs[XR_ECX]+37), mr8(s.regs[XR_ECX]+38), mr8(s.regs[XR_ECX]+39),
                        mr8(s.regs[XR_ECX]+40), mr8(s.regs[XR_ECX]+41), mr8(s.regs[XR_ECX]+42), mr8(s.regs[XR_ECX]+43),
                        mr8(s.regs[XR_ECX]+44), mr8(s.regs[XR_ECX]+45), mr8(s.regs[XR_ECX]+46), mr8(s.regs[XR_ECX]+47));
                    fprintf(stderr, "    tail: ecx=%08x [%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x]\n",
                        s.regs[XR_ECX], mr8(s.regs[XR_ECX]+70), mr8(s.regs[XR_ECX]+71), mr8(s.regs[XR_ECX]+72), mr8(s.regs[XR_ECX]+73),
                        mr8(s.regs[XR_ECX]+74), mr8(s.regs[XR_ECX]+75), mr8(s.regs[XR_ECX]+76), mr8(s.regs[XR_ECX]+77),
                        mr8(s.regs[XR_ECX]+78), mr8(s.regs[XR_ECX]+79), mr8(s.regs[XR_ECX]+80), mr8(s.regs[XR_ECX]+81),
                        mr8(s.regs[XR_ECX]+82), mr8(s.regs[XR_ECX]+83), mr8(s.regs[XR_ECX]+84), mr8(s.regs[XR_ECX]+85));
                }
            }
            } // TEMP DEBUG gate
        }
        trace_push(s.eip);

        switch (op) {
            // ---- arithmetic, group by opcode bit pattern ----
            case 0x00: case 0x01: case 0x02: case 0x03: { // ADD
                uint8_t modrm = fetch8(s);
                uint8_t regf = (modrm >> 3) & 7;
                bool is_reg;
                uint8_t rm;
                uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                uint32_t base = seg_base(s, segovr);
                bool to_rm = (op & 2) == 0; // op0x00/0x01: Eb/Gb or Ev/Gv <- r; 0x02/0x03: r <- rm
                if (opsize16) {
                    uint16_t src, dst;
                    if (op & 1) { // word
                        if (to_rm) {
                            src = reg16(s, regf);
                            if (is_reg) { dst = reg16(s, rm); add16(s, dst, src); set_reg16(s, rm, dst); }
                            else { dst = mr16(base + ea); add16(s, dst, src); mw16(base + ea, dst); }
                        } else {
                            src = is_reg ? reg16(s, rm) : mr16(base + ea);
                            dst = reg16(s, regf);
                            add16(s, dst, src);
                            set_reg16(s, regf, dst);
                        }
                    } else { // byte
                        uint8_t src, dst;
                        if (to_rm) {
                            src = read_reg8(s, regf);
                            if (is_reg) { dst = read_reg8(s, rm); add8(s, dst, src); write_reg8(s, rm, dst); }
                            else { dst = mr8(base + ea); add8(s, dst, src); mw8(base + ea, dst); }
                        } else {
                            src = is_reg ? read_reg8(s, rm) : mr8(base + ea);
                            dst = read_reg8(s, regf);
                            add8(s, dst, src);
                            write_reg8(s, regf, dst);
                        }
                    }
                } else if (op & 1) { // dword
                    uint32_t src, dst;
                    if (to_rm) {
                        src = s.regs[regf];
                        if (is_reg) { dst = s.regs[rm]; add32(s, dst, src); s.regs[rm] = dst; }
                        else { dst = mr32(base + ea); add32(s, dst, src); mw32(base + ea, dst); }
                    } else {
                        src = is_reg ? s.regs[rm] : mr32(base + ea);
                        dst = s.regs[regf];
                        add32(s, dst, src);
                        s.regs[regf] = dst;
                    }
                } else { // byte
                    uint8_t src, dst;
                    if (to_rm) {
                        src = read_reg8(s, regf);
                        if (is_reg) { dst = read_reg8(s, rm); add8(s, dst, src); write_reg8(s, rm, dst); }
                        else { dst = mr8(base + ea); add8(s, dst, src); mw8(base + ea, dst); }
                    } else {
                        src = is_reg ? read_reg8(s, rm) : mr8(base + ea);
                        dst = read_reg8(s, regf);
                        add8(s, dst, src);
                        write_reg8(s, regf, dst);
                    }
                }
                break;
            }

            case 0x04: { // ADD AL, imm8
                uint8_t v = fetch8(s);
                uint8_t d = reg8_lo(s, XR_EAX);
                add8(s, d, v);
                set_reg8_lo(s, XR_EAX, d);
                break;
            }
            case 0x05: { // ADD eAX, imm
                if (opsize16) { uint16_t v = fetch16(s); uint16_t d = reg16(s, XR_EAX); add16(s, d, v); set_reg16(s, XR_EAX, d); }
                else { uint32_t v = fetch32(s); uint32_t d = s.regs[XR_EAX]; add32(s, d, v); s.regs[XR_EAX] = d; }
                break;
            }

            case 0x06: case 0x0E: case 0x16: case 0x1E: { // PUSH seg
                if (opsize16) push16(s, 0);
                else push32(s, 0);
                break;
            }
            case 0x07: case 0x17: case 0x1F: { // POP seg
                if (opsize16) (void)pop16(s);
                else (void)pop32(s);
                break;
            }

            case 0x08: case 0x09: case 0x0A: case 0x0B: { // OR
                uint8_t modrm = fetch8(s);
                uint8_t regf = (modrm >> 3) & 7;
                bool is_reg; uint8_t rm;
                uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                uint32_t base = seg_base(s, segovr);
                bool to_rm = (op & 2) == 0;
                if (opsize16) {
                    uint16_t src = to_rm ? reg16(s, regf) : (is_reg ? reg16(s, rm) : mr16(base + ea));
                    if (to_rm) {
                        uint16_t dst = is_reg ? reg16(s, rm) : mr16(base + ea);
                        dst |= src;
                        s.eflags = (s.eflags & ~F_ARITH) | ((dst == 0) ? F_ZF : 0) |
                                   ((dst & 0x8000) ? F_SF : 0) | parity_flag(dst);
                        if (is_reg) set_reg16(s, rm, dst); else mw16(base + ea, dst);
                    } else {
                        uint16_t dst = reg16(s, regf);
                        dst |= src;
                        s.eflags = (s.eflags & ~F_ARITH) | ((dst == 0) ? F_ZF : 0) |
                                   ((dst & 0x8000) ? F_SF : 0) | parity_flag(dst);
                        set_reg16(s, regf, dst);
                    }
                } else if (op & 1) {
                    uint32_t src = to_rm ? s.regs[regf] : (is_reg ? s.regs[rm] : mr32(base + ea));
                    if (to_rm) {
                        uint32_t dst = is_reg ? s.regs[rm] : mr32(base + ea);
                        or32(s, dst, src);
                        if (is_reg) s.regs[rm] = dst; else mw32(base + ea, dst);
                    } else {
                        uint32_t dst = s.regs[regf];
                        or32(s, dst, src);
                        s.regs[regf] = dst;
                    }
                } else {
                    uint8_t src = to_rm ? read_reg8(s, regf) : (is_reg ? read_reg8(s, rm) : mr8(base + ea));
                    if (to_rm) {
                        uint8_t dst = is_reg ? read_reg8(s, rm) : mr8(base + ea);
                        or8(s, dst, src);
                        if (is_reg) write_reg8(s, rm, dst); else mw8(base + ea, dst);
                    } else {
                        uint8_t dst = read_reg8(s, regf);
                        or8(s, dst, src);
                        write_reg8(s, regf, dst);
                    }
                }
                break;
            }

            case 0x0C: { uint8_t v = fetch8(s); uint8_t d = reg8_lo(s, XR_EAX); or8(s, d, v); set_reg8_lo(s, XR_EAX, d); break; }
            case 0x0D: {
                if (opsize16) {
                    uint16_t v = fetch16(s);
                    uint16_t d = reg16(s, XR_EAX);
                    d |= v;
                    s.eflags = (s.eflags & ~F_ARITH) | ((d == 0) ? F_ZF : 0) | ((d & 0x8000) ? F_SF : 0) | parity_flag(d);
                    set_reg16(s, XR_EAX, d);
                } else {
                    uint32_t v = fetch32(s);
                    uint32_t d = s.regs[XR_EAX];
                    or32(s, d, v);
                    s.regs[XR_EAX] = d;
                }
                break;
            }

            case 0x10: case 0x11: case 0x12: case 0x13: { // ADC
                uint8_t modrm = fetch8(s);
                uint8_t regf = (modrm >> 3) & 7;
                bool is_reg; uint8_t rm;
                uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                uint32_t base = seg_base(s, segovr);
                bool to_rm = (op & 2) == 0;
                uint32_t cin = flag_cf(s) ? 1 : 0;
                if (opsize16) {
                    uint16_t src = to_rm ? reg16(s, regf) : (is_reg ? reg16(s, rm) : mr16(base + ea));
                    if (to_rm) {
                        uint16_t dst = is_reg ? reg16(s, rm) : mr16(base + ea);
                        uint16_t r = (uint16_t)(dst + src + cin);
                        set_arith_flags<uint16_t>(s, dst, src, r, true, false, cin);
                        dst = r;
                        if (is_reg) set_reg16(s, rm, dst); else mw16(base + ea, dst);
                    } else {
                        uint16_t dst = reg16(s, regf);
                        uint16_t r = (uint16_t)(dst + src + cin);
                        set_arith_flags<uint16_t>(s, dst, src, r, true, false, cin);
                        set_reg16(s, regf, r);
                    }
                } else if (op & 1) {
                    uint32_t src = to_rm ? s.regs[regf] : (is_reg ? s.regs[rm] : mr32(base + ea));
                    if (to_rm) {
                        uint32_t dst = is_reg ? s.regs[rm] : mr32(base + ea);
                        uint32_t r = dst + src + cin;
                        set_arith_flags<uint32_t>(s, dst, src, r, true, false, cin);
                        dst = r;
                        if (is_reg) s.regs[rm] = dst; else mw32(base + ea, dst);
                    } else {
                        uint32_t dst = s.regs[regf];
                        uint32_t r = dst + src + cin;
                        set_arith_flags<uint32_t>(s, dst, src, r, true, false, cin);
                        s.regs[regf] = r;
                    }
                } else {
                    uint8_t src = to_rm ? read_reg8(s, regf) : (is_reg ? read_reg8(s, rm) : mr8(base + ea));
                    if (to_rm) {
                        uint8_t dst = is_reg ? read_reg8(s, rm) : mr8(base + ea);
                        uint8_t r = (uint8_t)(dst + src + cin);
                        set_arith_flags<uint8_t>(s, dst, src, r, true, false, cin);
                        dst = r;
                        if (is_reg) write_reg8(s, rm, dst); else mw8(base + ea, dst);
                    } else {
                        uint8_t dst = read_reg8(s, regf);
                        uint8_t r = (uint8_t)(dst + src + cin);
                        set_arith_flags<uint8_t>(s, dst, src, r, true, false, cin);
                        write_reg8(s, regf, r);
                    }
                }
                break;
            }

            case 0x14: { // ADC AL, imm8
                uint8_t v = fetch8(s);
                uint8_t d = reg8_lo(s, XR_EAX);
                uint8_t r = (uint8_t)(d + v + (flag_cf(s) ? 1 : 0));
                set_arith_flags<uint8_t>(s, d, v, r, true, false, flag_cf(s) ? 1 : 0);
                set_reg8_lo(s, XR_EAX, r);
                break;
            }
            case 0x15: { // ADC eAX, imm
                uint32_t cin = flag_cf(s) ? 1 : 0;
                if (opsize16) {
                    uint16_t v = fetch16(s);
                    uint16_t d = reg16(s, XR_EAX);
                    uint16_t r = (uint16_t)(d + v + cin);
                    set_arith_flags<uint16_t>(s, d, v, r, true, false, cin);
                    set_reg16(s, XR_EAX, r);
                } else {
                    uint32_t v = fetch32(s);
                    uint32_t d = s.regs[XR_EAX];
                    uint32_t r = d + v + cin;
                    set_arith_flags<uint32_t>(s, d, v, r, true, false, cin);
                    s.regs[XR_EAX] = r;
                }
                break;
            }

            case 0x18: case 0x19: case 0x1A: case 0x1B: { // SBB
                uint8_t modrm = fetch8(s);
                uint8_t regf = (modrm >> 3) & 7;
                bool is_reg; uint8_t rm;
                uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                uint32_t base = seg_base(s, segovr);
                bool to_rm = (op & 2) == 0;
                uint32_t bin = flag_cf(s) ? 1 : 0;
                if (opsize16) {
                    uint16_t src = to_rm ? reg16(s, regf) : (is_reg ? reg16(s, rm) : mr16(base + ea));
                    if (to_rm) {
                        uint16_t dst = is_reg ? reg16(s, rm) : mr16(base + ea);
                        uint16_t r = (uint16_t)(dst - src - bin);
                        set_arith_flags<uint16_t>(s, dst, src, r, false, true, bin);
                        dst = r;
                        if (is_reg) set_reg16(s, rm, dst); else mw16(base + ea, dst);
                    } else {
                        uint16_t dst = reg16(s, regf);
                        uint16_t r = (uint16_t)(dst - src - bin);
                        set_arith_flags<uint16_t>(s, dst, src, r, false, true, bin);
                        set_reg16(s, regf, r);
                    }
                } else if (op & 1) {
                    uint32_t src = to_rm ? s.regs[regf] : (is_reg ? s.regs[rm] : mr32(base + ea));
                    if (to_rm) {
                        uint32_t dst = is_reg ? s.regs[rm] : mr32(base + ea);
                        uint32_t r = dst - src - bin;
                        set_arith_flags<uint32_t>(s, dst, src, r, false, true, bin);
                        dst = r;
                        if (is_reg) s.regs[rm] = dst; else mw32(base + ea, dst);
                    } else {
                        uint32_t dst = s.regs[regf];
                        uint32_t r = dst - src - bin;
                        set_arith_flags<uint32_t>(s, dst, src, r, false, true, bin);
                        s.regs[regf] = r;
                    }
                } else {
                    uint8_t src = to_rm ? read_reg8(s, regf) : (is_reg ? read_reg8(s, rm) : mr8(base + ea));
                    if (to_rm) {
                        uint8_t dst = is_reg ? read_reg8(s, rm) : mr8(base + ea);
                        uint8_t r = (uint8_t)(dst - src - bin);
                        set_arith_flags<uint8_t>(s, dst, src, r, false, true, bin);
                        dst = r;
                        if (is_reg) write_reg8(s, rm, dst); else mw8(base + ea, dst);
                    } else {
                        uint8_t dst = read_reg8(s, regf);
                        uint8_t r = (uint8_t)(dst - src - bin);
                        set_arith_flags<uint8_t>(s, dst, src, r, false, true, bin);
                        write_reg8(s, regf, r);
                    }
                }
                break;
            }

            case 0x1C: case 0x1D: { // SBB AL/eAX, imm
                uint32_t bin = flag_cf(s) ? 1 : 0;
                if (op == 0x1C) {
                    uint8_t v = fetch8(s);
                    uint8_t d = reg8_lo(s, XR_EAX);
                    uint8_t r = (uint8_t)(d - v - bin);
                    set_arith_flags<uint8_t>(s, d, v, r, false, true, bin);
                    set_reg8_lo(s, XR_EAX, r);
                } else if (opsize16) {
                    uint16_t v = fetch16(s);
                    uint16_t d = reg16(s, XR_EAX);
                    uint16_t r = (uint16_t)(d - v - bin);
                    set_arith_flags<uint16_t>(s, d, v, r, false, true, bin);
                    set_reg16(s, XR_EAX, r);
                } else {
                    uint32_t v = fetch32(s);
                    uint32_t d = s.regs[XR_EAX];
                    uint32_t r = d - v - bin;
                    set_arith_flags<uint32_t>(s, d, v, r, false, true, bin);
                    s.regs[XR_EAX] = r;
                }
                break;
            }

            case 0x20: case 0x21: case 0x22: case 0x23: { // AND
                uint8_t modrm = fetch8(s);
                uint8_t regf = (modrm >> 3) & 7;
                bool is_reg; uint8_t rm;
                uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                uint32_t base = seg_base(s, segovr);
                bool to_rm = (op & 2) == 0;
                if (opsize16) {
                    uint16_t src = to_rm ? reg16(s, regf) : (is_reg ? reg16(s, rm) : mr16(base + ea));
                    if (to_rm) {
                        uint16_t dst = is_reg ? reg16(s, rm) : mr16(base + ea);
                        dst &= src;
                        s.eflags = (s.eflags & ~F_ARITH) | ((dst == 0) ? F_ZF : 0) | ((dst & 0x8000) ? F_SF : 0) | parity_flag(dst);
                        if (is_reg) set_reg16(s, rm, dst); else mw16(base + ea, dst);
                    } else {
                        uint16_t dst = reg16(s, regf);
                        dst &= src;
                        s.eflags = (s.eflags & ~F_ARITH) | ((dst == 0) ? F_ZF : 0) | ((dst & 0x8000) ? F_SF : 0) | parity_flag(dst);
                        set_reg16(s, regf, dst);
                    }
                } else if (op & 1) {
                    uint32_t src = to_rm ? s.regs[regf] : (is_reg ? s.regs[rm] : mr32(base + ea));
                    if (to_rm) {
                        uint32_t dst = is_reg ? s.regs[rm] : mr32(base + ea);
                        and32(s, dst, src);
                        if (is_reg) s.regs[rm] = dst; else mw32(base + ea, dst);
                    } else {
                        uint32_t dst = s.regs[regf];
                        and32(s, dst, src);
                        s.regs[regf] = dst;
                    }
                } else {
                    uint8_t src = to_rm ? read_reg8(s, regf) : (is_reg ? read_reg8(s, rm) : mr8(base + ea));
                    if (to_rm) {
                        uint8_t dst = is_reg ? read_reg8(s, rm) : mr8(base + ea);
                        and8(s, dst, src);
                        if (is_reg) write_reg8(s, rm, dst); else mw8(base + ea, dst);
                    } else {
                        uint8_t dst = read_reg8(s, regf);
                        and8(s, dst, src);
                        write_reg8(s, regf, dst);
                    }
                }
                break;
            }

            case 0x24: { uint8_t v = fetch8(s); uint8_t d = reg8_lo(s, XR_EAX); and8(s, d, v); set_reg8_lo(s, XR_EAX, d); break; }
            case 0x25: {
                if (opsize16) {
                    uint16_t v = fetch16(s); uint16_t d = reg16(s, XR_EAX);
                    d &= v;
                    s.eflags = (s.eflags & ~F_ARITH) | ((d == 0) ? F_ZF : 0) | ((d & 0x8000) ? F_SF : 0) | parity_flag(d);
                    set_reg16(s, XR_EAX, d);
                } else {
                    uint32_t v = fetch32(s); uint32_t d = s.regs[XR_EAX];
                    and32(s, d, v);
                    s.regs[XR_EAX] = d;
                }
                break;
            }

            case 0x27: { // DAA
                uint8_t a = reg8_lo(s, XR_EAX);
                uint8_t old = a;
                bool old_cf = flag_cf(s);
                uint8_t cf = old_cf ? 1 : 0;
                if ((a & 0x0F) > 9 || flag_af(s)) { a = (uint8_t)(a + 6); cf = old_cf; }
                if (old > 0x99 || old_cf) { a = (uint8_t)(a + 0x60); cf = 1; }
                s.eflags = (s.eflags & ~F_CF) | (cf ? F_CF : 0);
                if ((old & 0x0F) > 9 || flag_af(s)) s.eflags |= F_AF; else s.eflags &= ~F_AF;
                set_reg8_lo(s, XR_EAX, a);
                s.eflags = (s.eflags & ~F_ARITH) | ((a == 0) ? F_ZF : 0) | ((a & 0x80) ? F_SF : 0) | parity_flag(a);
                s.eflags |= F_AF;
                break;
            }

            case 0x28: case 0x29: case 0x2A: case 0x2B: { // SUB
                uint8_t modrm = fetch8(s);
                uint8_t regf = (modrm >> 3) & 7;
                bool is_reg; uint8_t rm;
                uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                uint32_t base = seg_base(s, segovr);
                bool to_rm = (op & 2) == 0;
                if (opsize16) {
                    uint16_t src = to_rm ? reg16(s, regf) : (is_reg ? reg16(s, rm) : mr16(base + ea));
                    if (to_rm) {
                        uint16_t dst = is_reg ? reg16(s, rm) : mr16(base + ea);
                        sub16(s, dst, src);
                        if (is_reg) set_reg16(s, rm, dst); else mw16(base + ea, dst);
                    } else {
                        uint16_t dst = reg16(s, regf);
                        sub16(s, dst, src);
                        set_reg16(s, regf, dst);
                    }
                } else if (op & 1) {
                    uint32_t src = to_rm ? s.regs[regf] : (is_reg ? s.regs[rm] : mr32(base + ea));
                    if (to_rm) {
                        uint32_t dst = is_reg ? s.regs[rm] : mr32(base + ea);
                        sub32(s, dst, src);
                        if (is_reg) s.regs[rm] = dst; else mw32(base + ea, dst);
                    } else {
                        uint32_t dst = s.regs[regf];
                        sub32(s, dst, src);
                        s.regs[regf] = dst;
                    }
                } else {
                    uint8_t src = to_rm ? read_reg8(s, regf) : (is_reg ? read_reg8(s, rm) : mr8(base + ea));
                    if (to_rm) {
                        uint8_t dst = is_reg ? read_reg8(s, rm) : mr8(base + ea);
                        sub8(s, dst, src);
                        if (is_reg) write_reg8(s, rm, dst); else mw8(base + ea, dst);
                    } else {
                        uint8_t dst = read_reg8(s, regf);
                        sub8(s, dst, src);
                        write_reg8(s, regf, dst);
                    }
                }
                break;
            }

            case 0x2C: { uint8_t v = fetch8(s); uint8_t d = reg8_lo(s, XR_EAX); sub8(s, d, v); set_reg8_lo(s, XR_EAX, d); break; }
            case 0x2D: {
                if (opsize16) { uint16_t v = fetch16(s); uint16_t d = reg16(s, XR_EAX); sub16(s, d, v); set_reg16(s, XR_EAX, d); }
                else { uint32_t v = fetch32(s); uint32_t d = s.regs[XR_EAX]; sub32(s, d, v); s.regs[XR_EAX] = d; }
                break;
            }
            case 0x2F: { // DAS
                uint8_t a = reg8_lo(s, XR_EAX);
                bool old_cf = flag_cf(s);
                uint8_t cf = old_cf ? 1 : 0;
                bool old_af = flag_af(s);
                if ((a & 0x0F) > 9 || old_af) { a = (uint8_t)(a - 6); }
                if (a > 0x99 || old_cf) { a = (uint8_t)(a - 0x60); cf = 1; }
                s.eflags = (s.eflags & ~F_CF) | (cf ? F_CF : 0);
                if ((a & 0x0F) > 9 || old_af) s.eflags |= F_AF; else s.eflags &= ~F_AF;
                s.eflags = (s.eflags & ~F_ARITH) | ((a == 0) ? F_ZF : 0) | ((a & 0x80) ? F_SF : 0) | parity_flag(a);
                set_reg8_lo(s, XR_EAX, a);
                break;
            }

            case 0x30: case 0x31: case 0x32: case 0x33: { // XOR
                uint8_t modrm = fetch8(s);
                uint8_t regf = (modrm >> 3) & 7;
                bool is_reg; uint8_t rm;
                uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                uint32_t base = seg_base(s, segovr);
                bool to_rm = (op & 2) == 0;
                if (opsize16) {
                    uint16_t src = to_rm ? reg16(s, regf) : (is_reg ? reg16(s, rm) : mr16(base + ea));
                    if (to_rm) {
                        uint16_t dst = is_reg ? reg16(s, rm) : mr16(base + ea);
                        dst ^= src;
                        s.eflags = (s.eflags & ~F_ARITH) | ((dst == 0) ? F_ZF : 0) | ((dst & 0x8000) ? F_SF : 0) | parity_flag(dst);
                        if (is_reg) set_reg16(s, rm, dst); else mw16(base + ea, dst);
                    } else {
                        uint16_t dst = reg16(s, regf);
                        dst ^= src;
                        s.eflags = (s.eflags & ~F_ARITH) | ((dst == 0) ? F_ZF : 0) | ((dst & 0x8000) ? F_SF : 0) | parity_flag(dst);
                        set_reg16(s, regf, dst);
                    }
                } else if (op & 1) {
                    uint32_t src = to_rm ? s.regs[regf] : (is_reg ? s.regs[rm] : mr32(base + ea));
                    if (to_rm) {
                        uint32_t dst = is_reg ? s.regs[rm] : mr32(base + ea);
                        xor32(s, dst, src);
                        if (is_reg) s.regs[rm] = dst; else mw32(base + ea, dst);
                    } else {
                        uint32_t dst = s.regs[regf];
                        xor32(s, dst, src);
                        s.regs[regf] = dst;
                    }
                } else {
                    uint8_t src = to_rm ? read_reg8(s, regf) : (is_reg ? read_reg8(s, rm) : mr8(base + ea));
                    if (to_rm) {
                        uint8_t dst = is_reg ? read_reg8(s, rm) : mr8(base + ea);
                        xor8(s, dst, src);
                        if (is_reg) write_reg8(s, rm, dst); else mw8(base + ea, dst);
                    } else {
                        uint8_t dst = read_reg8(s, regf);
                        xor8(s, dst, src);
                        write_reg8(s, regf, dst);
                    }
                }
                break;
            }

            case 0x34: { uint8_t v = fetch8(s); uint8_t d = reg8_lo(s, XR_EAX); xor8(s, d, v); set_reg8_lo(s, XR_EAX, d); break; }
            case 0x35: {
                if (opsize16) {
                    uint16_t v = fetch16(s); uint16_t d = reg16(s, XR_EAX);
                    d ^= v;
                    s.eflags = (s.eflags & ~F_ARITH) | ((d == 0) ? F_ZF : 0) | ((d & 0x8000) ? F_SF : 0) | parity_flag(d);
                    set_reg16(s, XR_EAX, d);
                } else {
                    uint32_t v = fetch32(s); uint32_t d = s.regs[XR_EAX];
                    xor32(s, d, v);
                    s.regs[XR_EAX] = d;
                }
                break;
            }
            case 0x37: case 0x3F: { // AAA / AAS (adjust after add/sub)
                uint8_t a = reg8_lo(s, XR_EAX);
                bool old_af = flag_af(s);
                if ((a & 0x0F) > 9 || old_af) {
                    if (op == 0x37) { a = (uint8_t)(a + 6); }
                    else { a = (uint8_t)(a - 6); }
                    a = (uint8_t)((a & 0x0F) | 0);
                    uint16_t ax = reg16(s, XR_EAX);
                    if (op == 0x37) ax = (uint16_t)(ax + 0x100);
                    else ax = (uint16_t)(ax - 0x100);
                    set_reg16(s, XR_EAX, ax);
                    s.eflags |= F_AF;
                    s.eflags |= F_CF;
                } else {
                    s.eflags &= ~(F_AF | F_CF);
                }
                set_reg8_lo(s, XR_EAX, (uint8_t)(reg8_lo(s, XR_EAX) & 0x0F));
                break;
            }

            case 0x38: case 0x39: case 0x3A: case 0x3B: { // CMP
                uint8_t modrm = fetch8(s);
                uint8_t regf = (modrm >> 3) & 7;
                bool is_reg; uint8_t rm;
                uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                uint32_t base = seg_base(s, segovr);
                bool to_rm = (op & 2) == 0;
                if (opsize16) {
                    uint16_t src = to_rm ? reg16(s, regf) : (is_reg ? reg16(s, rm) : mr16(base + ea));
                    uint16_t dst = to_rm ? (is_reg ? reg16(s, rm) : mr16(base + ea)) : reg16(s, regf);
                    cmp16(s, dst, src);
                } else if (op & 1) {
                    uint32_t src = to_rm ? s.regs[regf] : (is_reg ? s.regs[rm] : mr32(base + ea));
                    uint32_t dst = to_rm ? (is_reg ? s.regs[rm] : mr32(base + ea)) : s.regs[regf];
                    cmp32(s, dst, src);
                } else {
                    uint8_t src = to_rm ? read_reg8(s, regf) : (is_reg ? read_reg8(s, rm) : mr8(base + ea));
                    uint8_t dst = to_rm ? (is_reg ? read_reg8(s, rm) : mr8(base + ea)) : read_reg8(s, regf);
                    cmp8(s, dst, src);
                }
                break;
            }

            case 0x3C: { uint8_t v = fetch8(s); cmp8(s, reg8_lo(s, XR_EAX), v); break; }
            case 0x3D: {
                if (opsize16) { uint16_t v = fetch16(s); cmp16(s, reg16(s, XR_EAX), v); }
                else { uint32_t v = fetch32(s); cmp32(s, s.regs[XR_EAX], v); }
                break;
            }

            // ---- INC / DEC ----
            case 0x40: case 0x41: case 0x42: case 0x43:
            case 0x44: case 0x45: case 0x46: case 0x47: { // INC r32
                uint8_t r = op & 7;
                uint32_t cf_save = s.eflags & F_CF;
                if (opsize16) {
                    uint16_t v = reg16(s, r);
                    uint16_t r2 = (uint16_t)(v + 1);
                    s.eflags = (s.eflags & ~F_ARITH);
                    set_arith_flags<uint16_t>(s, v, 1, r2, true, false);
                    s.eflags = (s.eflags & ~F_CF) | cf_save;
                    set_reg16(s, r, r2);
                } else {
                    uint32_t v = s.regs[r];
                    uint32_t r2 = v + 1;
                    s.eflags = (s.eflags & ~F_ARITH);
                    set_arith_flags<uint32_t>(s, v, 1, r2, true, false);
                    s.eflags = (s.eflags & ~F_CF) | cf_save;
                    s.regs[r] = r2;
                }
                break;
            }
            case 0x48: case 0x49: case 0x4A: case 0x4B:
            case 0x4C: case 0x4D: case 0x4E: case 0x4F: { // DEC r32
                uint8_t r = op & 7;
                uint32_t cf_save = s.eflags & F_CF;
                if (opsize16) {
                    uint16_t v = reg16(s, r);
                    uint16_t r2 = (uint16_t)(v - 1);
                    s.eflags = (s.eflags & ~F_ARITH);
                    set_arith_flags<uint16_t>(s, v, 1, r2, false, true);
                    s.eflags = (s.eflags & ~F_CF) | cf_save;
                    set_reg16(s, r, r2);
                } else {
                    uint32_t v = s.regs[r];
                    uint32_t r2 = v - 1;
                    s.eflags = (s.eflags & ~F_ARITH);
                    set_arith_flags<uint32_t>(s, v, 1, r2, false, true);
                    s.eflags = (s.eflags & ~F_CF) | cf_save;
                    s.regs[r] = r2;
                }
                break;
            }

            // ---- PUSH / POP ----
            case 0x50: case 0x51: case 0x52: case 0x53:
            case 0x54: case 0x55: case 0x56: case 0x57: { // PUSH r32
                uint8_t r = op & 7;
                if (opsize16) push16(s, reg16(s, r));
                else push32(s, s.regs[r]);
                break;
            }
            case 0x58: case 0x59: case 0x5A: case 0x5B:
            case 0x5C: case 0x5D: case 0x5E: case 0x5F: { // POP r32
                uint8_t r = op & 7;
                if (opsize16) set_reg16(s, r, pop16(s));
                else s.regs[r] = pop32(s);
                break;
            }
            case 0x60: { // PUSHAD
                uint32_t esp_val = s.regs[XR_ESP];
                for (int i = 0; i < 8; i++) {
                    uint32_t v = (i == 4) ? esp_val : s.regs[i];
                    push32(s, v);
                }
                break;
            }
            case 0x61: { // POPAD
                uint32_t vals[8];
                for (int i = 7; i >= 0; i--) vals[i] = pop32(s);
                for (int i = 0; i < 8; i++) if (i != 4) s.regs[i] = vals[i];
                break;
            }
            case 0x68: { // PUSH imm32
                if (opsize16) push16(s, fetch16(s));
                else push32(s, fetch32(s));
                break;
            }
            case 0x6A: { // PUSH imm8 (sign-extended)
                int8_t v = static_cast<int8_t>(fetch8(s));
                if (opsize16) push16(s, static_cast<uint16_t>(v));
                else push32(s, static_cast<uint32_t>(static_cast<int32_t>(v)));
                break;
            }
            case 0x6C: // INSB
            case 0x6D: // INSW/INSD
            case 0x6E: // OUTSB
            case 0x6F: // OUTSW/OUTSD
                if (rep) s.regs[XR_ECX] = 0;
                rep = repz = repnz = false;
                break;
            case 0x8F: { // POP r/m
                uint8_t modrm = fetch8(s);
                bool is_reg; uint8_t rm;
                uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                if (is_reg) {
                    if (opsize16) set_reg16(s, rm, pop16(s));
                    else s.regs[rm] = pop32(s);
                } else {
                    uint32_t base = seg_base(s, segovr);
                    if (opsize16) mw16(base + ea, pop16(s));
                    else mw32(base + ea, pop32(s));
                }
                break;
            }

            // ---- IMUL ----
            case 0x69: case 0x6B: { // IMUL r, r/m, imm
                uint8_t modrm = fetch8(s);
                uint8_t regf = (modrm >> 3) & 7;
                bool is_reg; uint8_t rm;
                uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                uint32_t base = seg_base(s, segovr);
                int64_t imm = (op == 0x6B) ? (int64_t)(int8_t)fetch8(s) : (int64_t)(int32_t)fetch32(s);
                int64_t src;
                if (opsize16) {
                    src = is_reg ? (int16_t)reg16(s, rm) : (int16_t)mr16(base + ea);
                    int64_t r = src * imm;
                    int16_t r16 = (int16_t)r;
                    set_reg16(s, regf, (uint16_t)r16);
                    s.eflags = (s.eflags & ~(F_CF | F_OF)) | ((r != (int64_t)r16) ? (F_CF | F_OF) : 0);
                } else {
                    src = is_reg ? (int32_t)s.regs[rm] : (int32_t)mr32(base + ea);
                    int64_t r = src * imm;
                    int32_t r32 = (int32_t)r;
                    s.regs[regf] = (uint32_t)r32;
                    s.eflags = (s.eflags & ~(F_CF | F_OF)) | ((r != (int64_t)r32) ? (F_CF | F_OF) : 0);
                }
                break;
            }

            // ---- Jcc rel8 ----
            case 0x70: case 0x71: case 0x72: case 0x73:
            case 0x74: case 0x75: case 0x76: case 0x77:
            case 0x78: case 0x79: case 0x7A: case 0x7B:
            case 0x7C: case 0x7D: case 0x7E: case 0x7F: {
                int8_t rel = static_cast<int8_t>(fetch8(s));
                uint32_t target = s.eip + static_cast<int32_t>(rel);
                bool taken = false;
                switch (op & 0x0F) {
                    case 0x0: taken = flag_of(s); break;
                    case 0x1: taken = !flag_of(s); break;
                    case 0x2: taken = flag_cf(s); break;
                    case 0x3: taken = !flag_cf(s); break;
                    case 0x4: taken = flag_zf(s); break;
                    case 0x5: taken = !flag_zf(s); break;
                    case 0x6: taken = flag_cf(s) || flag_zf(s); break;
                    case 0x7: taken = !flag_cf(s) && !flag_zf(s); break;
                    case 0x8: taken = flag_sf(s); break;
                    case 0x9: taken = !flag_sf(s); break;
                    case 0xA: taken = flag_pf(s); break;
                    case 0xB: taken = !flag_pf(s); break;
                    case 0xC: taken = flag_sf(s) != flag_of(s); break;
                    case 0xD: taken = flag_sf(s) == flag_of(s); break;
                    case 0xE: taken = flag_zf(s) || (flag_sf(s) != flag_of(s)); break;
                    case 0xF: taken = !flag_zf(s) && (flag_sf(s) == flag_of(s)); break;
                }
                if (taken) s.eip = target;
                break;
            }

            // ---- Group 1 (imm) 0x80-0x83 ----
            case 0x80: case 0x81: case 0x82: case 0x83: {
                uint8_t modrm = fetch8(s);
                uint8_t opf = (modrm >> 3) & 7;
                bool is_reg; uint8_t rm;
                uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                uint32_t base = seg_base(s, segovr);
                int ow2 = opsize16 ? 2 : 4;
                bool isbyte = (op == 0x80 || op == 0x82);
                bool signimm = (op == 0x83);

                if (opsize16) {
                    uint16_t imm = (signimm || isbyte)
                        ? (uint16_t)(int16_t)(int8_t)fetch8(s)
                        : fetch16(s);
                    uint16_t dst = is_reg ? reg16(s, rm) : mr16(base + ea);
                    switch (opf) {
                        case 0: { uint16_t r = (uint16_t)(dst + imm); set_arith_flags<uint16_t>(s, dst, imm, r, true, false); dst = r; break; }
                        case 1: { uint16_t r = (uint16_t)(dst | imm); s.eflags = (s.eflags & ~F_ARITH) | ((r == 0) ? F_ZF : 0) | ((r & 0x8000) ? F_SF : 0) | parity_flag(r); dst = r; break; }
                        case 2: { uint16_t r = (uint16_t)(dst + imm + (flag_cf(s) ? 1 : 0)); set_arith_flags<uint16_t>(s, dst, imm, r, true, false, flag_cf(s) ? 1 : 0); dst = r; break; }
                        case 3: { uint16_t r = (uint16_t)(dst - imm - (flag_cf(s) ? 1 : 0)); set_arith_flags<uint16_t>(s, dst, imm, r, false, true, flag_cf(s) ? 1 : 0); dst = r; break; }
                        case 4: { dst &= imm; s.eflags = (s.eflags & ~F_ARITH) | ((dst == 0) ? F_ZF : 0) | ((dst & 0x8000) ? F_SF : 0) | parity_flag(dst); break; }
                        case 5: { dst = (uint16_t)(dst - imm); set_arith_flags<uint16_t>(s, dst, imm, dst, false, true); break; }
                        case 6: { dst ^= imm; s.eflags = (s.eflags & ~F_ARITH) | ((dst == 0) ? F_ZF : 0) | ((dst & 0x8000) ? F_SF : 0) | parity_flag(dst); break; }
                        case 7: cmp16(s, dst, imm); break;
                    }
                    if (opf != 7) { if (is_reg) set_reg16(s, rm, dst); else mw16(base + ea, dst); }
                } else if (ow2 == 4 && !isbyte) {
                    uint32_t imm;
                    if (signimm) imm = (uint32_t)(int32_t)(int8_t)fetch8(s);
                    else imm = fetch32(s);
                    uint32_t dst = is_reg ? s.regs[rm] : mr32(base + ea);
                    switch (opf) {
                        case 0: { uint32_t r = dst + imm; set_arith_flags<uint32_t>(s, dst, imm, r, true, false); dst = r; break; }
                        case 1: { dst |= imm; s.eflags = (s.eflags & ~F_ARITH) | ((dst == 0) ? F_ZF : 0) | ((dst & 0x80000000) ? F_SF : 0) | parity_flag(dst); break; }
                        case 2: { uint32_t r = dst + imm + (flag_cf(s) ? 1 : 0); set_arith_flags<uint32_t>(s, dst, imm, r, true, false, flag_cf(s) ? 1 : 0); dst = r; break; }
                        case 3: { uint32_t r = dst - imm - (flag_cf(s) ? 1 : 0); set_arith_flags<uint32_t>(s, dst, imm, r, false, true, flag_cf(s) ? 1 : 0); dst = r; break; }
                        case 4: { dst &= imm; s.eflags = (s.eflags & ~F_ARITH) | ((dst == 0) ? F_ZF : 0) | ((dst & 0x80000000) ? F_SF : 0) | parity_flag(dst); break; }
                        case 5: { sub32(s, dst, imm); break; }
                        case 6: { dst ^= imm; s.eflags = (s.eflags & ~F_ARITH) | ((dst == 0) ? F_ZF : 0) | ((dst & 0x80000000) ? F_SF : 0) | parity_flag(dst); break; }
                        case 7: cmp32(s, dst, imm); break;
                    }
                    if (opf != 7) { if (is_reg) s.regs[rm] = dst; else mw32(base + ea, dst); }
                } else {
                    uint8_t imm = fetch8(s);
                    uint8_t dst = is_reg ? read_reg8(s, rm) : mr8(base + ea);
                    switch (opf) {
                        case 0: { uint8_t r = (uint8_t)(dst + imm); set_arith_flags<uint8_t>(s, dst, imm, r, true, false); dst = r; break; }
                        case 1: { dst |= imm; s.eflags = (s.eflags & ~F_ARITH) | ((dst == 0) ? F_ZF : 0) | ((dst & 0x80) ? F_SF : 0) | parity_flag(dst); break; }
                        case 2: { uint8_t r = (uint8_t)(dst + imm + (flag_cf(s) ? 1 : 0)); set_arith_flags<uint8_t>(s, dst, (uint8_t)(imm + (flag_cf(s) ? 1 : 0)), r, true, false); dst = r; break; }
                        case 3: { uint8_t r = (uint8_t)(dst - imm - (flag_cf(s) ? 1 : 0)); set_arith_flags<uint8_t>(s, dst, (uint8_t)(imm + (flag_cf(s) ? 1 : 0)), r, false, true); dst = r; break; }
                        case 4: { dst &= imm; s.eflags = (s.eflags & ~F_ARITH) | ((dst == 0) ? F_ZF : 0) | ((dst & 0x80) ? F_SF : 0) | parity_flag(dst); break; }
                        case 5: { sub8(s, dst, imm); break; }
                        case 6: { dst ^= imm; s.eflags = (s.eflags & ~F_ARITH) | ((dst == 0) ? F_ZF : 0) | ((dst & 0x80) ? F_SF : 0) | parity_flag(dst); break; }
                        case 7: cmp8(s, dst, imm); break;
                    }
                    if (opf != 7) { if (is_reg) write_reg8(s, rm, dst); else mw8(base + ea, dst); }
                }
                (void)ow2;
                break;
            }

            // ---- TEST ----
            case 0x84: case 0x85: {
                uint8_t modrm = fetch8(s);
                uint8_t regf = (modrm >> 3) & 7;
                bool is_reg; uint8_t rm;
                uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                uint32_t base = seg_base(s, segovr);
                if (opsize16) {
                    uint16_t a = is_reg ? reg16(s, rm) : mr16(base + ea);
                    uint16_t b = reg16(s, regf);
                    uint16_t r = (uint16_t)(a & b);
                    s.eflags = (s.eflags & ~F_ARITH) | ((r == 0) ? F_ZF : 0) | ((r & 0x8000) ? F_SF : 0) | parity_flag(r);
                } else if (op & 1) {
                    uint32_t a = is_reg ? s.regs[rm] : mr32(base + ea);
                    test32(s, a, s.regs[regf]);
                } else {
                    uint8_t a = is_reg ? read_reg8(s, rm) : mr8(base + ea);
                    test8(s, a, read_reg8(s, regf));
                }
                break;
            }

            // ---- XCHG ----
            case 0x86: case 0x87: {
                uint8_t modrm = fetch8(s);
                uint8_t regf = (modrm >> 3) & 7;
                bool is_reg; uint8_t rm;
                uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                uint32_t base = seg_base(s, segovr);
                if (opsize16) {
                    uint16_t a = reg16(s, regf);
                    uint16_t b = is_reg ? reg16(s, rm) : mr16(base + ea);
                    set_reg16(s, regf, b);
                    if (is_reg) set_reg16(s, rm, a); else mw16(base + ea, a);
                } else if (op & 1) {
                    uint32_t a = s.regs[regf];
                    uint32_t b = is_reg ? s.regs[rm] : mr32(base + ea);
                    s.regs[regf] = b;
                    if (is_reg) s.regs[rm] = a; else mw32(base + ea, a);
                } else {
                    uint8_t a = read_reg8(s, regf);
                    uint8_t b = is_reg ? read_reg8(s, rm) : mr8(base + ea);
                    write_reg8(s, regf, b);
                    if (is_reg) write_reg8(s, rm, a); else mw8(base + ea, a);
                }
                break;
            }

            // ---- MOV ----
            case 0x88: case 0x89: case 0x8A: case 0x8B: { // MOV
                uint8_t modrm = fetch8(s);
                uint8_t regf = (modrm >> 3) & 7;
                bool is_reg; uint8_t rm;
                uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                uint32_t base = seg_base(s, segovr);
                bool to_rm = (op & 2) == 0;
                if (opsize16) {
                    uint16_t src = to_rm ? reg16(s, regf) : (is_reg ? reg16(s, rm) : mr16(base + ea));
                    if (to_rm) { if (is_reg) set_reg16(s, rm, src); else mw16(base + ea, src); }
                    else set_reg16(s, regf, src);
                } else if (op & 1) {
                    uint32_t src = to_rm ? s.regs[regf] : (is_reg ? s.regs[rm] : mr32(base + ea));
                    if (to_rm) { if (is_reg) s.regs[rm] = src; else mw32(base + ea, src); }
                    else s.regs[regf] = src;
                } else {
                    uint8_t src = to_rm ? read_reg8(s, regf) : (is_reg ? read_reg8(s, rm) : mr8(base + ea));
                    if (to_rm) { if (is_reg) write_reg8(s, rm, src); else mw8(base + ea, src); }
                    else write_reg8(s, regf, src);
                }
                break;
            }

            case 0x8C: { // MOV r/m, Sreg
                uint8_t modrm = fetch8(s);
                uint8_t regf = (modrm >> 3) & 7;
                bool is_reg; uint8_t rm;
                uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                uint32_t v = s.seg[seg_index(regf)];
                if (is_reg) { if (opsize16) set_reg16(s, rm, (uint16_t)v); else s.regs[rm] = v; }
                else { uint32_t base = seg_base(s, segovr); if (opsize16) mw16(base + ea, (uint16_t)v); else mw32(base + ea, v); }
                break;
            }
            case 0x8D: { // LEA
                uint8_t modrm = fetch8(s);
                uint8_t regf = (modrm >> 3) & 7;
                uint32_t ea = calc_ea32(s, modrm, addrsize16);
                if (opsize16) set_reg16(s, regf, (uint16_t)ea);
                else s.regs[regf] = ea;
                break;
            }
            case 0x8E: { // MOV Sreg, r/m
                uint8_t modrm = fetch8(s);
                uint8_t regf = (modrm >> 3) & 7;
                bool is_reg; uint8_t rm;
                uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                uint16_t v = is_reg ? (uint16_t)s.regs[rm] : mr16(seg_base(s, segovr) + ea);
                if (regf == 4) s.fs_base = 0;
                else if (regf == 5) s.gs_base = 0;
                else s.seg[seg_index(regf)] = v;
                break;
            }

            case 0x90: break; // NOP
            case 0x91: case 0x92: case 0x93: case 0x94:
            case 0x95: case 0x96: case 0x97: { // XCHG eAX, r32
                uint8_t r = op & 7;
                uint32_t t = s.regs[XR_EAX];
                s.regs[XR_EAX] = s.regs[r];
                s.regs[r] = t;
                break;
            }

            case 0x98: { // CWDE
                if (opsize16) set_reg16(s, XR_EAX, (uint16_t)(int16_t)reg8_lo(s, XR_EAX));
                else s.regs[XR_EAX] = (uint32_t)(int32_t)(int16_t)reg16(s, XR_EAX);
                break;
            }
            case 0x99: { // CDQ
                if (opsize16) set_reg16(s, XR_EDX, (uint16_t)((reg16(s, XR_EAX) & 0x8000) ? 0xFFFF : 0));
                else s.regs[XR_EDX] = (s.regs[XR_EAX] & 0x80000000) ? 0xFFFFFFFF : 0;
                break;
            }

            case 0x9B: break; // WAIT/FWAIT

            case 0x9C: { // PUSHFD
                if (opsize16) push16(s, (uint16_t)s.eflags);
                else push32(s, s.eflags);
                break;
            }
            case 0x9D: { // POPFD
                if (opsize16) s.eflags = (s.eflags & ~0xFFFFu) | pop16(s);
                else s.eflags = pop32(s) | 0x2;
                break;
            }
            case 0x9E: { // SAHF: load SF:ZF:AF:PF:CF from AH
                uint8_t ah = (uint8_t)(s.regs[XR_EAX] >> 8);
                s.eflags = (s.eflags & ~(F_CF | F_PF | F_AF | F_ZF | F_SF)) |
                           ((ah & 0x01) ? F_CF : 0) | ((ah & 0x04) ? F_PF : 0) |
                           ((ah & 0x10) ? F_AF : 0) | ((ah & 0x40) ? F_ZF : 0) |
                           ((ah & 0x80) ? F_SF : 0);
                break;
            }
            case 0x9F: { // LAHF
                uint8_t ah = (uint8_t)s.eflags;
                set_reg8_hi(s, XR_EAX, ah);
                break;
            }

            // ---- moffs ----
            // The moffs displacement width follows the ADDRESS size (0x67),
            // not the operand size (0x66). 0x66 only narrows the data moved.
            case 0xA0: case 0xA1: { // MOV AL/eAX, moffs
                uint32_t base = seg_base(s, segovr);
                uint32_t off = addrsize16 ? fetch16(s) : fetch32(s);
                if (op == 0xA1) {
                    if (opsize16) set_reg16(s, XR_EAX, mr16(base + off));
                    else s.regs[XR_EAX] = mr32(base + off);
                } else {
                    set_reg8_lo(s, XR_EAX, mr8(base + off));
                }
                break;
            }
            case 0xA2: case 0xA3: { // MOV moffs, AL/eAX
                uint32_t base = seg_base(s, segovr);
                uint32_t off = addrsize16 ? fetch16(s) : fetch32(s);
                if (op == 0xA3) {
                    if (opsize16) mw16(base + off, reg16(s, XR_EAX));
                    else mw32(base + off, s.regs[XR_EAX]);
                } else {
                    mw8(base + off, reg8_lo(s, XR_EAX));
                }
                break;
            }

            // ---- string ops ----
            case 0xA4: { // MOVSB
                uint32_t base_src = (segovr >= 0) ? seg_base(s, segovr) : 0;
                auto step = [&]() {
                    mw8(s.regs[XR_EDI], mr8(base_src + s.regs[XR_ESI]));
                    if (flag_df(s)) { s.regs[XR_ESI]--; s.regs[XR_EDI]--; }
                    else { s.regs[XR_ESI]++; s.regs[XR_EDI]++; }
                };
                if (rep) { while (s.regs[XR_ECX] != 0) { step(); s.regs[XR_ECX]--; } }
                else step();
                break;
            }
            case 0xA5: { // MOVSW/MOVSD
                uint32_t base_src = (segovr >= 0) ? seg_base(s, segovr) : 0;
                int sz = opsize16 ? 2 : 4;
                auto step = [&]() {
                    if (opsize16) mw16(s.regs[XR_EDI], mr16(base_src + s.regs[XR_ESI]));
                    else mw32(s.regs[XR_EDI], mr32(base_src + s.regs[XR_ESI]));
                    if (flag_df(s)) { s.regs[XR_ESI] -= (uint32_t)sz; s.regs[XR_EDI] -= (uint32_t)sz; }
                    else { s.regs[XR_ESI] += (uint32_t)sz; s.regs[XR_EDI] += (uint32_t)sz; }
                };
                if (rep) { while (s.regs[XR_ECX] != 0) { step(); s.regs[XR_ECX]--; } }
                else step();
                break;
            }
            case 0xA6: case 0xA7: { // CMPSB/CMPSW/CMPSD
                uint32_t base_src = (segovr >= 0) ? seg_base(s, segovr) : 0;
                int sz = (op == 0xA6) ? 1 : (opsize16 ? 2 : 4);
                auto step = [&]() {
                    if (op == 0xA6) cmp8(s, mr8(s.regs[XR_EDI]), mr8(base_src + s.regs[XR_ESI]));
                    else if (opsize16) cmp16(s, mr16(s.regs[XR_EDI]), mr16(base_src + s.regs[XR_ESI]));
                    else cmp32(s, mr32(s.regs[XR_EDI]), mr32(base_src + s.regs[XR_ESI]));
                    if (flag_df(s)) { s.regs[XR_ESI] -= (uint32_t)sz; s.regs[XR_EDI] -= (uint32_t)sz; }
                    else { s.regs[XR_ESI] += (uint32_t)sz; s.regs[XR_EDI] += (uint32_t)sz; }
                };
                if (repz || repnz) {
                    while (s.regs[XR_ECX] != 0) {
                        step();
                        s.regs[XR_ECX]--;
                        if (repz && !(s.eflags & F_ZF)) break;
                        if (repnz && (s.eflags & F_ZF)) break;
                    }
                } else step();
                break;
            }
            case 0xA8: { // TEST AL, imm8
                uint8_t v = fetch8(s);
                test8(s, reg8_lo(s, XR_EAX), v);
                break;
            }
            case 0xA9: { // TEST eAX, imm
                if (opsize16) { uint16_t v = fetch16(s); uint16_t r = (uint16_t)(reg16(s, XR_EAX) & v); s.eflags = (s.eflags & ~F_ARITH) | ((r == 0) ? F_ZF : 0) | ((r & 0x8000) ? F_SF : 0) | parity_flag(r); }
                else { uint32_t v = fetch32(s); test32(s, s.regs[XR_EAX], v); }
                break;
            }
            case 0xAA: { // STOSB
                auto step = [&]() { mw8(s.regs[XR_EDI], reg8_lo(s, XR_EAX)); if (flag_df(s)) s.regs[XR_EDI]--; else s.regs[XR_EDI]++; };
                if (rep) { while (s.regs[XR_ECX] != 0) { step(); s.regs[XR_ECX]--; } }
                else step();
                break;
            }
            case 0xAB: { // STOSW/STOSD
                int sz = opsize16 ? 2 : 4;
                auto step = [&]() {
                    if (opsize16) mw16(s.regs[XR_EDI], reg16(s, XR_EAX));
                    else mw32(s.regs[XR_EDI], s.regs[XR_EAX]);
                    if (flag_df(s)) s.regs[XR_EDI] -= (uint32_t)sz; else s.regs[XR_EDI] += (uint32_t)sz;
                };
                if (rep) { while (s.regs[XR_ECX] != 0) { step(); s.regs[XR_ECX]--; } }
                else step();
                break;
            }
            case 0xAC: { // LODSB
                uint32_t base = (segovr >= 0) ? seg_base(s, segovr) : 0;
                set_reg8_lo(s, XR_EAX, mr8(base + s.regs[XR_ESI]));
                if (flag_df(s)) s.regs[XR_ESI]--; else s.regs[XR_ESI]++;
                break;
            }
            case 0xAD: { // LODSW/LODSD
                uint32_t base = (segovr >= 0) ? seg_base(s, segovr) : 0;
                if (opsize16) { set_reg16(s, XR_EAX, mr16(base + s.regs[XR_ESI])); if (flag_df(s)) s.regs[XR_ESI] -= 2; else s.regs[XR_ESI] += 2; }
                else { s.regs[XR_EAX] = mr32(base + s.regs[XR_ESI]); if (flag_df(s)) s.regs[XR_ESI] -= 4; else s.regs[XR_ESI] += 4; }
                break;
            }
            case 0xAE: { // SCASB
                int sz = 1;
                auto step = [&]() {
                    cmp8(s, reg8_lo(s, XR_EAX), mr8(s.regs[XR_EDI]));
                    if (flag_df(s)) s.regs[XR_EDI] -= (uint32_t)sz; else s.regs[XR_EDI] += (uint32_t)sz;
                };
                if (repz || repnz) {
                    while (s.regs[XR_ECX] != 0) {
                        step();
                        s.regs[XR_ECX]--;
                        if (repz && !(s.eflags & F_ZF)) break;
                        if (repnz && (s.eflags & F_ZF)) break;
                    }
                } else step();
                break;
            }
            case 0xAF: { // SCASW/SCASD
                int sz = opsize16 ? 2 : 4;
                auto step = [&]() {
                    if (opsize16) cmp16(s, reg16(s, XR_EAX), mr16(s.regs[XR_EDI]));
                    else cmp32(s, s.regs[XR_EAX], mr32(s.regs[XR_EDI]));
                    if (flag_df(s)) s.regs[XR_EDI] -= (uint32_t)sz; else s.regs[XR_EDI] += (uint32_t)sz;
                };
                if (repz || repnz) {
                    while (s.regs[XR_ECX] != 0) {
                        step();
                        s.regs[XR_ECX]--;
                        if (repz && !(s.eflags & F_ZF)) break;
                        if (repnz && (s.eflags & F_ZF)) break;
                    }
                } else step();
                break;
            }

            // ---- MOV r, imm ----
            case 0xB0: case 0xB1: case 0xB2: case 0xB3:
            case 0xB4: case 0xB5: case 0xB6: case 0xB7: {
                uint8_t v = fetch8(s);
                write_reg8(s, op & 7, v);
                break;
            }
            case 0xB8: case 0xB9: case 0xBA: case 0xBB:
            case 0xBC: case 0xBD: case 0xBE: case 0xBF: {
                if (opsize16) set_reg16(s, op & 7, fetch16(s));
                else s.regs[op & 7] = fetch32(s);
                break;
            }

            // ---- Group 2 shifts ----
            case 0xC0: case 0xC1: case 0xD0: case 0xD1: case 0xD2: case 0xD3: {
                uint8_t modrm = fetch8(s);
                uint8_t opf = (modrm >> 3) & 7;
                bool is_reg; uint8_t rm;
                uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                uint32_t base = seg_base(s, segovr);
                uint8_t count = 0;
                if (op == 0xC0 || op == 0xC1) count = fetch8(s);
                else if (op == 0xD0 || op == 0xD1) count = 1;
                else count = reg8_lo(s, XR_ECX);
                if (!(op & 1)) { // byte operand (0xC0/0xD0/0xD2)
                    uint8_t v = is_reg ? read_reg8(s, rm) : mr8(base + ea);
                    shift8(s, v, count, opf);
                    if (is_reg) write_reg8(s, rm, v); else mw8(base + ea, v);
                } else if (opsize16) {
                    uint16_t v = is_reg ? reg16(s, rm) : mr16(base + ea);
                    shift16(s, v, count, opf);
                    if (is_reg) set_reg16(s, rm, v); else mw16(base + ea, v);
                } else {
                    uint32_t v = is_reg ? s.regs[rm] : mr32(base + ea);
                    shift32(s, v, count, opf);
                    if (is_reg) s.regs[rm] = v; else mw32(base + ea, v);
                }
                break;
            }

            case 0xC2: { // RET imm16
                {
                uint16_t n = fetch16(s);
                uint32_t target = pop32(s);
                if (!guest_addr_may_hold_code(target) && !is_api_thunk_addr(target) &&
                    !is_callback_sentinel(target)) {
                    report_wild_transfer("ret", target, s, "ret-imm", 0, 0);
                }
                s.eip = target;
                s.regs[XR_ESP] += n;
                if (g_callback_sentinel && s.eip == g_callback_sentinel) s.halted = true;
                }
                break;
            }
            case 0xC3: { // RET
                {
                uint32_t target = pop32(s);
                if (!guest_addr_may_hold_code(target) && !is_api_thunk_addr(target) &&
                    !is_callback_sentinel(target)) {
                    report_wild_transfer("ret", target, s, "ret", 0, 0);
                }
                s.eip = target;
                if (g_callback_sentinel && s.eip == g_callback_sentinel) s.halted = true;
                }
                break;
            }

            case 0xC6: case 0xC7: { // MOV r/m, imm
                uint8_t modrm = fetch8(s);
                bool is_reg; uint8_t rm;
                uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                uint32_t base = seg_base(s, segovr);
                if (opsize16) {
                    uint16_t v = fetch16(s);
                    if (is_reg) set_reg16(s, rm, v); else mw16(base + ea, v);
                } else if (op == 0xC7) {
                    uint32_t v = fetch32(s);
                    if (is_reg) s.regs[rm] = v; else mw32(base + ea, v);
                } else {
                    uint8_t v = fetch8(s);
                    if (is_reg) write_reg8(s, rm, v); else mw8(base + ea, v);
                }
                break;
            }

            case 0xC8: { // ENTER
                uint16_t size = fetch16(s);
                uint8_t level = fetch8(s);
                uint32_t fp = s.regs[XR_EBP];
                uint32_t sp = s.regs[XR_ESP];
                if (level > 0) {
                    for (int i = 1; i <= level; i++) {
                        sp -= 4;
                        mw32(sp, mr32(fp - i * 4));
                    }
                    sp -= 4;
                    mw32(sp, fp);
                } else {
                    sp -= 4;
                    mw32(sp, fp);
                }
                sp -= size;
                s.regs[XR_EBP] = sp + size;
                s.regs[XR_ESP] = sp;
                break;
            }
            case 0xC9: { // LEAVE
                s.regs[XR_ESP] = s.regs[XR_EBP];
                s.regs[XR_EBP] = pop32(s);
                break;
            }

            case 0xCC: { // INT3
                // Raise a real EXCEPTION_BREAKPOINT through the guest's SEH
                // chain. MSVC uses int3 both as a debugger breakpoint and as
                // padding after noreturn paths (e.g. a C++ throw helper that
                // unwound without transferring control). Treating the byte as a
                // no-op was wrong: execution then falls into the following
                // function's byte stream and runs misaligned. If the app has a
                // handler it resumes there; if not the exception is unhandled
                // and reported, which is the honest outcome.
                interp_fault_int3(s);
                break;
            }
            case 0xCD: { // INT imm8
                uint8_t n = fetch8(s);
                if (n == 0x80 || n == 0x2E || n == 0x2B) {
                    // syscall-ish interrupt from guest: not supported
                    interp_fault(s, "int 0x80/0x2E");
                } else {
                    interp_fault(s, "int imm8");
                }
                break;
            }
            case 0xCE: { // INTO
                interp_fault(s, "into");
                break;
            }
            case 0xCF: { // IRET
                interp_fault(s, "iret");
                break;
            }

            case 0xD4: { // AAM
                uint8_t v = fetch8(s);
                if (v == 0) { interp_fault(s, "aam 0"); break; }
                uint16_t ax = reg16(s, XR_EAX);
                uint8_t al = (uint8_t)ax;
                uint8_t ah = (uint8_t)(al / v);
                set_reg16(s, XR_EAX, (uint16_t)((ah << 8) | (al % v)));
                uint8_t r = (uint8_t)(al % v);
                s.eflags = (s.eflags & ~F_ARITH) | ((r == 0) ? F_ZF : 0) | ((r & 0x80) ? F_SF : 0) | parity_flag(r);
                break;
            }
            case 0xD5: { // AAD
                uint8_t v = fetch8(s);
                uint16_t ax = reg16(s, XR_EAX);
                uint8_t al = (uint8_t)ax;
                uint8_t ah = (uint8_t)(ax >> 8);
                uint8_t r = (uint8_t)(al + ah * v);
                set_reg16(s, XR_EAX, (uint16_t)r);
                s.eflags = (s.eflags & ~F_ARITH) | ((r == 0) ? F_ZF : 0) | ((r & 0x80) ? F_SF : 0) | parity_flag(r);
                break;
            }
            case 0xD7: { // XLAT
                uint32_t base = (segovr >= 0) ? seg_base(s, segovr) : 0;
                set_reg8_lo(s, XR_EAX, mr8(base + s.regs[XR_EBX] + reg8_lo(s, XR_EAX)));
                break;
            }

            // ---- x87 ----
            case 0xD8: case 0xD9: case 0xDA: case 0xDB:
            case 0xDC: case 0xDD: case 0xDE: case 0xDF: {
                uint8_t modrm = fetch8(s);
                uint8_t regf = (modrm >> 3) & 7;
                bool is_reg;
                uint8_t rm;
                uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                uint32_t base = seg_base(s, segovr);
                bool mem = !is_reg;
                uint32_t maddr = base + ea;

                switch (op) {
                case 0xD8:
                    if (mem) {
                        double v = f32_to_double(mr32(maddr));
                        switch (regf) {
                            case 0: set_st_top(s, st_top(s) + v); break;
                            case 1: set_st_top(s, st_top(s) * v); break;
                            case 2: fpu_set_cmp_flags(s, st_top(s), v); s.fpu_sw = (s.fpu_sw & ~0x4500) | (s.fpu_sw & 0x4500); break;
                            case 3: fpu_set_cmp_flags(s, st_top(s), v); break;
                            case 4: set_st_top(s, st_top(s) - v); break;
                            case 5: set_st_top(s, v - st_top(s)); break;
                            case 6: set_st_top(s, st_top(s) / v); break;
                            case 7: set_st_top(s, v / st_top(s)); break;
                        }
                    } else {
                        switch (regf) {
                            case 0: set_st_top(s, st_top(s) + st_i(s, rm)); break;
                            case 1: set_st_top(s, st_top(s) * st_i(s, rm)); break;
                            case 2: fpu_set_cmp_flags(s, st_top(s), st_i(s, rm)); break;
                            case 3: fpu_set_cmp_flags(s, st_top(s), st_i(s, rm)); break;
                            case 4: set_st_top(s, st_top(s) - st_i(s, rm)); break;
                            case 5: set_st_top(s, st_i(s, rm) - st_top(s)); break;
                            case 6: set_st_top(s, st_top(s) / st_i(s, rm)); break;
                            case 7: set_st_top(s, st_i(s, rm) / st_top(s)); break;
                        }
                    }
                    break;

                case 0xD9:
                    if (mem) {
                        switch (regf) {
                            case 0: push_st(s, f32_to_double(mr32(maddr))); break;
                            case 2: mw32(maddr, double_to_f32_bits(st_top(s))); break;
                            case 3: { double v = st_top(s); pop_st(s); mw32(maddr, double_to_f32_bits(v)); break; }
                            case 5: s.fpu_cw = mr16(maddr); break;
                            case 7: mw16(maddr, s.fpu_cw); break;
                            case 6: case 4: break; // FNSTENV / FLDENV: no-op
                            case 1: break;
                            default: interp_fault(s, "x87 d9 mem"); break;
                        }
                    } else {
                        switch (modrm) {
                            case 0xE0: set_st_top(s, -st_top(s)); break; // FCHS
                            case 0xE1: set_st_top(s, fabs(st_top(s))); break; // FABS
                            case 0xE4: break; // FTST
                            case 0xE5: break; // FXAM
                            case 0xE8: push_st(s, 1.0); break; // FLD1
                            case 0xE9: push_st(s, log2(10.0) * 0); break; // FLDL2T
                            case 0xEA: push_st(s, log2(2.71828182845904523536)); break; // FLDL2E
                            case 0xEB: push_st(s, 3.14159265358979323846); break; // FLDPI
                            case 0xEC: push_st(s, 0.30102999566398119521); break; // FLDLG2
                            case 0xED: push_st(s, 0.69314718055994530942); break; // FLDLN2
                            case 0xEE: push_st(s, 0.0); break; // FLDZ
                            case 0xF0: set_st_top(s, pow(2.0, st_top(s)) - 1.0); break; // F2XM1
                            case 0xF1: { double y = st_i(s, 0), x = st_i(s, 1); set_st_top(s, y * log2(x)); break; } // FYL2X
                            case 0xF2: break; // FPTAN
                            case 0xF3: break; // FPATAN
                            case 0xF4: break; // FXTRACT
                            case 0xF5: break; // FPREM1
                            case 0xF6: s.fpu_top = (s.fpu_top + 1) & 7; break; // FDECSTP
                            case 0xF7: s.fpu_top = (s.fpu_top + 7) & 7; break; // FINCSTP
                            case 0xF8: break; // FPREM
                            case 0xF9: break; // FYL2XP1
                            case 0xFA: set_st_top(s, sqrt(st_top(s))); break; // FSQRT
                            case 0xFB: break; // FSINCOS
                            case 0xFC: set_st_top(s, round(st_top(s))); break; // FRNDINT
                            case 0xFD: { double m = st_i(s, 1); set_st_top(s, st_top(s) * pow(2.0, m)); break; } // FSCALE
                            case 0xFE: set_st_top(s, sin(st_top(s))); break; // FSIN
                            case 0xFF: set_st_top(s, cos(st_top(s))); break; // FCOS
                            default:
                                // register forms FLD/FXCH/FST/FSTP ST(i)
                                if (regf == 0) { push_st(s, st_i(s, rm)); } // FLD ST(i)
                                else if (regf == 1) { double t = st_i(s, rm); set_st_i(s, rm, st_top(s)); set_st_top(s, t); } // FXCH
                                else if (regf == 2) set_st_i(s, rm, st_top(s)); // FST ST(i)
                                else if (regf == 3) { set_st_i(s, rm, st_top(s)); pop_st(s); } // FSTP ST(i)
                                else interp_fault(s, "x87 d9 reg");
                                break;
                        }
                    }
                    break;

                case 0xDA:
                    if (mem) {
                        int32_t v = mr32(maddr);
                        switch (regf) {
                            case 0: set_st_top(s, st_top(s) + v); break;
                            case 1: set_st_top(s, st_top(s) * v); break;
                            case 2: fpu_set_cmp_flags(s, st_top(s), v); break;
                            case 3: fpu_set_cmp_flags(s, st_top(s), v); break;
                            case 4: set_st_top(s, st_top(s) - v); break;
                            case 5: set_st_top(s, v - st_top(s)); break;
                            case 6: set_st_top(s, st_top(s) / v); break;
                            case 7: set_st_top(s, v / st_top(s)); break;
                        }
                    } else {
                        if (modrm == 0xE9) { // FUCOMPP
                            double a = st_i(s, 0), b = st_i(s, 1);
                            fpu_set_cmp_flags(s, a, b);
                            pop_st(s); pop_st(s);
                        }
                    }
                    break;

                case 0xDB:
                    if (mem) {
                        switch (regf) {
                            case 0: push_st(s, (int32_t)mr32(maddr)); break; // FILD m32
                            case 2: set_st_top(s, (int32_t)mr32(maddr)); break; // FIST m32
                            case 3: { int32_t v = (int32_t)st_top(s); pop_st(s); mw32(maddr, (uint32_t)v); break; } // FISTP m32
                            case 5: case 7: break; // FLD/FSTP m80: unsupported, no-op
                            default: interp_fault(s, "x87 db mem"); break;
                        }
                    } else {
                        uint8_t idx = modrm & 7;
                        switch (regf) {
                            case 0: case 1: break; // FNENI/FNDISI
                            case 2: s.fpu_sw = 0; break; // FNCLEX
                            case 3: s.fpu_sw = 0; s.fpu_cw = 0x037F; break; // FNINIT
                            case 4: break; // FNSETPM
                            case 5: case 6: case 7: { // FUCOMI / FCOMI / FCOMIP st, st(i)
                                double a = st_top(s), b = st_i(s, idx);
                                uint16_t c0 = 0, c2 = 0, c3 = 0;
                                if (std::isnan(a) || std::isnan(b)) { c0 = c2 = c3 = 1; }
                                else if (a < b) c0 = 1;
                                else if (a == b) c3 = 1;
                                s.eflags = (s.eflags & ~(F_CF | F_PF | F_ZF)) | (c0 ? F_CF : 0) | (c2 ? F_PF : 0) | (c3 ? F_ZF : 0);
                                s.fpu_sw = (s.fpu_sw & ~0x4500) | (c0 ? 0x100 : 0) | (c2 ? 0x400 : 0) | (c3 ? 0x4000 : 0);
                                if (regf == 7) pop_st(s); // FCOMIP pops
                                break; }
                            default: interp_fault(s, "x87 db reg"); break;
                        }
                    }
                    break;

                case 0xDC:
                    if (mem) {
                        double v = f64_to_double(mr64(maddr));
                        switch (regf) {
                            case 0: set_st_top(s, st_top(s) + v); break;
                            case 1: set_st_top(s, st_top(s) * v); break;
                            case 2: fpu_set_cmp_flags(s, st_top(s), v); break;
                            case 3: fpu_set_cmp_flags(s, st_top(s), v); break;
                            case 4: set_st_top(s, st_top(s) - v); break;
                            case 5: set_st_top(s, v - st_top(s)); break;
                            case 6: set_st_top(s, st_top(s) / v); break;
                            case 7: set_st_top(s, v / st_top(s)); break;
                        }
                    } else {
                        double dst = st_i(s, rm);
                        switch (regf) {
                            case 0: set_st_i(s, rm, dst + st_top(s)); break;
                            case 1: set_st_i(s, rm, dst * st_top(s)); break;
                            case 2: fpu_set_cmp_flags(s, dst, st_top(s)); break;
                            case 3: fpu_set_cmp_flags(s, dst, st_top(s)); break;
                            case 4: set_st_i(s, rm, st_top(s) - dst); break;
                            case 5: set_st_i(s, rm, dst - st_top(s)); break;
                            case 6: set_st_i(s, rm, st_top(s) / dst); break;
                            case 7: set_st_i(s, rm, dst / st_top(s)); break;
                        }
                    }
                    break;

                case 0xDD:
                    if (mem) {
                        switch (regf) {
                            case 0: push_st(s, f64_to_double(mr64(maddr))); break;
                            case 2: mw64(maddr, double_to_f64_bits(st_top(s))); break;
                            case 3: { double v = st_top(s); pop_st(s); mw64(maddr, double_to_f64_bits(v)); break; }
                            case 7: mw16(maddr, s.fpu_sw); break; // FNSTSW m16
                            case 6: case 4: case 5: case 1: break;
                            default: interp_fault(s, "x87 dd mem"); break;
                        }
                    } else {
                        switch (regf) {
                            case 0: set_st_i(s, rm, 0); break; // FFREE
                            case 2: set_st_i(s, rm, st_top(s)); break; // FST ST(i)
                            case 3: { set_st_i(s, rm, st_top(s)); pop_st(s); break; } // FSTP ST(i)
                            case 4: fpu_set_cmp_flags(s, st_top(s), st_i(s, rm)); break; // FUCOM
                            case 5: fpu_set_cmp_flags(s, st_top(s), st_i(s, rm)); break; // FUCOMP
                            default: interp_fault(s, "x87 dd reg"); break;
                        }
                    }
                    break;

                case 0xDE:
                    if (mem) {
                        int16_t v = mr16(maddr);
                        switch (regf) {
                            case 0: set_st_top(s, st_top(s) + v); break;
                            case 1: set_st_top(s, st_top(s) * v); break;
                            case 2: fpu_set_cmp_flags(s, st_top(s), v); break;
                            case 3: fpu_set_cmp_flags(s, st_top(s), v); break;
                            case 4: set_st_top(s, st_top(s) - v); break;
                            case 5: set_st_top(s, v - st_top(s)); break;
                            case 6: set_st_top(s, st_top(s) / v); break;
                            case 7: set_st_top(s, v / st_top(s)); break;
                        }
                    } else {
                        double a = st_i(s, rm);
                        switch (regf) {
                            case 0: set_st_i(s, rm, a + st_top(s)); pop_st(s); break; // FADDP
                            case 1: set_st_i(s, rm, a * st_top(s)); pop_st(s); break; // FMULP
                            case 2: fpu_set_cmp_flags(s, st_top(s), a); break; // FCOMP (no pop)
                            case 3: fpu_set_cmp_flags(s, st_top(s), a); pop_st(s); pop_st(s); break; // FCOMPP
                            case 4: set_st_i(s, rm, st_top(s) - a); pop_st(s); break; // FSUBRP
                            case 5: set_st_i(s, rm, a - st_top(s)); pop_st(s); break; // FSUBP
                            case 6: set_st_i(s, rm, st_top(s) / a); pop_st(s); break; // FDIVRP
                            case 7: set_st_i(s, rm, a / st_top(s)); pop_st(s); break; // FDIVP
                        }
                    }
                    break;

                case 0xDF:
                    if (mem) {
                        switch (regf) {
                            case 0: push_st(s, (int16_t)mr16(maddr)); break; // FILD m16
                            case 2: set_st_top(s, (int16_t)mr16(maddr)); break; // FIST m16
                            case 3: { int16_t v = (int16_t)st_top(s); pop_st(s); mw16(maddr, (uint16_t)v); break; } // FISTP m16
                            case 5: case 7: case 4: case 6: case 1: break;
                            default: interp_fault(s, "x87 df mem"); break;
                        }
                    } else {
                        // register forms 0xD8–0xDF handled by the outer register-mode
                        // handlers below (FNSTSW AX, FUCOMPP, etc.)
                    }
                    break;
                }
                if (op == 0xDF && !mem && modrm >= 0xE0) {
                    if (modrm == 0xE0) set_reg16(s, XR_EAX, (uint16_t)(s.regs[XR_EAX] & 0xFFFF0000u) | s.fpu_sw);
                    else if (modrm >= 0xE8) { // FUCOMIP (E8-EF) / FCOMIP (F0-F7)
                        uint8_t idx = modrm & 7;
                        double a = st_top(s), b = st_i(s, idx);
                        uint16_t c0 = 0, c2 = 0, c3 = 0;
                        if (std::isnan(a) || std::isnan(b)) { c0 = c2 = c3 = 1; }
                        else if (a < b) c0 = 1;
                        else if (a == b) c3 = 1;
                        s.eflags = (s.eflags & ~(F_CF | F_PF | F_ZF)) | (c0 ? F_CF : 0) | (c2 ? F_PF : 0) | (c3 ? F_ZF : 0);
                        s.fpu_sw = (s.fpu_sw & ~0x4500) | (c0 ? 0x100 : 0) | (c2 ? 0x400 : 0) | (c3 ? 0x4000 : 0);
                        pop_st(s); // FCOMIP/FUCOMIP pop
                    }
                }
                break;
            }

            // ---- LOOP / JCXZ ----
            case 0xE0: case 0xE1: case 0xE2: { // LOOPNE/LOOPE/LOOP
                int8_t rel = static_cast<int8_t>(fetch8(s));
                uint32_t target = s.eip + static_cast<int32_t>(rel);
                s.regs[XR_ECX]--;
                if (s.regs[XR_ECX] != 0) {
                    bool cond = false;
                    if (op == 0xE2) cond = true;
                    else if (op == 0xE1) cond = (s.eflags & F_ZF) != 0; // LOOPE
                    else cond = (s.eflags & F_ZF) == 0; // LOOPNE
                    if (cond) s.eip = target;
                }
                break;
            }
            case 0xE3: { // JCXZ/JECXZ
                int8_t rel = static_cast<int8_t>(fetch8(s));
                uint32_t target = s.eip + static_cast<int32_t>(rel);
                if (s.regs[XR_ECX] == 0) s.eip = target;
                break;
            }

            case 0xE4: case 0xE5: case 0xEC: case 0xED: case 0xE6: case 0xE7:
            case 0xEE: case 0xEF: { // IN/OUT: no-op
                if (op == 0xE4 || op == 0xE5) (void)fetch8(s);
                break;
            }

            case 0xE8: { // CALL rel32
                int32_t rel = static_cast<int32_t>(fetch32(s));
                uint32_t target = s.eip + static_cast<uint32_t>(rel);
                if (is_api_thunk_addr(target)) {
                    s.eip = x86_dispatch_api(s, target, s.eip);
                } else {
                    push32(s, s.eip);
                    s.eip = target;
                }
                break;
            }
            case 0xE9: { // JMP rel32
                int32_t rel = static_cast<int32_t>(fetch32(s));
                uint32_t target = s.eip + static_cast<uint32_t>(rel);
                if (is_api_thunk_addr(target)) {
                    s.eip = x86_dispatch_api(s, target, pop32(s));
                } else {
                    s.eip = target;
                }
                break;
            }
            case 0xEB: { // JMP rel8
                int8_t rel = static_cast<int8_t>(fetch8(s));
                s.eip += static_cast<int32_t>(rel);
                break;
            }

            case 0xF1: { // INT1
                interp_fault(s, "int1");
                break;
            }
            case 0xF4: { // HLT
                s.halted = true;
                break;
            }
            case 0xF5: { // CMC
                s.eflags ^= F_CF;
                break;
            }
            case 0xF6: case 0xF7: { // Group 3
                uint8_t modrm = fetch8(s);
                uint8_t opf = (modrm >> 3) & 7;
                bool is_reg; uint8_t rm;
                uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                uint32_t base = seg_base(s, segovr);
                if (op == 0xF6) {
                    uint8_t v = is_reg ? read_reg8(s, rm) : mr8(base + ea);
                    switch (opf) {
                        case 0: { uint8_t t = fetch8(s); test8(s, v, t); break; }
                        case 2: { uint8_t r = (uint8_t)~v; if (is_reg) write_reg8(s, rm, r); else mw8(base + ea, r); s.eflags = (s.eflags & ~F_ARITH) | ((r == 0) ? F_ZF : 0) | ((r & 0x80) ? F_SF : 0) | parity_flag(r); break; } // NOT
                        case 3: { uint8_t r = (uint8_t)(-v); if (is_reg) write_reg8(s, rm, r); else mw8(base + ea, r); uint8_t zero = 0; sub8(s, zero, v); break; } // NEG
                        case 4: { uint8_t r = (uint8_t)(v * reg8_lo(s, XR_EAX)); set_reg16(s, XR_EAX, (uint16_t)((r << 8) | reg8_lo(s, XR_EAX))); uint8_t ah = (uint8_t)r; s.eflags = (s.eflags & ~(F_CF | F_OF)) | ((ah != 0) ? (F_CF | F_OF) : 0); break; } // MUL
                        case 5: { int8_t a = (int8_t)reg8_lo(s, XR_EAX); int8_t b = (int8_t)v; int16_t r = (int16_t)(a * b); set_reg16(s, XR_EAX, (uint16_t)r); uint8_t ah = (uint8_t)(r >> 8); s.eflags = (s.eflags & ~(F_CF | F_OF)) | (((int8_t)ah == (r >> 8) && (r >> 8) != ah) || ah != (int8_t)ah ? (F_CF | F_OF) : 0); break; } // IMUL
                        case 6: { // DIV
                            uint16_t ax = reg16(s, XR_EAX);
                            if (v == 0) { interp_fault(s, "div by zero"); break; }
                            uint8_t q = (uint8_t)(ax / v), rem = (uint8_t)(ax % v);
                            if (ax / v > 0xFF) { interp_fault(s, "div overflow"); break; }
                            set_reg8_lo(s, XR_EAX, q);
                            set_reg8_hi(s, XR_EAX, rem);
                            break;
                        }
                        case 7: { // IDIV
                            int16_t ax = (int16_t)reg16(s, XR_EAX);
                            int8_t dv = (int8_t)v;
                            if (dv == 0) { interp_fault(s, "idiv by zero"); break; }
                            int8_t q = (int8_t)(ax / dv), rem = (int8_t)(ax % dv);
                            if (q == -128 && ax == -32768) { /* OK special */ }
                            set_reg8_lo(s, XR_EAX, (uint8_t)q);
                            set_reg8_hi(s, XR_EAX, (uint8_t)rem);
                            break;
                        }
                        default: interp_fault(s, "group3 f6"); break;
                    }
                } else {
                    uint32_t vraw = is_reg ? s.regs[rm] : mr32(base + ea);
                    uint32_t v = opsize16 ? (vraw & 0xFFFF) : vraw;
                    switch (opf) {
                        case 0: { // TEST r/m, imm (operand is the r/m, not EAX)
                            if (opsize16) {
                                uint32_t t = fetch16(s);
                                uint16_t r = (uint16_t)((uint16_t)v & (uint16_t)t);
                                s.eflags = (s.eflags & ~F_ARITH) | ((r == 0) ? F_ZF : 0) | ((r & 0x8000) ? F_SF : 0) | parity_flag(r);
                            } else {
                                uint32_t t = fetch32(s);
                                test32(s, v, t);
                            }
                            break;
                        }
                        case 2: { // NOT
                            uint32_t r = ~v;
                            if (opsize16) {
                                uint16_t r16 = (uint16_t)r;
                                if (is_reg) s.regs[rm] = (s.regs[rm] & 0xFFFF0000) | r16;
                                else mw16(base + ea, r16);
                                s.eflags = (s.eflags & ~F_ARITH) | ((r16 == 0) ? F_ZF : 0) | ((r16 & 0x8000) ? F_SF : 0) | parity_flag(r16);
                            } else {
                                if (is_reg) s.regs[rm] = r; else mw32(base + ea, r);
                                s.eflags = (s.eflags & ~F_ARITH) | ((r == 0) ? F_ZF : 0) | ((r & 0x80000000) ? F_SF : 0) | parity_flag(r);
                            }
                            break;
                        }
                        case 3: { // NEG
                            uint32_t r = (uint32_t)(-(int64_t)v);
                            if (opsize16) {
                                uint16_t r16 = (uint16_t)r;
                                if (is_reg) s.regs[rm] = (s.regs[rm] & 0xFFFF0000) | r16;
                                else mw16(base + ea, r16);
                                s.eflags = (s.eflags & ~F_ARITH) | ((r16 == 0) ? F_ZF : 0) | ((r16 & 0x8000) ? F_SF : 0) | parity_flag(r16) | (v ? F_CF : 0) | ((v & 0xF) ? F_AF : 0);
                            } else {
                                if (is_reg) s.regs[rm] = r; else mw32(base + ea, r);
                                s.eflags = (s.eflags & ~F_ARITH) | ((r == 0) ? F_ZF : 0) | ((r & 0x80000000) ? F_SF : 0) | parity_flag(r) | (v ? F_CF : 0) | ((v & 0xF) ? F_AF : 0);
                            }
                            break;
                        }
                        case 4: { // MUL
                            if (opsize16) {
                                uint32_t r = (uint32_t)(uint16_t)s.regs[XR_EAX] * v;
                                set_reg16(s, XR_EAX, (uint16_t)r);
                                set_reg16(s, XR_EDX, (uint16_t)(r >> 16));
                                s.eflags = (s.eflags & ~(F_CF | F_OF)) | ((r >> 16) ? (F_CF | F_OF) : 0);
                            } else {
                                uint64_t r = (uint64_t)v * (uint64_t)s.regs[XR_EAX];
                                uint32_t hi = (uint32_t)(r >> 32);
                                s.regs[XR_EDX] = hi;
                                s.regs[XR_EAX] = (uint32_t)r;
                                s.eflags = (s.eflags & ~(F_CF | F_OF)) | (hi ? (F_CF | F_OF) : 0);
                            }
                            break;
                        }
                        case 5: { // IMUL
                            if (opsize16) {
                                int32_t r = (int16_t)s.regs[XR_EAX] * (int16_t)v;
                                set_reg16(s, XR_EAX, (uint16_t)r);
                                set_reg16(s, XR_EDX, (uint16_t)((uint32_t)r >> 16));
                                s.eflags = (s.eflags & ~(F_CF | F_OF)) | ((r != (int32_t)(int16_t)r) ? (F_CF | F_OF) : 0);
                            } else {
                                int64_t r = (int64_t)(int32_t)v * (int64_t)(int32_t)s.regs[XR_EAX];
                                uint32_t lo = (uint32_t)r, hi = (uint32_t)((uint64_t)r >> 32);
                                s.regs[XR_EDX] = hi;
                                s.regs[XR_EAX] = lo;
                                s.eflags = (s.eflags & ~(F_CF | F_OF)) | ((r != (int64_t)(int32_t)lo) ? (F_CF | F_OF) : 0);
                            }
                            break;
                        }
                        case 6: { // DIV
                            if (opsize16) {
                                uint32_t num = (uint16_t)s.regs[XR_EAX] | ((uint16_t)s.regs[XR_EDX] << 16);
                                if (v == 0) { interp_fault(s, "div by zero"); break; }
                                if (num / v > 0xFFFF) { interp_fault(s, "div overflow"); break; }
                                set_reg16(s, XR_EAX, (uint16_t)((num / v) & 0xFFFF));
                                set_reg16(s, XR_EDX, (uint16_t)((num % v) & 0xFFFF));
                            } else {
                                uint64_t vv = v;
                                uint64_t num = ((uint64_t)s.regs[XR_EDX] << 32) | s.regs[XR_EAX];
                                if (vv == 0) { interp_fault(s, "div by zero"); break; }
                                uint32_t q = (uint32_t)(num / vv), rem = (uint32_t)(num % vv);
                                s.regs[XR_EAX] = q;
                                s.regs[XR_EDX] = rem;
                            }
                            break;
                        }
                        case 7: { // IDIV
                            if (opsize16) {
                                int32_t num = (int16_t)s.regs[XR_EAX] | ((int32_t)(int16_t)s.regs[XR_EDX] << 16);
                                int16_t dv = (int16_t)v;
                                if (dv == 0) { interp_fault(s, "idiv by zero"); break; }
                                int32_t q = num / dv, rem = num % dv;
                                if (q > INT16_MAX || q < INT16_MIN) { interp_fault(s, "idiv overflow"); break; }
                                set_reg16(s, XR_EAX, (uint16_t)q);
                                set_reg16(s, XR_EDX, (uint16_t)rem);
                            } else {
                                int64_t vv = (int32_t)v;
                                if (vv == 0) { interp_fault(s, "idiv by zero"); break; }
                                int64_t num = ((int64_t)(int32_t)s.regs[XR_EDX] << 32) | (int32_t)s.regs[XR_EAX];
                                if (num == INT64_MIN && vv == -1) { s.regs[XR_EAX] = 0x80000000; s.regs[XR_EDX] = 0; break; }
                                if (num / vv > INT32_MAX || num / vv < INT32_MIN) { interp_fault(s, "idiv overflow"); break; }
                                s.regs[XR_EAX] = (uint32_t)(num / vv);
                                s.regs[XR_EDX] = (uint32_t)(num % vv);
                            }
                            break;
                        }
                        default: interp_fault(s, "group3 f7"); break;
                    }
                }
                break;
            }

            case 0xF8: s.eflags &= ~F_CF; break; // CLC
            case 0xF9: s.eflags |= F_CF; break; // STC
            case 0xFA: s.eflags &= ~F_IF; break; // CLI
            case 0xFB: s.eflags |= F_IF; break; // STI
            case 0xFC: s.eflags &= ~F_DF; break; // CLD
            case 0xFD: s.eflags |= F_DF; break; // STD

            case 0xFE: { // Group 4: INC/DEC Eb
                uint8_t modrm = fetch8(s);
                uint8_t opf = (modrm >> 3) & 7;
                bool is_reg; uint8_t rm;
                uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                uint32_t base = seg_base(s, segovr);
                uint8_t v = is_reg ? read_reg8(s, rm) : mr8(base + ea);
                s.eflags &= ~F_ARITH;
                if (opf == 0) { uint8_t r = (uint8_t)(v + 1); set_arith_flags<uint8_t>(s, v, 1, r, true, false); v = r; }
                else { uint8_t r = (uint8_t)(v - 1); set_arith_flags<uint8_t>(s, v, 1, r, false, true); v = r; }
                s.eflags &= ~F_CF;
                if (is_reg) write_reg8(s, rm, v); else mw8(base + ea, v);
                break;
            }

            case 0xFF: { // Group 5
                uint8_t modrm = fetch8(s);
                uint8_t opf = (modrm >> 3) & 7;
                bool is_reg; uint8_t rm;
                uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                uint32_t base = seg_base(s, segovr);
                uint32_t v = is_reg ? s.regs[rm] : mr32(base + ea);
                switch (opf) {
                    case 0: { uint32_t r = v + 1; s.eflags &= ~F_ARITH; set_arith_flags<uint32_t>(s, v, 1, r, true, false); s.eflags &= ~F_CF; if (is_reg) s.regs[rm] = r; else mw32(base + ea, r); break; }
                    case 1: { uint32_t r = v - 1; s.eflags &= ~F_ARITH; set_arith_flags<uint32_t>(s, v, 1, r, false, true); s.eflags &= ~F_CF; if (is_reg) s.regs[rm] = r; else mw32(base + ea, r); break; }
                    case 2: { // CALL r/m
                        if (getenv("THETAZ_TRACE_INDCALL")) {
                            fprintf(stderr, "[indcall] call target=0x%08x from=0x%08x src=%s reg=%u mem=0x%08x esi=0x%08x edi=0x%08x esp=0x%08x\n",
                                    v, s.eip, is_reg ? "reg" : "mem", rm, base + ea,
                                    s.regs[XR_ESI], s.regs[XR_EDI], s.regs[XR_ESP]);
                        }
                        // A control transfer to an address that is neither an
                        // API thunk nor inside any mapped guest region (images
                        // below 0x26000000, the guest heap, or a thread stack)
                        // is a wild jump. Report it with the full register set
                        // so the pointer can be traced to the API that made it.
                        if (!is_api_thunk_addr(v) && !guest_addr_may_hold_code(v) &&
                            !is_callback_sentinel(v)) {
                            report_wild_transfer("call", v, s,
                                                 is_reg ? "reg" : "mem", rm, base + ea);
                        }
                        if (is_api_thunk_addr(v)) { s.eip = x86_dispatch_api(s, v, s.eip); }
                        else { push32(s, s.eip); s.eip = v; }
                        break;
                    }
                    case 4: { // JMP r/m
                        if (is_api_thunk_addr(v)) { s.eip = x86_dispatch_api(s, v, pop32(s)); }
                        else s.eip = v;
                        break;
                    }
                    case 6: { // PUSH r/m
                        if (opsize16) push16(s, (uint16_t)v);
                        else push32(s, v);
                        break;
                    }
                    case 3: case 5: case 7: { // CALLF / JMPF / reserved
                        interp_fault(s, "far call/jmp");
                        break;
                    }
                    default: interp_fault(s, "group5"); break;
                }
                break;
            }

            case 0x0F: { // two-byte opcodes
                uint8_t op2 = fetch8(s);
                switch (op2) {
                    case 0x40: case 0x41: case 0x42: case 0x43:
                    case 0x44: case 0x45: case 0x46: case 0x47:
                    case 0x48: case 0x49: case 0x4A: case 0x4B:
                    case 0x4C: case 0x4D: case 0x4E: case 0x4F: { // CMOVcc
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        bool cond = false;
                        switch (op2 & 0x0F) {
                            case 0x0: cond = flag_of(s); break;
                            case 0x1: cond = !flag_of(s); break;
                            case 0x2: cond = flag_cf(s); break;
                            case 0x3: cond = !flag_cf(s); break;
                            case 0x4: cond = flag_zf(s); break;
                            case 0x5: cond = !flag_zf(s); break;
                            case 0x6: cond = flag_cf(s) || flag_zf(s); break;
                            case 0x7: cond = !flag_cf(s) && !flag_zf(s); break;
                            case 0x8: cond = flag_sf(s); break;
                            case 0x9: cond = !flag_sf(s); break;
                            case 0xA: cond = flag_pf(s); break;
                            case 0xB: cond = !flag_pf(s); break;
                            case 0xC: cond = flag_sf(s) != flag_of(s); break;
                            case 0xD: cond = flag_sf(s) == flag_of(s); break;
                            case 0xE: cond = flag_zf(s) || (flag_sf(s) != flag_of(s)); break;
                            case 0xF: cond = !flag_zf(s) && (flag_sf(s) == flag_of(s)); break;
                        }
                        if (cond) {
                            uint32_t src = is_reg ? s.regs[rm] : mr32(base + ea);
                            if (opsize16) set_reg16(s, regf, (uint16_t)src);
                            else s.regs[regf] = src;
                        }
                        break;
                    }

                    case 0x80: case 0x81: case 0x82: case 0x83:
                    case 0x84: case 0x85: case 0x86: case 0x87:
                    case 0x88: case 0x89: case 0x8A: case 0x8B:
                    case 0x8C: case 0x8D: case 0x8E: case 0x8F: { // Jcc rel32
                        int32_t rel = (int32_t)fetch32(s);
                        uint32_t target = s.eip + (uint32_t)rel;
                        bool taken = false;
                        switch (op2 & 0x0F) {
                            case 0x0: taken = flag_of(s); break;
                            case 0x1: taken = !flag_of(s); break;
                            case 0x2: taken = flag_cf(s); break;
                            case 0x3: taken = !flag_cf(s); break;
                            case 0x4: taken = flag_zf(s); break;
                            case 0x5: taken = !flag_zf(s); break;
                            case 0x6: taken = flag_cf(s) || flag_zf(s); break;
                            case 0x7: taken = !flag_cf(s) && !flag_zf(s); break;
                            case 0x8: taken = flag_sf(s); break;
                            case 0x9: taken = !flag_sf(s); break;
                            case 0xA: taken = flag_pf(s); break;
                            case 0xB: taken = !flag_pf(s); break;
                            case 0xC: taken = flag_sf(s) != flag_of(s); break;
                            case 0xD: taken = flag_sf(s) == flag_of(s); break;
                            case 0xE: taken = flag_zf(s) || (flag_sf(s) != flag_of(s)); break;
                            case 0xF: taken = !flag_zf(s) && (flag_sf(s) == flag_of(s)); break;
                        }
                        if (taken) s.eip = target;
                        break;
                    }

                    case 0x1F: { // Multi-byte NOP (0F 1F /0)
                        uint8_t modrm = fetch8(s);
                        bool is_reg; uint8_t rm;
                        (void)decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        break;
                    }
                    case 0x31: { // RDTSC
                        struct timespec ts;
                        clock_gettime(CLOCK_MONOTONIC, &ts);
                        uint64_t t = static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL + ts.tv_nsec;
                        s.regs[XR_EAX] = static_cast<uint32_t>(t);
                        s.regs[XR_EDX] = static_cast<uint32_t>(t >> 32);
                        break;
                    }

                    case 0x90: case 0x91: case 0x92: case 0x93:
                    case 0x94: case 0x95: case 0x96: case 0x97:
                    case 0x98: case 0x99: case 0x9A: case 0x9B:
                    case 0x9C: case 0x9D: case 0x9E: case 0x9F: { // SETcc
                        uint8_t modrm = fetch8(s);
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        bool cond = false;
                        switch (op2 & 0x0F) {
                            case 0x0: cond = flag_of(s); break;
                            case 0x1: cond = !flag_of(s); break;
                            case 0x2: cond = flag_cf(s); break;
                            case 0x3: cond = !flag_cf(s); break;
                            case 0x4: cond = flag_zf(s); break;
                            case 0x5: cond = !flag_zf(s); break;
                            case 0x6: cond = flag_cf(s) || flag_zf(s); break;
                            case 0x7: cond = !flag_cf(s) && !flag_zf(s); break;
                            case 0x8: cond = flag_sf(s); break;
                            case 0x9: cond = !flag_sf(s); break;
                            case 0xA: cond = flag_pf(s); break;
                            case 0xB: cond = !flag_pf(s); break;
                            case 0xC: cond = flag_sf(s) != flag_of(s); break;
                            case 0xD: cond = flag_sf(s) == flag_of(s); break;
                            case 0xE: cond = flag_zf(s) || (flag_sf(s) != flag_of(s)); break;
                            case 0xF: cond = !flag_zf(s) && (flag_sf(s) == flag_of(s)); break;
                        }
                        if (is_reg) write_reg8(s, rm, cond ? 1 : 0);
                        else mw8(base + ea, cond ? 1 : 0);
                        break;
                    }

                    case 0xA0: push32(s, s.seg[4]); break; // PUSH FS
                    case 0xA1: s.seg[4] = (uint16_t)pop32(s); break; // POP FS
                    case 0xA2: { // CPUID
                        switch (s.regs[XR_EAX]) {
                            case 0:
                                s.regs[XR_EAX] = 1;
                                s.regs[XR_EBX] = 0x756E6547u; // "Genu"
                                s.regs[XR_EDX] = 0x49656E69u; // "ineI"
                                s.regs[XR_ECX] = 0x6C65746Eu; // "ntel"
                                break;
                            case 1:
                                s.regs[XR_EAX] = 0x000006A6u; // family 6, model 0xA, stepping 6
                                s.regs[XR_EBX] = 0x00000800u;
                                s.regs[XR_ECX] = 0x00000001u; // SSE3
                                s.regs[XR_EDX] = 0x00000000u; // clear in case a CPUID leaf 1 sets them
                                s.regs[XR_EDX] = 0x1FBBFBFu;  // FPU/MMX/SSE/etc.
                                break;
                            case 0x80000000u:
                                s.regs[XR_EAX] = 0x80000001u;
                                s.regs[XR_EBX] = 0;
                                s.regs[XR_ECX] = 0;
                                s.regs[XR_EDX] = 0;
                                break;
                            case 0x80000001u:
                                s.regs[XR_EAX] = 0;
                                s.regs[XR_EBX] = 0;
                                s.regs[XR_ECX] = 0;
                                s.regs[XR_EDX] = 0x20100800u;
                                break;
                            default:
                                s.regs[XR_EAX] = 0;
                                s.regs[XR_EBX] = 0;
                                s.regs[XR_ECX] = 0;
                                s.regs[XR_EDX] = 0;
                                break;
                        }
                        break;
                    }
                    case 0xA3: { // BT
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        uint32_t v = is_reg ? s.regs[rm] : mr32(base + ea);
                        uint32_t bit = s.regs[regf] & 31;
                        s.eflags = (s.eflags & ~F_CF) | ((v >> bit) & 1 ? F_CF : 0);
                        break;
                    }
                    case 0xA8: push32(s, s.seg[5]); break; // PUSH GS
                    case 0xA9: s.seg[5] = (uint16_t)pop32(s); break; // POP GS
                    case 0xAB: { // BTS
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        uint32_t bit = s.regs[regf] & 31;
                        uint32_t v = is_reg ? s.regs[rm] : mr32(base + ea);
                        s.eflags = (s.eflags & ~F_CF) | ((v >> bit) & 1 ? F_CF : 0);
                        v |= (1u << bit);
                        if (is_reg) s.regs[rm] = v; else mw32(base + ea, v);
                        break;
                    }
                    case 0xAF: { // IMUL r32, r/m32
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        int32_t a = (int32_t)s.regs[regf];
                        int32_t b = (int32_t)(is_reg ? s.regs[rm] : mr32(base + ea));
                        int64_t r = (int64_t)a * b;
                        int32_t r32 = (int32_t)r;
                        s.regs[regf] = (uint32_t)r32;
                        s.eflags = (s.eflags & ~(F_CF | F_OF)) | ((r != (int64_t)r32) ? (F_CF | F_OF) : 0);
                        break;
                    }
                    case 0xB3: { // BTR
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        uint32_t bit = s.regs[regf] & 31;
                        uint32_t v = is_reg ? s.regs[rm] : mr32(base + ea);
                        s.eflags = (s.eflags & ~F_CF) | ((v >> bit) & 1 ? F_CF : 0);
                        v &= ~(1u << bit);
                        if (is_reg) s.regs[rm] = v; else mw32(base + ea, v);
                        break;
                    }
                    case 0xB6: case 0xB7: { // MOVZX
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        if (op2 == 0xB6) {
                            uint8_t v = is_reg ? read_reg8(s, rm) : mr8(base + ea);
                            if (opsize16) set_reg16(s, regf, v);
                            else s.regs[regf] = v;
                        } else {
                            uint16_t v = is_reg ? reg16(s, rm) : mr16(base + ea);
                            if (opsize16) set_reg16(s, regf, v);
                            else s.regs[regf] = v;
                        }
                        break;
                    }
                    case 0xBA: { // Group 8: BT/BTS/BTR/BTC imm8
                        uint8_t modrm = fetch8(s);
                        uint8_t opf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        uint8_t bit = fetch8(s) & 31;
                        uint32_t v = is_reg ? s.regs[rm] : mr32(base + ea);
                        s.eflags = (s.eflags & ~F_CF) | ((v >> bit) & 1 ? F_CF : 0);
                        if (opf == 1) v |= (1u << bit);
                        else if (opf == 2) v &= ~(1u << bit);
                        else if (opf == 3) v ^= (1u << bit);
                        if (opf != 0 && opf != 4) { if (is_reg) s.regs[rm] = v; else mw32(base + ea, v); }
                        break;
                    }
                    case 0xBB: { // BTC
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        uint32_t bit = s.regs[regf] & 31;
                        uint32_t v = is_reg ? s.regs[rm] : mr32(base + ea);
                        s.eflags = (s.eflags & ~F_CF) | ((v >> bit) & 1 ? F_CF : 0);
                        v ^= (1u << bit);
                        if (is_reg) s.regs[rm] = v; else mw32(base + ea, v);
                        break;
                    }
                    case 0xBC: case 0xBD: { // BSF / BSR (F3 = TZCNT / LZCNT)
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        uint32_t v = is_reg ? s.regs[rm] : mr32(base + ea);
                        bool is_cnt = repz; // F3 prefix: TZCNT/LZCNT
                        if (v == 0) {
                            s.eflags |= F_ZF;
                            if (is_cnt) { s.eflags |= F_CF; s.regs[regf] = opsize16 ? 16 : 32; }
                        } else {
                            s.eflags &= ~F_ZF;
                            if (is_cnt) s.eflags &= ~F_CF;
                            uint32_t idx = 0;
                            if (op2 == 0xBC) { uint32_t w = v; while (!(w & 1)) { w >>= 1; idx++; } }
                            else { for (uint32_t i = 31; i != 0xFFFFFFFF; i--) if (v & (1u << i)) { idx = i; break; } }
                            if (is_cnt && op2 == 0xBD) idx = (opsize16 ? 15 : 31) - idx; // LZCNT: count, not bit position
                            if (opsize16) set_reg16(s, regf, (uint16_t)idx);
                            else s.regs[regf] = idx;
                        }
                        break;
                    }
                    case 0xBE: case 0xBF: { // MOVSX
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        if (op2 == 0xBE) {
                            int8_t v = (int8_t)(is_reg ? read_reg8(s, rm) : mr8(base + ea));
                            if (opsize16) set_reg16(s, regf, (uint16_t)(int16_t)v);
                            else s.regs[regf] = (uint32_t)(int32_t)v;
                        } else {
                            int16_t v = (int16_t)(is_reg ? reg16(s, rm) : mr16(base + ea));
                            if (opsize16) set_reg16(s, regf, (uint16_t)v);
                            else s.regs[regf] = (uint32_t)(int32_t)v;
                        }
                        break;
                    }
                    case 0xC0: case 0xC1: { // XADD
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        if (op2 == 0xC0) {
                            uint8_t dst = is_reg ? read_reg8(s, rm) : mr8(base + ea);
                            uint8_t r = static_cast<uint8_t>(dst + read_reg8(s, regf));
                            set_arith_flags<uint8_t>(s, dst, read_reg8(s, regf), r, true, false);
                            write_reg8(s, regf, dst);
                            if (is_reg) write_reg8(s, rm, r); else mw8(base + ea, r);
                            break;
                        }
                        uint32_t dst = is_reg ? s.regs[rm] : mr32(base + ea);
                        uint32_t r = dst + s.regs[regf];
                        set_arith_flags<uint32_t>(s, dst, s.regs[regf], r, true, false);
                        s.regs[regf] = dst;
                        if (is_reg) s.regs[rm] = r; else mw32(base + ea, r);
                        break;
                    }
                    case 0xB0: case 0xB1: { // CMPXCHG r/m, reg
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        if (op2 == 0xB0) {
                            uint8_t a = reg8_lo(s, XR_EAX);
                            uint8_t dst = is_reg ? read_reg8(s, rm) : mr8(base + ea);
                            cmp8(s, a, dst);
                            if (a == dst) {
                                if (is_reg) write_reg8(s, rm, read_reg8(s, regf));
                                else mw8(base + ea, read_reg8(s, regf));
                            } else {
                                set_reg8_lo(s, XR_EAX, dst);
                            }
                        } else {
                            uint32_t a = s.regs[XR_EAX];
                            if (opsize16) {
                                uint16_t dst = is_reg ? reg16(s, rm) : mr16(base + ea);
                                cmp16(s, (uint16_t)a, dst);
                                if ((uint16_t)a == dst) {
                                    uint16_t src = reg16(s, regf);
                                    if (is_reg) set_reg16(s, rm, src);
                                    else mw16(base + ea, src);
                                } else {
                                    set_reg16(s, XR_EAX, dst);
                                }
                            } else {
                                uint32_t dst = is_reg ? s.regs[rm] : mr32(base + ea);
                                cmp32(s, a, dst);
                                if (a == dst) {
                                    if (is_reg) s.regs[rm] = s.regs[regf];
                                    else mw32(base + ea, s.regs[regf]);
                            } else {
                                s.regs[XR_EAX] = dst;
                            }
                        }
                        }
                        break;
                    }
                    case 0xC7: { // CMPXCHG8B m64 (group 9 /1)
                        {
                            uint8_t modrm = fetch8(s);
                            uint8_t regf = (modrm >> 3) & 7;
                            bool is_reg; uint8_t rm;
                            uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                            uint32_t base = seg_base(s, segovr);
                            if (regf != 1 || is_reg) { interp_fault(s, "cmpxchg8b"); break; }
                            uint32_t maddr = base + ea;
                            uint64_t dst = mr64(maddr);
                            uint64_t cmp = ((uint64_t)s.regs[XR_EDX] << 32) | (uint64_t)s.regs[XR_EAX];
                            if (dst == cmp) {
                                s.eflags |= F_ZF;
                                mw64(maddr, ((uint64_t)s.regs[XR_ECX] << 32) | (uint64_t)s.regs[XR_EBX]);
                            } else {
                                s.eflags &= ~F_ZF;
                                s.regs[XR_EAX] = (uint32_t)dst;
                                s.regs[XR_EDX] = (uint32_t)(dst >> 32);
                            }
                        }
                        break;
                    }
                    case 0xC8: case 0xC9: case 0xCA: case 0xCB:
                    case 0xCC: case 0xCD: case 0xCE: case 0xCF: { // BSWAP
                        uint8_t r = op2 & 7;
                        uint32_t v = s.regs[r];
                        v = (v >> 24) | ((v >> 8) & 0xFF00) | ((v << 8) & 0xFF0000) | (v << 24);
                        s.regs[r] = v;
                        break;
                    }

                    // ---- SSE / SSE2 ----
                    case 0x10: case 0x11: case 0x28: case 0x29: // MOVUPS/MOVSS/MOVSD/MOVAPS (+PD)
                    case 0x2B:                                  // MOVNTPS/MOVNTPD (store-only)
                    case 0x51: case 0x54: case 0x55: case 0x56: // SQRTPS, ANDPS, ANDNPS, ORPS
                    case 0x57: case 0x58: case 0x59:            // XORPS, ADDPS, MULPS
                    case 0x5C: case 0x5D: case 0x5E: case 0x5F: // SUBPS, MINPS, DIVPS, MAXPS
                    {
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        bool pF3 = repz, pF2 = repnz;
                        int kind = pF3 ? 2 : (pF2 ? 3 : (opsize16 ? 1 : 0)); // 0=PS 1=PD 2=SS 3=SD

                        uint64_t src[2] = {0, 0};
                        if (op2 == 0x11 || op2 == 0x29 || op2 == 0x2B) {
                            src[0] = s.xmm[regf][0];
                            src[1] = s.xmm[regf][1];
                        } else if (is_reg) {
                            src[0] = s.xmm[rm][0];
                            src[1] = s.xmm[rm][1];
                        } else {
                            uint32_t addr = base + ea;
                            if (kind == 2) src[0] = mr32(addr);
                            else if (kind == 3) src[0] = mr64(addr);
                            else { src[0] = mr64(addr); src[1] = mr64(addr + 8); }
                        }
                        uint64_t dst[2] = {s.xmm[regf][0], s.xmm[regf][1]};

                        switch (op2) {
                            case 0x10: case 0x28: // load
                                if (kind == 2) sse_write(s, regf, (dst[0] & ~0xFFFFFFFFull) | (src[0] & 0xFFFFFFFFull), dst[1]);
                                else if (kind == 3) sse_write(s, regf, src[0], dst[1]);
                                else sse_write(s, regf, src[0], src[1]);
                                break;
                            case 0x11: case 0x29: case 0x2B: { // store / reg-move / non-temporal store
                                uint64_t dr0 = s.xmm[rm][0], dr1 = s.xmm[rm][1];
                                if (kind == 2) {
                                    if (is_reg) sse_write(s, rm, (dr0 & ~0xFFFFFFFFull) | (src[0] & 0xFFFFFFFFull), dr1);
                                    else mw32(base + ea, static_cast<uint32_t>(src[0]));
                                } else if (kind == 3) {
                                    if (is_reg) sse_write(s, rm, src[0], dr1);
                                    else mw64(base + ea, src[0]);
                                } else {
                                    if (is_reg) sse_write(s, rm, src[0], src[1]);
                                    else { mw64(base + ea, src[0]); mw64(base + ea + 8, src[1]); }
                                }
                                break;
                            }
                            case 0x54: dst[0] &= src[0]; dst[1] &= src[1]; sse_write(s, regf, dst[0], dst[1]); break;
                            case 0x55: dst[0] = (~dst[0]) & src[0]; dst[1] = (~dst[1]) & src[1]; sse_write(s, regf, dst[0], dst[1]); break;
                            case 0x56: dst[0] |= src[0]; dst[1] |= src[1]; sse_write(s, regf, dst[0], dst[1]); break;
                            case 0x57: dst[0] ^= src[0]; dst[1] ^= src[1]; sse_write(s, regf, dst[0], dst[1]); break;
                            case 0x51: // sqrt
                                if (kind == 0) { for (int i = 0; i < 4; i++) xmm_ps(s, regf)[i] = std::sqrt(reinterpret_cast<float*>(&src)[i]); }
                                else if (kind == 1) { for (int i = 0; i < 2; i++) xmm_pd(s, regf)[i] = std::sqrt(reinterpret_cast<double*>(&src)[i]); }
                                else if (kind == 2) { xmm_ps(s, regf)[0] = std::sqrt(reinterpret_cast<float*>(&src)[0]); }
                                else { xmm_pd(s, regf)[0] = std::sqrt(reinterpret_cast<double*>(&src)[0]); }
                                break;
                            case 0x58: case 0x59: case 0x5C: case 0x5E: case 0x5D: case 0x5F: {
                                // 0x58 add, 0x59 mul, 0x5C sub, 0x5E div, 0x5D min, 0x5F max
                                int op = op2;
                                float* d = xmm_ps(s, regf);
                                double* dd = xmm_pd(s, regf);
                                float* sf = reinterpret_cast<float*>(&src);
                                double* sd = reinterpret_cast<double*>(&src);
                                if (kind == 0) {
                                    for (int i = 0; i < 4; i++) {
                                        if (op == 0x58) d[i] += sf[i];
                                        else if (op == 0x59) d[i] *= sf[i];
                                        else if (op == 0x5C) d[i] -= sf[i];
                                        else if (op == 0x5E) d[i] /= sf[i];
                                        else if (op == 0x5D) d[i] = std::fmin(d[i], sf[i]);
                                        else d[i] = std::fmax(d[i], sf[i]);
                                    }
                                } else if (kind == 1) {
                                    for (int i = 0; i < 2; i++) {
                                        if (op == 0x58) dd[i] += sd[i];
                                        else if (op == 0x59) dd[i] *= sd[i];
                                        else if (op == 0x5C) dd[i] -= sd[i];
                                        else if (op == 0x5E) dd[i] /= sd[i];
                                        else if (op == 0x5D) dd[i] = std::fmin(dd[i], sd[i]);
                                        else dd[i] = std::fmax(dd[i], sd[i]);
                                    }
                                } else if (kind == 2) {
                                    if (op == 0x58) d[0] += sf[0];
                                    else if (op == 0x59) d[0] *= sf[0];
                                    else if (op == 0x5C) d[0] -= sf[0];
                                    else if (op == 0x5E) d[0] /= sf[0];
                                    else if (op == 0x5D) d[0] = std::fmin(d[0], sf[0]);
                                    else d[0] = std::fmax(d[0], sf[0]);
                                } else {
                                    if (op == 0x58) dd[0] += sd[0];
                                    else if (op == 0x59) dd[0] *= sd[0];
                                    else if (op == 0x5C) dd[0] -= sd[0];
                                    else if (op == 0x5E) dd[0] /= sd[0];
                                    else if (op == 0x5D) dd[0] = std::fmin(dd[0], sd[0]);
                                    else dd[0] = std::fmax(dd[0], sd[0]);
                                }
                                break;
                            }
                        }
                        break;
                    }

                    case 0x2E: case 0x2F: { // UCOMISS/COMISS (UCOMISD/COMISD with 66)
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        double a, b;
                        if (opsize16) {
                            a = xmm_pd(s, regf)[0];
                            b = is_reg ? xmm_pd(s, rm)[0] : mr64(base + ea);
                        } else {
                            a = xmm_ps(s, regf)[0];
                            b = is_reg ? xmm_ps(s, rm)[0] : mr32(base + ea);
                        }
                        sse_comi_flags(s, a, b);
                        break;
                    }

                    case 0x2A: { // CVTSI2SS (F3) / CVTSI2SD (F2)
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        int32_t iv = is_reg ? static_cast<int32_t>(s.regs[rm]) : mr32(base + ea);
                        if (repnz) { // cvtsi2sd
                            s.xmm[regf][0] = 0;
                            s.xmm[regf][1] = 0;
                            xmm_pd(s, regf)[0] = static_cast<double>(iv);
                        } else { // cvtsi2ss
                            s.xmm[regf][0] = 0;
                            s.xmm[regf][1] = 0;
                            xmm_ps(s, regf)[0] = static_cast<float>(iv);
                        }
                        break;
                    }

                    case 0x2C: case 0x2D: { // CVTTSS2SI/CVTTSD2SI (CVTSS2SI/CVTSD2SI)
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        if (opsize16) { // cvttpd2dq / cvtpd2dq (xmm dest)
                            uint64_t src[2] = {0, 0};
                            sse_read(s, is_reg, rm, base, ea, src);
                            double* sd = reinterpret_cast<double*>(&src);
                            uint32_t r0, r1;
                            if (op2 == 0x2C) { r0 = static_cast<uint32_t>(static_cast<int32_t>(sd[0])); r1 = static_cast<uint32_t>(static_cast<int32_t>(sd[1])); }
                            else { r0 = static_cast<uint32_t>(static_cast<int32_t>(std::lrint(sd[0]))); r1 = static_cast<uint32_t>(static_cast<int32_t>(std::lrint(sd[1]))); }
                            sse_write(s, regf, r0 | (static_cast<uint64_t>(r1) << 32), 0);
                        } else {
                            double v;
                            if (repnz) v = is_reg ? xmm_pd(s, rm)[0] : mr64(base + ea);
                            else v = is_reg ? xmm_ps(s, rm)[0] : mr32(base + ea);
                            if (op2 == 0x2C) s.regs[regf] = static_cast<uint32_t>(static_cast<int32_t>(v));
                            else s.regs[regf] = static_cast<uint32_t>(static_cast<int32_t>(std::lrint(v)));
                        }
                        break;
                    }

                    case 0x5A: { // CVTPS2PD/CVTPD2PS/CVTSS2SD/CVTSD2SS
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        if (repz) { // cvtss2sd
                            float f = is_reg ? xmm_ps(s, rm)[0] : mr32(base + ea);
                            s.xmm[regf][0] = 0; s.xmm[regf][1] = 0;
                            xmm_pd(s, regf)[0] = static_cast<double>(f);
                        } else if (repnz) { // cvtsd2ss
                            double d = is_reg ? xmm_pd(s, rm)[0] : mr64(base + ea);
                            s.xmm[regf][0] = 0; s.xmm[regf][1] = 0;
                            xmm_ps(s, regf)[0] = static_cast<float>(d);
                        } else if (opsize16) { // cvtpd2ps
                            double d0 = is_reg ? xmm_pd(s, rm)[0] : mr64(base + ea);
                            double d1 = is_reg ? xmm_pd(s, rm)[1] : mr64(base + ea + 8);
                            s.xmm[regf][0] = 0; s.xmm[regf][1] = 0;
                            xmm_ps(s, regf)[0] = static_cast<float>(d0);
                            xmm_ps(s, regf)[1] = static_cast<float>(d1);
                        } else { // cvtps2pd
                            float f0 = is_reg ? xmm_ps(s, rm)[0] : mr32(base + ea);
                            float f1 = is_reg ? xmm_ps(s, rm)[1] : mr32(base + ea + 4);
                            s.xmm[regf][0] = 0; s.xmm[regf][1] = 0;
                            xmm_pd(s, regf)[0] = static_cast<double>(f0);
                            xmm_pd(s, regf)[1] = static_cast<double>(f1);
                        }
                        break;
                    }

                    case 0x5B: { // CVTDQ2PS / CVTPS2DQ / CVTTPS2DQ
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        uint64_t src[2] = {0, 0};
                        sse_read(s, is_reg, rm, base, ea, src);
                        uint32_t* sdw = reinterpret_cast<uint32_t*>(&src);
                        if (!opsize16 && !repz) { // cvtdq2ps
                            for (int i = 0; i < 4; i++) xmm_ps(s, regf)[i] = static_cast<float>(static_cast<int32_t>(sdw[i]));
                        } else {
                            float* sf = reinterpret_cast<float*>(&src);
                            uint32_t out[4];
                            for (int i = 0; i < 4; i++) {
                                out[i] = repz ? static_cast<uint32_t>(static_cast<int32_t>(sf[i]))
                                              : static_cast<uint32_t>(static_cast<int32_t>(std::lrint(sf[i])));
                            }
                            sse_write(s, regf, out[0] | (static_cast<uint64_t>(out[1]) << 32),
                                              out[2] | (static_cast<uint64_t>(out[3]) << 32));
                        }
                        break;
                    }

                    case 0xE6: { // CVTTPD2DQ (66) / CVTDQ2PD (F3)
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        if (repz) { // cvtdq2pd
                            uint32_t lo = is_reg ? static_cast<uint32_t>(s.xmm[rm][0]) : mr32(base + ea);
                            uint32_t hi = is_reg ? static_cast<uint32_t>(s.xmm[rm][0] >> 32) : mr32(base + ea + 4);
                            s.xmm[regf][0] = 0; s.xmm[regf][1] = 0;
                            xmm_pd(s, regf)[0] = static_cast<double>(static_cast<int32_t>(lo));
                            xmm_pd(s, regf)[1] = static_cast<double>(static_cast<int32_t>(hi));
                        } else { // cvttpd2dq
                            double d0 = is_reg ? xmm_pd(s, rm)[0] : mr64(base + ea);
                            double d1 = is_reg ? xmm_pd(s, rm)[1] : mr64(base + ea + 8);
                            uint32_t r0 = static_cast<uint32_t>(static_cast<int32_t>(d0));
                            uint32_t r1 = static_cast<uint32_t>(static_cast<int32_t>(d1));
                            sse_write(s, regf, r0 | (static_cast<uint64_t>(r1) << 32), 0);
                        }
                        break;
                    }

                    case 0x6E: { // MOVD xmm, r/m32 / (F3) MOVQ xmm, r/m64
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        if (repz) {
                            uint64_t v = is_reg ? (uint64_t)s.regs[rm] : mr64(base + ea);
                            sse_write(s, regf, v, 0);
                        } else {
                            uint32_t v = is_reg ? s.regs[rm] : mr32(base + ea);
                            sse_write(s, regf, v, 0);
                        }
                        break;
                    }
                    case 0x7E: { // MOVD r/m32, xmm / (F3) MOVQ xmm, r/m64
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        if (repz) {
                            uint64_t v = is_reg ? (uint64_t)s.regs[rm] : mr64(base + ea);
                            sse_write(s, regf, v, 0);
                        } else {
                            uint32_t v = static_cast<uint32_t>(s.xmm[regf][0]);
                            if (is_reg) s.regs[rm] = v;
                            else mw32(base + ea, v);
                        }
                        break;
                    }
                    case 0xD6: { // MOVQ xmm/m64, xmm
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        uint64_t v = s.xmm[regf][0];
                        if (is_reg) sse_write(s, rm, v, 0);
                        else mw64(base + ea, v);
                        break;
                    }
                    case 0x6F: case 0x7F: { // MOVDQA/MOVDQU load (6F) / store (7F)
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        if (op2 == 0x6F) {
                            uint64_t src[2] = {0, 0};
                            sse_read(s, is_reg, rm, base, ea, src);
                            sse_write(s, regf, src[0], src[1]);
                        } else {
                            if (is_reg) sse_write(s, rm, s.xmm[regf][0], s.xmm[regf][1]);
                            else { mw64(base + ea, s.xmm[regf][0]); mw64(base + ea + 8, s.xmm[regf][1]); }
                        }
                        break;
                    }

                    case 0xD7: { // PMOVMSKB: sign bits of each byte -> GPR
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        uint64_t src[2] = {0, 0};
                        sse_read(s, is_reg, rm, base, ea, src);
                        uint32_t r = 0;
                        const uint8_t* b = reinterpret_cast<const uint8_t*>(&src);
                        for (int i = 0; i < 16; i++) if (b[i] & 0x80) r |= (1u << i);
                        s.regs[regf] = r;
                        break;
                    }

                    case 0xAE: { // FXSAVE/FXRSTOR/LDMXCSR/STMXCSR + fences
                        uint8_t modrm = fetch8(s);
                        uint8_t opf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        uint32_t mxcsr = 0x1F80;
                        if (is_reg) {
                            // Memory fences: SFENCE(/7) LFENCE(/5) MFENCE(/6).
                            break;
                        }
                        switch (opf) {
                            case 0: // FXSAVE: write a mostly-empty 512-byte area.
                                for (int off = 0; off < 512; off += 4) mw32(base + ea + off, 0);
                                mw32(base + ea + 24, mxcsr); // MXCSR slot in FXSAVE area
                                mw16(base + ea, 0x037F);     // FPU control word
                                mw16(base + ea + 2, 0);      // status word
                                mw16(base + ea + 4, 0);      // tag word
                                break;
                            case 1: // FXRSTOR: no-op (state is managed by the host).
                                break;
                            case 2: // LDMXCSR
                                (void)mr32(base + ea);
                                break;
                            case 3: // STMXCSR
                                mw32(base + ea, mxcsr);
                                break;
                            default: // CLFLUSH and friends: no-op.
                                break;
                        }
                        break;
                    }

                    case 0x50: { // MOVMSKPS/MOVMSKPD: sign bits -> GPR
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        uint32_t mask = 0;
                        if (opsize16) {
                            const double* d = xmm_pd(s, regf);
                            for (int i = 0; i < 2; i++) if (d[i] < 0) mask |= 1u << i;
                        } else {
                            const float* f = xmm_ps(s, regf);
                            for (int i = 0; i < 4; i++) if (f[i] < 0) mask |= 1u << i;
                        }
                        uint8_t rm = modrm & 7;
                        s.regs[rm] = mask;
                        break;
                    }

                    case 0x52: case 0x53: { // RCPPS/RSQRTPS (+SS/PD forms)
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        uint64_t src[2] = {0, 0};
                        sse_read(s, is_reg, rm, base, ea, src);
                        int kind = repz ? 2 : (repnz ? 3 : (opsize16 ? 1 : 0));
                        // 0x52 = RSQRTPS (1/sqrt), 0x53 = RCPPS (1/x)
                        if (kind == 1) { double* d = xmm_pd(s, regf); const double* s2 = reinterpret_cast<const double*>(&src);
                                         for (int i = 0; i < 2; i++) d[i] = (op2 == 0x52) ? (1.0 / std::sqrt(s2[i])) : (1.0 / s2[i]); }
                        else if (kind == 2 || kind == 3) { double* d = xmm_pd(s, regf); const double* s2 = reinterpret_cast<const double*>(&src);
                                         d[0] = (op2 == 0x52) ? (1.0 / std::sqrt(s2[0])) : (1.0 / s2[0]); }
                        else { float* d = xmm_ps(s, regf); const float* s2 = reinterpret_cast<const float*>(&src);
                               for (int i = 0; i < 4; i++) d[i] = (op2 == 0x52) ? (1.0f / std::sqrt(s2[i])) : (1.0f / s2[i]); }
                        break;
                    }

                    case 0xC2: { // CMPPS/CMPPD/CMPSS/CMPSD r/m, imm8
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        uint8_t opc = fetch8(s) & 7;
                        uint64_t src[2] = {0, 0};
                        sse_read(s, is_reg, rm, base, ea, src);
                        int kind = repz ? 2 : (repnz ? 3 : (opsize16 ? 1 : 0));
                        auto cmp = [opc](double a, double b) -> double {
                            bool r;
                            switch (opc) {
                                case 0: r = a == b; break;
                                case 1: r = a < b; break;
                                case 2: r = a <= b; break;
                                case 3: r = std::isnan(a) || std::isnan(b); break;
                                case 4: r = !(a == b); break;
                                case 5: r = !(a < b); break;
                                case 6: r = !(a <= b); break;
                                default: r = !(std::isnan(a) || std::isnan(b)); break;
                            }
                            return r ? -1.0 : 0.0;
                        };
                        if (kind == 2 || kind == 3) {
                            const double* s2 = reinterpret_cast<const double*>(&src);
                            double* d = xmm_pd(s, regf);
                            d[0] = cmp(d[0], s2[0]);
                        } else if (kind == 1) {
                            const double* s2 = reinterpret_cast<const double*>(&src);
                            double* d = xmm_pd(s, regf);
                            for (int i = 0; i < 2; i++) d[i] = cmp(d[i], s2[i]);
                        } else {
                            const float* s2 = reinterpret_cast<const float*>(&src);
                            float* d = xmm_ps(s, regf);
                            for (int i = 0; i < 4; i++) d[i] = static_cast<float>(cmp(d[i], s2[i]));
                        }
                        break;
                    }

                    case 0xC3: { // MOVNTI m32, r32 (plain store)
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        if (!is_reg) mw32(base + ea, s.regs[regf]);
                        break;
                    }

                    case 0x3A: { // 0F 3A escape: PALIGNR/PBLENDW/PBLENDVB
                        uint8_t op3 = fetch8(s);
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        uint64_t src[2] = {0, 0};
                        sse_read(s, is_reg, rm, base, ea, src);
                        if (op3 == 0x0F) { // PALIGNR: concat [regf:src], shift right by imm bytes
                            uint8_t imm = fetch8(s);
                            uint8_t out[16];
                            const uint8_t* d = reinterpret_cast<uint8_t*>(&s.xmm[regf][0]);
                            const uint8_t* m = reinterpret_cast<const uint8_t*>(&src);
                            uint8_t b = imm & 0xFF;
                            for (int i = 0; i < 16; i++) {
                                int from = i + b; // into the 32-byte concatenation
                                out[i] = (from < 16) ? m[from] : d[from - 16];
                            }
                            sse_write(s, regf, reinterpret_cast<uint64_t*>(out)[0], reinterpret_cast<uint64_t*>(out)[1]);
                        } else if (op3 == 0x0E) { // PBLENDW: blend words by imm bits
                            uint8_t imm = fetch8(s);
                            uint16_t* d = reinterpret_cast<uint16_t*>(&s.xmm[regf][0]);
                            const uint16_t* m = reinterpret_cast<const uint16_t*>(&src);
                            for (int i = 0; i < 8; i++) if (imm & (1u << i)) d[i] = m[i];
                            sse_write(s, regf, d[0] | (uint64_t)d[1] << 16 | (uint64_t)d[2] << 32 | (uint64_t)d[3] << 48,
                                      d[4] | (uint64_t)d[5] << 16 | (uint64_t)d[6] << 32 | (uint64_t)d[7] << 48);
                        } else if (op3 == 0x40 || op3 == 0x41) { // PBLENDVB: blend by XMM0 sign bits (op3 0x40)
                            uint8_t out[16];
                            const uint8_t* d = reinterpret_cast<uint8_t*>(&s.xmm[regf][0]);
                            const uint8_t* m = reinterpret_cast<const uint8_t*>(&src);
                            const uint8_t* mask = reinterpret_cast<const uint8_t*>(&s.xmm[0][0]);
                            for (int i = 0; i < 16; i++) out[i] = (mask[i] & 0x80) ? d[i] : m[i];
                            sse_write(s, regf, reinterpret_cast<uint64_t*>(out)[0], reinterpret_cast<uint64_t*>(out)[1]);
                        } else {
                            fprintf(stderr, "[Thetax86] unhandled 0F 3A opcode 0x%02X\n", op3);
                            interp_fault(s, "unhandled 0F 3A opcode");
                        }
                        break;
                    }

                    case 0x77: { // EMMS: empty MMX state (no-op here; x87 state is host-managed)
                        break;
                    }

                    case 0x71: case 0x72: case 0x73: { // PSLLW/D/Q, PSRLW/D/Q, PSRAW/D by imm8
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7; // op selector
                        uint8_t rm = modrm & 7;          // xmm destination
                        uint8_t imm = fetch8(s);
                        uint64_t d[2] = {s.xmm[rm][0], s.xmm[rm][1]};
                        switch (op2) {
                            case 0x71: { // 16-bit lanes
                                uint16_t* p = reinterpret_cast<uint16_t*>(d);
                                uint8_t c = imm & 0xF;
                                if (regf == 2) { for (int i = 0; i < 8; i++) p[i] >>= c; }           // PSRLW
                                else if (regf == 4) { for (int i = 0; i < 8; i++) p[i] = static_cast<uint16_t>(static_cast<int16_t>(p[i]) >> c); } // PSRAW
                                else if (regf == 6) { for (int i = 0; i < 8; i++) p[i] <<= c; }       // PSLLW
                                break; }
                            case 0x72: { // 32-bit lanes
                                uint32_t* p = reinterpret_cast<uint32_t*>(d);
                                uint8_t c = imm & 0x1F;
                                if (regf == 2) { for (int i = 0; i < 4; i++) p[i] >>= c; }            // PSRLD
                                else if (regf == 4) { for (int i = 0; i < 4; i++) p[i] = static_cast<uint32_t>(static_cast<int32_t>(p[i]) >> c); } // PSRAD
                                else if (regf == 6) { for (int i = 0; i < 4; i++) p[i] <<= c; }       // PSLLD
                                break; }
                            case 0x73: { // 64-bit lanes / byte shifts
                                uint8_t c = imm & 0x3F;
                                if (regf == 2) { d[0] >>= c; d[1] >>= c; }                            // PSRLQ
                                else if (regf == 6) { d[0] <<= c; d[1] <<= c; }                       // PSLLQ
                                else if (regf == 3) { // PSRLDQ (byte shift)
                                    uint8_t* p = reinterpret_cast<uint8_t*>(d); uint8_t b = imm & 0xF;
                                    for (int i = 0; i < 16; i++) p[i] = (i + b < 16) ? p[i + b] : 0;
                                } else if (regf == 7) { // PSLLDQ
                                    uint8_t* p = reinterpret_cast<uint8_t*>(d); uint8_t b = imm & 0xF;
                                    for (int i = 15; i >= 0; i--) p[i] = (i >= b) ? p[i - b] : 0;
                                }
                                break; }
                        }
                        sse_write(s, rm, d[0], d[1]);
                        break;
                    }

                    case 0xA4: case 0xA5: case 0xAC: case 0xAD: { // SHLD/SHRD r/m, reg, imm8/CL
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        bool shld = (op2 == 0xA4 || op2 == 0xA5);
                        uint32_t count = (op2 == 0xA4 || op2 == 0xAC) ? fetch8(s) : (s.regs[XR_ECX] & 0x1F);
                        if (opsize16) {
                            uint16_t dst = is_reg ? reg16(s, rm) : mr16(base + ea);
                            uint16_t src = reg16(s, regf);
                            uint16_t res = dst; bool cf = false; bool of = false;
                            if (count != 0) {
                                uint8_t c = count & 0x1F;
                                if (c < 16) {
                                    if (shld) { cf = (dst >> (16 - c)) & 1; res = static_cast<uint16_t>((dst << c) | (src >> (16 - c))); }
                                    else { cf = (dst >> (c - 1)) & 1; res = static_cast<uint16_t>((dst >> c) | (src << (16 - c))); }
                                } else if (c == 16) { cf = shld ? (dst & 1) : (dst >> 15); res = src; }
                                of = (c == 1) && (((res ^ dst) >> 15) & 1);
                            }
                            set_arith_flags_shift<uint16_t>(s, res, cf, of);
                            if (is_reg) set_reg16(s, rm, res); else mw16(base + ea, res);
                        } else {
                            uint32_t dst = is_reg ? s.regs[rm] : mr32(base + ea);
                            uint32_t src = s.regs[regf];
                            uint32_t res = dst; bool cf = false; bool of = false;
                            if (count != 0) {
                                uint8_t c = count & 0x1F;
                                if (c < 32) {
                                    if (shld) { cf = (dst >> (32 - c)) & 1; res = (dst << c) | (src >> (32 - c)); }
                                    else { cf = (dst >> (c - 1)) & 1; res = (dst >> c) | (src << (32 - c)); }
                                } else if (c == 32) { cf = shld ? (dst & 1) : (dst >> 31); res = src; }
                                of = (c == 1) && (((res ^ dst) >> 31) & 1);
                            }
                            set_arith_flags_shift<uint32_t>(s, res, cf, of);
                            if (is_reg) s.regs[rm] = res; else mw32(base + ea, res);
                        }
                        break;
                    }

                    case 0x38: { // 0F 38 escape: PSHUFB/PABS*/POPCNT/CRC32
                        uint8_t op3 = fetch8(s);
                        switch (op3) {
                            case 0x00: { // PSHUFB xmm, xmm/m128
                                uint8_t modrm = fetch8(s);
                                uint8_t regf = (modrm >> 3) & 7;
                                bool is_reg; uint8_t rm;
                                uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                                uint32_t base = seg_base(s, segovr);
                                uint64_t src[2] = {0, 0};
                                sse_read(s, is_reg, rm, base, ea, src);
                                uint8_t out[16];
                                const uint8_t* d = reinterpret_cast<uint8_t*>(&s.xmm[regf][0]);
                                const uint8_t* m = reinterpret_cast<const uint8_t*>(&src);
                                for (int i = 0; i < 16; i++) out[i] = (m[i] & 0x80) ? 0 : d[m[i] & 0xF];
                                sse_write(s, regf, reinterpret_cast<uint64_t*>(out)[0], reinterpret_cast<uint64_t*>(out)[1]);
                                break; }
                            case 0x1C: case 0x1D: case 0x1E: { // PABSB/W/D
                                uint8_t modrm = fetch8(s);
                                uint8_t regf = (modrm >> 3) & 7;
                                bool is_reg; uint8_t rm;
                                uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                                uint32_t base = seg_base(s, segovr);
                                uint64_t src[2] = {0, 0};
                                sse_read(s, is_reg, rm, base, ea, src);
                                uint64_t d[2] = {src[0], src[1]};
                                if (op3 == 0x1C) { int8_t* p = reinterpret_cast<int8_t*>(d);
                                                   for (int i = 0; i < 16; i++) p[i] = p[i] < 0 ? static_cast<int8_t>(-p[i]) : p[i]; }
                                else if (op3 == 0x1D) { int16_t* p = reinterpret_cast<int16_t*>(d);
                                                   for (int i = 0; i < 8; i++) p[i] = p[i] < 0 ? static_cast<int16_t>(-p[i]) : p[i]; }
                                else { int32_t* p = reinterpret_cast<int32_t*>(d);
                                                   for (int i = 0; i < 4; i++) p[i] = p[i] < 0 ? -p[i] : p[i]; }
                                sse_write(s, regf, d[0], d[1]);
                                break; }
                            case 0xF0: { // F3 POPCNT r32, r/m32
                                uint8_t modrm = fetch8(s);
                                uint8_t regf = (modrm >> 3) & 7;
                                bool is_reg; uint8_t rm;
                                uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                                uint32_t base = seg_base(s, segovr);
                                uint32_t v = is_reg ? s.regs[rm] : mr32(base + ea);
                                uint32_t c = 0;
                                while (v) { c += (v & 1); v >>= 1; }
                                s.eflags = (s.eflags & ~F_ZF) | (c ? 0 : F_ZF);
                                s.regs[regf] = c;
                                break; }
                            case 0xF1: { // F2 CRC32 r32, r/m16 (66) or r/m32
                                uint8_t modrm = fetch8(s);
                                uint8_t regf = (modrm >> 3) & 7;
                                bool is_reg; uint8_t rm;
                                uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                                uint32_t base = seg_base(s, segovr);
                                uint32_t crc = s.regs[regf];
                                uint32_t v = is_reg ? (opsize16 ? reg16(s, rm) : s.regs[rm])
                                                    : (opsize16 ? mr16(base + ea) : mr32(base + ea));
                                crc = crc32_step(crc, (v >> 24) & 0xFF);
                                crc = crc32_step(crc, (v >> 16) & 0xFF);
                                crc = crc32_step(crc, (v >> 8) & 0xFF);
                                crc = crc32_step(crc, v & 0xFF);
                                s.regs[regf] = crc;
                                break; }
                            default:
                                fprintf(stderr, "[Thetax86] unhandled 0F 38 opcode 0x%02X\n", op3);
                                interp_fault(s, "unhandled 0F 38 opcode");
                                break;
                        }
                        break;
                    }

                    case 0xDB: case 0xDF: case 0xEB: case 0xEF: // PAND/PANDN/POR/PXOR
                    case 0xD4: case 0xF4:                        // PADDQ/PMULUDQ
                    case 0xF8: case 0xF9: case 0xFA: case 0xFB:  // PSUBB/W/D/Q
                    case 0xFC: case 0xFD: case 0xFE:             // PADDB/W/D
                    case 0x74: case 0x75: case 0x76: case 0x66:  // PCMPEQB/W/D, PCMPGTD
                    case 0x64: case 0x65:                        // PCMPGTB/W
                    case 0x6C: case 0x6D:                        // PUNPCKLQDQ/PUNPCKHQDQ
                    case 0x60: case 0x61: case 0x62:             // PUNPCKLBW/WD/DQ
                    case 0x68: case 0x69: case 0x6A:             // PUNPCKHBW/WD/DQ
                    case 0x63: case 0x67: case 0x6B:             // PACKSSWB/PACKUSWB/PACKSSDW
                    {
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        uint64_t src[2] = {0, 0};
                        sse_read(s, is_reg, rm, base, ea, src);
                        uint64_t dst0 = s.xmm[regf][0];
                        uint64_t dst1 = s.xmm[regf][1];
                        switch (op2) {
                            case 0xDB: sse_write(s, regf, dst0 & src[0], dst1 & src[1]); break;
                            case 0xDF: sse_write(s, regf, (~dst0) & src[0], (~dst1) & src[1]); break;
                            case 0xEB: sse_write(s, regf, dst0 | src[0], dst1 | src[1]); break;
                            case 0xEF: sse_write(s, regf, dst0 ^ src[0], dst1 ^ src[1]); break;
                            case 0xD4: sse_write(s, regf, dst0 + src[0], dst1 + src[1]); break;
                            case 0xF4:
                                sse_write(s, regf,
                                          static_cast<uint64_t>(static_cast<uint32_t>(dst0)) * static_cast<uint32_t>(src[0]),
                                          static_cast<uint64_t>(static_cast<uint32_t>(dst1)) * static_cast<uint32_t>(src[1]));
                                break;
                            case 0xF8: { // PSUBB
                                uint8_t d[16], b[16];
                                memcpy(d, &dst0, 8); memcpy(d + 8, &dst1, 8);
                                memcpy(b, &src[0], 8); memcpy(b + 8, &src[1], 8);
                                for (int i = 0; i < 16; i++) d[i] = static_cast<uint8_t>(d[i] - b[i]);
                                uint64_t o0, o1; memcpy(&o0, d, 8); memcpy(&o1, d + 8, 8);
                                sse_write(s, regf, o0, o1); break; }
                            case 0xF9: { // PSUBW
                                uint16_t d[8], b[8];
                                memcpy(d, &dst0, 8); memcpy(d + 4, &dst1, 8);
                                memcpy(b, &src[0], 8); memcpy(b + 4, &src[1], 8);
                                for (int i = 0; i < 8; i++) d[i] = static_cast<uint16_t>(d[i] - b[i]);
                                uint64_t o0, o1; memcpy(&o0, d, 8); memcpy(&o1, d + 4, 8);
                                sse_write(s, regf, o0, o1); break; }
                            case 0xFA: { // PSUBD
                                uint32_t d[4], b[4];
                                memcpy(d, &dst0, 8); memcpy(d + 2, &dst1, 8);
                                memcpy(b, &src[0], 8); memcpy(b + 2, &src[1], 8);
                                for (int i = 0; i < 4; i++) d[i] = d[i] - b[i];
                                uint64_t o0, o1; memcpy(&o0, d, 8); memcpy(&o1, d + 2, 8);
                                sse_write(s, regf, o0, o1); break; }
                            case 0xFB: sse_write(s, regf, dst0 - src[0], dst1 - src[1]); break;
                            case 0xFC: { // PADDB
                                uint8_t d[16], b[16];
                                memcpy(d, &dst0, 8); memcpy(d + 8, &dst1, 8);
                                memcpy(b, &src[0], 8); memcpy(b + 8, &src[1], 8);
                                for (int i = 0; i < 16; i++) d[i] = static_cast<uint8_t>(d[i] + b[i]);
                                uint64_t o0, o1; memcpy(&o0, d, 8); memcpy(&o1, d + 8, 8);
                                sse_write(s, regf, o0, o1); break; }
                            case 0xFD: { // PADDW
                                uint16_t d[8], b[8];
                                memcpy(d, &dst0, 8); memcpy(d + 4, &dst1, 8);
                                memcpy(b, &src[0], 8); memcpy(b + 4, &src[1], 8);
                                for (int i = 0; i < 8; i++) d[i] = static_cast<uint16_t>(d[i] + b[i]);
                                uint64_t o0, o1; memcpy(&o0, d, 8); memcpy(&o1, d + 4, 8);
                                sse_write(s, regf, o0, o1); break; }
                            case 0xFE: { // PADDD
                                uint32_t d[4], b[4];
                                memcpy(d, &dst0, 8); memcpy(d + 2, &dst1, 8);
                                memcpy(b, &src[0], 8); memcpy(b + 2, &src[1], 8);
                                for (int i = 0; i < 4; i++) d[i] = d[i] + b[i];
                                uint64_t o0, o1; memcpy(&o0, d, 8); memcpy(&o1, d + 2, 8);
                                sse_write(s, regf, o0, o1); break; }
                            case 0x74: { // PCMPEQB
                                uint8_t d[16], b[16];
                                memcpy(d, &dst0, 8); memcpy(d + 8, &dst1, 8);
                                memcpy(b, &src[0], 8); memcpy(b + 8, &src[1], 8);
                                for (int i = 0; i < 16; i++) d[i] = (d[i] == b[i]) ? 0xFF : 0;
                                uint64_t o0, o1; memcpy(&o0, d, 8); memcpy(&o1, d + 8, 8);
                                sse_write(s, regf, o0, o1); break; }
                            case 0x75: { // PCMPEQW
                                uint16_t d[8], b[8];
                                memcpy(d, &dst0, 8); memcpy(d + 4, &dst1, 8);
                                memcpy(b, &src[0], 8); memcpy(b + 4, &src[1], 8);
                                for (int i = 0; i < 8; i++) d[i] = (d[i] == b[i]) ? 0xFFFF : 0;
                                uint64_t o0, o1; memcpy(&o0, d, 8); memcpy(&o1, d + 4, 8);
                                sse_write(s, regf, o0, o1); break; }
                            case 0x76: { // PCMPEQD
                                uint32_t d[4], b[4];
                                memcpy(d, &dst0, 8); memcpy(d + 2, &dst1, 8);
                                memcpy(b, &src[0], 8); memcpy(b + 2, &src[1], 8);
                                for (int i = 0; i < 4; i++) d[i] = (d[i] == b[i]) ? 0xFFFFFFFFu : 0;
                                uint64_t o0, o1; memcpy(&o0, d, 8); memcpy(&o1, d + 2, 8);
                                sse_write(s, regf, o0, o1); break; }
                            case 0x66: { // PCMPGTD
                                int32_t d[4], b[4];
                                memcpy(d, &dst0, 8); memcpy(d + 2, &dst1, 8);
                                memcpy(b, &src[0], 8); memcpy(b + 2, &src[1], 8);
                                for (int i = 0; i < 4; i++) d[i] = (d[i] > b[i]) ? -1 : 0;
                                uint64_t o0, o1; memcpy(&o0, d, 8); memcpy(&o1, d + 2, 8);
                                sse_write(s, regf, o0, o1); break; }
                            case 0x64: { // PCMPGTB
                                int8_t d[16], b[16];
                                memcpy(d, &dst0, 8); memcpy(d + 8, &dst1, 8);
                                memcpy(b, &src[0], 8); memcpy(b + 8, &src[1], 8);
                                for (int i = 0; i < 16; i++) d[i] = (d[i] > b[i]) ? -1 : 0;
                                uint64_t o0, o1; memcpy(&o0, d, 8); memcpy(&o1, d + 8, 8);
                                sse_write(s, regf, o0, o1); break; }
                            case 0x65: { // PCMPGTW
                                int16_t d[8], b[8];
                                memcpy(d, &dst0, 8); memcpy(d + 4, &dst1, 8);
                                memcpy(b, &src[0], 8); memcpy(b + 4, &src[1], 8);
                                for (int i = 0; i < 8; i++) d[i] = (d[i] > b[i]) ? -1 : 0;
                                uint64_t o0, o1; memcpy(&o0, d, 8); memcpy(&o1, d + 4, 8);
                                sse_write(s, regf, o0, o1); break; }
                             case 0x6C: sse_write(s, regf, dst0, src[0]); break; // punpcklqdq
                             case 0x6D: sse_write(s, regf, dst1, src[1]); break; // punpckhqdq
                             case 0x60: case 0x61: case 0x62: { // PUNPCKLBW/WD/DQ
                                 uint64_t lo[2] = {dst0, src[0]};
                                 uint64_t hi[2] = {dst1, src[1]};
                                 if (op2 == 0x60) {
                                     uint8_t out[16];
                                     const uint8_t* a = reinterpret_cast<const uint8_t*>(lo);
                                     const uint8_t* b = reinterpret_cast<const uint8_t*>(lo) + 8;
                                     for (int i = 0; i < 8; i++) { out[i*2] = a[i]; out[i*2+1] = b[i]; }
                                     sse_write(s, regf, reinterpret_cast<uint64_t*>(out)[0], reinterpret_cast<uint64_t*>(out)[1]);
                                 } else if (op2 == 0x61) {
                                     uint16_t out[8];
                                     const uint16_t* a = reinterpret_cast<const uint16_t*>(lo);
                                     const uint16_t* b = reinterpret_cast<const uint16_t*>(lo) + 4;
                                     for (int i = 0; i < 4; i++) { out[i*2] = a[i]; out[i*2+1] = b[i]; }
                                     sse_write(s, regf, reinterpret_cast<uint64_t*>(out)[0], reinterpret_cast<uint64_t*>(out)[1]);
                                 } else {
                                     uint32_t out[4];
                                     const uint32_t* a = reinterpret_cast<const uint32_t*>(lo);
                                     const uint32_t* b = reinterpret_cast<const uint32_t*>(lo) + 2;
                                     out[0] = a[0]; out[1] = b[0]; out[2] = a[1]; out[3] = b[1];
                                     sse_write(s, regf, reinterpret_cast<uint64_t*>(out)[0], reinterpret_cast<uint64_t*>(out)[1]);
                                 }
                                 (void)hi; break;
                             }
                             case 0x68: case 0x69: case 0x6A: { // PUNPCKHBW/WD/DQ
                                 uint64_t hi[2] = {dst1, src[1]};
                                 if (op2 == 0x68) {
                                     uint8_t out[16];
                                     const uint8_t* a = reinterpret_cast<const uint8_t*>(hi);
                                     const uint8_t* b = reinterpret_cast<const uint8_t*>(hi) + 8;
                                     for (int i = 0; i < 8; i++) { out[i*2] = a[i]; out[i*2+1] = b[i]; }
                                     sse_write(s, regf, reinterpret_cast<uint64_t*>(out)[0], reinterpret_cast<uint64_t*>(out)[1]);
                                 } else if (op2 == 0x69) {
                                     uint16_t out[8];
                                     const uint16_t* a = reinterpret_cast<const uint16_t*>(hi);
                                     const uint16_t* b = reinterpret_cast<const uint16_t*>(hi) + 4;
                                     for (int i = 0; i < 4; i++) { out[i*2] = a[i]; out[i*2+1] = b[i]; }
                                     sse_write(s, regf, reinterpret_cast<uint64_t*>(out)[0], reinterpret_cast<uint64_t*>(out)[1]);
                                 } else {
                                     uint32_t out[4];
                                     const uint32_t* a = reinterpret_cast<const uint32_t*>(hi);
                                     const uint32_t* b = reinterpret_cast<const uint32_t*>(hi) + 2;
                                     out[0] = a[0]; out[1] = b[0]; out[2] = a[1]; out[3] = b[1];
                                     sse_write(s, regf, reinterpret_cast<uint64_t*>(out)[0], reinterpret_cast<uint64_t*>(out)[1]);
                                 }
                                 break;
                             }
                             case 0x63: case 0x67: { // PACKSSWB / PACKUSWB
                                 int16_t v[16];
                                 const int16_t* a = reinterpret_cast<const int16_t*>(&dst0);
                                 const int16_t* b = reinterpret_cast<const int16_t*>(&src);
                                 for (int i = 0; i < 8; i++) v[i] = a[i];
                                 for (int i = 0; i < 8; i++) v[i+8] = b[i];
                                 uint8_t out[16];
                                 for (int i = 0; i < 16; i++) {
                                     int x = v[i];
                                     if (op2 == 0x63) out[i] = static_cast<uint8_t>(x < -128 ? -128 : x > 127 ? 127 : x);
                                     else out[i] = static_cast<uint8_t>(x < 0 ? 0 : x > 255 ? 255 : x);
                                 }
                                 sse_write(s, regf, reinterpret_cast<uint64_t*>(out)[0], reinterpret_cast<uint64_t*>(out)[1]);
                                 break;
                             }
                             case 0x6B: { // PACKSSDW
                                 int32_t v[8];
                                 const int32_t* a = reinterpret_cast<const int32_t*>(&dst0);
                                 const int32_t* b = reinterpret_cast<const int32_t*>(&src);
                                 for (int i = 0; i < 4; i++) v[i] = a[i];
                                 for (int i = 0; i < 4; i++) v[i+4] = b[i];
                                 uint16_t out[8];
                                 for (int i = 0; i < 8; i++) {
                                     long x = v[i];
                                     out[i] = static_cast<uint16_t>(x < -32768 ? -32768 : x > 32767 ? 32767 : x);
                                 }
                                 sse_write(s, regf, reinterpret_cast<uint64_t*>(out)[0], reinterpret_cast<uint64_t*>(out)[1]);
                                 break;
                             }
                        }
                        break;
                    }

                    case 0x70: { // PSHUFD (66) / SHUFPS (ps) / SHUFPD (66) / SHUFSS? (F3 0F 70 is no)
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        uint8_t imm = fetch8(s);
                        uint64_t src[2] = {0, 0};
                        sse_read(s, is_reg, rm, base, ea, src);
                        if (opsize16 && !repz && !repnz) { // pshufd
                            uint32_t in[4]; memcpy(in, &src, 16);
                            uint32_t out[4];
                            for (int i = 0; i < 4; i++) out[i] = in[(imm >> (2 * i)) & 3];
                            sse_write(s, regf, out[0] | (static_cast<uint64_t>(out[1]) << 32),
                                              out[2] | (static_cast<uint64_t>(out[3]) << 32));
                        } else { // shufps (ps) / shufpd (pd)
                            if (opsize16) { // shufpd
                                double in[2]; memcpy(in, &src, 16);
                                double out[2];
                                out[0] = in[(imm >> 0) & 1];
                                out[1] = in[(imm >> 1) & 1];
                                // combine dest elements 0 and source-based
                                // shufpd: dest[0] from {dest0, src0}; dest[1] from {dest1, src1}
                                double dv[2] = {xmm_pd(s, regf)[0], xmm_pd(s, regf)[1]};
                                out[0] = ((imm >> 0) & 1) ? in[0] : dv[0];
                                out[1] = ((imm >> 1) & 1) ? in[1] : dv[1];
                                sse_write(s, regf, 0, 0);
                                xmm_pd(s, regf)[0] = out[0];
                                xmm_pd(s, regf)[1] = out[1];
                            } else { // shufps
                                float in[4]; memcpy(in, &src, 16);
                                float dv[4]; memcpy(dv, &s.xmm[regf][0], 16);
                                float out[4];
                                out[0] = dv[(imm >> 0) & 3];
                                out[1] = dv[(imm >> 2) & 3];
                                out[2] = in[(imm >> 4) & 3];
                                out[3] = in[(imm >> 6) & 3];
                                memcpy(&s.xmm[regf][0], out, 16);
                            }
                        }
                        break;
                    }

                    case 0xC6: { // SHUFPS/SHUFPD/SHUFSS/SHUFSD
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        uint8_t imm = fetch8(s);
                        uint64_t src[2] = {0, 0};
                        sse_read(s, is_reg, rm, base, ea, src);
                        if (opsize16) { // shufpd
                            double dv[2] = {xmm_pd(s, regf)[0], xmm_pd(s, regf)[1]};
                            double sv[2]; memcpy(sv, &src, 16);
                            double out0 = ((imm >> 0) & 1) ? sv[0] : dv[0];
                            double out1 = ((imm >> 1) & 1) ? sv[1] : dv[1];
                            sse_write(s, regf, 0, 0);
                            xmm_pd(s, regf)[0] = out0;
                            xmm_pd(s, regf)[1] = out1;
                        } else {
                            float dv[4]; memcpy(dv, &s.xmm[regf][0], 16);
                            float sv[4]; memcpy(sv, &src, 16);
                            float out[4];
                            out[0] = dv[(imm >> 0) & 3];
                            out[1] = dv[(imm >> 2) & 3];
                            out[2] = sv[(imm >> 4) & 3];
                            out[3] = sv[(imm >> 6) & 3];
                            memcpy(&s.xmm[regf][0], out, 16);
                        }
                        break;
                    }

                    case 0x12: case 0x13: case 0x14: case 0x15:
                    case 0x16: case 0x17: { // MOVLPS/MOVHPS/MOVLPD/MOVHPD
                        uint8_t modrm = fetch8(s);
                        uint8_t regf = (modrm >> 3) & 7;
                        bool is_reg; uint8_t rm;
                        uint32_t ea = decode_rm(s, modrm, addrsize16, &is_reg, &rm);
                        uint32_t base = seg_base(s, segovr);
                        bool pd = opsize16;
                        switch (op2) {
                            case 0x12: // movlps xmm, m64 / movhlps xmm, xmm
                                if (is_reg) { uint64_t t = s.xmm[rm][1]; sse_write(s, regf, t, 0); }
                                else { uint64_t t = mr64(base + ea); sse_write(s, regf, t, 0); }
                                break;
                            case 0x13: mw64(base + ea, s.xmm[regf][0]); break; // movlps m64, xmm
                            case 0x14: sse_write(s, regf, s.xmm[regf][0], mr64(base + ea)); break; // unpcklpd (pd only)
                            case 0x15: { // unpckhpd
                                uint64_t lo = s.xmm[regf][0], hi = s.xmm[regf][1];
                                sse_write(s, regf, lo, hi);
                                break;
                            }
                            case 0x16: // movhps xmm, m64 / movlhps xmm, xmm
                                if (is_reg) { sse_write(s, regf, s.xmm[regf][0], s.xmm[rm][0]); }
                                else { sse_write(s, regf, s.xmm[regf][0], mr64(base + ea)); }
                                break;
                            case 0x17: mw64(base + ea, s.xmm[regf][1]); break; // movhps m64, xmm
                        }
                        break;
                    }

                    default: {
                        char buf[64];
                        snprintf(buf, sizeof(buf), "0F %02X", op2);
                        interp_fault(s, buf);
                        break;
                    }
                }
                break;
            }

            default: {
                char buf[64];
                snprintf(buf, sizeof(buf), "opcode %02X", op);
                interp_fault(s, buf);
                break;
            }
        }
        break;
    }
}

    // Restore outer interpreter state
    g_interp_active = prev_interp_active;
    g_fault_state = prev_fault_state;
    g_callback_sentinel = prev_callback_sentinel;
    g_fault_jmp_ptr = prev_fault_jmp;
    return 0;
}

// ---------------------------------------------------------------------------
// Guest callback invocation (host -> guest)
// ---------------------------------------------------------------------------
// Run a guest cdecl function with no arguments to completion. We snapshot the
// current guest state, set up a return sentinel on the guest stack, interpret
// the callback until it `ret`s (popping the sentinel), then restore the caller.
int x86_call_guest(uint32_t fn) {
    if (!g_fault_state) return -1;
    const uint32_t sentinel = 0xCA11B000u;
    X86CpuState cb = *g_fault_state;
    cb.halted = false;
    cb.eip = fn;
    uint32_t esp = cb.regs[XR_ESP];
    esp -= 4;
    mw32(esp, sentinel);
    cb.regs[XR_ESP] = esp;

    g_callback_sentinel = sentinel;
    int rc = x86_run(cb, {});
    g_callback_sentinel = x86_prev_callback_sentinel();
    return rc;
}

uint32_t x86_call_guest_args(uint32_t fn, const uint32_t* args, int nargs) {
    return x86_call_guest_args_ebp(fn, args, nargs, 0);
}

uint32_t x86_call_guest_args_ebp(uint32_t fn, uint32_t* args, int nargs, uint32_t ebp) {
    return x86_call_guest_args_ebp(fn, (const uint32_t*)args, nargs, ebp);
}

uint32_t x86_call_guest_args_ebp(uint32_t fn, const uint32_t* args, int nargs, uint32_t ebp) {
    if (!g_fault_state) return 0;
    const uint32_t sentinel = 0xCA11B001u;
    X86CpuState cb = *g_fault_state;
    cb.halted = false;
    cb.eip = fn;
    if (ebp) cb.regs[XR_EBP] = ebp; // MSVC filters/finally run on the SEH frame's ebp
    uint32_t esp = cb.regs[XR_ESP];
    for (int i = nargs - 1; i >= 0; i--) {
        esp -= 4;
        mw32(esp, args[i]);
    }
    esp -= 4;
    mw32(esp, sentinel);
    cb.regs[XR_ESP] = esp;

    g_callback_sentinel = sentinel;
    x86_run(cb, {});
    g_callback_sentinel = x86_prev_callback_sentinel();
    return cb.regs[XR_EAX];
}

// Invoke a guest exception handler with EAX seeded. MSVC's x86 C++ handler
// chain does not take its FuncInfo from the DISPATCHER_CONTEXT: the
// per-function stub loads EAX with that function's FuncInfo, __CxxFrameHandler3
// forwards it as a fifth argument, and the internal reads it from [ebp+0x18].
// Passing only the four stack arguments (what x86_call_guest_args_ebp does)
// leaves EAX undefined, so a real MSVC guest handler has no try/catch data and
// returns ExceptionContinueSearch. eax_value == 0 leaves EAX untouched, which
// keeps the classic __except_handler3/4 path behaving as before.
uint32_t x86_call_guest_handler(uint32_t fn, const uint32_t* args, int nargs,
                                uint32_t eax_value) {
    if (eax_value == 0) return x86_call_guest_args_ebp(fn, args, nargs, 0);
    if (!g_fault_state) return 0;
    const uint32_t sentinel = 0xCA11B001u;
    X86CpuState cb = *g_fault_state;
    cb.halted = false;
    cb.eip = fn;
    cb.regs[XR_EAX] = eax_value;
    uint32_t esp = cb.regs[XR_ESP];
    for (int i = nargs - 1; i >= 0; i--) {
        esp -= 4;
        mw32(esp, args[i]);
    }
    esp -= 4;
    mw32(esp, sentinel);
    cb.regs[XR_ESP] = esp;

    g_callback_sentinel = sentinel;
    x86_run(cb, {});
    g_callback_sentinel = x86_prev_callback_sentinel();
    return cb.regs[XR_EAX];
}

// Save the current callback sentinel: nested callbacks (an API dispatched
// from inside another callback) must restore the OUTER sentinel on exit so the
// outer callback's RET still halts. Previously the nested callback cleared it,
// so the outer RET executed the sentinel bytes and faulted.
uint32_t x86_prev_callback_sentinel() {
    return g_callback_sentinel;
}

X86SehResume& x86_seh_resume_state() {
    static thread_local X86SehResume resume;
    return resume;
}

X86CpuState* x86_current_state() {
    return g_fault_state;
}

void x86_set_callback_sentinel(uint32_t sentinel) {
    g_callback_sentinel = sentinel;
}

} // namespace ThetaZ
