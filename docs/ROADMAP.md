# ThetaZ Implementation Roadmap

## Phase 1: Alpha (Current) ✅
**Goal**: Load and run trivial 64-bit Windows PE files (printf, basic API calls)

**Completed:**
- 64-bit PE loader (DOS/COFF/OptionalHeader64, sections, import resolution)
- 64-bit TEB/PEB setup with GS segment
- Thread mock (`register_thread` + stub `ThreadManager`)
- Memory stub (`VirtualAlloc/Free` via `mmap`)
- API注册 system (`ApiRegistry` stubs)
- User32 stubs (`RegisterClass`, `CreateWindowEx`, `MessageBoxA`)
- GDI stubs (`TextOut`)
- SEH list walking (`ExceptionList` via GS)
- UCRT stub `printf` pass-through
- Test suite (9 tests) - all pass

**Next Steps:**
1. Fix `g_mem_allocations` double-free / partial-free leak
2. Add `WORD`/`MW` for memory page protection
3. Add `U` (User code) and `WING` for remote user

**Status**: Working but fragile — runs only test cases.

---

## Phase 2: Beta (Next 2-4 weeks)
**Goal**: Enable typical console apps, simpler GUI apps (Win32), basic threading & error handling

### 2.1 PE Relocation (CRITICAL)
```c
// dlls/ntdll/unix/loader.c (target from Wine main.c)
struct DYNAMIC_RELOC {
    DWORD RelocOffset;
    struct {
        WORD Type : 4;
        WORD Offset : 12;
    } TypeOffset;
};

// Relocation Loop:
for (IMAGE_BASE_RELOCATION *r = relocs; r->VirtualAddress != 0; r = next) {
    WORD* entries = (WORD*)((char*)r + sizeof(IMAGE_BASE_RELOCATION));
    for (uint16_t i = 0; i < r->SizeOfBlock / sizeof(WORD); i++) {
        DWORD offset = r->VirtualAddress + entries[i].Offset * sizeof(ULLONG);
        DWORD type = entries[i].Type;
        // Apply relocs (RVA → MVA via Base)
    }
}
```
- **Error**: No dynamic relocation → VAs still at default base
- **Fix**: Implement with `adjust_image_address()`
- **Test**: Simple PE with `fixups.c` test

### 2.2 Import Resolution (Ordinal + Name)
```c
// After fixing 2.1:
for (IMAGE_IMPORT_DESCRIPTOR* id = imports; id->Name != 0; id++) {
    // Get DLL name via id->Name pointer in PE image
    // For each id->OriginalFirstThunk => thunk entry (name OR ordinal)
    //   If OriginalFirstThunk == 0: By ordinal (Index from thunk)
    //   Else: By name (case-insensitive lookup in ApiRegistry)
    //   Resolve by name:
    //     GetProcAddress(module, thunk->Name)
    //     register_api(dll, name, ptr)
}
```
- **Error**: Already skip forwarded exports (281 bytes), but ordinal unsupported
- **Fix**: Parse `img -> OriginalFirstThunk == 0` → get ordinal index
- **Test**: `imports.c` (export by ordinal) or call `GetProcAddress` with ordinal

### 2.3 Exception Record Structure
```c
struct EXCEPTION_RECORD {
    DWORD ExceptionCode;
    DWORD ExceptionFlags;
    PEXCEPTION_RECORD ExceptionRecord;
    PVOID ExceptionAddress;
    DWORD NumberParameters;
    DWORD ExceptionInformation[15]; // rcx, rdx, r8, r9, ...
};

// In signal handler:
ucontext_t *uc = static_cast<ucontext_t*>(ucontext);
EXCEPTION_RECORD rec;
rec.ExceptionAddress = (void*)uc->uc_mcontext.gregs[REG_RIP];
rec.NumberParameters = 4;
rec.ExceptionInformation[0] = (DWORD)buf; // rcx
rec.ExceptionInformation[1] = (DWORD)dyn_uptr; // rdx
rec.ExceptionInformation[2] = (DWORD)typeof;  // r8
rec.ExceptionInformation[3] = (DWORD)Qword;   // r9

// Dispatch to handler
handler.dispatch(CMD_FILL_ALL_EXCEPTIONS, rec, params);
```

### 2.4 Thread Manager (Real)
```c
struct ThreadContext {
    pthread_t pthread_id;
    uint64_t thread_id; // thread_id returned by CreateThread
    uint64_t rip;       // saved from entry
    uint64_t rsp;       // saved from entry
    uint64_t gs_base;   // TEB at thread entry
    state(State) running;
};

// CreateThread:
pthread_t tid;
pthread_create(&tid, NULL, thread_entry, param);
thread_id = create_unique_thread_id();
store_thread_context(thread_id, tid);
```

