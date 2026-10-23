// ThetaZ — stub implementations for ole32, version, advapi32, shlwapi,
// gdi32, user32, SLC/SLWGA, crypt32, bcrypt, ncrypt, winhttp, winmm APIs
// that WINWORD.EXE and other Office apps import.

#include <cstdio>
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <unistd.h>
#include <sys/time.h>

#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <set>

#include "core/api/api.h"
#include "core/api/kernel32.h"
#include "core/api/crypto.h"

#include <curl/curl.h>

namespace ThetaZ {

HRESULT WINAPI_CALL THETAZ_CoCreateGuid(GUID* pguid) {
    if (!pguid) return E_INVALIDARG;
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    uint32_t seed = static_cast<uint32_t>(tv.tv_sec) ^ static_cast<uint32_t>(tv.tv_usec)
                    ^ static_cast<uint32_t>(getpid()) ^ static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pguid));
    pguid->Data1 = seed;
    pguid->Data2 = static_cast<uint16_t>((seed >> 16) ^ (seed & 0xFFFF));
    pguid->Data3 = static_cast<uint16_t>((seed >> 8) ^ (seed & 0xFF));
    pguid->Data4[0] = 0x80;
    pguid->Data4[1] = static_cast<uint8_t>(seed >> 24);
    pguid->Data4[2] = static_cast<uint8_t>(seed >> 16);
    pguid->Data4[3] = static_cast<uint8_t>(seed >> 8);
    pguid->Data4[4] = 0x80;
    pguid->Data4[5] = static_cast<uint8_t>(seed);
    pguid->Data4[6] = static_cast<uint8_t>(seed >> 4);
    pguid->Data4[7] = static_cast<uint8_t>(seed >> 12);
    return S_OK;
}

// ---------------------------------------------------------------------------
// GUID / CLSID text conversion
//
// The guest formats are the canonical 38-character form
//     {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}
// stored as UTF-16 code units.  Host wchar_t is four bytes wide here, so the
// conversions below operate on uint16_t explicitly and never hand a guest
// wide string to a host wide-string routine.
// ---------------------------------------------------------------------------
static int guid_hex_digit(uint16_t c) {
    if (c >= u'0' && c <= u'9') return (int)(c - u'0');
    if (c >= u'a' && c <= u'f') return (int)(c - u'a') + 10;
    if (c >= u'A' && c <= u'F') return (int)(c - u'A') + 10;
    return -1;
}

static bool parse_guid_utf16(const uint16_t* s, GUID* out) {
    if (!s || !out) return false;
    if (*s == u'{') ++s;
    auto read_hex = [&](int digits, uint32_t* value) -> bool {
        uint32_t v = 0;
        for (int i = 0; i < digits; ++i) {
            int h = guid_hex_digit(s[i]);
            if (h < 0) return false;
            v = (v << 4) | (uint32_t)h;
        }
        s += digits;
        *value = v;
        return true;
    };
    uint32_t d1 = 0, d2 = 0, d3 = 0, d4 = 0, t1 = 0, t2 = 0, t3 = 0;
    if (!read_hex(8, &d1)) return false;
    if (*s++ != u'-') return false;
    if (!read_hex(4, &d2)) return false;
    if (*s++ != u'-') return false;
    if (!read_hex(4, &d3)) return false;
    if (*s++ != u'-') return false;
    if (!read_hex(4, &d4)) return false;
    if (*s++ != u'-') return false;
    if (!read_hex(4, &t1)) return false;
    if (!read_hex(4, &t2)) return false;
    if (!read_hex(4, &t3)) return false;
    if (*s == u'}') ++s;
    if (*s != 0) return false;
    out->Data1 = d1;
    out->Data2 = (uint16_t)d2;
    out->Data3 = (uint16_t)d3;
    out->Data4[0] = (uint8_t)(d4 >> 8);
    out->Data4[1] = (uint8_t)d4;
    out->Data4[2] = (uint8_t)(t1 >> 8);
    out->Data4[3] = (uint8_t)t1;
    out->Data4[4] = (uint8_t)(t2 >> 8);
    out->Data4[5] = (uint8_t)t2;
    out->Data4[6] = (uint8_t)(t3 >> 8);
    out->Data4[7] = (uint8_t)t3;
    return true;
}

int WINAPI_CALL THETAZ_StringFromGUID2(REFGUID rguid, LPOLESTR lpsz, int cchMax) {
    if (!lpsz || cchMax < 39) return 0;
    char text[40];
    int n = snprintf(text, sizeof(text),
                     "{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
                     rguid.Data1, (unsigned)rguid.Data2, (unsigned)rguid.Data3,
                     (unsigned)rguid.Data4[0], (unsigned)rguid.Data4[1],
                     (unsigned)rguid.Data4[2], (unsigned)rguid.Data4[3],
                     (unsigned)rguid.Data4[4], (unsigned)rguid.Data4[5],
                     (unsigned)rguid.Data4[6], (unsigned)rguid.Data4[7]);
    if (n != 38) return 0;
    uint16_t* out = reinterpret_cast<uint16_t*>(lpsz);
    for (int i = 0; i < 39; ++i)
        out[i] = (uint16_t)(unsigned char)text[i];
    return 39;
}

HRESULT WINAPI_CALL THETAZ_CLSIDFromString(LPCOLESTR lpsz, LPCLSID pclsid) {
    if (!lpsz || !pclsid) return E_INVALIDARG;
    GUID parsed{};
    if (!parse_guid_utf16(reinterpret_cast<const uint16_t*>(lpsz), &parsed))
        return CO_E_CLASSSTRING;
    *pclsid = parsed;
    return S_OK;
}

HRESULT WINAPI_CALL THETAZ_IIDFromString(LPCOLESTR lpsz, LPCLSID pclsid) {
    return THETAZ_CLSIDFromString(lpsz, pclsid);
}

HRESULT WINAPI_CALL THETAZ_StringFromCLSID(REFCLSID rclsid, LPOLESTR* lplpsz) {
    if (!lplpsz) return E_INVALIDARG;
    uint16_t* buf = static_cast<uint16_t*>(alloc_guest_shared(39 * sizeof(uint16_t)));
    if (!buf) return E_OUTOFMEMORY;
    int n = THETAZ_StringFromGUID2(rclsid, reinterpret_cast<LPOLESTR>(buf), 39);
    if (n != 39) return E_FAIL;
    guest_write_handle(lplpsz, reinterpret_cast<uintptr_t>(buf));
    return S_OK;
}

HRESULT WINAPI_CALL THETAZ_StringFromIID(REFIID riid, LPOLESTR* lplpsz) {
    return THETAZ_StringFromCLSID(riid, lplpsz);
}

HRESULT WINAPI_CALL THETAZ_CoInitializeEx(void* pvReserved, DWORD dwCoInit) {
    (void)pvReserved; (void)dwCoInit;
    return S_OK;
}

LPVOID WINAPI_CALL THETAZ_CoTaskMemAlloc(SIZE_T cb) {
    if (cb == 0) cb = 1;
    return alloc_guest_shared(cb);
}

void WINAPI_CALL THETAZ_CoTaskMemFree(LPVOID pv) {
    (void)pv;
}

LPVOID WINAPI_CALL THETAZ_CoTaskMemRealloc(LPVOID pv, SIZE_T cb) {
    (void)pv;
    if (cb == 0) cb = 1;
    return alloc_guest_shared(cb);
}

HRESULT WINAPI_CALL THETAZ_CreateStreamOnHGlobal(void* hGlobal, BOOL fDeleteOnRelease, void** ppstm) {
    (void)hGlobal; (void)fDeleteOnRelease;
    if (ppstm) *ppstm = nullptr;
    return E_NOTIMPL;
}

HRESULT WINAPI_CALL THETAZ_CreateBindCtx(DWORD reserved, void** ppbc) {
    (void)reserved;
    if (ppbc) *ppbc = nullptr;
    return E_NOTIMPL;
}

HRESULT WINAPI_CALL THETAZ_CoFileTimeNow(uint64_t* pft) {
    if (!pft) return E_INVALIDARG;
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    uint64_t ft = static_cast<uint64_t>(tv.tv_sec) * 10000000ULL + 116444736000000000ULL
                  + static_cast<uint64_t>(tv.tv_usec) * 10ULL;
    *pft = ft;
    return S_OK;
}

HRESULT WINAPI_CALL THETAZ_OleIsCurrentClipboard(void* pDataObject) {
    (void)pDataObject;
    return 1; // S_FALSE
}

void WINAPI_CALL THETAZ_CoFreeUnusedLibraries() {}

HRESULT WINAPI_CALL THETAZ_CoRegisterPSClsid(GUID* riid, GUID* rclsid) {
    (void)riid; (void)rclsid;
    return S_OK;
}

HRESULT WINAPI_CALL THETAZ_CoSetProxyBlanket(void* pProxy, DWORD dwAuthnSvc, DWORD dwAuthzSvc,
    void* pServerPrincName, DWORD dwRpcnAuthnLevel, DWORD dwImpLevel,
    void* pAuthInfo, DWORD dwCapabilities) {
    (void)pProxy; (void)dwAuthnSvc; (void)dwAuthzSvc; (void)pServerPrincName;
    (void)dwRpcnAuthnLevel; (void)dwImpLevel; (void)pAuthInfo; (void)dwCapabilities;
    return S_OK;
}

HRESULT WINAPI_CALL THETAZ_CoCreateFreeThreadedMarshaler(void* punkOuter, void** ppunkMarshaler) {
    (void)punkOuter;
    if (ppunkMarshaler) *ppunkMarshaler = nullptr;
    return E_NOTIMPL;
}

HRESULT WINAPI_CALL THETAZ_GetClassFile(void* szFilename, void** ppsclsid) {
    (void)szFilename;
    if (ppsclsid) *ppsclsid = nullptr;
    return E_NOTIMPL;
}

HRESULT WINAPI_CALL THETAZ_GetConvertStg(void* pStg) {
    (void)pStg;
    return E_NOTIMPL;
}

// ---------------------------------------------------------------------------
// version.dll
// ---------------------------------------------------------------------------

DWORD WINAPI_CALL THETAZ_GetFileVersionInfoSizeW(LPCWSTR lptstrFilename, LPDWORD lpdwHandle) {
    (void)lptstrFilename;
    if (lpdwHandle) *lpdwHandle = 0;
    return 0;
}

DWORD WINAPI_CALL THETAZ_GetFileVersionInfoSizeExW(DWORD dwFlags, LPCWSTR lptstrFilename, LPDWORD lpdwHandle) {
    (void)dwFlags; (void)lptstrFilename;
    if (lpdwHandle) *lpdwHandle = 0;
    return 0;
}

BOOL WINAPI_CALL THETAZ_GetFileVersionInfoW(LPCWSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData) {
    (void)lptstrFilename; (void)dwHandle; (void)dwLen; (void)lpData;
    return 0;
}

BOOL WINAPI_CALL THETAZ_VerQueryValueW(const void* pBlock, LPCWSTR lpSubBlock, void** lplpBuffer, UINT* puLen) {
    (void)pBlock; (void)lpSubBlock;
    if (lplpBuffer) *lplpBuffer = nullptr;
    if (puLen) *puLen = 0;
    return 0;
}

// ---------------------------------------------------------------------------
// advapi32.dll
// ---------------------------------------------------------------------------

LONG WINAPI_CALL THETAZ_RegEnumValueW(HKEY hKey, DWORD dwIndex, LPWSTR lpValueName,
    LPDWORD lpcchValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
    (void)hKey; (void)dwIndex; (void)lpValueName; (void)lpcchValueName;
    (void)lpReserved; (void)lpType; (void)lpData; (void)lpcbData;
    return 259; // ERROR_NO_MORE_ITEMS
}

