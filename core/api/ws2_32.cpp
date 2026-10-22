// ThetaZ — ws2_32 (WinSock2) layer backed by POSIX sockets.
// Copyright (c) 2026 ThetaZ Project. MIT License.

#include "core/api/api.h"
#include "core/api/kernel32.h"
#include "core/x86/api_dispatch.h"

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include <mutex>
#include <unordered_map>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>

namespace ThetaZ {

namespace {

constexpr int WSA_VERSION_REQUIRED = 0x0202;
constexpr int WSADESCRIPTION_LEN = 256;
constexpr int WSASYS_STATUS_LEN = 128;

struct WSAData {
    uint16_t wVersion;
    uint16_t wHighVersion;
    char szDescription[WSADESCRIPTION_LEN + 1];
    char szSystemStatus[WSASYS_STATUS_LEN + 1];
    uint16_t iMaxSockets;
    uint16_t iMaxUdpDg;
    uint32_t lpVendorInfo;  // guest-visible 32-bit pointer
};
static_assert(sizeof(WSAData) == 400, "WSADATA must be 400 bytes on x86");

// SOCKETs are small integer fd-like values; keep a map of windows socket -> fd.
std::mutex g_wsock_mutex;
std::unordered_map<uintptr_t, int> g_wsock_map;
uintptr_t g_next_wsock = 0x100;

int wsock_fd(uintptr_t s) {
    std::lock_guard<std::mutex> lock(g_wsock_mutex);
    auto it = g_wsock_map.find(s);
    return it != g_wsock_map.end() ? it->second : -1;
}

uintptr_t wsock_add(int fd) {
    std::lock_guard<std::mutex> lock(g_wsock_mutex);
    uintptr_t s = g_next_wsock++;
    g_wsock_map[s] = fd;
    return s;
}

bool wsock_remove(uintptr_t s) {
    std::lock_guard<std::mutex> lock(g_wsock_mutex);
    auto it = g_wsock_map.find(s);
    if (it == g_wsock_map.end()) return false;
    int fd = it->second;
    g_wsock_map.erase(it);
    if (fd >= 0) ::close(fd);
    return true;
}

// Windows sockaddr maps directly to sockaddr_in/sockaddr_in6 layouts.
inline sockaddr* ws_sockaddr(void* a) { return static_cast<sockaddr*>(a); }
inline socklen_t ws_sockaddr_len(int* len) { return len ? static_cast<socklen_t>(*len) : 0; }

int ws_errno_to_wsa() {
    switch (errno) {
        case EACCES: case EPERM: return 10013;  // WSAEACCES
        case EADDRINUSE: return 10048;          // WSAEADDRINUSE
        case EADDRNOTAVAIL: return 10049;       // WSAEADDRNOTAVAIL
        case EAFNOSUPPORT: return 10047;        // WSAEAFNOSUPPORT
        case EALREADY: return 10037;            // WSAEALREADY
        case ECONNABORTED: return 10053;        // WSAECONNABORTED
        case ECONNREFUSED: return 10061;        // WSAECONNREFUSED
        case ECONNRESET: return 10054;          // WSAECONNRESET
        case EFAULT: return 10014;              // WSAEFAULT
        case EHOSTUNREACH: return 10065;        // WSAEHOSTUNREACH
        case EINPROGRESS: return 10036;         // WSAEINPROGRESS
        case EINTR: return 10004;               // WSAEINTR
        case EINVAL: return 10022;              // WSAEINVAL
        case EISCONN: return 10056;             // WSAEISCONN
        case EMFILE: return 10024;              // WSAEMFILE
        case EMSGSIZE: return 10040;            // WSAEMSGSIZE
        case ENAMETOOLONG: return 10063;        // WSAENAMETOOLONG
        case ENETDOWN: return 10050;            // WSAENETDOWN
        case ENETUNREACH: return 10051;         // WSAENETUNREACH
        case ENOBUFS: return 10055;             // WSAENOBUFS
        case ENODEV: return 10019;              // WSAENODEV
        case ENOENT: return 10049;              // WSAEADDRNOTAVAIL
        case ENOTCONN: return 10057;            // WSAENOTCONN
        case ENOTSOCK: return 10038;            // WSAENOTSOCK
        case EOPNOTSUPP: return 10045;          // WSAEOPNOTSUPP
        case EPROTONOSUPPORT: return 10043;     // WSAEPROTONOSUPPORT
        case ETIMEDOUT: return 10060;           // WSAETIMEDOUT
        default: return errno;                  // fallback
    }
}

thread_local int g_ws_last_error = 0;

} // namespace

// ---------------------------------------------------------------------------
// WinSock API
// ---------------------------------------------------------------------------

int WINAPI_CALL THETAZ_WSAStartup(uint16_t wVersionRequested, void* lpWSAData) {
    (void)wVersionRequested;
    if (!lpWSAData) return 10022; // WSAEINVAL
    WSAData* d = static_cast<WSAData*>(lpWSAData);
    memset(d, 0, sizeof(WSAData));
    d->wVersion = 0x0202;
    d->wHighVersion = 0x0202;
    strcpy(d->szDescription, "ThetaZ Winsock 2.0");
    strcpy(d->szSystemStatus, "Running");
    d->iMaxSockets = 1024;
    d->iMaxUdpDg = 65535;
    g_ws_last_error = 0;
    return 0;
}

int WINAPI_CALL THETAZ_WSACleanup() {
    return 0;
}

int WINAPI_CALL THETAZ_WSAGetLastError() {
    return g_ws_last_error;
}

void WINAPI_CALL THETAZ_WSASetLastError(int iError) {
    g_ws_last_error = iError;
}

uintptr_t WINAPI_CALL THETAZ_socket(int af, int type, int protocol) {
    int fd = ::socket(af, type, protocol);
    if (fd < 0) { g_ws_last_error = ws_errno_to_wsa(); return ~0ull; }
    return wsock_add(fd);
}

int WINAPI_CALL THETAZ_closesocket(uintptr_t s) {
    if (!wsock_remove(s)) { g_ws_last_error = 10038; return 1; } // SOCKET_ERROR
    g_ws_last_error = 0;
    return 0;
}

int WINAPI_CALL THETAZ_bind(uintptr_t s, void* name, int namelen) {
    int fd = wsock_fd(s);
    if (fd < 0) { g_ws_last_error = 10038; return 1; }
    int r = ::bind(fd, ws_sockaddr(name), static_cast<socklen_t>(namelen));
    if (r < 0) { g_ws_last_error = ws_errno_to_wsa(); return 1; }
    return 0;
}

int WINAPI_CALL THETAZ_listen(uintptr_t s, int backlog) {
    int fd = wsock_fd(s);
    if (fd < 0) { g_ws_last_error = 10038; return 1; }
    int r = ::listen(fd, backlog);
    if (r < 0) { g_ws_last_error = ws_errno_to_wsa(); return 1; }
    return 0;
}

uintptr_t WINAPI_CALL THETAZ_accept(uintptr_t s, void* addr, int* addrlen) {
    int fd = wsock_fd(s);
    if (fd < 0) { g_ws_last_error = 10038; return ~0ull; }
    socklen_t len = addrlen ? static_cast<socklen_t>(*addrlen) : 0;
    int c = ::accept(fd, addr ? ws_sockaddr(addr) : nullptr, addr ? &len : nullptr);
    if (c < 0) { g_ws_last_error = ws_errno_to_wsa(); return ~0ull; }
    if (addrlen) *addrlen = static_cast<int>(len);
    return wsock_add(c);
}

int WINAPI_CALL THETAZ_connect(uintptr_t s, void* name, int namelen) {
    int fd = wsock_fd(s);
    if (fd < 0) { g_ws_last_error = 10038; return 1; }
    int r = ::connect(fd, ws_sockaddr(name), static_cast<socklen_t>(namelen));
    if (r < 0) { g_ws_last_error = ws_errno_to_wsa(); return 1; }
    return 0;
}

int WINAPI_CALL THETAZ_send(uintptr_t s, const char* buf, int len, int flags) {
    int fd = wsock_fd(s);
    if (fd < 0) { g_ws_last_error = 10038; return 1; }
    int r = static_cast<int>(::send(fd, buf, len, flags));
    if (r < 0) { g_ws_last_error = ws_errno_to_wsa(); return 1; }
    return r;
}

int WINAPI_CALL THETAZ_recv(uintptr_t s, char* buf, int len, int flags) {
    int fd = wsock_fd(s);
    if (fd < 0) { g_ws_last_error = 10038; return 1; }
    int r = static_cast<int>(::recv(fd, buf, len, flags));
    if (r < 0) { g_ws_last_error = ws_errno_to_wsa(); return 1; }
    return r;
}

int WINAPI_CALL THETAZ_sendto(uintptr_t s, const char* buf, int len, int flags, void* to, int tolen) {
    int fd = wsock_fd(s);
    if (fd < 0) { g_ws_last_error = 10038; return 1; }
    int r = static_cast<int>(::sendto(fd, buf, len, flags, to ? ws_sockaddr(to) : nullptr, static_cast<socklen_t>(tolen)));
    if (r < 0) { g_ws_last_error = ws_errno_to_wsa(); return 1; }
    return r;
}

int WINAPI_CALL THETAZ_recvfrom(uintptr_t s, char* buf, int len, int flags, void* from, int* fromlen) {
    int fd = wsock_fd(s);
    if (fd < 0) { g_ws_last_error = 10038; return 1; }
    socklen_t fl = fromlen ? static_cast<socklen_t>(*fromlen) : 0;
    int r = static_cast<int>(::recvfrom(fd, buf, len, flags, from ? ws_sockaddr(from) : nullptr, from ? &fl : nullptr));
    if (r < 0) { g_ws_last_error = ws_errno_to_wsa(); return 1; }
    if (fromlen) *fromlen = static_cast<int>(fl);
    return r;
}

int WINAPI_CALL THETAZ_setsockopt(uintptr_t s, int level, int optname, const char* optval, int optlen) {
    int fd = wsock_fd(s);
    if (fd < 0) { g_ws_last_error = 10038; return 1; }
    int r = ::setsockopt(fd, level, optname, optval, static_cast<socklen_t>(optlen));
    if (r < 0) { g_ws_last_error = ws_errno_to_wsa(); return 1; }
    return 0;
}

int WINAPI_CALL THETAZ_getsockopt(uintptr_t s, int level, int optname, char* optval, int* optlen) {
    int fd = wsock_fd(s);
    if (fd < 0) { g_ws_last_error = 10038; return 1; }
    socklen_t len = optlen ? static_cast<socklen_t>(*optlen) : 0;
    int r = ::getsockopt(fd, level, optname, optval, &len);
    if (r < 0) { g_ws_last_error = ws_errno_to_wsa(); return 1; }
    if (optlen) *optlen = static_cast<int>(len);
    return 0;
}

int WINAPI_CALL THETAZ_getsockname(uintptr_t s, void* name, int* namelen) {
    int fd = wsock_fd(s);
    if (fd < 0) { g_ws_last_error = 10038; return 1; }
    socklen_t len = namelen ? static_cast<socklen_t>(*namelen) : 0;
    int r = ::getsockname(fd, name ? ws_sockaddr(name) : nullptr, &len);
    if (r < 0) { g_ws_last_error = ws_errno_to_wsa(); return 1; }
    if (namelen) *namelen = static_cast<int>(len);
    return 0;
}

int WINAPI_CALL THETAZ_getpeername(uintptr_t s, void* name, int* namelen) {
    int fd = wsock_fd(s);
    if (fd < 0) { g_ws_last_error = 10038; return 1; }
    socklen_t len = namelen ? static_cast<socklen_t>(*namelen) : 0;
    int r = ::getpeername(fd, name ? ws_sockaddr(name) : nullptr, &len);
    if (r < 0) { g_ws_last_error = ws_errno_to_wsa(); return 1; }
    if (namelen) *namelen = static_cast<int>(len);
    return 0;
}

int WINAPI_CALL THETAZ_select(int nfds, void* readfds, void* writefds, void* exceptfds, void* timeout) {
    (void)nfds;
    // Windows fd_set (x86): { DWORD fd_count; SOCKET fd_array[64]; } — 260
    // bytes. Convert the guest sets to host fd_sets (glibc layout differs),
    // call select, then rewrite the guest sets with the ready sockets.
    fd_set h_read, h_write, h_except;
    FD_ZERO(&h_read); FD_ZERO(&h_write); FD_ZERO(&h_except);
    int max_fd = -1;
    auto fill = [&](void* fds, fd_set& out) {
        if (!fds) return;
        uint32_t* g = static_cast<uint32_t*>(fds);
        uint32_t count = g[0];
        if (count > 64) count = 64;
        for (uint32_t i = 0; i < count; i++) {
            int fd = wsock_fd(g[1 + i]);
            if (fd >= 0) { FD_SET(fd, &out); if (fd > max_fd) max_fd = fd; }
        }
    };
    fill(readfds, h_read);
    fill(writefds, h_write);
    fill(exceptfds, h_except);

    timeval* tv = nullptr;
    timeval tv_buf;
    if (timeout) {
        // Windows TIMEVAL: { LONG tv_sec; LONG tv_usec; } — same layout.
        tv_buf.tv_sec = static_cast<long>(static_cast<const int32_t*>(timeout)[0]);
        tv_buf.tv_usec = static_cast<long>(static_cast<const int32_t*>(timeout)[1]);
        tv = &tv_buf;
    }

    int r = ::select(max_fd + 1, readfds ? &h_read : nullptr,
                     writefds ? &h_write : nullptr, exceptfds ? &h_except : nullptr, tv);
    if (r < 0) { g_ws_last_error = ws_errno_to_wsa(); return 1; } // SOCKET_ERROR

    // Rewrite the guest sets: keep only the sockets that are ready.
    auto writeback = [&](void* fds, fd_set& in) {
        if (!fds) return;
        uint32_t* g = static_cast<uint32_t*>(fds);
        uint32_t count = g[0];
        if (count > 64) count = 64;
        uint32_t n = 0;
        for (uint32_t i = 0; i < count; i++) {
            int fd = wsock_fd(g[1 + i]);
            if (fd >= 0 && FD_ISSET(fd, &in)) g[1 + n++] = g[1 + i];
        }
        g[0] = n;
    };
    writeback(readfds, h_read);
    writeback(writefds, h_write);
    writeback(exceptfds, h_except);
    return r;
}

int WINAPI_CALL THETAZ_ioctlsocket(uintptr_t s, long cmd, unsigned long* argp) {
    int fd = wsock_fd(s);
    if (fd < 0) { g_ws_last_error = 10038; return 1; }
    int r = ::fcntl(fd, F_GETFL);
    if (r < 0) { g_ws_last_error = ws_errno_to_wsa(); return 1; }
    if (cmd == 0x8004667E) { // FIONBIO
        int fl = *argp ? (r | O_NONBLOCK) : (r & ~O_NONBLOCK);
        if (::fcntl(fd, F_SETFL, fl) < 0) { g_ws_last_error = ws_errno_to_wsa(); return 1; }
    }
    return 0;
}

int WINAPI_CALL THETAZ_shutdown(uintptr_t s, int how) {
    int fd = wsock_fd(s);
    if (fd < 0) { g_ws_last_error = 10038; return 1; }
    int r = ::shutdown(fd, how);
    if (r < 0) { g_ws_last_error = ws_errno_to_wsa(); return 1; }
    return 0;
}

uint32_t WINAPI_CALL THETAZ_htonl(uint32_t hostlong) {
    return htonl(hostlong);
}

uint16_t WINAPI_CALL THETAZ_htons(uint16_t hostshort) {
    return htons(hostshort);
}

uint32_t WINAPI_CALL THETAZ_ntohl(uint32_t netlong) {
    return ntohl(netlong);
}

uint16_t WINAPI_CALL THETAZ_ntohs(uint16_t netshort) {
    return ntohs(netshort);
}

unsigned long WINAPI_CALL THETAZ_inet_addr(const char* cp) {
    if (!cp) return ~0ul;
    in_addr a;
    if (inet_pton(AF_INET, cp, &a) != 1) return ~0ul;
    return a.s_addr;
}

char* WINAPI_CALL THETAZ_inet_ntoa(void* in) {
    static thread_local char buf[16];
    in_addr a;
    a.s_addr = *reinterpret_cast<uint32_t*>(in);
    const char* s = inet_ntoa(a);
    if (!s) return nullptr;
    strcpy(buf, s);
    return buf;
}

int WINAPI_CALL THETAZ_gethostname(char* name, int namelen) {
    int r = ::gethostname(name, namelen);
    if (r < 0) { g_ws_last_error = ws_errno_to_wsa(); return 1; }
    return 0;
}

void* WINAPI_CALL THETAZ_gethostbyname(const char* name) {
    if (!name) return nullptr;
    struct hostent* h = ::gethostbyname(name);
    if (!h) return nullptr;
    // The glibc result lives on the host heap (above 4GB) — a 32-bit guest
    // cannot dereference it. Copy the whole hostent into guest-visible memory:
    //   [hostent 16 bytes][name][alias strings][alias ptr array]
    //   [addr ptr array][addr blobs]
    int naddr = 0, nalias = 0;
    while (h->h_addr_list && h->h_addr_list[naddr]) naddr++;
    while (h->h_aliases && h->h_aliases[nalias]) nalias++;
    if (naddr > 64) naddr = 64;
    if (nalias > 8) nalias = 8;
    size_t size = 16 + 256 + 256 + (size_t)(nalias + 1) * 4 +
                  (size_t)(naddr + 1) * 4 + (size_t)naddr * h->h_length;
    uint8_t* mem = static_cast<uint8_t*>(alloc_guest_shared(size));
    if (!mem) return nullptr;

    uint8_t* p = mem + 16;
    // h_name
    size_t name_len = h->h_name ? strlen(h->h_name) : 0;
    if (name_len > 255) name_len = 255;
    memcpy(p, h->h_name, name_len);
    p[name_len] = 0;
    uint8_t* guest_name = p;
    p += name_len + 1;
    // alias strings (cap each at 63 chars)
    uint32_t alias_ptrs[9];
    for (int i = 0; i < nalias; i++) {
        size_t len = h->h_aliases[i] ? strlen(h->h_aliases[i]) : 0;
        if (len > 63) len = 63;
        memcpy(p, h->h_aliases[i], len);
        p[len] = 0;
        alias_ptrs[i] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p));
        p += len + 1;
    }
    alias_ptrs[nalias] = 0;
    // pointer arrays
    uint32_t* aliases = reinterpret_cast<uint32_t*>(p);
    for (int i = 0; i <= nalias; i++) aliases[i] = alias_ptrs[i];
    p += (size_t)(nalias + 1) * 4;
    uint32_t* addr_list = reinterpret_cast<uint32_t*>(p);
    p += (size_t)(naddr + 1) * 4;
    // address blobs
    for (int i = 0; i < naddr; i++) {
        memcpy(p, h->h_addr_list[i], h->h_length);
        addr_list[i] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p));
        p += h->h_length;
    }
    addr_list[naddr] = 0;
    // hostent header
    *reinterpret_cast<uint32_t*>(mem + 0) = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(guest_name));
    *reinterpret_cast<uint32_t*>(mem + 4) = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(aliases));
    *reinterpret_cast<uint16_t*>(mem + 8) = h->h_addrtype;
    *reinterpret_cast<uint16_t*>(mem + 10) = h->h_length;
    *reinterpret_cast<uint32_t*>(mem + 12) = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(addr_list));
    return mem;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void initialize_ws2_32_layer() {
    auto reg = [](const char* n, void* f) { register_api("ws2_32.dll", n, f); };
    reg("WSAStartup", (void*)THETAZ_WSAStartup);
    reg("WSACleanup", (void*)THETAZ_WSACleanup);
    reg("WSAGetLastError", (void*)THETAZ_WSAGetLastError);
    reg("WSASetLastError", (void*)THETAZ_WSASetLastError);
    reg("socket", (void*)THETAZ_socket);
    reg("closesocket", (void*)THETAZ_closesocket);
    reg("bind", (void*)THETAZ_bind);
    reg("listen", (void*)THETAZ_listen);
    reg("accept", (void*)THETAZ_accept);
    reg("connect", (void*)THETAZ_connect);
    reg("send", (void*)THETAZ_send);
    reg("recv", (void*)THETAZ_recv);
    reg("sendto", (void*)THETAZ_sendto);
    reg("recvfrom", (void*)THETAZ_recvfrom);
    reg("setsockopt", (void*)THETAZ_setsockopt);
    reg("getsockopt", (void*)THETAZ_getsockopt);
    reg("getsockname", (void*)THETAZ_getsockname);
    reg("getpeername", (void*)THETAZ_getpeername);
    reg("select", (void*)THETAZ_select);
    reg("ioctlsocket", (void*)THETAZ_ioctlsocket);
    reg("shutdown", (void*)THETAZ_shutdown);
    reg("htonl", (void*)THETAZ_htonl);
    reg("htons", (void*)THETAZ_htons);
    reg("ntohl", (void*)THETAZ_ntohl);
    reg("ntohs", (void*)THETAZ_ntohs);
    reg("inet_addr", (void*)THETAZ_inet_addr);
    reg("inet_ntoa", (void*)THETAZ_inet_ntoa);
    reg("gethostname", (void*)THETAZ_gethostname);
    reg("gethostbyname", (void*)THETAZ_gethostbyname);

    // Common WinSock2 ordinals.
    register_api_ordinal("ws2_32.dll", 1, (void*)THETAZ_accept);
    register_api_ordinal("ws2_32.dll", 2, (void*)THETAZ_bind);
    register_api_ordinal("ws2_32.dll", 3, (void*)THETAZ_closesocket);
    register_api_ordinal("ws2_32.dll", 4, (void*)THETAZ_connect);
    register_api_ordinal("ws2_32.dll", 8, (void*)THETAZ_htonl);
    register_api_ordinal("ws2_32.dll", 9, (void*)THETAZ_htons);
    register_api_ordinal("ws2_32.dll", 10, (void*)THETAZ_ioctlsocket);
    register_api_ordinal("ws2_32.dll", 11, (void*)THETAZ_inet_addr);
    register_api_ordinal("ws2_32.dll", 12, (void*)THETAZ_inet_ntoa);
    register_api_ordinal("ws2_32.dll", 13, (void*)THETAZ_listen);
    register_api_ordinal("ws2_32.dll", 14, (void*)THETAZ_ntohl);
    register_api_ordinal("ws2_32.dll", 15, (void*)THETAZ_ntohs);
    register_api_ordinal("ws2_32.dll", 16, (void*)THETAZ_recv);
    register_api_ordinal("ws2_32.dll", 17, (void*)THETAZ_recvfrom);
    register_api_ordinal("ws2_32.dll", 18, (void*)THETAZ_select);
    register_api_ordinal("ws2_32.dll", 19, (void*)THETAZ_send);
    register_api_ordinal("ws2_32.dll", 20, (void*)THETAZ_sendto);
    register_api_ordinal("ws2_32.dll", 21, (void*)THETAZ_setsockopt);
    register_api_ordinal("ws2_32.dll", 22, (void*)THETAZ_shutdown);
    register_api_ordinal("ws2_32.dll", 23, (void*)THETAZ_socket);
    register_api_ordinal("ws2_32.dll", 30, (void*)THETAZ_gethostname);
    register_api_ordinal("ws2_32.dll", 25, (void*)THETAZ_gethostbyname);
    register_api_ordinal("ws2_32.dll", 115, (void*)THETAZ_WSAStartup);
    register_api_ordinal("ws2_32.dll", 116, (void*)THETAZ_WSACleanup);
    register_api_ordinal("ws2_32.dll", 117, (void*)THETAZ_WSAGetLastError);
    register_api_ordinal("ws2_32.dll", 118, (void*)THETAZ_WSASetLastError);
}

} // namespace ThetaZ
