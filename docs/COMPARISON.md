# Technical Comparison Table: ThetaZ vs Wine vs ProtonGE

| Category | Wine (Base) | ProtonGE (Add-on) | ThetaZ | Gap % |
|----------|-------------|-------------------|--------|-------|
| **Build System** | autoconf + Makefile templates | Python entry + CMake glue | CMake only | ✅ (no PE/ELF split) |
| **Architecture** | C legacy (#ifdef everywhere) | Same + simpler config | C++17 + RAII + STL | ✅ (modern) |
| **32-bit Support** | Full (i386/x86_64) | Proton x86/64 + FEX ARM64 | ❌ (64-bit only) | -100% |
| **PE Loader** | Fully functional (relocations, imports, TLS) | Same | Works (but relocs skipped) | 50% |
| **Import Resolution** | By name + ordinal + forwarded | Same | By name only | 70% |
| **Relocation** | DYNAMIC_RELOC + BaseReloc | Same | ❌ (skipped) | 0% |
| **NT SySCALL Layer** | NtXxx implemented (RPC to wineserver) | Same | ❌ (inline mmap) | 0% |
| **Virtual Memory API** | NtAllocateVirtualMemory (kind of NtCreateFileMapping) | Same | Stub `mmap` | 10% |
| **Threads** | CreateThread (2-threads + async completions) | Same | Stub `register_thread` | 20% |
| **Sync** | Mutex/Semaphore/Event (wait + async) | Same | Basic stubs | 50% |
| **Exception Handling** | EXCEPTION_RECORD parsing + unwind | Same | Walk list only (no exception details) | 40% |
| **SEH** | Full unwind + nested exceptions | Same | Walk list + debug prints | 50% |
| **Win32u (GDI/Message)** | win32u.dll + winex11.drv + winewayland.drv | Same | Stub `user32` only | 10% |
| **GDI** | Full rendering (bitmap, DC caching) | Same | Stub fprintf | 5% |
| **D3D11/D3D12** | wined3d + dxvk/vkd3d-proton | Bundled DLLs | ❌ | 0% |
| **DXVK** | Direct3D9 → Vulkan | Bundled `dxvk.dll` | ❌ | 0% |
| **nvidia-d3d** | `d8vk` + `dxvk-nvapi` | Bundled | ❌ | 0% |
| **GDI+** | Full imaging + PDF | Same + SxS | ❌ | 0% |
| **Shell** | winelib win32u + views | Same + full | ❌ | 0% |
| **fonts** | FreeType fully linked | Same + VFS | ❌ | 0% |
| **User Profile** | `wineboot -u` | Same + Steam integration | ❌ | 0% |
| **Registry** | Registry files (config from Steam) | Same | ❌ | 0% |
| **Disk Redirect** | `C:\` → /home/user/.wine/c | Same | ❌ | 0% |
| **Input** | X11/Wayland drivers | Same | ❌ | 0% |
| **Sound** | CoreAudio/PulseAudio | Same | ❌ | 0% |
| **Network** | winsock2/api-ms-win-crt-network | Same | ❌ | 0% |
| **Games Tested** | Hundreds of AAA games | Hundreds (Proton) | hello.exe + 9 test cases | 0.01% |
| **Code LOC** | ~60M (Wine) | +10M (GE patches) | ~3.9K | 99.9% |
| **License** | LGPL 2.1 | Same (LGPL) | MIT | — |
| **Maintainers** | Alexandre Julliard + others | Valve + GloriousEggroll | Monouser (current) | — |
| **API Coverage** | ~80% | ~85% | ~30% | 60% |
| **Test Count** | Tens of thousands (internal + winehq) | Hundreds (Proton QE) | 9 | 97.9% |

## Column Legend
- ✅ = Implemented/Complete
- ❌ = Not Implemented/Incomplete
- Gap % = Estimated completion % of Wine implementation (higher is better)

## Significance
- **Architecture**: ThetaZ has the cleaner design (single build, RAII) but the implementation is **much simpler than Wine** — about 10-15x less code.
- **Test Coverage**: The 9 tests cover basic PE loading + calls but miss advanced features like:
  - 32→64 relocations
  - DLL forwarding
  - Thread creation + context switching
  - Asynchronous completions (events, semaphores)
  - Exception structures beyond the basic walk
- **Comparative Position**: ThetaZ is at **Phase 1 (Alpha)**: can load and run trivial C hello-world programs. It is not at **Phase 2 (Beta)**: capable of running anything non-trivial.

## Impact Assessment
If a real Windows game (e.g. a query动员/Unity/UE5 title) runs under current ThetaZ:
1. **Import resolution** will fail if exports are forwarded (e.g., `kernel32.dll!GetAdaptersInfo` → `iphlpapi.dll`)
2. **32位 to 64位 conversion** won't work for even 64-bit UWP games
3. **TLS callbacks** won't run → static constructors won't execute
4. **Window management** will fail → return NULL handle
5. **Direct3D calls** → crash
6. **Thread creation** → only stub returns handle, no context
7. **Exception handling** → no stack unwinding, no cleanup

**Conclusion**: For real applications, ThetaZ is effectively **incomplete**. The audit recommends the roadmap in `AUDIT.md` for progression from Alpha to Beta.