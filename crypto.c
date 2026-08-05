// crypto.c --- Optional AES-256-GCM resource encryption for WonRes
// Author: katahiromz
// License: MIT
#include <windows.h>
#include <bcrypt.h>
#include "WonRes.h"
#ifdef WONRES_ENABLE_CRYPTO
#include "WonCryptoP.h"
#endif

#pragma comment(lib, "bcrypt.lib")

#define WON_CRYPT_MAGIC        "WONRSRC1" // 8 bytes, no NUL terminator stored
#define WON_CRYPT_VERSION      1
#define WON_CRYPT_NONCE_SIZE   12
#define WON_CRYPT_TAG_SIZE     16

#pragma pack(push, 1)
typedef struct WON_CRYPT_HEADER {
    CHAR  szMagic[8];
    BYTE  bVersion;
    BYTE  abReserved[3];
    BYTE  abNonce[WON_CRYPT_NONCE_SIZE];
    BYTE  abTag[WON_CRYPT_TAG_SIZE];
    DWORD cbPlain;
} WON_CRYPT_HEADER, *PWON_CRYPT_HEADER;
#pragma pack(pop)

static SRWLOCK g_KeyLock = SRWLOCK_INIT;
static BYTE g_abKey[WON_ENCRYPTION_KEY_SIZE];
static BOOL g_bKeySet = FALSE;

static BOOL GetActiveKey(BYTE abKeyOut[WON_ENCRYPTION_KEY_SIZE])
{
    BOOL bSet;
    AcquireSRWLockShared(&g_KeyLock);
    bSet = g_bKeySet;
    if (bSet)
        CopyMemory(abKeyOut, g_abKey, WON_ENCRYPTION_KEY_SIZE);
    ReleaseSRWLockShared(&g_KeyLock);
    return bSet;
}

BOOL WONAPI WonSetEncryptionKey(const BYTE *pbKey, DWORD cbKey)
{
    if (!pbKey || cbKey != WON_ENCRYPTION_KEY_SIZE)
        return FALSE;

    AcquireSRWLockExclusive(&g_KeyLock);
    CopyMemory(g_abKey, pbKey, WON_ENCRYPTION_KEY_SIZE);
    g_bKeySet = TRUE;
    ReleaseSRWLockExclusive(&g_KeyLock);
    return TRUE;
}

BOOL WONAPI WonSetEncryptionPasswordW(LPCWSTR pszPassword, const BYTE *pbSalt, DWORD cbSalt,
                                      DWORD dwIterations)
{
    if (!pszPassword || !pszPassword[0] || !pbSalt || cbSalt < 16 ||
        dwIterations < WON_ENCRYPTION_MIN_ITERATIONS)
        return FALSE;

    BOOL bResult = FALSE;
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BYTE abDerived[WON_ENCRYPTION_KEY_SIZE];

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL,
                                    BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0)
        return FALSE;

    if (BCryptDeriveKeyPBKDF2(hAlg, (PUCHAR)pszPassword,
                              (ULONG)(lstrlenW(pszPassword) * sizeof(WCHAR)), (PUCHAR)pbSalt,
                              cbSalt, dwIterations, abDerived, sizeof(abDerived), 0) == 0) {
        AcquireSRWLockExclusive(&g_KeyLock);
        CopyMemory(g_abKey, abDerived, sizeof(abDerived));
        g_bKeySet = TRUE;
        ReleaseSRWLockExclusive(&g_KeyLock);
        bResult = TRUE;
    }

    SecureZeroMemory(abDerived, sizeof(abDerived));
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return bResult;
}

BOOL WONAPI WonSetEncryptionPasswordA(LPCSTR pszPassword, const BYTE *pbSalt, DWORD cbSalt,
                                      DWORD dwIterations)
{
    if (!pszPassword)
        return FALSE;

    int cch = MultiByteToWideChar(CP_UTF8, 0, pszPassword, -1, NULL, 0);
    if (cch <= 0)
        return FALSE;

    LPWSTR pszPasswordW = (LPWSTR)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)cch * sizeof(WCHAR));
    if (!pszPasswordW)
        return FALSE;

    MultiByteToWideChar(CP_UTF8, 0, pszPassword, -1, pszPasswordW, cch);
    BOOL bResult = WonSetEncryptionPasswordW(pszPasswordW, pbSalt, cbSalt, dwIterations);

    SecureZeroMemory(pszPasswordW, (SIZE_T)cch * sizeof(WCHAR));
    HeapFree(GetProcessHeap(), 0, pszPasswordW);
    return bResult;
}