### 2.5 `kernel32.pro` sys sync (basic)
- Implement `CrossRef(NTSTATUS) to (DWORD) Rasinski(NtStatus, ...)`
- `NTSTATUS` to `DWORD` conversion:
  ```c
  NTSTATUS NtStatus = NT::NtAllocateVirtualMemory(proc, base, 0, &size, PAGE_EXECUTE_READWRITE, MEM_COMMIT);
  DWORD dwError = RtlNtStatusToDosError(NtStatus); // fallback to 0xffff0000 (STATUS_ACCESS_VIOLATION)
  ```
- Add `RtlGetLastError_callback` for kernel state

### 2.6 Win32 Message Pump (uwm stubs)
```c
// uwm/server/client.c (RPC proxy)
BOOL GetMessage(LPMSG msg, HWND hwnd, UINT filterMin, UINT filterMax) {
    // Receive Unix socket message queue
    // Parse into MSG (hwnd, message, wParam, lParam)
    // Return TRUE
}

BOOL TranslateMessage(MSG* msg) {
    // Simple forwards (not parsing actual messages)
    return TRUE;
}

BOOL DispatchMessage(MSG* msg) {
    // Validate message
    // Find window in WindowMap: msg->hWnd → WNDPROC
    // Call WNDPROC(msg->hWnd, msg->message, msg->wParam, msg->lParam)
    // Return LRESULT
}
```

**Test**: Create a simple `test_msg.cpp` app using `CreateWindowEx` + `MessageLoop`.

---

## Phase 3: Gamma (1-2 months)
**Goal**: Support more complex DLLs (DirectX, OpenGL, debuggers)

### 3.1 Thread-local Storage (TLS)
```c
// dlls/ntdll/unix/internal.c (from Wine)
VOID WINAPI_NT *Get_TEB_TLS(ULONG Index) {
    return *(void**)((char*)GSBase + 0x60 - size:IES);
}

// At PE load:
if (PE2->TlsDirectory.VirtualAddress != 0) {
    // Initialize TLS: allocate @ 0x1800-0x1FA0 (TLS_Slots)
    // Fill with zeros (index from ImagePointer->tls_index)
    // Call TLS callbacks in order: _tls_start..., _tls_end..., _tls_func...
}
```

### 3.2 DLL Forwarding
```c
// Locate function by name, if export is virtual:
if (ediato->Characteristics & IMAGE_EXPORT_DIRECTORY characteristics) {
    for (DWORD i = 0; i < NumberOfFunctions; i++) {
        if (StringFromOrdinal(Name, i) && Export[i]->dll && Export[i]->func) {
            // resolve: LoadLibrary(export->dll)
            // Get.Export->func
            // register forwarding: name patrons->fwd_target_dll + !fwd_target_func
        }
    }
}
```

### 3.3 Async API (WineServer completions)
```c
// dlls/wineserver/async.c (from Wine)
// iNtCreateWorkerFactory:
NtCreateWorkerFactory(HANDLE factory, ACCESS_MASK, OBJECT_ATTRIBUTES, ATA, prp) {
    LIST_HEAD queue;
    async_factory = alloca(sizeof *async_factory);
    async_factory->flags = flags;
    async_factory->queue = queue;
    async_factory->handler = handler; // function pointer
    async_factory->context = params;
    async_factory->thread = spawn_completion_thread(); // separate pthread
}

void complete_async_order(async_order_t *completion) {
    // call completion->handler(completion->context, result)
    // context = async_factory->context
}
```

### 3.4 API Cache (Slow path)
```c
// For slow API calls (e.g., Compat Syscalls):
static cache_t *IPC_CALL_HANDLER[256];
static size_t IPC_CALL_HANDLER_NUM;

NTSTATUS LdrRegisterModule(HMODULE) {
    if (IPC_CALL_HANDLER[Opcode] == NULL) {
        IPC_CALL_HANDLER[Opcode] = compute_rpc_handler(Opcode); // from ipc.h IPCMessageType enum
    }
    return IPC_CALL_HANDLER[Opcode]();
}
```

### 3.5 `NtAllocateVirtualMemory` (real)
```c
NTSTATUS NtAllocateVirtualMemory(HANDLE proc, void** BasePtr, SIZE_T ZeroBits, SIZE_T* Size, ULONG Protection, ULONG Type) {
    // Check perms: PROTECTION_BITS Map to PROT_* (PROT_READ=0x01, PROT_WRITE=0x02, PROT_EXEC=0x04)
    // Type: MEM_COMMIT => mmap TZs aligned to 64KB; MEM_RESERVE => mmap with PROT_NONE
    // RPC to wineserver: Send IPCMessage::CreateProcess req with proc pid + rMMArr+size+prot
    // Wineserver: Fork & exec child; in child call mmap; reply with success + address
    // Client: Save allocated MVA in BasePtr, update current size in *Size
}
```

