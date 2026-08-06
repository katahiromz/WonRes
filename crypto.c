// crypto.c --- Optional AES-256 resource encryption for WonRes (XP-compatible)
// Author: katahiromz
// License: MIT
//
// Uses CryptoAPI (advapi32) so it runs on Windows XP and later.
// Scheme: AES-256-CBC + HMAC-SHA256 (encrypt-then-MAC).
// Blob format version 2 (incompatible with the Vista+ GCM version 1).
#include <windows.h>
#include <wincrypt.h>
#include "WonRes.h"
#ifdef WONRES_ENABLE_CRYPTO
#include "WonCryptoP.h"
#endif

#pragma comment(lib, "advapi32.lib")

/* Constants that older SDKs (targeting XP) may not define */
#ifndef PROV_RSA_AES
#define PROV_RSA_AES 24
#endif
#ifndef CALG_AES_256
#define CALG_AES_256 0x00006610
#endif
#ifndef CALG_SHA_256
#define CALG_SHA_256 0x0000800c
#endif
#ifndef CRYPT_IPSEC_HMAC_KEY
#define CRYPT_IPSEC_HMAC_KEY 0x00000100
#endif
#ifndef MS_ENH_RSA_AES_PROV_XP_W
#define MS_ENH_RSA_AES_PROV_XP_W L"Microsoft Enhanced RSA and AES Cryptographic Provider (Prototype)"
#endif
#ifndef MS_ENH_RSA_AES_PROV_W
#define MS_ENH_RSA_AES_PROV_W L"Microsoft Enhanced RSA and AES Cryptographic Provider"
#endif

#define WON_CRYPT_MAGIC        "WONRSRC1" // 8 bytes, no NUL terminator stored
#define WON_CRYPT_VERSION      2          // 2 = CBC+HMAC (XP); 1 was GCM (Vista+)
#define WON_CRYPT_IV_SIZE      16
#define WON_CRYPT_MAC_SIZE     32         // HMAC-SHA256

#pragma pack(push, 1)
typedef struct WON_CRYPT_HEADER {
    CHAR  szMagic[8];
    BYTE  bVersion;
    BYTE  abReserved[3];
    BYTE  abIV[WON_CRYPT_IV_SIZE];
    BYTE  abMac[WON_CRYPT_MAC_SIZE];
    DWORD cbPlain;
} WON_CRYPT_HEADER, *PWON_CRYPT_HEADER;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    BLOBHEADER hdr;
    DWORD      cbKeySize;
    BYTE       rgbKeyData[32];
} AES256KEYBLOB;
#pragma pack(pop)

static CRITICAL_SECTION g_KeyCS;
static BOOL g_bCSInit = FALSE;
static BYTE g_abKey[WON_ENCRYPTION_KEY_SIZE];
static BOOL g_bKeySet = FALSE;

static void EnsureCS(void)
{
    if (!g_bCSInit) {
        InitializeCriticalSection(&g_KeyCS);
        g_bCSInit = TRUE;
    }
}

// ---------------------------------------------------------------------------
// Ownership tracking for decrypted resource buffers
// ---------------------------------------------------------------------------
// WonLoadResource returns two very different kinds of pointer depending on
// whether the resource was encrypted:
//   - encrypted resource  -> a fresh buffer from HeapAlloc (WonCryptDecryptBuffer)
//   - plain resource      -> a pointer straight into the mapped module image
// WonFreeResourceMemory must HeapFree only the former. Since the pointer
// itself carries no marker we can safely inspect (probing bytes before an
// arbitrary foreign pointer could read unmapped memory and crash), we keep
// an explicit registry of the pointers we actually allocated and only free
// ones found in it. This lets callers call WonFreeResourceMemory
// unconditionally on every resource pointer, encrypted or not.
typedef struct WON_ALLOC_NODE {
    struct WON_ALLOC_NODE *pNext;
    LPVOID pMemory;
} WON_ALLOC_NODE, *PWON_ALLOC_NODE;

static CRITICAL_SECTION g_AllocCS;
static BOOL g_bAllocCSInit = FALSE;
static PWON_ALLOC_NODE g_pAllocList = NULL;

static void EnsureAllocCS(void)
{
    if (!g_bAllocCSInit) {
        InitializeCriticalSection(&g_AllocCS);
        g_bAllocCSInit = TRUE;
    }
}

