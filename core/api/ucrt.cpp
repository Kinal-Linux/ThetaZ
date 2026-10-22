// ThetaZ — Copyright (c) 2026 ThetaZ Project. MIT License.

#include "core/api/api.h"
#include "core/x86/api_dispatch.h"
#include "core/x86/x86.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cmath>
#include <cctype>
#include <cwchar>
#include <cwctype>
#include <csignal>
#include <csetjmp>
#include <clocale>
#include <cerrno>
#include <ctime>
#include <climits>
#include <cfloat>
#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <malloc.h>
#include <fenv.h>

using errno_t = int;

// UCRT / msvcrt compatibility layer.
//
// Modern MinGW links against "api-ms-win-crt-*.dll" (an api-set that aliases
// ucrtbase.dll). The ThetaZ loader resolves imports by DLL name + function,
// so every CRT function below is registered under the api-set names, plus
// ucrtbase.dll and msvcrt.dll where applicable.

namespace ThetaZ {

// ---------------------------------------------------------------------------
// minimal printf/puts so hello-world style programs produce visible output
// ---------------------------------------------------------------------------

// Implement just enough %-formatting for common cases.
// MS x64 varargs are passed as a contiguous run of 8-byte slots in the
// caller's home space; `ap` advances 8 bytes per argument. A 32-bit guest
// pushes 4-byte slots, so x86_dispatch_api sets g_vararg_slot to 4 around the
// call. Slot size is thread-local because the 64-bit guest path runs natively.
thread_local int g_vararg_slot = 8;

namespace {
struct VarArgs {
    uint8_t* ptr;

    int get_int() { int v = *reinterpret_cast<int*>(ptr); ptr += g_vararg_slot; return v; }
    unsigned get_uint() { unsigned v = *reinterpret_cast<unsigned*>(ptr); ptr += g_vararg_slot; return v; }
    long get_long() {
        if (g_vararg_slot == 4) { long v = *reinterpret_cast<int*>(ptr); ptr += 4; return v; }
        long v = *reinterpret_cast<long*>(ptr); ptr += 8; return v;
    }
    unsigned long get_ulong() {
        if (g_vararg_slot == 4) { unsigned long v = *reinterpret_cast<unsigned*>(ptr); ptr += 4; return v; }
        unsigned long v = *reinterpret_cast<unsigned long*>(ptr); ptr += 8; return v;
    }
    long long get_ll() { long long v = *reinterpret_cast<long long*>(ptr); ptr += 8; return v; }
    unsigned long long get_ull() { unsigned long long v = *reinterpret_cast<unsigned long long*>(ptr); ptr += 8; return v; }
    double get_double() { double v = *reinterpret_cast<double*>(ptr); ptr += 8; return v; }
    long double get_ld() { double v = *reinterpret_cast<double*>(ptr); ptr += 8; return (long double)v; }
    const char* get_str() {
        if (g_vararg_slot == 4) { const char* v = reinterpret_cast<const char*>(static_cast<uintptr_t>(*reinterpret_cast<uint32_t*>(ptr))); ptr += 4; return v; }
        const char* v = *reinterpret_cast<char* const*>(ptr); ptr += 8; return v;
    }
    void* get_ptr() {
        if (g_vararg_slot == 4) { void* v = reinterpret_cast<void*>(static_cast<uintptr_t>(*reinterpret_cast<uint32_t*>(ptr))); ptr += 4; return v; }
        void* v = *reinterpret_cast<void**>(ptr); ptr += 8; return v;
    }
    size_t get_size() {
        if (g_vararg_slot == 4) { size_t v = *reinterpret_cast<uint32_t*>(ptr); ptr += 4; return v; }
        size_t v = *reinterpret_cast<size_t*>(ptr); ptr += 8; return v;
    }
    ssize_t get_ssize() {
        if (g_vararg_slot == 4) { ssize_t v = *reinterpret_cast<int32_t*>(ptr); ptr += 4; return v; }
        ssize_t v = *reinterpret_cast<ssize_t*>(ptr); ptr += 8; return v;
    }
    // Guest wchar_t is 16-bit on both supported guest ABIs.
    const uint16_t* get_wstr() {
        if (g_vararg_slot == 4) {
            const uint16_t* v = reinterpret_cast<const uint16_t*>(
                static_cast<uintptr_t>(*reinterpret_cast<uint32_t*>(ptr)));
            ptr += 4;
            return v;
        }
        const uint16_t* v = *reinterpret_cast<const uint16_t* const*>(ptr);
        ptr += 8;
        return v;
    }
};
}

// Convert a UTF-16 (guest wchar_t) string to UTF-8 for host formatting.
static std::string utf16_to_utf8_string(const uint16_t* s) {
    std::string out;
    if (!s) return out;
    for (size_t i = 0; s[i]; ++i) {
        uint32_t cp = s[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && s[i + 1] >= 0xDC00 && s[i + 1] <= 0xDFFF) {
            cp = 0x10000u + ((cp - 0xD800u) << 10) + (s[i + 1] - 0xDC00u);
            ++i;
        } else if (cp >= 0xD800 && cp <= 0xDFFF) {
            cp = 0xFFFDu;
        }
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

static std::u16string utf8_to_utf16_string(const std::string& in) {
    std::u16string out;
    for (size_t i = 0; i < in.size();) {
        unsigned char c = static_cast<unsigned char>(in[i]);
        uint32_t cp = 0;
        size_t extra = 0;
        if (c < 0x80) { cp = c; extra = 0; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
        else { cp = 0xFFFD; extra = 0; }
        ++i;
        for (size_t k = 0; k < extra; ++k) {
            if (i >= in.size() || (static_cast<unsigned char>(in[i]) & 0xC0) != 0x80) {
                cp = 0xFFFD;
                break;
            }
            cp = (cp << 6) | (static_cast<unsigned char>(in[i]) & 0x3F);
            ++i;
        }
        if (cp <= 0xFFFF) {
            out.push_back(static_cast<char16_t>(cp));
        } else if (cp <= 0x10FFFF) {
            cp -= 0x10000;
            out.push_back(static_cast<char16_t>(0xD800 + (cp >> 10)));
            out.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
        } else {
            out.push_back(static_cast<char16_t>(0xFFFD));
        }
    }
    return out;
}

void set_guest_vararg_slot(int slot) {
    g_vararg_slot = slot;
}

static void append_padded(std::string& out, std::string value, int width, bool left) {
    if (width < 0) width = 0;
    size_t pad = static_cast<size_t>(width) > value.size()
                     ? static_cast<size_t>(width) - value.size() : 0;
    if (!left) out.append(pad, ' ');
    out += value;
    if (left) out.append(pad, ' ');
}

// Format guest arguments. `wide_strings` selects the Windows wide-printf
// meaning of %s/%c (%s consumes a UTF-16 string, %S/%hs a narrow one); narrow
// printf uses the opposite mapping. Numeric conversions are shared.
static std::string format_string(const char* fmt, VarArgs& va, bool wide_strings = false) {
    std::string out;
    const char* p = fmt;
    while (*p) {
        if (*p != '%') {
            out += *p++;
            continue;
        }
        p++;
        if (*p == '%') { out += '%'; p++; continue; }

        // flags/width/precision
        std::string flags;
        bool zero_flag = false;
        while (*p && strchr("-+ #0", *p)) {
            if (*p == '0') zero_flag = true;
            if (*p != '0') flags += *p;
            p++;
        }
        int width = 0;
        while (*p && strchr("0123456789", *p)) width = width * 10 + (*p++ - '0');
        if (*p == '*') {
            width = va.get_int();
            if (width < 0) { flags += '-'; width = -width; }
            p++;
        }
        int prec = -1;
        if (*p == '.') {
            p++;
            prec = 0;
            if (*p == '*') { prec = va.get_int(); p++; }
            else while (*p && strchr("0123456789", *p)) prec = prec * 10 + (*p++ - '0');
        }
        const bool left = flags.find('-') != std::string::npos;

        // Length modifiers. Consume them here so numeric conversions below can
        // build an equivalent host format string.
        enum Length { LEN_DEFAULT, LEN_HH, LEN_H, LEN_L, LEN_LL, LEN_BIG_L,
                      LEN_Z, LEN_J, LEN_T, LEN_I32, LEN_I64, LEN_W } len = LEN_DEFAULT;
        if (*p == 'h') {
            len = LEN_H; p++;
            if (*p == 'h') { len = LEN_HH; p++; }
        } else if (*p == 'l') {
            len = LEN_L; p++;
            if (*p == 'l') { len = LEN_LL; p++; }
        } else if (*p == 'L') { len = LEN_BIG_L; p++; }
        else if (*p == 'z') { len = LEN_Z; p++; }
        else if (*p == 'j') { len = LEN_J; p++; }
        else if (*p == 't') { len = LEN_T; p++; }
        else if (*p == 'w') { len = LEN_W; p++; }
        else if (*p == 'I') {
            p++;
            if (p[0] == '6' && p[1] == '4') { len = LEN_I64; p += 2; }
            else if (p[0] == '3' && p[1] == '2') { len = LEN_I32; p += 2; }
            else { len = LEN_I32; }
        }

        char conv = *p ? *p++ : 0;
        char buf[256];

        // Build the host conversion spec. Long modifiers are represented by
        // their C equivalents; Windows-specific names are normalized above.
        std::string spec = "%" + flags;
        if (zero_flag && prec < 0) spec += '0';
        if (width > 0) { char wb[16]; snprintf(wb, sizeof(wb), "%d", width); spec += wb; }
        if (prec >= 0) { char pb[16]; snprintf(pb, sizeof(pb), ".%d", prec); spec += pb; }

        switch (conv) {
        case 'd':
        case 'i': {
            long long v;
            if (len == LEN_L) v = va.get_long();
            else if (len == LEN_LL || len == LEN_I64 || len == LEN_J) v = va.get_ll();
            else if (len == LEN_Z || len == LEN_T) v = va.get_ssize();
            else {
                v = va.get_int();
                if (len == LEN_HH || len == LEN_H) v = static_cast<short>(v);
            }
            spec += "lld";
            snprintf(buf, sizeof(buf), spec.c_str(), v);
            out += buf;
            break;
        }
        case 'u':
        case 'o':
        case 'x':
        case 'X': {
            unsigned long long v;
            if (len == LEN_L) v = va.get_ulong();
            else if (len == LEN_LL || len == LEN_I64 || len == LEN_J) v = va.get_ull();
            else if (len == LEN_Z || len == LEN_T) v = va.get_size();
            else {
                v = va.get_uint();
                if (len == LEN_HH || len == LEN_H) v = static_cast<unsigned short>(v);
            }
            spec += "ll";
            spec += conv;
            snprintf(buf, sizeof(buf), spec.c_str(), v);
            out += buf;
            break;
        }
        case 'c':
        case 'C': {
            int cv = va.get_int();
            bool wide_char = (conv == 'C') ? !wide_strings : wide_strings;
            if (len == LEN_L) wide_char = true;
            else if (len == LEN_H) wide_char = false;
            std::string cs;
            if (wide_char) {
                uint16_t wc[3] = {static_cast<uint16_t>(cv), 0, 0};
                if (cv > 0xFFFF) {
                    uint32_t cp = static_cast<uint32_t>(cv) - 0x10000u;
                    wc[0] = static_cast<uint16_t>(0xD800 + (cp >> 10));
                    wc[1] = static_cast<uint16_t>(0xDC00 + (cp & 0x3FF));
                }
                cs = utf16_to_utf8_string(wc);
            } else {
                cs.push_back(static_cast<char>(cv));
            }
            if (prec >= 0 && cs.size() > static_cast<size_t>(prec)) cs.resize(prec);
            append_padded(out, cs, width, left);
            break;
        }
        case 's':
        case 'S': {
            bool wide_str = (conv == 'S') ? !wide_strings : wide_strings;
            if (len == LEN_L) wide_str = true;
            else if (len == LEN_H) wide_str = false;
            std::string cs;
            if (wide_str) {
                const uint16_t* sp = va.get_wstr();
                cs = sp ? utf16_to_utf8_string(sp) : "(null)";
            } else {
                const char* sp = va.get_str();
                cs = sp ? sp : "(null)";
            }
            if (prec >= 0 && cs.size() > static_cast<size_t>(prec)) cs.resize(prec);
            append_padded(out, cs, width, left);
            break;
        }
        case 'p': {
            void* v = va.get_ptr();
            spec += 'p';
            snprintf(buf, sizeof(buf), spec.c_str(), v);
            out += buf;
            break;
        }
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A': {
            if (len == LEN_BIG_L) {
                long double v = va.get_ld();
                spec += 'L';
                spec += conv;
                snprintf(buf, sizeof(buf), spec.c_str(), v);
            } else {
                double v = va.get_double();
                spec += conv;
                snprintf(buf, sizeof(buf), spec.c_str(), v);
            }
            out += buf;
            break;
        }
        case 'n': {
            int* np = static_cast<int*>(va.get_ptr());
            if (np) *np = static_cast<int>(out.size());
            break;
        }
        default:
            // Unknown conversion: emit it literally and do not consume an
            // argument (matches the conservative behavior of the old parser).
            out += '%';
            if (conv) out += conv;
        }
    }
    return out;
}

static int WINAPI_CALL ucrt_printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    VarArgs va{reinterpret_cast<uint8_t*>(args)};
    std::string out = format_string(fmt, va);
    va_end(args);
    fputs(out.c_str(), stdout);
    fflush(stdout);
    return static_cast<int>(out.size());
}

static int WINAPI_CALL ucrt_puts(const char* s) {
    fputs(s, stdout);
    fputc('\n', stdout);
    fflush(stdout);
    return 0;
}

// FILE* values crossing the host/guest boundary: glibc FILE* pointers live
// above 4GB, so the guest (32-bit or 64-bit) never sees them. Instead stdio
// entry points that *return* a FILE* hand out a token (a shared low-memory
// address) and entry points that *receive* a FILE* translate it back.
namespace {
struct FileTokenRegistry {
    std::mutex mtx;
    std::unordered_map<void*, FILE*> tokens;   // token -> host FILE*
    std::unordered_map<FILE*, void*> by_file;  // host FILE* -> token
};
FileTokenRegistry& file_tokens() {
    static FileTokenRegistry r;
    return r;
}
void* token_for_file(FILE* f) {
    if (!f) return nullptr;
    FileTokenRegistry& r = file_tokens();
    std::lock_guard<std::mutex> lock(r.mtx);
    auto it = r.by_file.find(f);
    if (it != r.by_file.end()) return it->second;
    void* tok = alloc_guest_shared(sizeof(void*));
    if (!tok) return static_cast<void*>(f);
    r.tokens[tok] = f;
    r.by_file[f] = tok;
    return tok;
}
FILE* file_for_token(void* tok) {
    if (!tok) return nullptr;
    FileTokenRegistry& r = file_tokens();
    std::lock_guard<std::mutex> lock(r.mtx);
    auto it = r.tokens.find(tok);
    if (it != r.tokens.end()) return it->second;
    return static_cast<FILE*>(tok);
}
}

// FILE* slots 0/1/2 -> stdin/stdout/stderr
static void* WINAPI_CALL ucrt___acrt_iob_func(unsigned index) {
    static FILE* slots[3] = {nullptr, nullptr, nullptr};
    if (index < 3 && !slots[index]) {
        slots[0] = stdin;
        slots[1] = stdout;
        slots[2] = stderr;
    }
    return index < 3 ? token_for_file(slots[index]) : nullptr;
}

static int* WINAPI_CALL ucrt___p__commode() {
    static int* p = static_cast<int*>(alloc_guest_shared(sizeof(int)));
    return p;
}

static int* WINAPI_CALL ucrt___p__fmode() {
    static int* p = static_cast<int*>(alloc_guest_shared(sizeof(int)));
    return p;
}

static int WINAPI_CALL ucrt_setvbuf(void* stream, char* buffer, int mode, size_t size) {
    (void)file_for_token(stream);
    (void)buffer;
    (void)mode;
    (void)size;
    return 0;
}

static int WINAPI_CALL ucrt___stdio_common_vfprintf(uint64_t options, void* stream,
                                                    const char* format, void* locale,
                                                    void* va_ptr) {
    (void)options;
    (void)locale;
    VarArgs va{reinterpret_cast<uint8_t*>(va_ptr)};
    std::string out = format_string(format, va);
    FILE* out_stream = file_for_token(stream);
    if (!out_stream) out_stream = stdout;
    fputs(out.c_str(), out_stream);
    fflush(out_stream);
    return static_cast<int>(out.size());
}

static int WINAPI_CALL ucrt_fwrite(const void* buffer, size_t size, size_t count, void* stream) {
    FILE* st = file_for_token(stream);
    if (st == stdout || st == stderr || st == nullptr) {
        size_t total = size * count;
        fwrite(buffer, 1, total, stdout);
        return count;
    }
    return 0;
}

static int WINAPI_CALL ucrt_fread(void* buffer, size_t size, size_t count, void* stream) {
    FILE* st = file_for_token(stream);
    if (st == nullptr) st = stdin;
    if (size == 0 || count == 0) return 0;
    size_t n = fread(buffer, size, count, st);
    return static_cast<int>(n);
}

static int WINAPI_CALL ucrt_fgetc(void* stream) {
    FILE* st = file_for_token(stream);
    if (st == nullptr) st = stdin;
    return fgetc(st);
}

static void* WINAPI_CALL ucrt_fopen(const char* filename, const char* mode) {
    return token_for_file(fopen(filename, mode));
}

static int WINAPI_CALL ucrt_fclose(void* stream) {
    return fclose(file_for_token(stream));
}

static int WINAPI_CALL ucrt_fflush(void* stream) {
    FILE* st = file_for_token(stream);
    if (st == nullptr) st = stdout;
    fflush(st);
    return 0;
}

static int WINAPI_CALL ucrt_fprintf(void* stream, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    VarArgs va{reinterpret_cast<uint8_t*>(args)};
    std::string out = format_string(fmt, va);
    va_end(args);
    FILE* out_stream = static_cast<FILE*>(stream);
    if (!out_stream) out_stream = stdout;
    fputs(out.c_str(), out_stream);
    fflush(out_stream);
    return static_cast<int>(out.size());
}

// ---------------------------------------------------------------------------
// CRT startup / runtime functions
// ---------------------------------------------------------------------------

namespace {

// atexit/_crt_atexit callback queue (run LIFO at CRT termination).
std::mutex g_atexit_mutex;
std::vector<uintptr_t> g_atexit_queue;

typedef void (__attribute__((ms_abi)) *Pfnv_t)();
typedef int (__attribute__((ms_abi)) *Pifv_t)();

// Real command line provided by the host (main_entry -> set_crt_command_line).
// Everything the CRT hands to the guest through __p___argc / __p___argv /
// __p__environ lives in shared low memory (<4GB): the getters return those
// pointers, and a 32-bit guest must be able to dereference them.
static int* g_shared_argc = nullptr;
static char** g_shared_argv = nullptr;      // argv array (guest-visible)
static char*** g_shared_argv_ptr = nullptr; // &g_shared_argv, what __p___argv returns
static char** g_shared_env = nullptr;       // environ array
static char*** g_shared_env_ptr = nullptr;  // &g_shared_env, what __p__environ returns
static char* g_shared_name = nullptr;       // default argv[0]
static char** g_shared_acmdln = nullptr;    // &acmdln string, what __p__acmdln returns
static std::string g_crt_cmdline;           // main process GetCommandLine string

static void init_crt_shared() {
    if (g_shared_argc) return;
    g_shared_argc = static_cast<int*>(alloc_guest_shared(sizeof(int)));
    g_shared_argv = static_cast<char**>(alloc_guest_shared(256 * sizeof(char*)));
    g_shared_argv_ptr = static_cast<char***>(alloc_guest_shared(sizeof(char**)));
    g_shared_env = static_cast<char**>(alloc_guest_shared(sizeof(char*)));
    g_shared_env_ptr = static_cast<char***>(alloc_guest_shared(sizeof(char**)));
    g_shared_name = static_cast<char*>(alloc_guest_shared(256));
    g_shared_acmdln = static_cast<char**>(alloc_guest_shared(sizeof(char*)));
    *g_shared_argc = 1;
    strcpy(g_shared_name, "thetaz");
    g_shared_argv[0] = g_shared_name;
    g_shared_argv[1] = nullptr;
    g_shared_env[0] = nullptr;
    *g_shared_acmdln = g_shared_name;
    *g_shared_argv_ptr = g_shared_argv;
    *g_shared_env_ptr = g_shared_env;
}

// Signal handler table (standard C signal numbers).
void* g_signal_handlers[NSIG];

// App type / parameter-handler / new-mode state.
int g_app_type = 0;
void* g_invalid_parameter_handler = nullptr;
int g_new_mode = 0;
void* g_user_math_error = nullptr;

} // namespace

std::string get_crt_command_line() {
    std::lock_guard<std::mutex> lock(g_atexit_mutex);
    return g_crt_cmdline;
}

void set_crt_command_line(int argc, char** argv) {
    std::lock_guard<std::mutex> lock(g_atexit_mutex);
    init_crt_shared();
    // Build the guest command line the way Windows presents it: argv[0]
    // followed by the remaining arguments, quoting any that contain spaces.
    {
        std::string line;
        for (int i = 1; i < argc; i++) {
            if (!line.empty()) line += ' ';
            std::string arg = argv[i] ? argv[i] : "";
            if (arg.find(' ') != std::string::npos) arg = "\"" + arg + "\"";
            line += arg;
        }
        g_crt_cmdline = line;
    }
    int n = argc > 0 ? argc - 1 : 0;
    if (n > 255) n = 255;
    *g_shared_argc = n > 0 ? n : 1;
    if (n <= 0) {
        g_shared_argv[0] = g_shared_name;
        g_shared_argv[1] = nullptr;
        return;
    }
    for (int i = 0; i < n; i++) {
        size_t len = strlen(argv[i + 1]);
        char* dst = static_cast<char*>(alloc_guest_shared(len + 1));
        if (!dst) {
            n = i;
            break;
        }
        memcpy(dst, argv[i + 1], len + 1);
        g_shared_argv[i] = dst;
    }
    *g_shared_argc = n;
    g_shared_argv[n] = nullptr;
}

void run_crt_exit_callbacks() {
    for (;;) {
        uintptr_t fn = 0;
        {
            std::lock_guard<std::mutex> lock(g_atexit_mutex);
            if (g_atexit_queue.empty()) break;
            fn = g_atexit_queue.back();
            g_atexit_queue.pop_back();
        }
        if (fn) {
            if (x86_dispatch_active() && fn < 0x100000000ull) {
                // 32-bit guest: the callback is guest code; the interpreter must
                // run it, the host cannot call a 32-bit address natively.
                x86_call_guest(static_cast<uint32_t>(fn));
            } else {
                reinterpret_cast<Pfnv_t>(fn)();
            }
        }
    }
    fflush(nullptr);
}

static int WINAPI_CALL ucrt__set_app_type(int app_type) {
    std::lock_guard<std::mutex> lock(g_atexit_mutex);
    int previous = g_app_type;
    g_app_type = app_type;
    return previous;
}

static int WINAPI_CALL ucrt__configure_narrow_argv(int mode) {
    (void)mode;
    return 0;
}

static int WINAPI_CALL ucrt__initialize_narrow_environment() {
    return 0;
}

static void* WINAPI_CALL ucrt__set_invalid_parameter_handler(void* handler) {
    std::lock_guard<std::mutex> lock(g_atexit_mutex);
    void* previous = g_invalid_parameter_handler;
    g_invalid_parameter_handler = handler;
    return previous;
}

static void* WINAPI_CALL ucrt___p___argc() {
    init_crt_shared();
    return g_shared_argc;
}

static void* WINAPI_CALL ucrt___p___argv() {
    init_crt_shared();
    return g_shared_argv_ptr;
}

static void* WINAPI_CALL ucrt___p__acmdln() {
    init_crt_shared();
    return g_shared_acmdln;
}

static int WINAPI_CALL ucrt__crt_atexit(void* func) {
    std::lock_guard<std::mutex> lock(g_atexit_mutex);
    if (func) g_atexit_queue.push_back(reinterpret_cast<uintptr_t>(func));
    return 0;
}

static void* WINAPI_CALL ucrt__onexit(void* func) {
    std::lock_guard<std::mutex> lock(g_atexit_mutex);
    if (func) g_atexit_queue.push_back(reinterpret_cast<uintptr_t>(func));
    return func;
}

static void WINAPI_CALL ucrt__initterm(void* first, void* last) {
    uintptr_t* p = static_cast<uintptr_t*>(first);
    uintptr_t* end = static_cast<uintptr_t*>(last);
    while (p < end) {
        if (*p != 0) {
            reinterpret_cast<Pfnv_t>(*p)();
        }
        p++;
    }
}

static int WINAPI_CALL ucrt__initterm_e(void* first, void* last) {
    uintptr_t* p = static_cast<uintptr_t*>(first);
    uintptr_t* end = static_cast<uintptr_t*>(last);
    while (p < end) {
        if (*p != 0) {
            int result = reinterpret_cast<Pifv_t>(*p)();
            fprintf(stderr, "[initterm_e] init 0x%llx -> %d\n",
                    (unsigned long long)*p, result);
            if (result != 0) return result;
        }
        p++;
    }
    return 0;
}

static void WINAPI_CALL ucrt__cexit() {
    run_crt_exit_callbacks();
}

static void WINAPI_CALL ucrt_abort() {
    abort();
}

static void WINAPI_CALL ucrt_exit(int code) {
    run_crt_exit_callbacks();
    terminate_current_process(static_cast<uint32_t>(code));
}

static void WINAPI_CALL ucrt__exit(int code) {
    terminate_current_process(static_cast<uint32_t>(code));
}

static void* WINAPI_CALL ucrt_signal(int sig, void* handler) {
    if (sig < 0 || sig >= NSIG) return reinterpret_cast<void*>(SIG_ERR);
    std::lock_guard<std::mutex> lock(g_atexit_mutex);
    void* previous = g_signal_handlers[sig];
    if (!previous) previous = reinterpret_cast<void*>(SIG_DFL);
    g_signal_handlers[sig] = handler;
    return previous;
}

// math functions
static double WINAPI_CALL ucrt_sqrt(double x) { return ::sqrt(x); }
static double WINAPI_CALL ucrt_fabs(double x) { return x < 0 ? -x : x; }

// string functions
static size_t WINAPI_CALL ucrt_strlen(const char* s) { return strlen(s); }
static char* WINAPI_CALL ucrt_strcpy(char* dst, const char* src) { return strcpy(dst, src); }
static char* WINAPI_CALL ucrt_strcat(char* dst, const char* src) { return strcat(dst, src); }
static int WINAPI_CALL ucrt_strcmp(const char* a, const char* b) { return strcmp(a, b); }
static int WINAPI_CALL ucrt_strncmp(const char* a, const char* b, size_t n) { return strncmp(a, b, n); }
static void* WINAPI_CALL ucrt_memcpy(void* dst, const void* src, size_t n) { return memcpy(dst, src, n); }
static void* WINAPI_CALL ucrt_memset(void* dst, int c, size_t n) { return memset(dst, c, n); }
static void* WINAPI_CALL ucrt_memmove(void* dst, const void* src, size_t n) { return memmove(dst, src, n); }
static int WINAPI_CALL ucrt_memcmp(const void* a, const void* b, size_t n) { return memcmp(a, b, n); }

// heap functions
static void* WINAPI_CALL ucrt_malloc(size_t size) { return alloc_guest_heap(size); }
static void WINAPI_CALL ucrt_free(void* ptr) { free_guest_heap(ptr); }
static void* WINAPI_CALL ucrt_calloc(size_t n, size_t size) { return calloc_guest_heap(n, size); }
static void* WINAPI_CALL ucrt_realloc(void* ptr, size_t size) { return realloc_guest_heap(ptr, size); }

// locale
// C locale is the only one ThetaZ provides; return real "C" values instead of
// fake-null. `_setlocale` returns the new locale name on success, or NULL if
// the requested locale is unsupported.
// Returned locale strings must also live below 4GB so a 32-bit guest can read
// them; cache each unique string in shared low memory.
static const char* shared_string(const char* s) {
    size_t len = strlen(s);
    char* p = static_cast<char*>(alloc_guest_shared(len + 1));
    if (!p) return s;
    memcpy(p, s, len + 1);
    return p;
}

static void* WINAPI_CALL ucrt__setlocale(int category, const char* locale) {
    (void)category;
    if (locale == nullptr) return const_cast<char*>(shared_string("C"));
    if (strcmp(locale, "C") == 0 || strcmp(locale, "") == 0 || strcmp(locale, "C.UTF-8") == 0) {
        return const_cast<char*>(shared_string("C"));
    }
    return nullptr;
}

static void* WINAPI_CALL ucrt_localeconv() {
    // Allocate lconv plus its string table once, in shared low memory.
    struct LconvLayout {
        struct lconv conv;
        char dot[2];
        char empty[1];
    };
    static LconvLayout* L = [] {
        LconvLayout* m = static_cast<LconvLayout*>(alloc_guest_shared(sizeof(LconvLayout)));
        memset(m, 0, sizeof(LconvLayout));
        m->dot[0] = '.';
        m->dot[1] = '\0';
        struct lconv& c = m->conv;
        c.decimal_point = m->dot;
        c.thousands_sep = m->empty;
        c.grouping = m->empty;
        c.int_curr_symbol = m->empty;
        c.currency_symbol = m->empty;
        c.mon_decimal_point = m->empty;
        c.mon_thousands_sep = m->empty;
        c.mon_grouping = m->empty;
        c.positive_sign = m->empty;
        c.negative_sign = m->empty;
        c.int_frac_digits = -1;
        c.frac_digits = -1;
        c.p_cs_precedes = -1;
        c.p_sep_by_space = -1;
        c.n_cs_precedes = -1;
        c.n_sep_by_space = -1;
        c.p_sign_posn = -1;
        c.n_sign_posn = -1;
        c.int_p_cs_precedes = -1;
        c.int_p_sep_by_space = -1;
        c.int_n_cs_precedes = -1;
        c.int_n_sep_by_space = -1;
        c.int_p_sign_posn = -1;
        c.int_n_sign_posn = -1;
        return m;
    }();
    return &L->conv;
}

static void* WINAPI_CALL ucrt__configthreadlocale(int flag) {
    (void)flag;
    // Per-thread locale is not enabled; report the process-wide default (0).
    return reinterpret_cast<void*>(0);
}

// environment
static void*** WINAPI_CALL ucrt___p__environ() {
    init_crt_shared();
    return reinterpret_cast<void***>(g_shared_env_ptr);
}

// runtime extras
static int WINAPI_CALL ucrt__set_new_mode(int mode) {
    std::lock_guard<std::mutex> lock(g_atexit_mutex);
    int previous = g_new_mode;
    g_new_mode = mode;
    return previous;
}

static void WINAPI_CALL ucrt___setusermatherr(void* handler) {
    std::lock_guard<std::mutex> lock(g_atexit_mutex);
    g_user_math_error = handler;
}

// vcruntime SEH helper: table-driven __try/__except dispatch (x86).
// The registration record pushed by MSVC codegen is { prev, __C_specific_handler,
// scope_table }, so the scope table address is at [frame + 8]; every scope
// address in the table is an RVA relative to the module ImageBase. A scope's
// filter field is either 0xFFFFFFFF (EXCEPTION_EXECUTE_HANDLER: accept
// unconditionally) or the RVA of a filter callback invoked as
// filter(EXCEPTION_POINTERS*, frame). Scopes with a zero JumpTarget are
// __finally blocks, inert during pass 1 and run during pass 2 (unwind).
// Dispatch protocol (the emulator's two-pass model, see THETAZ_RaiseException):
//   pass 1 (flags == 0): find the scope containing the fault PC; run its
//       filter; on accept write the handler's jump target into disp->TargetIp
//       and return EXCEPTION_EXECUTE_HANDLER so the dispatcher unwinds and
//       resumes at the recorded target.
//   pass 2 (flags & UNWINDING): run every __finally scope containing the PC.
static int WINAPI_CALL ucrt___C_specific_handler(void* record_, void* frame_, void* context_,
                                                 void* dispatch_) {
    constexpr uint32_t kExecHandler = 0xFFFFFFFF; // scope-table sentinel: accept unconditionally
    constexpr uint32_t kUnwindFlags = 0x6;        // EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND
    constexpr uint32_t kDispTargetIp = 0x20;      // DISPATCHER_CONTEXT.TargetIp
    constexpr uint32_t kDispImageBase = 0x08;     // DISPATCHER_CONTEXT.ImageBase
    constexpr uint32_t kDispControlPc = 0x00;     // DISPATCHER_CONTEXT.ControlPc
    constexpr uint32_t kDispHandlerData = 0x38;   // DISPATCHER_CONTEXT.HandlerData

    uint32_t record = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(record_));
    uint32_t frame = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(frame_));
    uint32_t ctx = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(context_));
    uint32_t disp = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(dispatch_));
    if (!disp || !record || !ctx) return 0; // ExceptionContinueSearch

    uint32_t flags = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(record + 4));
    uint32_t table = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(disp + kDispHandlerData));
    uint32_t base = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(disp + kDispImageBase));
    uint32_t pc = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(disp + kDispControlPc));
    if (!table || table >= 0x80000000u) return 0; // no (or bogus) scope table
    uint32_t count = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(table));
    if (!count || count > 256) return 0; // sanity

    for (uint32_t i = 0; i < count; i++) {
        const uint32_t* scope = reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(table + 4 + i * 16));
        uint32_t begin = scope[0];
        uint32_t end = scope[1];
        uint32_t handler = scope[2];
        uint32_t jump = scope[3];
        if (pc < base + begin || pc >= base + end) continue;

        if (flags & kUnwindFlags) {
            // Unwind pass: run __finally blocks (scopes without a jump target).
            if (jump) continue;
            uint32_t fargs[2] = { 1, frame };
            x86_call_guest_args(base + handler, fargs, 2); // handler(TRUE, frame)
            continue;
        }
        if (!jump) continue; // __finally scopes are inert during pass 1

        if (handler != kExecHandler) {
            // Filter callback: filter(EXCEPTION_POINTERS*, frame).
            uint32_t ptrs = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(alloc_guest_shared(8)));
            if (!ptrs) return 0;
            *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ptrs + 0)) = record;
            *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ptrs + 4)) = ctx;
            uint32_t fargs[2] = { ptrs, frame };
            int res = static_cast<int>(x86_call_guest_args(base + handler, fargs, 2));
            if (res == 0) continue;                 // EXCEPTION_CONTINUE_SEARCH
            if (res == -1) return 0xFFFFFFFF;       // EXCEPTION_CONTINUE_EXECUTION
            if (res != 1) continue;                 // unknown disposition: keep searching
        }

        // Accepted: hand the jump target to the dispatcher, ask for unwind.
        *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(disp + kDispTargetIp)) = base + jump;
        return 1; // EXCEPTION_EXECUTE_HANDLER
    }
    return 0; // ExceptionContinueSearch
}

