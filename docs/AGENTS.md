# AGENTS.md — ThetaZ Deep Technical Reference

> The low-level companion to the root `AGENTS.md`. This file holds every exact
> constant, offset, struct layout, scheme, and gotcha that makes the emulator
> work. It is the COMPLETE memory of how the code behaves — fix any file by
> reading this first.

---

## 1. THE 32-BIT GUEST ENVIRONMENT (core/x86/guest_env.cpp)

### Fixed guest addresses (identity-mapped: host addr == guest addr)
```
0x00F00000        kX86StackBase   main stack top, 1MB, grows down
0x00E00000        per-thread stacks allocated DOWNWARD from here (0x100000 each,
                  min floor 0x00100000); stack_base+size must stay < 0x100000000
0x7FFF0000        kX86TebAddr     main thread TEB (fs:0 SEH base = kGuestTeb)
0x7FFDF000        kX86PebAddr     PEB
0x7FFE0000        kX86ParamsAddr  RTL_USER_PROCESS_PARAMETERS
0x7FFE1000        kX86CmdlineAddr command line buffer (UNICODE, "thetaz")
0x7FFE2000        kX86TlsDataAddr TLS data block (zeroed, 0x1000)
0x7FFE3000        kX86TlsArrayAddr TLS array; [0] = kX86TlsDataAddr
0x7FFE8000        per-thread TEBs allocated DOWNWARD (0x1000 each, wrap 0x7FFE8000
                  if below 0x7FFE0000); tls_array = teb-0x3000, tls_data = teb-0x2000
```

### x86 TEB offsets (32-bit)
```
TEB_StackBase        = 0x04
TEB_StackLimit       = 0x08
TEB_Self             = 0x18
TEB_TlsPointer       = 0x2C   (ThreadLocalStoragePointer → TLS array)
TEB_Peb              = 0x30
TEB_ClientId_Process = 0x40
TEB_ClientId_Thread  = 0x44
```

### x86 PEB offsets (32-bit)
```
PEB_ImageBaseAddress  = 0x08
PEB_ProcessParameters = 0x10
PEB_ProcessHeap       = 0x18
PEB_NumberOfProcessors= 0x64
```

### x86 RTL_USER_PROCESS_PARAMETERS offsets (32-bit)
```
PARAMS_ImagePathName = 0x34   (UNICODE_STRING: Length@0, MaxLength@2, pad, Buffer@8)
PARAMS_CommandLine   = 0x3C
PARAMS_Environment   = 0x44
```
`write_us32(addr, len, maxlen, buffer)` writes Length/MaxLength at +0/+2 and Buffer
(a 32-bit guest pointer) at +8. The cmdline buffer content is `L"thetaz"` (14 bytes).

### Entry-point state
`x86_run_entry(entry)`:
```
eip   = entry
esp   = (g_stack_top - 0x40) & 0xFFFFFFF0   // mingw CRT reads [esp] during startup
fs_base = g_teb_addr (0x7FFF0000); gs_base = 0; seg[6] = {0}
```
`x86_run_guest_thread(entry, stack_base, stack_size, tid)` — per-thread TEB from the
descending allocator, per-thread TLS array/data, low stack if stack_base==0 or
base+size > 4GB, then the same `X86CpuState` shape with `fs_base = teb`.

`x86_alloc_guest(addr, size)` = `mmap(addr, size, RW, MAP_PRIVATE|MAP_ANONYMOUS|
MAP_FIXED_NOREPLACE)`; returns 0 unless the mapping lands exactly at addr.

---

## 2. THE x86 INTERPRETER (core/x86/interp.cpp, 3442 lines)

### X86CpuState
```
regs[8]   indexed by XR_EAX=0,ECX=1,EDX=2,EBX=3,ESP=4,EBP=5,ESI=6,EDI=7
eip, eflags (init 0x2), fs_base, gs_base (uint32), seg[6] (uint16)
fpu_st[8] (double), fpu_top, fpu_cw (0x037F), fpu_sw
xmm[8][2] (8 x 128-bit {lo,hi}), insn_count, halted, exit_code
```

### Instruction dispatch
- Single `for(;;)` + `switch(op)`, plus a nested `case 0x0F` two-byte switch.
- Prefixes consumed in an inner loop before `op`: 0x66→opsize16, 0x67→addrsize16,
  segment overrides 2E/36/3E/26/64/65→segovr 0..5 (default -1 → base 0),
  0xF0→locked, 0xF2→repnz/rep, 0xF3→repz/rep.
- `ow = opsize16 ? 2 : 4`; `decode_rm(s, modrm, addrsize16, &is_reg, &rm)` returns
  the effective address; `calc_ea32` handles SIB (scale<<6, index, base; base==5&&
  mod==0 → disp32) and 16-bit addressing under 0x67 (BX+SI, BX+DI, BP+SI, BP+DI,
  SI, DI, BP+disp16, BX; masked &0xFFFF).
- SSE variant selection: `kind = pF3 ? SS : pF2 ? SD : opsize16 ? PD : PS`.

### Implemented opcodes (exact)
- **Arithmetic 0x00–0x3D**: ADD/OR/ADC/SBB/AND/SUB/XOR/CMP (byte/word/dword, both
  directions, r/m+reg, AL/eAX imm forms). DAA 27, DAS 2F, AAA 37, AAS 3F.
  `set_arith_flags<T>` (ZF/SF/AF/PF/CF/OF, sub overflow = `(a^r)&((~b)^r)&msb`);
  AND/OR/XOR/TEST clear AF.
- **0x40–0x5F**: INC/DEC r32 (CF preserved), PUSH/POP r32. 0x60/0x61 PUSHAD/POPAD
  (PUSHAD pushes original ESP). 0x68/0x6A PUSH imm. 0x8F POP r/m. 0x69/0x6B IMUL.
- **0x70–0x7F** Jcc rel8; **0x80–0x83** Group1 imm (0x83 sign-extends imm8);
  0x84/0x85 TEST; 0x86/0x87 XCHG; 0x88–0x8B MOV; 0x8C MOV r/m,Sreg; 0x8D LEA;
  0x8E MOV Sreg,r/m (FS/GS set bases to 0); 0x90 NOP; 0x91–0x97 XCHG eAX,r;
  0x98 CWDE; 0x99 CDQ; 0x9B WAIT; 0x9C/0x9D PUSHFD/POPFD (POPFD forces bit1);
  0x9E/0x9F SAHF/LAHF.
- **0xA0–0xA3 moffs**: displacement uses ADDRESS-size (0x67), data uses 0x66.
- **0xA4–0xAF string ops**: MOVS/CMPS/STOS/LODS/SCAS with REP/REPE/REPNE, DF handling.
- **0xB0–0xBF** MOV r8/imm; **0xC0/0xC1/0xD0/0xD1/0xD2/0xD3** Group2 shifts
  ROL/ROR/RCL/RCR/SHL/SHR/SAR.
- **0xC2/C3 RET** — if `g_callback_sentinel && s.eip == sentinel` → `s.halted=true`.
- **0xC6/0xC7** MOV r/m,imm; 0xC8/0xC9 ENTER/LEAVE; 0xCC INT3; 0xCD INT;
  0xCE INTO; 0xCF IRET; 0xD4/0xD5 AAM/AAD; 0xD7 XLAT.
- **x87 0xD8–0xDF**: memory + register pseudo-ops (FLD/FST/FSTP m32/m64,
  FLDCW/FNSTCW, FILD/FIST/FISTP, FNCLEX/FNINIT, FNSTSW AX at DF/E0, FUCOMPP
  DF/E8/F0/F8, FADDP/FMULP/FCOMPP/FSUBP/FDIVP in DE, D9 register: FCHS E0, FABS E1,
  FLD1/FLDL2T/FLDL2E/FLDPI/FLDLG2/FLDLN2/FLDZ, F2XM1 F0, FYL2X F1, FSQRT FA,
  FRNDINT FC, FSCALE FD, FSIN FE, FCOS FF). FPU compares set C0/C2/C3 into fpu_sw.
- **0xE0–0xE3** LOOP/LOOPE/LOOPNE (dec ECX first)/JCXZ-JECXZ; **0xE4–0xEF**
  IN/OUT (no-ops); **0xE8 CALL rel32**, **0xE9 JMP rel32**, **0xEB JMP rel8**.
- **0xF6/0xF7 Group3**: TEST/NOT/NEG/MUL/IMUL/DIV/IDIV (div-by-zero → fault).
- **0xFE/0xFF Groups 4/5**: INC/DEC Eb; opf 2 CALL r/m, opf 4 JMP r/m (thunk-aware),
  opf 6 PUSH r/m; far forms fault.
