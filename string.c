// string.c --- Win32 resource loader for WonRes
// Author: katahiromz
// License: MIT
#include <windows.h>
#include <imagehlp.h>
#include <string.h>
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
    if (!p) // encrypted resource that failed to decrypt (no/wrong key, tampering)
        return 0;

    uID &= 0x000F;

    for (UINT i = 0; i < uID; i++)
        p += *p + 1;

    if (nBufferMax == 0) { // Special case: Return a pointer to (LPWSTR *)lpBuffer
        *((LPWSTR *)lpBuffer) = p + 1;
        return *p;
    }

    INT i = min(nBufferMax - 1, *p);
    if (i > 0) {
        memcpy(lpBuffer, p + 1, i * sizeof(WCHAR));
        lpBuffer[i] = UNICODE_NULL;
    } else {
        if (nBufferMax > 1) {
            lpBuffer[0] = UNICODE_NULL;
            return 0;
        }
    }

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

            if (p) {
                while (uID--)
                    p += *p + 1;

                if (nBufferMax != 1) {
                    retval = WideCharToMultiByte(CP_ACP, 0, (PWSTR)(p + 1), *p, lpBuffer,
                                                 nBufferMax - 1, NULL, NULL);
                }
            }
        }
    }

    lpBuffer[retval] = ANSI_NULL;
    return retval;
}