// ---------------------------------------------------------------------------
// Classic MSVC x86 SEH: __except_handler3 / __except_handler4_common
// ---------------------------------------------------------------------------
// The frame record pushed by classic MSVC codegen (VC7+, the layout Wine uses):
//   +0x00 prev        +0x0C trylevel
//   +0x04 handler     +0x10 _ebp        (value of EBP at try time)
//   +0x08 scopetable  +0x14 xpointers
// Scope table entries are 12 bytes: { previousTryLevel@0, lpfnFilter@4,
// lpfnHandler@8 }. TRYLEVEL_END = -1 (V4: -2). A scope whose filter is NULL is
// a __finally block — its code lives in lpfnHandler and it is only invoked
// during unwinding or when a handler above it accepts. Filters receive
// EXCEPTION_POINTERS* { record@0, context@4 } built by the handler and stored
// at [frame-4] (the compiler's GetExceptionInformation local). Filters and
// __finally code run with EBP = the frame's saved EBP (frame+0x10).
// On accept the handler local-unwinds its own __finally scopes, sets
// trylevel to the enclosing scope, and resumes at lpfnHandler with EBP
// restored — expressed here as EXCEPTION_EXECUTE_HANDLER + TargetIp so the
// dispatcher performs pass 2 (unwinding of outer frames) and resumes.
static int WINAPI_CALL ucrt__except_handler3(void* record_, void* frame_, void* context_,
                                             void* dispatch_) {
    uint32_t record = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(record_));
    uint32_t frame = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(frame_));
    uint32_t ctx = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(context_));
    uint32_t disp = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(dispatch_));
    if (!record || !ctx) return 0;

    uint32_t flags = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(record + 4));
    constexpr uint32_t kUnwindFlags = 0x6;
    constexpr uint32_t kFOff = 8, kTOff = 12, kEbpoff = 16;
    constexpr uint32_t kDispTargetIp = 0x20;
    constexpr uint32_t kCtxEip = 0xB8, kCtxEbp = 0xB4;

    uint32_t scopetable = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(frame + kFOff));
    uint32_t saved_ebp = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(frame + kEbpoff));
    uint32_t trylevel = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(frame + kTOff));
    constexpr uint32_t kTryEnd = 0xFFFFFFFFu; // TRYLEVEL_END

    if (flags & kUnwindFlags) {
        // Unwind: run __finally scopes (filter == NULL) from innermost out.
        while (trylevel != kTryEnd) {
            if (trylevel >= 256) return 0;
            const uint32_t* entry =
                reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(scopetable + trylevel * 12));
            uint32_t filter = entry[1], handler = entry[2];
            trylevel = entry[0];
if (!filter && handler)
                x86_call_guest_args_ebp(handler, static_cast<uint32_t*>(nullptr), 0, saved_ebp);
            }
        return 0; // ExceptionContinueSearch
    }

    // Pass 1: hunt for a filter that accepts.
    uint32_t start = trylevel;
    while (trylevel != kTryEnd) {
        if (trylevel >= 256) return 0;
        const uint32_t* entry =
            reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(scopetable + trylevel * 12));
        uint32_t prev = entry[0], filter = entry[1], handler = entry[2];

        if (filter) {
            uint32_t ptrs = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(alloc_guest_shared(8)));
            if (!ptrs) return 0;
            *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ptrs + 0)) = record;
            *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ptrs + 4)) = ctx;
            *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(frame - 4)) = ptrs; // GetExceptionInformation local
            int res = static_cast<int>(x86_call_guest_args_ebp(filter, &ptrs, 1, saved_ebp));
            if (res == -1) return 0xFFFFFFFF;      // EXCEPTION_CONTINUE_EXECUTION
            if (res == 1) {                        // EXCEPTION_EXECUTE_HANDLER
                // Local unwind: run this frame's __finally scopes down to the
                // accepted level (exclusive), exactly like msvcrt_local_unwind2.
                uint32_t lvl = start;
                while (lvl != trylevel && lvl != kTryEnd) {
                    const uint32_t* e =
                        reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(scopetable + lvl * 12));
                    uint32_t lprev = e[0], lfilter = e[1], lhandler = e[2];
                    lvl = lprev;
if (!lfilter && lhandler)
                x86_call_guest_args_ebp(lhandler, static_cast<uint32_t*>(nullptr), 0, saved_ebp);
            }
                // trylevel = enclosing scope; resume at the __except body with
                // EBP restored. The dispatcher reads TargetIp + ctx->Ebp.
                *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(frame + kTOff)) = prev;
                if (disp) *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(disp + kDispTargetIp)) = handler;
                *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ctx + kCtxEip)) = handler;
                *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ctx + kCtxEbp)) = saved_ebp;
                return 1; // EXCEPTION_EXECUTE_HANDLER
            }
        }
        trylevel = prev;
    }
    return 0; // ExceptionContinueSearch
}

