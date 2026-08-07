// WonRes.h --- Win32 resource manipulator (loader/updater/encrypter)
// Author: katahiromz
// License: MIT

#pragma once

#ifndef __WONRES__
#define __WONRES__

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

////////////////////////////////////////////////////////////////////////////////////
// Dialog boxes (RT_DIALOG), loaded/decrypted the same way as everything else above

INT_PTR WONAPI WonDialogBoxParamA(HINSTANCE hInstance, LPCSTR lpTemplateName, HWND hWndParent,
                                  DLGPROC lpDialogFunc, LPARAM dwInitParam);
INT_PTR WONAPI WonDialogBoxParamW(HINSTANCE hInstance, LPCWSTR lpTemplateName, HWND hWndParent,
                                  DLGPROC lpDialogFunc, LPARAM dwInitParam);

HWND WONAPI WonCreateDialogParamA(HINSTANCE hInstance, LPCSTR lpTemplateName, HWND hWndParent,
                                  DLGPROC lpDialogFunc, LPARAM dwInitParam);
HWND WONAPI WonCreateDialogParamW(HINSTANCE hInstance, LPCWSTR lpTemplateName, HWND hWndParent,
                                  DLGPROC lpDialogFunc, LPARAM dwInitParam);

// *Indirect* variants: caller already has an HGLOBAL from WonLoadResource
// (e.g. obtained via WonFindResource(..., RT_DIALOG) themselves). These
// still go through WonLockResource, so an encrypted template that fails to
// decrypt is handled the same fail-closed way as the non-Indirect forms.
INT_PTR WONAPI WonDialogBoxIndirectParamA(HINSTANCE hInstance, HGLOBAL hDialogTemplate,
                                          HWND hWndParent, DLGPROC lpDialogFunc,
                                          LPARAM dwInitParam);
INT_PTR WONAPI WonDialogBoxIndirectParamW(HINSTANCE hInstance, HGLOBAL hDialogTemplate,
                                          HWND hWndParent, DLGPROC lpDialogFunc,
                                          LPARAM dwInitParam);

HWND WONAPI WonCreateDialogIndirectParamA(HINSTANCE hInstance, HGLOBAL hDialogTemplate,
                                          HWND hWndParent, DLGPROC lpDialogFunc,
                                          LPARAM dwInitParam);
HWND WONAPI WonCreateDialogIndirectParamW(HINSTANCE hInstance, HGLOBAL hDialogTemplate,
                                          HWND hWndParent, DLGPROC lpDialogFunc,
                                          LPARAM dwInitParam);

// Convenience wrappers matching plain DialogBox/CreateDialog (dwInitParam == 0)
#define WonDialogBoxA(hInstance, lpTemplateName, hWndParent, lpDialogFunc) \
    WonDialogBoxParamA(hInstance, lpTemplateName, hWndParent, lpDialogFunc, 0)
#define WonDialogBoxW(hInstance, lpTemplateName, hWndParent, lpDialogFunc) \
    WonDialogBoxParamW(hInstance, lpTemplateName, hWndParent, lpDialogFunc, 0)
#define WonCreateDialogA(hInstance, lpTemplateName, hWndParent, lpDialogFunc) \
    WonCreateDialogParamA(hInstance, lpTemplateName, hWndParent, lpDialogFunc, 0)
#define WonCreateDialogW(hInstance, lpTemplateName, hWndParent, lpDialogFunc) \
    WonCreateDialogParamW(hInstance, lpTemplateName, hWndParent, lpDialogFunc, 0)
#define WonDialogBoxIndirectA(hInstance, hDialogTemplate, hWndParent, lpDialogFunc) \
    WonDialogBoxIndirectParamA(hInstance, hDialogTemplate, hWndParent, lpDialogFunc, 0)
#define WonDialogBoxIndirectW(hInstance, hDialogTemplate, hWndParent, lpDialogFunc) \
    WonDialogBoxIndirectParamW(hInstance, hDialogTemplate, hWndParent, lpDialogFunc, 0)
#define WonCreateDialogIndirectA(hInstance, hDialogTemplate, hWndParent, lpDialogFunc) \
    WonCreateDialogIndirectParamA(hInstance, hDialogTemplate, hWndParent, lpDialogFunc, 0)