- **0x0F two-byte**: CMOVcc 40-4F; Jcc rel32 80-8F; NOP 1F; RDTSC 31
  (clock_gettime ns → EAX:EDX); SETcc 90-9F; PUSH/POP FS A0/A1; CPUID A2
  (leaves 0,1,0x80000000,0x80000001, "GenuineIntel", EDX leaf1=0x1FBBFBF);
  BT/BTS/BTR/BTC A3/AB/B3/BB; PUSH/POP GS A8/A9; IMUL AF; Group8 BA; BSF/BSR
  BC/BD (+ **F3 = TZCNT/LZCNT**: CF=1 on zero src with ZF, count = 31-pos for
  LZCNT, not the bit position); MOVZX B6/B7; MOVSX BE/BF; XADD C0(r/m8)/C1(r/m32);
  CMPXCHG B0/B1; BSWAP C8-CF; **SHLD/SHRD A4/A5/AC/AD** (imm8 and CL forms,
  16/32-bit, CF/OF per real CPU); **EMMS 77** (no-op); **MOVNTPS/MOVNTPD 2B**
  (plain store); **MOVNTI C3** (plain store); **MOVMSKPS/PD 50**; **RCPPS 53 /
  RSQRTPS 52** (1/x and 1/sqrt, PS/PD/SS/SD via 66/F3/F2); **CMPPS/CMPPD/CMPSS/
  CMPSD C2** with imm8 pred (EQ/LT/LE/UNORD/NEQ/NLT/NLE/ORD, NaN-aware);
  **0F 38 escape**: PSHUFB 00, PABSB/W/D 1C/1D/1E, POPCNT F3-F0, CRC32 F2-F1
  (byte-wise reflected 0xEDB88320, 16/32-bit src by 66); **0F 3A escape**:
  PALIGNR 0F, PBLENDW 0E, PBLENDVB 40.
- **x87 memory conversion rule**: FLD/FADD/FMUL/FCOM/… m32/m64 (D8/DC, D9/DD /0)
  must convert the IEEE bits (`f32_to_double`/`f64_to_double`), never reinterpret
  the raw integer — printing a float used to read the bit pattern as an int.
  FST/FSTP m32/m64 (D9/DD /2,/3) store `double_to_f32/f64_bits`. FIST*/FILD*
  integer forms (DA/DE/DB/DF) are integer, no conversion. **FCOMI/FUCOMI
  (DB /6,/5), FCOMIP/FUCOMIP (DF /6,/5)** set ZF/PF/CF + C0/C2/C3 in fpu_sw,
  the /7,/8 pop variants pop ST(0).
- **SSE/SSE2**: MOVUPS/MOVSS/MOVSD/MOVAPS/MOVAPD 10/11/28/29; logical 54/55/56/57;
  SQRTPS 51; arith ADD/MUL/SUB/MIN/DIV/MAX 58/59/5C/5D/5E/5F; UCOMISS/COMISS/
  UCOMISD/COMISD 2E/2F; CVTSI2SS/SD 2A, CVTTSS2SI/CVTSD2SI/CVTSS2SI/CVTSD2SI
  2C/2D; CVT* 5A/5B/E6; MOVD 6E/7E (**F3 prefix = MOVQ xmm,r/m64 load — GCC
  emits `f3 0f 7e` for 64-bit loads**), MOVQ D6; MOVDQA/MOVDQU 6F/7F; PMOVMSKB D7;
  0F AE (FXSAVE opf0 zeros 512B + MXCSR@+24=0x1F80 + FCW@+0=0x037F; FXRSTOR opf1
  no-op; LDMXCSR opf2 read-only; STMXCSR opf3 writes 0x1F80; reg form = fences
  no-op; default CLFLUSH no-op); PAND/PANDN/POR/PXOR DB/DF/EB/EF; PADDQ/PMULUDQ
  D4/F4; PSUBB/W/D/Q F8-FB; PADDB/W/D FC-FE; PCMPEQB/W/D 74-76; PCMPGTD 66;
  PUNPCKLQDQ/HQDQ 6C/6D; PUNPCKLBW/WD/DQ 60-62; PUNPCKHBW/WD/DQ 68-6A;
  PACKSSWB/PACKUSWB/PACKSSDW 63/67/6B; **PCMPGTB/W/D 64/65/66** (signed >);
  **PSLLW/D/Q, PSRLW/D/Q, PSRAW/D, PSLLDQ, PSRLDQ by imm8 71/72/73** (reg-field
  op: /2 shift-right, /4 shift-right-arith, /3,/7 byte shifts, /6 shift-left);
  PSHUFD/SHUFPS/SHUFPD 70; SHUFPS/SHUFPD C6;
  MOVLPS/HPS/LPD/HPD + MOVHLPS/LHPS + UNPCKLPD/HPD 12-17.
- **MOVUPS-family store bug (FIXED)**: 0F 11/29/2B stores previously read their
  source from the DESTINATION memory (its own old contents) and reg-reg stores
  wrote to the reg field; stores now source `xmm[regf]` and write `rm`.
- **Known gaps** (remaining 0F two-byte ~79, all rare/privileged): ring-0
  (20-27, 30, 32-37, 78-7D, 7E/7F note), prefetch/NOP 18-1F, 0FA6/A7 CMPXCHG8B,
  0FB2, 0FB4/B5, 0FB8/B9, 0FC4/C5 PINSRW/PEXTRW, 0FC7, 0FD0-D3/D5/D8-DA/DC-DE/
  E0-E7/EC-EE/F0/F2-F7 MMX-family (opcodes exist; MMX regs map to fpu_st so most
  MMX ops are meaningless until the mm0-7 alias is done), 0F 38 further 38/3A
  SSSE3/SSE4 (PHADD, PMULHRSW, PSHUFB variants, CRC32 8-bit src form).

### Fault handling
- `interp_fault_handler(sig, siginfo_t*, uctx)` installed once (static flag) for
  SIGSEGV/SIGBUS/SIGFPE with SA_SIGINFO. If `g_interp_active`: prints signal, guest
  eip, si_addr, host RIP, dumps regs + trace ring, then `siglongjmp(*g_fault_jmp_ptr,1)`.
- `x86_run` saves/restores `g_interp_active`, `g_fault_state`, `g_callback_sentinel`,
  `g_fault_jmp_ptr` around `sigsetjmp`; nested runs fault into their own handler.
- Return codes: -1 fault, 2 insn-limit, (int)exit_code when halted, 0 clean.

### Memory helpers
`mr8/16/32/64`, `mw8/16/32/64` = direct deref of identity-mapped address.
`fetch8/16/32(s)` at s.eip. `push16/32`, `pop16/32`. `seg_base(s, segovr)` → 0 for
CS/SS/DS/ES/none, fs_base for FS, gs_base for GS. `seg_index(regf)` =
`map[8]={3,0,1,2,4,5,4,5}`. Registers: `reg16/set_reg16`, `reg8_lo/hi`,
`read_reg8/write_reg8` (r>=4 → r-4 high byte = AH/CH/DH/BH).

### Callback entry (host → guest)
- `x86_call_guest(fn)`: push sentinel 0xCA11B000; `x86_call_guest_args(fn,args,n)`:
  push args reverse-order then sentinel 0xCA11B001; both set `g_callback_sentinel`,
  copy `*g_fault_state`, run nested `x86_run`; RET to sentinel halts. Returns EAX.
- **Nested callbacks MUST save/restore the sentinel** (`x86_prev_callback_sentinel`),
  not clear it: a callback (e.g. WNDPROC WM_CLOSE) that calls an API which
  dispatches another callback (DestroyWindow → WM_DESTROY) used to clear
  `g_callback_sentinel`, so the OUTER callback's RET executed the sentinel bytes
  and faulted at eip=0xca11b002. All three callback sites (x86_call_guest,
  x86_call_guest_args, x86_run_guest_callback) now restore the previous value.

---

## 3. THUNKS + API DISPATCH (core/x86/api_dispatch.cpp)

### Thunk region
```
kThunkBase    = 0x76000000
kThunkSize    = 0x00100000   (1MB)
kThunkStride  = 32           → 32768 thunks
```
`init_api_dispatch()` mmaps it RW with MAP_FIXED_NOREPLACE. `is_api_thunk_addr(a)`:
`a >= base && a < base+size && ((a-base) % 32) == 0`. `api_thunk_at(a)` → the
`ApiThunk` if `in_use`.

