// dialog.c --- Win32 dialog-box loader for WonRes
// Author: katahiromz
// License: MIT
//
// DialogBoxParam/CreateDialogParam (and their non-Param and *Indirect*
// cousins) are, internally, just "find RT_DIALOG resource, then hand the
// in-memory DLGTEMPLATE to *IndirectParam". That means we get Won-aware
// (LOAD_LIBRARY_AS_DATAFILE-safe, cross-bitness, transparently-decrypting)
// dialog loading for free by doing exactly that ourselves: look the
// template up with WonFindResourceW, materialize it with WonLoadResource
// + WonLockResource -- which is where any WONRES_ENABLE_CRYPTO decryption
// already happens -- and then delegate to the real
// DialogBoxIndirectParam/CreateDialogIndirectParam to do the actual UI
// work, since the on-disk DLGTEMPLATE layout is unaffected by Won's
// custom resource lookup.
#include <windows.h>
#include <imagehlp.h>
#include "WonRes.h"

INT_PTR WONAPI WonDialogBoxIndirectParamW(HINSTANCE hInstance, HGLOBAL hDialogTemplate,
                                          HWND hWndParent, DLGPROC lpDialogFunc,
                                          LPARAM dwInitParam)
{
    LPCDLGTEMPLATEW pTemplate = (LPCDLGTEMPLATEW)WonLockResource(hDialogTemplate);
    if (!pTemplate) // encrypted resource that failed to decrypt (no/wrong key, tampering)
        return -1;

    return DialogBoxIndirectParamW(hInstance, pTemplate, hWndParent, lpDialogFunc, dwInitParam);
}

INT_PTR WONAPI WonDialogBoxIndirectParamA(HINSTANCE hInstance, HGLOBAL hDialogTemplate,
                                          HWND hWndParent, DLGPROC lpDialogFunc,
                                          LPARAM dwInitParam)
{
    LPCDLGTEMPLATEA pTemplate = (LPCDLGTEMPLATEA)WonLockResource(hDialogTemplate);
    if (!pTemplate)
        return -1;

    return DialogBoxIndirectParamA(hInstance, pTemplate, hWndParent, lpDialogFunc, dwInitParam);
}

HWND WONAPI WonCreateDialogIndirectParamW(HINSTANCE hInstance, HGLOBAL hDialogTemplate,
                                          HWND hWndParent, DLGPROC lpDialogFunc,
                                          LPARAM dwInitParam)
{
    LPCDLGTEMPLATEW pTemplate = (LPCDLGTEMPLATEW)WonLockResource(hDialogTemplate);
    if (!pTemplate)
        return NULL;

    return CreateDialogIndirectParamW(hInstance, pTemplate, hWndParent, lpDialogFunc, dwInitParam);
}

HWND WONAPI WonCreateDialogIndirectParamA(HINSTANCE hInstance, HGLOBAL hDialogTemplate,
                                          HWND hWndParent, DLGPROC lpDialogFunc,
                                          LPARAM dwInitParam)
{
    LPCDLGTEMPLATEA pTemplate = (LPCDLGTEMPLATEA)WonLockResource(hDialogTemplate);
    if (!pTemplate)
        return NULL;

    return CreateDialogIndirectParamA(hInstance, pTemplate, hWndParent, lpDialogFunc, dwInitParam);
}

////////////////////////////////////////////////////////////////////////////////////
// Find + load the RT_DIALOG resource, then delegate to *IndirectParam above.
// Both the modal (DialogBox) and modeless (CreateDialog) creators only need
// the template while the call is in progress -- CreateDialogIndirectParam
// copies out what it needs before returning -- so it's safe to release the
// (possibly heap-allocated, if decrypted) buffer via WonFreeResource right
// after the call in every case, success or failure.

INT_PTR WONAPI WonDialogBoxParamW(HINSTANCE hInstance, LPCWSTR lpTemplateName, HWND hWndParent,
                                  DLGPROC lpDialogFunc, LPARAM dwInitParam)
{
    HRSRC hRsrc = WonFindResourceW(hInstance, lpTemplateName, (LPWSTR)RT_DIALOG);
    if (!hRsrc)
        return -1;

    HGLOBAL hGlobal = WonLoadResource(hInstance, hRsrc);
    if (!hGlobal)
        return -1;

    INT_PTR ret =
        WonDialogBoxIndirectParamW(hInstance, hGlobal, hWndParent, lpDialogFunc, dwInitParam);

    WonFreeResource(hGlobal);
    return ret;
}

INT_PTR WONAPI WonDialogBoxParamA(HINSTANCE hInstance, LPCSTR lpTemplateName, HWND hWndParent,
                                  DLGPROC lpDialogFunc, LPARAM dwInitParam)
{
    HRSRC hRsrc = WonFindResourceA(hInstance, lpTemplateName, (LPSTR)RT_DIALOG);
    if (!hRsrc)
        return -1;

    HGLOBAL hGlobal = WonLoadResource(hInstance, hRsrc);
    if (!hGlobal)
        return -1;

    INT_PTR ret =
        WonDialogBoxIndirectParamA(hInstance, hGlobal, hWndParent, lpDialogFunc, dwInitParam);

    WonFreeResource(hGlobal);
    return ret;
}

HWND WONAPI WonCreateDialogParamW(HINSTANCE hInstance, LPCWSTR lpTemplateName, HWND hWndParent,
                                  DLGPROC lpDialogFunc, LPARAM dwInitParam)
{
    HRSRC hRsrc = WonFindResourceW(hInstance, lpTemplateName, (LPWSTR)RT_DIALOG);
    if (!hRsrc)
        return NULL;

    HGLOBAL hGlobal = WonLoadResource(hInstance, hRsrc);
    if (!hGlobal)
        return NULL;

    HWND hWnd =
        WonCreateDialogIndirectParamW(hInstance, hGlobal, hWndParent, lpDialogFunc, dwInitParam);

    WonFreeResource(hGlobal);
    return hWnd;
}

HWND WONAPI WonCreateDialogParamA(HINSTANCE hInstance, LPCSTR lpTemplateName, HWND hWndParent,
                                  DLGPROC lpDialogFunc, LPARAM dwInitParam)
{
    HRSRC hRsrc = WonFindResourceA(hInstance, lpTemplateName, (LPSTR)RT_DIALOG);
    if (!hRsrc)
        return NULL;

    HGLOBAL hGlobal = WonLoadResource(hInstance, hRsrc);
    if (!hGlobal)
        return NULL;

    HWND hWnd =
        WonCreateDialogIndirectParamA(hInstance, hGlobal, hWndParent, lpDialogFunc, dwInitParam);

    WonFreeResource(hGlobal);
    return hWnd;
}
