// test-crypto.c --- Tests for WonRes optional resource encryption
// Author: katahiromz
// License: MIT
#include <windows.h>
#include <imagehlp.h>
#include <stdio.h>
#include <assert.h>
#include "WonRes.h"
#include "WonCryptoP.h" // private: WonCryptEncryptBuffer/DecryptBuffer/IsEncryptedBlob

#define RT_TEST_DATA MAKEINTRESOURCEW(999)

static int g_nFailures = 0;

#define CHECK(cond, msg)                                                                        \
    do {                                                                                         \
        if (!(cond)) {                                                                           \
            fprintf(stderr, "FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__);                    \
            ++g_nFailures;                                                                       \
        } else {                                                                                 \
            printf("PASS: %s\n", (msg));                                                         \
        }                                                                                         \
    } while (0)

static const BYTE g_abTestKey[WON_ENCRYPTION_KEY_SIZE] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
};

static const BYTE g_abOtherKey[WON_ENCRYPTION_KEY_SIZE] = {
	0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00,
	0x0F, 0x1E, 0x2D, 0x3C, 0x4B, 0x5A, 0x69, 0x78, 0x87, 0x96, 0xA5, 0xB4, 0xC3, 0xD2, 0xE1, 0xF0,
};

////////////////////////////////////////////////////////////////////////////////////
// 1. Low-level round trip using the internal encrypt/decrypt helpers

static void TestEncryptDecryptRoundTrip(void)
{
	const char szPlain[] = "Hello, WonRes encrypted resource!";
	DWORD cbPlain = (DWORD)sizeof(szPlain);

	CHECK(WonSetEncryptionKey(g_abTestKey, sizeof(g_abTestKey)), "SetEncryptionKey (raw key)");

	PBYTE pbBlob = NULL;
	DWORD cbBlob = 0;
	CHECK(WonCryptEncryptBuffer((const BYTE *)szPlain, cbPlain, &pbBlob, &cbBlob),
		"EncryptBuffer succeeds with key set");
	CHECK(pbBlob != NULL && cbBlob > cbPlain, "Encrypted blob is non-null and larger than plain");
	CHECK(WonCryptIsEncryptedBlob(pbBlob), "Blob is recognized as encrypted");

	PBYTE pbOut = NULL;
	DWORD cbOut = 0;
	CHECK(WonCryptDecryptBuffer(pbBlob, &pbOut, &cbOut), "DecryptBuffer succeeds with correct key");
	CHECK(cbOut == cbPlain, "Decrypted length matches original");
	CHECK(pbOut != NULL && memcmp(pbOut, szPlain, cbPlain) == 0, "Decrypted content matches original");

	if (pbOut)
		WonFreeResourceMemory(pbOut);
	if (pbBlob)
		HeapFree(GetProcessHeap(), 0, pbBlob);
	WonClearEncryptionKey();
}

////////////////////////////////////////////////////////////////////////////////////
// 2. Fail-closed behavior

static void TestFailClosed(void)
{
	const char szPlain[] = "secret payload";
	DWORD cbPlain = (DWORD)sizeof(szPlain);

	// No key at all: encryption must refuse.
	WonClearEncryptionKey();
	CHECK(!WonIsEncryptionKeySet(), "No key is set initially");
	PBYTE pbBlob = NULL;
	DWORD cbBlob = 0;
	CHECK(!WonCryptEncryptBuffer((const BYTE *)szPlain, cbPlain, &pbBlob, &cbBlob),
		"EncryptBuffer refuses when no key is set");

	// Encrypt with key A, then try to decrypt with key B: must fail.
	WonSetEncryptionKey(g_abTestKey, sizeof(g_abTestKey));
	CHECK(WonCryptEncryptBuffer((const BYTE *)szPlain, cbPlain, &pbBlob, &cbBlob),
		"EncryptBuffer succeeds with key A");

	WonSetEncryptionKey(g_abOtherKey, sizeof(g_abOtherKey));
	PBYTE pbOut = NULL;
	DWORD cbOut = 0;
	CHECK(!WonCryptDecryptBuffer(pbBlob, &pbOut, &cbOut),
		"DecryptBuffer fails with wrong key (fail closed)");
	CHECK(pbOut == NULL, "No plaintext leaked when key is wrong");

	// No key at all again: decryption of a valid blob must also fail.
	WonClearEncryptionKey();
	CHECK(!WonCryptDecryptBuffer(pbBlob, &pbOut, &cbOut),
		"DecryptBuffer fails when no key is set (fail closed)");

	// Tampering: flip one byte of ciphertext, decryption (with correct key) must fail.
	WonSetEncryptionKey(g_abTestKey, sizeof(g_abTestKey));
	if (cbBlob > 0) {
		pbBlob[cbBlob - 1] ^= 0xFF; // corrupt last byte of ciphertext
		CHECK(!WonCryptDecryptBuffer(pbBlob, &pbOut, &cbOut),
			"DecryptBuffer fails on tampered ciphertext (MAC mismatch)");
	}

	if (pbBlob)
		HeapFree(GetProcessHeap(), 0, pbBlob);
	WonClearEncryptionKey();
}

////////////////////////////////////////////////////////////////////////////////////
// 3. IV uniqueness: encrypting the same plaintext twice must differ

static void TestNonceUniqueness(void)
{
	const char szPlain[] = "same plaintext, same key, twice";
	DWORD cbPlain = (DWORD)sizeof(szPlain);

	WonSetEncryptionKey(g_abTestKey, sizeof(g_abTestKey));

	PBYTE pbBlob1 = NULL, pbBlob2 = NULL;
	DWORD cbBlob1 = 0, cbBlob2 = 0;
	CHECK(WonCryptEncryptBuffer((const BYTE *)szPlain, cbPlain, &pbBlob1, &cbBlob1),
		"First encryption succeeds");
	CHECK(WonCryptEncryptBuffer((const BYTE *)szPlain, cbPlain, &pbBlob2, &cbBlob2),
		"Second encryption succeeds");
	CHECK(cbBlob1 == cbBlob2, "Both blobs have the same size");
	CHECK(cbBlob1 == 0 || memcmp(pbBlob1, pbBlob2, cbBlob1) != 0,
		"Two encryptions of identical plaintext produce different ciphertext (random IV)");

	if (pbBlob1)
		HeapFree(GetProcessHeap(), 0, pbBlob1);
	if (pbBlob2)
		HeapFree(GetProcessHeap(), 0, pbBlob2);
	WonClearEncryptionKey();
}

////////////////////////////////////////////////////////////////////////////////////
// 4. Password-based key derivation is deterministic given the same salt/iterations

static void TestPasswordDerivation(void)
{
	BYTE abSalt[16];
	for (int i = 0; i < 16; ++i)
		abSalt[i] = (BYTE)(i * 7 + 1);

	const char szPlain[] = "derived-key round trip";
	DWORD cbPlain = (DWORD)sizeof(szPlain);

	CHECK(WonSetEncryptionPasswordW(L"correct horse battery staple", abSalt, sizeof(abSalt),
		WON_ENCRYPTION_MIN_ITERATIONS),
		"SetEncryptionPasswordW succeeds with valid salt/iterations");

	PBYTE pbBlob = NULL;
	DWORD cbBlob = 0;
	CHECK(WonCryptEncryptBuffer((const BYTE *)szPlain, cbPlain, &pbBlob, &cbBlob),
		"Encrypt with password-derived key succeeds");

	// Re-derive from scratch with the same password/salt/iterations: must decrypt fine.
	WonClearEncryptionKey();
	CHECK(WonSetEncryptionPasswordW(L"correct horse battery staple", abSalt, sizeof(abSalt),
		WON_ENCRYPTION_MIN_ITERATIONS),
		"Re-deriving the same password/salt succeeds");

	PBYTE pbOut = NULL;
	DWORD cbOut = 0;
	CHECK(WonCryptDecryptBuffer(pbBlob, &pbOut, &cbOut),
		"Decrypt succeeds after re-deriving identical key from password");
	CHECK(cbOut == cbPlain && memcmp(pbOut, szPlain, cbPlain) == 0,
		"Content matches after password re-derivation round trip");

	// A different password must derive a different key and fail to decrypt.
	WonClearEncryptionKey();
	WonSetEncryptionPasswordW(L"wrong password", abSalt, sizeof(abSalt),
		WON_ENCRYPTION_MIN_ITERATIONS);
	PBYTE pbOut2 = NULL;
	DWORD cbOut2 = 0;
	CHECK(!WonCryptDecryptBuffer(pbBlob, &pbOut2, &cbOut2),
		"Decrypt fails with key derived from a different password");

	// Too few iterations must be rejected outright.
	CHECK(!WonSetEncryptionPasswordW(L"whatever", abSalt, sizeof(abSalt), 1000),
		"SetEncryptionPasswordW rejects iteration counts below the enforced minimum");

	if (pbOut)
		WonFreeResourceMemory(pbOut);
	if (pbBlob)
		HeapFree(GetProcessHeap(), 0, pbBlob);
	WonClearEncryptionKey();
}

////////////////////////////////////////////////////////////////////////////////////
// 5. End-to-end through the public update/load API on a real PE file

static void TestEndToEndPeRoundTrip(void)
{
	WCHAR szSelf[MAX_PATH], szTemp[MAX_PATH], szTempFile[MAX_PATH];
	CHECK(GetModuleFileNameW(NULL, szSelf, _countof(szSelf)) != 0, "GetModuleFileNameW succeeds");
	CHECK(GetTempPathW(_countof(szTemp), szTemp) != 0, "GetTempPathW succeeds");
	CHECK(GetTempFileNameW(szTemp, L"wcr", 0, szTempFile) != 0, "GetTempFileNameW succeeds");
	CHECK(CopyFileW(szSelf, szTempFile, FALSE), "Copy self exe to temp file for patching");

	const char szPlain[] = "end-to-end encrypted resource payload";
	DWORD cbPlain = (DWORD)sizeof(szPlain);

	WonSetEncryptionKey(g_abTestKey, sizeof(g_abTestKey));

	HANDLE hUpdate = WonBeginUpdateResourceW(szTempFile, FALSE);
	CHECK(hUpdate != NULL, "WonBeginUpdateResourceW succeeds");
	if (hUpdate) {
		CHECK(WonUpdateResourceEncryptedW(hUpdate, RT_RCDATA, RT_TEST_DATA,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
			(LPVOID)szPlain, cbPlain),
			"WonUpdateResourceEncryptedW succeeds");
		CHECK(WonEndUpdateResourceW(hUpdate, FALSE), "WonEndUpdateResourceW succeeds");
	}

	// Reload with the correct key: should transparently decrypt.
	HMODULE hMod = LoadLibraryExW(szTempFile, NULL, LOAD_LIBRARY_AS_DATAFILE);
	CHECK(hMod != NULL, "LoadLibraryExW(LOAD_LIBRARY_AS_DATAFILE) on patched file succeeds");
	if (hMod) {
		HRSRC hRsrc = WonFindResourceW(hMod, RT_TEST_DATA, RT_RCDATA);
		CHECK(hRsrc != NULL, "WonFindResourceW finds the encrypted resource");
		if (hRsrc) {
			HGLOBAL hRes = WonLoadResource(hMod, hRsrc);
			LPVOID pData = WonLockResource(hRes);
			CHECK(pData != NULL, "WonLockResource decrypts transparently with correct key");
			if (pData) {
				CHECK(memcmp(pData, szPlain, cbPlain) == 0,
					"Decrypted resource content matches original plaintext");
				WonFreeResourceMemory(pData);
			}
		}

		// Reload with the wrong key: must fail closed, not return ciphertext.
		WonSetEncryptionKey(g_abOtherKey, sizeof(g_abOtherKey));
		if (hRsrc) {
			HGLOBAL hRes = WonLoadResource(hMod, hRsrc);
			LPVOID pData = WonLockResource(hRes);
			CHECK(pData == NULL, "WonLockResource returns NULL with the wrong key");
		}

		// No key at all: must also fail closed.
		WonClearEncryptionKey();
		if (hRsrc) {
			HGLOBAL hRes = WonLoadResource(hMod, hRsrc);
			LPVOID pData = WonLockResource(hRes);
			CHECK(pData == NULL, "WonLockResource returns NULL when no key is configured");
		}

		FreeLibrary(hMod);
	}

	DeleteFileW(szTempFile);
	WonClearEncryptionKey();
}

int main(void)
{
	TestEncryptDecryptRoundTrip();
	TestFailClosed();
	TestNonceUniqueness();
	TestPasswordDerivation();
	TestEndToEndPeRoundTrip();

	if (g_nFailures) {
		fprintf(stderr, "\n%d check(s) FAILED\n", g_nFailures);
		return 1;
	}
	printf("\nAll checks passed.\n");
	return 0;
}
