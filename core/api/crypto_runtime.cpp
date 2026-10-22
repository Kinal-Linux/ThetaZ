// ThetaZ — Crypto/CAPI API implementations using OpenSSL
// Copyright (c) 2026 ThetaZ Project. MIT License.

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/ec.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <openssl/pkcs7.h>
#include <openssl/objects.h>

#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <mutex>
#include <atomic>

// Forward declare crypto types (normally in crypto.h)
namespace ThetaZ {
using HCRYPTPROV = void*;
using HCRYPTHASH = void*;
using HCRYPTKEY = void*;
using NCRYPT_PROV_HANDLE = void*;
using NCRYPT_KEY_HANDLE = void*;
using NCRYPT_HANDLE = void*;
using NCryptBufferDesc = void*;
using PBYTE = uint8_t*;
using WINHTTP_CURRENT_USER_IE_PROXY_CONFIG = void*;
using HINTERNET = void*;
using INTERNET_PORT = uint16_t;
using LPCWSTR = const uint16_t*;
using DWORD = uint32_t;
using BOOL = int32_t;
using LPDWORD = DWORD*;
using SECURITY_STATUS = int32_t;
using VOID = void;
using BYTE = uint8_t;
using LPVOID = void*;
using LPCSTR = const char*;
using LPWSTR = uint16_t*;
using DWORD_PTR = uintptr_t;
using HCRYPTPROV = void*;
using HCRYPTHASH = void*;
using HCRYPTKEY = void*;
using LPDWORD = DWORD*;
using DWORD = uint32_t;
using BOOL = int32_t;
using VOID = void;
using BYTE = uint8_t;
using LPVOID = void*;
using LPCSTR = const char*;
using LPWSTR = uint16_t*;
using DWORD_PTR = uintptr_t;

// Crypto constants
enum CryptoObjectType {
    CRYPTO_OBJ_PROVIDER = 1,
    CRYPTO_OBJ_HASH = 2,
    CRYPTO_OBJ_KEY = 3,
};

enum {
    PROV_RSA_FULL = 1,
    PROV_RSA_SIG = 2,
    PROV_DSS = 3,
    PROV_FORTEZZA = 4,
    PROV_RSA_AES = 24,
};

// HRESULT / NTSTATUS constants
#ifndef E_NOTIMPL
#define E_NOTIMPL 0x80004001L
#endif

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#define STATUS_NOT_IMPLEMENTED ((NTSTATUS)0xC0000002L)
#define STATUS_UNSUCCESSFUL ((NTSTATUS)0xC0000001L)
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xC000000DL)
#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xC0000023L)
#define STATUS_INVALID_SIGNATURE ((NTSTATUS)0xC000A000L)
#endif

#ifndef HRESULT
#define HRESULT long
#endif

enum {
    CALG_MD5 = 32771,
    CALG_SHA1 = 32772,
    CALG_SHA_256 = 32780,
    CALG_SHA_384 = 32781,
    CALG_SHA_512 = 32782,
    CALG_RSA_KEYX = 41984,
    CALG_RSA_SIGN = 41985,
    CALG_AES_128 = 26115,
    CALG_AES_192 = 26116,
    CALG_AES_256 = 26117,
    CALG_DES = 26113,
    CALG_3DES_112 = 26114,
    CALG_3DES = 26118,
    CALG_RC2 = 26119,
    CALG_RC4 = 26625,
};

enum {
    AT_KEYEXCHANGE = 1,
    AT_SIGNATURE = 2,
};

enum {
    SIMPLEBLOB = 1,
    PUBLICKEYBLOB = 6,
    PRIVATEKEYBLOB = 7,
    PLAINTEXTKEYBLOB = 8,
    OPAQUEKEYBLOB = 9,
    PUBLICKEYBLOBEX = 10,
    SYMMETRICWRAPKEYBLOB = 11,
};

enum {
    CRYPT_VERIFYCONTEXT = 0xF0000000,
    CRYPT_NEWKEYSET = 0x00000008,
    CRYPT_DELETEKEYSET = 0x00000010,
    CRYPT_MACHINE_KEYSET = 0x00000020,
    CRYPT_SILENT = 0x00000040,
    CRYPT_DEFAULT_CONTAINER_OPTIONAL = 0x00000080,
};

enum {
    HP_HASHVAL = 2,
    HP_HASHSIZE = 4,
};

enum {
    KP_IV = 1,
    KP_SALT = 2,
    KP_PADDING = 3,
    KP_MODE = 4,
    KP_MODE_BITS = 5,
    KP_PERMISSIONS = 6,
    KP_ALGID = 7,
    KP_BLOCKLEN = 8,
    KP_KEYLEN = 9,
    KP_SALT_EX = 10,
    KP_P = 11,
    KP_Q = 12,
    KP_G = 13,
    KP_PUB_EXPONENT = 13,
    KP_PUB_PARAMS = 14,
    KP_TAG = 14,
    KP_PIN_ID = 15,
    KP_EFFECTIVE_KEYLEN = 15,
    KP_PIN = 16,
    KP_SCHANNEL_ALG = 16,
    KP_CLIENT_RANDOM = 17,
    KP_SERVER_RANDOM = 18,
    KP_RP = 18,
    KP_SIGNATURE_LENGTH = 19,
    KP_OID = 19,
    KP_SIGNATURE_ALG = 20,
    KP_ECDH_EPHEMERAL = 21,
    KP_ECDH_PUBLIC = 22,
};
} // namespace ThetaZ

#include "core/api/api.h"

