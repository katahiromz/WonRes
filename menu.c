// menu.c --- Win32 menu loader for WonRes
// Author: katahiromz
// License: MIT
//
// Same idea as dialog.c: LoadMenu is just "find the RT_MENU resource, then
// hand the in-memory MENUTEMPLATE to LoadMenuIndirect". We reproduce that
// using WonFindResourceW/A + WonLoadResource + WonLockResource -- which is
// where any WONRES_ENABLE_CRYPTO decryption already happens -- and then
// delegate to the real LoadMenuIndirectA/W, since the on-disk MENUTEMPLATE
// layout is unaffected by Won's custom resource lookup.
#include <windows.h>
#include <imagehlp.h>
#include "WonRes.h"

// hMenuTemplate is the (still unlocked) HGLOBAL from WonLoadResource --
// mirrors WonDialogBoxIndirectParamW taking hDialogTemplate -- not a
// pre-locked MENUTEMPLATE pointer like real LoadMenuIndirect expects.
// WonLockResource does the locking (and any transparent decryption).
HMENU WONAPI WonLoadMenuIndirectW(HGLOBAL hMenuTemplate)
{
    LPVOID pTemplate = WonLockResource(hMenuTemplate);
    if (!pTemplate) // encrypted resource that failed to decrypt (no/wrong key, tampering)
        return NULL;

    return LoadMenuIndirectW(pTemplate);
}

HMENU WONAPI WonLoadMenuIndirectA(HGLOBAL hMenuTemplate)
{
    LPVOID pTemplate = WonLockResource(hMenuTemplate);
    if (!pTemplate)
        return NULL;

    return LoadMenuIndirectA(pTemplate);
}

////////////////////////////////////////////////////////////////////////////////////
// Find + load the RT_MENU resource, then delegate to *Indirect above.
// LoadMenuIndirect copies out what it needs before returning, so it's safe
// to release the (possibly heap-allocated, if decrypted) buffer via
// WonFreeResource right after the call in every case, success or failure.

HMENU WONAPI WonLoadMenuW(HINSTANCE hInstance, LPCWSTR lpMenuName)
{
    HRSRC hRsrc = WonFindResourceW(hInstance, lpMenuName, (LPWSTR)RT_MENU);
    if (!hRsrc)
        return NULL;

    HGLOBAL hGlobal = WonLoadResource(hInstance, hRsrc);
    if (!hGlobal)
        return NULL;

    HMENU hMenu = WonLoadMenuIndirectW(hGlobal);

    WonFreeResource(hGlobal);
    return hMenu;
}

HMENU WONAPI WonLoadMenuA(HINSTANCE hInstance, LPCSTR lpMenuName)
{
    HRSRC hRsrc = WonFindResourceA(hInstance, lpMenuName, (LPSTR)RT_MENU);
    if (!hRsrc)
        return NULL;

    HGLOBAL hGlobal = WonLoadResource(hInstance, hRsrc);
    if (!hGlobal)
        return NULL;

    HMENU hMenu = WonLoadMenuIndirectA(hGlobal);

    WonFreeResource(hGlobal);
    return hMenu;
}
