// string.c --- Win32 resource loader for WonRes
// Author: katahiromz
// License: MIT
#include <windows.h>
#include <imagehlp.h>
#include <string.h>
#include "WonRes.h"

int WONAPI WonLoadStringW(HINSTANCE hInstance, UINT uID, LPWSTR lpBuffer, int nBufferMax)
{
    if (!lpBuffer)
        return 0;

    HRSRC hRsrc =
        FindResourceW(hInstance, MAKEINTRESOURCEW((LOWORD(uID) >> 4) + 1), (LPWSTR)RT_STRING);
    if (!hRsrc)
        return 0;

    HGLOBAL hGlobal = LoadResource(hInstance, hRsrc);
    if (!hGlobal)
        return 0;

    LPWSTR p = LockResource(hGlobal);

    uID &= 0x000F;

    for (int i = 0; i < uID; i++)
        p += *p + 1;

    if (nBufferMax == 0) {
        *((LPWSTR *)lpBuffer) = p + 1;
        return *p;
    }

    int i = min(nBufferMax - 1, *p);
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

int WONAPI WonLoadStringA(HINSTANCE hInstance, UINT uID, LPSTR lpBuffer, int nBufferMax)
{
    if (!nBufferMax)
        return -1;

    int retval = 0;
    HRSRC hRsrc =
        FindResourceW(hInstance, MAKEINTRESOURCEW((LOWORD(uID) >> 4) + 1), (LPWSTR)RT_STRING);
    if (hRsrc) {
        HGLOBAL hGlobal = LoadResource(hInstance, hRsrc);
        if (hGlobal) {
            LPWSTR p = LockResource(hGlobal);
            uID &= 0x000F;

            while (uID--)
                p += *p + 1;

            if (nBufferMax != 1) {
                retval = WideCharToMultiByte(CP_ACP, 0, (PWSTR)(p + 1), *p, lpBuffer,
                                             nBufferMax - 1, NULL, NULL);
            }
        }
    }

    lpBuffer[retval] = ANSI_NULL;
    return retval;
}
