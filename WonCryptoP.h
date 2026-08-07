// WonCryptoP.h --- Private crypto helpers shared by loader.c/updater.c/crypto.c
// Author: katahiromz
// License: MIT
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Returns TRUE if pbData begins with the WonRes encrypted-resource header.
// Cheap header check only; safe to call even if no key has been set, and
// even on plain (non-encrypted) resource data.
BOOL WonCryptIsEncryptedBlob(const BYTE *pbData, DWORD cbData);

// Encrypts pbPlain (cbPlain bytes) into a newly HeapAlloc'd self-contained
// blob: header + random nonce + auth tag + ciphertext. Fails (FALSE) if no
// key has been configured. Caller frees *ppbBlob with HeapFree.
BOOL WonCryptEncryptBuffer(const BYTE *pbPlain, DWORD cbPlain, BYTE **ppbBlob, DWORD *pcbBlob);

// Decrypts + authenticates a blob produced by WonCryptEncryptBuffer. Fails
// (FALSE) if no key is set, the key is wrong, or the tag verification
// fails (tampering) -- never returns partially/incorrectly decrypted data.
// Caller frees *ppbPlain with HeapFree or WonFreeResource().
BOOL WonCryptDecryptBuffer(const BYTE *pbBlob, DWORD cbBlob, BYTE **ppbPlain, DWORD *pcbPlain);

DWORD WonCryptPeekPlainSize(const BYTE *pbBlob, DWORD cbBlob);

// Returns TRUE if pMemory is a buffer that WonCryptDecryptBuffer allocated
// and is still registered for WonFreeResource. Does not untrack or free.
// Safe to call with any pointer (including NULL / module-image pointers).
BOOL WonCryptIsOwnedBuffer(LPVOID pMemory);

#ifdef __cplusplus
} // extern "C"
#endif