namespace ThetaZ {



// =========================================================================
// Internal object structures
// =========================================================================


struct CryptoProvider {
    CryptoObjectType type = CRYPTO_OBJ_PROVIDER;
    uint64_t handle;
    EVP_PKEY_CTX* pkey_ctx = nullptr;
    uint32_t prov_type;
    DWORD flags;
};

struct CryptoHash {
    CryptoObjectType type = CRYPTO_OBJ_HASH;
    uint64_t handle;
    EVP_MD_CTX* ctx = nullptr;
    int alg_id;
};

struct CryptoKey {
    CryptoObjectType type = CRYPTO_OBJ_KEY;
    uint64_t handle;
    EVP_PKEY* pkey = nullptr;
    int alg_id;
    bool is_public;
};

// =========================================================================
// Internal state
// =========================================================================

static std::mutex g_crypto_mutex;
static std::unordered_map<uint64_t, CryptoProvider*> g_providers;
static std::unordered_map<uint64_t, CryptoHash*> g_hashes;
static std::unordered_map<uint64_t, CryptoKey*> g_keys;
static std::atomic<uint64_t> g_next_prov_id{0x1000};
static std::atomic<uint64_t> g_next_hash_id{0x2000};
static std::atomic<uint64_t> g_next_key_id{0x3000};

// =========================================================================
// Helper functions
// =========================================================================

static int algid_to_nid(DWORD algid) {
    switch (algid) {
        case CALG_MD5: return NID_md5;
        case CALG_SHA1: return NID_sha1;
        case CALG_SHA_256: return NID_sha256;
        case CALG_SHA_384: return NID_sha384;
        case CALG_SHA_512: return NID_sha512;
        case CALG_RSA_KEYX:
        case CALG_RSA_SIGN: return NID_rsaEncryption;
        case CALG_AES_128: return NID_aes_128_cbc;
        case CALG_AES_192: return NID_aes_192_cbc;
        case CALG_AES_256: return NID_aes_256_cbc;
        case CALG_DES: return NID_des_cbc;
        case CALG_3DES_112:
        case CALG_3DES: return NID_des_ede3_cbc;
        case CALG_RC4: return NID_rc4;
        default: return NID_undef;
    }
}

static const EVP_MD* algid_to_md(DWORD algid) {
    switch (algid) {
        case CALG_MD5: return EVP_md5();
        case CALG_SHA1: return EVP_sha1();
        case CALG_SHA_256: return EVP_sha256();
        case CALG_SHA_384: return EVP_sha384();
        case CALG_SHA_512: return EVP_sha512();
        default: return nullptr;
    }
}

// =========================================================================
// Provider management
// =========================================================================

BOOL WINAPI_CALL THETAZ_CryptAcquireContextW(HCRYPTPROV* phProv, const wchar_t* szContainer,
    const wchar_t* szProvider, DWORD dwProvType, DWORD dwFlags) {
    
    (void)szContainer; (void)szProvider;
    
    std::lock_guard<std::mutex> lock(g_crypto_mutex);
    
    if (dwFlags & CRYPT_VERIFYCONTEXT) {
        // Create a temporary context for verification operations
        CryptoProvider* prov = new CryptoProvider();
        prov->handle = g_next_prov_id++;
        prov->prov_type = dwProvType;
        prov->flags = dwFlags;
        
        g_providers[prov->handle] = prov;
        
        if (phProv) *phProv = reinterpret_cast<HCRYPTPROV>(prov->handle);
        return TRUE;
    }
    
    // For persistent key containers, we would need persistent storage
    // For now, just create a provider handle
    CryptoProvider* prov = new CryptoProvider();
    prov->handle = g_next_prov_id++;
    prov->prov_type = dwProvType;
    prov->flags = dwFlags;
    
    g_providers[prov->handle] = prov;
    
    if (phProv) *phProv = reinterpret_cast<HCRYPTPROV>(prov->handle);
    return TRUE;
}

BOOL WINAPI_CALL THETAZ_CryptContextAddRef(HCRYPTPROV hProv, DWORD* pdwReserved, DWORD dwFlags) {
    (void)hProv; (void)pdwReserved; (void)dwFlags;
    return TRUE; // Reference counting not fully implemented
}

BOOL WINAPI_CALL THETAZ_CryptReleaseContext(HCRYPTPROV hProv, DWORD dwFlags) {
    (void)dwFlags;
    
    uint64_t handle = reinterpret_cast<uint64_t>(hProv);
    
    std::lock_guard<std::mutex> lock(g_crypto_mutex);
    auto it = g_providers.find(handle);
    if (it != g_providers.end()) {
        delete it->second;
        g_providers.erase(it);
    }
    return TRUE;
}

// =========================================================================
// Hash operations
// =========================================================================

BOOL WINAPI_CALL THETAZ_CryptCreateHash(HCRYPTPROV hProv, DWORD Algid, HCRYPTKEY hKey, DWORD dwFlags, HCRYPTHASH* phHash) {
    (void)hProv; (void)hKey; (void)dwFlags;
    
    const EVP_MD* md = algid_to_md(Algid);
    if (!md) return FALSE;
    
    CryptoHash* hash = new CryptoHash();
    hash->ctx = EVP_MD_CTX_new();
    if (!hash->ctx) {
        delete hash;
        return FALSE;
    }
    
    if (EVP_DigestInit_ex(hash->ctx, md, nullptr) != 1) {
        EVP_MD_CTX_free(hash->ctx);
        delete hash;
        return FALSE;
    }
    
    hash->alg_id = Algid;
    hash->type = CRYPTO_OBJ_HASH;
    
    std::lock_guard<std::mutex> lock(g_crypto_mutex);
    hash->handle = g_next_hash_id++;
    g_hashes[hash->handle] = hash;
    
    if (phHash) *phHash = reinterpret_cast<HCRYPTHASH>(hash->handle);
    return TRUE;
}

BOOL WINAPI_CALL THETAZ_CryptDestroyHash(HCRYPTHASH hHash) {
    uint64_t handle = reinterpret_cast<uint64_t>(hHash);
    
    std::lock_guard<std::mutex> lock(g_crypto_mutex);
    auto it = g_hashes.find(handle);
    if (it != g_hashes.end()) {
        CryptoHash* hash = static_cast<CryptoHash*>(it->second);
        if (hash->ctx) EVP_MD_CTX_free(hash->ctx);
        delete hash;
        g_hashes.erase(it);
        return TRUE;
    }
    return FALSE;
}

BOOL WINAPI_CALL THETAZ_CryptHashData(HCRYPTHASH hHash, const uint8_t* pbData, DWORD dwDataLen, DWORD dwFlags) {
    (void)dwFlags;
    
    uint64_t handle = reinterpret_cast<uint64_t>(hHash);
    
    std::lock_guard<std::mutex> lock(g_crypto_mutex);
    auto it = g_hashes.find(handle);
    if (it == g_hashes.end()) return FALSE;
    
    CryptoHash* hash = static_cast<CryptoHash*>(it->second);
    if (!hash->ctx) return FALSE;
    
    if (EVP_DigestUpdate(hash->ctx, pbData, dwDataLen) != 1) {
        return FALSE;
    }
    return TRUE;
}

BOOL WINAPI_CALL THETAZ_CryptGetHashParam(HCRYPTHASH hHash, DWORD dwParam, uint8_t* pbData, LPDWORD pcbData, DWORD dwFlags) {
    (void)dwFlags;
    
    uint64_t handle = reinterpret_cast<uint64_t>(hHash);
    
    std::lock_guard<std::mutex> lock(g_crypto_mutex);
    auto it = g_hashes.find(handle);
    if (it == g_hashes.end()) return FALSE;
    
    CryptoHash* hash = static_cast<CryptoHash*>(it->second);
    
    if (dwParam == HP_HASHVAL) {
        unsigned int md_len = 0;
        if (EVP_DigestFinal_ex(hash->ctx, pbData, &md_len) != 1) {
            return FALSE;
        }
        if (pcbData) *pcbData = md_len;
        // Re-initialize for further use
        const EVP_MD* md = algid_to_md(hash->alg_id);
        if (md) EVP_DigestInit_ex(hash->ctx, md, nullptr);
        return TRUE;
    }
    else if (dwParam == HP_HASHSIZE) {
        const EVP_MD* md = algid_to_md(hash->alg_id);
        if (!md) return FALSE;
        int size = EVP_MD_size(md);
        if (pbData && pcbData && *pcbData >= sizeof(DWORD)) {
            *reinterpret_cast<DWORD*>(pbData) = size;
            *pcbData = sizeof(DWORD);
        }
        return TRUE;
    }
    return FALSE;
}

// =========================================================================
// Provider parameter operations
// =========================================================================

BOOL WINAPI_CALL THETAZ_CryptGetProvParam(HCRYPTPROV hProv, DWORD dwParam, uint8_t* pbData, LPDWORD pcbData, DWORD dwFlags) {
    (void)hProv; (void)dwParam; (void)pbData; (void)pcbData; (void)dwFlags;
    return FALSE; // Not fully implemented
}

BOOL WINAPI_CALL THETAZ_CryptSetProvParam(HCRYPTPROV hProv, DWORD dwParam, const uint8_t* pbData, DWORD dwFlags) {
    (void)hProv; (void)dwParam; (void)pbData; (void)dwFlags;
    return FALSE; // Not fully implemented
}

// =========================================================================
// Random number generation
// =========================================================================

BOOL WINAPI_CALL THETAZ_CryptGenRandom(HCRYPTPROV hProv, DWORD dwLen, uint8_t* pbBuffer) {
    (void)hProv;
    
    if (!pbBuffer || dwLen == 0) return FALSE;
    
    if (RAND_bytes(pbBuffer, static_cast<int>(dwLen)) != 1) {
        return FALSE;
    }
    return TRUE;
}

// =========================================================================
// Key operations
// =========================================================================

BOOL WINAPI_CALL THETAZ_CryptGenKey(HCRYPTPROV hProv, DWORD Algid, DWORD dwFlags, HCRYPTKEY* phKey) {
    (void)hProv; (void)dwFlags;
    
    CryptoKey* key = new CryptoKey();
    key->alg_id = Algid;
    key->type = CRYPTO_OBJ_KEY;
    
    int nid = algid_to_nid(Algid);
    if (nid == NID_undef) {
        delete key;
        return FALSE;
    }
    
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(nid, nullptr);
    if (!ctx) {
        delete key;
        return FALSE;
    }
    
    if (EVP_PKEY_keygen_init(ctx) != 1) {
        EVP_PKEY_CTX_free(ctx);
        delete key;
        return FALSE;
    }
    
    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_keygen(ctx, &key->pkey) != 1) {
        EVP_PKEY_CTX_free(ctx);
        delete key;
        return FALSE;
    }
    
    EVP_PKEY_CTX_free(ctx);
    key->is_public = false;
    
    std::lock_guard<std::mutex> lock(g_crypto_mutex);
    key->handle = g_next_key_id++;
    g_keys[key->handle] = key;
    
    if (phKey) *phKey = reinterpret_cast<HCRYPTKEY>(key->handle);
    return TRUE;
}

BOOL WINAPI_CALL THETAZ_CryptDestroyKey(HCRYPTKEY hKey) {
    uint64_t handle = reinterpret_cast<uint64_t>(hKey);
    
    std::lock_guard<std::mutex> lock(g_crypto_mutex);
    auto it = g_keys.find(handle);
    if (it != g_keys.end()) {
        CryptoKey* key = static_cast<CryptoKey*>(it->second);
        if (key->pkey) EVP_PKEY_free(key->pkey);
        delete key;
        g_keys.erase(it);
        return TRUE;
    }
    return FALSE;
}

BOOL WINAPI_CALL THETAZ_CryptGetKeyParam(HCRYPTKEY hKey, DWORD dwParam, uint8_t* pbData, LPDWORD pcbData, DWORD dwFlags) {
    (void)hKey; (void)dwParam; (void)pbData; (void)pcbData; (void)dwFlags;
    return FALSE; // Not fully implemented
}

BOOL WINAPI_CALL THETAZ_CryptExportKey(HCRYPTKEY hKey, HCRYPTKEY hExpKey, DWORD dwBlobType, DWORD dwFlags,
    uint8_t* pbData, LPDWORD pdwDataLen) {
    (void)hKey; (void)hExpKey; (void)dwBlobType; (void)dwFlags; (void)pbData; (void)pdwDataLen;
    return FALSE; // Not fully implemented
}

BOOL WINAPI_CALL THETAZ_CryptImportKey(HCRYPTPROV hProv, const uint8_t* pbData, DWORD dwDataLen,
    HCRYPTKEY hPubKey, DWORD dwFlags, HCRYPTKEY* phKey) {
    (void)hProv; (void)pbData; (void)dwDataLen; (void)hPubKey; (void)dwFlags;
    if (phKey) *phKey = nullptr;
    return FALSE; // Not fully implemented
}

// =========================================================================
// Encryption/Decryption
// =========================================================================

BOOL WINAPI_CALL THETAZ_CryptEncrypt(HCRYPTKEY hKey, HCRYPTHASH hHash, BOOL final, DWORD dwFlags,
    uint8_t* pbData, LPDWORD pdwDataLen, DWORD dwBufLen) {
    (void)hKey; (void)hHash; (void)final; (void)dwFlags; (void)pbData; (void)pdwDataLen; (void)dwBufLen;
    return FALSE; // Not fully implemented
}

BOOL WINAPI_CALL THETAZ_CryptDecrypt(HCRYPTKEY hKey, HCRYPTHASH hHash, BOOL final, DWORD dwFlags,
    uint8_t* pbData, LPDWORD pdwDataLen) {
    (void)hKey; (void)hHash; (void)final; (void)dwFlags; (void)pbData; (void)pdwDataLen;
    return FALSE; // Not fully implemented
}

// =========================================================================
// Key derivation
// =========================================================================

BOOL WINAPI_CALL THETAZ_CryptDeriveKey(HCRYPTPROV hProv, DWORD Algid, HCRYPTKEY hBaseData, DWORD dwFlags, HCRYPTKEY* phKey) {
    (void)hProv; (void)Algid; (void)hBaseData; (void)dwFlags;
    if (phKey) *phKey = nullptr;
    return FALSE; // Not fully implemented
}

// =========================================================================
// Sign/Verify
// =========================================================================

BOOL WINAPI_CALL THETAZ_CryptSignHashW(HCRYPTHASH hHash, DWORD dwKeySpec, const wchar_t* szDescription,
    DWORD dwFlags, uint8_t* pbSignature, LPDWORD pdwSigLen) {
    (void)hHash; (void)dwKeySpec; (void)szDescription; (void)dwFlags;
    (void)pbSignature; (void)pdwSigLen;
    return FALSE; // Not fully implemented
}

BOOL WINAPI_CALL THETAZ_CryptVerifySignatureW(HCRYPTHASH hHash, const uint8_t* pbSignature, DWORD dwSigLen,
    HCRYPTKEY hPubKey, const wchar_t* szDescription, DWORD dwFlags) {
    (void)hHash; (void)pbSignature; (void)dwSigLen; (void)hPubKey;
    (void)szDescription; (void)dwFlags;
    return FALSE; // Not fully implemented
}

// =========================================================================
// Provider enumeration
// =========================================================================

