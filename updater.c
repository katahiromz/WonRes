// updater.c --- Win32 resource updater for WonRes
// Author: katahiromz
// License: MIT
#include <windows.h>
#include <imagehlp.h>
#include "WonRes.h"

// Script: C99/Win32でBeginUpdateResourceWなどのリソース更新関数を再実装してください。
// 実行モジュール・更新モジュールについてx86/x64両方に対応して下さい。
// x86からx64の書き込み、x64からx86の書き込みにも対応してください。

HANDLE WONAPI WonBeginUpdateResourceW(LPCWSTR pFileName, BOOL bDeleteExistingResources)
{
    // TODO:
    return NULL;
}

BOOL WONAPI WonUpdateResourceW(HANDLE hUpdate, LPCWSTR lpType, LPCWSTR lpName, WORD wLanguage,
                               LPVOID lpData, DWORD cbData)
{
    // TODO:
    return FALSE;
}

BOOL WONAPI WonEndUpdateResourceW(HANDLE hUpdate, BOOL fDiscard)
{
    // TODO:
    return FALSE;
}

HANDLE WONAPI WonBeginUpdateResourceA(LPCSTR pFileName, BOOL bDeleteExistingResources)
{
    WCHAR szFileNameW[MAX_PATH];
    if (!MultiByteToWideChar(CP_ACP, 0, pFileName, -1, szFileNameW, _countof(szFileNameW)))
        return NULL;
    szFileNameW[_countof(szFileNameW) - 1] = UNICODE_NULL;
    return WonBeginUpdateResourceW(szFileNameW, bDeleteExistingResources);
}

BOOL WONAPI WonUpdateResourceA(HANDLE hUpdate, LPCSTR lpType, LPCSTR lpName, WORD wLanguage,
                               LPVOID lpData, DWORD cbData)
{
    WCHAR szTypeW[MAX_RES_ID_LEN], szNameW[MAX_RES_ID_LEN];
    LPWSTR pszTypeW, pszNameW;

    if (IS_INTRESOURCE(lpType)) {
        pszTypeW = MAKEINTRESOURCEW(PtrToUshort(lpType));
    } else {
        if (!MultiByteToWideChar(CP_ACP, 0, lpType, -1, szTypeW, _countof(szTypeW)))
            return FALSE;

        szTypeW[_countof(szTypeW) - 1] = UNICODE_NULL;
        pszTypeW = szTypeW;
    }

    if (IS_INTRESOURCE(lpName)) {
        pszNameW = MAKEINTRESOURCEW(PtrToUshort(lpName));
    } else {
        if (!MultiByteToWideChar(CP_ACP, 0, lpName, -1, szNameW, _countof(szNameW)))
            return FALSE;

        szNameW[_countof(szNameW) - 1] = UNICODE_NULL;
        pszNameW = szNameW;
    }

    return WonUpdateResourceW(hUpdate, pszTypeW, pszNameW, wLanguage, lpData, cbData);
}

BOOL WONAPI WonEndUpdateResourceA(HANDLE hUpdate, BOOL fDiscard)
{
    return WonEndUpdateResourceW(hUpdate, fDiscard);
}
