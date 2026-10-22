// ThetaZ — MediaFoundation (mfplat) API declarations
// Copyright (c) 2026 ThetaZ Project. MIT License.
//
// Minimal MediaFoundation stub for guest apps that import mfplat.dll.
// This is the Phase A skeleton (see docs/AGENTS.md §11): MFStartup,
// attribute store, sample/memory buffer, media type, and class factories.
// All decoding is deferred to a later phase (would call into GStreamer).

#pragma once

#include "core/api/api.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>

struct GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[8];
};

namespace ThetaZ {

// ============================================================================
// MediaFoundation HRESULT codes (subset — sufficient for stub implementation)
// ============================================================================
constexpr LONG MF_E_NOT_INITIALIZED        = 0xC00D36B0;
constexpr LONG MF_E_ALREADY_INITIALIZED    = 0xC00D36B1;
constexpr LONG MF_E_INVALID_FORMAT         = 0xC00D36C0;
constexpr LONG MF_E_INVALID_MEDIA_TYPE     = 0xC00D36C1;
constexpr LONG MF_E_INVALID_STREAM_DATA   = 0xC00D36CB;
constexpr LONG MF_E_UNSUPPORTED_BYTESTREAM_TYPE = 0xC00D3700;
constexpr LONG MF_E_UNSUPPORTED_REPRESENTATION = 0xC00D3701;
constexpr LONG MF_E_END_OF_STREAM          = 0xC00D3702;
constexpr LONG MF_E_TRANSFORM_TYPE_NOT_SET = 0xC00D3704;
constexpr LONG MF_E_TRANSFORM_INPUT_REMAINING = 0xC00D3705;
constexpr LONG MF_E_NO_MORE_TYPES          = 0xC00D3706;
constexpr LONG MF_E_TRANSFORM_NEED_MORE_INPUT = 0xC00D3707;
constexpr LONG MF_E_TRANSFORM_OUTPUT_REMAINING = 0xC00D3708;
constexpr LONG MF_E_NO_SAMPLE_DURATION     = 0xC00D3720;
constexpr LONG MF_E_NO_SAMPLE_TIMESTAMP    = 0xC00D3721;
constexpr LONG MF_E_SAMPLE_ALLOCATOR_EMPTY = 0xC00D380A;
constexpr LONG MF_E_NO_EVENTS_AVAILABLE    = 0xC00D3E80;

// MFStartup version
constexpr DWORD MF_VERSION = 0x00020070;  // MF_VERSION_WIN7

// MF_ATTRIBUTE_KEY identifiers used by MediaEngine (subset)
static const GUID MF_MEDIA_ENGINE_PLAYBACK_HWND = {
    0x2373a435, 0x9e4a, 0x4d99,
    { 0xa2, 0x0e, 0x8c, 0xcb, 0xa3, 0xa6, 0x4f, 0xd2 }
};

// ============================================================================
// IMFAttributes — implemented as a refcounted handle on the guest heap
// ============================================================================
// Layout on the guest heap (identity-mapped, all offsets < 4GB):
//   [0]  uint32_t refcount
//   [4]  uint32_t item_count
//   [8]  ptr to items array (each {guid, value_type, value_data})
//
// We don't expose the layout — we expose functions that read/write via
// handles. The MF API surface uses opaque pointers; we'll route all MF calls
// through these helpers.

struct MfAttrItem {
    uint64_t key_hi;    // first 8 bytes of GUID (x86 layout)
    uint64_t key_lo;    // second 8 bytes of GUID
    uint32_t vt;        // value type (MF_ATTRIBUTE_TYPE enum)
    // value follows inline (uint32, uint64, GUID, BSTR, IUnknown*)
    uint8_t  data[24];  // up to 24 bytes of payload (enough for GUID, UINT64, two U32)
};

// Guest-allocated attribute store
struct MfAttrStore {
    uint32_t refcount;
    uint32_t item_count;
    uint32_t items_off;   // offset in guest heap to items array
    uint32_t capacity;
};

// ============================================================================
// IMFMediaBuffer / IMFSample — minimal structs on guest heap
// ============================================================================
// IMFMediaBuffer:
//   [0]  refcount
//   [4]  current_length
//   [8]  max_length
//   [12] data_offset      // guest heap offset to bytes
//   [16] data_heap_offset // offset of allocated memory in guest heap

struct MfBuffer {
    uint32_t refcount;
    uint32_t current_length;
    uint32_t max_length;
    uint32_t data_offset;       // guest heap pointer to byte data
};

// IMFSample:
//   [0]  refcount
//   [4]  buffer_count
//   [8]  sample_time (100ns)
//   [16] sample_duration (100ns)
//   [20] buffer_off   // guest heap offset to MfBuffer*

struct MfSample {
    uint32_t refcount;
    uint32_t buffer_count;
    int64_t  sample_time;
    int64_t  sample_duration;
    uint32_t buffer_off;
};

// ============================================================================
// IMFMediaType — opaque pointer to MfMediaType on guest heap
// ============================================================================
struct MfMediaType {
    uint32_t refcount;
    uint64_t major_type_hi, major_type_lo;
    uint64_t subtype_hi, subtype_lo;
    uint32_t attr_off;   // embedded MfAttrStore for format-specific
};

// ============================================================================
// IMFMediaEngine / IMFMediaEngineClassFactory — opaque factories
// ============================================================================
// These are minimal stubs. A real implementation would:
//   1. Store the source URL
//   2. Build a GStreamer pipeline via gst_parse_launch
//   3. Pump frames into a texture
//   4. Forward IMFMediaEngineExtension callbacks to AVPro
// For now we just exist so apps that probe for the factory don't crash.

struct MfMediaEngine {
    uint32_t refcount;
    uint32_t extension_off;   // IMFMediaEngineExtension (or 0)
    uint32_t attr_off;        // engine attributes
    // The source URL would be stored as a BSTR; for the stub we keep it empty.
    uint32_t video_width, video_height;
    uint32_t playback_state;  // 0=Empty, 1=Ready, 2=Playing, 3=Paused, 4=Ended
};

struct MfMediaEngineClassFactory {
    uint32_t refcount;
};

struct MfMediaEngineExtension {
    uint32_t refcount;
    uint32_t sink_off;   // callback sink
};

// ============================================================================
// Exported functions — match mfplat.dll exports
// ============================================================================

// MFStartup / MFShutdown (kernel-mode init, but required by all MF apps)
HRESULT WINAPI_CALL THETAZ_MFStartup(ULONG version, DWORD flags);
HRESULT WINAPI_CALL THETAZ_MFShutdown();

// Attribute store
HRESULT WINAPI_CALL THETAZ_MFCreateAttributes(void** attributes, uint32_t initial_size);

// Memory buffer / sample / media type
HRESULT WINAPI_CALL THETAZ_MFCreateMemoryBuffer(DWORD max_length, void** buffer);
HRESULT WINAPI_CALL THETAZ_MFCreateSample(void** sample);
HRESULT WINAPI_CALL THETAZ_MFCreateMediaType(void** media_type);

// MediaEngine class factory
HRESULT WINAPI_CALL THETAZ_MFCreateMediaEngineClassFactory(void** class_factory);

// Source reader (stub — returns E_NOTIMPL)
HRESULT WINAPI_CALL THETAZ_MFCreateSourceReaderFromURL(const wchar_t* url, void* attributes, void** reader);
HRESULT WINAPI_CALL THETAZ_MFCreateSourceReaderFromByteStream(void* byte_stream, void* attributes, void** reader);

// IMFMediaEngineFactory::CreateInstance is what AVPro actually calls — we route it
// through MFCreateMediaEngineClassFactory above; this is the per-method entry
HRESULT WINAPI_CALL THETAZ_MFCreateMediaEngine(const wchar_t* url, void* attributes, void** media_engine);

// Activate helpers (used by mf.dll for audio renderer activation)
HRESULT WINAPI_CALL THETAZ_MFCreateAudioRendererActivate(void** activate);

void initialize_mfplat_layer();

} // namespace ThetaZ
