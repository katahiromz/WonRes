// crypto.c --- Optional AES-256 resource encryption for WonRes (XP-compatible)
// Author: katahiromz
// License: MIT
//
// Uses CryptoAPI (advapi32) so it runs on Windows XP and later.
// Scheme: LZNT1 compression (opportunistic) + AES-256-CBC + HMAC-SHA256
// (encrypt-then-MAC). Compression uses ntdll's RtlCompressBuffer /
// RtlDecompressBuffer, which -- like CryptoAPI -- have been present since
// NT4/XP, so the XP-compatibility story is unchanged.
// Blob format version 2 (incompatible with the Vista+ GCM version 1).
#include <windows.h>
#include <wincrypt.h>
#include "WonRes.h"

#ifdef WONRES_ENABLE_CRYPTO
#include "WonCryptoP.h"

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
#ifndef COMPRESSION_FORMAT_LZNT1
#define COMPRESSION_FORMAT_LZNT1 (0x0002)
#endif
#ifndef COMPRESSION_ENGINE_MAXIMUM
#define COMPRESSION_ENGINE_MAXIMUM (0x0100)
#endif

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
    DWORD cbPlain;      // size of the data actually AES-encrypted: the
                         // LZNT1-compressed payload size when szMagic is
                         // WON_CRYPT_MAGIC_COMPRESSED, otherwise equal to
                         // cbOriginal below.
    DWORD cbOriginal;   // true original (pre-compression) resource size.
                         // Only meaningful/used when szMagic is
                         // WON_CRYPT_MAGIC_COMPRESSED; mirrors cbPlain
                         // otherwise. Both DWORDs are covered by the MAC,
                         // and cbPlain must come right before cbOriginal,
                         // which must come right before the ciphertext --
                         // see the HmacSha256 calls below, which read them
                         // as one contiguous region.
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
static LONG g_lKeyCSState = 0; // 0=uninit, 1=another thread is initializing, 2=ready
static BYTE g_abKey[WON_ENCRYPTION_KEY_SIZE];
static BOOL g_bKeySet = FALSE;

// XP-safe (no InitOnceExecuteOnce, which is Vista+) double-checked init.
// The naive "if (!g_bCSInit) { InitializeCriticalSection(...); g_bCSInit =
// TRUE; }" this replaced had a real race: two threads calling this for the
// very first time concurrently could both see g_bCSInit == FALSE and both
// call InitializeCriticalSection on the same object, which is undefined
// behavior. This is a plausible hot path (WonSetEncryptionKey,
// WonLoadResource/WonFreeResource via EnsureAllocCS below), not just a
// theoretical concern. InterlockedCompareExchange(&x, v, v) is used purely
// as an atomic *read* of x (it always returns the pre-operation value,
// and only ever writes v back over an existing v, i.e. a no-op) since XP
// predates a dedicated atomic-load primitive.
static void EnsureCS(void)
{
    if (InterlockedCompareExchange(&g_lKeyCSState, 1, 0) == 0) {
        // We're the thread that won the 0->1 transition: do the real init.
        InitializeCriticalSection(&g_KeyCS);
        InterlockedExchange(&g_lKeyCSState, 2);
    } else {
        // Someone else is (or already did) initialize it -- wait for them.
        while (InterlockedCompareExchange(&g_lKeyCSState, 2, 2) != 2)
            Sleep(0);
    }
}

// ---------------------------------------------------------------------------
// Ownership tracking for decrypted resource buffers
// ---------------------------------------------------------------------------
// WonLoadResource returns two very different kinds of pointer depending on
// whether the resource was encrypted:
//   - encrypted resource  -> a fresh buffer from HeapAlloc (WonCryptDecryptBuffer)
//   - plain resource      -> a pointer straight into the mapped module image
// WonFreeResource must HeapFree only the former. Since the pointer
// itself carries no marker we can safely inspect (probing bytes before an
// arbitrary foreign pointer could read unmapped memory and crash), we keep
// an explicit registry of the pointers we actually allocated and only free
// ones found in it. This lets callers call WonFreeResource
// unconditionally on every resource pointer, encrypted or not.
typedef struct WON_ALLOC_NODE {
    struct WON_ALLOC_NODE *pNext;
    LPVOID pMemory;
} WON_ALLOC_NODE, *PWON_ALLOC_NODE;