BOOL WINAPI_CALL THETAZ_CryptEnumProvidersW(DWORD dwIndex, DWORD* pdwReserved, DWORD dwFlags,
    DWORD* pdwProvType, wchar_t* szProvName, LPDWORD pcchProvName) {
    (void)dwIndex; (void)pdwReserved; (void)dwFlags;
    (void)pdwProvType; (void)szProvName; (void)pcchProvName;
    return FALSE; // Not fully implemented
}

// =========================================================================
// Key retrieval
// =========================================================================

BOOL WINAPI_CALL THETAZ_CryptGetUserKey(HCRYPTPROV hProv, DWORD dwKeySpec, HCRYPTKEY* phKey) {
    (void)hProv; (void)dwKeySpec;
    if (phKey) *phKey = nullptr;
    return FALSE; // Not fully implemented
}

// =========================================================================
// MD5 legacy API
// =========================================================================

BOOL WINAPI_CALL THETAZ_CryptMD5Init(HCRYPTHASH* phHash) {
    return ::ThetaZ::THETAZ_CryptCreateHash((HCRYPTPROV)nullptr, CALG_MD5, (HCRYPTKEY)nullptr, 0, phHash);
}

BOOL WINAPI_CALL THETAZ_CryptMD5Update(HCRYPTHASH hHash, const uint8_t* pbData, DWORD dwDataLen) {
    return THETAZ_CryptHashData(hHash, pbData, dwDataLen, 0);
}

BOOL WINAPI_CALL THETAZ_CryptMD5Final(HCRYPTHASH hHash, uint8_t* pbDigest) {
    DWORD len = 16; // MD5 = 16 bytes
    return ::ThetaZ::THETAZ_CryptGetHashParam(hHash, HP_HASHVAL, pbDigest, &len, (DWORD)0);
}

// =========================================================================
// Certificate Store API (crypt32.dll) — real X509-backed implementation
// =========================================================================

namespace {

constexpr uint32_t kEncodingX509 = 1;          // X509_ASN_ENCODING
constexpr uint32_t kFindSubjectStrA = 0x70007;
constexpr uint32_t kFindSubjectStrW = 0x80008;
constexpr uint32_t kFindSha1Hash = 0x10000;
constexpr uint32_t kFindExisting = 0xd0000;

struct CertBlob { uint32_t cbData; uint32_t pbData; };         // 8 bytes
struct CertAlgId { uint32_t pszObjId; uint32_t par_cb; uint32_t par_pb; }; // 12
struct CertBitBlob { uint32_t cbData; uint32_t pbData; uint32_t unused; };  // 12
struct CertContextNative { uint32_t enc; uint32_t pbEnc; uint32_t cbEnc; uint32_t pInfo; uint32_t hStore; }; // 20
struct CertInfoNative {
    uint32_t ver;
    CertBlob serial;          // +4
    CertAlgId sigalg;         // +12
    CertBlob issuer;          // +24
    uint32_t nb_low, nb_high, na_low, na_high; // +32..+44
    CertBlob subject;         // +48
    CertAlgId spki_alg;       // +56
    CertBitBlob spki_pub;     // +68
    CertBitBlob issuer_uid;   // +80
    CertBitBlob subject_uid;  // +92
    uint32_t cext;            // +104
    uint32_t rext;            // +108
};                            // 112
static_assert(sizeof(CertInfoNative) == 112, "CERT_INFO x86 size");
static_assert(sizeof(CertContextNative) == 20, "CERT_CONTEXT x86 size");

struct HostCertStore {
    std::vector<X509*> certs;
};
struct HostChain {
    bool trusted = false;
    uint32_t depth = 1;
    uint32_t leaf_ctx = 0;
};

std::mutex g_cert_mutex;
std::unordered_map<uintptr_t, HostCertStore> g_cert_stores;
std::unordered_map<uintptr_t, HostChain> g_cert_chains;
std::atomic<uintptr_t> g_cert_next_store{0x8000};
std::atomic<uintptr_t> g_cert_next_chain{0x9000};

static std::vector<X509*> store_snapshot(uintptr_t h) {
    std::lock_guard<std::mutex> lock(g_cert_mutex);
    auto it = g_cert_stores.find(h);
    if (it == g_cert_stores.end()) return {};
    return it->second.certs;
}

uint32_t guest_copy(const void* src, size_t len) {
    void* p = alloc_guest_heap(len);
    if (!p) return 0;
    if (len) memcpy(p, src, len);
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p));
}

static uint32_t name_to_guest(X509_NAME* n) {
    if (!n) return 0;
    int len = i2d_X509_NAME(n, nullptr);
    unsigned char* buf = static_cast<unsigned char*>(alloc_guest_heap(len));
    if (!buf) return 0;
    unsigned char* p = buf;
    i2d_X509_NAME(n, &p);
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(buf));
}

static uint32_t blob_to_guest(const unsigned char* d, size_t len) { return guest_copy(d, len); }

uint32_t build_cert_context(X509* x, uint32_t store_handle) {
    if (!x) return 0;
    int derlen = i2d_X509(x, nullptr);
    if (derlen <= 0) return 0;
    unsigned char* der = static_cast<unsigned char*>(alloc_guest_heap(derlen));
    if (!der) return 0;
    {
        unsigned char* p = der;
        i2d_X509(x, &p);
    }
    uint32_t pbEnc = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(der));

    CertInfoNative* info = static_cast<CertInfoNative*>(calloc_guest_heap(1, sizeof(CertInfoNative)));
    if (!info) return 0;
    uint32_t pInfo = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(info));
    info->ver = static_cast<uint32_t>(X509_get_version(x));

    const ASN1_INTEGER* serial = X509_get0_serialNumber(x);
    if (serial) {
        int slen = ASN1_STRING_length(serial);
        const unsigned char* sd = ASN1_STRING_get0_data(serial);
        info->serial.cbData = static_cast<uint32_t>(slen);
        info->serial.pbData = blob_to_guest(sd, slen);
    }
    X509_NAME* subj = X509_get_subject_name(x);
    X509_NAME* iss = X509_get_issuer_name(x);
    if (subj) {
        int slen = i2d_X509_NAME(subj, nullptr);
        info->subject.cbData = static_cast<uint32_t>(slen);
        info->subject.pbData = name_to_guest(subj);
    }
    if (iss) {
        int ilen = i2d_X509_NAME(iss, nullptr);
        info->issuer.cbData = static_cast<uint32_t>(ilen);
        info->issuer.pbData = name_to_guest(iss);
    }
    X509_PUBKEY* pk = X509_get_X509_PUBKEY(x);
    if (pk) {
        int pkeylen = i2d_X509_PUBKEY(pk, nullptr);
        info->spki_pub.cbData = static_cast<uint32_t>(pkeylen);
        unsigned char* pkbuf = static_cast<unsigned char*>(alloc_guest_heap(pkeylen));
        if (pkbuf) {
            unsigned char* pp = pkbuf;
            i2d_X509_PUBKEY(pk, &pp);
            info->spki_pub.pbData = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pkbuf));
        }
        info->spki_pub.unused = 0;
    }

    CertContextNative* ctx = static_cast<CertContextNative*>(calloc_guest_heap(1, sizeof(CertContextNative)));
    if (!ctx) return 0;
    ctx->enc = kEncodingX509;
    ctx->pbEnc = pbEnc;
    ctx->cbEnc = static_cast<uint32_t>(derlen);
    ctx->pInfo = pInfo;
    ctx->hStore = store_handle;
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ctx));
}

} // namespace

HANDLE WINAPI_CALL THETAZ_CertOpenSystemStoreW(HANDLE hprov, LPCWSTR szSubsystemProtocol) {
    (void)hprov;
    uintptr_t h = g_cert_next_store.fetch_add(1);
    std::lock_guard<std::mutex> lock(g_cert_mutex);
    g_cert_stores[h] = HostCertStore{};
    return reinterpret_cast<HANDLE>(h);
}

BOOL WINAPI_CALL THETAZ_CertCloseStore(void* hCertStore, DWORD dwFlags) {
    (void)dwFlags;
    if (!hCertStore) return FALSE;
    uintptr_t h = reinterpret_cast<uintptr_t>(hCertStore);
    std::lock_guard<std::mutex> lock(g_cert_mutex);
    auto it = g_cert_stores.find(h);
    if (it == g_cert_stores.end()) return FALSE;
    for (X509* x : it->second.certs) X509_free(x);
    g_cert_stores.erase(it);
    return TRUE;
}

BOOL WINAPI_CALL THETAZ_CertAddEncodedCertificateToStore(void* hCertStore, DWORD dwCertEncodingType,
    const BYTE* pbCertEncoded, DWORD cbCertEncoded, DWORD dwAddDisposition, void** ppCertContext) {
    (void)dwCertEncodingType;
    if (!hCertStore || !pbCertEncoded || !cbCertEncoded) { if (ppCertContext) *ppCertContext = nullptr; return FALSE; }
    const unsigned char* p = pbCertEncoded;
    X509* x = d2i_X509(nullptr, &p, cbCertEncoded);
    if (!x) { if (ppCertContext) *ppCertContext = nullptr; return FALSE; }
    uintptr_t h = reinterpret_cast<uintptr_t>(hCertStore);
    uint32_t cctx = 0;
    {
        std::lock_guard<std::mutex> lock(g_cert_mutex);
        g_cert_stores[h].certs.push_back(x);
        cctx = build_cert_context(x, static_cast<uint32_t>(h));
    }
    if (ppCertContext) *ppCertContext = reinterpret_cast<void*>(static_cast<uintptr_t>(cctx));
    return TRUE;
}

