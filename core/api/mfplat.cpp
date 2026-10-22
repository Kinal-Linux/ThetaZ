// ThetaZ — MediaFoundation (mfplat) implementation
// Copyright (c) 2026 ThetaZ Project. MIT License.
//
// Minimal stub implementation. See docs/AGENTS.md §11 for the full design.
// This file implements just enough for guest apps that import mfplat.dll
// to not crash on startup. Decoding is not yet implemented (Phase B).

#include "core/api/mfplat.h"

#include <cstring>
#include <mutex>

#ifndef S_OK
#define S_OK 0L
#endif
#ifndef S_FALSE
#define S_FALSE 1L
#endif
#ifndef E_NOTIMPL
#define E_NOTIMPL 0x80004001L
#endif
#ifndef E_INVALIDARG
#define E_INVALIDARG 0x80070057L
#endif
#ifndef E_OUTOFMEMORY
#define E_OUTOFMEMORY 0x8007000EL
#endif
#ifndef E_FAIL
#define E_FAIL 0x80004005L
#endif
#ifndef E_POINTER
#define E_POINTER 0x80004003L
#endif

namespace ThetaZ {

using namespace ThetaZ;

// Singleton initialization state (process-global — MF is a per-process thing)
static std::atomic<int> g_mf_init_count{0};
static std::mutex g_mf_mutex;

// ----------------------------------------------------------------------------
// MFStartup / MFShutdown
// ----------------------------------------------------------------------------
HRESULT WINAPI_CALL THETAZ_MFStartup(ULONG version, DWORD flags)
{
    (void)version;
    (void)flags;
    // Real implementation would validate version against MF_VERSION, set up
    // platform MF objects, etc. For the stub, just count the inits.
    int count = g_mf_init_count.fetch_add(1) + 1;
    return S_OK;
}

HRESULT WINAPI_CALL THETAZ_MFShutdown()
{
    int prev = g_mf_init_count.fetch_sub(1);
    if (prev <= 0) {
        g_mf_init_count.store(0);
        return MF_E_NOT_INITIALIZED;
    }
    return S_OK;
}

// ----------------------------------------------------------------------------
// MFCreateAttributes
// ----------------------------------------------------------------------------
HRESULT WINAPI_CALL THETAZ_MFCreateAttributes(void** attributes, uint32_t initial_size)
{
    if (!attributes) return E_POINTER;
    *attributes = nullptr;

    // Round capacity up to next power of two for allocation efficiency
    uint32_t cap = initial_size ? initial_size : 4;
    uint32_t items_bytes = cap * sizeof(MfAttrItem);

    MfAttrStore* store = (MfAttrStore*)alloc_guest_heap(sizeof(MfAttrStore));
    if (!store) return E_OUTOFMEMORY;

    MfAttrItem* items = (MfAttrItem*)alloc_guest_heap(items_bytes);
    if (!items) {
        free_guest_heap(store);
        return E_OUTOFMEMORY;
    }
    std::memset(items, 0, items_bytes);

    store->refcount    = 1;
    store->item_count  = 0;
    store->items_off   = (uint32_t)(uintptr_t)items;
    store->capacity    = cap;

    *attributes = (void*)(uintptr_t)store;
    return S_OK;
}

// ----------------------------------------------------------------------------
// MFCreateMemoryBuffer
// ----------------------------------------------------------------------------
HRESULT WINAPI_CALL THETAZ_MFCreateMemoryBuffer(DWORD max_length, void** buffer)
{
    if (!buffer) return E_POINTER;
    *buffer = nullptr;
    if (max_length == 0) return E_INVALIDARG;

    MfBuffer* mfb = (MfBuffer*)alloc_guest_heap(sizeof(MfBuffer));
    if (!mfb) return E_OUTOFMEMORY;

    void* data = alloc_guest_heap(max_length);
    if (!data) {
        free_guest_heap(mfb);
        return E_OUTOFMEMORY;
    }
    std::memset(data, 0, max_length);

    mfb->refcount       = 1;
    mfb->current_length = 0;
    mfb->max_length     = max_length;
    mfb->data_offset    = (uint32_t)(uintptr_t)data;

    *buffer = (void*)(uintptr_t)mfb;
    return S_OK;
}

// ----------------------------------------------------------------------------
// MFCreateSample
// ----------------------------------------------------------------------------
HRESULT WINAPI_CALL THETAZ_MFCreateSample(void** sample)
{
    if (!sample) return E_POINTER;
    *sample = nullptr;

    MfSample* mfs = (MfSample*)alloc_guest_heap(sizeof(MfSample));
    if (!mfs) return E_OUTOFMEMORY;

    // Allocate space for up to 4 buffer pointers
    uint32_t bufs_bytes = 4 * sizeof(uint32_t);
    uint32_t* bufs = (uint32_t*)alloc_guest_heap(bufs_bytes);
    if (!bufs) {
        free_guest_heap(mfs);
        return E_OUTOFMEMORY;
    }
    std::memset(bufs, 0, bufs_bytes);

    mfs->refcount        = 1;
    mfs->buffer_count    = 0;
    mfs->sample_time     = 0;
    mfs->sample_duration = 0;
    mfs->buffer_off      = (uint32_t)(uintptr_t)bufs;

    *sample = (void*)(uintptr_t)mfs;
    return S_OK;
}

// ----------------------------------------------------------------------------
// MFCreateMediaType
// ----------------------------------------------------------------------------
HRESULT WINAPI_CALL THETAZ_MFCreateMediaType(void** media_type)
{
    if (!media_type) return E_POINTER;
    *media_type = nullptr;

    MfMediaType* mfmt = (MfMediaType*)alloc_guest_heap(sizeof(MfMediaType));
    if (!mfmt) return E_OUTOFMEMORY;

    // Embedded attributes
    uint32_t items_off = (uint32_t)(uintptr_t)alloc_guest_heap(8 * sizeof(MfAttrItem));
    if (!items_off) {
        free_guest_heap(mfmt);
        return E_OUTOFMEMORY;
    }

    mfmt->refcount     = 1;
    mfmt->major_type_hi = mfmt->major_type_lo = 0;
    mfmt->subtype_hi    = mfmt->subtype_lo    = 0;
    mfmt->attr_off     = items_off;

    *media_type = (void*)(uintptr_t)mfmt;
    return S_OK;
}

// ----------------------------------------------------------------------------
// MFCreateMediaEngineClassFactory
// ----------------------------------------------------------------------------
HRESULT WINAPI_CALL THETAZ_MFCreateMediaEngineClassFactory(void** class_factory)
{
    if (!class_factory) return E_POINTER;
    *class_factory = nullptr;

    MfMediaEngineClassFactory* factory =
        (MfMediaEngineClassFactory*)alloc_guest_heap(sizeof(MfMediaEngineClassFactory));
    if (!factory) return E_OUTOFMEMORY;
    factory->refcount = 1;

    *class_factory = (void*)(uintptr_t)factory;
    return S_OK;
}

// ----------------------------------------------------------------------------
// MFCreateSourceReaderFromURL — stub returns E_NOTIMPL
// ----------------------------------------------------------------------------
HRESULT WINAPI_CALL THETAZ_MFCreateSourceReaderFromURL(const wchar_t* url, void* attributes, void** reader)
{
    (void)url; (void)attributes;
    if (!reader) return E_POINTER;
    *reader = nullptr;
    // Phase B will build a real SourceReader using GStreamer. For now,
    // return E_NOTIMPL so callers know the feature is missing rather than
    // silently failing later.
    return E_NOTIMPL;
}

HRESULT WINAPI_CALL THETAZ_MFCreateSourceReaderFromByteStream(void* byte_stream, void* attributes, void** reader)
{
    (void)byte_stream; (void)attributes;
    if (!reader) return E_POINTER;
    *reader = nullptr;
    return E_NOTIMPL;
}

// ----------------------------------------------------------------------------
// MFCreateMediaEngine — instantiates an engine directly (not via the factory)
// ----------------------------------------------------------------------------
HRESULT WINAPI_CALL THETAZ_MFCreateMediaEngine(const wchar_t* url, void* attributes, void** media_engine)
{
    if (!media_engine) return E_POINTER;
    *media_engine = nullptr;
    (void)url; (void)attributes;

    MfMediaEngine* engine =
        (MfMediaEngine*)alloc_guest_heap(sizeof(MfMediaEngine));
    if (!engine) return E_OUTOFMEMORY;
    std::memset(engine, 0, sizeof(*engine));
    engine->refcount = 1;
    engine->playback_state = 1; // MF_MEDIA_ENGINE_READY

    *media_engine = (void*)(uintptr_t)engine;
    return S_OK;
}

// ----------------------------------------------------------------------------
// MFCreateAudioRendererActivate — minimal activate object
// ----------------------------------------------------------------------------
HRESULT WINAPI_CALL THETAZ_MFCreateAudioRendererActivate(void** activate)
{
    if (!activate) return E_POINTER;
    *activate = nullptr;

    // Allocate a small opaque blob the caller will treat as IMFActivate.
    // The exact interface vtable isn't needed for the stub — apps will
    // query it, find no real implementation, and gracefully fall back.
    void* obj = alloc_guest_heap(64);
    if (!obj) return E_OUTOFMEMORY;
    std::memset(obj, 0, 64);
    *activate = obj;
    return S_OK;
}

void initialize_mfplat_layer()
{
    register_api("mfplat.dll", "MFStartup",                       (void*)THETAZ_MFStartup);
    register_api("mfplat.dll", "MFShutdown",                      (void*)THETAZ_MFShutdown);
    register_api("mfplat.dll", "MFCreateAttributes",              (void*)THETAZ_MFCreateAttributes);
    register_api("mfplat.dll", "MFCreateMemoryBuffer",            (void*)THETAZ_MFCreateMemoryBuffer);
    register_api("mfplat.dll", "MFCreateSample",                  (void*)THETAZ_MFCreateSample);
    register_api("mfplat.dll", "MFCreateMediaType",               (void*)THETAZ_MFCreateMediaType);
    register_api("mfplat.dll", "MFCreateAudioRendererActivate",   (void*)THETAZ_MFCreateAudioRendererActivate);
    register_api("mfplat.dll", "MFCreateSourceReaderFromURL",     (void*)THETAZ_MFCreateSourceReaderFromURL);
    register_api("mfplat.dll", "MFCreateSourceReaderFromByteStream", (void*)THETAZ_MFCreateSourceReaderFromByteStream);

    register_api("mfmediaengine.dll", "MFCreateMediaEngineClassFactory", (void*)THETAZ_MFCreateMediaEngineClassFactory);
    register_api("mfmediaengine.dll", "MFCreateMediaEngine",              (void*)THETAZ_MFCreateMediaEngine);

    register_api("mfreadwrite.dll", "MFCreateSourceReaderFromURL",     (void*)THETAZ_MFCreateSourceReaderFromURL);
    register_api("mfreadwrite.dll", "MFCreateSourceReaderFromByteStream", (void*)THETAZ_MFCreateSourceReaderFromByteStream);
}

} // namespace ThetaZ