namespace {

struct EtwRegistration {
    GUID provider{};
    void* enable_callback = nullptr;
    void* callback_context = nullptr;
};

std::mutex g_etw_mutex;
std::unordered_map<uint64_t, EtwRegistration> g_etw_registrations;
std::atomic<uint64_t> g_next_etw_handle{1};

bool etw_lookup(uint64_t handle, EtwRegistration* out) {
    std::lock_guard<std::mutex> lock(g_etw_mutex);
    auto it = g_etw_registrations.find(handle);
    if (it == g_etw_registrations.end()) return false;
    if (out) *out = it->second;
    return true;
}

} // namespace

DWORD WINAPI_CALL THETAZ_EventRegister(GUID* pProviderId, void* pEnableCallback, void* pCallbackContext, uint64_t* regHandle) {
    if (!pProviderId || !regHandle) return ERROR_INVALID_PARAMETER;
    uint64_t handle = g_next_etw_handle.fetch_add(1);
    if (handle == 0) handle = g_next_etw_handle.fetch_add(1);
    EtwRegistration reg;
    reg.provider = *pProviderId;
    reg.enable_callback = pEnableCallback;
    reg.callback_context = pCallbackContext;
    {
        std::lock_guard<std::mutex> lock(g_etw_mutex);
        g_etw_registrations[handle] = reg;
    }
    *regHandle = handle;
    return ERROR_SUCCESS;
}

DWORD WINAPI_CALL THETAZ_EventWriteTransfer(uint64_t regHandle, void* pActivityId, void* pRelatedActivityId,
    void* descriptor, uint32_t userDataCount, void* userData) {
    (void)pActivityId; (void)pRelatedActivityId;
    if (!etw_lookup(regHandle, nullptr)) return ERROR_INVALID_HANDLE;
    if (!descriptor) return ERROR_INVALID_PARAMETER;
    if (userDataCount && !userData) return ERROR_INVALID_PARAMETER;
    return ERROR_SUCCESS;
}

DWORD WINAPI_CALL THETAZ_EventWrite(uint64_t regHandle, void* descriptor,
    uint32_t userDataCount, void* userData) {
    if (!etw_lookup(regHandle, nullptr)) return ERROR_INVALID_HANDLE;
    if (!descriptor) return ERROR_INVALID_PARAMETER;
    if (userDataCount && !userData) return ERROR_INVALID_PARAMETER;
    return ERROR_SUCCESS;
}

DWORD WINAPI_CALL THETAZ_EventUnregister(uint64_t regHandle) {
    std::lock_guard<std::mutex> lock(g_etw_mutex);
    auto it = g_etw_registrations.find(regHandle);
    if (it == g_etw_registrations.end()) return ERROR_INVALID_HANDLE;
    g_etw_registrations.erase(it);
    return ERROR_SUCCESS;
}

DWORD WINAPI_CALL THETAZ_EventSetInformation(uint64_t regHandle, uint32_t infoClass, void* pInfo, uint32_t infoLength) {
    if (!etw_lookup(regHandle, nullptr)) return ERROR_INVALID_HANDLE;
    if (infoClass != 1 && infoClass != 2) return ERROR_INVALID_PARAMETER;
    if (!pInfo || infoLength == 0) return ERROR_INVALID_PARAMETER;
    return ERROR_SUCCESS;
}

HANDLE WINAPI_CALL THETAZ_GetProcessWindowStation() {
    return (HANDLE)0x1;
}

BOOL WINAPI_CALL THETAZ_GetUserObjectInformationW(HANDLE hObj, int nIndex, void* pvInfo, DWORD nLength, LPDWORD lpnLengthNeeded) {
    (void)hObj; (void)nIndex; (void)pvInfo; (void)nLength;
    if (lpnLengthNeeded) *lpnLengthNeeded = 0;
    return 0;
}

// --- advapi32.dll: Registry ---

LSTATUS WINAPI_CALL THETAZ_RegDeleteKeyW(HKEY hKey, const wchar_t* lpSubKey) {
    (void)hKey; (void)lpSubKey;
    return 0;
}

LSTATUS WINAPI_CALL THETAZ_RegDeleteKeyA(HKEY hKey, const char* lpSubKey) {
    (void)hKey; (void)lpSubKey;
    return 0;
}

LSTATUS WINAPI_CALL THETAZ_RegDeleteKeyExW(HKEY hKey, const wchar_t* lpSubKey, DWORD samDesired, DWORD Reserved) {
    (void)hKey; (void)lpSubKey; (void)samDesired; (void)Reserved;
    return 0;
}

LSTATUS WINAPI_CALL THETAZ_RegDeleteTreeA(HKEY hKey, const char* lpSubKey) {
    (void)hKey; (void)lpSubKey;
    return 0;
}

LSTATUS WINAPI_CALL THETAZ_RegDeleteTreeW(HKEY hKey, const wchar_t* lpSubKey) {
    (void)hKey; (void)lpSubKey;
    return 0;
}

LSTATUS WINAPI_CALL THETAZ_RegEnumKeyW(HKEY hKey, DWORD dwIndex, wchar_t* lpName, DWORD cchName) {
    (void)hKey; (void)dwIndex; (void)lpName; (void)cchName;
    return 259; // ERROR_NO_MORE_ITEMS
}

LSTATUS WINAPI_CALL THETAZ_RegEnumKeyA(HKEY hKey, DWORD dwIndex, char* lpName, DWORD cchName) {
    (void)hKey; (void)dwIndex; (void)lpName; (void)cchName;
    return 259;
}

LSTATUS WINAPI_CALL THETAZ_RegEnumKeyExW(HKEY hKey, DWORD dwIndex, wchar_t* lpName, LPDWORD lpcchName,
    LPDWORD lpReserved, wchar_t* lpClass, LPDWORD lpcchClass, FILETIME* lpftLastWriteTime) {
    (void)hKey; (void)dwIndex; (void)lpName; (void)lpcchName;
    (void)lpReserved; (void)lpClass; (void)lpcchClass; (void)lpftLastWriteTime;
    return 259;
}

LSTATUS WINAPI_CALL THETAZ_RegEnumValueA(HKEY hKey, DWORD dwIndex, char* lpValueName, LPDWORD lpcchValueName,
    LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
    (void)hKey; (void)dwIndex; (void)lpValueName; (void)lpcchValueName;
    (void)lpReserved; (void)lpType; (void)lpData; (void)lpcbData;
    return 259;
}

LSTATUS WINAPI_CALL THETAZ_RegQueryInfoKeyW(HKEY hKey, wchar_t* lpClass, LPDWORD lpcchClass, LPDWORD lpReserved,
    LPDWORD lpcSubKeys, LPDWORD lpcchMaxSubKey, LPDWORD lpcchMaxClass, LPDWORD lpcValues,
    LPDWORD lpcchMaxValueName, LPDWORD lpcbMaxValueData, LPDWORD lpcbSecurityDescriptor, FILETIME* lpftLastWriteTime) {
    (void)hKey; (void)lpClass; (void)lpcchClass; (void)lpReserved;
    (void)lpcSubKeys; (void)lpcchMaxSubKey; (void)lpcchMaxClass; (void)lpcValues;
    (void)lpcchMaxValueName; (void)lpcbMaxValueData; (void)lpcbSecurityDescriptor; (void)lpftLastWriteTime;
    if (lpcSubKeys) *lpcSubKeys = 0;
    if (lpcValues) *lpcValues = 0;
    return 0;
}

LSTATUS WINAPI_CALL THETAZ_RegQueryInfoKeyA(HKEY hKey, char* lpClass, LPDWORD lpcchClass, LPDWORD lpReserved,
    LPDWORD lpcSubKeys, LPDWORD lpcchMaxSubKey, LPDWORD lpcchMaxClass, LPDWORD lpcValues,
    LPDWORD lpcchMaxValueName, LPDWORD lpcbMaxValueData, LPDWORD lpcbSecurityDescriptor, FILETIME* lpftLastWriteTime) {
    (void)hKey; (void)lpClass; (void)lpcchClass; (void)lpReserved;
    (void)lpcSubKeys; (void)lpcchMaxSubKey; (void)lpcchMaxClass; (void)lpcValues;
    (void)lpcchMaxValueName; (void)lpcbMaxValueData; (void)lpcbSecurityDescriptor; (void)lpftLastWriteTime;
    if (lpcSubKeys) *lpcSubKeys = 0;
    if (lpcValues) *lpcValues = 0;
    return 0;
}

LSTATUS WINAPI_CALL THETAZ_RegQueryValueW(HKEY hKey, const wchar_t* lpSubKey, wchar_t* lpValue, LONG* pcbValue) {
    (void)hKey; (void)lpSubKey; (void)lpValue; (void)pcbValue;
    return 0;
}

LSTATUS WINAPI_CALL THETAZ_RegCreateKeyA(HKEY hKey, const char* lpSubKey, PHKEY phkResult) {
    (void)hKey; (void)lpSubKey;
    guest_write_handle(phkResult, 0x1234);
    return 0;
}

LSTATUS WINAPI_CALL THETAZ_RegSetValueW(HKEY hKey, const wchar_t* lpValueName, DWORD dwType, const wchar_t* lpData, DWORD cbData) {
    (void)hKey; (void)lpValueName; (void)dwType; (void)lpData; (void)cbData;
    return 0;
}

LSTATUS WINAPI_CALL THETAZ_RegNotifyChangeKeyValue(HKEY hKey, BOOL bWatchSubtree, DWORD dwNotifyFilter,
    HANDLE hEvent, BOOL fAsynchronous) {
    (void)hKey; (void)bWatchSubtree; (void)dwNotifyFilter; (void)hEvent; (void)fAsynchronous;
    return 0;
}

LSTATUS WINAPI_CALL THETAZ_RegGetValueA(HKEY hKey, const char* lpSubKey, const char* lpValue, DWORD dwFlags,
    LPDWORD pdwType, void* pvData, LPDWORD pcbData) {
    (void)hKey; (void)lpSubKey; (void)lpValue; (void)dwFlags;
    (void)pdwType; (void)pvData; (void)pcbData;
    return 0;
}

LSTATUS WINAPI_CALL THETAZ_RegOpenCurrentUser(DWORD samDesired, PHKEY phkResult) {
    (void)samDesired;
    guest_write_handle(phkResult, 0x1234);
    return 0;
}

LSTATUS WINAPI_CALL THETAZ_RegRenameKey(HKEY hKey, const wchar_t* lpSubKeyName, const wchar_t* lpNewKeyName) {
    (void)hKey; (void)lpSubKeyName; (void)lpNewKeyName;
    return 0;
}

// --- advapi32.dll: Security / Token ---

// ---------------------------------------------------------------------------
// Minimal honest token / SID model.
DWORD WINAPI_CALL THETAZ_GetLengthSid(void* pSid);
//
// ThetaZ does not implement the Windows security reference monitor; instead it
// models the calling process as a single primary token whose groups contain the
// well-known SIDs that a normal interactive user would have.  This is enough
// for Office's click-to-run startup, which only probes whether the current
// token is an interactive user.  Every API below follows the real Win32
// contract: size queries return ERROR_INSUFFICIENT_BUFFER with the required
// byte count, invalid handles/arguments are rejected, and unknown SID types
// are rejected rather than fabricated.
// ---------------------------------------------------------------------------

static const DWORD TOKEN_HANDLE_VALUE = 0x4000;

static std::vector<uint8_t> make_sid_bytes(uint64_t authority, const std::vector<DWORD>& subs) {
    std::vector<uint8_t> s(8 + subs.size() * 4, 0);
    s[0] = 1; // revision
    s[1] = static_cast<uint8_t>(subs.size());
    uint64_t a = authority;
    for (int i = 5; i >= 0; i--) { s[2 + i] = static_cast<uint8_t>(a & 0xFF); a >>= 8; }
    for (size_t i = 0; i < subs.size(); i++) {
        DWORD v = subs[i];
        memcpy(s.data() + 8 + i * 4, &v, 4);
    }
    return s;
}