void* WINAPI_CALL THETAZ_CertFindCertificateInStore(void* hCertStore, DWORD dwCertEncodingType,
    DWORD dwFindFlags, DWORD dwFindType, const void* pvFindPara, void* pCertContext) {
    (void)dwCertEncodingType; (void)pCertContext; (void)dwFindFlags;
    if (!hCertStore) return nullptr;
    uintptr_t h = reinterpret_cast<uintptr_t>(hCertStore);
    std::vector<X509*> certs = store_snapshot(h);
    if (certs.empty()) return nullptr;

    for (X509* x : certs) {
        bool match = false;
        if (dwFindType == kFindExisting) {
            match = (x != nullptr);
        } else if (dwFindType == kFindSha1Hash && pvFindPara) {
            const unsigned char* want = static_cast<const unsigned char*>(pvFindPara);
            unsigned char got[20];
            unsigned int glen = 0;
            if (X509_digest(x, EVP_sha1(), got, &glen) == 1 && glen == 20) {
                match = (memcmp(got, want, 20) == 0);
            }
        } else if ((dwFindType == kFindSubjectStrA || dwFindType == kFindSubjectStrW) && pvFindPara) {
            std::string want;
            if (dwFindType == kFindSubjectStrW) {
                const uint16_t* wp = reinterpret_cast<const uint16_t*>(pvFindPara);
                while (*wp) want.push_back(static_cast<char>(*wp++ & 0xFF));
            } else {
                want = reinterpret_cast<const char*>(pvFindPara);
            }
            X509_NAME* subj = X509_get_subject_name(x);
            char buf[512];
            X509_NAME_oneline(subj, buf, sizeof(buf));
            std::string lowerbuf = buf;
            std::string lowerwant = want;
            for (auto& c : lowerbuf) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
            for (auto& c : lowerwant) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
            if (lowerbuf.find(lowerwant) != std::string::npos) match = true;
        }
        if (match) {
            return reinterpret_cast<void*>(static_cast<uintptr_t>(build_cert_context(x, static_cast<uint32_t>(h))));
        }
    }
    return nullptr;
}

void WINAPI_CALL THETAZ_CertFreeCertificateContext(void* pCertContext) {
    (void)pCertContext;
    // Guest-allocated context is intentionally leaked (identity-mapped guest heap).
}

BOOL WINAPI_CALL THETAZ_CertDeleteCertificateFromStore(void* pCertContext) {
    (void)pCertContext;
    return TRUE;
}

void* WINAPI_CALL THETAZ_CertEnumCertificatesInStore(void* hCertStore, void* pPrevCertContext) {
    (void)pPrevCertContext;
    if (!hCertStore) return nullptr;
    uintptr_t h = reinterpret_cast<uintptr_t>(hCertStore);
    std::vector<X509*> certs = store_snapshot(h);
    if (certs.empty()) return nullptr;
    return reinterpret_cast<void*>(static_cast<uintptr_t>(build_cert_context(certs[0], static_cast<uint32_t>(h))));
}

BOOL WINAPI_CALL THETAZ_CertVerifyCertificateChainPolicy(void* pszPolicyOID, void* pChainContext,
    void* pPolicyPara, void* pPolicyStatus) {
    (void)pszPolicyOID; (void)pPolicyPara;
    if (!pChainContext) { if (pPolicyStatus) memset(static_cast<void*>(pPolicyStatus), 0, 20); return FALSE; }
    uintptr_t h = reinterpret_cast<uintptr_t>(pChainContext);
    bool trusted = false;
    {
        std::lock_guard<std::mutex> lock(g_cert_mutex);
        auto it = g_cert_chains.find(h);
        if (it != g_cert_chains.end()) trusted = it->second.trusted;
    }
    if (pPolicyStatus) {
        uint8_t* st = static_cast<uint8_t*>(pPolicyStatus);
        // CERT_CHAIN_POLICY_STATUS: {cbSize u32, dwError u32, lChainIndex LONG, lElementIndex LONG, pvExtra u32}
        uint32_t* s = reinterpret_cast<uint32_t*>(st);
        s[0] = 20;
        s[1] = trusted ? 0 : 0x80096004u; // TRUST_E_SUBJECT_NOT_TRUSTED
        s[2] = trusted ? 0 : static_cast<uint32_t>(-1);
        s[3] = 0;
        s[4] = 0;
    }
    return trusted ? TRUE : FALSE;
}

BOOL WINAPI_CALL THETAZ_CertCompareCertificate(DWORD dwCertEncodingType, void* pCertContext1, void* pCertContext2) {
    (void)dwCertEncodingType;
    if (!pCertContext1 || !pCertContext2) return FALSE;
    uintptr_t a = reinterpret_cast<uintptr_t>(pCertContext1);
    uintptr_t b = reinterpret_cast<uintptr_t>(pCertContext2);
    if (a == b) return TRUE;
    return FALSE;
}

void* WINAPI_CALL THETAZ_CertOpenStore(LPCSTR szStoreProvider, DWORD dwEncodingType, HCRYPTPROV hCryptProv,
    DWORD dwFlags, const void* pvPara) {
    (void)szStoreProvider; (void)dwEncodingType; (void)hCryptProv; (void)dwFlags; (void)pvPara;
    uintptr_t h = g_cert_next_store.fetch_add(1);
    std::lock_guard<std::mutex> lock(g_cert_mutex);
    g_cert_stores[h] = HostCertStore{};
    return reinterpret_cast<void*>(h);
}

void* WINAPI_CALL THETAZ_CertDuplicateCertificateContext(void* pCertContext) {
    return pCertContext;
}

void* WINAPI_CALL THETAZ_CertDuplicateStore(void* hCertStore) {
    if (!hCertStore) return nullptr;
    uintptr_t src = reinterpret_cast<uintptr_t>(hCertStore);
    std::lock_guard<std::mutex> lock(g_cert_mutex);
    auto it = g_cert_stores.find(src);
    if (it == g_cert_stores.end()) return nullptr;
    uintptr_t h = g_cert_next_store.fetch_add(1);
    HostCertStore dup;
    for (X509* x : it->second.certs) {
        X509_up_ref(x);
        dup.certs.push_back(x);
    }
    g_cert_stores[h] = std::move(dup);
    return reinterpret_cast<void*>(h);
}

BOOL WINAPI_CALL THETAZ_CertAddStoreToCollection(void* hCollectionStore, void* hSiblingStore,
    DWORD dwUpdateFlags, DWORD dwPriority) {
    (void)dwUpdateFlags; (void)dwPriority;
    if (!hCollectionStore || !hSiblingStore) return FALSE;
    uintptr_t c = reinterpret_cast<uintptr_t>(hCollectionStore);
    uintptr_t s = reinterpret_cast<uintptr_t>(hSiblingStore);
    std::lock_guard<std::mutex> lock(g_cert_mutex);
    auto sit = g_cert_stores.find(s);
    if (sit == g_cert_stores.end()) return FALSE;
    auto cit = g_cert_stores.find(c);
    if (cit == g_cert_stores.end()) return FALSE;
    for (X509* x : sit->second.certs) {
        X509_up_ref(x);
        cit->second.certs.push_back(x);
    }
    return TRUE;
}

BOOL WINAPI_CALL THETAZ_CertCompareCertificateName(DWORD dwCertEncodingType, void* pCertName1, void* pCertName2) {
    (void)dwCertEncodingType;
    if (!pCertName1 || !pCertName2) return FALSE;
    CertBlob* n1 = static_cast<CertBlob*>(pCertName1);
    CertBlob* n2 = static_cast<CertBlob*>(pCertName2);
    if (n1->cbData != n2->cbData) return FALSE;
    if (n1->cbData == 0) return TRUE;
    return memcmp(reinterpret_cast<void*>(static_cast<uintptr_t>(n1->pbData)),
                  reinterpret_cast<void*>(static_cast<uintptr_t>(n2->pbData)), n1->cbData) == 0 ? TRUE : FALSE;
}

BOOL WINAPI_CALL THETAZ_CertCompareIntegerBlob(void* pIntBlob1, void* pIntBlob2) {
    if (!pIntBlob1 || !pIntBlob2) return FALSE;
    CertBlob* b1 = static_cast<CertBlob*>(pIntBlob1);
    CertBlob* b2 = static_cast<CertBlob*>(pIntBlob2);
    if (b1->cbData != b2->cbData) return FALSE;
    if (b1->cbData == 0) return TRUE;
    return memcmp(reinterpret_cast<void*>(static_cast<uintptr_t>(b1->pbData)),
                  reinterpret_cast<void*>(static_cast<uintptr_t>(b2->pbData)), b1->cbData) == 0 ? TRUE : FALSE;
}

void* WINAPI_CALL THETAZ_CertFindExtension(const char* pszObjId, DWORD cExtension, void* rgExtension) {
    if (!pszObjId || !rgExtension || cExtension == 0) return nullptr;
    uint8_t* arr = static_cast<uint8_t*>(rgExtension);
    for (DWORD i = 0; i < cExtension; i++) {
        uint32_t oid = *reinterpret_cast<uint32_t*>(arr + i * 16);
        const char* oidstr = reinterpret_cast<const char*>(static_cast<uintptr_t>(oid));
        if (oidstr && strcmp(oidstr, pszObjId) == 0) return arr + i * 16;
    }
    return nullptr;
}

static X509* cert_from_context(void* pCertContext) {
    if (!pCertContext) return nullptr;
    uint8_t* c = static_cast<uint8_t*>(pCertContext);
    uint32_t pbEnc = *reinterpret_cast<uint32_t*>(c + 4);
    uint32_t cbEnc = *reinterpret_cast<uint32_t*>(c + 8);
    if (!pbEnc || !cbEnc || cbEnc > 65536) return nullptr;
    const unsigned char* p = reinterpret_cast<const unsigned char*>(static_cast<uintptr_t>(pbEnc));
    return d2i_X509(nullptr, &p, cbEnc);
}

BOOL WINAPI_CALL THETAZ_CertGetCertificateContextProperty(void* pCertContext, DWORD dwPropId,
    void* pvData, DWORD* pcbData) {
    if (!pCertContext) return FALSE;
    X509* x = cert_from_context(pCertContext);
    if (!x) return FALSE;
    BOOL ok = FALSE;
    unsigned char buf[64];
    unsigned int len = 0;
    if (dwPropId == 3) {
        if (X509_digest(x, EVP_sha1(), buf, &len) != 1) len = 0;
        else ok = TRUE;
    } else if (dwPropId == 32) {
        if (X509_digest(x, EVP_sha256(), buf, &len) != 1) len = 0;
        else ok = TRUE;
    } else {
        X509_free(x);
        if (pcbData) *pcbData = 0;
        return FALSE;
    }
    if (pcbData) {
        if (!pvData) { *pcbData = len; X509_free(x); return TRUE; }
        if (*pcbData < len) { *pcbData = len; X509_free(x); return FALSE; }
    }
    if (pvData && len) memcpy(pvData, buf, len);
    if (pcbData) *pcbData = len;
    X509_free(x);
    return ok;
}

