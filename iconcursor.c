// iconcursor.c --- Win32 icon/cursor loader for WonRes
// Author: katahiromz
// License: MIT
//
// LoadIcon/LoadCursor are, internally, a two-hop resource lookup:
//   1. find the RT_GROUP_ICON/RT_GROUP_CURSOR resource (the NEWHEADER +
//      RESDIR[] directory describing every size/color-depth variant that
//      was packed into the group) and ask LookupIconIdFromDirectoryEx --
//      a public, documented Win32 API made exactly for this -- which
//      variant best matches the requested size, returning that variant's
//      RT_ICON/RT_CURSOR resource ID.
//   2. find *that* RT_ICON/RT_CURSOR resource (the actual pixel data) and
//      hand its bytes to CreateIconFromResourceEx, again a public API, to
//      build the real HICON/HCURSOR.
// Both hops go through WonFindResourceW/A + WonLoadResource +
// WonLockResource -- which is where any WONRES_ENABLE_CRYPTO decryption
// already happens -- so this file never needs to touch the private
// GRPICONDIR/RESDIR byte layout itself; LookupIconIdFromDirectoryEx does.
//
// hInstance == NULL together with an int resource (e.g. IDI_APPLICATION,
// IDC_ARROW) means "load one of the OS's own predefined icons/cursors" --
// those aren't PE resources at all, so Won's resource-directory lookup
// can't (and shouldn't) handle them; that one case is forwarded straight
// to the real LoadIconW/LoadCursorW, exactly like every other Win32
// LoadIcon/LoadCursor reimplementation has to.
#include <windows.h>
#include <imagehlp.h>
#include "WonRes.h"

static HANDLE LoadIconOrCursorW(HINSTANCE hInstance, LPCWSTR lpName, BOOL fIcon)
{
    if (!hInstance && IS_INTRESOURCE(lpName))
        return fIcon ? (HANDLE)LoadIconW(NULL, lpName) : (HANDLE)LoadCursorW(NULL, lpName);

    LPCWSTR pGroupType = fIcon ? (LPCWSTR)RT_GROUP_ICON : (LPCWSTR)RT_GROUP_CURSOR;
    LPCWSTR pItemType = fIcon ? (LPCWSTR)RT_ICON : (LPCWSTR)RT_CURSOR;
    INT cx = GetSystemMetrics(fIcon ? SM_CXICON : SM_CXCURSOR);
    INT cy = GetSystemMetrics(fIcon ? SM_CYICON : SM_CYCURSOR);

    // Hop 1: pick the best-matching variant out of the group directory.
    HRSRC hRsrcGroup = WonFindResourceW(hInstance, lpName, pGroupType);
    if (!hRsrcGroup)
        return NULL;

    HGLOBAL hGlobalGroup = WonLoadResource(hInstance, hRsrcGroup);
    if (!hGlobalGroup)
        return NULL;

    LPBYTE pDir = (LPBYTE)WonLockResource(hGlobalGroup);
    INT nID = 0;
    if (pDir) // NULL here means an encrypted resource that failed to decrypt
        nID = LookupIconIdFromDirectoryEx(pDir, fIcon, cx, cy, LR_DEFAULTCOLOR);
    WonFreeResource(hGlobalGroup);

    if (!nID)
        return NULL;

    // Hop 2: load the chosen variant's actual pixel data and build the handle.
    HRSRC hRsrcItem = WonFindResourceW(hInstance, MAKEINTRESOURCEW(nID), pItemType);
    if (!hRsrcItem)
        return NULL;

    // Must be read before WonLoadResource/WonFreeResource for an encrypted
    // resource: WonSizeofResource reports the plaintext size in that case
    // (see loader.c), matching what WonLockResource will hand back below.
    DWORD cbItem = WonSizeofResource(hInstance, hRsrcItem);

    HGLOBAL hGlobalItem = WonLoadResource(hInstance, hRsrcItem);
    if (!hGlobalItem)
        return NULL;

    LPBYTE pBits = (LPBYTE)WonLockResource(hGlobalItem);
    HANDLE hResult = NULL;
    if (pBits) {
        hResult = (HANDLE)CreateIconFromResourceEx(pBits, cbItem, fIcon, 0x00030000, cx, cy,
                                                    LR_DEFAULTCOLOR);
    }

    WonFreeResource(hGlobalItem);
    return hResult;
}

HICON WONAPI WonLoadIconW(HINSTANCE hInstance, LPCWSTR lpIconName)
{
    return (HICON)LoadIconOrCursorW(hInstance, lpIconName, TRUE);
}

HCURSOR WONAPI WonLoadCursorW(HINSTANCE hInstance, LPCWSTR lpCursorName)
{
    return (HCURSOR)LoadIconOrCursorW(hInstance, lpCursorName, FALSE);
}

////////////////////////////////////////////////////////////////////////////////////
// ANSI wrappers: convert the name to wide (same pattern as e.g.
// WonFindResourceExA in loader.c) and reuse the wide implementation. Int
// resources pass through untouched either way.

HICON WONAPI WonLoadIconA(HINSTANCE hInstance, LPCSTR lpIconName)
{
    LPCWSTR pszNameW;
    WCHAR szNameW[MAX_RES_ID_LEN];

    if (IS_INTRESOURCE(lpIconName)) {
        pszNameW = (LPCWSTR)lpIconName;
    } else {
        if (!MultiByteToWideChar(CP_ACP, 0, lpIconName, -1, szNameW, _countof(szNameW)))
            return NULL;
        szNameW[_countof(szNameW) - 1] = UNICODE_NULL;
        pszNameW = szNameW;
    }

    return WonLoadIconW(hInstance, pszNameW);
}

HCURSOR WONAPI WonLoadCursorA(HINSTANCE hInstance, LPCSTR lpCursorName)
{
    LPCWSTR pszNameW;
    WCHAR szNameW[MAX_RES_ID_LEN];

    if (IS_INTRESOURCE(lpCursorName)) {
        pszNameW = (LPCWSTR)lpCursorName;
    } else {
        if (!MultiByteToWideChar(CP_ACP, 0, lpCursorName, -1, szNameW, _countof(szNameW)))
            return NULL;
        szNameW[_countof(szNameW) - 1] = UNICODE_NULL;
        pszNameW = szNameW;
    }

    return WonLoadCursorW(hInstance, pszNameW);
}