### Signature table
`SigEntry { dll, name, argc, flags, wide }`; `flags & CDECL(1)` = cdecl else stdcall;
`wide` bit i set ⇒ slot i is the low half of an 8-byte value (next slot = high half).
Macros: `ST(dll,name,argc)`, `STW(...,wide)`, `CT(...)`, `CTW(...)`.
`sig_dll_name`: `api-ms-win-crt-*`→`ucrtbase.dll`, `msvcrt.dll`→`ucrtbase.dll`,
`vcruntime140.dll`→`ucrtbase.dll`.
`api_signature_for(dll,name,argc,is_stdcall)` + `find_sig` do the lookup.

### CRITICAL ABI FACTS
- Host WINAPI functions are SysV (`WINAPI_CALL` empty) ⇒ cif uses **FFI_DEFAULT_ABI**.
- Every guest arg passed as **`ffi_type_uint64`** (never uint32 — truncates the top
  half of pointer args).
- `alloc_api_thunk`: consults `find_sig` to override argc/stdcall; builds `ffi_cif`
  with `arg_types[] = &ffi_type_uint64` for each slot (wide slots still ONE uint64
  arg, but consume 2 stack slots). Returns `kThunkBase + i*32`.
- `x86_dispatch_api(s, thunk_addr, ret_addr)`:
  - known (in sig table): reads argc slots from `[esp]` (wide → combine 2 words);
    calls host fn; `rvalue` → EAX (+EDX for 64-bit); stdcall pops `4*argc`.
  - unknown: reads `t.info.argc` 32-bit words (the GetProcAddress default path).
  - Sets `g_x86_dispatch_active = true`, `set_guest_vararg_slot(4)` around the call
    (restored to 8 after).
  - **SEH resume** (BOTH paths): if `x86_seh_resume_state().active` → apply
    {eip,esp,ebp,edx}, clear. (The `!known` path lacked this — added with the
    C++ EH push; without it a raise from an unknown-signature thunk only resumes
    on the NEXT api dispatch.)
  - `THETAZ_TRACE_API` prints `[api] dll!name argc=N args=[...] -> 0x...`.
- `x86_alloc_proc_thunk(fn, name)` (GetProcAddress): guards `name<0x10000` (ordinal
  used as string ptr); looks up sig by name; default stdcall 1-arg.

---

## 4. THE API SURFACE (core/api/)

### 4.1 Guest allocators (api.cpp)
- Guest heap `0x60000000..0x76000000` (352MB): first-fit free list,
  `HeapBlock{size,next,prev,free}` (8-byte aligned), coalescing both directions,
  split threshold `sizeof(HeapBlock)+8`. `alloc_guest_heap/free/realloc/calloc`.
- Shared low bump `0x7D000000..0x7DF00000` (15MB): `alloc_guest_shared(size)`,
  0x1000-rounded, MAP_FIXED_NOREPLACE, returns nullptr at exhaustion.
- `load_guest_module(path)` finds-or-loads via PELoader. `get/set_main_module_base`.

### 4.2 ApiRegistry
`by_name_[lower_dll][name]`, `by_ordinal_[lower_dll][ordinal]`; `register_api(dll,name,
func)`, `register_api_ordinal`. `normalize_dll` lowercases. `initialize_api_layer` =
kernel32 + oleaut32 + ws2_32.

### 4.3 Handles / magic values (kernel32_runtime.cpp)
- Std handles: kStdIn=-10 (0xFFFFFFF6), kStdOut=-11, kStdErr=-12.
- File handles from 0x1000; INVALID_HANDLE_VALUE = -1 (0xFFFFFFFFFFFFFFFF).
- Builtin module handles from `g_next_builtin=0x78000000`, +0x1000 per dll
  (builtin set: api-ms-win-*, ext-ms-*, + 25 named dlls incl. kernel32, user32,
  oleaut32, ntdll, ucrtbase, msvcrt, advapi32, gdi32, ole32, shell32, comctl32,
  comdlg32, ws2_32, version, wininet, shlwapi, setupapi, msi, mpr, netapi32,
  secur32, crypt32, rpcrt4, winmm, imm32).
- Api-set mapping: `api-ms-win-crt-*`→ucrtbase.dll; `api-ms-win-core-*`→kernel32.dll.
- GetProcAddress ws2_32 ordinals: {115 WSAStartup, 116 WSACleanup, 117 WSAGetLastError,
  118 WSASetLastError, 1 accept, 2 bind, 3 closesocket, 4 connect, 8 htonl, 9 htons,
  10 ioctlsocket, 11 inet_addr, 12 inet_ntoa, 13 listen, 14 ntohl, 15 ntohs, 16 recv,
  17 recvfrom, 18 select, 19 send, 20 sendto, 21 setsockopt, 22 shutdown, 23 socket,
  25 gethostbyname, 30 gethostname}.
- Time: `EPOCH_1601_TO_1970=116444736000000000ULL`; QPF=1,000,000,000.
- `GetSystemInfo` (memset 96): +0 arch=0 (INTEL), +4 page=0x1000, +8 minApp=0x10000,
  +16 maxApp=0x7FFE0000, +28 numProcs=sysconf, +32 procType=0, +36 gran=0x10000.
- `GetVersionExW` (memset 284): cbSize=276, 6.1.7601, platform 2 (NT).
- `GlobalMemoryStatusEx`: ullTotalVirtual=ullAvailVirtual=0x7FFF0000.
- `GetStartupInfoW` (x86): memset 68; cb@0=68; wShowWindow@48=7; hStdIn@56=-10;
  hStdOut@60=-11; hStdErr@64=-12.
- `GetSystemPowerStatus`: ACLine=1, BatteryFlag=128, LifePercent=100, times 0xFFFFFFFF.
- `GetVolumeInformationW`: serial 0x7468E7A5, FS "NTFS"; disk free 8/512/1M/2M;
  `GetDriveTypeW`→3; `GetTempPath*`="/tmp"; `GetProductInfo`→0x06;
  `GetPhysicallyInstalledSystemMemory`→8*1024*1024; `GetPriorityClass`→0x20;
  `GetExitCodeThread`→0x103; `IsWow64Process`→TRUE; `GetProcessHandleCount`→32.
- `IsProcessorFeaturePresent(23)` → FALSE (force off PF_FASTFAIL_AVAILABLE).
- Fake DCs from 0x10; GetStockObject→0x1000+fn(0..31); SelectObject→0x1001;
  CreateFontA→0x1002; CreateSolidBrush→0x1003; CreatePen→0x1004;
  GetClientRect→{0,0,800,600}; GetSystemMetrics 0→1920,1→1080,else 800;
  GetDeviceCaps 12/90→96,4→1920,8→1080,10→32.
- Process heap=(HANDLE)1, HeapCreate→(HANDLE)2, IOCP→0x3000,
  memory-resource-notification→0x3001, file-mapping handles from 0x2000.

### 4.4 CreateFileW flag mapping
O_RDWR default; access bit1→O_RDONLY, bit2→O_WRONLY, bits1|2→O_RDWR;
dispositions 1=O_CREAT|O_EXCL, 2=O_CREAT|O_TRUNC, 3=open, 4=O_CREAT, 5=O_CREAT|O_TRUNC;
mode 0644; reopen fallback on EEXIST.

### 4.5 ws2_32 (ws2_32.cpp)
- WSAData EXACTLY 400 bytes: wVersion@0, wHighVersion@2, szDescription[257]@4,
  szSystemStatus[129]@261, iMaxSockets@390, iMaxUdpDg@392, **lpVendorInfo(uint32)@394**.
  (Host `char*` would make it 408 — this /GS bug corrupted the stack cookie.)
- WSAStartup: wVersion=wHighVersion=0x0202, desc "ThetaZ Winsock 2.0", status
  "Running", iMaxSockets=1024, iMaxUdpDg=65535; null WSADATA → 10022.
- Socket handles from `g_next_wsock=0x100`; INVALID_SOCKET=~0ull.
- `ws_errno_to_wsa` table + `g_ws_last_error` thread-local.
- `select` (real): guest fd_set = 260 bytes (u32 count + 64 SOCKETs). Converts to
  host fd_sets via `wsock_fd` mapping, calls `::select`, rewrites the guest sets to
  the READY sockets only, returns the count or SOCKET_ERROR(-1).