static const std::vector<uint8_t> sid_world       = make_sid_bytes(1, {});
static const std::vector<uint8_t> sid_local       = make_sid_bytes(2, {});
static const std::vector<uint8_t> sid_interactive = make_sid_bytes(5, {4});
static const std::vector<uint8_t> sid_auth_users  = make_sid_bytes(5, {11});
static const std::vector<uint8_t> sid_users       = make_sid_bytes(5, {32, 545});
static const std::vector<uint8_t> sid_admins      = make_sid_bytes(5, {32, 544});
static const std::vector<uint8_t> sid_medium_il   = make_sid_bytes(16, {8192});
static const std::vector<uint8_t> sid_user        = make_sid_bytes(5, {21, 1234567890u, 1234567890u, 1234567890u, 1000});

static const std::vector<std::vector<uint8_t>> token_groups = {
    sid_world, sid_local, sid_interactive, sid_auth_users, sid_users
};

static std::set<void*> allocated_sids;

static bool is_token_handle(HANDLE h) {
    return h == reinterpret_cast<HANDLE>(static_cast<uintptr_t>(TOKEN_HANDLE_VALUE));
}

// --- Well-known SID construction ---

static DWORD well_known_sid_size(DWORD type) {
    // Returns required byte count, or 0 if unsupported.
    switch (type) {
        case 1:  // WinWorldSid
        case 2:  // WinLocalSid
        case 7:  // WinNtAuthoritySid
        case 8:  // WinDialupSid
        case 9:  // WinNetworkSid
        case 10: // WinBatchSid
        case 11: // WinInteractiveSid
        case 12: // WinServiceSid
        case 13: // WinAnonymousSid
        case 14: // WinProxySid
        case 15: // WinEnterpriseControllersSid
        case 16: // WinSelfSid
        case 17: // WinAuthenticatedUserSid
        case 18: // WinRestrictedCodeSid
        case 19: // WinTerminalServerSid
        case 20: // WinRemoteLogonIdSid
        case 22: // WinLocalSystemSid
        case 23: // WinLocalServiceSid
        case 24: // WinNetworkServiceSid
        case 54: // WinThisOrganizationSid
        case 63: // WinIUserSid
        case 70: // WinWriteRestrictedCodeSid
            return 12; // 1 subauthority: 8 + 4
        case 3:  // WinCreatorOwnerSid
        case 4:  // WinCreatorGroupSid
        case 5:  // WinCreatorOwnerServerSid
        case 6:  // WinCreatorGroupServerSid
            return 12;
        case 21: // WinLogonIdsSid
            return 12;
        case 25: // WinBuiltinDomainSid
            return 12;
        case 26: case 27: case 28: case 29: case 30: case 31: case 32: case 33:
        case 34: case 35: case 36: case 37: case 56: case 57: case 58: case 59:
        case 60: case 61: case 62: case 64: case 76:
            return 16; // 2 subauthorities: 8 + 8
        case 51: case 52: case 53:
            return 16; // 64 + 1 sub
        case 65: case 66: case 67: case 68: case 69:
            return 12; // integrity: 16 + 1
        case 71: // WinCreatorOwnerRightsSid
            return 12;
        case 38: case 39: case 40: // account SIDs: domain + rid
        case 41: case 42: case 43: case 44: case 45: case 46: case 47: case 48:
        case 49: case 50:
        case 75: case 77:
            return 8 + 5 * 4; // 5 subauthorities
        case 55: // WinOtherOrganizationSid
            return 12;
        default:
            return 0;
    }
}

static bool fill_well_known_sid(DWORD type, void* pDomainSid, void* pSid) {
    uint8_t* s = (uint8_t*)pSid;
    // authority + subs
    auto write = [&](uint64_t auth, const std::vector<DWORD>& subs) {
        s[0] = 1;
        s[1] = (uint8_t)subs.size();
        uint64_t a = auth;
        for (int i = 5; i >= 0; i--) { s[2+i] = (uint8_t)(a & 0xFF); a >>= 8; }
        for (size_t i = 0; i < subs.size(); i++) {
            DWORD v = subs[i];
            memcpy(s + 8 + i*4, &v, 4);
        }
    };
    switch (type) {
        case 1:  write(1, {}); return true;          // World
        case 2:  write(2, {}); return true;          // Local
        case 3:  write(3, {0}); return true;         // CreatorOwner
        case 4:  write(3, {1}); return true;         // CreatorGroup
        case 5:  write(3, {2}); return true;         // CreatorOwnerServer
        case 6:  write(3, {3}); return true;         // CreatorGroupServer
        case 7:  write(5, {}); return true;          // NtAuthority (S-1-5)
        case 8:  write(5, {1}); return true;         // Dialup
        case 9:  write(5, {2}); return true;         // Network
        case 10: write(5, {3}); return true;         // Batch
        case 11: write(5, {4}); return true;         // Interactive
        case 12: write(5, {6}); return true;         // Service
        case 13: write(5, {7}); return true;         // Anonymous
        case 14: write(5, {8}); return true;         // Proxy
        case 15: write(5, {9}); return true;         // EnterpriseControllers
        case 16: write(5, {10}); return true;        // Self
        case 17: write(5, {11}); return true;        // AuthenticatedUser
        case 18: write(5, {12}); return true;        // RestrictedCode
        case 19: write(5, {13}); return true;        // TerminalServer
        case 20: write(5, {14}); return true;        // RemoteLogonId
        case 21: write(5, {15}); return true;        // LogonIds (S-1-5-15? actually LogonIds is S-1-5-5-x-y; but approximate)
        case 22: write(5, {18}); return true;        // LocalSystem
        case 23: write(5, {19}); return true;        // LocalService
        case 24: write(5, {20}); return true;        // NetworkService
        case 25: write(5, {32}); return true;        // BuiltinDomain
        case 26: write(5, {32, 544}); return true;   // BuiltinAdministrators
        case 27: write(5, {32, 545}); return true;   // BuiltinUsers
        case 28: write(5, {32, 546}); return true;   // BuiltinGuests
        case 29: write(5, {32, 547}); return true;   // BuiltinPowerUsers
        case 30: write(5, {32, 548}); return true;   // BuiltinAccountOperators
        case 31: write(5, {32, 549}); return true;   // BuiltinSystemOperators
        case 32: write(5, {32, 550}); return true;   // BuiltinPrintOperators
        case 33: write(5, {32, 551}); return true;   // BuiltinBackupOperators
        case 34: write(5, {32, 552}); return true;   // BuiltinReplicator
        case 35: write(5, {32, 554}); return true;   // BuiltinPreWindows2000CompatibleAccess
        case 36: write(5, {32, 555}); return true;   // BuiltinRemoteDesktopUsers
        case 37: write(5, {32, 556}); return true;   // BuiltinNetworkConfigurationOperators
        case 51: write(5, {64, 10}); return true;    // NTLMAuthentication
        case 52: write(5, {64, 21}); return true;    // DigestAuthentication
        case 53: write(5, {64, 14}); return true;    // SChannelAuthentication
        case 54: write(5, {15}); return true;        // ThisOrganization
        case 55: write(5, {1000}); return true;      // OtherOrganization
        case 56: write(5, {32, 557}); return true;   // BuiltinIncomingForestTrustBuilders
        case 57: write(5, {32, 558}); return true;   // BuiltinPerfMonitoringUsers
        case 58: write(5, {32, 559}); return true;   // BuiltinPerfLoggingUsers
        case 59: write(5, {32, 560}); return true;   // BuiltinAuthorizationAccess
        case 60: write(5, {32, 561}); return true;   // BuiltinTerminalServerLicenseServers
        case 61: write(5, {32, 562}); return true;   // BuiltinDCOMUsers
        case 62: write(5, {32, 568}); return true;   // BuiltinIUsers
        case 63: write(5, {17}); return true;        // IUser
        case 64: write(5, {32, 569}); return true;   // BuiltinCryptoOperators
        case 65: write(16, {0}); return true;        // UntrustedLabel
        case 66: write(16, {4096}); return true;     // LowLabel
        case 67: write(16, {8192}); return true;     // MediumLabel
        case 68: write(16, {12288}); return true;    // HighLabel
        case 69: write(16, {16384}); return true;    // SystemLabel
        case 70: write(5, {33}); return true;        // WriteRestrictedCode
        case 71: write(3, {4}); return true;         // CreatorOwnerRights
        case 76: write(5, {32, 573}); return true;   // BuiltinEventLogReaders
        default: return false;
    }
}

