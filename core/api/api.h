// ThetaZ — Copyright (c) 2026 ThetaZ Project. MIT License.

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <cstdint>
#include <cstddef>

// Windows calling convention: __stdcall on Windows, empty elsewhere.
#ifndef WINAPI_CALL
#ifdef _WIN32
#define WINAPI_CALL __stdcall
#else
#define WINAPI_CALL
#endif
#endif

// Windows constants
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#define TRUE_VALUE 1
#define FALSE_VALUE 0

// HRESULT
#ifndef HRESULT
using HRESULT = int32_t;
#endif

namespace ThetaZ {

// ---------------------------------------------------------------------------
// Basic Windows types
// ---------------------------------------------------------------------------
using BYTE = uint8_t;
using WORD = uint16_t;
using DWORD = uint32_t;
using BOOL = int32_t;
using LONG = int32_t;
using ULONG = uint32_t;
using USHORT = uint16_t;
using INT = int;
using UINT = uint32_t;
using CHAR = char;
using WCHAR = uint16_t;
using UCHAR = uint8_t;
using PVOID = void*;
using LPVOID = void*;
using LPCVOID = const void*;
using VOID = void;
using HANDLE = void*;
using HMODULE = HANDLE;
using HKEY = void*;
using LPSTR = char*;
using LPWSTR = uint16_t*;
using PWSTR = uint16_t*;
using LPCSTR = const char*;
using LPCWSTR = const uint16_t*;
using LPBYTE = uint8_t*;
using SIZE_T = size_t;
using LPDWORD = DWORD*;
using PDWORD = DWORD*;
using PBOOL = BOOL*;
using LPBOOL = BOOL*;
using PULONG = ULONG*;
using ULONG_PTR = uint64_t;
using LONGLONG = int64_t;
using ULONGLONG = uint64_t;
using NTSTATUS = LONG;
using BOOLEAN = BYTE;
using IErrorInfo = void*;
using HINSTANCE = HANDLE;
using HWND = HANDLE;
using HMENU = HANDLE;
using HHOOK = HANDLE;
using HKL = HANDLE;
using HGLOBAL = HANDLE;
using WinHandle = HANDLE;
using WPARAM = uintptr_t;
using LPARAM = int64_t;
using LRESULT = int64_t;
using LONG_PTR = int64_t;
using UINT_PTR = uintptr_t;
using DWORD_PTR = uintptr_t;
using ATOM = uint16_t;
struct POINT { LONG x; LONG y; };
using SOCKET = uintptr_t;
using LPHANDLE = HANDLE*;
using LPVOID = void*;
using LPCVOID = const void*;
using WNDCLASSEXW = void*;  // Opaque pointer to WNDCLASSEXW
using PCWSTR = LPCWSTR;

using PDWORD_PTR = DWORD_PTR*;
using PSIZE_T = SIZE_T*;
using FARPROC = void*;
using LPPROC_THREAD_ATTRIBUTE_LIST = void*;
using PTP_SIMPLE_CALLBACK = void*;

using HMONITOR = HANDLE;
using HINTERNET = void*;
using INTERNET_PORT = uint16_t;
using WINHTTP_CURRENT_USER_IE_PROXY_CONFIG = void*;
using SECURITY_STATUS = LONG;
using NCRYPT_PROV_HANDLE = void*;
using NCRYPT_KEY_HANDLE = void*;
using NCRYPT_HANDLE = void*;
using NCryptBufferDesc = void*;
using PBYTE = BYTE*;
using SOCKADDR = void*;
using PWCHAR = WCHAR*;
using COMPUTER_NAME_FORMAT = DWORD;
using LPWNDCLASSW = void*;
using LPWSAPROTOCOL_INFOW = void*;
using GROUP = DWORD;
using LPWSABUF = void*;
using LPWSAOVERLAPPED = void*;
using LPWSAOVERLAPPED_COMPLETION_ROUTINE = void*;
using LPWSABUF = void*;
using LPWSAOVERLAPPED = void*;
using LPWSAOVERLAPPED_COMPLETION_ROUTINE = void*;
using LPWSABUF = void*;
using LPWSAOVERLAPPED = void*;
using LPWSAOVERLAPPED_COMPLETION_ROUTINE = void*;
using LPHANDLE = HANDLE*;

// OLEAUT32 types (needed for OLEAUT32 function declarations)
using BSTR = uint16_t*;
using OLECHAR = uint16_t;
using LCID = DWORD;

// Forward declarations for Windows structures
struct WOW64CONTEXT;

// Forward declarations for Windows structures
struct WOW64CONTEXT;

using SOCKADDR = void*;
using PWCHAR = WCHAR*;
using COMPUTER_NAME_FORMAT = DWORD;
using LPWNDCLASSW = void*;
using LPWSAPROTOCOL_INFOW = void*;
using GROUP = DWORD;
using LPWSABUF = void*;
using LPWSAOVERLAPPED = void*;
using LPWSAOVERLAPPED_COMPLETION_ROUTINE = void*;
using LPHANDLE = HANDLE*;

// ---------------------------------------------------------------------------
// PathResolver functions
// ---------------------------------------------------------------------------
std::string get_prefix_path();
std::string get_drive_c_path();
std::string get_windows_path();
std::string get_program_files_path();
std::string get_user_profile_path();
std::string get_system32_path();

// ---------------------------------------------------------------------------
// API Registry
// ---------------------------------------------------------------------------
class ApiRegistry {
public:
    static ApiRegistry& instance();
    void register_function(const std::string& dll, const std::string& name, void* func);
    void register_function_ordinal(const std::string& dll, uint16_t ordinal, void* func);
    void* find_function(const std::string& dll, const std::string& name) const;
    void* find_function_ordinal(const std::string& dll, uint16_t ordinal) const;
    const char* name_for_ordinal(const std::string& dll, uint16_t ordinal) const;
    bool has_dll(const std::string& dll) const;

private:
    ApiRegistry();
    std::unordered_map<std::string, std::unordered_map<std::string, void*>> by_name_;
    std::unordered_map<std::string, std::unordered_map<uint16_t, void*>> by_ordinal_;
};

void register_api(const std::string& dll, const std::string& name, void* func);
void register_api_ordinal(const std::string& dll, uint16_t ordinal, void* func);
void set_crt_command_line(int argc, char** argv);
std::string get_crt_command_line();
void set_guest_process_command_line(uint64_t pid, const std::string& cmdline);
void set_main_module_base(uint64_t base);
uint64_t get_main_module_base();
void set_main_image_dir(const char* dir);
const std::string& get_main_image_dir();
bool guest_is_32bit();
void set_guest_is_32bit(bool is_32bit);
void set_guest_process_image_base(uint64_t pid, uint64_t base);
uint64_t get_guest_process_image_base(uint64_t pid);
void release_guest_process_state(uint64_t pid);
void release_fls_process(uint64_t pid);
void* load_guest_module(const char* path);
void* get_builtin_module_handle(const char* name);
const char* builtin_handle_name(void* handle);

// Guest-visible memory
void* alloc_guest_heap(size_t size);
void free_guest_heap(void* p);
void* realloc_guest_heap(void* p, size_t size);
size_t HeapBlockSize(const void* p);
void* alloc_guest_shared(size_t size);

// UCRT / layer initialization
void initialize_ucrt_layer();
void initialize_api_layer();
void initialize_user32_layer();
void initialize_window_layer();
void initialize_process_layer();
void initialize_oleaut32_layer();
void initialize_ole32_apis();
void initialize_environment();

// Process / memory helpers (process.cpp, api.cpp)
void terminate_current_process(uint32_t exit_code);
void* calloc_guest_heap(size_t count, size_t size);

// Guest vararg slot width used by the CRT printf/sprintf family (ucrt.cpp).
void set_guest_vararg_slot(int slot);

// Additional runtime kernel32 APIs (kernel32_runtime.cpp).
void initialize_kernel32_runtime();
void initialize_ws2_32_layer();
void initialize_wine_compat_layer();
void initialize_kernel32_batch();
void theta_record_command_line(int argc, char** argv);
void theta_record_environment();

// kernel32 process APIs (process.cpp)
void WINAPI_CALL THETAZ_ExitProcess(uint32_t exit_code);
BOOL WINAPI_CALL THETAZ_GetExitCodeProcess(HANDLE process, DWORD* code);
DWORD WINAPI_CALL THETAZ_GetCurrentProcessId();

// Display/GDI bridge (window.cpp). Renders UTF-8 text to the X11 window owned
// by the given hwnd. Called by the gdi32 TextOutA/W implementations.
void theta_gdi_text(uint64_t hwnd, int x, int y, const char* utf8_text);

// Associates a DC handle with a guest hwnd so GDI draws route to the right
// window (window.cpp calls this from BeginPaint; GetDC registers itself).
void theta_register_dc_hwnd(uintptr_t dc, uint64_t hwnd);

// UTF-8 / UTF-16LE string conversion helpers (kernel32_runtime.cpp)
std::string utf8_to_utf16le(const std::string& in, std::u16string& out);
std::string utf16le_to_utf8(const uint16_t* s);
int generic_api_stub();

// ---------------------------------------------------------------------------
// Forward declarations for OLEAUT32 functions (defined in oleaut32.cpp)
// ---------------------------------------------------------------------------
    HRESULT WINAPI_CALL THETAZ_SafeArrayCreate(DWORD vt, UINT cDims, void* rgsabound);
    HRESULT WINAPI_CALL THETAZ_SafeArrayDestroy(void* psa);
    UINT WINAPI_CALL THETAZ_SafeArrayGetDim(void* psa);
    HRESULT WINAPI_CALL THETAZ_SafeArrayGetLBound(void* psa, UINT nDim, long* plLbound);
    HRESULT WINAPI_CALL THETAZ_VarBstrFromUI1(BYTE bIn, LCID lcid, unsigned short dwFlags, BSTR* pbstrOut);
    HRESULT WINAPI_CALL THETAZ_VarBstrFromI2(short iIn, LCID lcid, unsigned short dwFlags, BSTR* pbstrOut);
    HRESULT WINAPI_CALL THETAZ_VarBstrFromI4(long lIn, LCID lcid, unsigned short dwFlags, BSTR* pbstrOut);
    HRESULT WINAPI_CALL THETAZ_VarBstrFromR4(float fltIn, LCID lcid, unsigned short dwFlags, BSTR* pbstrOut);
    HRESULT WINAPI_CALL THETAZ_VarBstrFromR8(double dblIn, LCID lcid, unsigned short dwFlags, BSTR* pbstrOut);
    HRESULT WINAPI_CALL THETAZ_VarBstrFromI8(LONGLONG i64In, LCID lcid, unsigned short dwFlags, BSTR* pbstrOut);

} // namespace ThetaZ