- `gethostbyname` (fixed): builds a **guest-visible hostent copy** in
  alloc_guest_shared: 16-byte header + h_name string + alias strings + pointer
  array + addr_list + addr blobs (≤64 addrs / ≤8 aliases). Never returns the host
  `hostent*` (>4GB) anymore.

### 4.6 oleaut32 (oleaut32.cpp)
- BSTR: 4-byte little-endian byte length (excludes terminator) + data + null.
  API returns data pointer; `free_bstr = ((uint8_t*)bstr)-4`. **Now on the guest
  heap** (`alloc_guest_heap(bytes+4+2)` / `free_guest_heap`) — no more host-malloc
  >4GB pointers.
- VARIANT = 16-byte opaque; vt@0; VariantClear frees BSTR ptr at offset 8.
  `VariantChangeType` just copies. VT_EMPTY=0, VT_BSTR=8.
- GetErrorInfo → S_OK, *pperrinfo=nullptr; SetErrorInfo no-op.

### 4.7 ucrt (ucrt.cpp)
- Vararg slot `thread_local g_vararg_slot = 8` (32-bit dispatcher sets 4 around calls).
- Own formatter: `%d i u x X o c s p f e g l{ld lu lx lf lld llu llx} z{zd zu zx}`.
- FILE token registry: guest token (alloc_guest_shared(sizeof(void*))) ↔ glibc FILE*.
  `__acrt_iob_func(0..2)` → stdin/stdout/stderr tokens. `ucrt_fwrite` only writes
  stdout/stderr tokens.
- Shared CRT: `g_shared_argc`, `g_shared_argv` (256 ptrs), `g_shared_argv_ptr`
  (returned by `__p___argv`), `g_shared_env`, `g_shared_env_ptr`, `g_shared_name`
  ("thetaz"), `g_shared_acmdln` (returned by `__p__acmdln`). Default argc=1.
- `set_crt_command_line(argc,argv)`: guest argc = host argc-1 capped 255.
- atexit: LIFO `g_atexit_queue`; 32-bit callbacks via `x86_call_guest` when
  dispatch active, else native ms_abi.
- `__C_specific_handler` returns 0 (ExceptionContinueSearch) always.
- Registration triple-writes each func under api-set + `ucrtbase.dll` + `msvcrt.dll`.

### 4.8 process.cpp
- **x86 PROCESS_INFORMATION (16 bytes)**: hProcess@0, hThread@4, pid@8, tid@0xC.
  hProcess=child_pid, hThread=tid, dwProcessId=child_pid, dwThreadId=tid.
- Child pids from `g_next_child_pid=0x10000`. STILL_ACTIVE=0x103.
- **Self-relaunch**: if target fails to load AND matches main module's name prefix →
  load a FRESH copy of the image (own .data/relocations), run via
  `x86_run_guest_thread`, mark child terminated.
- `terminate_current_process`: pid==0 → host `_exit(code)` (skip host atexit);
  child → ThetaServer::terminate_process + ThreadManager::exit_thread.

### 4.9 GetProcAddress (kernel32_runtime.cpp)
- Wraps host fn into a 32-bit thunk ONLY when `x86_dispatch_active()`;
  else returns the host pointer (64-bit guests).
- Ordinals: must map ord→name BEFORE thunk alloc (else ordinal value used as a
  string pointer crashes at host addr 0x73). Lookup: dll → api_set_target →
  kernel32.dll fallback.
- `dll_name_for_handle(hModule)` maps builtin handles → dll names.

### 4.10 EncodePointer / DecodePointer (the __security_cookie scheme)
The guest `__security_cookie` lives at `image_base + 0x66e780` (0xa6e780 for base
0x400000), randomized per run by the app's CRT. OfficeSetup decodes encoded function
pointers with:
```
decoded = ror(ptr ^ cookie, cookie & 0x1f)
```
so the emulator implements:
```
EncodePointer(ptr) = rotl(ptr, cookie & 0x1f) ^ cookie
DecodePointer(ptr) = rotr(ptr ^ cookie, cookie & 0x1f)
```
`guest_security_cookie()` reads `*(uint32_t*)(0x400000 + 0x66e780)` (identity-mapped
direct read; returns 0 if unmapped). Round-trip verified. Both of the app's own
encode fns (0x820035, 0x822cc0) use `ror(ptr, 0x20-(cookie&0x1f)) ^ cookie`
= the same `rol(ptr,cl)^cookie`.

