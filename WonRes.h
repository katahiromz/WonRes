// WonRes.h --- Win32 resource loader
// Author: katahiromz
// License: MIT

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WONAPI
#define WONAPI WINAPI
#endif

#ifndef MAX_RES_ID_LEN
#define MAX_RES_ID_LEN 256
#endif

BOOL WONAPI WonEnumResourceTypesA(HMODULE hModule, ENUMRESTYPEPROCA lpEnumFunc, LONG_PTR lParam);
BOOL WONAPI WonEnumResourceTypesW(HMODULE hModule, ENUMRESTYPEPROCW lpEnumFunc, LONG_PTR lParam);

BOOL WONAPI WonEnumResourceNamesA(HMODULE hModule, LPCSTR lpType, ENUMRESNAMEPROCA lpEnumFunc,
                                  LONG_PTR lParam);
BOOL WONAPI WonEnumResourceNamesW(HMODULE hModule, LPCWSTR lpType, ENUMRESNAMEPROCW lpEnumFunc,
                                  LONG_PTR lParam);

BOOL WONAPI WonEnumResourceLanguagesA(HMODULE hModule, LPCSTR lpType, LPCSTR lpName,
                                      ENUMRESLANGPROCA lpEnumFunc, LONG_PTR lParam);

BOOL WONAPI WonEnumResourceLanguagesW(HMODULE hModule, LPCWSTR lpType, LPCWSTR lpName,
                                      ENUMRESLANGPROCW lpEnumFunc, LONG_PTR lParam);

HRSRC WONAPI WonFindResourceA(HMODULE hModule, LPCSTR lpName, LPCSTR lpType);
HRSRC WONAPI WonFindResourceW(HMODULE hModule, LPCWSTR lpName, LPCWSTR lpType);

HRSRC WONAPI WonFindResourceExA(HMODULE hModule, LPCSTR lpType, LPCSTR lpName, WORD wLanguage);
HRSRC WONAPI WonFindResourceExW(HMODULE hModule, LPCWSTR lpType, LPCWSTR lpName, WORD wLanguage);

DWORD WONAPI WonSizeofResource(HMODULE hModule, HRSRC hrsrc);
HGLOBAL WONAPI WonLoadResource(HMODULE hModule, HRSRC hrsrc);
LPVOID WONAPI WonLockResource(HGLOBAL hResData);

HANDLE WONAPI WonBeginUpdateResourceA(LPCSTR pFileName, BOOL bDeleteExistingResources);
HANDLE WONAPI WonBeginUpdateResourceW(LPCWSTR pFileName, BOOL bDeleteExistingResources);
BOOL WONAPI WonEndUpdateResourceA(HANDLE hUpdate, BOOL fDiscard);
BOOL WONAPI WonEndUpdateResourceW(HANDLE hUpdate, BOOL fDiscard);

BOOL WONAPI WonUpdateResourceA(HANDLE hUpdate, LPCSTR lpType, LPCSTR lpName, WORD wLanguage,
                               LPVOID lpData, DWORD cbData);
BOOL WONAPI WonUpdateResourceW(HANDLE hUpdate, LPCWSTR lpType, LPCWSTR lpName, WORD wLanguage,
                               LPVOID lpData, DWORD cbData);

INT WONAPI WonLoadStringA(HINSTANCE hInstance, UINT uID, LPSTR lpBuffer, INT nBufferMax);
INT WONAPI WonLoadStringW(HINSTANCE hInstance, UINT uID, LPWSTR lpBuffer, INT nBufferMax);

HBITMAP WONAPI WonLoadBitmapA(HINSTANCE hInstance, LPCSTR lpBitmapName);
HBITMAP WONAPI WonLoadBitmapW(HINSTANCE hInstance, LPCWSTR lpBitmapName);