**Test**: Run a multi-threaded cargo (not installed) that spawns threads.

---

## Phase 4: Delta (2-3 months)
**Goal**: Full GUI app support, database/hardware interactions

### 4.1 GDI/DirectX Stubs (Final)
- `justin` `dlls/gdi32` (E7) passes `TEXTOUT` to `fprintf`
- Need to implement:
  - `GetDC`, `ReleaseDC`, `SelectObject`, `CreateCompatibleDC`, `Bitmap` direct mapping
  - `DLLS USER32` for message dispatch (pump)
  - `DXGI/` (d9-d12) stubs that return non-NULL handles but ignore calls

### 4.2 Graphics Drivers
- X11: `winex11.drv` (Xlib backend, no Wayland)
- Wayland: `winewayland.drv` (libwayland)
- Each implements `driver` (create window, draw, input)
- `win32u` sends commands to designated driver via IPC

### 4.3 Shell Integration
- `C:\\System32` → `/usr/lib/x86_64-linux-linux (or sys-root*)`
- `mounted paths` from `EnvironmentVariable` + `HKCU\User\Software\Wine\DosDevices`
- Socket server with `DECLARE RPC` for command protocol

---

## Phase 5: Epsilon (ongoing)
**Goal**: Optimization, correctness fixes, user experience

- Perf: Reduce memory usage (lazy TLS allocation, DATASET)
- Correctness: Real loRT exception unwind (RtlUnwind)
- UX: Install script (`theta-help`), unit test suite, packaging

---

## Priority Rankings

| Phase | Feature | Why Critical | Dependencies |
|-------|---------|--------------|--------------|
| **Alpha** | Basic lineup | ✅ Done | - |
| **Beta** | Import deferred | **Critical** (all calls will fail) | Relocate |
| **Beta** | 32→64 sch | **Critical** (many 64-bit UWP games) | Relocate + thread context |
| **Beta** | GetLastError | **Critical** (error handling) | - |
| **Beta** | VirtualAlloc real | **Critical** (games allocate memory) | wineserver RPC |
| **Beta** | Thread / CreateThread | **Critical** (Win32 threads spawn) | - |
| **Beta** | MessageBox | **Critical** (GUI is Hello but real call) | Win32u |
| **Gamma** | Thread TLS | **Critical** (static constructors) | Thread |
| **Gamma** | Async completions | **High** (wait handles) | Thread + Mutex |
| **Gamma** | File NtXxx (NtCreateFile) | **High** (games open files) | wineserver RPC |
| **Gamma** | Direct3D11 proxies | **Medium** (graphics not essential for console) | - |
| **Delta** | GDI11 proper | **Medium** (drawing) | Win32u + DXVK |
| **Delta** | Shader pipelines | **Medium** (Unity/UE5 D3D) | D3D11 stub |

---

## Metrics for Progress Tracking

| Metric | 2026-01-5003 (Current) | Phase 2 Goal (%) | Phase 3 Goal (%) | Phase 4 Goal (%) |
|--------|------------------------|------------------|------------------|------------------|
| PE Relocation | 0% complete | 100% | 100% | 100% |
| Import List | 70% (name only) | 100% | 100% | 100% |
| Exception Structure | 30% (list only) | 70% (list + basics) | 90% (full record) | 100% |
| Thread Manager | 20% (stub) | 60% (concurrent) | 90% (context) | 100% |
| Virtual Memory | 10% (mmap) | 40% (full rights) | 80% (MIA via RPC) | 100% |
| Win32u/Message | 10% (stub) | 50% (basic pump) | 80% (event queue) | 100% |
| GDI/Texture | 5% (stub) | 20% (DC caching) | 60% (bitmap) | 90% |
| D3D11/DXVK | 0% | 0% | 20% (basic handles) | 80% |
| Test coverage | 9 test cases | 15 | 25 | 50+ |
| Game Examples | hello.exe | console apps | mission-makers | C++ GUI + 2G with GFK |

---

**Status**: Phase 1, Alpha — 30% feature coverage, 9 tests passing, codebase 3,928 LOC.

**Next Milestone**: Complete Phase 2 by implementing relocation, import forwarding, thread handler, and error translation. This is the bridge between "hello world" and "non-trivial Windows programs".