### 4.11 Path translation
`wine_to_host(path)` (kernel32_runtime.cpp): any drive-letter/UNC path → mapped to
`$WINEPREFIX/drive_c/...`, `\`→`/`. WINEPREFIX default =
`/home/eronic/Prefixes/ThetaZ`. (environment.cpp's `EnvironmentSetup` uses a SEPARATE
`~/.ThetaZ` prefix — two prefix systems coexist.)

---

## 5. SEH — RaiseException TWO-PASS DISPATCH (kernel32_runtime.cpp)

Constants: `kGuestTeb=0x7FFF0000`, `kDispExecuteHandler=1`,
`kDispContinueExecution=0xFFFFFFFF`, `kSehUnwinding=0x2`, `kSehExitUnwind=0x4`.

- Allocates guest-visible `record` (128 bytes) + `ctx` (0x300) via alloc_guest_shared.
- record layout: +0 code, +4 flags, +12 ExceptionAddress (=EIP), +16 NumberParameters,
  +20.. (+20+4*i) args (max 15).
- ctx (x86 CONTEXT): +0 ContextFlags=0x10007 (CONTEXT_i386|CONTEXT_FULL);
  Edi=0x9C, Esi=0xA0, Ebx=0xA4, Edx=0xA8, Ecx=0xAC, Eax=0xB0, Ebp=0xB4, Eip=0xB8,
  SegCs=0xBC, EFlags=0xC0, Esp=0xC4, SegSs=0xC8 — populated from `x86_current_state()`.
- **Pass 1**: walk the fs:0 frame chain (`fs_head = *(uint32_t*)0x7FFF0000`; frame
  layout prev@+0, handler@+4; max depth 32), call each handler via
  `x86_call_guest_args(handler, {record, frame, ctx, 0}, 4)`. Disposition 1 (EXECUTE
  HANDLER) → snapshot resume {Eip=ctx+0xB8, Esp=ctx+0xC4, Ebp=ctx+0xB4} and stop.
  Disposition 0xFFFFFFFF (CONTINUE EXECUTION) → immediately set `X86SehResume` from
  those offsets and return. Unknown nonzero disposition stops the search.
- **Pass 2 (unwind)**: re-call every frame from fs_head down to (excluding) the
  handling frame with `ExceptionFlags = dwExceptionFlags | 0x2 | 0x4`.
- Finally `X86SehResume.active = true` with saved eip/esp/ebp; `x86_dispatch_api`
  applies it to the interpreter state after the API call returns.

### DISPATCHER_CONTEXT + handler invocation
- `build_guest_dispatch_context(frame, ctx, pc)` (kernel32_runtime.cpp) allocates a
  guest-visible **x86 DISPATCHER_CONTEXT (56 bytes)**: ControlPc@0, ImageBase@8 =
  `get_main_module_base()`, FunctionEntry@0x10=0, EstablisherFrame@0x18=frame,
  TargetIp@0x20 (filled by handlers), ContextRecord@0x28=ctx, HandlerData@0x38=
  `*(uint32_t*)(frame+8)` (the scope table), ScopeIndex@0x48=0.
- `call_seh_handler(handler, record, frame, ctx, disp)`: **if the handler address is
  an IAT thunk (0x76000000 region — imported `__C_specific_handler`), call
  `api_thunk_at(handler)->fn` directly with the 4 guest pointers** (never
  `x86_call_guest_args` on a data-region thunk). Otherwise guest-call it.
- SEH resume reads `disp->TargetIp` (+0x20) first, falling back to `ctx+0xB8`;
  RtlUnwind uses the same machinery.

### ucrt `__C_specific_handler` — TABLE-DRIVEN (ucrt.cpp ~612)
MSVC x86 frames push `{prev, __C_specific_handler, scope_table}` — table lives at
`[frame+8]`. Scope entries are **16 bytes {begin, end, handler, jump}**, ImageBase-
**relative RVAs** (no pdata; OfficeSetup's exception directory is empty).
- Pass 1 (rec flags & 0x6 == 0): walk entries; entry with `handler==0xFFFFFFFF` =
  `__except` block → accept unconditionally (write `base+jump` to disp TargetIp,
  return 1); else run the filter via `x86_call_guest_args(filter, {record, frame}, 2)`:
  filter returns 0 → continue; 1 → accept; -1 → ContinueExecution. Scopes with a
  nonzero jump are inert `__finally` (skipped in pass 1).
- Pass 2 (rec flags & 0x6): run `__finally` handlers (`handler(TRUE, frame)`); return
  0. `__C_specific_handler` is a 32-bit IMPORT (IAT thunk 0x760005e0 seen in tests)
  — resolved via GetProcAddress; `vcruntime140.dll`/`vcruntime140_1.dll` are
  builtin-DLL names (kernel32_runtime.cpp `is_builtin_dll` ~212).

`x86_seh_resume_state()` returns `thread_local X86SehResume{active,eip,esp,ebp,edx}`;
`x86_current_state()` returns `g_fault_state`. **EDX resume**: C++ catch funclets
receive the (PMD-adjusted) exception object in EDX — RaiseException reads
`ctx+0xA8` (the CONTEXT Edx slot — NOT 0xAC which is Ecx) into `seh.edx`, and
`x86_dispatch_api` applies it. **Both dispatch paths** (known-signature AND the
unknown-signature `!t.info.known` thunk path) apply the resume after `ffi_call` —
a GetProcAddress-resolved thunk whose sig row is missing falls into the second
path; without the check the raise's resume is only consumed by the NEXT API
dispatch (observed: the thrower's fallthrough `puts` got redirected instead of
the throw call itself).

### ucrt `__CxxFrameHandler` / `_CxxThrowException` — C++ EH (ucrt.cpp ~985)
- `_CxxThrowException(pObject, pThrowInfo)` raises `0xE06D7363`,
  `EXCEPTION_NONCONTINUABLE`, 4 params `{magic, pObject, pThrowInfo, 0}`
  (magic 0x19930520/21/22 = FuncInfo V2/V3/V4).
- Registration records (probed by FuncInfo magic at the slot): EH3
  `{prev@0, handler@4, ebp@8, pFuncInfo@0xC, tryLevel@0x10, xp@0x14}` and EH4 /GS
  `{prev@0, handler@4, ebp@8, cookie@0xC, pFuncInfo@0x10, tryLevel@0x14, xp@0x18}`.
- FuncInfo V2 `{magic@0, maxState@4, pUnwindMap@8, nTryBlocks@12, pTryBlockMap@16,
  nIPMapEntries@20, pIPToStateMap@24, nCatchHandlers@28, pCatchHandlerArray@32}`;
  V3/V4 adds `{flags@36, pUnwindMapForUnexpected@40}`. **These offsets are the real
  MSVC ehhdr.h layout — an earlier implementation read nTryBlocks/nIPMapEntries at
  +16/+24 and never matched any try block (n_try = the pointer value → >256 bail).**
- UnwindMapEntry `{toState, action}` (8B), TryBlockMapEntry `{tryLow, tryHigh,
  catchHigh, nCatches, pHandlerArray}` (20B), CatchHandlerType `{adjectives,
  pType, handler}` (12B), IPToStateMapEntry `{ip, state}` (8B, sorted; state of
  last entry with ip <= pc wins; 0 = outside all trys).
- Pass 1: state from `ctx+0xB8` → try blocks innermost-first (state in
  [tryLow, tryHigh)) → for each catch, `cxx_type_match`: catch TypeDescriptor name
  (`pType+8`, e.g. `.?AVBase@@`) vs each thrown CatchableType's pType name; PMD at
  `p_ct+8`: `pdisp==-1` → non-virtual base, subobject = object + mdisp; else
  virtual base → `object + pdisp` → vbtable → `vbtable[vdisp]` added to mdisp.
- On match: run own destructor funclets for abandoned scopes (`cxx_unwind_frame`,
  states down to catchHigh, action < 0x80000000 = funclet), write `tryLevel =
  catchHigh` at `frame+try_level_off`, then **resume-context: Eip = funclet,
  EBP = frame's saved ebp (`frame+8`), ESP = frame address, EDX = object + mdisp
  (frameless catch funclet ABI — no return address; funclet jumps to the
  continuation)**. Return 1 (EXECUTE_HANDLER).
- Pass 2 (flags & 0x6): run unwind funclets from tryLevel down to 0, return 0.
- Registered on **vcruntime140.dll only** (like `__C_specific_handler`): names
  `__CxxFrameHandler`, `__CxxFrameHandler3`, `_CxxThrowException`.
- **Signature table**: `_CxxThrowException` MUST be a `CT("ucrtbase.dll", ...)`
  row — `find_sig` canonicalizes vcruntime140→ucrtbase, so a row keyed
  `"vcruntime140.dll"` never matches and the thunk lands in the `!known`
  dispatch path (fixed this session; the resume check there now makes it safe
  either way).
- Test `tests/cxx_eh_test.c` (hand-built V4 FuncInfo + EH4 records): inner frame
  catch WrongType declines, outer catch of base type Base matches thrown Derived
  (PMD +4), catch funclet resumes via EDX, pass-2 unwind funclet runs —
  `CXX PASS caught=0x403024 expected=0x403024 unwound=1`, exit 0.

---

## 6. X11 GUI LAYER (core/api/window.cpp)

### 32-bit guest struct layouts (verified — the WNDPROC is a guest function)
```
MSG (x86): hwnd@0, message@4, wParam@8, lParam@0xC, time@0x10, pt@0x14, size 0x1C
WNDCLASSA (x86): style@0, lpfnWndProc@4, cbClsExtra@8, cbWndExtra@0xC, hInstance@0x10,
                 hIcon@0x14, hCursor@0x18, hbrBackground@0x1C, lpszMenuName@0x20,
                 lpszClassName@0x24, size 0x28
CREATESTRUCTA (x86): lpCreateParams@0, hInstance@4, hMenu@8, hwndParent@0xC, cy@0x10,
                     cx@0x14, y@0x18, x@0x1C, style@0x20, lpszName@0x24,
                     lpszClass@0x28, dwExStyle@0x2C, size 0x30
