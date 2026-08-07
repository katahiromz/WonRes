// string.c --- Win32 resource loader for WonRes
// Author: katahiromz
// License: MIT
#include <windows.h>
#include <imagehlp.h>
#include <string.h>
#include <assert.h>
#include "WonRes.h"

INT WONAPI WonLoadStringW(HINSTANCE hInstance, UINT uID, LPWSTR lpBuffer, INT nBufferMax)
{
    if (!lpBuffer)
        return 0;

    HRSRC hRsrc =
        WonFindResourceW(hInstance, MAKEINTRESOURCEW((LOWORD(uID) >> 4) + 1), (LPWSTR)RT_STRING);
    if (!hRsrc)
        return 0;

    HGLOBAL hGlobal = WonLoadResource(hInstance, hRsrc);
    if (!hGlobal)
        return 0;

    LPWSTR p = WonLockResource(hGlobal);
    if (!p) { // encrypted resource that failed to decrypt (no/wrong key, tampering)
        WonFreeResource(hGlobal);
        return 0;
    }

    uID &= 0x000F;

    for (UINT i = 0; i < uID; i++)
        p += *p + 1;

    if (nBufferMax == 0) {
        // Special case: return a pointer straight into the resource data
        // via (LPWSTR *)lpBuffer, matching real LoadStringW's own
        // undocumented behavior. That pointer has to stay valid *after*
        // we return, so -- unlike every other path here -- hGlobal must
        // NOT be freed: for a plaintext resource p just aliases the
        // module image and never needed freeing anyway, but for an
        // *encrypted* one p is the (otherwise-orphaned) decrypted heap
        // buffer, and freeing it now would hand the caller a dangling
        // pointer. This one path intentionally leaks until process exit,
        // exactly as documented on WonFreeResource() in WonRes.h.
#ifdef WONRES_ENABLE_CRYPTO
        assert(0);
#endif
        *((LPWSTR *)lpBuffer) = p + 1;
        return *p;
    }

    INT i = min(nBufferMax - 1, *p);
    if (i > 0) {
        memcpy(lpBuffer, p + 1, i * sizeof(WCHAR));
        lpBuffer[i] = UNICODE_NULL;
    } else if (nBufferMax > 1) {
        lpBuffer[0] = UNICODE_NULL;
        i = 0;
    }

    WonFreeResource(hGlobal);
    return i;
}

INT WONAPI WonLoadStringA(HINSTANCE hInstance, UINT uID, LPSTR lpBuffer, INT nBufferMax)
{
    if (nBufferMax <= 0)
        return -1;

    INT retval = 0;
    HRSRC hRsrc =
        WonFindResourceW(hInstance, MAKEINTRESOURCEW((LOWORD(uID) >> 4) + 1), (LPWSTR)RT_STRING);
    if (hRsrc) {
        HGLOBAL hGlobal = WonLoadResource(hInstance, hRsrc);
        if (hGlobal) {
            LPWSTR p = WonLockResource(hGlobal);
            uID &= 0x000F;

            if (p) { // NULL here means an encrypted resource that failed to decrypt
                while (uID--)
                    p += *p + 1;

                if (nBufferMax != 1) {
                    retval = WideCharToMultiByte(CP_ACP, 0, (PWSTR)(p + 1), *p, lpBuffer,
                                                 nBufferMax - 1, NULL, NULL);
                }
            }

            // Safe unconditionally here: WonLoadStringA always copies into
            // the caller's own lpBuffer (unlike WonLoadStringW's
            // nBufferMax==0 special case), so nothing outlives this call
            // that could still be pointing into hGlobal.
            WonFreeResource(hGlobal);
        }
    }

    lpBuffer[retval] = ANSI_NULL;
    return retval;
}
