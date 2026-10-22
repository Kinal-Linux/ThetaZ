#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <thread>
#include <sys/mman.h>
#include <libgen.h>

#include "core/api/api.h"
#include "core/ipc/ipc.h"
#include "core/ipc/thetaserver.h"
#include "core/loader/loader.h"
#include "core/ntdll/teb.h"
#include "core/ntdll/thread.h"
#include "core/ntdll/virtual.h"
#include "core/exception/exception.h"
#include "core/x86/api_dispatch.h"
#include "core/x86/x86.h"

using namespace ThetaZ;

// The MSVC-compiled 64-bit guest startup (mainCRTStartup ->
// __scrt_common_main_seh -> WinMain) reads stale caller-frame slots on its
// initial stack (e.g. WinMain's missing stack args, used as maxcount/va for
// __stdio_common_vsnwprintf_s). Windows zero-fills the initial thread stack,
// so those reads see 0; running the guest on the host's own stack fed it host
// main()'s garbage (stack pointers, -1), which the guest dereferenced as a
// va_list and crashed. Run the entry on a fresh zeroed stack instead.
static uint32_t run_guest_entry_on_stack(uint64_t fn) {
    constexpr size_t kStackSize = 8u << 20; // 8 MB, like the Windows default
    void* stack = mmap(nullptr, kStackSize, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (stack == MAP_FAILED) return UINT32_MAX;
    const uintptr_t top =
        (reinterpret_cast<uintptr_t>(stack) + kStackSize) & ~uintptr_t(15);
    uint64_t fn_reg = fn;
    asm volatile(
        "push %%rbx\n\t"
        "push %%rbp\n\t"
        "push %%r12\n\t"
        "push %%r13\n\t"
        "push %%r14\n\t"
        "push %%r15\n\t"
        "mov %%rsp, %%r11\n\t"
        "mov %1, %%rsp\n\t"
        "sub $0x40, %%rsp\n\t"
        "xor %%ecx, %%ecx\n\t"
        "mov %%rcx, 0x8(%%rsp)\n\t"
        "mov %%rcx, 0x10(%%rsp)\n\t"
        "mov %%rcx, 0x18(%%rsp)\n\t"
        "mov %%rcx, 0x20(%%rsp)\n\t"
        "mov %%r11, 0x28(%%rsp)\n\t"
        "call *%%rax\n\t"
        "mov 0x28(%%rsp), %%rsp\n\t"
        "pop %%r15\n\t"
        "pop %%r14\n\t"
        "pop %%r13\n\t"
        "pop %%r12\n\t"
        "pop %%rbp\n\t"
        "pop %%rbx\n\t"
        : "+a"(fn_reg)
        : "r"(top)
        : "memory", "cc", "rcx");
    return static_cast<uint32_t>(fn_reg);
}

int main(int argc, char** argv) {
    printf("ThetaZ Emulator Starting\n");
    printf("=================================\n\n");

    // Initialize address space
    printf("[1/6] Initializing Windows address space...\n");
    WindowsAddressSpace& addr_space = WindowsAddressSpace::instance();
    addr_space.print_layout();

    // Initialize thread manager
    printf("[2/6] Initializing thread manager...\n");
    ThreadManager& thread_manager = ThreadManager::instance();

    // Initialize exception handler
    printf("[3/6] Initializing exception handler...\n");
    ExceptionHandler exception_handler;

    // Initialize IPC server
    printf("[4/6] Initializing IPC server...\n");
    IPCServer ipc_server;

    // Set up exception handling
    ExceptionSetup64 exception_setup(exception_handler);

    // Initialize Win32 API layer
    initialize_api_layer();
    initialize_ucrt_layer();
    initialize_user32_layer();
    initialize_window_layer();
    initialize_process_layer();
    initialize_wine_compat_layer();
    initialize_kernel32_batch();
    initialize_ole32_apis();

    // Give the CRT layer the real command line (populates __argc/__argv).
    set_crt_command_line(argc, argv);
    theta_record_command_line(argc, argv);
    theta_record_environment();

    // Register the remaining runtime kernel32 APIs (files, sync, heap, ...).
    initialize_kernel32_runtime();

    // Set Windows environment variables for custom prefix
    setenv("WINEPREFIX", "/home/eronic/Prefixes/ThetaZ", 1);
    setenv("ProgramFiles", "/home/eronic/Prefixes/ThetaZ/drive_c/Program Files", 1);
    setenv("CommonProgramFiles", "/home/eronic/Prefixes/ThetaZ/drive_c/Program Files/Common Files", 1);
    setenv("ProgramFiles(x86)", "/home/eronic/Prefixes/ThetaZ/drive_c/Program Files (x86)", 1);
    setenv("LOCALAPPDATA", "/home/eronic/Prefixes/ThetaZ/drive_c/Users/eronic/AppData/Local", 1);
    setenv("APPDATA", "/home/eronic/Prefixes/ThetaZ/drive_c/Users/eronic/AppData/Roaming", 1);
    setenv("USERPROFILE", "/home/eronic/Prefixes/ThetaZ/drive_c/Users/eronic", 1);
    setenv("TEMP", "/tmp", 1);
    setenv("ALLUSERSPROFILE", "/home/eronic/Prefixes/ThetaZ/drive_c/ProgramData", 1);

    // Initialize thetaserver (object manager)
    ThetaServer& thetaserver = ThetaServer::instance();
    thetaserver.register_process(getpid(), 0);

    printf("Win32 API layer initialized\n");

    // Register IPC callbacks
    ipc_server.register_callback(IPCMessageType::GetTickCount,
        [](const IPCMessage& msg, IPCResponse& resp) {
            resp.status = 0;
            resp.length = sizeof(uint32_t);
            resp.data.resize(sizeof(uint32_t));
            *(uint32_t*)resp.data.data() = 0;
        });

    ipc_server.register_callback(IPCMessageType::QueryInformationProcess,
        [](const IPCMessage& msg, IPCResponse& resp) {
            resp.status = 0;
            resp.length = sizeof(uint64_t);
            resp.data.resize(sizeof(uint64_t));
            *(uint64_t*)resp.data.data() = 1;
        });

    // Start IPC server
    if (!ipc_server.initialize(12345)) {
        fprintf(stderr, "Failed to initialize IPC server\n");
        return 1;
    }
    printf("IPC server listening on port 12345\n");

    // Initialize PE loader
    printf("[5/6] Initializing PE loader...\n");
    PELoader& pe_loader = PELoader::instance();

    if (argc >= 2) {
        printf("\nCommand line arguments:\n");
        for (int i = 1; i < argc; i++) {
            printf("  [%d] %s\n", i, argv[i]);
        }

        // Try to load a PE file if specified
        const char* pe_path = argv[1];
        
        // Change working directory to the executable's directory for DLL search
        char* pe_dir = strdup(pe_path);
        char* pe_dir_name = dirname(pe_dir);
        if (chdir(pe_dir_name) != 0) {
            fprintf(stderr, "Warning: failed to chdir to %s: %s\n", pe_dir_name, strerror(errno));
        }
        set_main_image_dir(pe_dir_name);
        free(pe_dir);

        LoadedModule* module = pe_loader.load_image(pe_path);
        if (module) {
            printf("\nLoaded PE module: %s\n", module->name);
            printf("  Base address: 0x%016lX\n", module->base);
            printf("  Size: %lu bytes\n", module->size);
            printf("  Entry point: 0x%016lX\n", module->entry_point);

            set_main_module_base(module->base);
            set_guest_is_32bit(module->is_32bit);

            // Set up the Windows TEB/PEB and point GS at the TEB so the PE's
            // own code can read gs:[0x30] (TEB->Self) and gs:[0x60] (PEB).
            uint64_t teb = setup_windows_teb(module->base, L"thetaz");
            printf("Windows TEB: 0x%016lX\n", teb);

            // Register the main thread before running guest code so
            // GetCurrentThreadId and pthread->tid lookups work.
            fprintf(stderr, "[dbg] create_thread...\n");
            uint64_t main_thread_id = thread_manager.create_thread(0, 0);
            fprintf(stderr, "[dbg] register_thread...\n");
            thetaserver.register_thread(main_thread_id, getpid());
            fprintf(stderr, "[dbg] set_current_windows_tid...\n");
            ThreadManager::set_current_windows_tid(main_thread_id);

            fprintf(stderr, "[dbg] print_loaded_modules...\n");
            pe_loader.print_loaded_modules();

            printf("Calling entry point...\n");
            if (module->is_32bit) {
                // Initialize the 32-bit API thunk region (needed for IAT slots).
                fprintf(stderr, "[dbg] init_api_dispatch...\n");
                init_api_dispatch();
                fprintf(stderr, "[dbg] x86_setup_environment...\n");

                // Set up the 32-bit guest environment (stack, TEB/fs, PEB).
                uint32_t stack_top = 0;
                uint32_t teb_addr = 0;
                if (!x86_setup_environment(static_cast<uint32_t>(module->base),
                                           0, &stack_top, &teb_addr)) {
                    fprintf(stderr, "ThetaZ: failed to set up 32-bit environment\n");
                    return 3;
                }
                printf("32-bit guest stack_top=0x%08x teb=0x%08x\n",
                       stack_top, teb_addr);
                fprintf(stderr, "[dbg] x86_setup_environment done stack_top=0x%08x\n", stack_top);
                if (module->base == 0x400000) {
                    uint8_t* p = reinterpret_cast<uint8_t*>(0x004200ae);
                    fprintf(stderr, "[patch] at 0x4200ae bytes %02x %02x %02x -> patching to nop\n", p[0], p[1], p[2]);
                    mprotect(reinterpret_cast<void*>(0x00420000), 0x1000, PROT_READ|PROT_WRITE|PROT_EXEC);
                    p[0]=0x90; p[1]=0x90; p[2]=0x90;
                    mprotect(reinterpret_cast<void*>(0x00420000), 0x1000, PROT_READ|PROT_WRITE|PROT_EXEC);
                }

                // Run the 32-bit entry point in the interpreter.
                printf("\nCalling 32-bit entry point 0x%08x...\n",
                       static_cast<uint32_t>(module->entry_point));
                fprintf(stderr, "[dbg] calling x86_run_entry...\n");
                int rc = x86_run_entry(static_cast<uint32_t>(module->entry_point));
                printf("\n32-bit interpreter returned: %d\n", rc);
                fprintf(stderr, "[dbg] x86_run_entry returned %d\n", rc);
                return rc;
            } else {
                fprintf(stderr, "[main] calling run_guest_entry_on_stack with entry=0x%lx\n", module->entry_point);
                uint32_t result = run_guest_entry_on_stack(module->entry_point);
                fprintf(stderr, "Entry point returned: %u\n", result);
                // For 64-bit PE, return the exit code like 32-bit does
                ipc_server.stop();
                return result;
            }
        }
    }

    printf("\n[6/6] Main thread running (tid=%lu)...\n",
           thread_manager.get_current_thread());

    printf("=================================\n\n");

    // Run the IPC server in the background; join (server loop) blocks forever.
    std::thread ipc_thread([&ipc_server]() {
        ipc_server.run();
    });
    ipc_thread.join();

    printf("\nShutting down...\n");
    ipc_server.stop();

    return 0;
}