void* WINAPI_CALL THETAZ_CertGetSubjectCertificateFromStore(void* hCertStore, DWORD dwCertEncodingType, void* pCertId) {
    (void)dwCertEncodingType; (void)pCertId;
    if (!hCertStore) return nullptr;
    std::vector<X509*> certs = store_snapshot(reinterpret_cast<uintptr_t>(hCertStore));
    if (certs.empty()) return nullptr;
    return reinterpret_cast<void*>(static_cast<uintptr_t>(build_cert_context(certs[0], static_cast<uint32_t>(reinterpret_cast<uintptr_t>(hCertStore)))));
}

BOOL WINAPI_CALL THETAZ_CertGetCertificateChain(void* hChainEngine, void* pCertContext,
    void* pTime, void* hAdditionalStore, void* pChainPara, DWORD dwFlags, void* pvReserved, void** ppChainContext) {
    (void)hChainEngine; (void)pTime; (void)hAdditionalStore; (void)pChainPara; (void)dwFlags; (void)pvReserved;
    if (ppChainContext) *ppChainContext = nullptr;
    X509* leaf = cert_from_context(pCertContext);
    if (!leaf) return FALSE;

    X509_STORE* store = X509_STORE_new();
    {
        std::lock_guard<std::mutex> lock(g_cert_mutex);
        for (const auto& kv : g_cert_stores) {
            for (X509* x : kv.second.certs) X509_STORE_add_cert(store, x);
        }
    }
    X509_STORE_CTX* vctx = X509_STORE_CTX_new();
    X509_STORE_CTX_init(vctx, store, leaf, nullptr);
    int rc = X509_verify_cert(vctx);
    int depth = X509_STORE_CTX_get_error_depth(vctx);

    uintptr_t h = g_cert_next_chain.fetch_add(1);
    {
        std::lock_guard<std::mutex> lock(g_cert_mutex);
        HostChain ch;
        ch.trusted = (rc == 1);
        ch.depth = static_cast<uint32_t>(depth >= 0 ? depth : 1);
        g_cert_chains[h] = ch;
    }
    X509_STORE_CTX_free(vctx);
    X509_STORE_free(store);
    X509_free(leaf);
    if (ppChainContext) *ppChainContext = reinterpret_cast<void*>(h);
    return TRUE;
}

void WINAPI_CALL THETAZ_CertFreeCertificateChain(void* pChainContext) {
    if (!pChainContext) return;
    std::lock_guard<std::mutex> lock(g_cert_mutex);
    g_cert_chains.erase(reinterpret_cast<uintptr_t>(pChainContext));
}

DWORD WINAPI_CALL THETAZ_CertOIDToAlgId(const char* pszObjId) {
    if (!pszObjId) return 0;
    if (strcmp(pszObjId, "1.2.840.113549.1.1.5") == 0) return CALG_SHA1;
    if (strcmp(pszObjId, "1.2.840.113549.1.1.11") == 0) return CALG_SHA_256;
    if (strcmp(pszObjId, "1.2.840.113549.1.1.4") == 0) return CALG_MD5;
    if (strcmp(pszObjId, "1.2.840.10045.4.3.2") == 0) return CALG_SHA_256;
    if (strcmp(pszObjId, "1.2.840.10045.4.1") == 0) return CALG_SHA1;
    return 0;
}

HRESULT WINAPI_CALL THETAZ_CertSelectionGetSerializedBlob(void* pCertContext, void** ppCertBlob) {
    (void)pCertContext;
    if (ppCertBlob) *ppCertBlob = nullptr;
    return E_NOTIMPL;
}

int WINAPI_CALL THETAZ_CryptUIDlgSelectCertificateFromStore(void* hCertStore, void* hwnd,
    void* pwszTitle, void* pwszDisplayString, DWORD dwDontUseColumn, DWORD dwFlags, void* pvReserved) {
    (void)hCertStore; (void)hwnd; (void)pwszTitle; (void)pwszDisplayString;
    (void)dwDontUseColumn; (void)dwFlags; (void)pvReserved;
    return 0;
}

// =========================================================================
// BCrypt API — real implementations backed by OpenSSL.
// =========================================================================

namespace {

std::string w16_to_ascii(const uint16_t* s) {
    if (!s) return "";
    std::string out;
    while (*s) out.push_back(static_cast<char>(*s++ & 0xFF));
    return out;
}

const EVP_MD* bcrypt_md_for_len(size_t len) {
    switch (len) {
        case 16: return EVP_md5();
        case 20: return EVP_sha1();
        case 28: return EVP_sha224();
        case 32: return EVP_sha256();
        case 48: return EVP_sha384();
        case 64: return EVP_sha512();
        default: return nullptr;
    }
}

struct BcryptAlg {
    std::string name;
    const EVP_MD* md = nullptr;
    uint32_t hash_len = 0;
    uint32_t block_len = 0;
    bool is_hash = false;
    bool is_rsa = false;
    bool is_ecdsa = false;
    int ec_nid = 0;
};

struct BcryptHash {
    const EVP_MD* md = nullptr;
    EVP_MD_CTX* ctx = nullptr;
};

std::mutex g_bcrypt_mutex;
std::unordered_map<uintptr_t, BcryptAlg*> g_bcrypt_algs;
std::unordered_map<uintptr_t, BcryptHash*> g_bcrypt_hashes;
std::atomic<uintptr_t> g_bcrypt_next_alg{0x6000};
std::atomic<uintptr_t> g_bcrypt_next_hash{0x7000};

} // namespace

NTSTATUS WINAPI_CALL THETAZ_BCryptOpenAlgorithmProvider(void** phAlgorithm, LPCWSTR pszAlgId,
    LPCWSTR pszImplementation, DWORD dwFlags) {
    (void)pszImplementation; (void)dwFlags;
    if (!phAlgorithm) return STATUS_INVALID_PARAMETER;
    *phAlgorithm = nullptr;
    std::string alg = w16_to_ascii(reinterpret_cast<const uint16_t*>(pszAlgId));
    if (alg.empty()) return STATUS_UNSUCCESSFUL;

    BcryptAlg* a = new BcryptAlg();
    a->name = alg;
    a->is_hash = true;
    a->md = nullptr;
    if (alg == "MD5" || alg == "69992715-9D72-4999-AC2B-E4AA8F13C5F3") { a->md = EVP_md5(); a->hash_len = 16; a->block_len = 64; }
    else if (alg == "SHA1" || alg == "33B85817-2856-4658-8DBC-E0AADBD1A6F3") { a->md = EVP_sha1(); a->hash_len = 20; a->block_len = 64; }
    else if (alg == "SHA224") { a->md = EVP_sha224(); a->hash_len = 28; a->block_len = 64; }
    else if (alg == "SHA256" || alg == "ECDSA_P256") { a->md = EVP_sha256(); a->hash_len = 32; a->block_len = 64; }
    else if (alg == "SHA384") { a->md = EVP_sha384(); a->hash_len = 48; a->block_len = 128; }
    else if (alg == "SHA512") { a->md = EVP_sha512(); a->hash_len = 64; a->block_len = 128; }
    else if (alg == "RSA") { a->is_hash = false; a->is_rsa = true; }
    else if (alg == "ECDSA_P256") { a->is_hash = false; a->is_ecdsa = true; a->ec_nid = NID_X9_62_prime256v1; }
    else if (alg == "ECDSA_P384") { a->is_hash = false; a->is_ecdsa = true; a->ec_nid = NID_secp384r1; }
    else if (alg == "AES" || alg == "CHACHA20_POLY1305") { a->is_hash = false; }
    else { delete a; return STATUS_NOT_IMPLEMENTED; }

    a->hash_len = a->md ? static_cast<uint32_t>(EVP_MD_size(a->md)) : a->hash_len;
    uintptr_t h = g_bcrypt_next_alg.fetch_add(1);
    {
        std::lock_guard<std::mutex> lock(g_bcrypt_mutex);
        g_bcrypt_algs[h] = a;
    }
    *phAlgorithm = reinterpret_cast<void*>(h);
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL THETAZ_BCryptGetProperty(void* hObject, LPCWSTR pszProperty, void* pbOutput,
    DWORD cbOutput, DWORD* pcbResult, DWORD dwFlags) {
    (void)dwFlags;
    if (!hObject) return STATUS_INVALID_PARAMETER;
    std::string prop = w16_to_ascii(reinterpret_cast<const uint16_t*>(pszProperty));
    std::string val;
    uint32_t num = 0;
    bool is_num = false;

    const EVP_MD* md = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_bcrypt_mutex);
        auto ait = g_bcrypt_algs.find(reinterpret_cast<uintptr_t>(hObject));
        if (ait != g_bcrypt_algs.end() && ait->second) {
            md = ait->second->md;
            if (prop == "ObjectLength") { num = 0; is_num = true; }
            else if (prop == "HashDigestLength") { num = ait->second->hash_len; is_num = true; }
            else if (prop == "AlgorithmName") { val = ait->second->name; }
            else if (prop == "BlockLength") { num = ait->second->block_len; is_num = true; }
            else return STATUS_NOT_IMPLEMENTED;
        }
    }

    if (is_num) {
        if (pcbResult) *pcbResult = sizeof(DWORD);
        if (!pbOutput) return STATUS_SUCCESS;
        if (cbOutput < sizeof(DWORD)) return STATUS_BUFFER_TOO_SMALL;
        *reinterpret_cast<DWORD*>(pbOutput) = num;
        return STATUS_SUCCESS;
    }
    if (!val.empty()) {
        size_t wchars = val.size() + 1;
        DWORD need = static_cast<DWORD>(wchars * sizeof(uint16_t));
        if (pcbResult) *pcbResult = need;
        if (!pbOutput) return STATUS_SUCCESS;
        if (cbOutput < need) return STATUS_BUFFER_TOO_SMALL;
        uint16_t* out = reinterpret_cast<uint16_t*>(pbOutput);
        for (size_t i = 0; i < val.size(); i++) out[i] = static_cast<uint16_t>(val[i]);
        out[val.size()] = 0;
        return STATUS_SUCCESS;
    }
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS WINAPI_CALL THETAZ_BCryptGenRandom(void* hObject, void* pbBuffer, ULONG cbBuffer, DWORD dwFlags) {
    (void)hObject; (void)dwFlags;
    if (pbBuffer && cbBuffer > 0) {
        return RAND_bytes(static_cast<uint8_t*>(pbBuffer), static_cast<int>(cbBuffer)) == 1 ? 0 : STATUS_UNSUCCESSFUL;
    }
    return STATUS_UNSUCCESSFUL;
}

