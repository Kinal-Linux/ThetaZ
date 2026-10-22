// ThetaZ — 32-bit API call dispatch (thunk region + stdcall marshalling)
// Copyright (c) 2026 ThetaZ Project. MIT License.

#pragma once
#include <cstdint>
#include <cstddef>

namespace ThetaZ {

struct X86CpuState;

// One resolved import used by 32-bit guests. `fn` is the host (ms_abi) function;
// `argc` is the number of 32-bit stack args; `stdcall` selects callee-pops.
struct ApiThunk {
    const char* dll;
    const char* name;
    void* fn;
    int argc;
    bool stdcall;
    bool known; // true if argc came from the authoritative signature table
};

// Reserve the low stub region (below 4GB) and load the signature table.
void init_api_dispatch();

// Allocate a guest-visible thunk marker for an imported function and return its
// guest address (used as the IAT slot value for 32-bit images).
uint32_t alloc_api_thunk(void* fn, int argc, bool stdcall,
                         const char* dll, const char* name);

// Look up a thunk by guest address (0 if not a thunk).
const ApiThunk* api_thunk_at(uint32_t guest_addr);

// True if the address lies in the reserved thunk region.
bool is_api_thunk_addr(uint32_t guest_addr);

// Signature table lookup: "dll!name" -> arg count + stdcall convention.
// Returns false if the function is unknown to the table.
bool api_signature_for(const char* dll, const char* name, int* argc, bool* is_stdcall);

// Interpreted call to a thunk. Ret addr is where execution continues. Handles
// stdcall stack cleanup and return-value placement (eax / edx:eax).
uint32_t x86_dispatch_api(X86CpuState& s, uint32_t thunk_addr, uint32_t ret_addr);

// True while a 32-bit interpreted API call is executing. The shared API layer
// uses this to return guest-friendly values (thunks instead of host pointers).
bool x86_dispatch_active();

// Allocate a thunk for a runtime-resolved function (GetProcAddress result).
uint32_t x86_alloc_proc_thunk(void* fn, const char* name);

// Allocate a 64-bit guest import shim: a trampoline that converts the MS x64
// calling convention (args in RCX/RDX/R8/R9) to the host SysV ABI
// (RDI/RSI/RDX/RCX) and jumps to `fn`. Stack args (5+) are moved from their
// ms_abi offsets ([rsp+0x28+8k]) down to the SysV offsets ([rsp+8+8k]); the
// shift count comes from the signature table when `dll`/`name` are given.
// Returns the host address to store in a PE32+ IAT slot (0 on failure).
uint64_t x86_alloc64_shim(void* fn, const char* dll = nullptr, const char* name = nullptr);

// Call a guest function with explicit EBP override for SEH frame setup.
uint32_t x86_call_guest_args_ebp(uint32_t fn, uint32_t* args, int nargs, uint32_t ebp);

} // namespace ThetaZ