// Records pMemory as ours. Best-effort: if the tiny tracking node can't be
// allocated, pMemory is simply never HeapFree'd later (a leak, not a
// crash) rather than risking a bad free.
static void TrackAllocatedBuffer(LPVOID pMemory)
{
    PWON_ALLOC_NODE pNode = (PWON_ALLOC_NODE)HeapAlloc(GetProcessHeap(), 0, sizeof(WON_ALLOC_NODE));
    if (!pNode)
        return;
    pNode->pMemory = pMemory;

    EnsureAllocCS();
    EnterCriticalSection(&g_AllocCS);
    pNode->pNext = g_pAllocList;
    g_pAllocList = pNode;
    LeaveCriticalSection(&g_AllocCS);
}

// If pMemory is one of ours, unlinks it and returns TRUE (caller may now
// HeapFree it). Otherwise returns FALSE and does nothing.
static BOOL UntrackAllocatedBuffer(LPVOID pMemory)
{
    BOOL bFound = FALSE;

    EnsureAllocCS();
    EnterCriticalSection(&g_AllocCS);
    PWON_ALLOC_NODE *ppCur = &g_pAllocList;
    while (*ppCur) {
        if ((*ppCur)->pMemory == pMemory) {
            PWON_ALLOC_NODE pDead = *ppCur;
            *ppCur = pDead->pNext;
            HeapFree(GetProcessHeap(), 0, pDead);
            bFound = TRUE;
            break;
        }
        ppCur = &(*ppCur)->pNext;
    }
    LeaveCriticalSection(&g_AllocCS);
    return bFound;
}

static BOOL GetActiveKey(BYTE abKeyOut[WON_ENCRYPTION_KEY_SIZE])
{
    BOOL bSet;
    EnsureCS();
    EnterCriticalSection(&g_KeyCS);
    bSet = g_bKeySet;
    if (bSet)
        CopyMemory(abKeyOut, g_abKey, WON_ENCRYPTION_KEY_SIZE);
    LeaveCriticalSection(&g_KeyCS);
    return bSet;
}

BOOL WONAPI WonSetEncryptionKey(const BYTE *pbKey, DWORD cbKey)
{
    if (!pbKey || cbKey != WON_ENCRYPTION_KEY_SIZE)
        return FALSE;

    EnsureCS();
    EnterCriticalSection(&g_KeyCS);
    CopyMemory(g_abKey, pbKey, WON_ENCRYPTION_KEY_SIZE);
    g_bKeySet = TRUE;
    LeaveCriticalSection(&g_KeyCS);
    return TRUE;
}

// ---------------------------------------------------------------------------
// CryptoAPI helpers (XP+)
// ---------------------------------------------------------------------------

