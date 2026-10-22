// ThetaZ — OLEAUT32 API declarations for cross-file access
// Copyright (c) 2026 ThetaZ Project. MIT License.

#pragma once

#include "core/api/api.h"

namespace ThetaZ {

// OLEAUT32 types and constants
using BSTR = uint16_t*;
using OLECHAR = uint16_t;

inline constexpr DWORD VT_BSTR = 8;

// BSTR functions
void free_bstr(BSTR bstr);
size_t bstr_byte_len(BSTR bstr);
BSTR WINAPI_CALL THETAZ_SysAllocString(const OLECHAR* psz);
BSTR WINAPI_CALL THETAZ_SysAllocStringLen(const OLECHAR* psz, UINT len);

// Variant functions
LONG WINAPI_CALL THETAZ_VariantCopy(void* pvargDest, const void* pvargSrc);
LONG WINAPI_CALL THETAZ_VariantCopyInd(void* pvargDest, const void* pvargSrc);

} // namespace ThetaZ