PAINTSTRUCT (x86): hdc@0, fErase@4, rcPaint@8, size 0x40
```
All reads/writes use 32-bit widths (64-bit reads corrupt hwnd/message).

### Message pump
- `g_queue` (deque<GuestMsg>) + `g_queue_cond`; `PostQuitMessage` sets
  `g_quit_pending` + `g_quit_exit_code` and pushes WM_QUIT.
- `GetMessageA` blocks on `g_queue_cond`; WM_QUIT never filtered. `PeekMessageA`
  scans with optional remove. `DispatchMessageA` looks up wnd_proc and calls it.
- `call_wnd_proc(wnd_proc, hwnd, message, wparam, lparam)`: **32-bit WNDPROC goes
  through `x86_call_guest_args(wnd_proc, {hwnd,msg,wparam,lparam}, 4)`**; 64-bit via
  ms_abi function pointer.
- Window registry: `g_windows[hwnd]`, `g_classes[classname]`, `g_next_hwnd=0x1001`,
  `kDesktopHwnd=0x1000`.
- `WM_CREATE` returns -1 ⇒ creation fails (window erased).

### X11 backend
- `x11_display_init()`: `XInitThreads()` + `XOpenDisplay(nullptr)`. Event thread
  started (and **detached**) in `initialize_window_layer`.
- `CreateWindowExA` → `XCreateSimpleWindow` + `XSelectInput(Exposure|Button|
  Key|StructureNotify)` + `XStoreName` + WM_DELETE_WINDOW protocol.
  Registers `g_x11_windows[hwnd]=xw`, `g_x11_hwnds[xw]=hwnd`.
- `ShowWindow` → `XMapRaised`/`XUnmapWindow` + `XFlush`.
- **Event loop MUST poll**: `while (XPending(d)==0) usleep(5000); XNextEvent(...)`.
  (Blocking XNextEvent deadlocks with other threads using the display.)
  Expose→WM_PAINT; ButtonPress→WM_LBUTTONDOWN(0x0201)/release(0x0202) — except
  controls, which intercept clicks (buttons post WM_COMMAND, see below);
  KeyPress→WM_KEYDOWN+WM_CHAR; ClientMessage→WM_CLOSE; ConfigureNotify updates
  win->w/h.
- **Locks**: `g_window_mutex` guards g_windows/g_queue/g_x11_windows/g_x11_hwnds.
  `g_x11_mutex` serializes ALL Xlib calls. Lock order ALWAYS window→x11, never nested.
  `x11_paint_window`/`theta_gdi_text` copy data under g_window_mutex, RELEASE, then
  do X calls under g_x11_mutex. ShowWindow releases g_window_mutex before X calls.
  (ShowWindow's self-deadlock re-locking g_window_mutex was a real bug.)
- `theta_gdi_text(hwnd,x,y,utf8)` renders via XDrawString (called by TextOutA/W).
- `theta_register_dc_hwnd(dc,hwnd)` + `theta_gdi_hwnd_for_dc(dc)` (kernel32_runtime)
  route GDI text to the right window. BeginPaint registers DC 0x1; GetDC registers
  its DC per hwnd.

### Controls (predefined BUTTON/STATIC/EDIT/LISTBOX/COMBOBOX classes)
- `lookup_control_class(name)`: case-insensitive match against user32's
  predefined classes; these have **no guest WNDPROC** — behavior lives in this
  layer (`Window::ctrl_type`, `Window::id` = hMenu low word, `Window::parent`).
- `CreateWindowExA` with a control class creates an X11 **subwindow** of the
  parent's X11 window (XSelectInput = Exposure|Button only; no WM_DELETE) and
  skips WM_CREATE (no proc to call).
- **Buttons post WM_COMMAND (0x0111) to the parent**: wParam = control id
  (MAKEWPARAM(id, BN_CLICKED=0)), lParam = control hwnd. Two paths:
  - X11 ButtonRelease on a control → `post_button_command` (posted to queue);
  - `SendMessageA(btn, BM_CLICK)` → synchronous `call_wnd_proc(parent_proc,
    parent, WM_COMMAND, id, h)` (gathered under the lock, called after release —
    never call a guest WNDPROC while holding `g_window_mutex`).
- **Checkboxes** (style bits 0-1 = BS_CHECKBOX 2 / BS_AUTOCHECKBOX 3):
  `Window::checked`; BM_GETCHECK (0xF0) → checked?1:0; BM_SETCHECK (0xF1) sets +
  repaint; X11 ButtonRelease toggles + repaints, then posts BN_CLICKED.
- `x11_paint_window` draws controls: button label centered, checkbox with
  `[x]`/`[ ]` marker, statics left-aligned (host-side; guests get WM_PAINT as
  a notification only).
- `DestroyWindow` also erases children from g_windows + the X11 maps (X11
  subwindows die with the parent window automatically).
- Test: `tests/ctrl_test.c` (CTRL PASS) — button/checkbox/static creation,
  BM_GETCHECK/SETCHECK roundtrip, BM_CLICK → WM_COMMAND ids 1,2.

### Timers (SetTimer/KillTimer, WM_TIMER=0x0113)
- `GuestTimerInfo {hwnd,id,func,period,due}` map keyed `(hwnd<<32)|id`.
- Background `timer_thread_main` (detached, started in `initialize_window_layer`):
  every ~10ms checks due timers, posts WM_TIMER (wParam=id, lParam=func) to the guest
  queue, reschedules periodic, erases one-shots.
- `WINAPI_SetTimer`: `id==0` → auto-assign next id; returns 0 on failure.
  `WINAPI_KillTimer` erases the entry. Signature table: SetTimer(4)/KillTimer(2).

---

## 7. CORE/NTDLL (64-bit guest + thread model)

### ThreadManager (thread.cpp)
- tids: `++next_thread_id_` from 0 → 1,2,3...; never recycled.
- Maps: `threads_[tid]=ThreadRecord`, `teb_to_tid_[teb]`, `pthread_to_tid_[pthread]`.
- `create_thread(start_addr, param, stack_size, pid)`: start_addr==0 registers the
  main host thread (reuses main TEB). Else mmap stack (default 1MB),
  `setup_thread_teb(peb, tid, stack_base, stack_top)`, write TEB+0x048=tid,
  pthread_create + detach.
- `thread_entry(arg)`: set_gs_base(teb) for guest threads; set g_current_windows_tid;
  if pid!=0 → g_current_guest_pid + register_thread; if start_addr<4GB →
  `x86_run_guest_thread`; on child exit → `ThetaServer::terminate_process(pid,rc)` +
  `exit_thread(rc)`. 64-bit path: call proc directly (not exercised).
- Suspend/resume: SIGUSR1/SIGUSR2 with a thread_local {mutex, cv, suspended}.
- `terminate_thread` uses pthread_cancel; `exit_thread` records + pthread_exit.

### WindowsAddressSpace (virtual.cpp)
- Two 8GB PROT_NONE zones; high zone base shifted by `0x7FFFFFFE00000000`.
- `reserve/reserve_and_commit/commit/decommit/release/protect/query`.
- `page_to_prot`: masks GUARD|NOCACHE|WRITECOMBINE; NOACCESS→NONE, READONLY→R,
  READWRITE/WRITECOPY→RW, EXECUTE→X, EXECUTE_READ→RX, EXECUTE_READWRITE/WRITECOPY→RWX.
- `query`: committed → state=MEM_COMMIT + block protect; else MEM_RESERVE +
  PAGE_NOACCESS; type always MEM_PRIVATE; no covering region → false.
- PAGE_GUARD not modeled (no trap).

### sync (sync.cpp)
Semaphore (counting), Event (manual/auto; auto-reset NOT consumed by wait),
CriticalSection (recursive, owner=Windows tid), Mutex (recursive), SpinLock,
ReaderWriterLock (writer-preferring). `WaitHandle` tagged union of shared_ptrs.
`create_mutex` ignores the initial flag. A SECOND parallel implementation lives in
ThetaServer (EventObject/SemaphoreObject/MutexObject).

### teb (teb.cpp) — 64-bit guest
```
teb_offsets: ExceptionList=0x00, StackBase=0x08, StackLimit=0x10, SubSystemTib=0x18,
  FiberData=0x20, ArbitraryUserPointer=0x28, Self=0x30, EnvironmentPointer=0x38,
  ClientId_UniqueProcess=0x40, ClientId_UniqueThread=0x48, TlsPointer=0x58,
  ProcessEnvironmentBlock=0x60, LastErrorValue=0x68, DeallocationStack=0x1780,
  TlsSlots=0x1800, TlsExpansionSlots=0x17A0, TEB_SIZE=0x2000
peb_offsets: ImageBaseAddress=0x10, Ldr=0x18, ProcessParameters=0x20,
  ProcessHeap=0x30, SessionId=0x58, PEB_SIZE=0x800