static BOOL AcquireAesProvider(HCRYPTPROV *phProv)
{
    if (CryptAcquireContextW(phProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
        return TRUE;
    if (CryptAcquireContextW(phProv, NULL, MS_ENH_RSA_AES_PROV_W, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
        return TRUE;
    return CryptAcquireContextW(phProv, NULL, MS_ENH_RSA_AES_PROV_XP_W, PROV_RSA_AES,
                                CRYPT_VERIFYCONTEXT);
}

static BOOL ImportAes256Key(HCRYPTPROV hProv, const BYTE abKey[32], HCRYPTKEY *phKey)
{
    AES256KEYBLOB blob;
    blob.hdr.bType = PLAINTEXTKEYBLOB;
    blob.hdr.bVersion = CUR_BLOB_VERSION;
    blob.hdr.reserved = 0;
    blob.hdr.aiKeyAlg = CALG_AES_256;
    blob.cbKeySize = 32;
    CopyMemory(blob.rgbKeyData, abKey, 32);
    return CryptImportKey(hProv, (BYTE *)&blob, sizeof(blob), 0, 0, phKey);
}

// HMAC-SHA256 via CryptoAPI. Key is imported as PLAINTEXTKEYBLOB + CRYPT_IPSEC_HMAC_KEY.
static BOOL HmacSha256(HCRYPTPROV hProv, const BYTE *pbKey, DWORD cbKey,
                       const BYTE *pbData1, DWORD cbData1,
                       const BYTE *pbData2, DWORD cbData2,
                       BYTE abOut[32])
{
    HCRYPTKEY hKey = 0;
    HCRYPTHASH hHash = 0;
    BOOL bOk = FALSE;
    DWORD cb = 32;
    BYTE *pbBlob = NULL;
    DWORD cbBlob;

    if (cbKey == 0 || cbKey > 128)
        return FALSE;

    cbBlob = sizeof(BLOBHEADER) + sizeof(DWORD) + cbKey;
    pbBlob = (BYTE *)HeapAlloc(GetProcessHeap(), 0, cbBlob);
    if (!pbBlob)
        return FALSE;

    {
        BLOBHEADER *pHdr = (BLOBHEADER *)pbBlob;
        DWORD *pcb = (DWORD *)(pbBlob + sizeof(BLOBHEADER));
        pHdr->bType = PLAINTEXTKEYBLOB;
        pHdr->bVersion = CUR_BLOB_VERSION;
        pHdr->reserved = 0;
        pHdr->aiKeyAlg = CALG_RC2; // ignored for HMAC
        *pcb = cbKey;
        CopyMemory(pbBlob + sizeof(BLOBHEADER) + sizeof(DWORD), pbKey, cbKey);
    }

    if (!CryptImportKey(hProv, pbBlob, cbBlob, 0, CRYPT_IPSEC_HMAC_KEY, &hKey))
        goto done;

    if (!CryptCreateHash(hProv, CALG_HMAC, hKey, 0, &hHash))
        goto done;

    {
        HMAC_INFO hi;
        ZeroMemory(&hi, sizeof(hi));
        hi.HashAlgid = CALG_SHA_256;
        if (!CryptSetHashParam(hHash, HP_HMAC_INFO, (BYTE *)&hi, 0))
            goto done;
    }

    if (cbData1 && !CryptHashData(hHash, (BYTE *)pbData1, cbData1, 0))
        goto done;
    if (cbData2 && !CryptHashData(hHash, (BYTE *)pbData2, cbData2, 0))
        goto done;

    if (!CryptGetHashParam(hHash, HP_HASHVAL, abOut, &cb, 0) || cb != 32)
        goto done;

    bOk = TRUE;
done:
    if (hHash)
        CryptDestroyHash(hHash);
    if (hKey)
        CryptDestroyKey(hKey);
    if (pbBlob) {
        SecureZeroMemory(pbBlob, cbBlob);
        HeapFree(GetProcessHeap(), 0, pbBlob);
    }
    return bOk;
}

// PBKDF2-HMAC-SHA256 (RFC 2898). Works on XP with the AES provider.
static BOOL Pbkdf2HmacSha256(const BYTE *pbPassword, DWORD cbPassword,
                             const BYTE *pbSalt, DWORD cbSalt,
                             DWORD dwIterations,
                             BYTE *pbDerived, DWORD cbDerived)
{
    HCRYPTPROV hProv = 0;
    BOOL bOk = FALSE;
    BYTE abU[32], abT[32];
    DWORD i, j, k;

    if (!AcquireAesProvider(&hProv))
        return FALSE;

    for (i = 1; (i - 1) * 32 < cbDerived; ++i) {
        BYTE abBlock[4];
        abBlock[0] = (BYTE)((i >> 24) & 0xFF);
        abBlock[1] = (BYTE)((i >> 16) & 0xFF);
        abBlock[2] = (BYTE)((i >> 8) & 0xFF);
        abBlock[3] = (BYTE)(i & 0xFF);

        // U1 = HMAC(password, salt || INT(i))
        // We need a single contiguous buffer for the first round.
        {
            BYTE *pbMsg = (BYTE *)HeapAlloc(GetProcessHeap(), 0, cbSalt + 4);
            if (!pbMsg)
                goto fail;
            CopyMemory(pbMsg, pbSalt, cbSalt);
            CopyMemory(pbMsg + cbSalt, abBlock, 4);
            if (!HmacSha256(hProv, pbPassword, cbPassword, pbMsg, cbSalt + 4, NULL, 0, abU)) {
                HeapFree(GetProcessHeap(), 0, pbMsg);
                goto fail;
            }
            HeapFree(GetProcessHeap(), 0, pbMsg);
        }
        CopyMemory(abT, abU, 32);

        for (j = 1; j < dwIterations; ++j) {
            if (!HmacSha256(hProv, pbPassword, cbPassword, abU, 32, NULL, 0, abU))
                goto fail;
            for (k = 0; k < 32; ++k)
                abT[k] ^= abU[k];
        }

        {
            DWORD off = (i - 1) * 32;
            DWORD n = cbDerived - off;
            if (n > 32)
                n = 32;
            CopyMemory(pbDerived + off, abT, n);
        }
    }

    bOk = TRUE;
fail:
    if (hProv)
        CryptReleaseContext(hProv, 0);
    SecureZeroMemory(abU, sizeof(abU));
    SecureZeroMemory(abT, sizeof(abT));
    return bOk;
}

BOOL WONAPI WonSetEncryptionPasswordW(LPCWSTR pszPassword, const BYTE *pbSalt, DWORD cbSalt,
                                      DWORD dwIterations)
{
    if (!pszPassword || !pszPassword[0] || !pbSalt || cbSalt < 16 ||
        dwIterations < WON_ENCRYPTION_MIN_ITERATIONS)
        return FALSE;

    DWORD cbPassword = (DWORD)(lstrlenW(pszPassword) * sizeof(WCHAR));
    BYTE abDerived[WON_ENCRYPTION_KEY_SIZE];

    if (!Pbkdf2HmacSha256((const BYTE *)pszPassword, cbPassword, pbSalt, cbSalt, dwIterations,
                          abDerived, sizeof(abDerived)))
        return FALSE;

    EnsureCS();
    EnterCriticalSection(&g_KeyCS);
    CopyMemory(g_abKey, abDerived, sizeof(abDerived));
    g_bKeySet = TRUE;
    LeaveCriticalSection(&g_KeyCS);

    SecureZeroMemory(abDerived, sizeof(abDerived));
    return TRUE;
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
    EnsureCS();
    EnterCriticalSection(&g_KeyCS);
    SecureZeroMemory(g_abKey, sizeof(g_abKey));
    g_bKeySet = FALSE;
    LeaveCriticalSection(&g_KeyCS);
}

BOOL WONAPI WonIsEncryptionKeySet(VOID)
{
    BOOL bSet;
    EnsureCS();
    EnterCriticalSection(&g_KeyCS);
    bSet = g_bKeySet;
    LeaveCriticalSection(&g_KeyCS);
    return bSet;
}

VOID WONAPI WonFreeResourceMemory(LPVOID pMemory)
{
    if (!pMemory)
        return;

    // Only pointers WonCryptDecryptBuffer actually HeapAlloc'd are ours to
    // free. A resource that wasn't encrypted is a raw pointer into the
    // mapped module image (see WonLoadResource) and must never reach
    // HeapFree -- doing so used to fail (or corrupt the heap) whenever
    // encrypted and non-encrypted resources were mixed, because the old
    // code decided whether to call HeapFree from the global
    // g_bEnableCrypto flag alone instead of per-pointer.
    if (UntrackAllocatedBuffer(pMemory))
        HeapFree(GetProcessHeap(), 0, pMemory);
}

BOOL WonCryptIsEncryptedBlob(const BYTE *pbData, DWORD cbData)
{
    // cbData must cover the whole fixed-size header before we dereference
    // any of it (magic, version, IV, MAC, cbPlain). Without this check a
    // small legitimate resource (fewer than sizeof(WON_CRYPT_HEADER) bytes)
    // that happens to start with bytes resembling the magic could cause
    // every caller downstream to read past the end of its buffer.
    if (!pbData || cbData < sizeof(WON_CRYPT_HEADER))
        return FALSE;
    return memcmp(pbData, WON_CRYPT_MAGIC, sizeof(WON_CRYPT_MAGIC) - 1) == 0 &&
           pbData[8] == WON_CRYPT_VERSION;
}

// Bounds-checked peek at an encrypted blob's declared plaintext size. This
// does NOT authenticate the blob (no key/MAC involved) -- it only protects
// against a corrupted/malicious header reporting a nonsensical size, so
// callers that just need a size (e.g. WonSizeofResource) never get a value
// larger than the real blob. The actual fail-closed guarantee still comes
// from WonCryptDecryptBuffer's MAC verification.
DWORD WonCryptPeekPlainSize(const BYTE *pbBlob, DWORD cbBlob)
{
    if (!WonCryptIsEncryptedBlob(pbBlob, cbBlob))
        return 0;

    PWON_CRYPT_HEADER pHeader = (PWON_CRYPT_HEADER)pbBlob;
    DWORD cbPlain = pHeader->cbPlain;
    if (cbPlain > cbBlob - sizeof(WON_CRYPT_HEADER))
        return 0;
    return cbPlain;
}

// Derive a dedicated MAC key from the encryption key.
static void DeriveMacKey(const BYTE abEncKey[32], BYTE abMacKey[32])
{
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    DWORD cb = 32;
    static const char prefix[] = "WonRes-MAC-v2";

    ZeroMemory(abMacKey, 32);
    if (!AcquireAesProvider(&hProv))
        return;
    if (CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        CryptHashData(hHash, (BYTE *)prefix, sizeof(prefix) - 1, 0);
        CryptHashData(hHash, (BYTE *)abEncKey, 32, 0);
        CryptGetHashParam(hHash, HP_HASHVAL, abMacKey, &cb, 0);
        CryptDestroyHash(hHash);
    }
    CryptReleaseContext(hProv, 0);
}

BOOL WonCryptEncryptBuffer(const BYTE *pbPlain, DWORD cbPlain, BYTE **ppbBlob, DWORD *pcbBlob)
{
    if (!ppbBlob || !pcbBlob)
        return FALSE;
    *ppbBlob = NULL;
    *pcbBlob = 0;

    BYTE abKey[WON_ENCRYPTION_KEY_SIZE];
    if (!GetActiveKey(abKey))
        return FALSE;

    BOOL bResult = FALSE;
    HCRYPTPROV hProv = 0;
    HCRYPTKEY hKey = 0;
    PBYTE pbBlob = NULL;
    BYTE abMacKey[32];
    DWORD cbPad = 16 - (cbPlain % 16);
    DWORD cbCipher = cbPlain + cbPad;
    DWORD cbBlob = sizeof(WON_CRYPT_HEADER) + cbCipher;

    if (!AcquireAesProvider(&hProv))
        goto cleanup;
    if (!ImportAes256Key(hProv, abKey, &hKey))
        goto cleanup;

    pbBlob = (PBYTE)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cbBlob ? cbBlob : 1);
    if (!pbBlob)
        goto cleanup;

    PWON_CRYPT_HEADER pHeader = (PWON_CRYPT_HEADER)pbBlob;
    CopyMemory(pHeader->szMagic, WON_CRYPT_MAGIC, sizeof(pHeader->szMagic));
    pHeader->bVersion = WON_CRYPT_VERSION;
    pHeader->cbPlain = cbPlain;

    if (!CryptGenRandom(hProv, sizeof(pHeader->abIV), pHeader->abIV))
        goto cleanup;

    if (!CryptSetKeyParam(hKey, KP_IV, pHeader->abIV, 0))
        goto cleanup;

    PBYTE pbCipher = pbBlob + sizeof(WON_CRYPT_HEADER);
    if (cbPlain)
        CopyMemory(pbCipher, pbPlain, cbPlain);
    FillMemory(pbCipher + cbPlain, cbPad, (BYTE)cbPad);

    {
        // Final=FALSE: padding is applied manually above (PKCS7-style, fixed
        // WON_CRYPT_IV_SIZE block), so we must NOT let CryptoAPI append its
        // own extra padding block. With Final=TRUE, CryptEncrypt always adds
        // one more full block of PKCS5 padding even when the input is
        // already block-aligned, which both overruns the exact-sized buffer
        // we allocated and makes cbData != cbCipher -- causing this call to
        // always fail (and thus WonCryptEncryptBuffer to always fail).
        DWORD cbData = cbCipher;
        if (!CryptEncrypt(hKey, 0, FALSE, 0, pbCipher, &cbData, cbCipher) || cbData != cbCipher)
            goto cleanup;
    }

    // Encrypt-then-MAC: MAC covers (magic..IV) || cbPlain || ciphertext
    DeriveMacKey(abKey, abMacKey);
    if (!HmacSha256(hProv, abMacKey, 32,
                    pbBlob, FIELD_OFFSET(WON_CRYPT_HEADER, abMac),
                    (BYTE *)&pHeader->cbPlain, sizeof(DWORD) + cbCipher,
                    pHeader->abMac))
        goto cleanup;

    *ppbBlob = pbBlob;
    *pcbBlob = cbBlob;
    pbBlob = NULL;
    bResult = TRUE;

cleanup:
    if (pbBlob)
        HeapFree(GetProcessHeap(), 0, pbBlob);
    if (hKey)
        CryptDestroyKey(hKey);
    if (hProv)
        CryptReleaseContext(hProv, 0);
    SecureZeroMemory(abKey, sizeof(abKey));
    SecureZeroMemory(abMacKey, sizeof(abMacKey));
    return bResult;
}

BOOL WonCryptDecryptBuffer(const BYTE *pbBlob, DWORD cbBlob, BYTE **ppbPlain, DWORD *pcbPlain)
{
    if (!ppbPlain || !pcbPlain || !WonCryptIsEncryptedBlob(pbBlob, cbBlob))
        return FALSE;
    *ppbPlain = NULL;
    *pcbPlain = 0;

    BYTE abKey[WON_ENCRYPTION_KEY_SIZE];
    if (!GetActiveKey(abKey))
        return FALSE;

    PWON_CRYPT_HEADER pHeader = (PWON_CRYPT_HEADER)pbBlob;
    DWORD cbPlain = pHeader->cbPlain;

    // pHeader->cbPlain is untrusted at this point -- we haven't verified
    // the MAC yet, so it may be attacker-controlled or simply corrupted.
    // Bound it against the real, caller-supplied blob size *before*
    // deriving cbCipher and using it to size the HmacSha256/CryptDecrypt
    // reads below. Previously an inflated cbPlain could make cbCipher
    // larger than the actual blob, causing HmacSha256 (and then
    // CopyMemory/CryptDecrypt) to read past the end of pbBlob.
    if (cbPlain > cbBlob - sizeof(WON_CRYPT_HEADER)) {
        SecureZeroMemory(abKey, sizeof(abKey));
        return FALSE;
    }

    DWORD cbPad = 16 - (cbPlain % 16);
    DWORD cbCipher = cbPlain + cbPad;

    // The header must declare exactly the amount of ciphertext that's
    // actually present in this blob -- no more, no less.
    if (sizeof(WON_CRYPT_HEADER) + cbCipher != cbBlob) {
        SecureZeroMemory(abKey, sizeof(abKey));
        return FALSE;
    }

    BOOL bResult = FALSE;
    HCRYPTPROV hProv = 0;
    HCRYPTKEY hKey = 0;
    PBYTE pbPlain = NULL;
    BYTE abMacKey[32];
    BYTE abComputedMac[32];

    if (!AcquireAesProvider(&hProv))
        goto cleanup;

    // Verify MAC before any decryption (fail-closed)
    DeriveMacKey(abKey, abMacKey);
    if (!HmacSha256(hProv, abMacKey, 32,
                    pbBlob, FIELD_OFFSET(WON_CRYPT_HEADER, abMac),
                    (BYTE *)&pHeader->cbPlain, sizeof(DWORD) + cbCipher,
                    abComputedMac))
        goto cleanup;

    {
        BYTE diff = 0;
        DWORD i;
        for (i = 0; i < 32; ++i)
            diff |= (BYTE)(abComputedMac[i] ^ pHeader->abMac[i]);
        if (diff != 0)
            goto cleanup; // wrong key or tampered
    }

    if (!ImportAes256Key(hProv, abKey, &hKey))
        goto cleanup;
    if (!CryptSetKeyParam(hKey, KP_IV, (BYTE *)pHeader->abIV, 0))
        goto cleanup;

    pbPlain = (PBYTE)HeapAlloc(GetProcessHeap(), 0, cbCipher ? cbCipher : 1);
    if (!pbPlain)
        goto cleanup;
    CopyMemory(pbPlain, pbBlob + sizeof(WON_CRYPT_HEADER), cbCipher);

    {
        // Final=FALSE to match the encrypt side: our padding is manual, so
        // CryptDecrypt must not try to auto-strip PKCS5 padding itself. The
        // manual PKCS7 validation below handles that instead.
        DWORD cbData = cbCipher;
        if (!CryptDecrypt(hKey, 0, FALSE, 0, pbPlain, &cbData))
            goto cleanup;

        // PKCS#7 padding check
        if (cbData < 1 || cbData > cbCipher)
            goto cleanup;
        {
            BYTE pad = pbPlain[cbData - 1];
            if (pad < 1 || pad > 16 || (DWORD)pad > cbData)
                goto cleanup;
            DWORD i;
            for (i = 0; i < pad; ++i) {
                if (pbPlain[cbData - 1 - i] != pad)
                    goto cleanup;
            }
            if (cbData - pad != cbPlain)
                goto cleanup;
        }
    }

    TrackAllocatedBuffer(pbPlain);
    *ppbPlain = pbPlain;
    *pcbPlain = cbPlain;
    pbPlain = NULL;
    bResult = TRUE;

cleanup:
    if (pbPlain)
        HeapFree(GetProcessHeap(), 0, pbPlain);
    if (hKey)
        CryptDestroyKey(hKey);
    if (hProv)
        CryptReleaseContext(hProv, 0);
    SecureZeroMemory(abKey, sizeof(abKey));
    SecureZeroMemory(abMacKey, sizeof(abMacKey));
    SecureZeroMemory(abComputedMac, sizeof(abComputedMac));
    return bResult;
}