// /GS variant: the scope table is encoded with the security cookie and uses
// -2 as the trylevel terminator. SCOPETABLE_V4 = { gs_cookie_offset,
// gs_cookie_xor, eh_cookie_offset, eh_cookie_xor, entries[] } where entries
// have the same 12-byte { previousTryLevel, lpfnFilter, lpfnHandler } layout.
// (Cookie validation is skipped, mirroring the reference implementation.)
static int WINAPI_CALL ucrt__except_handler4_common(void* cookie_, void* check_cookie_,
                                                    void* record_, void* frame_, void* context_,
                                                    void* dispatch_) {
    (void)check_cookie_;
    uint32_t cookie = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(cookie_));
    uint32_t record = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(record_));
    uint32_t frame = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(frame_));
    uint32_t ctx = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(context_));
    uint32_t disp = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(dispatch_));
    if (!record || !ctx) return 0;

    uint32_t flags = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(record + 4));
    constexpr uint32_t kUnwindFlags = 0x6;
    constexpr uint32_t kFOff = 8, kTOff = 12, kEbpoff = 16;
    constexpr uint32_t kDispTargetIp = 0x20;
    constexpr uint32_t kCtxEip = 0xB8, kCtxEbp = 0xB4;
    constexpr uint32_t kTryEnd = 0xFFFFFFFEu; // -2

    uint32_t raw_table = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(frame + kFOff));
    uint32_t scopetable = raw_table ^ cookie;
    uint32_t saved_ebp = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(frame + kEbpoff));
    uint32_t trylevel = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(frame + kTOff));
    if (!scopetable || scopetable >= 0x80000000u) return 0;

    if (flags & kUnwindFlags) {
        while (trylevel != kTryEnd) {
            if (trylevel >= 256) return 0;
            const uint32_t* entry =
                reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(scopetable + 16 + trylevel * 12));
            uint32_t filter = entry[1], handler = entry[2];
            trylevel = entry[0];
            if (!filter && handler)
                x86_call_guest_args_ebp(handler, static_cast<uint32_t*>(nullptr), 0, saved_ebp);
        }
        return 0;
    }

    uint32_t start = trylevel;
    while (trylevel != kTryEnd) {
        if (trylevel >= 256) return 0;
        const uint32_t* entry =
            reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(scopetable + 16 + trylevel * 12));
        uint32_t prev = entry[0], filter = entry[1], handler = entry[2];

        if (filter) {
            uint32_t ptrs = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(alloc_guest_shared(8)));
            if (!ptrs) return 0;
            *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ptrs + 0)) = record;
            *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ptrs + 4)) = ctx;
            *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(frame - 4)) = ptrs;
            int res = static_cast<int>(x86_call_guest_args_ebp(filter, &ptrs, 1, saved_ebp));
            if (res == -1) return 0xFFFFFFFF;
            if (res == 1) {
                uint32_t lvl = start;
                while (lvl != trylevel && lvl != kTryEnd) {
                    const uint32_t* e =
                        reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(scopetable + 16 + lvl * 12));
                    uint32_t lprev = e[0], lfilter = e[1], lhandler = e[2];
                    lvl = lprev;
                    if (!lfilter && lhandler)
                        x86_call_guest_args_ebp(lhandler, static_cast<uint32_t*>(nullptr), 0, saved_ebp);
                }
                *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(frame + kTOff)) = prev;
                if (disp) *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(disp + kDispTargetIp)) = handler;
                *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ctx + kCtxEip)) = handler;
                *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ctx + kCtxEbp)) = saved_ebp;
                return 1;
            }
        }
        trylevel = prev;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// MSVC C++ exceptions: __CxxFrameHandler / __CxxFrameHandler3 (x86 ABI).
// ---------------------------------------------------------------------------
// Public MSVC ABI (Microsoft's ehhdr.h layouts), implemented from scratch:
//
//   EH_EXCEPTION_NUMBER = 0xE06D7363. RaiseException params[0..3] =
//   { magic, pExceptionObject, pThrowInfo, 0 } with magic 0x19930520/21/22
//   (V2/V3/V4). The registration record pushed by MSVC x86 codegen is:
//     EH3: { prev@0, handler@4, ebp@8, pFuncInfo@0xC, tryLevel@0x10, xp@0x14 }
//     EH4: { prev@0, handler@4, ebp@8, cookie@0xC, pFuncInfo@0x10,
//            tryLevel@0x14, xp@0x18 }                    (with /GS)
//   which layout is in use is discovered by probing pFuncInfo's magic.
//   FuncInfo (all pointers absolute after relocation):
//     V2:  { magic, maxState, pUnwindMap, nTryBlocks, pTryBlockMap,
//            nIPMapEntries, pIPToStateMap, nCatchHandlers, pCatchHandlerArray }
//     V3/V4: same + { flags, pUnwindMapForUnexpected }
//   UnwindMapEntry[i] = { toState, action }      (action = unwind funclet or 0)
//   TryBlockMapEntry  = { tryLow, tryHigh, catchHigh, nCatches, pHandlerArray }
//   CatchHandlerType  = { adjectives, pType, handler }
//     (adjectives & 4 = catch by reference; handler = catch funclet address)
//   IPToStateMapEntry = { ip, state }
//   TypeDescriptor    = { pVFTable, spare, name[] }  (mangled ".?AVName@@")
//   ThrowInfo         = { attributes, pmfnUnwind, pForwardCompat,
//                         pCatchableTypeArray }
//   CatchableTypeArray= { n, pCatchableTypes[] }     (pointers)
//   CatchableType     = { properties, pType, PMD{mdisp,pdisp,vdisp},
//                         sizeOrOffset, copyFunction }
//   PMD: non-virtual bases have pdisp == -1 and the subobject sits at
//   object + mdisp; virtual bases are vbtable-indirected.
//
// Dispatch model (the emulator's two-pass RaiseException):
//   pass 1 (flags==0): match the thrown type against the catch descriptors of
//       the innermost containing try block; on match, run this frame's own
//       unwind funclets for the scopes between the fault state and the catch
//       state, update tryLevel to catchHigh, write the resume context
//       (Eip = funclet, Ebp = saved ebp, Esp = record top, Edx = exception
//       object) and return EXCEPTION_EXECUTE_HANDLER (1).
//   pass 2 (flags & 6): run this frame's active unwind funclets (destructor
//       cleanups), tryLevel down to state 0.
//
// Funclets are frameless: entered with EBP = the establisher's saved EBP.
// Catch funclets receive the exception object in EDX and END WITH A JUMP to
// the function's continuation (no return address — entered via context
// restore, so the resume path is used). Unwind funclets are plain calls.
static uint32_t cxx_read32(uint32_t addr) {
    return *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(addr));
}

// Runs the unwind funclets for the states in (toState, fromState].
static void cxx_unwind_frame(uint32_t pFuncInfo, uint32_t from_state, uint32_t to_state, uint32_t ebp) {
    if (!pFuncInfo) return;
    uint32_t max_state = cxx_read32(pFuncInfo + 4);
    uint32_t p_map = cxx_read32(pFuncInfo + 8);
    if (!p_map || max_state > 4096) return;
    uint32_t state = from_state;
    uint32_t guard = 0;
    while (state > to_state && guard++ < 4096) {
        if (state > max_state) break;
        uint32_t action = cxx_read32(p_map + state * 8 + 4);
        if (action && action < 0x80000000u) {
            // Frameless destructor funclet; returns to the caller.
            x86_call_guest_args_ebp(action, static_cast<uint32_t*>(nullptr), 0, ebp);
        }
        uint32_t next = cxx_read32(p_map + state * 8);
        if (next >= state) break; // no progress: stop to avoid a loop
        state = next;
    }
}

// Returns the FuncInfo state for the given IP, or 0xFFFFFFFF when unknown.
// The map is sorted by ip; the state of the entry with the largest ip <= pc
// (last entry before the next range) is the one.
static uint32_t cxx_ip_to_state(uint32_t pFuncInfo, uint32_t pc) {
    uint32_t n = cxx_read32(pFuncInfo + 20);           // nIPMapEntries
    uint32_t p_map = cxx_read32(pFuncInfo + 24);       // pIPToStateMap
    if (!p_map || n > 4096) return 0xFFFFFFFF;
    uint32_t best = 0xFFFFFFFF;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t ip = cxx_read32(p_map + i * 8);
        if (ip <= pc) best = i;
        else break; // sorted: no later entry can match
    }
    return best == 0xFFFFFFFF ? best : cxx_read32(p_map + best * 8 + 4);
}

// Type match: the catch type name must equal the mangled name of the thrown
// type or one of its base classes (each CatchableType in the throw's array).
// On match, *pdisp_out receives the PMD displacement applied by the caller.
static bool cxx_type_match(uint32_t p_catch_type, uint32_t p_throw_info,
                           uint32_t p_object, int32_t* pdisp_out) {
    *pdisp_out = 0;
    if (!p_catch_type || !p_throw_info) return false;
    const char* catch_name =
        reinterpret_cast<const char*>(static_cast<uintptr_t>(p_catch_type + 8));
    if (!catch_name || !catch_name[0]) return false;
    uint32_t p_cta = cxx_read32(p_throw_info + 12);
    if (!p_cta) return false;
    int32_t n = static_cast<int32_t>(cxx_read32(p_cta));
    if (n <= 0 || n > 64) return false;
    for (int32_t i = 0; i < n; i++) {
        uint32_t p_ct = cxx_read32(p_cta + 4 + i * 4); // CatchableType* slot
        if (!p_ct || p_ct < 0x400000u) continue;
        uint32_t p_type = cxx_read32(p_ct + 4);
        if (!p_type) continue;
        const char* tname =
            reinterpret_cast<const char*>(static_cast<uintptr_t>(p_type + 8));
        if (!tname) continue;
        if (strcmp(tname, catch_name) != 0) continue;
        // Matched. Compute the subobject displacement (PMD at p_ct + 8).
        int32_t mdisp = static_cast<int32_t>(cxx_read32(p_ct + 8));
        int32_t pdisp = static_cast<int32_t>(cxx_read32(p_ct + 12));
        int32_t vdisp = static_cast<int32_t>(cxx_read32(p_ct + 16));
        if (pdisp == -1 || !p_object) {
            *pdisp_out = mdisp; // non-virtual base (or primary type)
        } else {
            // Virtual base: subobject = base + mdisp + vbtable[vdisp]
            uint32_t vb_ptr = p_object + pdisp;
            if (vb_ptr < 0x400000u) return false;
            uint32_t vbtable = cxx_read32(vb_ptr);
            if (!vbtable || vbtable < 0x400000u) return false;
            int32_t off = static_cast<int32_t>(cxx_read32(vbtable + vdisp));
            *pdisp_out = mdisp + off;
        }
        return true;
    }
    return false;
}

// int __cdecl __CxxFrameHandler(EXCEPTION_RECORD*, EXCEPTION_REGISTRATION*,
//                               CONTEXT*, DISPATCHER_CONTEXT*)
static int WINAPI_CALL ucrt___CxxFrameHandler(void* record_, void* frame_, void* context_,
                                              void* dispatch_) {
    constexpr uint32_t kEhException = 0xE06D7363;
    constexpr uint32_t kV2 = 0x19930520, kV3 = 0x19930521, kV4 = 0x19930522;
    constexpr uint32_t kUnwindFlags = 0x6; // EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND
    constexpr uint32_t kCtxEip = 0xB8, kCtxEbp = 0xB4, kCtxEdx = 0xA8, kCtxEsp = 0xC4;

    uint32_t record = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(record_));
    uint32_t frame = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(frame_));
    uint32_t ctx = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(context_));
    uint32_t disp = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(dispatch_));
    if (!record || !ctx) return 0; // ExceptionContinueSearch

    // Only C++ exceptions with a recognized magic reach here.
    if (cxx_read32(record) != kEhException) return 0;
    uint32_t nargs = cxx_read32(record + 16);
    if (nargs < 3) return 0;
    uint32_t magic = cxx_read32(record + 20);
    if (magic != kV2 && magic != kV3 && magic != kV4) return 0;
    uint32_t p_object = cxx_read32(record + 24);
    uint32_t p_throw_info = cxx_read32(record + 28);
    if (!p_throw_info || p_throw_info < 0x400000u) return 0;

    // The record layout (EH3 vs EH4) is implied by which slot holds a
    // plausible FuncInfo pointer: probe +0xC first, then +0x10.
    uint32_t p_func_info = 0;
    uint32_t try_level_off = 0;
    for (int pass = 0; pass < 2; pass++) {
        uint32_t probe = cxx_read32(frame + (pass == 0 ? 0x0C : 0x10));
        if (probe && probe >= 0x400000u) {
            uint32_t fm = cxx_read32(probe);
            if (fm == kV2 || fm == kV3 || fm == kV4) {
                p_func_info = probe;
                try_level_off = (pass == 0 ? 0x10 : 0x14);
                break;
            }
        }
    }
    if (!p_func_info) return 0;

    uint32_t flags = cxx_read32(record + 4);
    uint32_t ebp = cxx_read32(frame + 8);

    if (flags & kUnwindFlags) {
        // Pass 2: run this frame's destructor funclets (states down to 0).
        uint32_t try_level = cxx_read32(frame + try_level_off);
        if (try_level != 0xFFFFFFFFu) cxx_unwind_frame(p_func_info, try_level, 0, ebp);
        return 0; // ExceptionContinueSearch
    }

    // Pass 1: state of the fault PC, then innermost-first try blocks.
    uint32_t state = cxx_ip_to_state(p_func_info, cxx_read32(ctx + kCtxEip));
    if (state == 0xFFFFFFFFu) return 0;
    uint32_t n_try = cxx_read32(p_func_info + 12);     // nTryBlocks
    uint32_t p_try_map = cxx_read32(p_func_info + 16); // pTryBlockMap
    if (!p_try_map || n_try > 256) return 0;

    for (int32_t i = static_cast<int32_t>(n_try) - 1; i >= 0; i--) {
        uint32_t tb = p_try_map + i * 20;
        int32_t try_low = static_cast<int32_t>(cxx_read32(tb));
        int32_t try_high = static_cast<int32_t>(cxx_read32(tb + 4));
        if (state < static_cast<uint32_t>(try_low) ||
            state >= static_cast<uint32_t>(try_high))
            continue;
        uint32_t n_catches = cxx_read32(tb + 12);
        uint32_t p_catches = cxx_read32(tb + 16);
        if (!p_catches || n_catches > 256) continue;
        for (uint32_t j = 0; j < n_catches; j++) {
            uint32_t ch = p_catches + j * 12;
            uint32_t p_catch_type = cxx_read32(ch + 4);
            int32_t sub_disp = 0;
            if (!cxx_type_match(p_catch_type, p_throw_info, p_object, &sub_disp))
                continue;
            int32_t catch_high = static_cast<int32_t>(cxx_read32(tb + 8));
            // Run our own destructor funclets for the scopes being abandoned.
            if (state > static_cast<uint32_t>(catch_high))
                cxx_unwind_frame(p_func_info, state, static_cast<uint32_t>(catch_high), ebp);
            // tryLevel = the scope outside the try (catchHigh).
            *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(frame + try_level_off)) =
                static_cast<uint32_t>(catch_high);
            // Resume in the catch funclet: frameless, EBP = saved ebp, the
            // exception object (subobject-adjusted) in EDX.
            uint32_t funclet = cxx_read32(ch + 8);
            if (!funclet || funclet < 0x400000u) return 0;
            *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ctx + kCtxEip)) = funclet;
            *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ctx + kCtxEbp)) = ebp;
            *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ctx + kCtxEsp)) = frame;
            *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(ctx + kCtxEdx)) =
                p_object + static_cast<uint32_t>(sub_disp);
            (void)disp;
            return 1; // EXCEPTION_EXECUTE_HANDLER
        }
    }
    return 0; // ExceptionContinueSearch
}

// void __cdecl _CxxThrowException(void* pExceptionObject, _ThrowInfo* pThrowInfo)
static void WINAPI_CALL ucrt__CxxThrowException(uint32_t p_exception_object, uint32_t p_throw_info) {
    void WINAPI_CALL THETAZ_RaiseException(DWORD dwExceptionCode, DWORD dwExceptionFlags,
                                           DWORD nNumberOfArguments, void* lpArguments);
    uint32_t args[4];
    args[0] = 0x19930522; // V4 magic (any accepted by __CxxFrameHandler)
    args[1] = p_exception_object;
    args[2] = p_throw_info;
    args[3] = 0;
    THETAZ_RaiseException(0xE06D7363, 1 /* EXCEPTION_NONCONTINUABLE */, 4, args);
}

// CRT local-unwind helpers used by longjmp through SEH frames.
static void WINAPI_CALL ucrt__local_unwind2(void* frame_, int trylevel) {
    uint32_t frame = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(frame_));
    uint32_t scopetable = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(frame + 8));
    uint32_t saved_ebp = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(frame + 16));
    uint32_t lvl = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(frame + 12));
    if (!scopetable || scopetable >= 0x80000000u) return;
    while (lvl != 0xFFFFFFFFu && lvl != static_cast<uint32_t>(trylevel)) {
        const uint32_t* e =
            reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(scopetable + lvl * 12));
        uint32_t filter = e[1], handler = e[2];
        lvl = e[0];
        if (!filter && handler)
            x86_call_guest_args_ebp(handler, static_cast<uint32_t*>(nullptr), 0, saved_ebp);
    }
}

static void WINAPI_CALL ucrt__local_unwind4(void* cookie_, void* frame_, int trylevel) {
    uint32_t cookie = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(cookie_));
    uint32_t frame = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(frame_));
    uint32_t scopetable = (*reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(frame + 8))) ^ cookie;
    uint32_t saved_ebp = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(frame + 16));
    uint32_t lvl = *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(frame + 12));
    if (!scopetable || scopetable >= 0x80000000u) return;
    while (lvl != 0xFFFFFFFEu && lvl != static_cast<uint32_t>(trylevel)) {
        const uint32_t* e =
            reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(scopetable + 16 + lvl * 12));
        uint32_t filter = e[1], handler = e[2];
        lvl = e[0];
        if (!filter && handler)
            x86_call_guest_args_ebp(handler, static_cast<uint32_t*>(nullptr), 0, saved_ebp);
    }
}

// ---------------------------------------------------------------------------
// Extended runtime surface (mostly exercised by PE32+ guests, but registered
// for 32-bit guests too). These are the functions WINWORD.EXE's CRT startup
// needs; several are trivial stubs that can be hardened as the 64-bit app
// surface grows.
// ---------------------------------------------------------------------------