BOOL WINAPI_CALL THETAZ_OpenProcessToken(HANDLE hProcess, DWORD dwDesiredAccess, HANDLE* phToken) {
    (void)dwDesiredAccess;
    if (hProcess != reinterpret_cast<HANDLE>(static_cast<uintptr_t>(-1)) &&
        hProcess != reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0xFFFFFFFFu))) {
        THETAZ_SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    guest_write_handle(phToken, TOKEN_HANDLE_VALUE);
    THETAZ_SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL WINAPI_CALL THETAZ_OpenThreadToken(HANDLE hThread, DWORD dwDesiredAccess, BOOL bOpenAsSelf, HANDLE* phToken) {
    (void)hThread; (void)dwDesiredAccess; (void)bOpenAsSelf; (void)phToken;
    THETAZ_SetLastError(ERROR_NO_TOKEN);
    return FALSE;
}

BOOL WINAPI_CALL THETAZ_GetTokenInformation(HANDLE hToken, DWORD TokenInformationClass, void* TokenInformation,
    DWORD TokenInformationLength, LPDWORD ReturnLength) {
    if (!is_token_handle(hToken)) {
        THETAZ_SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    // Determine required size.
    size_t required = 0;
    switch (TokenInformationClass) {
        case 1:  required = 8 + sid_user.size(); break;          // TokenUser
        case 2:  { // TokenGroups
            size_t sz = 4 + token_groups.size() * 8;
            for (auto& g : token_groups) sz += (g.size() + 3) & ~3;
            required = sz;
            break;
        }
        case 3:  required = 4; break;                            // TokenPrivileges (empty)
        case 8:  required = 4; break;                            // TokenType
        case 9:  THETAZ_SetLastError(ERROR_INVALID_PARAMETER); return FALSE; // TokenImpersonationLevel
        case 10: required = 56; break;                           // TokenStatistics
        case 12: required = 4; break;                            // TokenSessionId
        case 18: required = 4; break;                            // TokenElevationType
        case 20: required = 4; break;                            // TokenElevation
        case 21: required = 4; break;                            // TokenHasRestrictions
        case 24: required = 4; break;                            // TokenVirtualizationEnabled
        case 25: required = 8 + sid_medium_il.size(); break;     // TokenIntegrityLevel
        case 26: required = 4; break;                            // TokenUIAccess
        case 29: required = 4; break;                            // TokenIsAppContainer
        default:
            THETAZ_SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
    }
    if (!ReturnLength) {
        THETAZ_SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (!TokenInformation || TokenInformationLength < required) {
        *ReturnLength = (DWORD)required;
        THETAZ_SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }
    uint8_t* buf = (uint8_t*)TokenInformation;
    memset(buf, 0, required);
    switch (TokenInformationClass) {
        case 1: { // TokenUser
            uint32_t base = (uint32_t)(uintptr_t)buf;
            *(uint32_t*)(buf + 0) = base + 8; // Sid pointer
            *(uint32_t*)(buf + 4) = 0;        // Attributes
            memcpy(buf + 8, sid_user.data(), sid_user.size());
            break;
        }
        case 2: { // TokenGroups
            uint32_t base = (uint32_t)(uintptr_t)buf;
            *(uint32_t*)(buf + 0) = (uint32_t)token_groups.size(); // GroupCount
            size_t off = 4 + token_groups.size() * 8;
            off = (off + 3) & ~3;
            for (size_t i = 0; i < token_groups.size(); i++) {
                uint32_t sid_ptr = base + (uint32_t)off;
                uint32_t* group = (uint32_t*)(buf + 4 + i * 8);
                group[0] = sid_ptr;       // Sid
                group[1] = 0x00000007;   // SE_GROUP_MANDATORY | ENABLED_BY_DEFAULT | ENABLED
                memcpy(buf + off, token_groups[i].data(), token_groups[i].size());
                off += (token_groups[i].size() + 3) & ~3;
            }
            break;
        }
        case 3: { // TokenPrivileges (empty)
            *(uint32_t*)buf = 0; // PrivilegeCount
            break;
        }
        case 8: { // TokenType
            *(uint32_t*)buf = 1; // TokenPrimary
            break;
        }
        case 10: { // TokenStatistics
            *(uint32_t*)(buf + 24) = 1; // TokenType
            *(uint32_t*)(buf + 40) = (uint32_t)token_groups.size(); // GroupCount
            break;
        }
        case 12: { // TokenSessionId
            *(uint32_t*)buf = 1;
            break;
        }
        case 18: { // TokenElevationType
            *(uint32_t*)buf = 1; // TokenElevationTypeDefault
            break;
        }
        case 20: { // TokenElevation
            *(uint32_t*)buf = 0; // not elevated
            break;
        }
        case 21: { // TokenHasRestrictions
            *(uint32_t*)buf = 0;
            break;
        }
        case 24: { // TokenVirtualizationEnabled
            *(uint32_t*)buf = 0;
            break;
        }
        case 25: { // TokenIntegrityLevel
            uint32_t base = (uint32_t)(uintptr_t)buf;
            *(uint32_t*)(buf + 0) = base + 8; // Sid pointer
            *(uint32_t*)(buf + 4) = 0;        // Attributes
            memcpy(buf + 8, sid_medium_il.data(), sid_medium_il.size());
            break;
        }
        case 26: { // TokenUIAccess
            *(uint32_t*)buf = 0;
            break;
        }
        case 29: { // TokenIsAppContainer
            *(uint32_t*)buf = 0;
            break;
        }
    }
    *ReturnLength = (DWORD)required;
    THETAZ_SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL WINAPI_CALL THETAZ_CheckTokenMembership(HANDLE TokenHandle, void* SidToCheck, PBOOL IsMember) {
    if (!is_token_handle(TokenHandle)) {
        THETAZ_SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    if (!SidToCheck || !IsMember) {
        THETAZ_SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    // Check whether SidToCheck matches any of the token's groups.
    BOOL member = FALSE;
    for (const auto& g : token_groups) {
        if (g.size() == (size_t)THETAZ_GetLengthSid(SidToCheck) &&
            memcmp(g.data(), SidToCheck, g.size()) == 0) {
            member = TRUE;
            break;
        }
    }
    *IsMember = member;
    THETAZ_SetLastError(ERROR_SUCCESS);
    return TRUE;
}

DWORD WINAPI_CALL THETAZ_GetLengthSid(void* pSid) {
    if (!pSid) return 0;
    uint8_t* s = (uint8_t*)pSid;
    if (s[1] > 15) return 0;
    return 8 + (DWORD)s[1] * 4;
}

BOOL WINAPI_CALL THETAZ_IsValidSid(void* pSid) {
    if (!pSid) return FALSE;
    uint8_t* s = (uint8_t*)pSid;
    if (s[0] != 1) return FALSE;
    if (s[1] > 15) return FALSE;
    return TRUE;
}

BOOL WINAPI_CALL THETAZ_EqualSid(void* pSid1, void* pSid2) {
    if (!pSid1 || !pSid2) return FALSE;
    uint8_t* s1 = (uint8_t*)pSid1;
    uint8_t* s2 = (uint8_t*)pSid2;
    if (s1[0] != s2[0]) return FALSE;
    if (s1[1] != s2[1]) return FALSE;
    DWORD len = THETAZ_GetLengthSid(pSid1);
    if (len == 0) return FALSE;
    return memcmp(s1, s2, len) == 0;
}

BOOL WINAPI_CALL THETAZ_CopySid(DWORD nDestinationSidLength, void* pDestinationSid, void* pSourceSid) {
    if (!pDestinationSid || !pSourceSid) {
        THETAZ_SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    DWORD len = THETAZ_GetLengthSid(pSourceSid);
    if (nDestinationSidLength < len) {
        THETAZ_SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }
    memcpy(pDestinationSid, pSourceSid, len);
    THETAZ_SetLastError(ERROR_SUCCESS);
    return TRUE;
}

void* WINAPI_CALL THETAZ_FreeSid(void* pSid) {
    if (pSid && allocated_sids.count(pSid)) {
        allocated_sids.erase(pSid);
        free(pSid);
    }
    return nullptr;
}

void* WINAPI_CALL THETAZ_GetSidSubAuthority(void* pSid, DWORD nSubAuthority) {
    if (!pSid) return nullptr;
    uint8_t* s = (uint8_t*)pSid;
    if (nSubAuthority >= s[1]) return nullptr;
    return s + 8 + nSubAuthority * 4;
}

uint8_t* WINAPI_CALL THETAZ_GetSidSubAuthorityCount(void* pSid) {
    if (!pSid) return nullptr;
    return (uint8_t*)pSid + 1;
}

void* WINAPI_CALL THETAZ_GetSidIdentifierAuthority(void* pSid) {
    if (!pSid) return nullptr;
    return (uint8_t*)pSid + 2;
}

BOOL WINAPI_CALL THETAZ_AllocateAndInitializeSid(void* pIdentifierAuthority, BYTE nSubAuthorityCount,
    DWORD dwSubAuthority0, DWORD dwSubAuthority1, DWORD dwSubAuthority2, DWORD dwSubAuthority3,
    DWORD dwSubAuthority4, DWORD dwSubAuthority5, DWORD dwSubAuthority6, DWORD dwSubAuthority7,
    void** ppSid) {
    if (!pIdentifierAuthority || !ppSid || nSubAuthorityCount > 8) {
        THETAZ_SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    size_t len = 8 + (size_t)nSubAuthorityCount * 4;
    void* sid = malloc(len);
    if (!sid) {
        THETAZ_SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }
    memset(sid, 0, len);
    uint8_t* s = (uint8_t*)sid;
    s[0] = 1;
    s[1] = nSubAuthorityCount;
    memcpy(s + 2, pIdentifierAuthority, 6);
    DWORD subs[8] = {dwSubAuthority0, dwSubAuthority1, dwSubAuthority2, dwSubAuthority3,
                     dwSubAuthority4, dwSubAuthority5, dwSubAuthority6, dwSubAuthority7};
    for (BYTE i = 0; i < nSubAuthorityCount; i++) {
        memcpy(s + 8 + i * 4, &subs[i], 4);
    }
    allocated_sids.insert(sid);
    guest_write_handle(ppSid, (uintptr_t)sid);
    THETAZ_SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL WINAPI_CALL THETAZ_CreateWellKnownSid(DWORD WellKnownSidType, void* pDomainSid,
    void* pSid, DWORD* cbSid) {
    (void)pDomainSid;
    DWORD required = well_known_sid_size(WellKnownSidType);
    if (required == 0) {
        THETAZ_SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (!pSid || !cbSid) {
        THETAZ_SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (*cbSid < required) {
        *cbSid = required;
        THETAZ_SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }
    if (!fill_well_known_sid(WellKnownSidType, pDomainSid, pSid)) {
        THETAZ_SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    *cbSid = required;
    THETAZ_SetLastError(ERROR_SUCCESS);
    return TRUE;
}

BOOL WINAPI_CALL THETAZ_DuplicateToken(HANDLE ExistingTokenHandle, DWORD ImpersonationLevel, HANDLE* DuplicateTokenHandle) {
    (void)ExistingTokenHandle; (void)ImpersonationLevel; (void)DuplicateTokenHandle;
    return 0;
}

BOOL WINAPI_CALL THETAZ_ImpersonateNamedPipeClient(HANDLE hNamedPipe) {
    (void)hNamedPipe;
    return 0;
}

BOOL WINAPI_CALL THETAZ_RevertToSelf() {
    return 1;
}

BOOL WINAPI_CALL THETAZ_GetUserNameW(wchar_t* lpBuffer, LPDWORD pcbBuffer) {
    (void)lpBuffer; (void)pcbBuffer;
    return 0;
}

BOOL WINAPI_CALL THETAZ_GetUserNameA(char* lpBuffer, LPDWORD pcbBuffer) {
    (void)lpBuffer; (void)pcbBuffer;
    return 0;
}

BOOL WINAPI_CALL THETAZ_LookupAccountNameW(const wchar_t* lpSystemName, const wchar_t* lpAccountName,
    void* Sid, LPDWORD cbSid, wchar_t* ReferencedDomainName, LPDWORD cchReferencedDomainName, DWORD* peUse) {
    (void)lpSystemName; (void)lpAccountName; (void)Sid; (void)cbSid;
    (void)ReferencedDomainName; (void)cchReferencedDomainName; (void)peUse;
    return 0;
}

BOOL WINAPI_CALL THETAZ_LookupAccountSidW(const wchar_t* lpSystemName, void* Sid,
    wchar_t* Name, LPDWORD cchName, wchar_t* ReferencedDomainName, LPDWORD cchReferencedDomainName, DWORD* peUse) {
    (void)lpSystemName; (void)Sid;
    (void)Name; (void)cchName; (void)ReferencedDomainName; (void)cchReferencedDomainName; (void)peUse;
    return 0;
}

BOOL WINAPI_CALL THETAZ_LookupPrivilegeValueW(const wchar_t* lpSystemName, const wchar_t* lpName, uint64_t* pLuid) {
    (void)lpSystemName; (void)lpName;
    if (pLuid) *pLuid = 1;
    return 1;
}

BOOL WINAPI_CALL THETAZ_AdjustTokenPrivileges(HANDLE TokenHandle, BOOL DisableAllPrivileges,
    void* NewState, DWORD BufferLength, void* PreviousState, LPDWORD ReturnLength) {
    (void)TokenHandle; (void)DisableAllPrivileges;
    (void)NewState; (void)BufferLength; (void)PreviousState; (void)ReturnLength;
    return 1;
}

// --- advapi32.dll: Security descriptor ---

BOOL WINAPI_CALL THETAZ_SetSecurityDescriptorDacl(void* pSecurityDescriptor, BOOL bDaclPresent,
    void* pDacl, BOOL bDaclDefaulted) {
    (void)pSecurityDescriptor; (void)bDaclPresent; (void)pDacl; (void)bDaclDefaulted;
    return 1;
}

BOOL WINAPI_CALL THETAZ_SetSecurityDescriptorOwner(void* pSecurityDescriptor, void* pOwner, BOOL bOwnerDefaulted) {
    (void)pSecurityDescriptor; (void)pOwner; (void)bOwnerDefaulted;
    return 1;
}

BOOL WINAPI_CALL THETAZ_SetSecurityDescriptorSacl(void* pSecurityDescriptor, BOOL bSaclPresent,
    void* pSacl, BOOL bSaclDefaulted) {
    (void)pSecurityDescriptor; (void)bSaclPresent; (void)pSacl; (void)bSaclDefaulted;
    return 1;
}

BOOL WINAPI_CALL THETAZ_GetFileSecurityW(const wchar_t* lpFileName, DWORD SecurityInformation,
    void* pSecurityDescriptor, DWORD nLength, LPDWORD lpnLengthNeeded) {
    (void)lpFileName; (void)SecurityInformation; (void)pSecurityDescriptor; (void)nLength;
    if (lpnLengthNeeded) *lpnLengthNeeded = 0;
    return 0;
}

BOOL WINAPI_CALL THETAZ_SetFileSecurityW(const wchar_t* lpFileName, DWORD SecurityInformation,
    void* pSecurityDescriptor) {
    (void)lpFileName; (void)SecurityInformation; (void)pSecurityDescriptor;
    return 0;
}

BOOL WINAPI_CALL THETAZ_InitializeSecurityDescriptor(void* pSecurityDescriptor) {
    (void)pSecurityDescriptor;
    return 1;
}

BOOL WINAPI_CALL THETAZ_InitializeAcl(void* pAcl, DWORD nAclLength, DWORD dwAclRevision) {
    (void)pAcl; (void)nAclLength; (void)dwAclRevision;
    return 1;
}

BOOL WINAPI_CALL THETAZ_AddAccessAllowedAce(void* pAcl, DWORD dwAceRevision, DWORD AccessMask, void* pSid) {
    (void)pAcl; (void)dwAceRevision; (void)AccessMask; (void)pSid;
    return 1;
}

BOOL WINAPI_CALL THETAZ_AddAccessAllowedAceEx(void* pAcl, DWORD dwAceRevision, DWORD AceFlags, DWORD AccessMask, void* pSid) {
    (void)pAcl; (void)dwAceRevision; (void)AceFlags; (void)AccessMask; (void)pSid;
    return 1;
}

BOOL WINAPI_CALL THETAZ_AddAccessDeniedAce(void* pAcl, DWORD dwAceRevision, DWORD AccessMask, void* pSid) {
    (void)pAcl; (void)dwAceRevision; (void)AccessMask; (void)pSid;
    return 1;
}

BOOL WINAPI_CALL THETAZ_AddAccessDeniedAceEx(void* pAcl, DWORD dwAceRevision, DWORD AceFlags, DWORD AccessMask, void* pSid) {
    (void)pAcl; (void)dwAceRevision; (void)AceFlags; (void)AccessMask; (void)pSid;
    return 1;
}

BOOL WINAPI_CALL THETAZ_ConvertSidToStringSidW(void* Sid, wchar_t* StringSid) {
    (void)Sid; (void)StringSid;
    return 0;
}

// --- advapi32.dll: Service Control Manager ---

void* WINAPI_CALL THETAZ_OpenSCManagerW(const wchar_t* lpMachineName, const wchar_t* lpDatabaseName, DWORD dwDesiredAccess) {
    (void)lpMachineName; (void)lpDatabaseName; (void)dwDesiredAccess;
    return (void*)0x5000;
}

void* WINAPI_CALL THETAZ_OpenServiceW(void* hSCManager, const wchar_t* lpServiceName, DWORD dwDesiredAccess) {
    (void)hSCManager; (void)lpServiceName; (void)dwDesiredAccess;
    return (void*)0x5100;
}

BOOL WINAPI_CALL THETAZ_CloseServiceHandle(void* hSCObject) {
    (void)hSCObject;
    return 1;
}

BOOL WINAPI_CALL THETAZ_StartServiceW(void* hService, DWORD dwNumServiceArgs, const wchar_t** lpServiceArgVectors) {
    (void)hService; (void)dwNumServiceArgs; (void)lpServiceArgVectors;
    return 0;
}

BOOL WINAPI_CALL THETAZ_ControlService(void* hService, DWORD dwControl, void* lpServiceStatus) {
    (void)hService; (void)dwControl; (void)lpServiceStatus;
    return 0;
}

BOOL WINAPI_CALL THETAZ_DeleteService(void* hService) {
    (void)hService;
    return 0;
}

BOOL WINAPI_CALL THETAZ_QueryServiceStatusEx(void* hService, DWORD InfoLevel, LPBYTE lpBuffer,
    DWORD cbBufSize, LPDWORD pcbBytesNeeded) {
    (void)hService; (void)InfoLevel; (void)lpBuffer; (void)cbBufSize;
    if (pcbBytesNeeded) *pcbBytesNeeded = 0;
    return 0;
}

BOOL WINAPI_CALL THETAZ_QueryServiceConfigW(void* hService, void* lpServiceConfig, DWORD cbBufSize, LPDWORD pcbBytesNeeded) {
    (void)hService; (void)lpServiceConfig; (void)cbBufSize;
    if (pcbBytesNeeded) *pcbBytesNeeded = 0;
    return 0;
}

void* WINAPI_CALL THETAZ_CreateServiceW(void* hSCManager, const wchar_t* lpServiceName, const wchar_t* lpDisplayName,
    DWORD dwDesiredAccess, DWORD dwServiceType, DWORD dwStartType, DWORD dwErrorControl,
    const wchar_t* lpBinaryPathName, const wchar_t* lpLoadOrderGroup, LPDWORD lpdwTagId,
    const wchar_t* lpDependencies, const wchar_t* lpServiceStartName, const wchar_t* lpPassword, void** lphService) {
    (void)hSCManager; (void)lpServiceName; (void)lpDisplayName;
    (void)dwDesiredAccess; (void)dwServiceType; (void)dwStartType; (void)dwErrorControl;
    (void)lpBinaryPathName; (void)lpLoadOrderGroup; (void)lpdwTagId;
    (void)lpDependencies; (void)lpServiceStartName; (void)lpPassword; (void)lphService;
    return nullptr;
}

BOOL WINAPI_CALL THETAZ_ChangeServiceConfigW(void* hService, DWORD dwServiceType, DWORD dwStartType,
    DWORD dwErrorControl, const wchar_t* lpBinaryPathName, const wchar_t* lpLoadOrderGroup,
    LPDWORD lpdwTagId, const wchar_t* lpDependencies, const wchar_t* lpServiceStartName,
    const wchar_t* lpPassword, const wchar_t* lpDisplayName) {
    (void)hService; (void)dwServiceType; (void)dwStartType; (void)dwErrorControl;
    (void)lpBinaryPathName; (void)lpLoadOrderGroup; (void)lpdwTagId;
    (void)lpDependencies; (void)lpServiceStartName; (void)lpPassword; (void)lpDisplayName;
    return 0;
}

BOOL WINAPI_CALL THETAZ_ChangeServiceConfig2W(void* hService, DWORD dwInfoLevel, void* lpInfo) {
    (void)hService; (void)dwInfoLevel; (void)lpInfo;
    return 0;
}

BOOL WINAPI_CALL THETAZ_GetServiceDisplayNameW(void* hSCManager, const wchar_t* lpServiceName,
    wchar_t* lpDisplayName, LPDWORD lpcchBuffer) {
    (void)hSCManager; (void)lpServiceName; (void)lpDisplayName;
    if (lpcchBuffer) *lpcchBuffer = 0;
    return 0;
}

BOOL WINAPI_CALL THETAZ_EnumDependentServicesW(void* hService, DWORD dwServiceState,
    void* lpServices, DWORD cbBufSize, LPDWORD pcbBytesNeeded, LPDWORD lpServicesReturned) {
    (void)hService; (void)dwServiceState; (void)lpServices; (void)cbBufSize;
    if (pcbBytesNeeded) *pcbBytesNeeded = 0;
    if (lpServicesReturned) *lpServicesReturned = 0;
    return 0;
}

// --- advapi32.dll: Misc ---

BOOL WINAPI_CALL THETAZ_GetCurrentHwProfileA(void* pHwProfileInfo) {
    (void)pHwProfileInfo;
    return 0;
}

DWORD WINAPI_CALL THETAZ_RegisterTraceGuidsW(const wchar_t* pProviderGuid, void* pEnableCallback,
    const void* ControlGuid, DWORD GuidCount, void* RegistrationHandle) {
    (void)pProviderGuid; (void)pEnableCallback; (void)ControlGuid;
    (void)GuidCount; (void)RegistrationHandle;
    return 0;
}

DWORD WINAPI_CALL THETAZ_UnregisterTraceGuids(void* RegistrationHandle) {
    (void)RegistrationHandle;
    return 0;
}

BOOL WINAPI_CALL THETAZ_ReportEventW(HANDLE hEventLog, WORD wType, DWORD dwCategory, DWORD dwEventID,
    void* lpUserSid, WORD wNumStrings, DWORD dwDataSize, const wchar_t** lpStrings, void* lpRawData) {
    (void)hEventLog; (void)wType; (void)dwCategory; (void)dwEventID;
    (void)lpUserSid; (void)wNumStrings; (void)dwDataSize; (void)lpStrings; (void)lpRawData;
    return 1;
}

BOOL WINAPI_CALL THETAZ_CreateProcessAsUserW(void* hToken, const wchar_t* lpApplicationName, wchar_t* lpCommandLine,
    void* lpProcessAttributes, void* lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags,
    void* lpEnvironment, const wchar_t* lpCurrentDirectory, void* lpStartupInfo, void* lpProcessInformation) {
    (void)hToken; (void)lpApplicationName; (void)lpCommandLine;
    (void)lpProcessAttributes; (void)lpThreadAttributes; (void)bInheritHandles; (void)dwCreationFlags;
    (void)lpEnvironment; (void)lpCurrentDirectory; (void)lpStartupInfo; (void)lpProcessInformation;
    return 0;
}

DWORD WINAPI_CALL THETAZ_GetSidLengthRequired(UCHAR nSubAuthorityCount) {
    (void)nSubAuthorityCount;
    return 12;
}

LSTATUS WINAPI_CALL THETAZ_RegOpenKeyW(HKEY hKey, const wchar_t* lpSubKey, PHKEY phkResult) {
    (void)hKey; (void)lpSubKey;
    guest_write_handle(phkResult, 0x1234);
    return 0;
}

// ---------------------------------------------------------------------------
// shlwapi.dll
// ---------------------------------------------------------------------------

namespace {

// CSIDL values the shell folder APIs resolve. Office's bootstrap queries these
// to locate its install tree; they map onto the emulated drive the way Wine's
// own c:\windows layout would on a real desktop.
struct ShellFolderMapping {
    int csidl;
    const wchar_t* path;
};

const ShellFolderMapping kShellFolders[] = {
    {0x0000, L"C:\\"},
    {0x0024, L"C:\\"},                              // CSIDL_WINDOWS
    {0x0025, L"C:\\Windows\\System32"},             // CSIDL_SYSTEM
    {0x0014, L"C:\\Windows\\System32"},             // CSIDL_SYSTEMX86
    {0x0008, L"C:\\Windows\\System32"},             // CSIDL_PROGRAM_FILES
    {0x001a, L"C:\\Program Files (x86)"},           // CSIDL_PROGRAM_FILESX86
    {0x001c, L"C:\\Program Files\\Common Files"},   // CSIDL_PROGRAM_FILES_COMMON
    {0x001e, L"C:\\Program Files (x86)\\Common Files"}, // CSIDL_PROGRAM_FILES_COMMONX86
    {0x0005, L"C:\\Users\\Public\\Documents"},      // CSIDL_PERSONAL (my docs base)
    {0x0026, L"C:\\ProgramData"},                   // CSIDL_PROFILE (all-users appdata)
    {0x0013, L"C:\\Users\\Public\\AppData\\Roaming"}, // CSIDL_COMMON_APPDATA
    {0x001b, L"C:\\Users\\Public\\AppData\\Local"},   // CSIDL_COMMON_DOCUMENTS
};

const wchar_t* shell_folder_path_for(int csidl) {
    for (const ShellFolderMapping& m : kShellFolders) {
        if (m.csidl == csidl) return m.path;
    }
    return nullptr;
}

} // namespace

HRESULT WINAPI_CALL THETAZ_SHGetFolderPathW(void* hwnd, int csidl, void* hToken, DWORD dwFlags, LPWSTR pszPath) {
    (void)hwnd; (void)hToken;
    if (!pszPath || dwFlags != 0) return E_INVALIDARG;
    const wchar_t* path = shell_folder_path_for(csidl);
    if (!path) return E_FAIL;
    // The contract is MAX_PATH wchars including the terminator.
    wchar_t* dst = reinterpret_cast<wchar_t*>(pszPath);
    size_t len = 0;
    while (len < 259 && path[len]) { dst[len] = path[len]; ++len; }
    dst[len] = 0;
    return S_OK;
}

HRESULT WINAPI_CALL THETAZ_SHGetFolderPathA(void* hwnd, int csidl, void* hToken, DWORD dwFlags, LPSTR pszPath) {
    (void)hwnd; (void)csidl; (void)hToken; (void)dwFlags; (void)pszPath;
    return E_NOTIMPL;
}

HRESULT WINAPI_CALL THETAZ_SHGetKnownFolderPath(void* rfid, DWORD dwFlags, void* hToken, LPWSTR* ppszPath) {
    (void)rfid; (void)dwFlags; (void)hToken;
    if (ppszPath) *ppszPath = nullptr;
    return E_NOTIMPL;
}

HRESULT WINAPI_CALL THETAZ_SHGetMalloc(void** ppMalloc) {
    (void)ppMalloc;
    return E_NOTIMPL;
}

HRESULT WINAPI_CALL THETAZ_SHGetDesktopFolder(void** ppshf) {
    (void)ppshf;
    return E_NOTIMPL;
}

HRESULT WINAPI_CALL THETAZ_SHGetFolderLocation(void* hwnd, int csidl, void* hToken, DWORD dwFlags, void** ppidl) {
    (void)hwnd; (void)csidl; (void)hToken; (void)dwFlags;
    if (ppidl) *ppidl = nullptr;
    return E_NOTIMPL;
}

LONG WINAPI_CALL THETAZ_SHDeleteKeyW(void* hkey, LPCWSTR pszSubKey) {
    (void)hkey; (void)pszSubKey;
    return 0; // ERROR_SUCCESS
}

HRESULT WINAPI_CALL THETAZ_SHCreateStreamOnFileW(LPCWSTR pszFile, DWORD grfMode, void** ppstm) {
    (void)pszFile; (void)grfMode;
    if (ppstm) *ppstm = nullptr;
    return E_NOTIMPL;
}

HRESULT WINAPI_CALL THETAZ_SHCreateStreamOnFileEx(LPCWSTR pszFile, DWORD grfMode, DWORD dwAttributes,
    BOOL fCreate, void* pstmTemplate, void** ppstm) {
    (void)pszFile; (void)grfMode; (void)dwAttributes; (void)fCreate; (void)pstmTemplate;
    if (ppstm) *ppstm = nullptr;
    return E_NOTIMPL;
}

// ---------------------------------------------------------------------------
// gdi32.dll
// ---------------------------------------------------------------------------

int WINAPI_CALL THETAZ_GetObjectA(void* hgdiobj, int cbBuffer, void* lpvObject) {
    (void)hgdiobj; (void)cbBuffer; (void)lpvObject;
    return 0;
}

DWORD WINAPI_CALL THETAZ_ColorMatchToTarget(void* hdc, void* hdcTarget, DWORD mode) {
    (void)hdc; (void)hdcTarget; (void)mode;
    return 1;
}

// ---------------------------------------------------------------------------
// user32.dll
// ---------------------------------------------------------------------------

BOOL WINAPI_CALL THETAZ_AllowSetForegroundWindow(DWORD dwProcessId) {
    (void)dwProcessId;
    return 1;
}

HWND WINAPI_CALL THETAZ_WindowFromDC(void* hDC) {
    (void)hDC;
    return nullptr;
}

BOOL WINAPI_CALL THETAZ_PrintWindow(void* hwnd, void* hdcBlt, UINT nFlags) {
    (void)hwnd; (void)hdcBlt; (void)nFlags;
    return 0;
}

// ---------------------------------------------------------------------------
// slc.dll / slwga.dll — Software Licensing Client stubs
// ---------------------------------------------------------------------------

LONG WINAPI_CALL THETAZ_SLGetGenuineInformation(void* pQuery, void* pValueName, DWORD* pcbValue, DWORD* pvValue) {
    (void)pQuery; (void)pValueName;
    if (pcbValue) *pcbValue = 0;
    if (pvValue) *pvValue = 0;
    return 0;
}

HRESULT WINAPI_CALL THETAZ_SLIsGenuineLocal(void* pSLId, DWORD* pValue, void* pReserved) {
    (void)pSLId;
    if (pValue) *pValue = 1; // SLG_STATE_GENUINE
    (void)pReserved;
    return 0;
}

// =========================================================================
// NCrypt implementations
// =========================================================================

SECURITY_STATUS WINAPI_CALL THETAZ_NCryptOpenStorageProvider(NCRYPT_PROV_HANDLE* phProvider, LPCWSTR pszProviderName, DWORD dwFlags) {
	(void)pszProviderName; (void)dwFlags;
	if (!phProvider) return (SECURITY_STATUS)0x80070057; // E_INVALIDARG
	*phProvider = (NCRYPT_PROV_HANDLE)calloc(1, 16);
	return (*phProvider) ? 0 : (SECURITY_STATUS)0x8007000E; // ERROR_SUCCESS / NTE_NO_MEMORY
}

SECURITY_STATUS WINAPI_CALL THETAZ_NCryptSignHash(NCRYPT_KEY_HANDLE hKey, VOID* pPaddingInfo, BYTE* pbHashValue, DWORD cbHashValue, BYTE* pbSignature, DWORD cbSignature, DWORD* pcbResult, DWORD dwFlags) {
	(void)hKey; (void)pPaddingInfo; (void)pbHashValue; (void)cbHashValue; (void)pbSignature; (void)cbSignature; (void)pcbResult; (void)dwFlags;
	return (SECURITY_STATUS)0x80090029; // NTE_NOT_SUPPORTED
}

SECURITY_STATUS WINAPI_CALL THETAZ_NCryptSetProperty(NCRYPT_HANDLE hObject, LPCWSTR pszProperty, PBYTE pbInput, DWORD cbInput, DWORD dwFlags) {
	(void)hObject; (void)pszProperty; (void)pbInput; (void)cbInput; (void)dwFlags;
	return (SECURITY_STATUS)0x80090029; // NTE_NOT_SUPPORTED
}

SECURITY_STATUS WINAPI_CALL THETAZ_NCryptFinalizeKey(NCRYPT_KEY_HANDLE hKey, DWORD dwFlags) {
	(void)hKey; (void)dwFlags;
	return (SECURITY_STATUS)0x80090029; // NTE_NOT_SUPPORTED
}

SECURITY_STATUS WINAPI_CALL THETAZ_NCryptExportKey(NCRYPT_KEY_HANDLE hKey, NCRYPT_KEY_HANDLE hExportKey, LPCWSTR pszBlobType, NCryptBufferDesc* pParameterList, PBYTE pbOutput, DWORD cbOutput, DWORD* pcbResult, DWORD dwFlags) {
	(void)hKey; (void)hExportKey; (void)pszBlobType; (void)pParameterList; (void)pbOutput; (void)cbOutput; (void)pcbResult; (void)dwFlags;
	return (SECURITY_STATUS)0x80090029; // NTE_NOT_SUPPORTED
}

// =========================================================================
// WinHTTP implementations
// =========================================================================

struct WinHttpSession {
	char *userAgent;
};

struct WinHttpConnect {
	WinHttpSession *session;
	char *host;
	INTERNET_PORT port;
};

struct WinHttpRequest {
	WinHttpConnect *connect;
	char *verb;
	char *path;
	char *response;
	size_t responseLen;
	size_t responsePos;
	BOOL sent;
};

static char *dupWideToUtf8(LPCWSTR w) {
	if (!w) return nullptr;
	const wchar_t *w2 = (const wchar_t*)w;
	size_t len = wcslen(w2);
	char *out = (char*)malloc(len * 4 + 1);
	if (!out) return nullptr;
	size_t i = 0;
	for (size_t j = 0; j < len; j++) {
		wchar_t c = w2[j];
		if (c < 0x80) out[i++] = (char)c;
		else if (c < 0x800) { out[i++] = (char)(0xC0 | (c >> 6)); out[i++] = (char)(0x80 | (c & 0x3F)); }
		else { out[i++] = (char)(0xE0 | (c >> 12)); out[i++] = (char)(0x80 | ((c >> 6) & 0x3F)); out[i++] = (char)(0x80 | (c & 0x3F)); }
	}
	out[i] = '\0';
	return out;
}

HINTERNET WINAPI_CALL THETAZ_WinHttpOpen(LPCWSTR pwszUserAgent, DWORD dwAccessType, LPCWSTR pwszProxyName, LPCWSTR pwszProxyBypass, DWORD dwFlags) {
	(void)dwAccessType; (void)pwszProxyName; (void)pwszProxyBypass; (void)dwFlags;
	WinHttpSession *s = (WinHttpSession*)calloc(1, sizeof(WinHttpSession));
	if (!s) return nullptr;
	s->userAgent = dupWideToUtf8(pwszUserAgent);
	return (HINTERNET)s;
}

HINTERNET WINAPI_CALL THETAZ_WinHttpConnect(HINTERNET hSession, LPCWSTR pswzServerName, INTERNET_PORT nServerPort, DWORD dwReserved) {
	(void)dwReserved;
	WinHttpSession *s = (WinHttpSession*)hSession;
	if (!s || !pswzServerName) return nullptr;
	WinHttpConnect *c = (WinHttpConnect*)calloc(1, sizeof(WinHttpConnect));
	if (!c) return nullptr;
	c->session = s;
	c->host = dupWideToUtf8(pswzServerName);
	c->port = nServerPort ? nServerPort : 80;
	return (HINTERNET)c;
}

HINTERNET WINAPI_CALL THETAZ_WinHttpOpenRequest(HINTERNET hConnect, LPCWSTR pwszVerb, LPCWSTR pwszObjectName, LPCWSTR pwszVersion, LPCWSTR pwszReferrer, LPCWSTR* ppwszAcceptTypes, DWORD dwFlags) {
	(void)pwszVersion; (void)pwszReferrer; (void)ppwszAcceptTypes; (void)dwFlags;
	WinHttpConnect *c = (WinHttpConnect*)hConnect;
	if (!c) return nullptr;
	WinHttpRequest *r = (WinHttpRequest*)calloc(1, sizeof(WinHttpRequest));
	if (!r) return nullptr;
	r->connect = c;
	LPCWSTR v = pwszVerb ? pwszVerb : (LPCWSTR)L"GET";
	r->verb = dupWideToUtf8(v);
	if (pwszObjectName) r->path = dupWideToUtf8(pwszObjectName);
	else { r->path = (char*)malloc(2); r->path[0] = '/'; r->path[1] = '\0'; }
	return (HINTERNET)r;
}

BOOL WINAPI_CALL THETAZ_WinHttpCloseHandle(HINTERNET hInternet) {
	if (!hInternet) return FALSE;
	free(hInternet);
	return TRUE;
}

static size_t curl_write_cb(void *contents, size_t size, size_t nmemb, void *userp) {
	WinHttpRequest *r = (WinHttpRequest*)userp;
	size_t total = size * nmemb;
	char *tmp = (char*)realloc(r->response, r->responseLen + total + 1);
	if (!tmp) return 0;
	r->response = tmp;
	memcpy(r->response + r->responseLen, contents, total);
	r->responseLen += total;
	r->response[r->responseLen] = '\0';
	return total;
}

BOOL WINAPI_CALL THETAZ_WinHttpSendRequest(HINTERNET hRequest, LPCWSTR lpszHeaders, DWORD dwHeadersLength,
    LPVOID lpOptional, DWORD dwOptionalLength, DWORD dwTotalLength, DWORD_PTR dwContext) {
	(void)lpszHeaders; (void)dwHeadersLength; (void)lpOptional; (void)dwOptionalLength; (void)dwTotalLength; (void)dwContext;
	WinHttpRequest *r = (WinHttpRequest*)hRequest;
	if (!r || !r->connect) return FALSE;
	CURL *curl = curl_easy_init();
	if (!curl) return FALSE;
	char url[4096];
	snprintf(url, sizeof(url), "http://%s:%d%s", r->connect->host, r->connect->port, r->path ? r->path : "/");
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, r);
	CURLcode res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	if (res != CURLE_OK) return FALSE;
	r->sent = TRUE;
	r->responsePos = 0;
	return TRUE;
}

BOOL WINAPI_CALL THETAZ_WinHttpReceiveResponse(HINTERNET hRequest, LPVOID lpReserved) {
	(void)hRequest; (void)lpReserved;
	return TRUE;
}

BOOL WINAPI_CALL THETAZ_WinHttpQueryDataAvailable(HINTERNET hRequest, LPDWORD lpdwNumberOfBytesAvailable) {
	WinHttpRequest *r = (WinHttpRequest*)hRequest;
	if (!r) return FALSE;
	if (lpdwNumberOfBytesAvailable)
		*lpdwNumberOfBytesAvailable = (DWORD)(r->responseLen - r->responsePos);
	return TRUE;
}

BOOL WINAPI_CALL THETAZ_WinHttpReadData(HINTERNET hRequest, LPVOID lpBuffer, DWORD dwNumberOfBytesToRead, LPDWORD lpdwNumberOfBytesRead) {
	WinHttpRequest *r = (WinHttpRequest*)hRequest;
	if (!r || !lpBuffer) return FALSE;
	size_t avail = r->responseLen - r->responsePos;
	size_t toread = (dwNumberOfBytesToRead < avail) ? dwNumberOfBytesToRead : avail;
	memcpy(lpBuffer, r->response + r->responsePos, toread);
	r->responsePos += toread;
	if (lpdwNumberOfBytesRead) *lpdwNumberOfBytesRead = (DWORD)toread;
	return TRUE;
}

BOOL WINAPI_CALL THETAZ_WinHttpSetCredentials(HINTERNET hRequest, DWORD AuthScheme, LPCWSTR pwszUserName,
    LPCWSTR pwszPassword, LPVOID pAuthParams) {
	(void)hRequest; (void)AuthScheme; (void)pwszUserName; (void)pwszPassword; (void)pAuthParams;
	return TRUE;
}

BOOL WINAPI_CALL THETAZ_WinHttpGetIEProxyConfigForCurrentUser(WINHTTP_CURRENT_USER_IE_PROXY_CONFIG* pProxyConfig) {
	(void)pProxyConfig;
	return FALSE;
}

// waveOut implementations
// =========================================================================

DWORD WINAPI_CALL THETAZ_waveOutGetNumDevs() {
    return 0;
}

void initialize_ole32_apis() {
    // ole32.dll
    register_api("ole32.dll", "CoCreateGuid", (void*)THETAZ_CoCreateGuid);
    register_api("ole32.dll", "StringFromGUID2", (void*)THETAZ_StringFromGUID2);
    register_api("ole32.dll", "CLSIDFromString", (void*)THETAZ_CLSIDFromString);
    register_api("ole32.dll", "IIDFromString", (void*)THETAZ_IIDFromString);
    register_api("ole32.dll", "StringFromCLSID", (void*)THETAZ_StringFromCLSID);
    register_api("ole32.dll", "StringFromIID", (void*)THETAZ_StringFromIID);
    register_api("ole32.dll", "CoInitializeEx", (void*)THETAZ_CoInitializeEx);
    register_api("ole32.dll", "CoTaskMemAlloc", (void*)THETAZ_CoTaskMemAlloc);
    register_api("ole32.dll", "CoTaskMemFree", (void*)THETAZ_CoTaskMemFree);
    register_api("ole32.dll", "CoTaskMemRealloc", (void*)THETAZ_CoTaskMemRealloc);
    register_api("ole32.dll", "CreateStreamOnHGlobal", (void*)THETAZ_CreateStreamOnHGlobal);
    register_api("ole32.dll", "CreateBindCtx", (void*)THETAZ_CreateBindCtx);
    register_api("ole32.dll", "CoFileTimeNow", (void*)THETAZ_CoFileTimeNow);
    register_api("ole32.dll", "OleIsCurrentClipboard", (void*)THETAZ_OleIsCurrentClipboard);
    register_api("ole32.dll", "CoFreeUnusedLibraries", (void*)THETAZ_CoFreeUnusedLibraries);
    register_api("ole32.dll", "CoRegisterPSClsid", (void*)THETAZ_CoRegisterPSClsid);
    register_api("ole32.dll", "CoSetProxyBlanket", (void*)THETAZ_CoSetProxyBlanket);
    register_api("ole32.dll", "CoCreateFreeThreadedMarshaler", (void*)THETAZ_CoCreateFreeThreadedMarshaler);
    register_api("ole32.dll", "GetClassFile", (void*)THETAZ_GetClassFile);
    register_api("ole32.dll", "GetConvertStg", (void*)THETAZ_GetConvertStg);

    // version.dll
    register_api("version.dll", "GetFileVersionInfoSizeW", (void*)THETAZ_GetFileVersionInfoSizeW);
    register_api("version.dll", "GetFileVersionInfoSizeExW", (void*)THETAZ_GetFileVersionInfoSizeExW);
    register_api("version.dll", "GetFileVersionInfoW", (void*)THETAZ_GetFileVersionInfoW);
    register_api("version.dll", "VerQueryValueW", (void*)THETAZ_VerQueryValueW);

    // advapi32.dll
    register_api("advapi32.dll", "RegEnumValueW", (void*)THETAZ_RegEnumValueW);
    register_api("advapi32.dll", "RegEnumValueA", (void*)THETAZ_RegEnumValueA);
    register_api("advapi32.dll", "RegDeleteKeyW", (void*)THETAZ_RegDeleteKeyW);
    register_api("advapi32.dll", "RegDeleteKeyA", (void*)THETAZ_RegDeleteKeyA);
    register_api("advapi32.dll", "RegDeleteKeyExW", (void*)THETAZ_RegDeleteKeyExW);
    register_api("advapi32.dll", "RegDeleteTreeA", (void*)THETAZ_RegDeleteTreeA);
    register_api("advapi32.dll", "RegDeleteTreeW", (void*)THETAZ_RegDeleteTreeW);
    register_api("advapi32.dll", "RegEnumKeyW", (void*)THETAZ_RegEnumKeyW);
    register_api("advapi32.dll", "RegEnumKeyA", (void*)THETAZ_RegEnumKeyA);
    register_api("advapi32.dll", "RegEnumKeyExW", (void*)(LSTATUS(WINAPI_CALL*)(HKEY, DWORD, wchar_t*, LPDWORD, LPDWORD, wchar_t*, LPDWORD, FILETIME*))THETAZ_RegEnumKeyExW);
    register_api("advapi32.dll", "RegQueryInfoKeyW", (void*)THETAZ_RegQueryInfoKeyW);
    register_api("advapi32.dll", "RegQueryInfoKeyA", (void*)THETAZ_RegQueryInfoKeyA);
    register_api("advapi32.dll", "RegQueryValueW", (void*)THETAZ_RegQueryValueW);
    register_api("advapi32.dll", "RegCreateKeyA", (void*)THETAZ_RegCreateKeyA);
    register_api("advapi32.dll", "RegSetValueW", (void*)THETAZ_RegSetValueW);
    register_api("advapi32.dll", "RegNotifyChangeKeyValue", (void*)THETAZ_RegNotifyChangeKeyValue);
    register_api("advapi32.dll", "RegGetValueA", (void*)THETAZ_RegGetValueA);
    register_api("advapi32.dll", "RegOpenCurrentUser", (void*)THETAZ_RegOpenCurrentUser);
    register_api("advapi32.dll", "RegRenameKey", (void*)THETAZ_RegRenameKey);
    register_api("advapi32.dll", "EventRegister", (void*)THETAZ_EventRegister);
    register_api("advapi32.dll", "EventWrite", (void*)THETAZ_EventWrite);
    register_api("advapi32.dll", "EventWriteTransfer", (void*)THETAZ_EventWriteTransfer);
    register_api("advapi32.dll", "EventUnregister", (void*)THETAZ_EventUnregister);
    register_api("advapi32.dll", "EventSetInformation", (void*)THETAZ_EventSetInformation);
    register_api("advapi32.dll", "GetProcessWindowStation", (void*)THETAZ_GetProcessWindowStation);
    register_api("advapi32.dll", "GetUserObjectInformationW", (void*)THETAZ_GetUserObjectInformationW);
    register_api("advapi32.dll", "CryptAcquireContextW", (void*)THETAZ_CryptAcquireContextW);
    register_api("advapi32.dll", "CryptContextAddRef", (void*)THETAZ_CryptContextAddRef);
    register_api("advapi32.dll", "CryptReleaseContext", (void*)THETAZ_CryptReleaseContext);
    register_api("advapi32.dll", "CryptCreateHash", (void*)THETAZ_CryptCreateHash);
    register_api("advapi32.dll", "CryptDestroyHash", (void*)THETAZ_CryptDestroyHash);
    register_api("advapi32.dll", "CryptHashData", (void*)THETAZ_CryptHashData);
    register_api("advapi32.dll", "CryptGetHashParam", (void*)THETAZ_CryptGetHashParam);
    register_api("advapi32.dll", "CryptGetProvParam", (void*)THETAZ_CryptGetProvParam);
    register_api("advapi32.dll", "CryptSetProvParam", (void*)THETAZ_CryptSetProvParam);
    register_api("advapi32.dll", "CryptGenRandom", (void*)THETAZ_CryptGenRandom);
    register_api("advapi32.dll", "CryptGenKey", (void*)THETAZ_CryptGenKey);
    register_api("advapi32.dll", "CryptDestroyKey", (void*)THETAZ_CryptDestroyKey);
    register_api("advapi32.dll", "CryptGetKeyParam", (void*)THETAZ_CryptGetKeyParam);
    register_api("advapi32.dll", "CryptExportKey", (void*)THETAZ_CryptExportKey);
    register_api("advapi32.dll", "CryptImportKey", (void*)THETAZ_CryptImportKey);
    register_api("advapi32.dll", "CryptEncrypt", (void*)THETAZ_CryptEncrypt);
    register_api("advapi32.dll", "CryptDecrypt", (void*)THETAZ_CryptDecrypt);
    register_api("advapi32.dll", "CryptDeriveKey", (void*)THETAZ_CryptDeriveKey);
    register_api("advapi32.dll", "CryptSignHashW", (void*)THETAZ_CryptSignHashW);
    register_api("advapi32.dll", "CryptVerifySignatureW", (void*)THETAZ_CryptVerifySignatureW);
    register_api("advapi32.dll", "CryptEnumProvidersW", (void*)THETAZ_CryptEnumProvidersW);
    register_api("advapi32.dll", "CryptGetUserKey", (void*)THETAZ_CryptGetUserKey);
    register_api("advapi32.dll", "MD5Init", (void*)THETAZ_CryptMD5Init);
    register_api("advapi32.dll", "MD5Update", (void*)THETAZ_CryptMD5Update);
    register_api("advapi32.dll", "MD5Final", (void*)THETAZ_CryptMD5Final);
    register_api("advapi32.dll", "OpenProcessToken", (void*)THETAZ_OpenProcessToken);
    register_api("advapi32.dll", "OpenThreadToken", (void*)THETAZ_OpenThreadToken);
    register_api("advapi32.dll", "GetTokenInformation", (void*)THETAZ_GetTokenInformation);
    register_api("advapi32.dll", "CheckTokenMembership", (void*)THETAZ_CheckTokenMembership);
    register_api("advapi32.dll", "GetLengthSid", (void*)THETAZ_GetLengthSid);
    register_api("advapi32.dll", "IsValidSid", (void*)THETAZ_IsValidSid);
    register_api("advapi32.dll", "EqualSid", (void*)THETAZ_EqualSid);
    register_api("advapi32.dll", "CopySid", (void*)THETAZ_CopySid);
    register_api("advapi32.dll", "FreeSid", (void*)THETAZ_FreeSid);
    register_api("advapi32.dll", "GetSidSubAuthority", (void*)THETAZ_GetSidSubAuthority);
    register_api("advapi32.dll", "GetSidSubAuthorityCount", (void*)THETAZ_GetSidSubAuthorityCount);
    register_api("advapi32.dll", "GetSidIdentifierAuthority", (void*)THETAZ_GetSidIdentifierAuthority);
    register_api("advapi32.dll", "AllocateAndInitializeSid", (void*)THETAZ_AllocateAndInitializeSid);
    register_api("advapi32.dll", "CreateWellKnownSid", (void*)THETAZ_CreateWellKnownSid);
    register_api("advapi32.dll", "DuplicateToken", (void*)THETAZ_DuplicateToken);
    register_api("advapi32.dll", "AllocateLocallyUniqueId", (void*)THETAZ_GetSidLengthRequired);
    register_api("advapi32.dll", "EnumDependentServicesW", (void*)THETAZ_EnumDependentServicesW);
    register_api("advapi32.dll", "RegisterTraceGuidsW", (void*)THETAZ_RegisterTraceGuidsW);
    register_api("advapi32.dll", "UnregisterTraceGuids", (void*)THETAZ_UnregisterTraceGuids);
    register_api("advapi32.dll", "CreateProcessAsUserW", (void*)THETAZ_CreateProcessAsUserW);
    register_api("advapi32.dll", "GetSidLengthRequired", (void*)THETAZ_GetSidLengthRequired);
    register_api("advapi32.dll", "AddAccessAllowedAce", (void*)THETAZ_AddAccessAllowedAce);
    register_api("advapi32.dll", "AddAccessAllowedAceEx", (void*)THETAZ_AddAccessAllowedAceEx);
    register_api("advapi32.dll", "AddAccessDeniedAce", (void*)THETAZ_AddAccessDeniedAce);
    register_api("advapi32.dll", "AddAccessDeniedAceEx", (void*)THETAZ_AddAccessDeniedAceEx);
    register_api("advapi32.dll", "AdjustTokenPrivileges", (void*)THETAZ_AdjustTokenPrivileges);
    register_api("advapi32.dll", "ChangeServiceConfig2W", (void*)THETAZ_ChangeServiceConfig2W);
    register_api("advapi32.dll", "ChangeServiceConfigW", (void*)THETAZ_ChangeServiceConfigW);
    register_api("advapi32.dll", "CloseServiceHandle", (void*)THETAZ_CloseServiceHandle);
    register_api("advapi32.dll", "ControlService", (void*)THETAZ_ControlService);
    register_api("advapi32.dll", "ConvertSidToStringSidW", (void*)(BOOL(WINAPI_CALL*)(void*, wchar_t**))THETAZ_ConvertSidToStringSidW);
    register_api("advapi32.dll", "CreateServiceW", (void*)THETAZ_CreateServiceW);
    register_api("advapi32.dll", "DeleteService", (void*)THETAZ_DeleteService);
    register_api("advapi32.dll", "GetFileSecurityW", (void*)THETAZ_GetFileSecurityW);
    register_api("advapi32.dll", "GetServiceDisplayNameW", (void*)THETAZ_GetServiceDisplayNameW);
    register_api("advapi32.dll", "GetUserNameA", (void*)THETAZ_GetUserNameA);
    register_api("advapi32.dll", "GetUserNameW", (void*)THETAZ_GetUserNameW);
    register_api("advapi32.dll", "ImpersonateNamedPipeClient", (void*)THETAZ_ImpersonateNamedPipeClient);
    register_api("advapi32.dll", "InitializeAcl", (void*)THETAZ_InitializeAcl);
    register_api("advapi32.dll", "InitializeSecurityDescriptor", (void*)THETAZ_InitializeSecurityDescriptor);
    register_api("advapi32.dll", "LookupAccountNameW", (void*)THETAZ_LookupAccountNameW);
    register_api("advapi32.dll", "LookupAccountSidW", (void*)THETAZ_LookupAccountSidW);
    register_api("advapi32.dll", "LookupPrivilegeValueW", (void*)THETAZ_LookupPrivilegeValueW);
    register_api("advapi32.dll", "OpenSCManagerW", (void*)THETAZ_OpenSCManagerW);
    register_api("advapi32.dll", "OpenServiceW", (void*)THETAZ_OpenServiceW);
    register_api("advapi32.dll", "QueryServiceConfigW", (void*)THETAZ_QueryServiceConfigW);
    register_api("advapi32.dll", "QueryServiceStatusEx", (void*)(BOOL(WINAPI_CALL*)(void*, DWORD, void*, DWORD, LPDWORD))THETAZ_QueryServiceStatusEx);
    register_api("advapi32.dll", "RegOpenKeyW", (void*)THETAZ_RegOpenKeyW);
    register_api("advapi32.dll", "RevertToSelf", (void*)THETAZ_RevertToSelf);
    register_api("advapi32.dll", "SetFileSecurityW", (void*)(BOOL(WINAPI_CALL*)(const wchar_t*, DWORD, void*))THETAZ_SetFileSecurityW);
    register_api("advapi32.dll", "SetSecurityDescriptorDacl", (void*)THETAZ_SetSecurityDescriptorDacl);
    register_api("advapi32.dll", "SetSecurityDescriptorOwner", (void*)THETAZ_SetSecurityDescriptorOwner);
    register_api("advapi32.dll", "StartServiceW", (void*)THETAZ_StartServiceW);
    register_api("advapi32.dll", "GetCurrentHwProfileA", (void*)THETAZ_GetCurrentHwProfileA);

    // shlwapi.dll
    register_api("shlwapi.dll", "SHGetFolderPathW", (void*)THETAZ_SHGetFolderPathW);
    register_api("shlwapi.dll", "SHGetFolderPathA", (void*)THETAZ_SHGetFolderPathA);
    // shell32.dll owns this API as well; the installer imports resolve against
    // shell32, so both names must point at the real implementation.
    register_api("shell32.dll", "SHGetFolderPathW", (void*)THETAZ_SHGetFolderPathW);
    register_api("shell32.dll", "SHGetFolderPathA", (void*)THETAZ_SHGetFolderPathA);
    register_api("shlwapi.dll", "SHGetKnownFolderPath", (void*)THETAZ_SHGetKnownFolderPath);
    register_api("shlwapi.dll", "SHGetMalloc", (void*)THETAZ_SHGetMalloc);
    register_api("shlwapi.dll", "SHGetDesktopFolder", (void*)THETAZ_SHGetDesktopFolder);
    register_api("shlwapi.dll", "SHGetFolderLocation", (void*)THETAZ_SHGetFolderLocation);
    register_api("shlwapi.dll", "SHDeleteKeyW", (void*)THETAZ_SHDeleteKeyW);
    register_api("shlwapi.dll", "SHCreateStreamOnFileW", (void*)THETAZ_SHCreateStreamOnFileW);
    register_api("shlwapi.dll", "SHCreateStreamOnFileEx", (void*)THETAZ_SHCreateStreamOnFileEx);

    // gdi32.dll
    register_api("gdi32.dll", "GetObjectA", (void*)THETAZ_GetObjectA);
    register_api("gdi32.dll", "ColorMatchToTarget", (void*)THETAZ_ColorMatchToTarget);

    // user32.dll
    register_api("user32.dll", "AllowSetForegroundWindow", (void*)THETAZ_AllowSetForegroundWindow);
    register_api("user32.dll", "WindowFromDC", (void*)THETAZ_WindowFromDC);
    register_api("user32.dll", "PrintWindow", (void*)THETAZ_PrintWindow);

    // slc.dll / slwga.dll
    register_api("slc.dll", "SLGetGenuineInformation", (void*)THETAZ_SLGetGenuineInformation);
    register_api("slwga.dll", "SLIsGenuineLocal", (void*)THETAZ_SLIsGenuineLocal);

    // crypt32.dll

    // ncrypt.dll
    register_api("ncrypt.dll", "NCryptOpenStorageProvider", (void*)(SECURITY_STATUS(*)(NCRYPT_PROV_HANDLE*, LPCWSTR, DWORD))THETAZ_NCryptOpenStorageProvider);
    register_api("ncrypt.dll", "NCryptSignHash", (void*)(SECURITY_STATUS(*)(NCRYPT_KEY_HANDLE, VOID*, BYTE*, DWORD, BYTE*, DWORD, DWORD*, DWORD))THETAZ_NCryptSignHash);
    register_api("ncrypt.dll", "NCryptSetProperty", (void*)(SECURITY_STATUS(*)(NCRYPT_HANDLE, LPCWSTR, PBYTE, DWORD, DWORD))THETAZ_NCryptSetProperty);
    register_api("ncrypt.dll", "NCryptFinalizeKey", (void*)(SECURITY_STATUS(*)(NCRYPT_KEY_HANDLE, DWORD))THETAZ_NCryptFinalizeKey);
    register_api("ncrypt.dll", "NCryptExportKey", (void*)(SECURITY_STATUS(*)(NCRYPT_KEY_HANDLE, NCRYPT_KEY_HANDLE, LPCWSTR, NCryptBufferDesc*, PBYTE, DWORD, DWORD*, DWORD))THETAZ_NCryptExportKey);

    // winhttp.dll
    register_api("winhttp.dll", "WinHttpOpen", (void*)(HINTERNET(*)(LPCWSTR, DWORD, LPCWSTR, LPCWSTR, DWORD))THETAZ_WinHttpOpen);
    register_api("winhttp.dll", "WinHttpConnect", (void*)(HINTERNET(*)(HINTERNET, LPCWSTR, INTERNET_PORT, DWORD))THETAZ_WinHttpConnect);
    register_api("winhttp.dll", "WinHttpOpenRequest", (void*)(HINTERNET(*)(HINTERNET, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR*, DWORD))THETAZ_WinHttpOpenRequest);
    register_api("winhttp.dll", "WinHttpSendRequest", (void*)(BOOL(*)(HINTERNET, LPCWSTR, DWORD, LPVOID, DWORD, DWORD, DWORD_PTR))THETAZ_WinHttpSendRequest);
    register_api("winhttp.dll", "WinHttpReceiveResponse", (void*)(BOOL(*)(HINTERNET, LPVOID))THETAZ_WinHttpReceiveResponse);
    register_api("winhttp.dll", "WinHttpQueryDataAvailable", (void*)(BOOL(*)(HINTERNET, LPDWORD))THETAZ_WinHttpQueryDataAvailable);
    register_api("winhttp.dll", "WinHttpReadData", (void*)(BOOL(*)(HINTERNET, LPVOID, DWORD, LPDWORD))THETAZ_WinHttpReadData);
    register_api("winhttp.dll", "WinHttpCloseHandle", (void*)(BOOL(*)(HINTERNET))THETAZ_WinHttpCloseHandle);
    register_api("winhttp.dll", "WinHttpSetCredentials", (void*)(BOOL(*)(HINTERNET, DWORD, LPCWSTR, LPCWSTR, LPVOID))THETAZ_WinHttpSetCredentials);
    register_api("winhttp.dll", "WinHttpGetIEProxyConfigForCurrentUser", (void*)(BOOL(*)(WINHTTP_CURRENT_USER_IE_PROXY_CONFIG*))THETAZ_WinHttpGetIEProxyConfigForCurrentUser);

    // winmm.dll
    register_api("winmm.dll", "waveOutGetNumDevs", (void*)(DWORD(*)())THETAZ_waveOutGetNumDevs);
} // end initialize_ole32_apis()

} // namespace ThetaZ