params_offsets: ImagePathName=0x60, CommandLine=0x70, PARAMS_SIZE=0x1000
```
- `setup_windows_teb(image_base, cmdline)`: alloc PEB(0x800)+TEB(0x2000) via
  `alloc_zero` (leaked), sets g_main_peb/g_main_teb, writes PEB fields, UNICODE_STRINGs
  (Length excludes null, MaximumLength includes), stack from pthread_getattr_np,
  `set_gs_base(teb)` via `syscall(SYS_arch_prctl, ARCH_SET_GS, teb)`.
- `set_gs_base` per-host-thread; FS stays Linux libc TLS.

---

## 8. EXCEPTION (64-bit host SEH) + IPC

### exception.cpp
- Vectored handlers: file-local singly-linked list `g_exception_handlers`
  (`VectoredHandlerEntry{handler,next}`) + `g_exception_mutex`.
  AddVectoredExceptionHandler prepends; Remove erases; CallFirstVectoredExceptionHandler
  walks until disposition != EXCEPTION_CONTINUE_SEARCH.
- `ExceptionHandler::dispatch(code, ctx, user_data)` order:
  1. host-registered handlers map → return true if claimed;
  2. guest vectored handlers;
  3. `walk_exception_chain` (TEB ExceptionList, x64 frames {Handler@0, Prev@8},
     depth cap 16, `SehHandler = LONG(WINAPI_CALL*)(WinExceptionRecord*,uint64_t,void*,void*)`);
  4. top-level filter (SetUnhandledExceptionFilter); else `unhandled_exception`.
- ExceptionCode enum: AccessViolation=0xC0000005, IntDivByZero=0xC0000094,
  IntOverflow=0xC0000095, FloatDivByZero=0xC0000096, BoundCheck=0xC0000097,
  InvalidOpcode=0xC000001D, StackOverflow=0xC00000FD, InvalidHandle=0xC0000008,
  InvalidParameter=0xC000000D, ControlCExit=0xC000013A.
- except_offsets: Handler=0x00, Prev=0x08. except_record_offsets:
  ExceptionCode=0, Flags=4, Record=8, Address=0x10, NumberParameters=0x18,
  ExceptionInformation=0x1C.

### signal_x86_64.cpp
SIGSEGV→AccessViolation, SIGFPE→IntegerDivideByZero, SIGILL→InvalidOpcode.
`ExceptionSetup64` installs SA_SIGINFO|SA_ONSTACK handlers; captures full ucontext
into CPUContext64, calls `handler->dispatch`; on true writes back regs/RIP/EFLAGS/XMM
and returns; else SIG_DFL + raise.

### IPC (server.cpp) + ThetaServer (thetaserver.cpp)
- Port 12345; wire `[type u32][length u32][request_id u32][data...]`;
  response `[status u32][length u32][data...]`. Multi-frame parsing, 64KB read cap.
  `send_message` is effectively dead (pending_messages_ never populated).
- ThetaServer: objects `unordered_map<handle, shared_ptr>`; handles from
  `next_handle()` atomic starting 0x1000; **process/thread handles = pid/tid**
  (bypass the counter). `register_process/thread`, `terminate_process` (keeps the
  process object, cleans its handle table), `terminate_thread` (propagates to the
  process if it was main_thread_id). Waits dispatch by ObjectType; auto-reset events
  clear after wait; semaphores decrement; mutexes recurse; wait_for_any/all are 5ms
  polling loops. `[θserver]` log prefix.

---

## 9. KNOWN BUGS / GAPS

1. **OfficeSetup exits with ExitProcess(1)** after a clean parent teardown — the
   app's own normal exit for this stage (child exits 0/1, parent exits 1). Two
   teardown blockers were FIXED this session:
   - `WINAPI_HeapSize` returned 0 → the app's grow helper 0x820603
     (`HeapSize` → `HeapReAlloc` → `memset(new+oldsize,0,...)`) zeroed the WHOLE
     new buffer, destroying the copied old entries; the teardown decoded zero
     slots (`ror(0^cookie,cl)` = rotation of the cookie) and called garbage.
     Fix: `HeapSize`/`LocalSize`/`GlobalSize` → `ThetaZ::HeapBlockSize(ptr)`
     (api.h/api.cpp) returning the guest-heap block payload size.
   - `WINAPI_HeapAlloc` ignored `dwFlags` — `HEAP_ZERO_MEMORY (0x8)` never
     zeroed, so the app's registry `calloc(512,4)` reused a freed env-copy block
     (0x60001520, 0x192e bytes of host-env strings) and its tail (entries 3+)
     still held stale bytes → teardown walked garbage handles → `_invalid_parameter`
     → `_invoke_watson` → `RaiseException(0xC0000417)` → `ExitProcess(0xC0000417)`.
     Fix: `HeapAlloc` memsets when `dwFlags & 0x8`; `LocalAlloc`/`GlobalAlloc`
     zero when `uFlags & 0x40` (LMEM_ZEROIN/GMEM_ZEROIN).
2. **oleaut32 BSTRs use host malloc** — may exceed 4GB (latent).
3. **`gethostbyname` returns host `hostent*`** — may exceed 4GB (latent).
4. **`select` (ws2_32) is a stub**; host fd_set layout differs. (DONE: real 260-byte
   fd_set rewrite — see §4 ws2_32; stale note.)
5. **`__CxxFrameHandler`** V2/V3 unwind of `__finally`-style scopes beyond the
   catch scope, and `__CxxFrameHandler4` (x64-style fully-digested tables), not
   tested. Classic `__except_handler3` / `__except_handler4_common` and
   `__C_specific_handler` + `__CxxFrameHandler` V4 catch/unwind are DONE.
6. **Event::wait auto-reset not consumed**; `create_mutex` ignores initial flag.
7. **`kernel32_api_additions.cpp`** is dead code with duplicate symbols (never build).
8. **path_resolver.cpp / environment.cpp** are scaffolds; real translation is
   `wine_to_host`. Two prefix systems (WINEPREFIX vs ~/.ThetaZ) can disagree.
9. **Interpreter gaps**: unknown opcodes → interp_fault; SSE/x87 not 100% complete.
10. **In-process child sharing**: self-relaunch children share the guest heap and
    process globals with the parent (fundamental in-process limitation).
11. **`send_message` (IPCServer)** always returns false (unused path).
12. **GetThreadId/thread context APIs** are stubs; Wow64GetThreadContext=FALSE.

---

## 10. DEBUGGING CHEAT SHEET

- `THETAZ_TRACE_INSNS=1` — 128-entry EIP ring dumped on fault; also per-insn dump.
- `THETAZ_TRACE_API=1` — `[api] dll!name argc=N args=[...] -> ret (caller=0x...)` —
  caller = the guest EIP after the call (the return address).
- Fault EIP that is a rotation of the __security_cookie ⇒ zero entry decoded in the
  encoded-pointer container (OfficeSetup teardown).
- `objdump -d app.exe` + `objdump -p app.exe` for the guest-side analysis; guest
  identity-mapping means you can read guest addresses from the host process.
- Stale IPC on port 12345: `for p in $(pgrep -x ThetaZ); do kill -9 $p; done`.
- The X11 display :0 does NOT support X_GetImage → xwd/import fail; verify GUI via
  `xdotool search --name` + the app's `[user32]` stderr logs (redirect to file and
  check — stdout printf is buffered and lost on SIGKILL).
- To attach a debug hook in the interpreter: guard on `s.eip == 0xADDR` in the main
  loop right after `trace_push(s.eip)` (exact-match on eip misses mid-instruction
  eips — use a range and print `s.eip`).

---

## 11. MEDIA FOUNDATION (MF) ARCHITECTURE — Design notes for ThetaZ

> Status: **research/design only**. Code: not yet implemented. This section is
> a distilled reference for what would need to be built to make modern Windows
> media (AVPro, VideoPlayer plugins, browser video, Office 365 web content) work
> in ThetaZ.

### 11.1 The MF DLL surface (what VRChat's AVPro calls)

VRChat's AVPro plugin and Office 365's video/streaming surfaces all call into
this stack of DLLs:

| DLL | Purpose | ThetaZ impl? |
|---|---|---|
| `mfplat.dll` | Core MF platform: work queues, sample allocators, attribute store, source resolver, transforms registry | not yet |
| `mfreadwrite.dll` | `IMFSourceReader`, `IMFSinkWriter` — the high-level read/write API used by most video code | not yet |
| `mfmediaengine.dll` | `IMFMediaEngine`, `IMFMediaEngineClassFactory` — the modern playback engine that decodes to a `D3D11_TEXTURE2D` for the app to display | not yet |
| `mf.dll` | The session/topology loader that wires transforms together (`IMFMediaSession`) | partial (none) |
| `winegstreamer.dll` | The actual decode work: turns MF calls into GStreamer pipeline operations | not yet |
| `mfh264enc.dll` | H264 encoder (rare path) | not yet |
| `mfmp4srcsnk.dll` | MP4 source/sink | not yet |
| `mfsrcsnk.dll` | Generic source/sink | not yet |

### 11.2 The IMFMediaEngine path (the VRChat TV path)

This is the call chain from AVPro setting a video URL to pixels appearing on
a 3D surface in Unity:

```
AVPro call
  → CoCreateInstance(CLSID_MFMediaEngineClassFactory)
  → IMFMediaEngineClassFactory::CreateInstance(flags, attrs, &engine)
  → IMFMediaEngineEx::SetSource(url)
  → IMFMediaEngineEx::SetSourceElements / SetVideoTexture (D3D11 texture)
  → IMFMediaEngineEx::Play()
        ↓
  internally: MF creates a topology (IMFMediaSession) that looks like:
        Source (URL/byte-stream)
          ↓ (compressed H264/H265/AAC bytes)
        Decoder (mfh264dec via winegstreamer)
          ↓ (raw NV12 video frames)
        Color converter (NV12 → BGRA or NV12 → texture)
          ↓
        Sink (IMFMediaEngineEx's internal frame-sink queue)
          ↓ (IMFTransform::ProcessOutput to the app's callback)
        AVPro's texture callback → Unity RenderTexture
```

The TEX2D bridge happens in `IMFMediaEngineEx::SetVideoSurface` or via the
IMFMediaEngineExtension interface — AVPro specifically uses the extension
mechanism (upstream commit `072897c6f41`). Without that, the texture callback
never fires.

### 11.3 The winegstreamer layer (what actually decodes)

winegstreamer translates MF primitives into a GStreamer pipeline:

```
MF calls                                  GStreamer pipeline
----------                                 -----------------
IMFSourceResolver::CreateObjectFromURL →   gst_parse_launch("uridecodebin ...")
IMFByteStreamHandler::BeginRead          → appsrc ! ...
IMFMediaSource::Start                     → gst_element_set_state(pipeline, PLAYING)
IMFTransform::ProcessInput(sample)        → gst_buffer_new_wrapped(sample data)
IMFTransform::ProcessOutput               → gst_bus_timed_pop / sample callback
mf_format_to_gst_format(GUID)             → gst_caps_new_simple(...)
mf_color_convert(BGR24 ↔ NV12)             → videoconvert + videoscale_omx
```

The native formats that matter for ThetaZ:

- **H264 byte-stream** (Annex B): `video/x-h264, stream-format=byte-stream`
- **H264 AVC1** (length-prefixed): `video/x-h264, stream-format=avc` (MKV)
- **NV12**: `video/x-raw, format=NV12` (D3D11-friendly, no chroma subsampling
  trickery)
- **YV12 / I420**: `video/x-raw, format=YV12` (also needs alignment fix)

### 11.4 Critical bugs/misfeatures that block video on wine 11.0

These are the bugs the upstream Proton patches fix (see `Wine patches` commit
chain in proton-rtsp-full git log):

1. **NV12 alignment**: GStreamer may hand back buffers whose row pitch is the
   video width (e.g. 1920) but D3D11 requires 32-byte aligned rows. Without
   the `buffer_fix_nv12` copy step, the texture is corrupted → black screen.
   Wine patch: `8791d4da324`. Already in fork via newer version.
2. **MediaEngine sample queue too small**: default `MAX_QUEUED_SAMPLES=2`
   causes stalling under slow color conversion. Fix: bump to 5–16.
   Patch: `fe6a2883`. **NOT yet in our fork** (now applied — see proton fork
   commit `e92981ddbf2`).
3. **MF source resolver over-rejects**: required the
   `MF_RESOLUTION_CONTENT_DOES_NOT_HAVE_TO_MATCH_EXTENSION_OR_MIME_TYPE`
   flag even when no extension is present. Patch: `1c84c378466`.
   **NOT yet in our fork** (now applied).
4. **MediaEngine extensions**: `IMFMediaEngineExtension` (used by AVPro to
   install a custom texture sink) — wine didn't implement it. Patch:
   `072897c6f41`. Already in fork.
5. **Media-converter stream-discard race**: media-converter GStreamer element
   could deadlock on shutdown. Patch: `5274533e13a`. **NOT yet in our fork**
   (now applied).
6. **MediaEngine sample-pump blocking**: low-FPS scenes starved the source
   stream. Patch: `a7fcac55` (the commit our fork is based on).

### 11.5 What ThetaZ would need to implement (no copying from Wine)

Since ThetaZ is original code and Wine/Proton are study-only, a ThetaZ MF
implementation would be its own design — but informed by the above.

**Phase A (skeleton, 1-2 weeks):**
- `core/api/mfplat.cpp`: stub `mfplat.dll`. Exports: `MFStartup`, `MFShutdown`,
  `MFCreateAttributes`, `MFCreateMemoryBuffer`, `MFCreateSample`, all
  registered CLSIDs (return E_NOTIMPL or basic stubs). The CLSID table is
  defined in the same file — no external `registration.c` needed (we
  generate all guest CLSIDs in-process).
- `core/api/mfreadwrite.cpp`: stub `mfreadwrite.dll`. Exports:
  `MFCreateSourceReaderFromURL`, `MFCreateSourceReaderFromByteStream`,
  `MFCreateSinkWriterFromURL`.
- `core/api/mfmediaengine.cpp`: stub `mfmediaengine.dll`. Exports:
  `MFCreateMediaEngineClassFactory`, `MFCreateAudioRendererActivate`.
- `core/api/mf.cpp`: stub `mf.dll` (session/topology loader).

**Phase B (real implementation, 2-4 weeks):**
- Wire `IMFMediaEngineClassFactory` into the API registry so `CoCreateInstance`
  returns a real factory. The factory creates an `IMFMediaEngineEx` that
  stores its source URL, attributes, and extension.
- Implement `IMFMediaEngineExtension` (`SetSourceOnMediaEngine`,
  `BeginGetSample`, etc.) — this is the path AVPro uses.
- Implement the IMFTransform/MFT pipeline: source → decoder → color converter
  → sink. Use GStreamer via `gst_parse_launch` for the actual decode.
- Sample queue: 5–16 samples, configurable via attribute, exactly as
  upstream patch `fe6a2883`.

**Phase C (texture integration, 1-2 weeks):**
- Add `ID3D11Texture2D` allocation for the output. ThetaZ doesn't have D3D11
  yet — for VR use the X11+OpenGL path: allocate an OpenGL texture of the
  same dimensions, blit the NV12/BGRA frame into it with `glTexSubImage2D`.
- Wire `IMFMediaEngineExtension::BeginGetSample` callback: when the texture
  has a new frame, post a `WM_APP` message to a guest window so the app's
  WndProc can repaint (or call the guest callback directly via the existing
  `call_wnd_proc` path).

**Phase D (Office 365 readiness, separate workstream):**
- Office 365 Click-to-Run (OfficeC2RClient.exe, OfficeClickToRun.exe,
  WWAHost.exe) is PE64 and currently fails in ThetaZ with massive unresolved
  imports — see ThetaZ AGENTS.md §7 "INVESTIGATED — Prefix-app smoke tests".
- Required DLL coverage:
  - `kernelbase.dll`: ~700 functions
  - `ntdll.dll`: ~250 functions
  - `msvcp140.dll`, `vcruntime140.dll`, `vcruntime140_1.dll`: 100+ each
  - `crypt32.dll`, `wintrust.dll`, `bcrypt.dll`: 100+ (code-signing)
  - `wininet.dll`, `winhttp.dll`, `webio.dll`: 200+ (HTTP/networking)
  - `webservices.dll`: 50+ (SOAP/WCF for Office activation)
  - `dwmapi.dll`, `uxtheme.dll`: 50+ (visual styles)
  - `combase.dll`, `coml2.dll`, `propsys.dll`: 100+ (COM/WinRT)
  - `dxgi.dll`, `d3d11.dll`, `d2d1.dll`, `dwrite.dll`: 1000+ (D2D rendering)
  - `windows.storage.dll`, `windows.applicationmodel.dll`: WinRT activation
- This is a 6-12 month effort. Office 365 should not be considered a
  near-term goal.

### 11.6 The GStreamer integration question

ThetaZ is C++17 on Linux. Direct libgstreamer-1.0 linking via pkg-config is
the path. The Steam Runtime container (used by Proton) provides
libgstreamer-1.0 + plugins; the host system needs the same packages
(`gst-libav`, `gst-plugins-good`, `gst-plugins-bad`, `gst-plugins-base`,
`gst-plugins-ugly`).

For non-Steam-runtime distributions (like Arch Linux native), add to
`CMakeLists.txt`:

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED IMPORTED_TARGET gstreamer-1.0)
pkg_check_modules(GSTAPP REQUIRED IMPORTED_TARGET gstreamer-app-1.0)
target_link_libraries(ThetaZLib PkgConfig::GST PkgConfig::GSTAPP)
```

### 11.7 Files to create when implementation begins

```
core/api/mfplat.h            # MFT_REGISTRATION_INFO, IMFAttributes, etc.
core/api/mfplat.cpp          # ~800 lines: MFStartup, attributes, work queues
core/api/mfreadwrite.h
core/api/mfreadwrite.cpp     # ~600 lines: SourceReader/SinkWriter
core/api/mfmediaengine.h
core/api/mfmediaengine.cpp   # ~1500 lines: IMFMediaEngineEx + ClassFactory + Extension
core/api/mf.h
core/api/mf.cpp              # ~600 lines: IMFMediaSession, topology loader
core/media/gstreamer_bridge.h
core/media/gstreamer_bridge.cpp  # ~400 lines: pipeline construction helpers
core/media/nv12_fix.h
core/media/nv12_fix.cpp      # ~100 lines: row-alignment copy
docs/MEDIA_FOUNDATION.md     # public API reference
tests/mf_basic_test.c        # verify attribute store + sample allocation
tests/mfmediaengine_test.c   # verify factory + extension registration
```

Estimated total: ~5000 lines of new code. Build target: 0 errors, 0
warnings. Functional target: play a 30-second 1080p H264 MP4 in a guest
process without crashing, frame-accurate within ±100ms.
DO NOT ASSUME THE LICENSE IS MIT AS THIS CAUSES DISCREPANCIES IN TIV COMMITS