#ifdef WONRES_ENABLE_CRYPTO
////////////////////////////////////////////////////////////////////////////////////
// Optional resource encryption (AES-256-GCM via CNG)
//
// Resources written with WonUpdateResourceEncrypted{A,W} are stored as
// authenticated ciphertext. WonLockResource decrypts them transparently on
// read -- but ONLY if the correct key/password has been configured first.
// If no key is set, or the key is wrong, or the stored data has been
// tampered with, decryption fails closed: WonLockResource returns NULL
// instead of ciphertext or garbage. Each resource is encrypted with a
// fresh random nonce, so encrypting identical data twice yields different
// output. Callers are responsible for provisioning/storing the key or
// password securely; this library only protects the resource *data*.

#define WON_ENCRYPTION_KEY_SIZE       32      // AES-256
#define WON_ENCRYPTION_MIN_ITERATIONS 100000  // minimum PBKDF2 iterations enforced

BOOL WONAPI WonSetEncryptionKey(const BYTE *pbKey, DWORD cbKey);
BOOL WONAPI WonSetEncryptionPasswordA(LPCSTR pszPassword, const BYTE *pbSalt, DWORD cbSalt,
                                      DWORD dwIterations);
BOOL WONAPI WonSetEncryptionPasswordW(LPCWSTR pszPassword, const BYTE *pbSalt, DWORD cbSalt,
                                      DWORD dwIterations);
VOID WONAPI WonClearEncryptionKey(VOID);
BOOL WONAPI WonIsEncryptionKeySet(VOID);

BOOL WONAPI WonUpdateResourceEncryptedA(HANDLE hUpdate, LPCSTR lpType, LPCSTR lpName,
                                        WORD wLanguage, LPVOID lpData, DWORD cbData);
BOOL WONAPI WonUpdateResourceEncryptedW(HANDLE hUpdate, LPCWSTR lpType, LPCWSTR lpName,
                                        WORD wLanguage, LPVOID lpData, DWORD cbData);

#ifdef UNICODE
#define WonSetEncryptionPassword WonSetEncryptionPasswordW
#define WonUpdateResourceEncrypted WonUpdateResourceEncryptedW
#else
#define WonSetEncryptionPassword WonSetEncryptionPasswordA
#define WonUpdateResourceEncrypted WonUpdateResourceEncryptedA
#endif
#endif // def WONRES_ENABLE_CRYPTO

#ifdef UNICODE
#define WonEnumResourceTypes WonEnumResourceTypesW
#define WonEnumResourceNames WonEnumResourceNamesW
#define WonEnumResourceLanguages WonEnumResourceLanguagesW
#define WonFindResource WonFindResourceW
#define WonFindResourceEx WonFindResourceExW
#define WonBeginUpdateResource WonBeginUpdateResourceW
#define WonUpdateResource WonUpdateResourceW
#define WonEndUpdateResource WonEndUpdateResourceW
#define WonLoadString WonLoadStringW
#define WonLoadBitmap WonLoadBitmapW
#else
#define WonEnumResourceTypes WonEnumResourceTypesA
#define WonEnumResourceNames WonEnumResourceNamesA
#define WonEnumResourceLanguages WonEnumResourceLanguagesA
#define WonFindResource WonFindResourceA
#define WonFindResourceEx WonFindResourceExA
#define WonBeginUpdateResource WonBeginUpdateResourceA
#define WonUpdateResource WonUpdateResourceA
#define WonEndUpdateResource WonEndUpdateResourceA
#define WonLoadString WonLoadStringA
#define WonLoadBitmap WonLoadBitmapA
#endif

// Frees a buffer returned by WonLockResource() for an *encrypted* resource
// (decryption allocates a fresh heap buffer instead of aliasing the image).
// Safe/no-op to call on NULL. Not calling it just leaks until process exit,
// same as the OS does for classic LoadResource() handles.
VOID WONAPI WonFreeResourceMemory(LPVOID pMemory);

#ifdef __cplusplus
} // extern "C"
#endif