NTSTATUS WINAPI_CALL THETAZ_BCryptEncrypt(void* hKey, void* pbInput, DWORD cbInput, void* pPaddingInfo,
    void* pbIV, DWORD cbIV, void* pbOutput, DWORD cbOutput, DWORD* pcbResult, DWORD dwFlags) {
    (void)hKey; (void)pPaddingInfo; (void)pbIV; (void)cbIV; (void)pbOutput; (void)cbOutput; (void)pcbResult; (void)dwFlags;
    if (pbInput && pbOutput && cbOutput >= cbInput) {
        memcpy(pbOutput, pbInput, cbInput);
        if (pcbResult) *pcbResult = cbInput;
        return 0;
    }
    return STATUS_UNSUCCESSFUL;
}

NTSTATUS WINAPI_CALL THETAZ_BCryptSignHash(void* hKey, void* pPaddingInfo, void* pbInput, DWORD cbInput,
    void* pbSignature, DWORD cbSignature, DWORD* pcbResult, DWORD dwFlags) {
    (void)hKey; (void)pPaddingInfo; (void)pbInput; (void)cbInput; (void)pbSignature; (void)cbSignature; (void)pcbResult; (void)dwFlags;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS WINAPI_CALL THETAZ_BCryptCloseAlgorithmProvider(void* hAlgorithm, DWORD dwFlags) {
    (void)dwFlags;
    if (!hAlgorithm) return STATUS_SUCCESS;
    BcryptAlg* a = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_bcrypt_mutex);
        auto it = g_bcrypt_algs.find(reinterpret_cast<uintptr_t>(hAlgorithm));
        if (it != g_bcrypt_algs.end()) { a = it->second; g_bcrypt_algs.erase(it); }
    }
    delete a;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL THETAZ_BCryptDecrypt(void* hKey, void* pbInput, DWORD cbInput, void* pPaddingInfo,
    void* pbIV, DWORD cbIV, void* pbOutput, DWORD cbOutput, DWORD* pcbResult, DWORD dwFlags) {
    (void)hKey; (void)pPaddingInfo; (void)pbIV; (void)cbIV; (void)pbOutput; (void)cbOutput; (void)pcbResult; (void)dwFlags;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS WINAPI_CALL THETAZ_BCryptCreateHash(void* hAlgorithm, void** phHash, void* pbHashObject,
    DWORD cbHashObject, void* pbSecret, DWORD cbSecret, DWORD dwFlags) {
    (void)pbHashObject; (void)cbHashObject; (void)pbSecret; (void)cbSecret; (void)dwFlags;
    if (!phHash) return STATUS_INVALID_PARAMETER;
    *phHash = nullptr;
    BcryptAlg* a = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_bcrypt_mutex);
        auto it = g_bcrypt_algs.find(reinterpret_cast<uintptr_t>(hAlgorithm));
        if (it != g_bcrypt_algs.end()) a = it->second;
    }
    if (!a || !a->md) return STATUS_INVALID_PARAMETER;

    BcryptHash* h = new BcryptHash();
    h->md = a->md;
    h->ctx = EVP_MD_CTX_new();
    if (!h->ctx || EVP_DigestInit_ex(h->ctx, h->md, nullptr) != 1) {
        if (h->ctx) EVP_MD_CTX_free(h->ctx);
        delete h;
        return STATUS_UNSUCCESSFUL;
    }
    uintptr_t hid = g_bcrypt_next_hash.fetch_add(1);
    {
        std::lock_guard<std::mutex> lock(g_bcrypt_mutex);
        g_bcrypt_hashes[hid] = h;
    }
    *phHash = reinterpret_cast<void*>(hid);
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL THETAZ_BCryptHashData(void* hHash, void* pbInput, DWORD cbInput, DWORD dwFlags) {
    (void)dwFlags;
    BcryptHash* h = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_bcrypt_mutex);
        auto it = g_bcrypt_hashes.find(reinterpret_cast<uintptr_t>(hHash));
        if (it != g_bcrypt_hashes.end()) h = it->second;
    }
    if (!h || !h->ctx) return STATUS_INVALID_PARAMETER;
    if (cbInput == 0) return STATUS_SUCCESS;
    if (EVP_DigestUpdate(h->ctx, pbInput, cbInput) != 1) return STATUS_UNSUCCESSFUL;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL THETAZ_BCryptFinishHash(void* hHash, void* pbOutput, DWORD cbOutput, DWORD dwFlags) {
    (void)dwFlags;
    BcryptHash* h = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_bcrypt_mutex);
        auto it = g_bcrypt_hashes.find(reinterpret_cast<uintptr_t>(hHash));
        if (it != g_bcrypt_hashes.end()) h = it->second;
    }
    if (!h || !h->ctx) return STATUS_INVALID_PARAMETER;
    uint32_t want = static_cast<uint32_t>(EVP_MD_size(h->md));
    if (!pbOutput || cbOutput < want) return STATUS_BUFFER_TOO_SMALL;
    unsigned int outlen = 0;
    if (EVP_DigestFinal_ex(h->ctx, reinterpret_cast<unsigned char*>(pbOutput), &outlen) != 1) return STATUS_UNSUCCESSFUL;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL THETAZ_BCryptDestroyHash(void* hHash) {
    BcryptHash* h = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_bcrypt_mutex);
        auto it = g_bcrypt_hashes.find(reinterpret_cast<uintptr_t>(hHash));
        if (it != g_bcrypt_hashes.end()) { h = it->second; g_bcrypt_hashes.erase(it); }
    }
    if (h) {
        if (h->ctx) EVP_MD_CTX_free(h->ctx);
        delete h;
    }
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL THETAZ_BCryptDestroyKey(void* hKey) {
    if (!hKey) return STATUS_SUCCESS;
    uint64_t handle = reinterpret_cast<uint64_t>(hKey);
    CryptoKey* k = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_crypto_mutex);
        auto it = g_keys.find(handle);
        if (it != g_keys.end()) { k = it->second; g_keys.erase(it); }
    }
    if (k) {
        if (k->pkey) EVP_PKEY_free(k->pkey);
        delete k;
    }
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL THETAZ_BCryptDuplicateHash(void* hHash, void** phHash, void* pbHashObject,
    DWORD cbHashObject, DWORD dwFlags) {
    (void)hHash; (void)phHash; (void)pbHashObject; (void)cbHashObject; (void)dwFlags;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS WINAPI_CALL THETAZ_BCryptDuplicateKey(void* hKey, void** phKey, void* pbKeyObject,
    DWORD cbKeyObject, DWORD dwFlags) {
    (void)hKey; (void)phKey; (void)pbKeyObject; (void)cbKeyObject; (void)dwFlags;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS WINAPI_CALL THETAZ_BCryptDeriveKey(void* hSharedSecret, LPCWSTR pszKDF, void* pParameterList,
    void* pbDerivedKey, DWORD cbDerivedKey, DWORD* pcbResult, DWORD dwFlags) {
    (void)hSharedSecret; (void)pszKDF; (void)pParameterList;
    (void)pbDerivedKey; (void)cbDerivedKey; (void)pcbResult; (void)dwFlags;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS WINAPI_CALL THETAZ_BCryptImportKey(void* hAlgorithm, void* hImportKey, LPCWSTR pszBlobType,
    void* phKey, void* pbKeyObject, DWORD cbKeyObject, void* pbInput, DWORD cbInput, DWORD dwFlags) {
    (void)hAlgorithm; (void)hImportKey; (void)pszBlobType; (void)phKey;
    (void)pbKeyObject; (void)cbKeyObject; (void)pbInput; (void)cbInput; (void)dwFlags;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS WINAPI_CALL THETAZ_BCryptExportKey(void* hKey, void* hExportKey, LPCWSTR pszBlobType,
    void* pbOutput, DWORD cbOutput, DWORD* pcbResult, DWORD dwFlags) {
    (void)hKey; (void)hExportKey; (void)pszBlobType; (void)pbOutput; (void)cbOutput; (void)pcbResult; (void)dwFlags;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS WINAPI_CALL THETAZ_BCryptKeyDerivation(void* hKey, void* pbSecret, DWORD cbSecret,
    void* pbDerivedKey, DWORD cbDerivedKey, DWORD* pcbResult, DWORD dwFlags) {
    (void)hKey; (void)pbSecret; (void)cbSecret; (void)pbDerivedKey; (void)cbDerivedKey; (void)pcbResult; (void)dwFlags;
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS WINAPI_CALL THETAZ_BCryptImportKeyPair(void* hAlgorithm, void* hImportKey,
    LPCWSTR pszBlobType, void** phKey, void* pbInput, DWORD cbInput, DWORD dwFlags) {
    (void)hAlgorithm; (void)hImportKey; (void)dwFlags;
    if (!phKey) return STATUS_INVALID_PARAMETER;
    *phKey = nullptr;
    std::string blobtype = w16_to_ascii(reinterpret_cast<const uint16_t*>(pszBlobType));
    const uint8_t* blob = static_cast<const uint8_t*>(pbInput);
    if (!blob || cbInput < 8) return STATUS_INVALID_PARAMETER;

    EVP_PKEY* pkey = nullptr;
    uint32_t magic = *reinterpret_cast<const uint32_t*>(blob);

    if (blobtype == "ECCPUBLICBLOB") {
        uint32_t cbKey = *reinterpret_cast<const uint32_t*>(blob + 4);
        int nid = (magic == 0x31534345) ? NID_X9_62_prime256v1 :
                  (magic == 0x33534345) ? NID_secp384r1 : 0;
        if (!nid || 8 + 2u * cbKey > cbInput) return STATUS_INVALID_PARAMETER;
        EC_GROUP* group = EC_GROUP_new_by_curve_name(nid);
        EC_POINT* pt = EC_POINT_new(group);
        BIGNUM* x = BN_bin2bn(blob + 8, cbKey, nullptr);
        BIGNUM* y = BN_bin2bn(blob + 8 + cbKey, cbKey, nullptr);
        EC_POINT_set_affine_coordinates(group, pt, x, y, nullptr);
        EC_KEY* ec = EC_KEY_new();
        EC_KEY_set_group(ec, group);
        EC_KEY_set_public_key(ec, pt);
        EC_KEY_set_asn1_flag(ec, OPENSSL_EC_NAMED_CURVE);
        EC_KEY_set_conv_form(ec, POINT_CONVERSION_UNCOMPRESSED);
        pkey = EVP_PKEY_new();
        EVP_PKEY_assign_EC_KEY(pkey, ec);
        BN_free(x); BN_free(y);
        EC_POINT_free(pt);
        EC_GROUP_free(group);
    } else if (blobtype == "RSAPUBLICBLOB") {
        if (magic != 0x31415352 || cbInput < 24) return STATUS_INVALID_PARAMETER;
        uint32_t cbPublicExp = *reinterpret_cast<const uint32_t*>(blob + 8);
        uint32_t cbModulus = *reinterpret_cast<const uint32_t*>(blob + 12);
        if (24 + cbPublicExp + cbModulus > cbInput) return STATUS_INVALID_PARAMETER;
        RSA* rsa = RSA_new();
        BIGNUM* e = BN_lebin2bn(blob + 24, cbPublicExp, nullptr);
        BIGNUM* n = BN_bin2bn(blob + 24 + cbPublicExp, cbModulus, nullptr);
        RSA_set0_key(rsa, n, e, nullptr);
        pkey = EVP_PKEY_new();
        EVP_PKEY_assign_RSA(pkey, rsa);
    } else {
        return STATUS_NOT_IMPLEMENTED;
    }

    if (!pkey) return STATUS_UNSUCCESSFUL;
    CryptoKey* k = new CryptoKey();
    k->pkey = pkey;
    k->is_public = true;
    k->alg_id = 0;
    uint64_t hid = g_next_key_id.fetch_add(1);
    k->handle = hid;
    {
        std::lock_guard<std::mutex> lock(g_crypto_mutex);
        g_keys[hid] = k;
    }
    *phKey = reinterpret_cast<void*>(hid);
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI_CALL THETAZ_BCryptVerifySignature(void* hKey, void* pPaddingInfo,
    void* pbHash, DWORD cbHash, void* pbSignature, DWORD cbSignature, DWORD dwFlags) {
    (void)pPaddingInfo; (void)dwFlags;
    if (!hKey || !pbHash || !pbSignature) return STATUS_INVALID_PARAMETER;
    CryptoKey* k = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_crypto_mutex);
        auto it = g_keys.find(reinterpret_cast<uint64_t>(hKey));
        if (it != g_keys.end()) k = it->second;
    }
    if (!k || !k->pkey) return STATUS_INVALID_PARAMETER;
    const EVP_MD* md = bcrypt_md_for_len(cbHash);
    if (!md) return STATUS_INVALID_PARAMETER;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return STATUS_UNSUCCESSFUL;
    int ok = EVP_DigestVerifyInit(ctx, nullptr, md, nullptr, k->pkey);
    if (ok == 1) ok = EVP_DigestVerify(ctx, static_cast<const unsigned char*>(pbSignature), cbSignature,
                                       static_cast<const unsigned char*>(pbHash), cbHash);
    EVP_MD_CTX_free(ctx);
    return (ok == 1) ? STATUS_SUCCESS : STATUS_INVALID_SIGNATURE;
}

// =========================================================================
// CryptMsg (PKCS#7) + CryptDecodeObject + WinVerifyTrust (Authenticode)
// =========================================================================

namespace {

struct HostCryptMsg {
    std::vector<uint8_t> acc;
    PKCS7* p7 = nullptr;
    bool final = false;
};
std::mutex g_msg_mutex;
std::unordered_map<uintptr_t, HostCryptMsg> g_msgs;
std::atomic<uintptr_t> g_msg_next{0xa000};

const uint8_t kPeSubjectGuid[16] = {
    0xa8, 0xa8, 0x89, 0xc6, 0x78, 0x8e, 0xd0, 0x11,
    0x8c, 0x47, 0x00, 0xc0, 0x4f, 0xc2, 0x95, 0xee,
};

struct WinTrustFileInfoNative { uint32_t cbStruct; uint32_t pcwszFilePath; uint32_t hFile; uint32_t pgKnownSubject; };
struct WinTrustDataNative {
    uint32_t cbStruct;
    uint32_t pPolicyCallbackData;
    uint32_t pSIPClientData;
    uint32_t dwUIChoice;
    uint32_t fdwRevocationChecks;
    uint32_t dwUnionChoice;
    uint32_t union_ptr;
    uint32_t dwStateAction;
    uint32_t hWVTStateData;
    uint32_t pwszURLReference;
    uint32_t dwProvFlags;
    uint32_t dwUIContext;
    uint32_t pSignatureSettings;
};
static_assert(sizeof(WinTrustDataNative) == 52, "WINTRUST_DATA x86 size");

std::string wchar16_to_path(const uint16_t* w) {
    std::string out;
    if (!w) return out;
    while (*w) out.push_back(static_cast<char>(*w++ & 0xFF));
    return out;
}

bool extract_pe_pkcs7(const std::vector<uint8_t>& pe, std::vector<uint8_t>& out) {
    if (pe.size() < 0x40) return false;
    uint32_t e_lfanew = *reinterpret_cast<const uint32_t*>(pe.data() + 0x3C);
    if (e_lfanew + 24 + 4 > pe.size()) return false;
    if (memcmp(pe.data() + e_lfanew, "PE\0\0", 4) != 0) return false;
    uint16_t magic = *reinterpret_cast<const uint16_t*>(pe.data() + e_lfanew + 24);
    uint32_t secdir_off;
    if (magic == 0x10b) secdir_off = e_lfanew + 24 + 96 + 4 * 8;
    else if (magic == 0x20b) secdir_off = e_lfanew + 24 + 112 + 4 * 8;
    else return false;
    if (secdir_off + 8 > pe.size()) return false;
    uint32_t sec_rva = *reinterpret_cast<const uint32_t*>(pe.data() + secdir_off);
    uint32_t sec_size = *reinterpret_cast<const uint32_t*>(pe.data() + secdir_off + 4);
    if (!sec_rva || !sec_size || sec_rva + 8 > pe.size()) return false;
    uint32_t dwLength = *reinterpret_cast<const uint32_t*>(pe.data() + sec_rva);
    uint16_t wCertType = *reinterpret_cast<const uint16_t*>(pe.data() + sec_rva + 6);
    if (dwLength < 8 || sec_rva + dwLength > pe.size()) return false;
    if (wCertType != 0x0002) return false;
    out.assign(pe.begin() + sec_rva + 8, pe.begin() + sec_rva + dwLength);
    return true;
}

} // namespace

void* WINAPI_CALL THETAZ_CryptMemAlloc(ULONG cbSize) {
    void* p = alloc_guest_heap(cbSize ? cbSize : 1);
    return p;
}
void* WINAPI_CALL THETAZ_CryptMemFree(void* pv) {
    free_guest_heap(pv);
    return nullptr;
}

void* WINAPI_CALL THETAZ_CryptMsgOpenToDecode(DWORD dwMsgEncodingType, DWORD dwFlags, DWORD dwMsgType,
    HCRYPTPROV hCryptProv, void* pRecipientInfo, void* pStreamInfo) {
    (void)dwMsgEncodingType; (void)dwFlags; (void)dwMsgType; (void)hCryptProv; (void)pRecipientInfo; (void)pStreamInfo;
    uintptr_t h = g_msg_next.fetch_add(1);
    std::lock_guard<std::mutex> lock(g_msg_mutex);
    g_msgs[h] = HostCryptMsg{};
    return reinterpret_cast<void*>(h);
}
BOOL WINAPI_CALL THETAZ_CryptMsgUpdate(void* hCryptMsg, const BYTE* pbData, DWORD cbData, BOOL fFinal) {
    if (!hCryptMsg) return FALSE;
    std::lock_guard<std::mutex> lock(g_msg_mutex);
    auto it = g_msgs.find(reinterpret_cast<uintptr_t>(hCryptMsg));
    if (it == g_msgs.end()) return FALSE;
    if (pbData && cbData) it->second.acc.insert(it->second.acc.end(), pbData, pbData + cbData);
    if (fFinal && !it->second.acc.empty()) {
        const unsigned char* p = it->second.acc.data();
        it->second.p7 = d2i_PKCS7(nullptr, &p, static_cast<long>(it->second.acc.size()));
        it->second.final = true;
    }
    return TRUE;
}
BOOL WINAPI_CALL THETAZ_CryptMsgGetParam(void* hCryptMsg, DWORD dwParamType, DWORD dwIndex, void* pvData, DWORD* pcbData) {
    if (!hCryptMsg) return FALSE;
    uintptr_t h = reinterpret_cast<uintptr_t>(hCryptMsg);
    std::lock_guard<std::mutex> lock(g_msg_mutex);
    auto it = g_msgs.find(h);
    if (it == g_msgs.end()) return FALSE;
    HostCryptMsg& m = it->second;
    if (dwParamType == 4) {
        // CMSG_CONTENT_PARAM: return the accumulated raw message bytes
        DWORD need = static_cast<DWORD>(m.acc.size());
        if (pcbData) {
            if (!pvData) { *pcbData = need; return TRUE; }
            if (*pcbData < need) { *pcbData = need; return FALSE; }
            *pcbData = need;
        }
        if (pvData && need) memcpy(pvData, m.acc.data(), need);
        return TRUE;
    }
    if (dwParamType == 11) {
        // CMSG_CERT_COUNT_PARAM
        DWORD count = m.p7 ? static_cast<DWORD>(sk_X509_num(PKCS7_get0_signers(m.p7, nullptr, 0))) : 0;
        if (pcbData) {
            if (!pvData) { *pcbData = sizeof(DWORD); return TRUE; }
            if (*pcbData < sizeof(DWORD)) { *pcbData = sizeof(DWORD); return FALSE; }
            *pcbData = sizeof(DWORD);
        }
        if (pvData) *reinterpret_cast<DWORD*>(pvData) = count;
        return TRUE;
    }
    if (pcbData) *pcbData = 0;
    return FALSE;
}
BOOL WINAPI_CALL THETAZ_CryptMsgClose(void* hCryptMsg) {
    if (!hCryptMsg) return FALSE;
    uintptr_t h = reinterpret_cast<uintptr_t>(hCryptMsg);
    HostCryptMsg* m = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_msg_mutex);
        auto it = g_msgs.find(h);
        if (it == g_msgs.end()) return FALSE;
        m = &it->second;
    }
    if (m->p7) PKCS7_free(m->p7);
    std::lock_guard<std::mutex> lock(g_msg_mutex);
    g_msgs.erase(h);
    return TRUE;
}
void* WINAPI_CALL THETAZ_CryptMsgDuplicate(void* hCryptMsg) {
    if (!hCryptMsg) return nullptr;
    uintptr_t h = reinterpret_cast<uintptr_t>(hCryptMsg);
    std::lock_guard<std::mutex> lock(g_msg_mutex);
    auto it = g_msgs.find(h);
    if (it == g_msgs.end()) return nullptr;
    uintptr_t nh = g_msg_next.fetch_add(1);
    g_msgs[nh] = HostCryptMsg{it->second.acc, nullptr, it->second.final};
    if (it->second.p7) {
        // re-parse a dup
        const unsigned char* p = it->second.acc.data();
        g_msgs[nh].p7 = d2i_PKCS7(nullptr, &p, static_cast<long>(it->second.acc.size()));
    }
    return reinterpret_cast<void*>(nh);
}
BOOL WINAPI_CALL THETAZ_CryptMsgControl(void* hCryptMsg, DWORD dwFlags, DWORD dwCtrlType, const void* pvCtrlPara) {
    (void)hCryptMsg; (void)dwFlags; (void)dwCtrlType; (void)pvCtrlPara;
    return FALSE;
}

BOOL WINAPI_CALL THETAZ_CryptDecodeObjectEx(DWORD dwCertEncodingType, const char* lpszStructType,
    const BYTE* pbEncoded, DWORD cbEncoded, DWORD dwFlags, void* pDecodePara, void* pvStructInfo, DWORD* pcbStructInfo);

BOOL WINAPI_CALL THETAZ_CryptDecodeObject(DWORD dwCertEncodingType, const char* lpszStructType,
    const BYTE* pbEncoded, DWORD cbEncoded, DWORD dwFlags, void* pvStructInfo, DWORD* pcbStructInfo) {
    return THETAZ_CryptDecodeObjectEx(dwCertEncodingType, lpszStructType, pbEncoded, cbEncoded,
        dwFlags, nullptr, pvStructInfo, pcbStructInfo);
}

BOOL WINAPI_CALL THETAZ_CryptDecodeObjectEx(DWORD dwCertEncodingType, const char* lpszStructType,
    const BYTE* pbEncoded, DWORD cbEncoded, DWORD dwFlags, void* pDecodePara, void* pvStructInfo, DWORD* pcbStructInfo) {
    (void)dwCertEncodingType; (void)dwFlags; (void)pDecodePara;
    if (!lpszStructType || !pbEncoded || !cbEncoded) return FALSE;
    const unsigned char* p = pbEncoded;
    if (strcmp(lpszStructType, "PKCS7_SIGNER_INFO") == 0) {
        // peek: decode via PKCS7 not implemented per-signer here; return the raw blob size
        DWORD need = cbEncoded;
        if (pcbStructInfo) {
            if (!pvStructInfo) { *pcbStructInfo = need; return TRUE; }
            if (*pcbStructInfo < need) { *pcbStructInfo = need; return FALSE; }
            *pcbStructInfo = need;
        }
        if (pvStructInfo) memcpy(pvStructInfo, pbEncoded, need);
        return TRUE;
    }
    if (strcmp(lpszStructType, "X509_NAME") == 0) {
        X509_NAME* n = d2i_X509_NAME(nullptr, &p, cbEncoded);
        if (!n) return FALSE;
        int len = i2d_X509_NAME(n, nullptr);
        DWORD need = static_cast<DWORD>(len);
        if (pcbStructInfo) {
            if (!pvStructInfo) { *pcbStructInfo = need; X509_NAME_free(n); return TRUE; }
            if (*pcbStructInfo < need) { *pcbStructInfo = need; X509_NAME_free(n); return FALSE; }
            *pcbStructInfo = need;
        }
        if (pvStructInfo) {
            unsigned char* out = reinterpret_cast<unsigned char*>(pvStructInfo);
            unsigned char* q = out;
            i2d_X509_NAME(n, &q);
        }
        X509_NAME_free(n);
        return TRUE;
    }
    if (pcbStructInfo) *pcbStructInfo = 0;
    return FALSE;
}

BOOL WINAPI_CALL THETAZ_CryptEnumOIDInfo(DWORD dwGroupId, DWORD dwFlags, void* pvArg, void* pvFuncArg) {
    (void)dwGroupId; (void)dwFlags; (void)pvArg; (void)pvFuncArg;
    return FALSE;
}

BOOL WINAPI_CALL THETAZ_CryptRegisterOIDFunction(DWORD dwEncodingType, const char* pszFuncName,
    const char* pszOID, void* pwszDll, void* pszOverrideFuncName) {
    (void)dwEncodingType; (void)pszFuncName; (void)pszOID; (void)pwszDll; (void)pszOverrideFuncName;
    return TRUE;
}

BOOL WINAPI_CALL THETAZ_CryptSIPRetrieveSubjectGuid(const wchar_t* pwszFileName, void* hFileIn, void* pgSubjectGuid) {
    (void)pwszFileName; (void)hFileIn;
    if (!pgSubjectGuid) return FALSE;
    memcpy(pgSubjectGuid, kPeSubjectGuid, 16);
    return TRUE;
}

BOOL WINAPI_CALL THETAZ_CryptSIPLoad(void* pNewProv, DWORD dwReserved) {
    (void)pNewProv; (void)dwReserved;
    return FALSE;
}

BOOL WINAPI_CALL THETAZ_CryptSIPAddProvider(void* pNewProv) {
    (void)pNewProv;
    return FALSE;
}

static uint32_t build_prov_cert_ctx(uint32_t leaf_ctx) {
    // CRYPT_PROVIDER_CERT (44 bytes) with pCert = leaf_ctx
    uint32_t* s = static_cast<uint32_t*>(calloc_guest_heap(1, 44));
    if (!s) return 0;
    s[0] = 44;          // cbStruct
    s[1] = leaf_ctx;    // pCert
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(s));
}

void* WINAPI_CALL THETAZ_WTHelperGetProvCertFromChain(void* pProvData, DWORD idxCert) {
    if (!pProvData) return nullptr;
    uintptr_t h = reinterpret_cast<uintptr_t>(pProvData);
    uint32_t leaf = 0;
    {
        std::lock_guard<std::mutex> lock(g_cert_mutex);
        auto it = g_cert_chains.find(h);
        if (it != g_cert_chains.end()) leaf = it->second.leaf_ctx;
    }
    if (!leaf) return nullptr;
    (void)idxCert;
    return reinterpret_cast<void*>(static_cast<uintptr_t>(build_prov_cert_ctx(leaf)));
}

void* WINAPI_CALL THETAZ_WTHelperGetProvSignerFromChain(void* pProvData, DWORD idxSigner,
    BOOL fCounterSigner, DWORD idxCounterSigner) {
    (void)idxSigner; (void)fCounterSigner; (void)idxCounterSigner;
    // CRYPT_PROVIDER_SGNR (52 bytes); minimal: cbStruct + pasCsCertChain ptr
    if (!pProvData) return nullptr;
    uint32_t* s = static_cast<uint32_t*>(calloc_guest_heap(1, 52));
    if (!s) return nullptr;
    s[0] = 52;
    return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(s));
}

LONG WINAPI_CALL THETAZ_WinVerifyTrustEx(void* hwnd, void* pgActionID, void* pWVTData) {
    (void)hwnd; (void)pgActionID;
    if (!pWVTData) return 0x80070057; // E_INVALIDARG
    WinTrustDataNative* wtd = static_cast<WinTrustDataNative*>(pWVTData);
    if (wtd->dwUnionChoice != 1) return 0x8009002A; // no file => TRUST_E_NOSIGNATURE
    if (wtd->dwStateAction == 2) { // WTD_STATEACTION_CLOSE
        if (wtd->hWVTStateData) {
            std::lock_guard<std::mutex> lock(g_cert_mutex);
            g_cert_chains.erase(wtd->hWVTStateData);
        }
        return 0;
    }
    if (wtd->dwStateAction == 0) return 0; // WTD_STATEACTION_IGNORE

    WinTrustFileInfoNative* fi = reinterpret_cast<WinTrustFileInfoNative*>(static_cast<uintptr_t>(wtd->union_ptr));
    if (!fi || !fi->pcwszFilePath) return 0x80070057;
    std::string path = wchar16_to_path(reinterpret_cast<const uint16_t*>(static_cast<uintptr_t>(fi->pcwszFilePath)));
    if (path.empty()) return 0x80070002;

    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) return 0x80070002;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::vector<uint8_t> pe(static_cast<size_t>(sz));
    if (sz > 0) fread(pe.data(), 1, static_cast<size_t>(sz), fp);
    fclose(fp);

    std::vector<uint8_t> p7der;
    if (!extract_pe_pkcs7(pe, p7der)) return 0x8009002A; // TRUST_E_NOSIGNATURE

    const unsigned char* p = p7der.data();
    PKCS7* p7 = d2i_PKCS7(nullptr, &p, static_cast<long>(p7der.size()));
    if (!p7) return 0x80092006; // TRUST_E_BAD_DIGEST

    STACK_OF(X509)* signers = PKCS7_get0_signers(p7, nullptr, 0);
    LONG result = 0x800B0100; // TRUST_E_NO_SIGNER_CERT
    if (signers && sk_X509_num(signers) > 0) {
        X509* leaf = sk_X509_value(signers, 0);
        X509_up_ref(leaf);
        X509_STORE* store = X509_STORE_new();
        {
            std::lock_guard<std::mutex> lock(g_cert_mutex);
            for (const auto& kv : g_cert_stores)
                for (X509* x : kv.second.certs) X509_STORE_add_cert(store, x);
        }
        X509_STORE_CTX* vctx = X509_STORE_CTX_new();
        X509_STORE_CTX_init(vctx, store, leaf, nullptr);
        int rc = X509_verify_cert(vctx);
        bool trusted = (rc == 1);
        X509_STORE_CTX_free(vctx);
        X509_STORE_free(store);
        X509_free(leaf);

        uintptr_t h = g_cert_next_chain.fetch_add(1);
        {
            std::lock_guard<std::mutex> lock(g_cert_mutex);
            HostChain ch;
            ch.trusted = trusted;
            ch.leaf_ctx = build_cert_context(sk_X509_value(signers, 0), 0);
            g_cert_chains[h] = ch;
        }
        wtd->hWVTStateData = static_cast<uint32_t>(h);
        result = trusted ? 0 : 0x800B0109; // CERT_E_UNTRUSTEDROOT
    }
    PKCS7_free(p7);
    return result;
}

} // namespace ThetaZ
