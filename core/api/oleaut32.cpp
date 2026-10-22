// ThetaZ — OLEAUT32 compatibility: BSTR / VARIANT core + error info.
// Several real apps (e.g. OfficeSetup) import OLEAUT32 by ordinal, so the
// ordinal exports are registered here as well as the names.
// Copyright (c) 2026 ThetaZ Project. MIT License.

#include "core/api/api.h"

#include <cstdlib>
#include <cstring>
#include <mutex>

// HRESULT constants (subset from winerror.h)
#define S_OK 0L
#define S_FALSE 1L
#define E_NOTIMPL 0x80004001L
#define E_INVALIDARG 0x80070057L
#define E_OUTOFMEMORY 0x8007000EL
#define E_FAIL 0x80004005L

#define FAILED(hr) ((hr) < 0)
#define SUCCEEDED(hr) ((hr) >= 0)

using ThetaZ::register_api;
using ThetaZ::register_api_ordinal;
using ThetaZ::alloc_guest_heap;
using ThetaZ::free_guest_heap;

namespace ThetaZ {

using VARIANT = uint8_t; // opaque 16-byte blob

static constexpr DWORD VT_EMPTY = 0;
constexpr DWORD VT_BSTR = 8;

int bstr_len(const OLECHAR* s) {
    if (!s) return 0;
    int n = 0;
    while (s[n]) n++;
    return n;
}

// BSTR layout: 4-byte byte-length prefix (excludes terminator), then the
// string data, then a null terminator. The returned pointer is the data.
// The whole block lives on the guest heap so the pointer is < 4GB and a
// 32-bit guest can dereference it.
static BSTR alloc_bstr(size_t char_len) {
    size_t bytes = char_len * sizeof(OLECHAR);
    void* mem = alloc_guest_heap(bytes + 4 + sizeof(OLECHAR));
    if (!mem) return nullptr;
    *reinterpret_cast<uint32_t*>(mem) = static_cast<uint32_t>(bytes);
    OLECHAR* data = reinterpret_cast<OLECHAR*>(static_cast<uint8_t*>(mem) + 4);
    data[char_len] = 0;
    return data;
}

void free_bstr(BSTR bstr) {
    if (!bstr) return;
    uint8_t* mem = reinterpret_cast<uint8_t*>(bstr) - 4;
    free_guest_heap(mem);
}

size_t bstr_byte_len(BSTR bstr) {
    if (!bstr) return 0;
    uint8_t* mem = reinterpret_cast<uint8_t*>(bstr) - 4;
    return *reinterpret_cast<uint32_t*>(mem);
}

// ---------------------------------------------------------------------------
// BSTR API
// ---------------------------------------------------------------------------

BSTR WINAPI_CALL THETAZ_SysAllocString(const OLECHAR* psz) {
    if (!psz) return nullptr;
    int len = bstr_len(psz);
    BSTR out = alloc_bstr(static_cast<size_t>(len));
    if (!out) return nullptr;
    for (int i = 0; i < len; i++) out[i] = psz[i];
    return out;
}

BSTR WINAPI_CALL THETAZ_SysAllocStringLen(const OLECHAR* psz, UINT len) {
    BSTR out = alloc_bstr(len);
    if (!out) return nullptr;
    if (psz) {
        int src_len = bstr_len(psz);
        for (UINT i = 0; i < len; i++) out[i] = i < static_cast<UINT>(src_len) ? psz[i] : 0;
    } else {
        for (UINT i = 0; i < len; i++) out[i] = 0;
    }
    return out;
}

void WINAPI_CALL THETAZ_SysFreeString(BSTR bstr) {
    free_bstr(bstr);
}

UINT WINAPI_CALL THETAZ_SysStringLen(BSTR bstr) {
    return static_cast<UINT>(bstr_byte_len(bstr) / sizeof(OLECHAR));
}

int WINAPI_CALL THETAZ_SysReAllocString(BSTR* pbstr, const OLECHAR* psz) {
    if (!pbstr) return 0;
    free_bstr(*pbstr);
    *pbstr = THETAZ_SysAllocString(psz);
    return *pbstr != nullptr ? 1 : 0;
}

int WINAPI_CALL THETAZ_SysReAllocStringLen(BSTR* pbstr, const OLECHAR* psz, UINT len) {
    if (!pbstr) return 0;
    free_bstr(*pbstr);
    *pbstr = THETAZ_SysAllocStringLen(psz, len);
    return *pbstr != nullptr ? 1 : 0;
}

// ---------------------------------------------------------------------------
// VARIANT core
// ---------------------------------------------------------------------------

void WINAPI_CALL THETAZ_VariantInit(void* pvarg) {
    if (pvarg) memset(pvarg, 0, 16);
}

LONG WINAPI_CALL THETAZ_VariantClear(void* pvarg) {
    if (!pvarg) return 0x80000005; // E_FAIL
    uint8_t* b = static_cast<uint8_t*>(pvarg);
    DWORD vt = 0;
    memcpy(&vt, b, 4);
    if (vt == VT_BSTR) {
        BSTR s = nullptr;
        memcpy(&s, b + 8, sizeof(BSTR));
        free_bstr(s);
    }
    memset(b, 0, 16);
    return 0; // S_OK
}

LONG WINAPI_CALL THETAZ_VariantCopy(void* pvargDest, const void* pvargSrc) {
    if (!pvargDest || !pvargSrc) return 0x80000005; // E_FAIL
    const uint8_t* src = static_cast<const uint8_t*>(pvargSrc);
    uint8_t* dst = static_cast<uint8_t*>(pvargDest);
    DWORD vt = 0;
    memcpy(&vt, src, 4);
    memcpy(dst, src, 16);
    if (vt == VT_BSTR) {
        BSTR s = nullptr;
        memcpy(&s, src + 8, sizeof(BSTR));
        BSTR copy = THETAZ_SysAllocString(s);
        memcpy(dst + 8, &copy, sizeof(BSTR));
    }
    return 0; // S_OK
}

LONG WINAPI_CALL THETAZ_VariantCopyInd(void* pvargDest, const void* pvargSrc) {
    return THETAZ_VariantCopy(pvargDest, pvargSrc);
}

LONG WINAPI_CALL THETAZ_VariantChangeType(void* pvargDest, const void* pvargSrc, WORD wFlags, DWORD vt) {
    (void)wFlags;
    (void)vt;
    return THETAZ_VariantCopy(pvargDest, pvargSrc);
}

// ---------------------------------------------------------------------------
// Error info
// ---------------------------------------------------------------------------

LONG WINAPI_CALL THETAZ_GetErrorInfo(DWORD dwReserved, void** pperrinfo) {
    (void)dwReserved;
    if (pperrinfo) *pperrinfo = nullptr;
    return 0; // S_OK
}

LONG WINAPI_CALL THETAZ_SetErrorInfo(DWORD dwReserved, void* perrinfo) {
    (void)dwReserved;
    (void)perrinfo;
    return 0; // S_OK
}

// ---------------------------------------------------------------------------
// SafeArray stubs
// ---------------------------------------------------------------------------

HRESULT WINAPI_CALL THETAZ_SafeArrayCreate(DWORD vt, UINT cDims, void* rgsabound) {
    (void)vt; (void)cDims; (void)rgsabound;
    return E_NOTIMPL;
}

HRESULT WINAPI_CALL THETAZ_SafeArrayDestroy(void* psa) {
    (void)psa;
    return E_NOTIMPL;
}

UINT WINAPI_CALL THETAZ_SafeArrayGetDim(void* psa) {
    (void)psa;
    return 0;
}

HRESULT WINAPI_CALL THETAZ_SafeArrayGetLBound(void* psa, UINT nDim, long* plLbound) {
    (void)psa; (void)nDim; (void)plLbound;
    return E_NOTIMPL;
}

// ---------------------------------------------------------------------------
// VarBstrFrom* stubs
// ---------------------------------------------------------------------------

HRESULT WINAPI_CALL THETAZ_VarBstrFromUI1(BYTE bIn, LCID lcid, unsigned short dwFlags, BSTR* pbstrOut) {
    (void)bIn; (void)lcid; (void)dwFlags;
    if (pbstrOut) *pbstrOut = nullptr;
    return E_NOTIMPL;
}

HRESULT WINAPI_CALL THETAZ_VarBstrFromI2(short iIn, LCID lcid, unsigned short dwFlags, BSTR* pbstrOut) {
    (void)iIn; (void)lcid; (void)dwFlags;
    if (pbstrOut) *pbstrOut = nullptr;
    return E_NOTIMPL;
}

HRESULT WINAPI_CALL THETAZ_VarBstrFromI4(long lIn, LCID lcid, unsigned short dwFlags, BSTR* pbstrOut) {
    (void)lIn; (void)lcid; (void)dwFlags;
    if (pbstrOut) *pbstrOut = nullptr;
    return E_NOTIMPL;
}

HRESULT WINAPI_CALL THETAZ_VarBstrFromR4(float fltIn, LCID lcid, unsigned short dwFlags, BSTR* pbstrOut) {
    (void)fltIn; (void)lcid; (void)dwFlags;
    if (pbstrOut) *pbstrOut = nullptr;
    return E_NOTIMPL;
}

HRESULT WINAPI_CALL THETAZ_VarBstrFromR8(double dblIn, LCID lcid, unsigned short dwFlags, BSTR* pbstrOut) {
    (void)dblIn; (void)lcid; (void)dwFlags;
    if (pbstrOut) *pbstrOut = nullptr;
    return E_NOTIMPL;
}

HRESULT WINAPI_CALL THETAZ_VarBstrFromI8(LONGLONG i64In, LCID lcid, unsigned short dwFlags, BSTR* pbstrOut) {
    (void)i64In; (void)lcid; (void)dwFlags;
    if (pbstrOut) *pbstrOut = nullptr;
    return E_NOTIMPL;
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

void initialize_oleaut32_layer() {
    static std::once_flag once;
    std::call_once(once, [] {
        register_api("oleaut32.dll", "SysAllocString", (void*)THETAZ_SysAllocString);
        register_api("oleaut32.dll", "SysAllocStringLen", (void*)THETAZ_SysAllocStringLen);
        register_api("oleaut32.dll", "SysFreeString", (void*)THETAZ_SysFreeString);
        register_api("oleaut32.dll", "SysStringLen", (void*)THETAZ_SysStringLen);
        register_api("oleaut32.dll", "SysReAllocString", (void*)THETAZ_SysReAllocString);
        register_api("oleaut32.dll", "SysReAllocStringLen", (void*)THETAZ_SysReAllocStringLen);
        register_api("oleaut32.dll", "VariantInit", (void*)THETAZ_VariantInit);
        register_api("oleaut32.dll", "VariantClear", (void*)THETAZ_VariantClear);
        register_api("oleaut32.dll", "VariantCopy", (void*)THETAZ_VariantCopy);
        register_api("oleaut32.dll", "VariantCopyInd", (void*)THETAZ_VariantCopyInd);
        register_api("oleaut32.dll", "VariantChangeType", (void*)THETAZ_VariantChangeType);
        register_api("oleaut32.dll", "GetErrorInfo", (void*)THETAZ_GetErrorInfo);
        register_api("oleaut32.dll", "SetErrorInfo", (void*)THETAZ_SetErrorInfo);

        register_api_ordinal("oleaut32.dll", 2, (void*)THETAZ_SysAllocString);
        register_api_ordinal("oleaut32.dll", 3, (void*)THETAZ_SysReAllocString);
        register_api_ordinal("oleaut32.dll", 4, (void*)THETAZ_SysAllocStringLen);
        register_api_ordinal("oleaut32.dll", 5, (void*)THETAZ_SysReAllocStringLen);
        register_api_ordinal("oleaut32.dll", 6, (void*)THETAZ_SysFreeString);
        register_api_ordinal("oleaut32.dll", 7, (void*)THETAZ_SysStringLen);
        register_api_ordinal("oleaut32.dll", 8, (void*)THETAZ_VariantInit);
        register_api_ordinal("oleaut32.dll", 9, (void*)THETAZ_VariantClear);
        register_api_ordinal("oleaut32.dll", 10, (void*)THETAZ_VariantCopy);
        register_api_ordinal("oleaut32.dll", 11, (void*)THETAZ_VariantCopyInd);
        register_api_ordinal("oleaut32.dll", 12, (void*)THETAZ_VariantChangeType);
        register_api_ordinal("oleaut32.dll", 200, (void*)THETAZ_GetErrorInfo);
        register_api_ordinal("oleaut32.dll", 201, (void*)THETAZ_SetErrorInfo);

        // Missing ordinals from WINWORD unresolved
        register_api_ordinal("oleaut32.dll", 17, (void*)THETAZ_SafeArrayCreate);
        register_api_ordinal("oleaut32.dll", 18, (void*)THETAZ_SafeArrayDestroy);
        register_api_ordinal("oleaut32.dll", 21, (void*)THETAZ_SafeArrayGetDim);
        register_api_ordinal("oleaut32.dll", 25, (void*)THETAZ_SafeArrayGetLBound);
        register_api_ordinal("oleaut32.dll", 149, (void*)THETAZ_VarBstrFromUI1);
        register_api_ordinal("oleaut32.dll", 152, (void*)THETAZ_VarBstrFromI2);
        register_api_ordinal("oleaut32.dll", 153, (void*)THETAZ_VarBstrFromI4);
        register_api_ordinal("oleaut32.dll", 155, (void*)THETAZ_VarBstrFromR4);
        register_api_ordinal("oleaut32.dll", 162, (void*)THETAZ_VarBstrFromR8);
        register_api_ordinal("oleaut32.dll", 176, (void*)THETAZ_VarBstrFromI8);
    });
}

} // namespace ThetaZ