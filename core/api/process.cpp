// ThetaZ — real CreateProcessA / process-exit semantics.
// A child process is simulated in-process: PELoader loads the target EXE at a
// conflict-free base, its entry point runs on a dedicated guest thread, and the
// child is tracked as a ThetaServer Process object (its pid doubles as the
// PROCESS_INFORMATION.hProcess handle). Child exit is routed through
// terminate_current_process so the CRT's exit() only ends the child, not the
// host emulator.
// Copyright (c) 2026 ThetaZ Project. MIT License.

#include "core/api/api.h"
#include "core/ipc/thetaserver.h"

using ThetaZ::register_api;
using ThetaZ::THETAZ_ExitProcess;
using ThetaZ::THETAZ_GetExitCodeProcess;
using ThetaZ::THETAZ_GetCurrentProcessId;
using ThetaZ::initialize_process_layer;
#include "core/loader/loader.h"
#include "core/ntdll/thread.h"
#include "core/x86/x86.h"
#include "core/api/path_resolver.h"

#include <cstdio>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <mutex>
#include <string>
#include <unistd.h>

namespace ThetaZ {

namespace {

std::mutex g_pid_mutex;
uint64_t g_next_child_pid = 0x10000;

uint64_t alloc_child_pid() {
    std::lock_guard<std::mutex> lock(g_pid_mutex);
    return g_next_child_pid++;
}

// Extracts the application path from either the application argument or the
// first quoted/unquoted token of the command line.
std::string resolve_target(const char* application, const char* commandline) {
    if (application && application[0]) return application;

    if (commandline) {
        const char* s = commandline;
        while (*s == ' ' || *s == '\t') s++;
        bool quoted = false;
        const char* start = s;
        while (*s) {
            if (*s == '"') quoted = !quoted;
            else if ((*s == ' ' || *s == '\t') && !quoted) break;
            s++;
        }
        std::string target(start, s - start);
        if (target.size() >= 2 && target.front() == '"' && target.back() == '"') {
            target = target.substr(1, target.size() - 2);
        }
        return target;
    }
    return "";
}

static BOOL WINAPI_CALL THETAZ_CreateProcessA(LPCSTR application, LPSTR commandline,
                                              void* process, void* thread,
                                              BOOL inherit, DWORD flags, void* env,
                                              LPCSTR current_dir, void* startup_info,
                                              void* process_info) {
    (void)process;
    (void)thread;
    (void)inherit;
    (void)flags;
    (void)env;
    (void)current_dir;
    (void)startup_info;
    if (!process_info) return FALSE_VALUE;

    std::string target = resolve_target(application, commandline);
    if (target.empty()) {
        printf("[kernel32] CreateProcessA: no application or command line\n");
        return FALSE_VALUE;
    }
    // Guest paths arrive as C:\... or Z:\...; map them back to host paths
    // before handing the name to the PE loader.
    {
        std::string translated = ThetaZ::get_path_resolver().translate_windows_to_linux(target);
        if (!translated.empty() && translated != target) target = translated;
    }

    PELoader& loader = PELoader::instance();

    // Self-relaunch guard: if the caller is already a child process and the
    // target matches the main module, count how many active copies exist. On
    // real Windows each child gets its own address space; in our in-process
    // emulator they share one, so unbounded copies exhaust the low zone.
    LoadedModule* main_mod = loader.find_module_by_base(get_main_module_base());
    if (main_mod && main_mod->name[0]) {
        const char* mn = main_mod->name;
        size_t mn_len = strlen(mn);
        if (target.size() <= mn_len && strncasecmp(mn + (mn_len - target.size()),
                                                    target.c_str(), target.size()) == 0) {
            uint32_t copy_count = loader.count_modules_named(mn);
            if (copy_count >= 4) {
                printf("[kernel32] CreateProcessA: %u active copies of \"%s\", skipping recursive spawn\n",
                       copy_count, target.c_str());
                return FALSE_VALUE;
            }
        }
    }

    LoadedModule* module = loader.load_image(target.c_str());
    if (!module && target.find(".exe") == std::string::npos) {
        module = loader.load_image((target + ".exe").c_str());
    }
    if (!module) {
        // Self-relaunch: the app truncated its own module path. Load a FRESH
        // copy of the main image (own .data/relocations) so the child runs as a
        // real separate process instead of re-initializing the parent's globals.
        LoadedModule* main_mod = loader.find_module_by_base(get_main_module_base());
        if (main_mod && main_mod->name && strncmp(main_mod->name, target.c_str(), target.size()) == 0) {
            module = loader.load_image(main_mod->name);
            printf("[kernel32] CreateProcessA: self-relaunch, loaded fresh copy at base=0x%lX\n",
                   module ? module->base : 0UL);
        }
    }
    // A 32-bit child must live below 4GB (identity-mapped guest pointers). If
    // the fresh copy fell back to a high host address (every low candidate
    // taken by the parent's image and the Wine DLLs), reload it at one of the
    // dedicated relaunch bases. These sit above the low Wine DLL range and
    // below the guest heap, so they stay free for re-mapped main images.
    if (module && module->base >= 0x100000000ull && target.size() < 512) {
        std::string name = module->name ? module->name : "";
        // Unload the SPECIFIC copy we just loaded (its high host address).
        // Never unload by name: parent and child copies of the same image share
        // a name, so a name-based unload would munmap the parent at 0x400000.
        uint64_t old_base = module->base;
        loader.unload_image(old_base);
        module = nullptr;
        const uint64_t relaunch_bases[] = {0x20000000, 0x30000000, 0x40000000, 0x50000000};
        for (uint64_t rb : relaunch_bases) {
            module = loader.load_image(name.c_str(), rb);
            if (module) {
                printf("[kernel32] CreateProcessA: remapped child to base=0x%lX\n", module->base);
                break;
            }
        }
    }
    if (!module) {
        printf("[kernel32] CreateProcessA: failed to load \"%s\"\n", target.c_str());
        return FALSE_VALUE;
    }
    printf("[kernel32] CreateProcessA: loaded \"%s\" base=0x%lX entry=0x%lX\n",
           module->name, module->base, module->entry_point);

    uint64_t child_pid = alloc_child_pid();
    ThetaServer& server = ThetaServer::instance();
    server.register_process(child_pid, getpid());
    set_guest_process_image_base(child_pid, module->base);
    // The child must see the command line the parent passed, not the main
    // process's line, or self-relaunching installers recurse.
    {
        std::string child_cmd;
        if (commandline && commandline[0]) child_cmd = commandline;
        else if (application && application[0]) child_cmd = application;
        set_guest_process_command_line(child_pid, child_cmd);
    }

    uint64_t tid = ThreadManager::instance().create_thread(module->entry_point, 0, 0, child_pid);
    if (tid == 0) {
        printf("[kernel32] CreateProcessA: failed to start child thread\n");
        return FALSE_VALUE;
    }
    server.set_process_main_thread(child_pid, tid);

    // The child's return code is reported by thread_entry (thread.cpp) when the
    // guest main() returns, via terminate_process(child_pid, rc). Do NOT mark
    // the child terminated here — that would hand the parent an immediate
    // exit code 0 while the child is still running.

    // PROCESS_INFORMATION layout depends on the guest bitness:
    //   x86 guest:  hProcess @0, hThread @4, dwProcessId @8, dwThreadId @0xC
    //               (16 bytes, all 4-byte fields).
    //   x64 guest:  hProcess @0 (8B), hThread @8 (8B), dwProcessId @16 (4B),
    //               dwThreadId @20 (4B)  (32 bytes).
    // The child pid doubles as the process handle in both cases.
    uint8_t* pi = static_cast<uint8_t*>(process_info);
    if (guest_is_32bit()) {
        *reinterpret_cast<uint32_t*>(pi + 0x00) = static_cast<uint32_t>(child_pid);
        *reinterpret_cast<uint32_t*>(pi + 0x04) = static_cast<uint32_t>(tid);
        *reinterpret_cast<uint32_t*>(pi + 0x08) = static_cast<uint32_t>(child_pid);
        *reinterpret_cast<uint32_t*>(pi + 0x0C) = static_cast<uint32_t>(tid);
    } else {
        *reinterpret_cast<uint64_t*>(pi + 0x00) = static_cast<uint64_t>(child_pid);
        *reinterpret_cast<uint64_t*>(pi + 0x08) = static_cast<uint64_t>(tid);
        *reinterpret_cast<uint32_t*>(pi + 0x10) = static_cast<uint32_t>(child_pid);
        *reinterpret_cast<uint32_t*>(pi + 0x14) = static_cast<uint32_t>(tid);
    }
    return TRUE_VALUE;
}

} // namespace

BOOL WINAPI_CALL THETAZ_GetExitCodeProcess(HANDLE process, DWORD* code) {
    if (!code) return FALSE_VALUE;
    ProcessObject* obj = ThetaServer::instance().get_process(reinterpret_cast<uint64_t>(process));
    if (!obj) {
        *code = 0;
        return FALSE_VALUE;
    }
    *code = obj->terminated ? obj->exit_code : 0x103; // STILL_ACTIVE
    return TRUE_VALUE;
}

DWORD WINAPI_CALL THETAZ_GetCurrentProcessId() {
    uint64_t pid = ThreadManager::instance().get_current_guest_pid();
    return static_cast<DWORD>(pid == 0 ? static_cast<uint64_t>(getpid()) : pid);
}

// Exits the calling guest process. For the main process this ends the emulator
// (preserves the pre-CreateProcessA behavior where guest exit() host-exited);
// for a child it marks the child terminated and ends only the child thread.
void terminate_current_process(uint32_t exit_code) {
    uint64_t pid = ThreadManager::instance().get_current_guest_pid();
    printf("ExitProcess(%u) called\n", exit_code);
    fflush(stdout);
    if (pid == 0) {
        // Main guest process: use _exit to terminate immediately without running
        // host atexit handlers (which would crash since they're for the host, not the guest).
        _exit(static_cast<int>(exit_code));
    }
    printf("[kernel32] child process %lu exiting with code %u\n", pid, exit_code);
    ThetaZ::release_guest_process_state(pid);
    ThetaServer::instance().terminate_process(pid, exit_code);
    // Free the child's image mapping so the low zone can be reused by future
    // CreateProcess calls. Without this, each self-relaunch child leaves its
    // ~8MB image mapped and the 0x20000000-0x60000000 zone fills up after ~125
    // children. Only once no other thread of the process is still running guest
    // code from it: unmapping earlier leaves those threads (and the SEH
    // handlers dispatched for them) executing unmapped memory.
    uint64_t child_base = get_guest_process_image_base(pid);
    if (child_base && ThetaServer::instance().live_thread_count(pid) <= 1) {
        PELoader::instance().unload_image(child_base);
        set_guest_process_image_base(pid, 0);
    }
    ThreadManager::instance().exit_thread(exit_code);
}

void WINAPI_CALL THETAZ_ExitProcess(uint32_t exit_code) {
    terminate_current_process(exit_code);
}

// Windows semantics: when the primary thread of a process exits, the process
// exits with that code. Spawned threads end just themselves.
void WINAPI_CALL THETAZ_ExitThread(DWORD dwExitCode) {
    uint64_t pid = ThreadManager::instance().get_current_guest_pid();
    if (pid == 0 && ThreadManager::instance().get_current_thread() <= 1) {
        terminate_current_process(dwExitCode);
    }
    ThreadManager::instance().exit_thread(dwExitCode);
}

void WINAPI_CALL THETAZ_FreeLibraryAndExitThread(HMODULE hLibModule, DWORD dwExitCode) {
    (void)hLibModule; // builtin modules are never unloaded
    THETAZ_ExitThread(dwExitCode);
}

static std::string wchar_to_utf8(const WCHAR* s) {
    std::string out;
    if (!s) return out;
    for (; *s; s++) {
        uint32_t cp = *s;
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

BOOL WINAPI_CALL THETAZ_CreateProcessW(LPCWSTR application, LPWSTR commandline,
                                              void* process, void* thread,
                                              BOOL inherit, DWORD flags, void* env,
                                              LPCWSTR current_dir, void* startup_info,
                                              void* process_info) {
    std::string app = wchar_to_utf8(application);
    std::string cmd = wchar_to_utf8(commandline);
    if (getenv("THETAZ_TRACE_CP")) {
        fprintf(stderr, "[cp] app=\"%s\" cmd=\"%s\"\n", app.c_str(), cmd.c_str());
    }
    return THETAZ_CreateProcessA(app.empty() ? nullptr : app.c_str(),
                                 cmd.empty() ? nullptr : const_cast<char*>(cmd.c_str()),
                                 process, thread, inherit, flags, env,
                                 nullptr, startup_info, process_info);
}

void initialize_process_layer() {
    static std::once_flag once;
    std::call_once(once, [] {
        register_api("kernel32.dll", "CreateProcessA", (void*)THETAZ_CreateProcessA);
        register_api("kernel32.dll", "CreateProcessW", (void*)THETAZ_CreateProcessW);
        register_api("kernel32.dll", "ExitProcess", (void*)THETAZ_ExitProcess);
        register_api("kernel32.dll", "ExitThread", (void*)THETAZ_ExitThread);
        register_api("kernel32.dll", "FreeLibraryAndExitThread", (void*)THETAZ_FreeLibraryAndExitThread);
        register_api("kernel32.dll", "GetExitCodeProcess", (void*)THETAZ_GetExitCodeProcess);
        register_api("kernel32.dll", "GetCurrentProcessId", (void*)THETAZ_GetCurrentProcessId);
    });
}

} // namespace ThetaZ