namespace {

int g_fmode = 0; // default _O_TEXT

// MSVC CRT onexit table: { first, last, end } of a growable callback array.
struct OnexitTable {
    void** first;
    void** last;
    void** end;
};

// vcruntime _exception: { _What, _DoFree }.
struct StdException {
    const char* what;
    bool dofree;
};

// Thread-local "current exception" state (vcruntime __current_exception*).
thread_local void* g_current_exception = nullptr;
thread_local void* g_current_exception_context = nullptr;

// Thread-local exe atexit callbacks (registered, never run — MSVC runs them
// at thread teardown, which we approximate by doing nothing).
std::vector<void*>& g_tls_atexit_callbacks() {
    static std::vector<void*> v;
    return v;
}

// The C locale object __initialize_lconv_for_unsigned_char hands out. The
// Windows __crt_locale_data_public starts with { _locale, _lconv, _ctype,
// _pclmap, _pcumap } — msvcp140 reads _lconv->decimal_point etc.
struct LconvC {
    const char* decimal_point;
    const char* thousands_sep;
    const char* grouping;
    const char* int_curr_symbol;
    const char* currency_symbol;
    const char* mon_decimal_point;
    const char* mon_thousands_sep;
    const char* mon_grouping;
    const char* positive_sign;
    const char* negative_sign;
    char digits[14]; // int_frac_digits .. int_n_sign_posn (back-to-back chars)
};

struct UcrtLocaleData {
    void* locale;
    LconvC* lconv;
    const void* ctype;
    const void* pclmap;
    const void* pcumap;
};

LconvC g_c_lconv = {
    ".", "", "", "", "", "", "", "", "", "", {0},
};

UcrtLocaleData g_c_locale = {&g_c_locale, &g_c_lconv, nullptr, nullptr, nullptr};

} // namespace

// _set_fmode: switch default text/binary translation; returns the old mode.
static int WINAPI_CALL ucrt__set_fmode(int mode) {
    int old = g_fmode;
    g_fmode = mode;
    return old;
}

// _seh_filter_exe: CRT wrapper around the exception filter for the main-exe
// path. Returning 1 (= EXCEPTION_EXECUTE_HANDLER) lets the CRT terminate
// normally instead of re-raising into a debugger.
static long WINAPI_CALL ucrt__seh_filter_exe(unsigned long code, void* exptrs) {
    (void)code;
    (void)exptrs;
    return 1;
}

// _invoke_watson: the CRT fail-fast path (invalid parameter / C runtime
// error). Report and die — real behavior raises 0xC0000417 and hits the app's
// SEH, which a PE32+ guest can't process yet.
static void WINAPI_CALL ucrt__invoke_watson(const wchar_t* expr, const wchar_t* func,
                                            const wchar_t* file, unsigned int line,
                                            uintptr_t reserved) {
    (void)expr; (void)func; (void)file; (void)line; (void)reserved;
    fprintf(stderr, "[ucrt] _invoke_watson: fatal C runtime error\n");
    abort();
}

// _initialize_onexit_table: zero the {first,last,end} table. Returns TRUE —
// __scrt_initialize_crt fails the whole CRT startup if this is falsy.
static int WINAPI_CALL ucrt__initialize_onexit_table(void* table) {
    OnexitTable* t = static_cast<OnexitTable*>(table);
    t->first = nullptr;
    t->last = nullptr;
    t->end = nullptr;
    return 1;
}

// _register_onexit_function: append fn to the growable callback table.
static int WINAPI_CALL ucrt__register_onexit_function(void* table, void* fn) {
    OnexitTable* t = static_cast<OnexitTable*>(table);
    if (t->last >= t->end) {
        size_t cur = static_cast<size_t>(t->end - t->first);
        size_t cap = cur ? cur * 2 : 32;
        void** nb = static_cast<void**>(realloc(t->first, cap * sizeof(void*)));
        if (!nb) return -1;
        t->first = nb;
        t->last = nb + cur;
        t->end = nb + cap;
    }
    *t->last++ = fn;
    return 0;
}

// _c_exit: flush buffers and terminate without running atexit handlers.
static void WINAPI_CALL ucrt__c_exit() {
    fflush(nullptr);
    _Exit(0);
}

// _register_thread_local_exe_atexit_callback: stash the callback.
static void WINAPI_CALL ucrt__register_thread_local_exe_atexit_callback(void (*cb)(void)) {
    g_tls_atexit_callbacks().push_back(reinterpret_cast<void*>(cb));
}

// _get_narrow_winmain_command_line: ANSI command line for winmain CRTs.
static const char* WINAPI_CALL ucrt__get_narrow_winmain_command_line() {
    if (g_shared_acmdln && *g_shared_acmdln) return *g_shared_acmdln;
    return "";
}

// terminate: C++ termination path.
static void WINAPI_CALL ucrt_terminate() {
    fprintf(stderr, "[ucrt] terminate called\n");
    abort();
}

// __initialize_lconv_for_unsigned_char: CRT initializer (__xi table entry)
// that bumps the lconv char fields to UCHAR_MAX for -J (unsigned char) code.
// It is an *initializer*: returns 0 on success. The CRT's __scrt_common_main_seh
// fails the whole startup if _initterm_e sees a nonzero result, so returning a
// pointer here would abort boot with exit code 0xFF.
static int WINAPI_CALL ucrt___initialize_lconv_for_unsigned_char() {
    return 0;
}

// strncpy_s: bounded copy, always null-terminates, ERANGE on truncation.
static int WINAPI_CALL ucrt_strncpy_s(char* dst, size_t dsz, const char* src, size_t count) {
    if (!dst || dsz == 0 || !src) {
        if (dst && dsz) dst[0] = 0;
        return EINVAL;
    }
    size_t slen = strnlen(src, count);
    if (slen + 1 > dsz) {
        dst[0] = 0;
        return ERANGE;
    }
    memcpy(dst, src, slen);
    dst[slen] = 0;
    return 0;
}

// _stricmp: case-insensitive compare.
static int WINAPI_CALL ucrt__stricmp(const char* a, const char* b) {
    if (!a || !b) return 0;
    // Check for obviously invalid pointers (high region not mapped)
    uintptr_t ua = reinterpret_cast<uintptr_t>(a);
    uintptr_t ub = reinterpret_cast<uintptr_t>(b);
    if (ua >= 0x8000000000000000ULL || ub >= 0x8000000000000000ULL) {
        return 0;
    }
    return strcasecmp(a, b);
}

// wcsncat_s: bounded wide concat.
static int WINAPI_CALL ucrt_wcsncat_s(wchar_t* dst, size_t dsz, const wchar_t* src, size_t count) {
    if (!dst || dsz == 0 || !src) {
        if (dst && dsz) dst[0] = 0;
        return EINVAL;
    }
    size_t dlen = wcslen(dst);
    size_t slen = wcsnlen(src, count);
    if (dlen + slen + 1 > dsz) {
        dst[0] = 0;
        return ERANGE;
    }
    wcsncpy(dst + dlen, src, slen);
    dst[dlen + slen] = 0;
    return 0;
}

// Write an already-formatted narrow result into a guest buffer. `secure`
// implements the *_s contract: truncation is an error and clears the buffer.
// `maxchars` bounds the payload when the API has a separate count parameter.
static int write_narrow_result(char* buf, size_t bufsz, const std::string& text,
                               bool secure, size_t maxchars = SIZE_MAX) {
    size_t room = bufsz ? bufsz - 1 : 0;
    bool too_long = text.size() > room ||
                    (maxchars != SIZE_MAX && text.size() > maxchars);
    if (secure && too_long) {
        if (buf && bufsz) buf[0] = 0;
        return -1;
    }
    size_t n = text.size();
    if (n > room) n = room;
    if (maxchars != SIZE_MAX && n > maxchars) n = maxchars;
    if (buf && bufsz) {
        memcpy(buf, text.data(), n);
        buf[n] = 0;
    }
    return static_cast<int>(text.size());
}

static int write_wide_result(uint16_t* buf, size_t bufsz, const std::string& text,
                             bool secure, size_t maxchars = SIZE_MAX) {
    std::u16string u16 = utf8_to_utf16_string(text);
    size_t room = bufsz ? bufsz - 1 : 0;
    bool too_long = u16.size() > room ||
                    (maxchars != SIZE_MAX && u16.size() > maxchars);
    if (secure && too_long) {
        if (buf && bufsz) buf[0] = 0;
        return -1;
    }
    size_t n = u16.size();
    if (n > room) n = room;
    if (maxchars != SIZE_MAX && n > maxchars) n = maxchars;
    if (buf && bufsz) {
        memcpy(buf, u16.data(), n * sizeof(uint16_t));
        buf[n] = 0;
    }
    return static_cast<int>(u16.size());
}

static std::string format_guest_wide(const uint16_t* fmt, VarArgs& va) {
    std::string fmt8 = utf16_to_utf8_string(fmt);
    return format_string(fmt8.c_str(), va, /*wide_strings=*/true);
}

// __stdio_common_vsnwprintf_s / __stdio_common_vsnprintf_s and their
// unbounded counterparts. The guest's va_list is a pointer to a run of 4-byte
// (x86) or 8-byte (x64 Windows ABI) argument slots, never a host SysV
// va_list, so formatting goes through VarArgs rather than host vsnprintf.
static int WINAPI_CALL ucrt___stdio_common_vsnwprintf_s(
    uint64_t options, uint16_t* buf, size_t bufsz, size_t maxcount,
    const uint16_t* fmt, void* locale, void* va_ptr) {
    (void)options;
    (void)locale;
    if (!fmt) return -1;
    VarArgs va{static_cast<uint8_t*>(va_ptr)};
    return write_wide_result(buf, bufsz, format_guest_wide(fmt, va), true, maxcount);
}

static int WINAPI_CALL ucrt___stdio_common_vsnprintf_s(
    uint64_t options, char* buf, size_t bufsz, size_t maxcount,
    const char* fmt, void* locale, void* va_ptr) {
    (void)options;
    (void)locale;
    if (!fmt) return -1;
    VarArgs va{static_cast<uint8_t*>(va_ptr)};
    return write_narrow_result(buf, bufsz, format_string(fmt, va), true, maxcount);
}

static int WINAPI_CALL ucrt___stdio_common_vswprintf_s(
    uint64_t options, uint16_t* buf, size_t bufsz,
    const uint16_t* fmt, void* locale, void* va_ptr) {
    (void)options;
    (void)locale;
    if (!fmt) return -1;
    VarArgs va{static_cast<uint8_t*>(va_ptr)};
    return write_wide_result(buf, bufsz, format_guest_wide(fmt, va), true);
}

static int WINAPI_CALL ucrt___stdio_common_vsprintf_s(
    uint64_t options, char* buf, size_t bufsz,
    const char* fmt, void* locale, void* va_ptr) {
    (void)options;
    (void)locale;
    if (!fmt) return -1;
    VarArgs va{static_cast<uint8_t*>(va_ptr)};
    return write_narrow_result(buf, bufsz, format_string(fmt, va), true);
}

static int WINAPI_CALL ucrt___stdio_common_vswprintf(
    uint64_t options, uint16_t* buf, size_t bufsz,
    const uint16_t* fmt, void* locale, void* va_ptr) {
    (void)options;
    (void)locale;
    if (!fmt) return -1;
    VarArgs va{static_cast<uint8_t*>(va_ptr)};
    return write_wide_result(buf, bufsz, format_guest_wide(fmt, va), false);
}

static int WINAPI_CALL ucrt___stdio_common_vsprintf(
    uint64_t options, char* buf, size_t bufsz,
    const char* fmt, void* locale, void* va_ptr) {
    (void)options;
    (void)locale;
    if (!fmt) return -1;
    VarArgs va{static_cast<uint8_t*>(va_ptr)};
    return write_narrow_result(buf, bufsz, format_string(fmt, va), false);
}

// vcruntime __current_exception / __current_exception_context.
static void* WINAPI_CALL ucrt___current_exception() {
    return g_current_exception;
}

static void* WINAPI_CALL ucrt___current_exception_context() {
    return g_current_exception_context;
}

// vcruntime __std_exception_copy / __std_exception_destroy.
static void WINAPI_CALL ucrt___std_exception_copy(void* src, void* dst) {
    StdException* s = static_cast<StdException*>(src);
    StdException* d = static_cast<StdException*>(dst);
    if (s->what) {
        d->what = strdup(s->what);
        d->dofree = d->what != nullptr;
    } else {
        d->what = nullptr;
        d->dofree = false;
    }
}

static void WINAPI_CALL ucrt___std_exception_destroy(void* e) {
    StdException* s = static_cast<StdException*>(e);
    if (s->dofree && s->what) free(const_cast<char*>(s->what));
    s->what = nullptr;
    s->dofree = false;
}

// vcruntime __std_terminate.
static void WINAPI_CALL ucrt___std_terminate() {
    fprintf(stderr, "[ucrt] __std_terminate called\n");
    abort();
}

// msvcp140: performance counters (first msvcp140 entries).
// NO-ARG, return int64 in RAX (msvcp140.spec: @ cdecl -ret64 _Query_perf_counter()).
// The guest calls these without setting rcx; a pointer-taking version reads
// garbage (e.g. rcx=0x20) and SIGSEGVs on *lp.
static int64_t WINAPI_CALL msvcp140_Query_perf_counter() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (static_cast<int64_t>(ts.tv_sec) * 10000000LL) + (ts.tv_nsec / 100);
}

static int64_t WINAPI_CALL msvcp140_Query_perf_frequency() {
    return 10000000LL;  // 10 MHz, like a typical Windows QPC
}

// msvcp140: allocation/length failure throws — no 64-bit guest EH unwinding
// yet, so report and abort.
static void WINAPI_CALL msvcp140_Xbad_alloc() {
    fprintf(stderr, "[msvcp140] std::bad_alloc\n");
    abort();
}

static void WINAPI_CALL msvcp140_Xlength_error(const char* msg) {
    fprintf(stderr, "[msvcp140] std::length_error: %s\n", msg ? msg : "");
    abort();
}

// vcruntime140_1 __CxxFrameHandler4 (x64 C++ EH personality): no 64-bit guest
// unwinding yet; CONTINUE_SEARCH keeps non-exception paths working.
static long WINAPI_CALL vcruntime140_1___CxxFrameHandler4(void* pExcept, void* pfn,
                                                        void* pContext, void* pDispatcher) {
    (void)pExcept; (void)pfn; (void)pContext; (void)pDispatcher;
    return 0;
}

// Missing C runtime functions
static char* WINAPI_CALL ucrt__strdup(const char* str) {
    if (!str) return nullptr;
    size_t len = strlen(str);
    char* dup = static_cast<char*>(malloc(len + 1));
    if (dup) memcpy(dup, str, len + 1);
    return dup;
}

static char* WINAPI_CALL ucrt_getenv(const char* name) {
    return getenv(name);
}

static char* WINAPI_CALL ucrt_strchr(const char* str, int c) {
    return const_cast<char*>(strchr(str, c));
}

static size_t WINAPI_CALL ucrt_strcspn(const char* str1, const char* str2) {
    return strcspn(str1, str2);
}

static wchar_t* WINAPI_CALL ucrt_wcsrchr(const wchar_t* str, wchar_t c) {
    return const_cast<wchar_t*>(wcsrchr(str, c));
}

static int WINAPI_CALL ucrt_wcscmp(const wchar_t* a, const wchar_t* b) {
    if (!a || !b) return 0;
    return wcscmp(a, b);
}

static wchar_t* WINAPI_CALL ucrt_wcsstr(const wchar_t* haystack, const wchar_t* needle) {
    return const_cast<wchar_t*>(wcsstr(haystack, needle));
}

static int WINAPI_CALL ucrt__assert(int expr, const char* filename, unsigned line) {
    if (!expr) {
        fprintf(stderr, "Assertion failed: %s, file %s, line %u\n", "expr", filename ? filename : "", line);
        abort();
    }
    return 0;
}

static int WINAPI_CALL ucrt__wcsnicmp(const wchar_t* a, const wchar_t* b, size_t count) {
    if (!a || !b) return 0;
    return wcsncasecmp(a, b, count);
}

static int WINAPI_CALL ucrt__wcsicmp(const wchar_t* a, const wchar_t* b) {
    return ucrt_wcscmp(a, b);
}

static int WINAPI_CALL ucrt__vsnprintf(char* buf, size_t count, const char* fmt, void* va_ptr) {
    if (!fmt) return -1;
    if (!buf || count == 0) return 0;
    VarArgs va{static_cast<uint8_t*>(va_ptr)};
    return write_narrow_result(buf, count, format_string(fmt, va), false);
}

static void WINAPI_CALL ucrt_type_info_dtor(void* e) {
    (void)e;
}

static const char* WINAPI_CALL ucrt_exception_what(void* e) {
    StdException* s = static_cast<StdException*>(e);
    return s ? s->what : "";
}

static void WINAPI_CALL ucrt_exception_ctor(void* e, const char* msg, int flag) {
    (void)e; (void)msg; (void)flag;
}

static void WINAPI_CALL ucrt_exception_dtor(void* e) {
    (void)e;
}

static errno_t WINAPI_CALL ucrt_memcpy_s(void* dest, size_t destsz, const void* src, size_t count) {
    if (!dest || !src || destsz < count) return EINVAL;
    memcpy(dest, src, count);
    return 0;
}

static int WINAPI_CALL ucrt___uncaught_exception() { return 0; }
static int WINAPI_CALL ucrt___uncaught_exceptions() { return 0; }

static int WINAPI_CALL ucrt___lc_collate_cp_func() { return 0; }
static const char* WINAPI_CALL ucrt___lc_locale_name_func() { return "C"; }
static int WINAPI_CALL ucrt___mb_cur_max_func() { return 1; }

static void* WINAPI_CALL ucrt_memchr(const void* s, int c, size_t n) { return const_cast<void*>(memchr(s, c, n)); }

static size_t WINAPI_CALL ucrt_wcslen(const wchar_t* s) { return s ? ::wcslen(s) : 0; }

static size_t WINAPI_CALL ucrt_strnlen(const char* s, size_t maxlen) { return s ? strnlen(s, maxlen) : 0; }

static errno_t WINAPI_CALL ucrt__ui64tow_s(unsigned long long value, wchar_t* buffer, size_t sizeInWords, int radix) {
    if (!buffer || sizeInWords == 0) return EINVAL;
    if (radix < 2 || radix > 36) { buffer[0] = 0; return EINVAL; }
    wchar_t tmp[65];
    wchar_t* p = tmp + 64;
    *p = 0;
    unsigned long long v = value;
    do {
        unsigned digit = v % (unsigned)radix;
        *--p = (digit < 10) ? L'0' + digit : L'a' + digit - 10;
        v /= (unsigned)radix;
    } while (v);
    size_t len = tmp + 64 - p;
    if (len >= sizeInWords) { buffer[0] = 0; return ERANGE; }
    wcscpy(buffer, p);
    return 0;
}