// ---------------------------------------------------------------------------
// Global aliases so translation units that define WinAPI functions outside the
// ThetaZ namespace still resolve the shared Windows types.
// ---------------------------------------------------------------------------
using ThetaZ::BYTE;
using ThetaZ::WORD;
using ThetaZ::DWORD;
using ThetaZ::BOOL;
using ThetaZ::LONG;
using ThetaZ::ULONG;
using ThetaZ::USHORT;
using ThetaZ::INT;
using ThetaZ::UINT;
using ThetaZ::CHAR;
using ThetaZ::WCHAR;
using ThetaZ::PVOID;
using ThetaZ::LPVOID;
using ThetaZ::LPCVOID;
using ThetaZ::VOID;
using ThetaZ::HANDLE;
using ThetaZ::HMODULE;
using ThetaZ::HKEY;
using ThetaZ::LPSTR;
using ThetaZ::LPWSTR;
using ThetaZ::LPCSTR;
using ThetaZ::LPCWSTR;
using ThetaZ::LPBYTE;
using ThetaZ::SIZE_T;
using ThetaZ::LPDWORD;
using ThetaZ::PDWORD;
using ThetaZ::PBOOL;
using ThetaZ::LPBOOL;
using ThetaZ::PULONG;
using ThetaZ::ULONG_PTR;
using ThetaZ::LONGLONG;
using ThetaZ::ULONGLONG;
using ThetaZ::NTSTATUS;
using ThetaZ::BOOLEAN;
using ThetaZ::IErrorInfo;
using ThetaZ::register_api;
using ThetaZ::register_api_ordinal;
using ThetaZ::initialize_oleaut32_layer;
using ThetaZ::initialize_user32_layer;
using ThetaZ::initialize_window_layer;
using ThetaZ::initialize_process_layer;
using ThetaZ::initialize_api_layer;
using ThetaZ::initialize_environment;
using ThetaZ::load_guest_module;
using ThetaZ::get_builtin_module_handle;
using ThetaZ::builtin_handle_name;
using ThetaZ::set_main_module_base;
using ThetaZ::get_main_module_base;
using ThetaZ::set_main_image_dir;
using ThetaZ::get_main_image_dir;
using ThetaZ::set_guest_process_image_base;
using ThetaZ::get_guest_process_image_base;
using ThetaZ::alloc_guest_heap;
using ThetaZ::free_guest_heap;
using ThetaZ::realloc_guest_heap;
using ThetaZ::alloc_guest_shared;
using ThetaZ::set_crt_command_line;
using ThetaZ::initialize_ucrt_layer;
using ThetaZ::terminate_current_process;
using ThetaZ::calloc_guest_heap;
using ThetaZ::THETAZ_ExitProcess;
using ThetaZ::THETAZ_GetExitCodeProcess;
using ThetaZ::THETAZ_GetCurrentProcessId;
using ThetaZ::HINSTANCE;
using ThetaZ::HWND;
using ThetaZ::HMENU;
using ThetaZ::WinHandle;
using ThetaZ::WPARAM;
using ThetaZ::LPARAM;
using ThetaZ::LRESULT;
using ThetaZ::LONG_PTR;
using ThetaZ::UINT_PTR;
using ThetaZ::DWORD_PTR;
using ThetaZ::ATOM;
using ThetaZ::set_guest_vararg_slot;
using ThetaZ::initialize_kernel32_runtime;
using ThetaZ::initialize_ws2_32_layer;
using ThetaZ::initialize_wine_compat_layer;
using ThetaZ::initialize_ole32_apis;
using ThetaZ::theta_record_command_line;
using ThetaZ::theta_record_environment;

// OLEAUT32 ordinal functions for WINWORD
using ThetaZ::THETAZ_SafeArrayCreate;
using ThetaZ::THETAZ_SafeArrayDestroy;
using ThetaZ::THETAZ_SafeArrayGetDim;
using ThetaZ::THETAZ_SafeArrayGetLBound;
using ThetaZ::THETAZ_VarBstrFromUI1;
using ThetaZ::THETAZ_VarBstrFromI2;
using ThetaZ::THETAZ_VarBstrFromI4;
using ThetaZ::THETAZ_VarBstrFromR4;
using ThetaZ::THETAZ_VarBstrFromR8;
using ThetaZ::THETAZ_VarBstrFromI8;