#define WonCreateDialogIndirectW(hInstance, hDialogTemplate, hWndParent, lpDialogFunc) \
    WonCreateDialogIndirectParamW(hInstance, hDialogTemplate, hWndParent, lpDialogFunc, 0)

////////////////////////////////////////////////////////////////////////////////////
// Menus (RT_MENU), loaded/decrypted the same way as everything else above

HMENU WONAPI WonLoadMenuA(HINSTANCE hInstance, LPCSTR lpMenuName);
HMENU WONAPI WonLoadMenuW(HINSTANCE hInstance, LPCWSTR lpMenuName);

// *Indirect* variants: caller already has an HGLOBAL from WonLoadResource
// (e.g. obtained via WonFindResource(..., RT_MENU) themselves), passed in
// place of the usual (pre-locked) MENUTEMPLATE pointer. This still goes
// through WonLockResource, so an encrypted template that fails to decrypt
// is handled the same fail-closed way as the non-Indirect forms.
HMENU WONAPI WonLoadMenuIndirectA(HGLOBAL hMenuTemplate);
HMENU WONAPI WonLoadMenuIndirectW(HGLOBAL hMenuTemplate);

////////////////////////////////////////////////////////////////////////////////////
// Accelerator tables (RT_ACCELERATOR), loaded/decrypted the same way as
// everything else above. Unlike dialogs/menus, real Win32 has no separate
// *Indirect entry point here -- CreateAcceleratorTableA/W already fills
// that role, taking the raw ACCEL[] array directly -- so there's no
// WonLoadAcceleratorsIndirect to go with it.

HACCEL WONAPI WonLoadAcceleratorsA(HINSTANCE hInstance, LPCSTR lpTableName);
HACCEL WONAPI WonLoadAcceleratorsW(HINSTANCE hInstance, LPCWSTR lpTableName);

////////////////////////////////////////////////////////////////////////////////////
// Icons (RT_GROUP_ICON/RT_ICON) and cursors (RT_GROUP_CURSOR/RT_CURSOR),
// loaded/decrypted the same way as everything else above. Real Win32 has
// no *Indirect entry points for these -- the public LookupIconIdFromDirectoryEx
// + CreateIconFromResourceEx pair already fills that role internally.
//
// hInstance == NULL with an int resource name (IDI_*/IDC_* constants) is
// forwarded to the real LoadIconW/LoadCursorW: those refer to the OS's own
// predefined icons/cursors, not a PE resource Won's loader could find.
//
// Animated icons/cursors (RT_ANIICON/RT_ANICURSOR, i.e. embedded .ani/RIFF
// data) are handled transparently too: if the name doesn't resolve to a
// static RT_GROUP_ICON/RT_GROUP_CURSOR, these fall back to the animated
// form the same way the real LoadIcon/LoadCursor do. That fallback (in
// anicursor.c) has to bridge through a short-lived temp file, since Win32
// has no public API to build an animated handle straight from memory --
// see anicursor.c for details and the resulting disk-exposure trade-off.

HICON WONAPI WonLoadIconA(HINSTANCE hInstance, LPCSTR lpIconName);
HICON WONAPI WonLoadIconW(HINSTANCE hInstance, LPCWSTR lpIconName);

HCURSOR WONAPI WonLoadCursorA(HINSTANCE hInstance, LPCSTR lpCursorName);
HCURSOR WONAPI WonLoadCursorW(HINSTANCE hInstance, LPCWSTR lpCursorName);

////////////////////////////////////////////////////////////////////////////////////
// FormatMessage. Only FORMAT_MESSAGE_FROM_HMODULE is Won-specific -- the
// module's RT_MESSAGETABLE resource is located/decrypted the same way as
// everything else above, and the located message text is then handed to
// the real FormatMessageA/W (as FORMAT_MESSAGE_FROM_STRING) to do the
// actual insert-sequence formatting. Every other dwFlags combination is
// forwarded to the real FormatMessageA/W untouched.

DWORD WONAPI WonFormatMessageA(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId,
                               DWORD dwLanguageId, LPSTR lpBuffer, DWORD nSize,
                               va_list *Arguments);