static errno_t WINAPI_CALL ucrt__i64tow_s(long long value, wchar_t* buffer, size_t sizeInWords, int radix) {
    if (!buffer || sizeInWords == 0) return EINVAL;
    if (radix < 2 || radix > 36) { buffer[0] = 0; return EINVAL; }
    bool neg = false;
    unsigned long long v;
    if (value < 0 && radix == 10) { neg = true; v = (unsigned long long)(-value); }
    else v = (unsigned long long)value;
    wchar_t tmp[65];
    wchar_t* p = tmp + 64;
    *p = 0;
    do {
        unsigned digit = v % (unsigned)radix;
        *--p = (digit < 10) ? L'0' + digit : L'a' + digit - 10;
        v /= (unsigned)radix;
    } while (v);
    if (neg) *--p = L'-';
    size_t len = tmp + 64 - p;
    if (len >= sizeInWords) { buffer[0] = 0; return ERANGE; }
    wcscpy(buffer, p);
    return 0;
}

static errno_t WINAPI_CALL ucrt__ultoa_s(unsigned long value, char* buffer, size_t sizeInChars, int radix) {
    if (!buffer || sizeInChars == 0) return EINVAL;
    if (radix < 2 || radix > 36) { buffer[0] = 0; return EINVAL; }
    char tmp[65];
    char* p = tmp + 64;
    *p = 0;
    unsigned long v = value;
    do {
        unsigned digit = v % (unsigned)radix;
        *--p = (digit < 10) ? '0' + digit : 'a' + digit - 10;
        v /= (unsigned)radix;
    } while (v);
    size_t len = tmp + 64 - p;
    if (len >= sizeInChars) { buffer[0] = 0; return ERANGE; }
    strcpy(buffer, p);
    return 0;
}

static errno_t WINAPI_CALL ucrt__ltoa_s(long value, char* buffer, size_t sizeInChars, int radix) {
    if (!buffer || sizeInChars == 0) return EINVAL;
    if (radix < 2 || radix > 36) { buffer[0] = 0; return EINVAL; }
    bool neg = false;
    unsigned long v;
    if (value < 0 && radix == 10) { neg = true; v = (unsigned long)(-value); }
    else v = (unsigned long)value;
    char tmp[65];
    char* p = tmp + 64;
    *p = 0;
    do {
        unsigned digit = v % (unsigned)radix;
        *--p = (digit < 10) ? '0' + digit : 'a' + digit - 10;
        v /= (unsigned)radix;
    } while (v);
    if (neg) *--p = '-';
    size_t len = tmp + 64 - p;
    if (len >= sizeInChars) { buffer[0] = 0; return ERANGE; }
    strcpy(buffer, p);
    return 0;
}

static int WINAPI_CALL ucrt__atoi64(const char* s) { return s ? (int)strtoll(s, nullptr, 10) : 0; }

static errno_t WINAPI_CALL ucrt__ui64toa_s(unsigned long long value, char* buffer, size_t sizeInChars, int radix) {
    if (!buffer || sizeInChars == 0) return EINVAL;
    if (radix < 2 || radix > 36) { buffer[0] = 0; return EINVAL; }
    char tmp[65];
    char* p = tmp + 64;
    *p = 0;
    unsigned long long v = value;
    do {
        unsigned digit = v % (unsigned)radix;
        *--p = (digit < 10) ? '0' + digit : 'a' + digit - 10;
        v /= (unsigned)radix;
    } while (v);
    size_t len = tmp + 64 - p;
    if (len >= sizeInChars) { buffer[0] = 0; return ERANGE; }
    strcpy(buffer, p);
    return 0;
}

static errno_t WINAPI_CALL ucrt__i64toa_s(long long value, char* buffer, size_t sizeInChars, int radix) {
    if (!buffer || sizeInChars == 0) return EINVAL;
    if (radix < 2 || radix > 36) { buffer[0] = 0; return EINVAL; }
    bool neg = false;
    unsigned long long v;
    if (value < 0 && radix == 10) { neg = true; v = (unsigned long long)(-value); }
    else v = (unsigned long long)value;
    char tmp[65];
    char* p = tmp + 64;
    *p = 0;
    do {
        unsigned digit = v % (unsigned)radix;
        *--p = (digit < 10) ? '0' + digit : 'a' + digit - 10;
        v /= (unsigned)radix;
    } while (v);
    if (neg) *--p = '-';
    size_t len = tmp + 64 - p;
    if (len >= sizeInChars) { buffer[0] = 0; return ERANGE; }
    strcpy(buffer, p);
    return 0;
}

// CRT internals / thread / stdio functions
static void* WINAPI_CALL ucrt__execute_onexit_table() { return nullptr; }
static void* WINAPI_CALL ucrt__seh_filter_dll(void* ex) { return nullptr; }
static int* WINAPI_CALL ucrt__errno() { return &errno; }
static wchar_t* WINAPI_CALL ucrt__wcsdup(const wchar_t* s) {
    if (!s) return nullptr;
    size_t len = wcslen(s);
    wchar_t* dup = static_cast<wchar_t*>(malloc((len + 1) * sizeof(wchar_t)));
    if (dup) wcscpy(dup, s);
    return dup;
}
static void WINAPI_CALL ucrt__invalid_parameter_noinfo() { abort(); }
static time_t WINAPI_CALL ucrt__time64(time_t* t) {
    time_t now = ::time(nullptr);
    if (t) *t = now;
    return now;
}
struct BeginThreadExArg {
    unsigned (*start)(void*);
    void* arg;
};

static void* ucrt__beginthreadex_start(void* arg) {
    BeginThreadExArg* a = static_cast<BeginThreadExArg*>(arg);
    a->start(a->arg);
    free(a);
    return nullptr;
}

static uintptr_t WINAPI_CALL ucrt__beginthreadex(void* security, unsigned stack_size,
                                                unsigned (*start)(void*), void* arg,
                                                unsigned initflag, unsigned* thrdaddr) {
    (void)security; (void)stack_size; (void)initflag; (void)thrdaddr;
    pthread_t tid;
    BeginThreadExArg* a = static_cast<BeginThreadExArg*>(malloc(sizeof(BeginThreadExArg)));
    a->start = start;
    a->arg = arg;
    if (pthread_create(&tid, nullptr, ucrt__beginthreadex_start, a) == 0) {
        return (uintptr_t)tid;
    }
    free(a);
    return 0;
}
static void WINAPI_CALL ucrt__lock_file(FILE* f) { (void)f; flockfile(f); }
static void WINAPI_CALL ucrt__unlock_file(FILE* f) { (void)f; funlockfile(f); }
static int WINAPI_CALL ucrt__fdsign(FILE* f) { (void)f; return 0; }
static int WINAPI_CALL ucrt__dsign(FILE* f) { (void)f; return 0; }
static void* WINAPI_CALL ucrt__get_stream_buffer_pointers(FILE* f, void** base, size_t* size) {
    (void)f; (void)base; (void)size; return nullptr;
}
static int WINAPI_CALL ucrt_fgetpos(FILE* f, fpos_t* pos) { return f ? ::fgetpos(f, pos) : -1; }
static int WINAPI_CALL ucrt_fsetpos(FILE* f, const fpos_t* pos) { return f ? ::fsetpos(f, pos) : -1; }
static int WINAPI_CALL ucrt__fdclass(FILE* f) { (void)f; return 0; }
static errno_t WINAPI_CALL ucrt__ultow_s(unsigned long val, wchar_t* buf, size_t size, int radix) {
    (void)radix;
    if (!buf || size == 0) return EINVAL;
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "%lu", val);
    mbstowcs(buf, tmp, size);
    return 0;
}
static void* WINAPI_CALL ucrt__aligned_malloc(size_t size, size_t alignment) {
    return ::aligned_alloc(alignment, size);
}
static void WINAPI_CALL ucrt__aligned_free(void* ptr) { ::free(ptr); }
static int WINAPI_CALL ucrt__callnewh(size_t size) { (void)size; return 1; }
static void* WINAPI_CALL ucrt__recalloc(void* ptr, size_t count, size_t size) {
    return realloc(ptr, count * size);
}
static time_t WINAPI_CALL ucrt__mktime64(struct tm* tm) {
    return ::mktime(tm);
}
static int WINAPI_CALL ucrt__wtoi(const wchar_t* str) {
    return str ? (int)wcstol(str, nullptr, 10) : 0;
}
static long long WINAPI_CALL ucrt_wcstoll(const wchar_t* str, wchar_t** endptr, int base) {
    return str ? ::wcstoll(str, endptr, base) : 0;
}
static char* WINAPI_CALL ucrt_strstr(const char* haystack, const char* needle) {
    return haystack && needle ? const_cast<char*>(::strstr(haystack, needle)) : nullptr;
}
static int WINAPI_CALL ucrt__stdio_common_vswscanf(void* stream, const wchar_t* fmt, va_list va, wchar_t* buf, size_t bufsz) {
    (void)stream; (void)fmt; (void)va; (void)buf; (void)bufsz; return 0;
}
static int WINAPI_CALL ucrt__stdio_common_vsscanf(void* stream, const char* fmt, va_list va, char* buf, size_t bufsz) {
    (void)stream; (void)fmt; (void)va; (void)buf; (void)bufsz; return 0;
}
static errno_t WINAPI_CALL ucrt__gmtime64_s(struct tm* tm, const time_t* t) {
    if (!tm || !t) return EINVAL;
    gmtime_r(t, tm);
    return 0;
}
static char* WINAPI_CALL ucrt_strrchr(const char* str, int c) {
    return const_cast<char*>(::strrchr(str, c));
}
static double WINAPI_CALL ucrt_atan2f(float y, float x) { return ::atan2f(y, x); }
static int WINAPI_CALL ucrt_fputc(int c, FILE* stream) { return stream ? ::fputc(c, stream) : EOF; }
static void* WINAPI_CALL ucrt_bsearch(const void* key, const void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*)) {
    return ::bsearch(key, base, nmemb, size, compar);
}
static int WINAPI_CALL ucrt_rand() { return ::rand(); }

// Math functions (additions to existing sqrt/fabs)
static double WINAPI_CALL ucrt_sin(double x) { return ::sin(x); }
static double WINAPI_CALL ucrt_cos(double x) { return ::cos(x); }
static double WINAPI_CALL ucrt_tan(double x) { return ::tan(x); }
static double WINAPI_CALL ucrt_sinf(float x) { return ::sinf(x); }
static double WINAPI_CALL ucrt_cosf(float x) { return ::cosf(x); }
static double WINAPI_CALL ucrt_pow(double x, double y) { return ::pow(x, y); }
static double WINAPI_CALL ucrt_powf(float x, float y) { return ::powf(x, y); }
static double WINAPI_CALL ucrt_sqrtf(float x) { return ::sqrtf(x); }
static double WINAPI_CALL ucrt_log(double x) { return ::log(x); }
static double WINAPI_CALL ucrt_log10(double x) { return ::log10(x); }
static double WINAPI_CALL ucrt_exp(double x) { return ::exp(x); }
static double WINAPI_CALL ucrt_expf(float x) { return ::expf(x); }
static double WINAPI_CALL ucrt_ceil(double x) { return ::ceil(x); }
static double WINAPI_CALL ucrt_ceilf(float x) { return ::ceilf(x); }
static double WINAPI_CALL ucrt_floor(double x) { return ::floor(x); }
static double WINAPI_CALL ucrt_floorf(float x) { return ::floorf(x); }
static double WINAPI_CALL ucrt_round(double x) { return ::round(x); }
static double WINAPI_CALL ucrt_atan2(double y, double x) { return ::atan2(y, x); }

// Additional missing CRT math functions
static double WINAPI_CALL ucrt_atan(double x) { return ::atan(x); }
static double WINAPI_CALL ucrt_acos(double x) { return ::acos(x); }
static double WINAPI_CALL ucrt_asin(double x) { return ::asin(x); }
static double WINAPI_CALL ucrt_ldexp(double x, int exp) { return ::ldexp(x, exp); }
static int WINAPI_CALL ucrt__isnan(double x) { return std::isnan(x); }
static double WINAPI_CALL ucrt_frexp(double x, int* exp) { return ::frexp(x, exp); }
static double WINAPI_CALL ucrt_modf(double x, double* iptr) { return ::modf(x, iptr); }
static float WINAPI_CALL ucrt_logf(float x) { return ::logf(x); }
static float WINAPI_CALL ucrt_acosf(float x) { return ::acosf(x); }
static float WINAPI_CALL ucrt_tanf(float x) { return ::tanf(x); }
static double WINAPI_CALL ucrt_nextafter(double x, double y) { return ::nextafter(x, y); }
static float WINAPI_CALL ucrt_fmodf(float x, float y) { return ::fmodf(x, y); }
static double WINAPI_CALL ucrt_fmod(double x, double y) { return ::fmod(x, y); }
static double WINAPI_CALL ucrt_trunc(double x) { return ::trunc(x); }
static float WINAPI_CALL ucrt_roundf(float x) { return ::roundf(x); }
static float WINAPI_CALL ucrt_strtof(const char* str, char** endptr) { return str ? ::strtof(str, endptr) : 0.0f; }
static float WINAPI_CALL ucrt_wcstof(const wchar_t* str, wchar_t** endptr) { return str ? ::wcstof(str, endptr) : 0.0f; }

// Missing CRT functions
static int WINAPI_CALL ucrt_ungetc(int c, FILE* stream) { return stream ? ::ungetc(c, stream) : EOF; }
static long WINAPI_CALL ucrt_strtol(const char* str, char** endptr, int base) { return str ? ::strtol(str, endptr, base) : 0; }
static double WINAPI_CALL ucrt_strtod(const char* str, char** endptr) { return str ? ::strtod(str, endptr) : 0.0; }
static int WINAPI_CALL ucrt_atoi(const char* str) { return str ? ::atoi(str) : 0; }
static unsigned long WINAPI_CALL ucrt_strtoul(const char* str, char** endptr, int base) { return str ? ::strtoul(str, endptr, base) : 0; }

// String functions (additions to existing wcscmp/wcsrchr/wcsstr/wcschr/wcslen)
static wchar_t* WINAPI_CALL ucrt_wcscpy(wchar_t* dst, const wchar_t* src) { return dst && src ? ::wcscpy(dst, src) : dst; }
static wchar_t* WINAPI_CALL ucrt_wcscat(wchar_t* dst, const wchar_t* src) { return dst && src ? ::wcscat(dst, src) : dst; }
static wchar_t* WINAPI_CALL ucrt_wcsncpy(wchar_t* dst, const wchar_t* src, size_t n) { return dst && src ? ::wcsncpy(dst, src, n) : dst; }
static int WINAPI_CALL ucrt_wcsncmp(const wchar_t* a, const wchar_t* b, size_t n) { return a && b ? ::wcsncmp(a, b, n) : (a ? 1 : (b ? -1 : 0)); }
static unsigned long WINAPI_CALL ucrt_wcstoul(const wchar_t* str, wchar_t** endptr, int base) { return str ? ::wcstoul(str, endptr, base) : 0; }
static double WINAPI_CALL ucrt_wcstod(const wchar_t* str, wchar_t** endptr) { return str ? ::wcstod(str, endptr) : 0.0; }
static errno_t WINAPI_CALL ucrt_wcsncpy_s(wchar_t* dst, size_t dstsz, const wchar_t* src, size_t count) {
    if (!dst || dstsz == 0) return EINVAL;
    if (!src) { dst[0] = L'\0'; return EINVAL; }
    if (count >= dstsz) count = dstsz - 1;
    wcsncpy(dst, src, count);
    dst[count] = L'\0';
    return 0;
}
static errno_t WINAPI_CALL ucrt_wcscpy_s(wchar_t* dst, size_t dstsz, const wchar_t* src) {
    if (!dst || dstsz == 0) return EINVAL;
    if (!src) { dst[0] = L'\0'; return EINVAL; }
    size_t len = wcslen(src);
    if (len >= dstsz) return ERANGE;
    wcscpy(dst, src);
    return 0;
}
static size_t WINAPI_CALL ucrt_wcsnlen(const wchar_t* s, size_t maxlen) { return s ? ::wcsnlen(s, maxlen) : 0; }
static errno_t WINAPI_CALL ucrt_wcscat_s(wchar_t* dst, size_t dstsz, const wchar_t* src) {
    if (!dst || dstsz == 0) return EINVAL;
    if (!src) return EINVAL;
    size_t dlen = wcslen(dst);
    if (dlen >= dstsz) return ERANGE;
    size_t slen = wcslen(src);
    if (dlen + slen >= dstsz) return ERANGE;
    wcscat(dst, src);
    return 0;
}

static unsigned long long WINAPI_CALL ucrt_wcstoull(const wchar_t* str, wchar_t** endptr, int base) { return str ? ::wcstoull(str, endptr, base) : 0; }
static unsigned long long WINAPI_CALL ucrt_strtoull(const char* str, char** endptr, int base) { return str ? ::strtoull(str, endptr, base) : 0; }
static long long WINAPI_CALL ucrt_strtoll(const char* str, char** endptr, int base) { return str ? ::strtoll(str, endptr, base) : 0; }
static errno_t WINAPI_CALL ucrt_strcpy_s(char* dst, size_t dstsz, const char* src) {
    if (!dst || dstsz == 0) return EINVAL;
    if (!src) { dst[0] = '\0'; return EINVAL; }
    size_t len = strlen(src);
    if (len >= dstsz) return ERANGE;
    strcpy(dst, src);
    return 0;
}
static errno_t WINAPI_CALL ucrt__localtime64_s(struct tm* tm, const time_t* t) {
    if (!tm || !t) return EINVAL;
    localtime_r(t, tm);
    return 0;
}
static HANDLE WINAPI_CALL ucrt_CreateSemaphoreExW(void* lpSemaphoreAttributes, LONG lInitialCount, LONG lMaximumCount, const wchar_t* lpName, DWORD dwFlags, DWORD dwDesiredAccess) {
    (void)lpSemaphoreAttributes; (void)lInitialCount; (void)lMaximumCount; (void)lpName; (void)dwFlags; (void)dwDesiredAccess;
    return (HANDLE)1; // Fake handle for compatibility
}
static int WINAPI_CALL ucrt__write(int fd, const void* buf, unsigned int count) { return ::write(fd, buf, count); }
static int WINAPI_CALL ucrt_MessageBoxW(void* hWnd, const wchar_t* lpText, const wchar_t* lpCaption, UINT uType) {
    (void)hWnd; (void)lpText; (void)lpCaption; (void)uType;
    return 0;
}
static long WINAPI_CALL ucrt__wtol(const wchar_t* str) { return str ? ::wcstol(str, nullptr, 10) : 0; }
static long WINAPI_CALL ucrt_PropVariantClear(void* pvar) { (void)pvar; return 0; }
static int WINAPI_CALL ucrt__ldsign(FILE* f) { (void)f; return 0; }
static int WINAPI_CALL ucrt__ldclass(FILE* f) { (void)f; return 0; }
static int WINAPI_CALL ucrt__fpclass(double x) { return std::fpclassify(x); }

static size_t WINAPI_CALL ucrt_wcscspn(const wchar_t* s1, const wchar_t* s2) { return s1 && s2 ? ::wcscspn(s1, s2) : 0; }

