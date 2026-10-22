// ThetaZ — Copyright (c) 2026 ThetaZ Project. MIT License.

#pragma once
#include <cstdint>
#include <cstddef>

namespace ThetaZ {

// x64 Windows TEB (Thread Environment Block) layout offsets
namespace teb_offsets {
constexpr size_t ExceptionList = 0x000;          // NT_TIB
constexpr size_t StackBase = 0x008;
constexpr size_t StackLimit = 0x010;
constexpr size_t SubSystemTib = 0x018;
constexpr size_t FiberData = 0x020;
constexpr size_t ArbitraryUserPointer = 0x028;
constexpr size_t Self = 0x030;
constexpr size_t EnvironmentPointer = 0x038;
constexpr size_t ClientId_UniqueProcess = 0x040;
constexpr size_t ClientId_UniqueThread = 0x048;
constexpr size_t ThreadLocalStoragePointer = 0x058;
constexpr size_t ProcessEnvironmentBlock = 0x060;
constexpr size_t LastErrorValue = 0x068;
constexpr size_t DeallocationStack = 0x1780;
constexpr size_t TlsSlots = 0x1800;              // 64 TLS slots
constexpr size_t TlsExpansionSlots = 0x17A0;     // (x64: 0x17A0)
constexpr size_t TEB_SIZE = 0x2000;
}

// x64 Windows PEB (Process Environment Block) layout offsets
namespace peb_offsets {
constexpr size_t ImageBaseAddress = 0x010;
constexpr size_t Ldr = 0x018;
constexpr size_t ProcessParameters = 0x020;
constexpr size_t ProcessHeap = 0x030;
constexpr size_t SessionId = 0x058;
constexpr size_t PEB_SIZE = 0x800;
}

// x64 Windows RTL_USER_PROCESS_PARAMETERS layout offsets
namespace params_offsets {
constexpr size_t ImagePathName = 0x060;          // UNICODE_STRING (16 bytes)
constexpr size_t CommandLine = 0x070;            // UNICODE_STRING (16 bytes)
constexpr size_t PARAMS_SIZE = 0x1000;
}

// Set up the Windows TEB + PEB for the current (main) thread and point GS at it.
// Returns the TEB address.
uint64_t setup_windows_teb(uint64_t image_base, const wchar_t* command_line);

// Set up a TEB for a secondary thread, sharing the process PEB. Returns the TEB
// address (does NOT switch GS; the new thread does that itself).
uint64_t setup_thread_teb(uint64_t peb, uint64_t tid,
                          uint64_t stack_base, uint64_t stack_limit);

// The PEB/TEB created by setup_windows_teb (0 if not yet created).
uint64_t get_main_peb();
uint64_t get_main_teb();

// Switch the current thread's GS base to the given TEB (or 0 to clear).
bool set_gs_base(uint64_t address);

void add_module_to_ldr(uint64_t base, const char* name);

} // namespace ThetaZ