DWORD WONAPI WonFormatMessageW(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId,
                               DWORD dwLanguageId, LPWSTR lpBuffer, DWORD nSize,
                               va_list *Arguments);

////////////////////////////////////////////////////////////////////////////////////
// LoadImage: the generic IMAGE_BITMAP/IMAGE_ICON/IMAGE_CURSOR loader. Only
// resource loading (no LR_LOADFROMFILE) is Won-specific -- icons/cursors
// reuse the same LookupIconIdFromDirectoryEx + CreateIconFromResourceEx
// pair as WonLoadIcon/WonLoadCursor, and bitmaps go through
// CreateDIBitmap/CreateDIBSection over the raw RT_BITMAP resource bytes.
// LR_LOADFROMFILE has nothing to do with PE resources at all and is
// forwarded to the real LoadImageA/W untouched, as is hInstance == NULL
// with an int resource name (an OS-predefined OEM image, same special
// case as WonLoadIcon/WonLoadCursor). IMAGE_ICON/IMAGE_CURSOR also get
// the same animated (.ani/RIFF) fallback described above WonLoadIconA/W.
//
// Known gaps vs. the real LoadImage: LR_LOADTRANSPARENT and
// LR_LOADMAP3DCOLORS color-remapping for IMAGE_BITMAP aren't implemented;
// LR_SHARED doesn't provide real handle caching (every call still
// allocates a fresh handle).

HANDLE WONAPI WonLoadImageA(HINSTANCE hInstance, LPCSTR lpName, UINT uType, INT cxDesired,
                            INT cyDesired, UINT fuLoad);
HANDLE WONAPI WonLoadImageW(HINSTANCE hInstance, LPCWSTR lpName, UINT uType, INT cxDesired,
                            INT cyDesired, UINT fuLoad);

// Frees a buffer returned by WonLoadResource() for an *encrypted* resource
// (decryption allocates a fresh heap buffer instead of aliasing the image).
// Safe/no-op to call on NULL. Not calling it just leaks until process exit,
// same as the OS does for classic LoadResource() handles.
BOOL WONAPI WonFreeResource(HGLOBAL hGlobal);

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

#define WON_CRYPT_MAGIC               "WONRSRC1" // 8 bytes, no NUL terminator stored
#define WON_ENCRYPTION_KEY_SIZE       32         // AES-256
#define WON_ENCRYPTION_MIN_ITERATIONS 100000     // minimum PBKDF2 iterations enforced

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
#define WonDialogBoxParam WonDialogBoxParamW
#define WonCreateDialogParam WonCreateDialogParamW
#define WonDialogBoxIndirectParam WonDialogBoxIndirectParamW
#define WonCreateDialogIndirectParam WonCreateDialogIndirectParamW
#define WonDialogBox WonDialogBoxW
#define WonCreateDialog WonCreateDialogW
#define WonDialogBoxIndirect WonDialogBoxIndirectW
#define WonCreateDialogIndirect WonCreateDialogIndirectW
#define WonLoadMenu WonLoadMenuW
#define WonLoadMenuIndirect WonLoadMenuIndirectW
#define WonLoadAccelerators WonLoadAcceleratorsW
#define WonLoadIcon WonLoadIconW
#define WonLoadCursor WonLoadCursorW
#define WonFormatMessage WonFormatMessageW
#define WonLoadImage WonLoadImageW
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
#define WonDialogBoxParam WonDialogBoxParamA
#define WonCreateDialogParam WonCreateDialogParamA
#define WonDialogBoxIndirectParam WonDialogBoxIndirectParamA
#define WonCreateDialogIndirectParam WonCreateDialogIndirectParamA
#define WonDialogBox WonDialogBoxA
#define WonCreateDialog WonCreateDialogA
#define WonDialogBoxIndirect WonDialogBoxIndirectA
#define WonCreateDialogIndirect WonCreateDialogIndirectA
#define WonLoadMenu WonLoadMenuA
#define WonLoadMenuIndirect WonLoadMenuIndirectA
#define WonLoadAccelerators WonLoadAcceleratorsA
#define WonLoadIcon WonLoadIconA
#define WonLoadCursor WonLoadCursorA
#define WonFormatMessage WonFormatMessageA
#define WonLoadImage WonLoadImageA
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ndef __WONRES__