static int WINAPI_CALL ucrt__wsplitpath_s(const wchar_t* path, wchar_t* drive, size_t drive_sz, wchar_t* dir, size_t dir_sz, wchar_t* fname, size_t fname_sz, wchar_t* ext, size_t ext_sz) {
    (void)path; (void)drive; (void)drive_sz; (void)dir; (void)dir_sz; (void)fname; (void)fname_sz; (void)ext; (void)ext_sz;
    return 0;
}
static errno_t WINAPI_CALL ucrt_wmemcpy_s(wchar_t* dst, size_t dstsz, const wchar_t* src, size_t count) {
    if (!dst || !src || dstsz < count) return EINVAL;
    wmemcpy(dst, src, count);
    return 0;
}
static int WINAPI_CALL ucrt_ShellExecuteW(void* hwnd, const wchar_t* lpOperation, const wchar_t* lpFile, const wchar_t* lpParameters, const wchar_t* lpDirectory, int nShowCmd) {
    (void)hwnd; (void)lpOperation; (void)lpFile; (void)lpParameters; (void)lpDirectory; (void)nShowCmd;
    return 0;
}
static BOOL WINAPI_CALL ucrt_SetWaitableTimer(void* hTimer, const long long* pDueTime, long lPeriod, void* pfnCompletionRoutine, void* lpArgToCompletionRoutine, int fResume) {
    (void)hTimer; (void)pDueTime; (void)lPeriod; (void)pfnCompletionRoutine; (void)lpArgToCompletionRoutine; (void)fResume;
    return FALSE;
}
static int WINAPI_CALL ucrt_fputwc(wint_t c, FILE* stream) { return stream ? ::fputwc(c, stream) : WEOF; }
static int WINAPI_CALL ucrt_fgetwc(FILE* stream) { return stream ? ::fgetwc(stream) : WEOF; }
static int WINAPI_CALL ucrt_ferror(FILE* stream) { return stream ? ::ferror(stream) : 0; }
static int WINAPI_CALL ucrt__difftime64(time_t t1, time_t t0) { return (int)(t1 - t0); }
static float WINAPI_CALL ucrt_atanf(float x) { return ::atanf(x); }
static float WINAPI_CALL ucrt_asinf(float x) { return ::asinf(x); }
static long long WINAPI_CALL ucrt__wtoi64(const wchar_t* str) { return str ? ::wcstoll(str, nullptr, 10) : 0; }
static wchar_t* WINAPI_CALL ucrt__wgetenv(const wchar_t* name) { 
    if (!name) return nullptr;
    size_t len = wcslen(name) + 1;
    char* aname = static_cast<char*>(malloc(len));
    if (!aname) return nullptr;
    wcstombs(aname, name, len);
    const char* val = getenv(aname);
    free(aname);
    if (!val) return nullptr;
    len = strlen(val) + 1;
    wchar_t* wval = static_cast<wchar_t*>(malloc(len * sizeof(wchar_t)));
    if (!wval) return nullptr;
    mbstowcs(wval, val, len);
    return wval;
}
static long long WINAPI_CALL ucrt__wcstoi64(const wchar_t* str, wchar_t** endptr, int base) { return str ? ::wcstoll(str, endptr, base) : 0; }
static errno_t WINAPI_CALL ucrt__wcslwr_s(wchar_t* str, size_t size) { (void)str; (void)size; return 0; }
static wint_t WINAPI_CALL ucrt_ungetwc(wint_t c, FILE* stream) { return stream ? ::ungetwc(c, stream) : WEOF; }
static float WINAPI_CALL ucrt_modff(float x, float* iptr) { return ::modff(x, iptr); }
static int WINAPI_CALL ucrt__mkgmtime64(struct tm* tm) { return tm ? ::mktime(tm) : 0; }
static errno_t WINAPI_CALL ucrt__itow_s(int val, wchar_t* buf, size_t size, int radix) {
    if (!buf || size == 0) return EINVAL;
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "%d", val);
    mbstowcs(buf, tmp, size);
    return 0;
}
static errno_t WINAPI_CALL ucrt_strerror_s(char* buf, size_t size, int errnum) {
    if (!buf || size == 0) return EINVAL;
    char* msg = ::strerror(errnum);
    if (msg) strncpy(buf, msg, size - 1);
    buf[size - 1] = '\0';
    return 0;
}
static errno_t WINAPI_CALL ucrt_strcat_s(char* dst, size_t dstsz, const char* src) {
    if (!dst || dstsz == 0) return EINVAL;
    if (!src) return EINVAL;
    size_t dlen = strlen(dst);
    if (dlen >= dstsz) return ERANGE;
    size_t slen = strlen(src);
    if (dlen + slen >= dstsz) return ERANGE;
    strcat(dst, src);
    return 0;
}
static const char* WINAPI_CALL ucrt__std_type_info_name(void* e) {
    StdException* s = static_cast<StdException*>(e);
    return s ? s->what : "";
}
static void WINAPI_CALL ucrt_srand(unsigned int seed) { ::srand(seed); }
static int WINAPI_CALL ucrt__read(int fd, void* buf, unsigned int count) { return ::read(fd, buf, count); }
static long WINAPI_CALL ucrt_PropVariantCopy(void* dest, const void* src) { (void)dest; (void)src; return 0; }
static const unsigned short* WINAPI_CALL ucrt__pctype_func() { return nullptr; }
static wchar_t* WINAPI_CALL ucrt_wcspbrk(const wchar_t* s1, const wchar_t* s2) { return s1 && s2 ? const_cast<wchar_t*>(::wcspbrk(s1, s2)) : nullptr; }
static int WINAPI_CALL ucrt__gmtime64(time_t* t) {
    if (!t) return 0;
    struct tm tm;
    gmtime_r(t, &tm);
    *t = mktime(&tm);
    return 0;
}
static int WINAPI_CALL ucrt__free_locale(void* loc) { (void)loc; return 0; }
static float WINAPI_CALL ucrt_fmaxf(float x, float y) { return x > y ? x : y; }
static int WINAPI_CALL ucrt__finite(double x) { return std::isfinite(x); }
static int WINAPI_CALL ucrt_feof(FILE* stream) { return stream ? ::feof(stream) : 0; }
static float WINAPI_CALL ucrt_hypotf(float x, float y) { return ::hypotf(x, y); }

static int WINAPI_CALL ucrt_GetThreadPriority(void* hThread) { (void)hThread; return 0; }

// Character classification (additions to existing iswpunct)
static int WINAPI_CALL ucrt_iswspace(wint_t c) { return ::iswspace(c); }
static int WINAPI_CALL ucrt_iswdigit(wint_t c) { return ::iswdigit(c); }
static int WINAPI_CALL ucrt_iswalpha(wint_t c) { return ::iswalpha(c); }
static int WINAPI_CALL ucrt_iswalnum(wint_t c) { return ::iswalnum(c); }
static int WINAPI_CALL ucrt_iswxdigit(wint_t c) { return ::iswxdigit(c); }
static int WINAPI_CALL ucrt_iswprint(wint_t c) { return ::iswprint(c); }
static int WINAPI_CALL ucrt_iswupper(wint_t c) { return ::iswupper(c); }
static int WINAPI_CALL ucrt_iswlower(wint_t c) { return ::iswlower(c); }
static int WINAPI_CALL ucrt_towlower(wint_t c) { return ::towlower(c); }
static int WINAPI_CALL ucrt_towupper(wint_t c) { return ::towupper(c); }
static int WINAPI_CALL ucrt_isalpha(int c) { return ::isalpha(c); }
static int WINAPI_CALL ucrt_isdigit(int c) { return ::isdigit(c); }
static int WINAPI_CALL ucrt_isalnum(int c) { return ::isalnum(c); }
static int WINAPI_CALL ucrt_isxdigit(int c) { return ::isxdigit(c); }
static int WINAPI_CALL ucrt_ispunct(int c) { return ::ispunct(c); }
static int WINAPI_CALL ucrt_isprint(int c) { return ::isprint(c); }
static int WINAPI_CALL ucrt_isspace(int c) { return ::isspace(c); }
static int WINAPI_CALL ucrt_isupper(int c) { return ::isupper(c); }
static int WINAPI_CALL ucrt_islower(int c) { return ::islower(c); }
static int WINAPI_CALL ucrt_tolower(int c) { return ::tolower(c); }
static int WINAPI_CALL ucrt_toupper(int c) { return ::toupper(c); }

// vcruntime functions
static void WINAPI_CALL ucrt__purecall() { abort(); }
static void WINAPI_CALL ucrt___std_type_info_destroy_list(void* list) { (void)list; }
static int WINAPI_CALL ucrt___std_type_info_compare(void* a, void* b) { return a == b ? 0 : 1; }
static void* WINAPI_CALL ucrt___RTDynamicCast(void* obj, void* src_type, void* dst_type, ptrdiff_t offset, int is_reference) {
    (void)obj; (void)src_type; (void)dst_type; (void)offset; (void)is_reference;
    return nullptr;
}
static long WINAPI_CALL ucrt___C_specific_handler_noexcept(void* pExcept, void* pfn, void* pContext, void* pDispatcher) {
    (void)pExcept; (void)pfn; (void)pContext; (void)pDispatcher;
    return 0;
}

// ---------------------------------------------------------------------------
// CRT batch 2: missing functions needed by Wine DLLs + Office
// ---------------------------------------------------------------------------

struct ucrt_stat64i32 {
    int st_dev;
    int st_ino;
    unsigned short st_mode;
    short st_nlink;
    short st_uid;
    short st_gid;
    int st_rdev;
    long st_size;
    time_t satime;
    time_t smtime;
    time_t sctime;
};

// stdio
static int ucrt__fileno(void* stream) {
    FILE* f = file_for_token(stream);
    return f ? ::fileno(f) : -1;
}
static int ucrt_fseek(void* stream, long offset, int origin) {
    FILE* f = file_for_token(stream);
    return f ? ::fseek(f, offset, origin) : -1;
}
static long ucrt_ftell(void* stream) {
    FILE* f = file_for_token(stream);
    return f ? ::ftell(f) : -1;
}
static int ucrt__setmode(int fd, int mode) { return 0; }
static errno_t ucrt_fopen_s(void** pfile, const char* filename, const char* mode) {
    if (!pfile) return EINVAL;
    *pfile = nullptr;
    if (!filename || !mode) return EINVAL;
    FILE* f = ::fopen(filename, mode);
    if (!f) return errno ? errno : EIO;
    *pfile = token_for_file(f);
    return 0;
}
static void* ucrt__fdopen(int fd, const char* mode) {
    FILE* f = ::fdopen(fd, mode);
    return f ? token_for_file(f) : nullptr;
}
static int ucrt__fstat64i32(int fd, ucrt_stat64i32* buf) {
    if (!buf) return -1;
    struct stat st;
    int r = ::fstat(fd, &st);
    if (r != 0) return r;
    memset(buf, 0, sizeof(*buf));
    buf->st_dev = st.st_dev;
    buf->st_ino = st.st_ino;
    buf->st_mode = st.st_mode;
    buf->st_nlink = st.st_nlink;
    buf->st_uid = st.st_uid;
    buf->st_gid = st.st_gid;
    buf->st_rdev = st.st_rdev;
    buf->st_size = static_cast<long>(st.st_size);
    buf->satime = st.st_atime;
    buf->smtime = st.st_mtime;
    buf->sctime = st.st_ctime;
    return 0;
}
static int ucrt__stat64i32(const char* path, ucrt_stat64i32* buf) {
    if (!buf || !path) return -1;
    struct stat st;
    int r = ::stat(path, &st);
    if (r != 0) return r;
    memset(buf, 0, sizeof(*buf));
    buf->st_dev = st.st_dev;
    buf->st_ino = st.st_ino;
    buf->st_mode = st.st_mode;
    buf->st_nlink = st.st_nlink;
    buf->st_uid = st.st_uid;
    buf->st_gid = st.st_gid;
    buf->st_rdev = st.st_rdev;
    buf->st_size = static_cast<long>(st.st_size);
     buf->satime = st.st_atime;
     buf->smtime = st.st_mtime;
     buf->sctime = st.st_ctime;
    return 0;
}
static char* ucrt__tempnam(const char* dir, const char* prefix) {
    char* r = ::tempnam(dir ? dir : "/tmp", prefix ? prefix : "tmp");
    return r;
}

// string
static char* ucrt_strncpy(char* dst, const char* src, size_t count) {
    if (!dst || !src || count == 0) return dst;
    size_t i = 0;
    for (; i < count && src[i]; ++i) dst[i] = src[i];
    for (; i < count; ++i) dst[i] = '\0';
    return dst;
}
static int ucrt__strnicmp(const char* s1, const char* s2, size_t count) {
    if (!s1 || !s2) return (int)(uintptr_t)s1 - (int)(uintptr_t)s2;
    return ::strncasecmp(s1, s2, count);
}
static int ucrt_strcoll(const char* s1, const char* s2) {
    return ::strcoll(s1, s2);
}
static char* ucrt_strtok(char* str, const char* delim) {
    return ::strtok(str, delim);
}
static errno_t ucrt_strtok_s(char* str, const char* delim, char** context) {
    if (!str || !delim || !context) return EINVAL;
    char* tok = ::strtok(str, delim);
    *context = tok ? tok + strlen(tok) + 1 : nullptr;
    return 0;
}
static size_t ucrt_strncat_s(char* dst, size_t dstsz, const char* src, size_t count) {
    if (!dst || !src) return EINVAL;
    size_t dlen = ::strnlen(dst, dstsz);
    if (dlen >= dstsz) return EINVAL;
    size_t i = 0;
    for (; i < count && src[i] && dlen + i + 1 < dstsz; ++i)
        dst[dlen + i] = src[i];
    dst[dlen + i] = '\0';
    return 0;
}
static char* ucrt_strerror(char* buf, size_t sizeInBytes, int errnum) {
    if (!buf || sizeInBytes == 0) return buf;
    const char* msg = ::strerror(errnum);
    strncpy(buf, msg, sizeInBytes - 1);
    buf[sizeInBytes - 1] = '\0';
    return buf;
}
static size_t ucrt__mbsrchr(const unsigned char* str, unsigned int c) {
    const unsigned char* r = nullptr;
    for (const unsigned char* p = str; *p; ++p) {
        if (*p == (unsigned char)c) r = p;
    }
    if ((unsigned char)c == 0) return r ? (size_t)(r - str) : ::strlen((const char*)str);
    return r ? (size_t)(r - str) : (size_t)-1;
}
static int ucrt__swab(char* src, char* dst, int len) {
    if (!src || !dst || len < 0) return -1;
    for (int i = 0; i + 1 < len; i += 2) {
        char t = src[i]; src[i] = src[i+1]; src[i+1] = t;
        char u = dst[i]; dst[i] = dst[i+1]; dst[i+1] = u;
        dst[i] = src[i+1]; dst[i+1] = src[i];
    }
    return 0;
}

// wide string
static size_t ucrt_wcsncat(wchar_t* dst, const wchar_t* src, size_t count) {
    if (!dst || !src) return 0;
    size_t dlen = ::wcslen(dst);
    size_t i = 0;
    for (; i < count && src[i]; ++i) dst[dlen + i] = src[i];
    dst[dlen + i] = L'\0';
    return dlen + i;
}
static errno_t ucrt_wcstok_s(wchar_t* str, const wchar_t* delim, wchar_t** context) {
    if (!delim || !context) return EINVAL;
    wchar_t* tok = wcstok(str, delim, context);
    return 0;
}
static errno_t ucrt_wcstombs_s(size_t* ret, char* dst, size_t dstsz, const wchar_t* src, size_t count) {
    if (ret) *ret = 0;
    if (!src) return EINVAL;
    if (!dst || dstsz == 0) {
        if (ret) *ret = ::wcstombs(nullptr, src, 0);
        return 0;
    }
    size_t r = ::wcstombs(dst, src, dstsz - 1 < count ? dstsz - 1 : count);
    if (r == (size_t)-1) return EILSEQ;
    dst[r] = '\0';
    if (ret) *ret = r;
    return 0;
}
static size_t ucrt_wcstombs(char* dst, const wchar_t* src, size_t count) {
    if (!src) return 0;
    return ::wcstombs(dst, src, count);
}
static errno_t ucrt__wfopen_s(void** pfile, const wchar_t* filename, const wchar_t* mode) {
    if (!pfile) return EINVAL;
    *pfile = nullptr;
    if (!filename || !mode) return EINVAL;
    char fname[1024], fmode[32];
    wcstombs(fname, filename, sizeof(fname));
    wcstombs(fmode, mode, sizeof(fmode));
    FILE* f = ::fopen(fname, fmode);
    if (!f) return errno ? errno : EIO;
    *pfile = token_for_file(f);
    return 0;
}
static errno_t ucrt_wmemmove_s(wchar_t* dst, size_t dstsz, const wchar_t* src, size_t count) {
    if (!dst || !src) return EINVAL;
    if (count > dstsz) return EINVAL;
    ::wmemmove(dst, src, count);
    return 0;
}
static int ucrt__wmkdir(const wchar_t* path) {
    char buf[1024];
    wcstombs(buf, path, sizeof(buf));
    return ::mkdir(buf, 0777);
}
static errno_t ucrt__wputenv_s(const wchar_t* name, const wchar_t* value) {
    if (!name || !value) return EINVAL;
    char nbuf[512], vbuf[1024];
    wcstombs(nbuf, name, sizeof(nbuf));
    wcstombs(vbuf, value, sizeof(vbuf));
    return ::setenv(nbuf, vbuf, 1) == 0 ? 0 : errno;
}
static wchar_t* ucrt__wsetlocale(int category, const wchar_t* locale) {
    static wchar_t wbuf[256] = L"C";
    if (!locale) {
        const char* r = ::setlocale(category, nullptr);
        if (r) mbstowcs(wbuf, r, 255);
        return wbuf;
    }
    char buf[256];
    wcstombs(buf, locale, sizeof(buf));
    const char* r = ::setlocale(category, buf);
    if (r) mbstowcs(wbuf, r, 255);
    return wbuf;
}
static int ucrt__waccess(const wchar_t* path, int mode) {
    char buf[1024];
    wcstombs(buf, path, sizeof(buf));
    return ::access(buf, mode);
}

// conversion / locale
static wchar_t ucrt_btowc(int c) { return (wchar_t)::btowc(c); }
static double ucrt__strtod_l(const char* str, char** endptr, locale_t locale) {
    return ::strtod(str, endptr);
}
static long double ucrt_strtold(const char* str, char** endptr) {
    return ::strtold(str, endptr);
}
static long double ucrt__wcstold_l(const wchar_t* str, wchar_t** endptr, locale_t locale) {
    if (!str) return 0;
    char buf[1024];
    wcstombs(buf, str, sizeof(buf));
    return ::strtold(buf, nullptr);
}
static double ucrt__wcstod_l(const wchar_t* str, wchar_t** endptr, locale_t locale) {
    if (!str) return 0;
    char buf[1024];
    wcstombs(buf, str, sizeof(buf));
    return ::strtod(buf, nullptr);
}
static double ucrt__wtof_l(const wchar_t* str, locale_t locale) {
    return ucrt__wcstod_l(str, nullptr, locale);
}
static int64_t ucrt__strtoi64(const char* str, char** endptr, int base) {
    return ::strtoll(str, endptr, base);
}
static int ucrt__isctype_l(int c, int type, locale_t locale) {
    return ::isctype(c, type);
}
static int ucrt_iswascii(wint_t c) { return c <= 127; }
static int ucrt_iswpunct(wint_t c) { return ::iswpunct(c); }
static int ucrt__ecvt_s(char* buf, size_t size, double val, int ndigits, int* decpt, int* sign) {
    if (!buf || size == 0) return 1;
    char* r = ::ecvt(val, ndigits, decpt, sign);
    if (!r) return 1;
    strncpy(buf, r, size - 1);
    buf[size - 1] = '\0';
    return 0;
}