VOID WONAPI WonClearEncryptionKey(VOID)
{
    AcquireSRWLockExclusive(&g_KeyLock);
    SecureZeroMemory(g_abKey, sizeof(g_abKey));
    g_bKeySet = FALSE;
    ReleaseSRWLockExclusive(&g_KeyLock);
}

BOOL WONAPI WonIsEncryptionKeySet(VOID)
{
    BOOL bSet;
    AcquireSRWLockShared(&g_KeyLock);
    bSet = g_bKeySet;
    ReleaseSRWLockShared(&g_KeyLock);
    return bSet;
}

VOID WONAPI WonFreeResourceMemory(LPVOID pMemory)
{
    if (pMemory)
        HeapFree(GetProcessHeap(), 0, pMemory);
}

BOOL WonCryptIsEncryptedBlob(const BYTE *pbData)
{
    if (!pbData)
        return FALSE;
    return memcmp(pbData, WON_CRYPT_MAGIC, sizeof(WON_CRYPT_MAGIC) - 1) == 0 &&
           pbData[8] == WON_CRYPT_VERSION;
}

// Common AES-256-GCM setup; returns opened alg/key or FALSE on failure.
static BOOL OpenGcmKey(const BYTE abKey[WON_ENCRYPTION_KEY_SIZE], BCRYPT_ALG_HANDLE *phAlg,
                       BCRYPT_KEY_HANDLE *phKey, PBYTE *ppbKeyObject)
{
    DWORD cbKeyObject = 0, cbResult = 0;

    *phAlg = NULL;
    *phKey = NULL;
    *ppbKeyObject = NULL;

    if (BCryptOpenAlgorithmProvider(phAlg, BCRYPT_AES_ALGORITHM, NULL, 0) != 0)
        return FALSE;
    if (BCryptSetProperty(*phAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
                          sizeof(BCRYPT_CHAIN_MODE_GCM), 0) != 0)
        return FALSE;

    BCryptGetProperty(*phAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&cbKeyObject, sizeof(cbKeyObject),
                      &cbResult, 0);
    *ppbKeyObject = (PBYTE)HeapAlloc(GetProcessHeap(), 0, cbKeyObject ? cbKeyObject : 1);
    if (!*ppbKeyObject)
        return FALSE;

    return BCryptGenerateSymmetricKey(*phAlg, phKey, *ppbKeyObject, cbKeyObject,
                                      (PUCHAR)abKey, WON_ENCRYPTION_KEY_SIZE, 0) == 0;
}

static void CloseGcmKey(BCRYPT_ALG_HANDLE hAlg, BCRYPT_KEY_HANDLE hKey, PBYTE pbKeyObject)
{
    if (hKey)
        BCryptDestroyKey(hKey);
    if (pbKeyObject)
        HeapFree(GetProcessHeap(), 0, pbKeyObject);
    if (hAlg)
        BCryptCloseAlgorithmProvider(hAlg, 0);
}

