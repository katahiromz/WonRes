// anicursor.c --- Win32 animated icon/cursor (.ani/RIFF) loader for WonRes
// Author: katahiromz
// License: MIT
//
// Animated icons/cursors are stored as RT_ANIICON/RT_ANICURSOR resources
// containing a complete RIFF ('ACON' form) blob -- essentially a whole
// .ani file embedded verbatim, with an 'anih' ANIHEADER chunk, optional
// 'rate'/'seq ' chunks, and a 'fram' LIST of 'icon' sub-chunks that are
// each themselves a full, self-contained .ico/.cur file.
//
// Unlike static RT_ICON/RT_CURSOR data, there is no public Win32 API that
// builds an animated HICON/HCURSOR straight from an in-memory RIFF buffer
// (no CreateIconFromResourceEx equivalent for this format) -- the only
// documented entry points are LoadImage(..., LR_LOADFROMFILE) and
// LoadCursorFromFile, and both read from disk. So this bridges the two:
// the raw (and, if WONRES_ENABLE_CRYPTO is in play, already-decrypted)
// RIFF bytes obtained the usual Won way -- WonFindResourceW/A +
// WonLoadResource + WonLockResource -- are written out to a short-lived
// temp file, handed to the real LoadImageW to build the handle, and the
// temp file is deleted immediately after, success or failure, so the
// plaintext only ever touches disk for the instant it takes to load it.
// FILE_ATTRIBUTE_TEMPORARY additionally hints the OS to keep it cached in
// memory rather than flushing it to disk if it can avoid doing so.
//
// This is an internal helper (not part of the public WonRes.h surface),
// called from iconcursor.c and loadimage.c as a fallback once their
// normal RT_GROUP_ICON/RT_GROUP_CURSOR lookup misses -- exactly the same
// order the real LoadIcon/LoadCursor try things in.
#include <windows.h>
#include <imagehlp.h>
#include "WonRes.h"

HANDLE WonpLoadAnimatedIconOrCursorW(HINSTANCE hInstance, LPCWSTR lpName, BOOL fIcon, INT cx,
                                     INT cy, UINT fuLoad)
{
    LPCWSTR pType = fIcon ? (LPCWSTR)RT_ANIICON : (LPCWSTR)RT_ANICURSOR;

    HRSRC hRsrc = WonFindResourceW(hInstance, lpName, pType);
    if (!hRsrc)
        return NULL;

    // Must be read before WonLoadResource/WonFreeResource for an encrypted
    // resource: WonSizeofResource reports the plaintext size in that case
    // (see loader.c), matching what WonLockResource will hand back below.
    DWORD cb = WonSizeofResource(hInstance, hRsrc);

    HGLOBAL hGlobal = WonLoadResource(hInstance, hRsrc);
    if (!hGlobal)
        return NULL;

    LPBYTE pBits = (LPBYTE)WonLockResource(hGlobal);
    if (!pBits) { // encrypted resource that failed to decrypt
        WonFreeResource(hGlobal);
        return NULL;
    }

    HANDLE hResult = NULL;
    WCHAR szTempDir[MAX_PATH];
    WCHAR szTempFile[MAX_PATH];

    if (GetTempPathW(_countof(szTempDir), szTempDir) &&
        GetTempFileNameW(szTempDir, L"won", 0, szTempFile)) {
        HANDLE hFile = CreateFileW(szTempFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                                   FILE_ATTRIBUTE_TEMPORARY, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD cbWritten = 0;
            BOOL fOk = WriteFile(hFile, pBits, cb, &cbWritten, NULL) && cbWritten == cb;
            CloseHandle(hFile);

            if (fOk) {
                hResult = (HANDLE)LoadImageW(NULL, szTempFile, fIcon ? IMAGE_ICON : IMAGE_CURSOR,
                                             cx, cy, fuLoad | LR_LOADFROMFILE);
            }
        }

        DeleteFileW(szTempFile); // best-effort cleanup, success or failure
    }

    WonFreeResource(hGlobal);
    return hResult;
}