// integer
static long long ucrt_llabs(long long x) { return x < 0 ? -x : x; }
static long long ucrt_llround(double x) { return (long long)::round(x); }
static long ucrt_lround(double x) { return (long)::round(x); }
static long ucrt_lroundf(float x) { return (long)::roundf(x); }
static int ucrt_i64toa_s(int64_t val, char* buf, size_t size, int radix) {
    if (!buf || size == 0) return 1;
    snprintf(buf, size, "%lld", val);
    return 0;
}
static int ucrt__ltow_s(long val, wchar_t* buf, size_t size, int radix) {
    if (!buf || size == 0) return 1;
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "%ld", val);
    mbstowcs(buf, tmp, size);
    return 0;
}

// FP / math extras
static int ucrt__controlfp_s(unsigned int* cur, unsigned int newval, unsigned int mask) {
    if (cur) *cur = 0;
    return 0;
}
static int ucrt__fpreset(void) { return 0; }
static double ucrt_cbrt(double x) { return ::cbrt(x); }
static float ucrt_cbrtf(float x) { return ::cbrtf(x); }
static float ucrt_exp2(float x) { return ::exp2f(x); }
static float ucrt_expm1f(float x) { return ::expm1f(x); }
static int ucrt_feclearexcept(int excepts) { return ::feclearexcept(excepts); }
static int ucrt_fegetround(void) { return ::fegetround(); }
static int ucrt_fesetround(int round) { return ::fesetround(round); }
static float ucrt_ilogbf(float x) { return ::ilogbf(x); }
static float ucrt_log10f(float x) { return ::log10f(x); }
static float ucrt_log1pf(float x) { return ::log1pf(x); }
static float ucrt_scalbnf(float x, int n) { return ::scalbnf(x, n); }
static float ucrt_tanhf(float x) { return ::tanhf(x); }

// CRT internals
static void ucrt__set_doserrno(int err) {}
static int ucrt__get_doserrno(void) { return 0; }
static void ucrt__set_errno(int err) { errno = err; }
static int ucrt__getpid(void) { return ::getpid(); }
static int ucrt__getch(void) { return 0; }
static void ucrt__crt_at_quick_exit(void (*func)(void*), void* context) {}
static void ucrt__wassert(const wchar_t* expr, const wchar_t* file, unsigned line) {
    fprintf(stderr, "assertion failed: %ls at %ls:%u\n", expr, file, line);
    ::abort();
}
static void* ucrt__aligned_realloc(void* ptr, size_t size, size_t alignment) {
    return ::aligned_alloc(alignment, size);
}
static void* ucrt__create_locale(int category, const char* locale) {
    return (void*)1;
}
static void* ucrt_bsearch_s(const void* key, const void* base, size_t num, size_t width,
                            int (*compare)(const void*, const void*, void*), void* context) {
    (void)context;
    return ::bsearch(key, base, num, width, reinterpret_cast<__compar_fn_t>(compare));
}

// More missing CRT functions
static void ucrt__endthread() { pthread_exit(0); }
static int ucrt___sys_nerr_val = 133;
static long ucrt___timezone_val = 0;
static char* ucrt___tzname_val[2] = {(char*)"UTC", (char*)""};

// C++ EH helpers (msvcrt.dll imports these)
static void ucrt__cxx_throw_exception(void*_throwInfo, void* pException) { (void)_throwInfo; (void)pException; }

// misc missing CRT
static int ucrt_unlink(const char* path) { return ::unlink(path); }
static void* ucrt__local_unwind(int frame, int trylevel) { (void)frame; (void)trylevel; return nullptr; }
static void* ucrt___RTtypeid(void* typeinfo) { return typeinfo; }

// Additional CRT functions not in batch1
static int ucrt__fseeki64(void* stream, int64_t offset, int origin) {
    FILE* f = file_for_token(stream);
    return f ? ::fseeko(f, offset, origin) : -1;
}
static long long ucrt__ftelli64(void* stream) {
    FILE* f = file_for_token(stream);
    return f ? static_cast<long long>(::ftello(f)) : -1;
}
static unsigned int ucrt___lc_codepage_func_3() { return 65001; }

// ---------------------------------------------------------------------------
// registration
// ---------------------------------------------------------------------------

static void register_ucrt_api(const char* api_set, const char* name, void* func) {
    register_api(api_set, name, func);
    register_api("ucrtbase.dll", name, func);
    register_api("ucrtbase", name, func);       // extensionless imports
    register_api("msvcrt.dll", name, func);
}