BOOL WonCryptEncryptBuffer(const BYTE *pbPlain, DWORD cbPlain, BYTE **ppbBlob, DWORD *pcbBlob)
{
    if (!ppbBlob || !pcbBlob)
        return FALSE;
    *ppbBlob = NULL;
    *pcbBlob = 0;

    BYTE abKey[WON_ENCRYPTION_KEY_SIZE];
    if (!GetActiveKey(abKey))
        return FALSE; // no key configured: refuse to "encrypt" with nothing

    BOOL bResult = FALSE;
    BCRYPT_ALG_HANDLE hAlg;
    BCRYPT_KEY_HANDLE hKey;
    PBYTE pbKeyObject;
    PBYTE pbBlob = NULL;

    if (!OpenGcmKey(abKey, &hAlg, &hKey, &pbKeyObject))
        goto cleanup;

    DWORD cbBlob = sizeof(WON_CRYPT_HEADER) + cbPlain;
    pbBlob = (PBYTE)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cbBlob ? cbBlob : 1);
    if (!pbBlob)
        goto cleanup;

    PWON_CRYPT_HEADER pHeader = (PWON_CRYPT_HEADER)pbBlob;
    CopyMemory(pHeader->szMagic, WON_CRYPT_MAGIC, sizeof(pHeader->szMagic));
    pHeader->bVersion = WON_CRYPT_VERSION;
    pHeader->cbPlain = cbPlain;

    if (BCryptGenRandom(NULL, pHeader->abNonce, sizeof(pHeader->abNonce),
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
        goto cleanup;

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = pHeader->abNonce;
    authInfo.cbNonce = sizeof(pHeader->abNonce);
    authInfo.pbTag = pHeader->abTag;
    authInfo.cbTag = sizeof(pHeader->abTag);
    authInfo.pbAuthData = (PUCHAR)pbBlob; // magic+version+reserved bound as AAD
    authInfo.cbAuthData = FIELD_OFFSET(WON_CRYPT_HEADER, abNonce);

    ULONG cbCipherOut = 0;
    if (cbPlain > 0) {
        if (BCryptEncrypt(hKey, (PUCHAR)pbPlain, cbPlain, &authInfo, NULL, 0,
                          pbBlob + sizeof(WON_CRYPT_HEADER), cbPlain, &cbCipherOut, 0) != 0 ||
            cbCipherOut != cbPlain)
            goto cleanup;
    } else {
        if (BCryptEncrypt(hKey, NULL, 0, &authInfo, NULL, 0, NULL, 0, &cbCipherOut, 0) != 0)
            goto cleanup;
    }

    *ppbBlob = pbBlob;
    *pcbBlob = cbBlob;
    pbBlob = NULL;
    bResult = TRUE;

cleanup:
    if (pbBlob)
        HeapFree(GetProcessHeap(), 0, pbBlob);
    CloseGcmKey(hAlg, hKey, pbKeyObject);
    SecureZeroMemory(abKey, sizeof(abKey));
    return bResult;
}

BOOL WonCryptDecryptBuffer(const BYTE *pbBlob, BYTE **ppbPlain, DWORD *pcbPlain)
{
    if (!ppbPlain || !pcbPlain || !WonCryptIsEncryptedBlob(pbBlob))
        return FALSE;
    *ppbPlain = NULL;
    *pcbPlain = 0;

    BYTE abKey[WON_ENCRYPTION_KEY_SIZE];
    if (!GetActiveKey(abKey))
        return FALSE; // fail closed: no key -> no decryption, ever

    PWON_CRYPT_HEADER pHeader = (PWON_CRYPT_HEADER)pbBlob;
    DWORD cbPlain = pHeader->cbPlain;

    BOOL bResult = FALSE;
    BCRYPT_ALG_HANDLE hAlg;
    BCRYPT_KEY_HANDLE hKey;
    PBYTE pbKeyObject;
    PBYTE pbPlain = NULL;

    if (!OpenGcmKey(abKey, &hAlg, &hKey, &pbKeyObject))
        goto cleanup;

    pbPlain = (PBYTE)HeapAlloc(GetProcessHeap(), 0, cbPlain ? cbPlain : 1);
    if (!pbPlain)
        goto cleanup;

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = pHeader->abNonce;
    authInfo.cbNonce = sizeof(pHeader->abNonce);
    authInfo.pbTag = pHeader->abTag;
    authInfo.cbTag = sizeof(pHeader->abTag);
    authInfo.pbAuthData = (PUCHAR)pbBlob;
    authInfo.cbAuthData = FIELD_OFFSET(WON_CRYPT_HEADER, abNonce);

    ULONG cbPlainOut = 0;
    // BCryptDecrypt verifies the GCM tag internally; wrong key or tampered
    // ciphertext/AAD makes this fail with STATUS_AUTH_TAG_MISMATCH.
    NTSTATUS status = (cbPlain > 0)
        ? BCryptDecrypt(hKey, (PUCHAR)(pbBlob + sizeof(WON_CRYPT_HEADER)), cbPlain, &authInfo,
                        NULL, 0, pbPlain, cbPlain, &cbPlainOut, 0)
        : BCryptDecrypt(hKey, NULL, 0, &authInfo, NULL, 0, NULL, 0, &cbPlainOut, 0);

    if (status != 0 || cbPlainOut != cbPlain)
        goto cleanup;

    *ppbPlain = pbPlain;
    *pcbPlain = cbPlain;
    pbPlain = NULL;
    bResult = TRUE;

cleanup:
    if (pbPlain)
        HeapFree(GetProcessHeap(), 0, pbPlain);
    CloseGcmKey(hAlg, hKey, pbKeyObject);
    SecureZeroMemory(abKey, sizeof(abKey));
    return bResult;
}
