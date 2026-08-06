// accel.c --- Win32 accelerator-table loader for WonRes
// Author: katahiromz
// License: MIT
//
// Same idea as dialog.c/menu.c: LoadAccelerators is just "find the
// RT_ACCELERATOR resource, then hand its raw ACCEL[] array to
// CreateAcceleratorTable" -- unlike dialogs/menus there's no separate
// *Indirect entry point in real Win32 (CreateAcceleratorTable already *is*
// the indirect half), so this file reproduces the whole thing directly:
// WonFindResourceW/A + WonLoadResource + WonLockResource -- which is where
// any WONRES_ENABLE_CRYPTO decryption already happens -- then
// CreateAcceleratorTableA/W over the resulting buffer, since the on-disk
// ACCEL array layout is unaffected by Won's custom resource lookup.
#include <windows.h>
#include <imagehlp.h>
#include "WonRes.h"

HACCEL WONAPI WonLoadAcceleratorsW(HINSTANCE hInstance, LPCWSTR lpTableName)
{
    HRSRC hRsrc = WonFindResourceW(hInstance, lpTableName, (LPWSTR)RT_ACCELERATOR);
    if (!hRsrc)
        return NULL;

    // Must be read before WonLoadResource/WonFreeResource for an encrypted
    // resource: WonSizeofResource reports the plaintext size in that case
    // (see loader.c), matching what WonLockResource will hand back below.
    DWORD cbAccel = WonSizeofResource(hInstance, hRsrc);

    HGLOBAL hGlobal = WonLoadResource(hInstance, hRsrc);
    if (!hGlobal)
        return NULL;

    HACCEL hAccel = NULL;
    LPACCEL pAccel = (LPACCEL)WonLockResource(hGlobal);
    if (pAccel) { // NULL here means an encrypted resource that failed to decrypt
        INT cEntries = (INT)(cbAccel / sizeof(ACCEL));
        if (cEntries > 0)
            hAccel = CreateAcceleratorTableW(pAccel, cEntries);
    }

    WonFreeResource(hGlobal);
    return hAccel;
}

HACCEL WONAPI WonLoadAcceleratorsA(HINSTANCE hInstance, LPCSTR lpTableName)
{
    HRSRC hRsrc = WonFindResourceA(hInstance, lpTableName, (LPSTR)RT_ACCELERATOR);
    if (!hRsrc)
        return NULL;

    DWORD cbAccel = WonSizeofResource(hInstance, hRsrc);

    HGLOBAL hGlobal = WonLoadResource(hInstance, hRsrc);
    if (!hGlobal)
        return NULL;

    HACCEL hAccel = NULL;
    LPACCEL pAccel = (LPACCEL)WonLockResource(hGlobal);
    if (pAccel) {
        INT cEntries = (INT)(cbAccel / sizeof(ACCEL));
        if (cEntries > 0)
            hAccel = CreateAcceleratorTableA(pAccel, cEntries);
    }

    WonFreeResource(hGlobal);
    return hAccel;
}