static CRITICAL_SECTION g_AllocCS;
static LONG g_lAllocCSState = 0; // 0=uninit, 1=another thread is initializing, 2=ready
static PWON_ALLOC_NODE g_pAllocList = NULL;

// Same XP-safe double-checked init as EnsureCS() above -- see its comment.
static void EnsureAllocCS(void)
{
    if (InterlockedCompareExchange(&g_lAllocCSState, 1, 0) == 0) {
        InitializeCriticalSection(&g_AllocCS);
        InterlockedExchange(&g_lAllocCSState, 2);
    } else {
        while (InterlockedCompareExchange(&g_lAllocCSState, 2, 2) != 2)
            Sleep(0);
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

// Non-destructive check: is pMemory still on the ownership list?
BOOL WonCryptIsOwnedBuffer(LPVOID pMemory)
{
    BOOL bFound = FALSE;

    if (!pMemory)
        return FALSE;

    EnsureAllocCS();
    EnterCriticalSection(&g_AllocCS);
    for (PWON_ALLOC_NODE p = g_pAllocList; p; p = p->pNext) {
        if (p->pMemory == pMemory) {
            bFound = TRUE;
            break;
        }
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

BOOL WonCryptIsEncryptedBlob(const BYTE *pbData, DWORD cbData)
{
    // cbData must cover the whole fixed-size header before we dereference
    // any of it (magic, version, IV, MAC, cbPlain). Without this check a
    // small legitimate resource (fewer than sizeof(WON_CRYPT_HEADER) bytes)
    // that happens to start with bytes resembling the magic could cause
    // every caller downstream to read past the end of its buffer.
    if (!pbData || cbData < sizeof(WON_CRYPT_HEADER))
        return FALSE;
    if (pbData[8] != WON_CRYPT_VERSION)
        return FALSE;
    return memcmp(pbData, WON_CRYPT_MAGIC, sizeof(WON_CRYPT_MAGIC) - 1) == 0 ||
           memcmp(pbData, WON_CRYPT_MAGIC_COMPRESSED, sizeof(WON_CRYPT_MAGIC_COMPRESSED) - 1) == 0;
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

    if (memcmp(pHeader->szMagic, WON_CRYPT_MAGIC_COMPRESSED, sizeof(pHeader->szMagic)) == 0) {
        // Compressed: the true plaintext is the *decompressed* size, which
        // (unlike cbPlain above) can legitimately be larger than the blob
        // itself, so it can't be bounds-checked against cbBlob the same
        // way. This is still just an unauthenticated peek at an untrusted
        // header field -- callers that need a guarantee must go through
        // WonCryptDecryptBuffer, which verifies the MAC before trusting it.
        return pHeader->cbOriginal;
    }
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

// ---------------------------------------------------------------------------
// Opportunistic LZNT1 compression (ntdll, present since NT4/XP)
// ---------------------------------------------------------------------------
// RtlCompressBuffer/RtlDecompressBuffer aren't declared in <windows.h>, so
// they're resolved dynamically -- same pattern as AcquireAesProvider's
// provider-name fallback chain above. ntdll.dll is always already loaded
// in-process, so GetModuleHandle (not LoadLibrary) is enough here.
typedef LONG (WINAPI *PFN_RtlGetCompressionWorkSpaceSize)(USHORT CompressionFormatAndEngine,
                                                           PULONG pcbWorkSpace,
                                                           PULONG pcbFragmentWorkSpace);
typedef LONG (WINAPI *PFN_RtlCompressBuffer)(USHORT CompressionFormatAndEngine,
                                             PUCHAR pbUncompressed, ULONG cbUncompressed,
                                             PUCHAR pbCompressed, ULONG cbCompressed,
                                             ULONG cbChunk, PULONG pcbFinal, PVOID pWorkSpace);
typedef LONG (WINAPI *PFN_RtlDecompressBuffer)(USHORT CompressionFormat,
                                               PUCHAR pbUncompressed, ULONG cbUncompressed,
                                               PUCHAR pbCompressed, ULONG cbCompressed,
                                               PULONG pcbFinal);

// Compresses pbSrc with LZNT1. Only succeeds (returns TRUE) if the data is
// genuinely compressible, i.e. the compressed result is strictly smaller
// than cbSrc -- that's the "if it's compressible, compress it" contract:
// callers fall back to storing the data uncompressed on FALSE, whether that
// FALSE is because ntdll's compressor isn't available, the data has no
// redundancy to exploit, or cbSrc is 0. Caller HeapFree's *ppbDst on TRUE.
static BOOL CompressBufferLznt1(const BYTE *pbSrc, DWORD cbSrc, BYTE **ppbDst, DWORD *pcbDst)
{
    *ppbDst = NULL;
    *pcbDst = 0;
    if (!pbSrc || cbSrc == 0)
        return FALSE;

    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (!hNtdll)
        return FALSE;

    PFN_RtlGetCompressionWorkSpaceSize pfnWorkSpaceSize =
        (PFN_RtlGetCompressionWorkSpaceSize)GetProcAddress(hNtdll, "RtlGetCompressionWorkSpaceSize");
    PFN_RtlCompressBuffer pfnCompress =
        (PFN_RtlCompressBuffer)GetProcAddress(hNtdll, "RtlCompressBuffer");
    if (!pfnWorkSpaceSize || !pfnCompress)
        return FALSE;

    ULONG cbWorkSpace = 0, cbFragWorkSpace = 0;
    USHORT wFormat = (USHORT)(COMPRESSION_FORMAT_LZNT1 | COMPRESSION_ENGINE_MAXIMUM);
    if (pfnWorkSpaceSize(wFormat, &cbWorkSpace, &cbFragWorkSpace) != 0)
        return FALSE;

    PVOID pWorkSpace = HeapAlloc(GetProcessHeap(), 0, cbWorkSpace ? cbWorkSpace : 1);
    if (!pWorkSpace)
        return FALSE;

    // We only ever want the compressed form when it's smaller than cbSrc
    // (see the size check below), so a same-size scratch buffer is always
    // big enough for any result we'd actually keep; if the real compressed
    // size would exceed that, RtlCompressBuffer just fails with
    // STATUS_BUFFER_TOO_SMALL, which we treat the same as "not
    // compressible" and fall back to storing the data uncompressed.
    PBYTE pbDst = (PBYTE)HeapAlloc(GetProcessHeap(), 0, cbSrc);
    if (!pbDst) {
        HeapFree(GetProcessHeap(), 0, pWorkSpace);
        return FALSE;
    }

    ULONG cbFinal = 0;
    LONG status = pfnCompress(wFormat, (PUCHAR)pbSrc, cbSrc, pbDst, cbSrc,
                              4096, &cbFinal, pWorkSpace);
    HeapFree(GetProcessHeap(), 0, pWorkSpace);

    if (status != 0 || cbFinal == 0 || cbFinal >= cbSrc) {
        HeapFree(GetProcessHeap(), 0, pbDst);
        return FALSE;
    }

    *ppbDst = pbDst;
    *pcbDst = (DWORD)cbFinal;
    return TRUE;
}

// Decompresses an LZNT1 blob produced by CompressBufferLznt1 back to its
// known original size cbOriginal (already MAC-authenticated by the caller
// before this runs). Caller HeapFree's *ppbDst on TRUE.
static BOOL DecompressBufferLznt1(const BYTE *pbSrc, DWORD cbSrc, DWORD cbOriginal, BYTE **ppbDst)
{
    *ppbDst = NULL;
    if (cbOriginal == 0)
        return FALSE;

    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (!hNtdll)
        return FALSE;

    PFN_RtlDecompressBuffer pfnDecompress =
        (PFN_RtlDecompressBuffer)GetProcAddress(hNtdll, "RtlDecompressBuffer");
    if (!pfnDecompress)
        return FALSE;

    PBYTE pbDst = (PBYTE)HeapAlloc(GetProcessHeap(), 0, cbOriginal);
    if (!pbDst)
        return FALSE;

    ULONG cbFinal = 0;
    LONG status = pfnDecompress((USHORT)COMPRESSION_FORMAT_LZNT1, pbDst, cbOriginal,
                                (PUCHAR)pbSrc, cbSrc, &cbFinal);
    if (status != 0 || cbFinal != cbOriginal) {
        HeapFree(GetProcessHeap(), 0, pbDst);
        return FALSE;
    }

    *ppbDst = pbDst;
    return TRUE;
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

    // Opportunistic compression: only kept if it actually shrinks the data
    // (see CompressBufferLznt1). Whatever we end up encrypting -- the
    // compressed bytes or the original plaintext -- is "the payload".
    PBYTE pbCompressed = NULL;
    DWORD cbCompressed = 0;
    BOOL bCompressed = CompressBufferLznt1(pbPlain, cbPlain, &pbCompressed, &cbCompressed);

    const BYTE *pbPayload = bCompressed ? pbCompressed : pbPlain;
    DWORD cbPayload = bCompressed ? cbCompressed : cbPlain;

    DWORD cbPad = 16 - (cbPayload % 16);
    DWORD cbCipher = cbPayload + cbPad;
    DWORD cbBlob = sizeof(WON_CRYPT_HEADER) + cbCipher;

    if (!AcquireAesProvider(&hProv))
        goto cleanup;
    if (!ImportAes256Key(hProv, abKey, &hKey))
        goto cleanup;

    pbBlob = (PBYTE)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cbBlob ? cbBlob : 1);
    if (!pbBlob)
        goto cleanup;

    PWON_CRYPT_HEADER pHeader = (PWON_CRYPT_HEADER)pbBlob;
    CopyMemory(pHeader->szMagic, bCompressed ? WON_CRYPT_MAGIC_COMPRESSED : WON_CRYPT_MAGIC,
              sizeof(pHeader->szMagic));
    pHeader->bVersion = WON_CRYPT_VERSION;
    pHeader->cbPlain = cbPayload;
    pHeader->cbOriginal = cbPlain;

    if (!CryptGenRandom(hProv, sizeof(pHeader->abIV), pHeader->abIV))
        goto cleanup;

    if (!CryptSetKeyParam(hKey, KP_IV, pHeader->abIV, 0))
        goto cleanup;

    PBYTE pbCipher = pbBlob + sizeof(WON_CRYPT_HEADER);
    if (cbPayload)
        CopyMemory(pbCipher, pbPayload, cbPayload);
    FillMemory(pbCipher + cbPayload, cbPad, (BYTE)cbPad);

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

    // Encrypt-then-MAC: MAC covers (magic..IV) || cbPlain || cbOriginal || ciphertext.
    // cbPlain and cbOriginal are read as one contiguous region together with
    // the ciphertext that immediately follows the header in memory.
    DeriveMacKey(abKey, abMacKey);
    {
        DWORD cbTail = sizeof(WON_CRYPT_HEADER) - FIELD_OFFSET(WON_CRYPT_HEADER, cbPlain);
        if (!HmacSha256(hProv, abMacKey, 32,
                        pbBlob, FIELD_OFFSET(WON_CRYPT_HEADER, abMac),
                        (BYTE *)&pHeader->cbPlain, cbTail + cbCipher,
                        pHeader->abMac))
            goto cleanup;
    }

    *ppbBlob = pbBlob;
    *pcbBlob = cbBlob;
    pbBlob = NULL;
    bResult = TRUE;

cleanup:
    if (pbCompressed)
        HeapFree(GetProcessHeap(), 0, pbCompressed);
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
    BOOL bCompressed = (memcmp(pHeader->szMagic, WON_CRYPT_MAGIC_COMPRESSED,
                               sizeof(pHeader->szMagic)) == 0);
    DWORD cbPayload = pHeader->cbPlain; // size of the AES-encrypted payload:
                                        // the LZNT1 payload if bCompressed,
                                        // else the final plaintext itself.

    // pHeader->cbPlain is untrusted at this point -- we haven't verified
    // the MAC yet, so it may be attacker-controlled or simply corrupted.
    // Bound it against the real, caller-supplied blob size *before*
    // deriving cbCipher and using it to size the HmacSha256/CryptDecrypt
    // reads below. Previously an inflated cbPlain could make cbCipher
    // larger than the actual blob, causing HmacSha256 (and then
    // CopyMemory/CryptDecrypt) to read past the end of pbBlob.
    if (cbPayload > cbBlob - sizeof(WON_CRYPT_HEADER)) {
        SecureZeroMemory(abKey, sizeof(abKey));
        return FALSE;
    }

    DWORD cbPad = 16 - (cbPayload % 16);
    DWORD cbCipher = cbPayload + cbPad;

    // The header must declare exactly the amount of ciphertext that's
    // actually present in this blob -- no more, no less.
    if (sizeof(WON_CRYPT_HEADER) + cbCipher != cbBlob) {
        SecureZeroMemory(abKey, sizeof(abKey));
        return FALSE;
    }

    BOOL bResult = FALSE;
    HCRYPTPROV hProv = 0;
    HCRYPTKEY hKey = 0;
    PBYTE pbPayload = NULL; // decrypted payload -- still LZNT1-compressed if bCompressed
    PBYTE pbFinal = NULL;   // final buffer handed back to the caller
    BYTE abMacKey[32];
    BYTE abComputedMac[32];

    if (!AcquireAesProvider(&hProv))
        goto cleanup;

    // Verify MAC before any decryption (fail-closed). Covers cbPlain and
    // cbOriginal together with the ciphertext -- see the encrypt side.
    DeriveMacKey(abKey, abMacKey);
    {
        DWORD cbTail = sizeof(WON_CRYPT_HEADER) - FIELD_OFFSET(WON_CRYPT_HEADER, cbPlain);
        if (!HmacSha256(hProv, abMacKey, 32,
                        pbBlob, FIELD_OFFSET(WON_CRYPT_HEADER, abMac),
                        (BYTE *)&pHeader->cbPlain, cbTail + cbCipher,
                        abComputedMac))
            goto cleanup;
    }

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

    pbPayload = (PBYTE)HeapAlloc(GetProcessHeap(), 0, cbCipher ? cbCipher : 1);
    if (!pbPayload)
        goto cleanup;
    CopyMemory(pbPayload, pbBlob + sizeof(WON_CRYPT_HEADER), cbCipher);

    {
        // Final=FALSE to match the encrypt side: our padding is manual, so
        // CryptDecrypt must not try to auto-strip PKCS5 padding itself. The
        // manual PKCS7 validation below handles that instead.
        DWORD cbData = cbCipher;
        if (!CryptDecrypt(hKey, 0, FALSE, 0, pbPayload, &cbData))
            goto cleanup;

        // PKCS#7 padding check
        if (cbData < 1 || cbData > cbCipher)
            goto cleanup;
        {
            BYTE pad = pbPayload[cbData - 1];
            if (pad < 1 || pad > 16 || (DWORD)pad > cbData)
                goto cleanup;
            DWORD i;
            for (i = 0; i < pad; ++i) {
                if (pbPayload[cbData - 1 - i] != pad)
                    goto cleanup;
            }
            if (cbData - pad != cbPayload)
                goto cleanup;
        }
    }

    if (bCompressed) {
        // MAC is verified at this point, so pHeader->cbOriginal is
        // trustworthy; a tampered value would already have failed above.
        // Still sanity-check it: a genuine compressed blob always expands
        // on decompression (CompressBufferLznt1 only kept results smaller
        // than the original), so cbOriginal <= cbPayload is impossible for
        // legitimate data.
        DWORD cbOriginal = pHeader->cbOriginal;
        if (cbOriginal == 0 || cbOriginal <= cbPayload)
            goto cleanup;
        if (!DecompressBufferLznt1(pbPayload, cbPayload, cbOriginal, &pbFinal))
            goto cleanup;

        TrackAllocatedBuffer(pbFinal);
        *ppbPlain = pbFinal;
        *pcbPlain = cbOriginal;
        pbFinal = NULL;
    } else {
        TrackAllocatedBuffer(pbPayload);
        *ppbPlain = pbPayload;
        *pcbPlain = cbPayload;
        pbPayload = NULL;
    }
    bResult = TRUE;

cleanup:
    if (pbPayload)
        HeapFree(GetProcessHeap(), 0, pbPayload);
    if (pbFinal)
        HeapFree(GetProcessHeap(), 0, pbFinal);
    if (hKey)
        CryptDestroyKey(hKey);
    if (hProv)
        CryptReleaseContext(hProv, 0);
    SecureZeroMemory(abKey, sizeof(abKey));
    SecureZeroMemory(abMacKey, sizeof(abMacKey));
    SecureZeroMemory(abComputedMac, sizeof(abComputedMac));
    return bResult;
}
#endif // def WONRES_ENABLE_CRYPTO

BOOL WONAPI WonFreeResource(HGLOBAL hGlobal)
{
#ifdef WONRES_ENABLE_CRYPTO
    if (!hGlobal)
        return TRUE;

    // Only pointers WonCryptDecryptBuffer actually HeapAlloc'd are ours to
    // free. A resource that wasn't encrypted is a raw pointer into the
    // mapped module image (see WonLoadResource) and must never reach
    // HeapFree -- doing so used to fail (or corrupt the heap) whenever
    // encrypted and non-encrypted resources were mixed, because the old
    // code decided whether to call HeapFree from the global
    // g_bEnableCrypto flag alone instead of per-pointer.
    if (UntrackAllocatedBuffer(hGlobal))
        HeapFree(GetProcessHeap(), 0, hGlobal);
#endif
    return TRUE;
}