void initialize_ucrt_layer() {
    // runtime
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "_set_app_type", (void*)ucrt__set_app_type);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "__set_app_type", (void*)ucrt__set_app_type);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "_configure_narrow_argv", (void*)ucrt__configure_narrow_argv);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "_initialize_narrow_environment", (void*)ucrt__initialize_narrow_environment);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "_set_invalid_parameter_handler", (void*)ucrt__set_invalid_parameter_handler);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "__p___argc", (void*)ucrt___p___argc);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "__p___argv", (void*)ucrt___p___argv);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "__p__acmdln", (void*)ucrt___p__acmdln);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "_crt_atexit", (void*)ucrt__crt_atexit);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "_onexit", (void*)ucrt__onexit);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "_initterm", (void*)ucrt__initterm);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "_initterm_e", (void*)ucrt__initterm_e);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "_cexit", (void*)ucrt__cexit);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "abort", (void*)ucrt_abort);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "exit", (void*)ucrt_exit);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "_exit", (void*)ucrt__exit);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "signal", (void*)ucrt_signal);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "_set_new_mode", (void*)ucrt__set_new_mode);
    register_ucrt_api("api-ms-win-crt-heap-l1-1-0.dll", "_set_new_mode", (void*)ucrt__set_new_mode);

    // stdio
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "printf", (void*)ucrt_printf);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "puts", (void*)ucrt_puts);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "fprintf", (void*)ucrt_fprintf);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "fflush", (void*)ucrt_fflush);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "__acrt_iob_func", (void*)ucrt___acrt_iob_func);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "__p__commode", (void*)ucrt___p__commode);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "__p__fmode", (void*)ucrt___p__fmode);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "setvbuf", (void*)ucrt_setvbuf);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "__stdio_common_vfprintf", (void*)ucrt___stdio_common_vfprintf);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "__stdio_common_vsnwprintf_s", (void*)ucrt___stdio_common_vsnwprintf_s);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "__stdio_common_vsnprintf_s", (void*)ucrt___stdio_common_vsnprintf_s);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "__stdio_common_vswprintf_s", (void*)ucrt___stdio_common_vswprintf_s);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "__stdio_common_vsprintf_s", (void*)ucrt___stdio_common_vsprintf_s);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "fwrite", (void*)ucrt_fwrite);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "fread", (void*)ucrt_fread);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "fgetc", (void*)ucrt_fgetc);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "fopen", (void*)ucrt_fopen);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "fclose", (void*)ucrt_fclose);

    // math
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "sqrt", (void*)ucrt_sqrt);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "fabs", (void*)ucrt_fabs);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "__setusermatherr", (void*)ucrt___setusermatherr);

    // vcruntime (no api-set: exact names)
    register_api("vcruntime140.dll", "__C_specific_handler", (void*)ucrt___C_specific_handler);
    register_api("vcruntime140.dll", "__CxxFrameHandler", (void*)ucrt___CxxFrameHandler);
    register_api("vcruntime140.dll", "__CxxFrameHandler3", (void*)ucrt___CxxFrameHandler);
    register_api("vcruntime140.dll", "_CxxThrowException", (void*)ucrt__CxxThrowException);
    register_api("ucrtbase.dll", "__C_specific_handler", (void*)ucrt___C_specific_handler);
    register_ucrt_api("api-ms-win-crt-private-l1-1-0.dll", "_except_handler3", (void*)ucrt__except_handler3);
    register_ucrt_api("api-ms-win-crt-private-l1-1-0.dll", "_except_handler4_common", (void*)ucrt__except_handler4_common);
    register_ucrt_api("api-ms-win-crt-private-l1-1-0.dll", "_local_unwind2", (void*)ucrt__local_unwind2);
    register_ucrt_api("api-ms-win-crt-private-l1-1-0.dll", "_local_unwind4", (void*)ucrt__local_unwind4);

    // string
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "strlen", (void*)ucrt_strlen);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "strcpy", (void*)ucrt_strcpy);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "strcat", (void*)ucrt_strcat);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "strcmp", (void*)ucrt_strcmp);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "strncmp", (void*)ucrt_strncmp);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "memcpy", (void*)ucrt_memcpy);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "memset", (void*)ucrt_memset);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "memmove", (void*)ucrt_memmove);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "memcmp", (void*)ucrt_memcmp);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "strnlen", (void*)ucrt_strnlen);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "_ui64tow_s", (void*)ucrt__ui64tow_s);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "_i64tow_s", (void*)ucrt__i64tow_s);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "_wtoi", (void*)ucrt__wtoi);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "_ultoa_s", (void*)ucrt__ultoa_s);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "_ltoa_s", (void*)ucrt__ltoa_s);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "_atoi64", (void*)ucrt__atoi64);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "_wtoi64", (void*)ucrt__wtoi64);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "_ui64toa_s", (void*)ucrt__ui64toa_s);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "_i64toa_s", (void*)ucrt__i64toa_s);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "strcpy_s", (void*)ucrt_strcpy_s);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "strcat_s", (void*)ucrt_strcat_s);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "strncat_s", (void*)ucrt_strncat_s);

    // wide string
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "wcslen", (void*)ucrt_wcslen);

    // heap
    register_ucrt_api("api-ms-win-crt-heap-l1-1-0.dll", "malloc", (void*)ucrt_malloc);
    register_ucrt_api("api-ms-win-crt-heap-l1-1-0.dll", "free", (void*)ucrt_free);
    register_ucrt_api("api-ms-win-crt-heap-l1-1-0.dll", "calloc", (void*)ucrt_calloc);
    register_ucrt_api("api-ms-win-crt-heap-l1-1-0.dll", "realloc", (void*)ucrt_realloc);

    // locale
    register_ucrt_api("api-ms-win-crt-locale-l1-1-0.dll", "_setlocale", (void*)ucrt__setlocale);
    register_ucrt_api("api-ms-win-crt-locale-l1-1-0.dll", "setlocale", (void*)ucrt__setlocale);
    register_ucrt_api("api-ms-win-crt-locale-l1-1-0.dll", "localeconv", (void*)ucrt_localeconv);
    register_ucrt_api("api-ms-win-crt-locale-l1-1-0.dll", "_configthreadlocale", (void*)ucrt__configthreadlocale);

    // environment
    register_ucrt_api("api-ms-win-crt-environment-l1-1-0.dll", "_environ", (void*)ucrt__initialize_narrow_environment);
    register_ucrt_api("api-ms-win-crt-environment-l1-1-0.dll", "__p__environ", (void*)ucrt___p__environ);

    // private api-set: unsafe string ops + vcruntime SEH helper
    register_ucrt_api("api-ms-win-crt-private-l1-1-0.dll", "__acrt_iob_func", (void*)ucrt___acrt_iob_func);
    register_ucrt_api("api-ms-win-crt-private-l1-1-0.dll", "__C_specific_handler", (void*)ucrt___C_specific_handler);
    register_ucrt_api("api-ms-win-crt-private-l1-1-0.dll", "memcpy", (void*)ucrt_memcpy);
    register_ucrt_api("api-ms-win-crt-private-l1-1-0.dll", "memset", (void*)ucrt_memset);
    register_ucrt_api("api-ms-win-crt-private-l1-1-0.dll", "memmove", (void*)ucrt_memmove);
    register_ucrt_api("api-ms-win-crt-private-l1-1-0.dll", "memcmp", (void*)ucrt_memcmp);
    register_ucrt_api("api-ms-win-crt-private-l1-1-0.dll", "__stdio_common_vsprintf", (void*)ucrt___stdio_common_vsprintf);
    register_ucrt_api("api-ms-win-crt-private-l1-1-0.dll", "__stdio_common_vswprintf", (void*)ucrt___stdio_common_vswprintf);
    register_ucrt_api("api-ms-win-crt-private-l1-1-0.dll", "_assert", (void*)ucrt__assert);
    register_ucrt_api("api-ms-win-crt-private-l1-1-0.dll", "_strdup", (void*)ucrt__strdup);
    register_ucrt_api("api-ms-win-crt-private-l1-1-0.dll", "getenv", (void*)ucrt_getenv);
    register_ucrt_api("api-ms-win-crt-private-l1-1-0.dll", "strchr", (void*)ucrt_strchr);
    register_ucrt_api("api-ms-win-crt-private-l1-1-0.dll", "strcspn", (void*)ucrt_strcspn);
    register_ucrt_api("api-ms-win-crt-private-l1-1-0.dll", "wcscmp", (void*)ucrt_wcscmp);
    register_ucrt_api("api-ms-win-crt-private-l1-1-0.dll", "wcsstr", (void*)ucrt_wcsstr);
    register_ucrt_api("api-ms-win-crt-private-l1-1-0.dll", "wcsrchr", (void*)ucrt_wcsrchr);
    register_ucrt_api("api-ms-win-crt-private-l1-1-0.dll", "_vsnprintf", (void*)ucrt__vsnprintf);
    register_api("ntdll.dll", "_vsnprintf", (void*)ucrt__vsnprintf);

    // ---- PE32+ (64-bit guest) runtime surface ----
    // runtime api-set
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "_c_exit", (void*)ucrt__c_exit);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "terminate", (void*)ucrt_terminate);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "_seh_filter_exe", (void*)ucrt__seh_filter_exe);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "_invoke_watson", (void*)ucrt__invoke_watson);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "_initialize_onexit_table", (void*)ucrt__initialize_onexit_table);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "_register_onexit_function", (void*)ucrt__register_onexit_function);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "_register_thread_local_exe_atexit_callback", (void*)ucrt__register_thread_local_exe_atexit_callback);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "_get_narrow_winmain_command_line", (void*)ucrt__get_narrow_winmain_command_line);
    // stdio
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "_set_fmode", (void*)ucrt__set_fmode);
    // string
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "strncpy_s", (void*)ucrt_strncpy_s);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "_stricmp", (void*)ucrt__stricmp);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "_wcsnicmp", (void*)ucrt__wcsnicmp);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "_wcsicmp", (void*)ucrt__wcsicmp);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "_vsnprintf", (void*)ucrt__vsnprintf);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "wcsncat_s", (void*)ucrt_wcsncat_s);
    // locale
    register_ucrt_api("api-ms-win-crt-locale-l1-1-0.dll", "__initialize_lconv_for_unsigned_char", (void*)ucrt___initialize_lconv_for_unsigned_char);
    // vcruntime140.dll (host memcpy/memset are ABI-identical and called
    // through the 64-bit shim either way)
    register_api("vcruntime140.dll", "memcpy", (void*)memcpy);
    register_api("vcruntime140.dll", "memset", (void*)memset);
    register_api("vcruntime140.dll", "__current_exception", (void*)ucrt___current_exception);
    register_api("vcruntime140.dll", "__current_exception_context", (void*)ucrt___current_exception_context);
    register_api("vcruntime140.dll", "__std_exception_copy", (void*)ucrt___std_exception_copy);
    register_api("vcruntime140.dll", "__std_exception_destroy", (void*)ucrt___std_exception_destroy);
    register_api("vcruntime140.dll", "__std_terminate", (void*)ucrt___std_terminate);
    // vcruntime140_1.dll
    register_api("vcruntime140_1.dll", "__CxxFrameHandler4", (void*)vcruntime140_1___CxxFrameHandler4);
    // msvcp140.dll (first entries)
    register_api("msvcp140.dll", "_Query_perf_counter", (void*)msvcp140_Query_perf_counter);
    register_api("msvcp140.dll", "_Query_perf_frequency", (void*)msvcp140_Query_perf_frequency);
    register_api("msvcp140.dll", "?_Xbad_alloc@std@@YAXXZ", (void*)msvcp140_Xbad_alloc);
    register_api("msvcp140.dll", "?_Xlength_error@std@@YAXPEBD@Z", (void*)msvcp140_Xlength_error);

    // Math functions (api-ms-win-crt-math-l1-1-0.dll)
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "sin", (void*)ucrt_sin);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "cos", (void*)ucrt_cos);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "tan", (void*)ucrt_tan);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "sinf", (void*)ucrt_sinf);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "cosf", (void*)ucrt_cosf);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "pow", (void*)ucrt_pow);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "powf", (void*)ucrt_powf);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "sqrtf", (void*)ucrt_sqrtf);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "log", (void*)ucrt_log);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "log10", (void*)ucrt_log10);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "exp", (void*)ucrt_exp);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "expf", (void*)ucrt_expf);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "ceil", (void*)ucrt_ceil);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "ceilf", (void*)ucrt_ceilf);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "floor", (void*)ucrt_floor);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "floorf", (void*)ucrt_floorf);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "round", (void*)ucrt_round);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "fmod", (void*)ucrt_fmod);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "atan2", (void*)ucrt_atan2);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "atan2f", (void*)ucrt_atan2f);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "atan", (void*)ucrt_atan);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "acos", (void*)ucrt_acos);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "asin", (void*)ucrt_asin);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "ldexp", (void*)ucrt_ldexp);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "_isnan", (void*)ucrt__isnan);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "frexp", (void*)ucrt_frexp);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "modf", (void*)ucrt_modf);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "logf", (void*)ucrt_logf);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "acosf", (void*)ucrt_acosf);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "tanf", (void*)ucrt_tanf);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "nextafter", (void*)ucrt_nextafter);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "fmodf", (void*)ucrt_fmodf);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "trunc", (void*)ucrt_trunc);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "roundf", (void*)ucrt_roundf);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "strtof", (void*)ucrt_strtof);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "wcstof", (void*)ucrt_wcstof);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "_wtol", (void*)ucrt__wtol);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "roundf", (void*)ucrt_roundf);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "_fpclass", (void*)ucrt__fpclass);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "ldexp", (void*)ucrt_ldexp);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "modf", (void*)ucrt_modf);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "logf", (void*)ucrt_logf);

    // String functions (api-ms-win-crt-string-l1-1-0.dll)
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "wcscpy", (void*)ucrt_wcscpy);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "wcscat", (void*)ucrt_wcscat);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "wcsncpy", (void*)ucrt_wcsncpy);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "wcsncmp", (void*)ucrt_wcsncmp);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "wcstoul", (void*)ucrt_wcstoul);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "wcstod", (void*)ucrt_wcstod);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "wcsncpy_s", (void*)ucrt_wcsncpy_s);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "wcscpy_s", (void*)ucrt_wcscpy_s);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "wcsnlen", (void*)ucrt_wcsnlen);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "wcscat_s", (void*)ucrt_wcscat_s);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "wcstoull", (void*)ucrt_wcstoull);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "strcpy_s", (void*)ucrt_strcpy_s);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "wcscspn", (void*)ucrt_wcscspn);

    // Character classification (api-ms-win-crt-string-l1-1-0.dll / locale)
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "iswspace", (void*)ucrt_iswspace);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "iswdigit", (void*)ucrt_iswdigit);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "iswalpha", (void*)ucrt_iswalpha);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "iswalnum", (void*)ucrt_iswalnum);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "iswxdigit", (void*)ucrt_iswxdigit);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "iswprint", (void*)ucrt_iswprint);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "iswupper", (void*)ucrt_iswupper);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "iswlower", (void*)ucrt_iswlower);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "towlower", (void*)ucrt_towlower);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "towupper", (void*)ucrt_towupper);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "isalpha", (void*)ucrt_isalpha);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "isdigit", (void*)ucrt_isdigit);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "isalnum", (void*)ucrt_isalnum);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "isxdigit", (void*)ucrt_isxdigit);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "ispunct", (void*)ucrt_ispunct);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "isprint", (void*)ucrt_isprint);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "isspace", (void*)ucrt_isspace);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "isupper", (void*)ucrt_isupper);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "islower", (void*)ucrt_islower);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "tolower", (void*)ucrt_tolower);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "toupper", (void*)ucrt_toupper);

    // vcruntime functions
    register_ucrt_api("vcruntime140.dll", "_purecall", (void*)ucrt__purecall);
    register_ucrt_api("vcruntime140.dll", "__std_type_info_destroy_list", (void*)ucrt___std_type_info_destroy_list);
    register_ucrt_api("vcruntime140.dll", "__std_type_info_compare", (void*)ucrt___std_type_info_compare);
    register_ucrt_api("vcruntime140.dll", "__RTDynamicCast", (void*)ucrt___RTDynamicCast);
    register_ucrt_api("vcruntime140.dll", "__C_specific_handler_noexcept", (void*)ucrt___C_specific_handler_noexcept);

    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "memchr", (void*)ucrt_memchr);

    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "_fseeki64", (void*)ucrt__fseeki64);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "fseeki64", (void*)ucrt__fseeki64);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "_ftelli64", (void*)ucrt__ftelli64);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "ftelli64", (void*)ucrt__ftelli64);
    register_ucrt_api("api-ms-win-crt-locale-l1-1-0.dll", "___lc_codepage_func", (void*)ucrt___lc_codepage_func_3);

    // CRT batch 2: stdio
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "_fileno", (void*)ucrt__fileno);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "fseek", (void*)ucrt_fseek);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "ftell", (void*)ucrt_ftell);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "_setmode", (void*)ucrt__setmode);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "fopen_s", (void*)ucrt_fopen_s);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "_fdopen", (void*)ucrt__fdopen);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "_tempnam", (void*)ucrt__tempnam);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "_wfopen_s", (void*)ucrt__wfopen_s);
    register_ucrt_api("api-ms-win-crt-filesystem-l1-1-0.dll", "_fstat64i32", (void*)ucrt__fstat64i32);
    register_ucrt_api("api-ms-win-crt-filesystem-l1-1-0.dll", "_stat64i32", (void*)ucrt__stat64i32);
    register_ucrt_api("api-ms-win-crt-filesystem-l1-1-0.dll", "_unlink", (void*)ucrt_unlink);
    register_ucrt_api("api-ms-win-crt-filesystem-l1-1-0.dll", "_wmkdir", (void*)ucrt__wmkdir);

    // CRT batch 2: string
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "strncpy", (void*)ucrt_strncpy);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "_strnicmp", (void*)ucrt__strnicmp);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "strcoll", (void*)ucrt_strcoll);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "strtok", (void*)ucrt_strtok);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "strtok_s", (void*)ucrt_strtok_s);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "strncat_s", (void*)ucrt_strncat_s);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "strerror", (void*)ucrt_strerror);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "_mbsrchr", (void*)ucrt__mbsrchr);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "_swab", (void*)ucrt__swab);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "iswascii", (void*)ucrt_iswascii);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "iswpunct", (void*)ucrt_iswpunct);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "_isctype_l", (void*)ucrt__isctype_l);

    // CRT batch 2: wide string
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "wcsncat", (void*)ucrt_wcsncat);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "wcstok_s", (void*)ucrt_wcstok_s);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "wcstombs_s", (void*)ucrt_wcstombs_s);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "wcstombs", (void*)ucrt_wcstombs);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "wmemmove_s", (void*)ucrt_wmemmove_s);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "_wsetlocale", (void*)ucrt__wsetlocale);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "_wputenv_s", (void*)ucrt__wputenv_s);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "_waccess", (void*)ucrt__waccess);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "_wstat", (void*)ucrt__stat64i32);

    // CRT batch 2: conversion
    register_ucrt_api("api-ms-win-crt-convert-l1-1-0.dll", "btowc", (void*)ucrt_btowc);
    register_ucrt_api("api-ms-win-crt-convert-l1-1-0.dll", "_strtod_l", (void*)ucrt__strtod_l);
    register_ucrt_api("api-ms-win-crt-convert-l1-1-0.dll", "strtold", (void*)ucrt_strtold);
    register_ucrt_api("api-ms-win-crt-convert-l1-1-0.dll", "_wcstod_l", (void*)ucrt__wcstod_l);
    register_ucrt_api("api-ms-win-crt-convert-l1-1-0.dll", "_wcstold_l", (void*)ucrt__wcstold_l);
    register_ucrt_api("api-ms-win-crt-convert-l1-1-0.dll", "_wtof_l", (void*)ucrt__wtof_l);
    register_ucrt_api("api-ms-win-crt-convert-l1-1-0.dll", "_strtoi64", (void*)ucrt__strtoi64);
    register_ucrt_api("api-ms-win-crt-convert-l1-1-0.dll", "_ecvt_s", (void*)ucrt__ecvt_s);
    register_ucrt_api("api-ms-win-crt-convert-l1-1-0.dll", "mbstowcs", (void*)ucrt_wcstombs);
    register_ucrt_api("api-ms-win-crt-convert-l1-1-0.dll", "mbstowcs_s", (void*)ucrt_wcstombs_s);

    // CRT batch 2: integer
    register_ucrt_api("api-ms-win-crt-utility-l1-1-0.dll", "llabs", (void*)ucrt_llabs);
    register_ucrt_api("api-ms-win-crt-utility-l1-1-0.dll", "llround", (void*)ucrt_llround);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "lround", (void*)ucrt_lround);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "lroundf", (void*)ucrt_lroundf);
    register_ucrt_api("api-ms-win-crt-convert-l1-1-0.dll", "_i64toa_s", (void*)ucrt_i64toa_s);
    register_ucrt_api("api-ms-win-crt-convert-l1-1-0.dll", "_ltow_s", (void*)ucrt__ltow_s);

    // CRT batch 2: FP / math
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "_controlfp_s", (void*)ucrt__controlfp_s);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "_fpreset", (void*)ucrt__fpreset);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "feclearexcept", (void*)ucrt_feclearexcept);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "fegetround", (void*)ucrt_fegetround);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "fesetround", (void*)ucrt_fesetround);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "cbrt", (void*)ucrt_cbrt);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "cbrtf", (void*)ucrt_cbrtf);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "exp2", (void*)ucrt_exp2);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "expm1f", (void*)ucrt_expm1f);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "ilogbf", (void*)ucrt_ilogbf);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "log10f", (void*)ucrt_log10f);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "log1pf", (void*)ucrt_log1pf);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "scalbnf", (void*)ucrt_scalbnf);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "tanhf", (void*)ucrt_tanhf);

    // CRT batch 2: CRT internals
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "_set_doserrno", (void*)ucrt__set_doserrno);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "_get_doserrno", (void*)ucrt__get_doserrno);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "_set_errno", (void*)ucrt__set_errno);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "_getpid", (void*)ucrt__getpid);
    register_ucrt_api("api-ms-win-crt-conio-l1-1-0.dll", "_getch", (void*)ucrt__getch);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "_crt_at_quick_exit", (void*)ucrt__crt_at_quick_exit);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "_wassert", (void*)ucrt__wassert);
    register_ucrt_api("api-ms-win-crt-heap-l1-1-0.dll", "_aligned_realloc", (void*)ucrt__aligned_realloc);
    register_ucrt_api("api-ms-win-crt-locale-l1-1-0.dll", "_create_locale", (void*)ucrt__create_locale);
    register_ucrt_api("api-ms-win-crt-utility-l1-1-0.dll", "bsearch_s", (void*)ucrt_bsearch_s);

    // CRT batch 2: C++ EH (msvcrt.dll imports)
    register_ucrt_api("msvcrt.dll", "_CxxThrowException", (void*)ucrt__cxx_throw_exception);
    register_ucrt_api("msvcrt.dll", "??0exception@@QEAA@AEBQEBDH@Z", (void*)ucrt_exception_ctor);
    register_ucrt_api("msvcrt.dll", "??0exception@@QEAA@AEBV0@@Z", (void*)ucrt_exception_ctor);
    register_ucrt_api("msvcrt.dll", "??1exception@@UEAA@XZ", (void*)ucrt_exception_dtor);
    register_ucrt_api("msvcrt.dll", "??1type_info@@UEAA@XZ", (void*)ucrt_type_info_dtor);
    register_ucrt_api("msvcrt.dll", "?terminate@@YAXXZ", (void*)ucrt_terminate);
    register_ucrt_api("msvcrt.dll", "?what@exception@@UEBAPEBDXZ", (void*)ucrt_exception_what);
    register_ucrt_api("ucrtbase.dll", "_CxxThrowException", (void*)ucrt__cxx_throw_exception);
    register_ucrt_api("msvcrt.dll", "_fileno", (void*)ucrt__fileno);
    register_ucrt_api("msvcrt.dll", "fseek", (void*)ucrt_fseek);
    register_ucrt_api("msvcrt.dll", "ftell", (void*)ucrt_ftell);
    register_ucrt_api("msvcrt.dll", "_strnicmp", (void*)ucrt__strnicmp);
    register_ucrt_api("msvcrt.dll", "strncpy", (void*)ucrt_strncpy);
    register_ucrt_api("msvcrt.dll", "memcpy_s", (void*)ucrt_memcpy_s);
    register_ucrt_api("msvcrt.dll", "_ltow_s", (void*)ucrt__ltow_s);
    register_ucrt_api("msvcrt.dll", "wcstombs", (void*)ucrt_wcstombs);

    // Missing CRT functions from WINWORD unresolved
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "_execute_onexit_table", (void*)ucrt__execute_onexit_table);
    register_ucrt_api("api-ms-win-crt-private-l1-1-0.dll", "_seh_filter_dll", (void*)ucrt__seh_filter_dll);
    register_ucrt_api("api-ms-win-crt-environment-l1-1-0.dll", "_errno", (void*)ucrt__errno);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "_wcsdup", (void*)ucrt__wcsdup);
    register_ucrt_api("api-ms-win-crt-private-l1-1-0.dll", "_invalid_parameter_noinfo", (void*)ucrt__invalid_parameter_noinfo);
    register_ucrt_api("api-ms-win-crt-time-l1-1-0.dll", "_time64", (void*)ucrt__time64);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "strtoul", (void*)ucrt_strtoul);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "_beginthreadex", (void*)ucrt__beginthreadex);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "ungetc", (void*)ucrt_ungetc);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "strtol", (void*)ucrt_strtol);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "strtod", (void*)ucrt_strtod);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "fputc", (void*)ucrt_fputc);
    register_ucrt_api("api-ms-win-crt-utility-l1-1-0.dll", "bsearch", (void*)ucrt_bsearch);
    register_ucrt_api("api-ms-win-crt-convert-l1-1-0.dll", "atoi", (void*)ucrt_atoi);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "strrchr", (void*)ucrt_strrchr);
    register_ucrt_api("api-ms-win-crt-heap-l1-1-0.dll", "_callnewh", (void*)ucrt__callnewh);
    register_ucrt_api("api-ms-win-crt-heap-l1-1-0.dll", "_recalloc", (void*)ucrt__recalloc);
    register_ucrt_api("api-ms-win-crt-utility-l1-1-0.dll", "rand", (void*)ucrt_rand);
    register_ucrt_api("api-ms-win-crt-time-l1-1-0.dll", "_mktime64", (void*)ucrt__mktime64);
    register_ucrt_api("api-ms-win-crt-convert-l1-1-0.dll", "_ultow_s", (void*)ucrt__ultow_s);
    register_ucrt_api("api-ms-win-crt-heap-l1-1-0.dll", "_aligned_malloc", (void*)ucrt__aligned_malloc);
    register_ucrt_api("api-ms-win-crt-heap-l1-1-0.dll", "_aligned_free", (void*)ucrt__aligned_free);
    register_ucrt_api("api-ms-win-crt-convert-l1-1-0.dll", "wcstoll", (void*)ucrt_wcstoll);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "strstr", (void*)ucrt_strstr);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "__stdio_common_vswscanf", (void*)ucrt__stdio_common_vswscanf);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "__stdio_common_vsscanf", (void*)ucrt__stdio_common_vsscanf);
    register_ucrt_api("api-ms-win-crt-time-l1-1-0.dll", "_gmtime64_s", (void*)ucrt__gmtime64_s);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "atan2f", (void*)ucrt_atan2f);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "_lock_file", (void*)ucrt__lock_file);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "_unlock_file", (void*)ucrt__unlock_file);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "fgetpos", (void*)ucrt_fgetpos);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "fsetpos", (void*)ucrt_fsetpos);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "_fdclass", (void*)ucrt__fdclass);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "_fdsign", (void*)ucrt__fdsign);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "_dsign", (void*)ucrt__dsign);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "_get_stream_buffer_pointers", (void*)ucrt__get_stream_buffer_pointers);

    // Missing CRT functions from WINWORD unresolved
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "_write", (void*)ucrt__write);
    register_ucrt_api("user32.dll", "MessageBoxW", (void*)ucrt_MessageBoxW);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "_wtol", (void*)ucrt__wtol);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "_localtime64_s", (void*)ucrt__localtime64_s);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "_fpclass", (void*)ucrt__fpclass);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "_ldsign", (void*)ucrt__ldsign);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "_ldclass", (void*)ucrt__ldclass);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "ldexp", (void*)ucrt_ldexp);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "modf", (void*)ucrt_modf);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "logf", (void*)ucrt_logf);
    register_ucrt_api("api-ms-win-crt-convert-l1-1-0.dll", "strtoull", (void*)ucrt_strtoull);
    register_ucrt_api("api-ms-win-crt-convert-l1-1-0.dll", "strtoll", (void*)ucrt_strtoll);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "strcpy_s", (void*)ucrt_strcpy_s);
    register_ucrt_api("api-ms-win-crt-time-l1-1-0.dll", "_localtime64_s", (void*)ucrt__localtime64_s);
    register_ucrt_api("kernel32.dll", "CreateSemaphoreExW", (void*)ucrt_CreateSemaphoreExW);
    register_ucrt_api("oleaut32.dll", "PropVariantClear", (void*)ucrt_PropVariantClear);

    // Additional missing CRT functions from WINWORD unresolved
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "_wsplitpath_s", (void*)ucrt__wsplitpath_s);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "wmemcpy_s", (void*)ucrt_wmemcpy_s);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "ungetwc", (void*)ucrt_ungetwc);
    register_ucrt_api("shell32.dll", "ShellExecuteW", (void*)ucrt_ShellExecuteW);
    register_ucrt_api("kernel32.dll", "SetWaitableTimer", (void*)ucrt_SetWaitableTimer);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "fputwc", (void*)ucrt_fputwc);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "fgetwc", (void*)ucrt_fgetwc);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "ferror", (void*)ucrt_ferror);
    register_ucrt_api("api-ms-win-crt-time-l1-1-0.dll", "_difftime64", (void*)ucrt__difftime64);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "atanf", (void*)ucrt_atanf);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "asinf", (void*)ucrt_asinf);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "ungetwc", (void*)ucrt_ungetwc);
    register_ucrt_api("api-ms-win-crt-time-l1-1-0.dll", "_mkgmtime64", (void*)ucrt__mkgmtime64);
    register_ucrt_api("api-ms-win-crt-convert-l1-1-0.dll", "_itow_s", (void*)ucrt__itow_s);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "strerror_s", (void*)ucrt_strerror_s);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "strcat_s", (void*)ucrt_strcat_s);
    register_ucrt_api("msvcrt.dll", "__std_type_info_name", (void*)ucrt__std_type_info_name);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "srand", (void*)ucrt_srand);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "_read", (void*)ucrt__read);
    register_ucrt_api("vcruntime140.dll", "PropVariantCopy", (void*)ucrt_PropVariantCopy);
    register_ucrt_api("api-ms-win-crt-locale-l1-1-0.dll", "__pctype_func", (void*)ucrt__pctype_func);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "wcspbrk", (void*)ucrt_wcspbrk);
    register_ucrt_api("api-ms-win-crt-time-l1-1-0.dll", "_gmtime64", (void*)ucrt__gmtime64);
    register_ucrt_api("api-ms-win-crt-locale-l1-1-0.dll", "_free_locale", (void*)ucrt__free_locale);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "fmaxf", (void*)ucrt_fmaxf);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "_finite", (void*)ucrt__finite);
    register_ucrt_api("api-ms-win-crt-stdio-l1-1-0.dll", "feof", (void*)ucrt_feof);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "hypotf", (void*)ucrt_hypotf);
    register_ucrt_api("kernel32.dll", "GetThreadPriority", (void*)ucrt_GetThreadPriority);
    register_ucrt_api("api-ms-win-crt-time-l1-1-0.dll", "_mkgmtime64", (void*)ucrt__mkgmtime64);
    register_ucrt_api("api-ms-win-crt-math-l1-1-0.dll", "modff", (void*)ucrt_modff);
    register_ucrt_api("api-ms-win-crt-time-l1-1-0.dll", "_gmtime64", (void*)ucrt__gmtime64);

    // Additional missing CRT functions

    // CRT batch 2: vcruntime140.dll
    register_ucrt_api("vcruntime140.dll", "_local_unwind", (void*)ucrt__local_unwind);
    register_ucrt_api("vcruntime140.dll", "__RTtypeid", (void*)ucrt___RTtypeid);
    register_ucrt_api("vcruntime140.dll", "__uncaught_exception", (void*)ucrt___uncaught_exception);
    register_ucrt_api("vcruntime140.dll", "__uncaught_exceptions", (void*)ucrt___uncaught_exceptions);

    // CRT batch 2: remaining missing
    register_ucrt_api("api-ms-win-crt-utility-l1-1-0.dll", "div", (void*)::div);
    register_ucrt_api("api-ms-win-crt-runtime-l1-1-0.dll", "_endthread", (void*)ucrt__endthread);
    register_ucrt_api("api-ms-win-crt-string-l1-1-0.dll", "strncat", (void*)::strncat);
    register_ucrt_api("api-ms-win-crt-locale-l1-1-0.dll", "___lc_collate_cp_func", (void*)ucrt___lc_collate_cp_func);
    register_ucrt_api("api-ms-win-crt-private-l1-1-0.dll", "_o____lc_codepage_func", (void*)ucrt___lc_codepage_func_3);
    register_ucrt_api("api-ms-win-crt-private-l1-1-0.dll", "_o____lc_locale_name_func", (void*)ucrt___lc_locale_name_func);
    register_ucrt_api("api-ms-win-crt-private-l1-1-0.dll", "_o____mb_cur_max_func", (void*)ucrt___mb_cur_max_func);
    register_ucrt_api("api-ms-win-crt-locale-l1-1-0.dll", "___lc_locale_name_func", (void*)ucrt___lc_locale_name_func);
    register_ucrt_api("api-ms-win-crt-locale-l1-1-0.dll", "___mb_cur_max_func", (void*)ucrt___mb_cur_max_func);
    register_ucrt_api("ucrtbase.dll", "___lc_locale_name_func", (void*)ucrt___lc_locale_name_func);
    register_ucrt_api("ucrtbase.dll", "___mb_cur_max_func", (void*)ucrt___mb_cur_max_func);
    register_api("ucrtbase.dll", "__sys_nerr", (void*)&ucrt___sys_nerr_val);
    register_api("ucrtbase.dll", "__timezone", (void*)&ucrt___timezone_val);
    register_api("ucrtbase.dll", "__tzname", (void*)ucrt___tzname_val);
}

} // namespace ThetaZ
