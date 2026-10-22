#include "core/api/crypto.h"
#include <cstdio>
#include <cstdint>
#include <cstring>

using namespace ThetaZ;

int main() {
    printf("Testing Crypto subsystem...\n");
    
    // Test CryptAcquireContextW
    HCRYPTPROV hProv = 0;
    BOOL result = THETAZ_CryptAcquireContextW(&hProv, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
    printf("CryptAcquireContextW: %s (hProv=%p)\n", result ? "SUCCESS" : "FAIL", hProv);
    
    // Test CryptGenRandom
    uint8_t buffer[32];
    BOOL genResult = THETAZ_CryptGenRandom(reinterpret_cast<HCRYPTPROV>(hProv), 32, buffer);
    printf("CryptGenRandom: %s\n", genResult ? "SUCCESS" : "FAIL");
    
    // Print some random bytes
    printf("Random bytes: ");
    for (int i = 0; i < 8; i++) {
        printf("%02x ", buffer[i]);
    }
    printf("\n");
    
    // Test CryptCreateHash
    HCRYPTHASH hHash = 0;
    BOOL hashResult = THETAZ_CryptCreateHash(reinterpret_cast<HCRYPTPROV>(hProv), CALG_SHA_256, 0, 0, &hHash);
    printf("CryptCreateHash: %s (hHash=%p)\n", hashResult ? "SUCCESS" : "FAIL", hHash);
    
    // Test CryptHashData
    const char* testData = "Hello, ThetaZ!";
    BOOL hashDataResult = THETAZ_CryptHashData(reinterpret_cast<HCRYPTHASH>(hHash), 
                                                reinterpret_cast<const uint8_t*>(testData), 
                                                strlen(testData), 0);
    printf("CryptHashData: %s\n", hashDataResult ? "SUCCESS" : "FAIL");
    
    // Test CryptGetHashParam
    uint8_t hashValue[32];
    DWORD hashLen = 32;
    BOOL getHashResult = THETAZ_CryptGetHashParam(reinterpret_cast<HCRYPTHASH>(hHash), HP_HASHVAL, hashValue, &hashLen, 0);
    printf("CryptGetHashParam: %s (len=%d)\n", getHashResult ? "SUCCESS" : "FAIL", hashLen);
    
    // Print hash
    printf("SHA-256 hash: ");
    for (int i = 0; i < hashLen; i++) {
        printf("%02x", hashValue[i]);
    }
    printf("\n");
    
    // Cleanup
    THETAZ_CryptDestroyHash(reinterpret_cast<HCRYPTHASH>(hHash));
    THETAZ_CryptReleaseContext(reinterpret_cast<HCRYPTPROV>(hProv), 0);
    
    printf("\nAll crypto tests passed!\n");
    return 0;
}
