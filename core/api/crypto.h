#pragma once

#include "core/api/kernel32.h"

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#define STATUS_NOT_IMPLEMENTED ((NTSTATUS)0xC0000002L)
#define STATUS_UNSUCCESSFUL ((NTSTATUS)0xC0000001L)
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xC000000DL)
#endif

namespace ThetaZ {

// =========================================================================
// Crypto/CAPI types and constants
// =========================================================================

// Provider types
enum {
    PROV_RSA_FULL = 1,
    PROV_RSA_SIG = 2,
    PROV_DSS = 3,
    PROV_FORTEZZA = 4,
    PROV_RSA_AES = 24,
};

// Algorithm IDs
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

// Key specs
enum {
    AT_KEYEXCHANGE = 1,
    AT_SIGNATURE = 2,
};

// Key blob types
enum {
    SIMPLEBLOB = 1,
    PUBLICKEYBLOB = 6,
    PRIVATEKEYBLOB = 7,
    PLAINTEXTKEYBLOB = 8,
    OPAQUEKEYBLOB = 9,
    PUBLICKEYBLOBEX = 10,
    SYMMETRICWRAPKEYBLOB = 11,
};

// Provider flags
enum {
    CRYPT_VERIFYCONTEXT = 0xF0000000,
    CRYPT_NEWKEYSET = 0x00000008,
    CRYPT_DELETEKEYSET = 0x00000010,
    CRYPT_MACHINE_KEYSET = 0x00000020,
    CRYPT_SILENT = 0x00000040,
    CRYPT_DEFAULT_CONTAINER_OPTIONAL = 0x00000080,
};

// Hash parameter IDs
enum {
    HP_HASHVAL = 2,
    HP_HASHSIZE = 4,
};

// Key parameter IDs
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

// Crypto object types
enum CryptoObjectType {
    CRYPTO_OBJ_PROVIDER = 1,
    CRYPTO_OBJ_HASH = 2,
    CRYPTO_OBJ_KEY = 3,
};

// Forward declarations
struct CryptoProvider;
struct CryptoHash;
struct CryptoKey;

using HCRYPTPROV = void*;
using HCRYPTHASH = void*;
using HCRYPTKEY = void*;

// =========================================================================
// advapi32.dll Crypto API
// =========================================================================

BOOL WINAPI_CALL THETAZ_CryptAcquireContextW(HCRYPTPROV* phProv, const wchar_t* szContainer,
    const wchar_t* szProvider, DWORD dwProvType, DWORD dwFlags);

BOOL WINAPI_CALL THETAZ_CryptContextAddRef(HCRYPTPROV hProv, DWORD* pdwReserved, DWORD dwFlags);

BOOL WINAPI_CALL THETAZ_CryptReleaseContext(HCRYPTPROV hProv, DWORD dwFlags);

BOOL WINAPI_CALL THETAZ_CryptCreateHash(HCRYPTPROV hProv, DWORD Algid, HCRYPTKEY hKey, DWORD dwFlags, HCRYPTHASH* phHash);

BOOL WINAPI_CALL THETAZ_CryptDestroyHash(HCRYPTHASH hHash);

BOOL WINAPI_CALL THETAZ_CryptHashData(HCRYPTHASH hHash, const uint8_t* pbData, DWORD dwDataLen, DWORD dwFlags);

BOOL WINAPI_CALL THETAZ_CryptGetHashParam(HCRYPTHASH hHash, DWORD dwParam, uint8_t* pbData, LPDWORD pcbData, DWORD dwFlags);

BOOL WINAPI_CALL THETAZ_CryptGetProvParam(HCRYPTPROV hProv, DWORD dwParam, uint8_t* pbData, LPDWORD pcbData, DWORD dwFlags);

BOOL WINAPI_CALL THETAZ_CryptSetProvParam(HCRYPTPROV hProv, DWORD dwParam, const uint8_t* pbData, DWORD dwFlags);

BOOL WINAPI_CALL THETAZ_CryptGenRandom(HCRYPTPROV hProv, DWORD dwLen, uint8_t* pbBuffer);

BOOL WINAPI_CALL THETAZ_CryptGenKey(HCRYPTPROV hProv, DWORD Algid, DWORD dwFlags, HCRYPTKEY* phKey);

BOOL WINAPI_CALL THETAZ_CryptDestroyKey(HCRYPTKEY hKey);

BOOL WINAPI_CALL THETAZ_CryptGetKeyParam(HCRYPTKEY hKey, DWORD dwParam, uint8_t* pbData, LPDWORD pcbData, DWORD dwFlags);

BOOL WINAPI_CALL THETAZ_CryptExportKey(HCRYPTKEY hKey, HCRYPTKEY hExpKey, DWORD dwBlobType, DWORD dwFlags,
    uint8_t* pbData, LPDWORD pdwDataLen);

BOOL WINAPI_CALL THETAZ_CryptImportKey(HCRYPTPROV hProv, const uint8_t* pbData, DWORD dwDataLen,
    HCRYPTKEY hPubKey, DWORD dwFlags, HCRYPTKEY* phKey);

BOOL WINAPI_CALL THETAZ_CryptEncrypt(HCRYPTKEY hKey, HCRYPTHASH hHash, BOOL final, DWORD dwFlags,
    uint8_t* pbData, LPDWORD pdwDataLen, DWORD dwBufLen);

BOOL WINAPI_CALL THETAZ_CryptDecrypt(HCRYPTKEY hKey, HCRYPTHASH hHash, BOOL final, DWORD dwFlags,
    uint8_t* pbData, LPDWORD pdwDataLen);

BOOL WINAPI_CALL THETAZ_CryptDeriveKey(HCRYPTPROV hProv, DWORD Algid, HCRYPTKEY hBaseData, DWORD dwFlags, HCRYPTKEY* phKey);

BOOL WINAPI_CALL THETAZ_CryptSignHashW(HCRYPTHASH hHash, DWORD dwKeySpec, const wchar_t* szDescription,
    DWORD dwFlags, uint8_t* pbSignature, LPDWORD pdwSigLen);

BOOL WINAPI_CALL THETAZ_CryptVerifySignatureW(HCRYPTHASH hHash, const uint8_t* pbSignature, DWORD dwSigLen,
    HCRYPTKEY hPubKey, const wchar_t* szDescription, DWORD dwFlags);

BOOL WINAPI_CALL THETAZ_CryptEnumProvidersW(DWORD dwIndex, DWORD* pdwReserved, DWORD dwFlags,
    DWORD* pdwProvType, wchar_t* szProvName, LPDWORD pcchProvName);

BOOL WINAPI_CALL THETAZ_CryptGetUserKey(HCRYPTPROV hProv, DWORD dwKeySpec, HCRYPTKEY* phKey);

BOOL WINAPI_CALL THETAZ_CryptProtectMemory(void* pvData, DWORD cbData, DWORD dwFlags);

BOOL WINAPI_CALL THETAZ_CryptUnprotectMemory(void* pvData, DWORD cbData, DWORD dwFlags);

BOOL WINAPI_CALL THETAZ_CryptProtectMemory(void* pvData, DWORD cbData, DWORD dwFlags);

BOOL WINAPI_CALL THETAZ_CryptUnprotectMemory(void* pvData, DWORD cbData, DWORD dwFlags);

// =========================================================================
// MD5 API (legacy)
// =========================================================================

BOOL WINAPI_CALL THETAZ_CryptMD5Init(HCRYPTHASH* phHash);

BOOL WINAPI_CALL THETAZ_CryptMD5Update(HCRYPTHASH hHash, const uint8_t* pbData, DWORD dwDataLen);

BOOL WINAPI_CALL THETAZ_CryptMD5Final(HCRYPTHASH hHash, uint8_t* pbDigest);

// =========================================================================
// Certificate Store API (crypt32.dll)
// =========================================================================

HANDLE WINAPI_CALL THETAZ_CertOpenSystemStoreW(HANDLE hprov, LPCWSTR szSubsystemProtocol);

void* WINAPI_CALL THETAZ_CertFindCertificateInStore(HANDLE hCertStore, DWORD dwCertEncodingType,
    DWORD dwFindFlags, DWORD dwFindType, const void* pvFindPara, void* pCertContext);

void WINAPI_CALL THETAZ_CertFreeCertificateContext(void* pCertContext);

BOOL WINAPI_CALL THETAZ_CertDeleteCertificateFromStore(void* pCertContext);

void* WINAPI_CALL THETAZ_CertEnumCertificatesInStore(HANDLE hCertStore, void* pPrevCertContext);

BOOL WINAPI_CALL THETAZ_CertVerifyCertificateChainPolicy(void* pszPolicyOID, void* pChainContext,
    void* pPolicyPara, void* pPolicyStatus);

BOOL WINAPI_CALL THETAZ_CertCompareCertificate(DWORD dwCertEncodingType, void* pCertContext1, void* pCertContext2);

void* WINAPI_CALL THETAZ_CertOpenStore(LPCSTR szStoreProvider, DWORD dwEncodingType,
    HCRYPTPROV hCryptProv, DWORD dwFlags, const void* pvPara);

BOOL WINAPI_CALL THETAZ_CertCloseStore(void* hCertStore, DWORD dwFlags);

BOOL WINAPI_CALL THETAZ_CertAddEncodedCertificateToStore(void* hCertStore, DWORD dwCertEncodingType,
    const BYTE* pbCertEncoded, DWORD cbCertEncoded, DWORD dwAddDisposition, void** ppCertContext);

void* WINAPI_CALL THETAZ_CertDuplicateCertificateContext(void* pCertContext);

void* WINAPI_CALL THETAZ_CertDuplicateStore(void* hCertStore);

BOOL WINAPI_CALL THETAZ_CertAddStoreToCollection(void* hCollectionStore, void* hSiblingStore,
    DWORD dwUpdateFlags, DWORD dwPriority);

BOOL WINAPI_CALL THETAZ_CertCompareCertificateName(DWORD dwCertEncodingType, void* pCertName1, void* pCertName2);

BOOL WINAPI_CALL THETAZ_CertCompareIntegerBlob(void* pIntBlob1, void* pIntBlob2);

void* WINAPI_CALL THETAZ_CertFindExtension(const char* pszObjId, DWORD cExtension, void* rgExtension);

BOOL WINAPI_CALL THETAZ_CertGetCertificateContextProperty(void* pCertContext, DWORD dwPropId,
    void* pvData, DWORD* pcbData);

void* WINAPI_CALL THETAZ_CertGetSubjectCertificateFromStore(void* hCertStore, DWORD dwCertEncodingType,
    void* pCertId);

BOOL WINAPI_CALL THETAZ_CertGetCertificateChain(void* hChainEngine, void* pCertContext,
    void* pTime, void* hAdditionalStore, void* pChainPara, DWORD dwFlags, void* pvReserved, void** ppChainContext);

void WINAPI_CALL THETAZ_CertFreeCertificateChain(void* pChainContext);

DWORD WINAPI_CALL THETAZ_CertOIDToAlgId(const char* pszObjId);

void* WINAPI_CALL THETAZ_CryptMemAlloc(ULONG cbSize);
void* WINAPI_CALL THETAZ_CryptMemFree(void* pv);
void* WINAPI_CALL THETAZ_CryptMsgOpenToDecode(DWORD dwMsgEncodingType, DWORD dwFlags, DWORD dwMsgType,
    HCRYPTPROV hCryptProv, void* pRecipientInfo, void* pStreamInfo);
BOOL WINAPI_CALL THETAZ_CryptMsgUpdate(void* hCryptMsg, const BYTE* pbData, DWORD cbData, BOOL fFinal);
BOOL WINAPI_CALL THETAZ_CryptMsgGetParam(void* hCryptMsg, DWORD dwParamType, DWORD dwIndex, void* pvData, DWORD* pcbData);
BOOL WINAPI_CALL THETAZ_CryptMsgClose(void* hCryptMsg);
void* WINAPI_CALL THETAZ_CryptMsgDuplicate(void* hCryptMsg);
BOOL WINAPI_CALL THETAZ_CryptMsgControl(void* hCryptMsg, DWORD dwFlags, DWORD dwCtrlType, const void* pvCtrlPara);
BOOL WINAPI_CALL THETAZ_CryptDecodeObject(DWORD dwCertEncodingType, const char* lpszStructType,
    const BYTE* pbEncoded, DWORD cbEncoded, DWORD dwFlags, void* pvStructInfo, DWORD* pcbStructInfo);
BOOL WINAPI_CALL THETAZ_CryptDecodeObjectEx(DWORD dwCertEncodingType, const char* lpszStructType,
    const BYTE* pbEncoded, DWORD cbEncoded, DWORD dwFlags, void* pDecodePara, void* pvStructInfo, DWORD* pcbStructInfo);
BOOL WINAPI_CALL THETAZ_CryptEnumOIDInfo(DWORD dwGroupId, DWORD dwFlags, void* pvArg, void* pvFuncArg);
BOOL WINAPI_CALL THETAZ_CryptRegisterOIDFunction(DWORD dwEncodingType, const char* pszFuncName,
    const char* pszOID, void* pwszDll, void* pszOverrideFuncName);
BOOL WINAPI_CALL THETAZ_CryptSIPRetrieveSubjectGuid(const wchar_t* pwszFileName, void* hFileIn, void* pgSubjectGuid);
BOOL WINAPI_CALL THETAZ_CryptSIPLoad(void* pNewProv, DWORD dwReserved);
BOOL WINAPI_CALL THETAZ_CryptSIPAddProvider(void* pNewProv);
LONG WINAPI_CALL THETAZ_WinVerifyTrustEx(void* hwnd, void* pgActionID, void* pWVTData);
void* WINAPI_CALL THETAZ_WTHelperGetProvCertFromChain(void* pProvData, DWORD idxCert);
void* WINAPI_CALL THETAZ_WTHelperGetProvSignerFromChain(void* pProvData, DWORD idxSigner,
    BOOL fCounterSigner, DWORD idxCounterSigner);

BOOL WINAPI_CALL THETAZ_CryptProtectMemory(void* pvData, DWORD cbData, DWORD dwFlags);

BOOL WINAPI_CALL THETAZ_CryptUnprotectMemory(void* pvData, DWORD cbData, DWORD dwFlags);

HRESULT WINAPI_CALL THETAZ_CertSelectionGetSerializedBlob(void* pCertContext, void** ppCertBlob);

int WINAPI_CALL THETAZ_CryptUIDlgSelectCertificateFromStore(void* hCertStore, void* hwnd,
    void* pwszTitle, void* pwszDisplayString, DWORD dwDontUseColumn, DWORD dwFlags, void* pvReserved);

// =========================================================================
// BCrypt API (bcrypt.dll)
// =========================================================================

NTSTATUS WINAPI_CALL THETAZ_BCryptOpenAlgorithmProvider(void** phAlgorithm, LPCWSTR pszAlgId,
    LPCWSTR pszImplementation, DWORD dwFlags);

NTSTATUS WINAPI_CALL THETAZ_BCryptGetProperty(void* hObject, LPCWSTR pszProperty, void* pbOutput,
    DWORD cbOutput, DWORD* pcbResult, DWORD dwFlags);

NTSTATUS WINAPI_CALL THETAZ_BCryptGenRandom(void* hObject, void* pbBuffer, ULONG cbBuffer, DWORD dwFlags);

NTSTATUS WINAPI_CALL THETAZ_BCryptEncrypt(void* hKey, void* pbInput, DWORD cbInput, void* pPaddingInfo,
    void* pbIV, DWORD cbIV, void* pbOutput, DWORD cbOutput, DWORD* pcbResult, DWORD dwFlags);

NTSTATUS WINAPI_CALL THETAZ_BCryptSignHash(void* hKey, void* pPaddingInfo, void* pbInput, DWORD cbInput,
    void* pbSignature, DWORD cbSignature, DWORD* pcbResult, DWORD dwFlags);

NTSTATUS WINAPI_CALL THETAZ_BCryptCloseAlgorithmProvider(void* hAlgorithm, DWORD dwFlags);

NTSTATUS WINAPI_CALL THETAZ_BCryptDecrypt(void* hKey, void* pbInput, DWORD cbInput, void* pPaddingInfo,
    void* pbIV, DWORD cbIV, void* pbOutput, DWORD cbOutput, DWORD* pcbResult, DWORD dwFlags);

NTSTATUS WINAPI_CALL THETAZ_BCryptCreateHash(void* hAlgorithm, void** phHash, void* pbHashObject,
    DWORD cbHashObject, void* pbSecret, DWORD cbSecret, DWORD dwFlags);

NTSTATUS WINAPI_CALL THETAZ_BCryptHashData(void* hHash, void* pbInput, DWORD cbInput, DWORD dwFlags);

NTSTATUS WINAPI_CALL THETAZ_BCryptFinishHash(void* hHash, void* pbOutput, DWORD cbOutput, DWORD dwFlags);

NTSTATUS WINAPI_CALL THETAZ_BCryptDestroyHash(void* hHash);

NTSTATUS WINAPI_CALL THETAZ_BCryptDestroyKey(void* hKey);

NTSTATUS WINAPI_CALL THETAZ_BCryptDuplicateHash(void* hHash, void** phHash, void* pbHashObject,
    DWORD cbHashObject, DWORD dwFlags);

NTSTATUS WINAPI_CALL THETAZ_BCryptDuplicateKey(void* hKey, void** phKey, void* pbKeyObject,
    DWORD cbKeyObject, DWORD dwFlags);

NTSTATUS WINAPI_CALL THETAZ_BCryptDeriveKey(void* hSharedSecret, LPCWSTR pszKDF, void* pParameterList,
    void* pbDerivedKey, DWORD cbDerivedKey, DWORD* pcbResult, DWORD dwFlags);

NTSTATUS WINAPI_CALL THETAZ_BCryptImportKey(void* hAlgorithm, void* hImportKey, LPCWSTR pszBlobType,
    void* phKey, void* pbKeyObject, DWORD cbKeyObject, void* pbInput, DWORD cbInput, DWORD dwFlags);

NTSTATUS WINAPI_CALL THETAZ_BCryptExportKey(void* hKey, void* hExportKey, LPCWSTR pszBlobType,
    void* pbOutput, DWORD cbOutput, DWORD* pcbResult, DWORD dwFlags);

NTSTATUS WINAPI_CALL THETAZ_BCryptKeyDerivation(void* hKey, void* pbSecret, DWORD cbSecret,
    void* pbDerivedKey, DWORD cbDerivedKey, DWORD* pcbResult, DWORD dwFlags);

NTSTATUS WINAPI_CALL THETAZ_BCryptImportKeyPair(void* hAlgorithm, void* hImportKey,
    LPCWSTR pszBlobType, void** phKey, void* pbInput, DWORD cbInput, DWORD dwFlags);

NTSTATUS WINAPI_CALL THETAZ_BCryptVerifySignature(void* hKey, void* pPaddingInfo,
    void* pbHash, DWORD cbHash, void* pbSignature, DWORD cbSignature, DWORD dwFlags);

// =========================================================================
// NCrypt API (ncrypt.dll)
// =========================================================================

NTSTATUS WINAPI_CALL THETAZ_NCryptOpenStorageProvider(void** phProvider, LPCWSTR pszProviderName, LPCWSTR pszProviderNameDisplay, DWORD dwFlags);
NTSTATUS WINAPI_CALL THETAZ_NCryptSignHash(void* hKey, void* pPaddingInfo, void* pbInput, DWORD cbInput,
    void* pbSignature, DWORD cbSignature, DWORD* pcbResult, DWORD dwFlags);
NTSTATUS WINAPI_CALL THETAZ_NCryptSetProperty(void* hObject, LPCWSTR pszProperty, void* pbInput, DWORD cbInput, DWORD dwFlags);
NTSTATUS WINAPI_CALL THETAZ_NCryptFinalizeKey(void* hKey, DWORD dwFlags);
NTSTATUS WINAPI_CALL THETAZ_NCryptExportKey(void* hKey, void* hExportKey, LPCWSTR pszBlobType,
    void* pParameterList, void* pbOutput, DWORD cbOutput, DWORD* pcbResult, DWORD dwFlags);
NTSTATUS WINAPI_CALL THETAZ_NCryptImportKey(void* hProvider, void* hImportKey, LPCWSTR pszBlobType,
    void* phKey, void* pbKeyObject, DWORD cbKeyObject, void* pbInput, DWORD cbInput, DWORD dwFlags);
NTSTATUS WINAPI_CALL THETAZ_NCryptDeleteKey(void* hKey, DWORD dwFlags);
NTSTATUS WINAPI_CALL THETAZ_NCryptFreeObject(void* hObject);

// =========================================================================
// WinHTTP API (winhttp.dll)
// =========================================================================

HINTERNET WINAPI_CALL THETAZ_WinHttpOpen(LPCWSTR pwszUserAgent, DWORD dwAccessType, LPCWSTR pwszProxyName, LPCWSTR pwszProxyBypass, DWORD dwFlags);
HINTERNET WINAPI_CALL THETAZ_WinHttpConnect(HINTERNET hSession, LPCWSTR pswzServerName, INTERNET_PORT nServerPort, DWORD dwReserved);
HINTERNET WINAPI_CALL THETAZ_WinHttpOpenRequest(HINTERNET hConnect, LPCWSTR pwszVerb, LPCWSTR pwszObjectName,
    LPCWSTR pwszVersion, LPCWSTR pwszReferrer, LPCWSTR* ppwszAcceptTypes, DWORD dwFlags);
BOOL WINAPI_CALL THETAZ_WinHttpSendRequest(HINTERNET hRequest, LPCWSTR lpszHeaders, DWORD dwHeadersLength,
    LPVOID lpOptional, DWORD dwOptionalLength, DWORD dwTotalLength, DWORD_PTR dwContext);
BOOL WINAPI_CALL THETAZ_WinHttpReceiveResponse(HINTERNET hRequest, LPVOID lpReserved);
BOOL WINAPI_CALL THETAZ_WinHttpQueryDataAvailable(HINTERNET hRequest, LPDWORD lpdwNumberOfBytesAvailable);
BOOL WINAPI_CALL THETAZ_WinHttpReadData(HINTERNET hRequest, LPVOID lpBuffer, DWORD dwNumberOfBytesToRead, LPDWORD lpdwNumberOfBytesRead);
BOOL WINAPI_CALL THETAZ_WinHttpCloseHandle(HINTERNET hInternet);
BOOL WINAPI_CALL THETAZ_WinHttpSetCredentials(HINTERNET hRequest, DWORD dwAuthScheme, LPCWSTR pwszUserName,
    LPCWSTR pwszPassword, LPVOID pAuthParams);
BOOL WINAPI_CALL THETAZ_WinHttpGetIEProxyConfigForCurrentUser(WINHTTP_CURRENT_USER_IE_PROXY_CONFIG* pProxyConfig);

// =========================================================================
// WaveOut API (winmm.dll)
// =========================================================================

DWORD WINAPI_CALL THETAZ_waveOutGetNumDevs();


// =========================================================================
// NCrypt API (ncrypt.dll)
// =========================================================================

SECURITY_STATUS WINAPI_CALL THETAZ_NCryptOpenStorageProvider(NCRYPT_PROV_HANDLE* phProvider, LPCWSTR pszProviderName, DWORD dwFlags);
SECURITY_STATUS WINAPI_CALL THETAZ_NCryptSignHash(NCRYPT_KEY_HANDLE hKey, VOID* pPaddingInfo, BYTE* pbHashValue, DWORD cbHashValue, BYTE* pbSignature, DWORD cbSignature, DWORD* pcbResult, DWORD dwFlags);
SECURITY_STATUS WINAPI_CALL THETAZ_NCryptSetProperty(NCRYPT_HANDLE hObject, LPCWSTR pszProperty, PBYTE pbInput, DWORD cbInput, DWORD dwFlags);
SECURITY_STATUS WINAPI_CALL THETAZ_NCryptFinalizeKey(NCRYPT_KEY_HANDLE hKey, DWORD dwFlags);
SECURITY_STATUS WINAPI_CALL THETAZ_NCryptExportKey(NCRYPT_KEY_HANDLE hKey, NCRYPT_KEY_HANDLE hExportKey, LPCWSTR pszBlobType,
    NCryptBufferDesc* pParameterList, PBYTE pbOutput, DWORD cbOutput, DWORD* pcbResult, DWORD dwFlags);
SECURITY_STATUS WINAPI_CALL THETAZ_NCryptImportKey(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE hImportKey, LPCWSTR pszBlobType,
    NCRYPT_KEY_HANDLE* phKey, VOID* pbKeyObject, DWORD cbKeyObject, PBYTE pbInput, DWORD cbInput, DWORD dwFlags);
SECURITY_STATUS WINAPI_CALL THETAZ_NCryptDeleteKey(NCRYPT_KEY_HANDLE hKey, DWORD dwFlags);
SECURITY_STATUS WINAPI_CALL THETAZ_NCryptFreeObject(NCRYPT_HANDLE hObject);

} // namespace ThetaZ

// =========================================================================
// WaveOut API (winmm.dll)
