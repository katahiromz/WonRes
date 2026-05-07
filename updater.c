// updater.c --- Win32 resource updater for WonRes
// Author: katahiromz
// License: MIT
#include <windows.h>
#include <imagehlp.h>
#include <string.h>
#include "WonRes.h"

// Script: C99/Win32でBeginUpdateResourceWなどのリソース更新関数を再実装してください。
// 実行モジュール・更新モジュールについてx86/x64両方に対応して下さい。
// x86からx64の書き込み、x64からx86の書き込みにも対応してください。

// リソースエントリの内部構造体
typedef struct WON_RES_ENTRY {
    LPWSTR type;
    LPWSTR name;
    WORD lang;
    DWORD size;
    LPVOID data;
    struct WON_RES_ENTRY *next;
} WON_RES_ENTRY, *PWON_RES_ENTRY;

typedef struct WON_UPDATE_DATA {
    LPWSTR pFileName;
    BOOL bDeleteExisting;
    PWON_RES_ENTRY pEntries;
} WON_UPDATE_DATA, *PWON_UPDATE_DATA;

// ヘルパー：リソースID/名前の複製
static inline LPWSTR DuplicateResId(LPCWSTR pszId)
{
    if (IS_INTRESOURCE(pszId))
        return (LPWSTR)pszId;
    return _wcsdup(pszId);
}

// ヘルパー：リソースID/名前の解放
static inline void FreeResId(LPWSTR pszId)
{
    if (!IS_INTRESOURCE(pszId))
        free(pszId);
}

// リソースIDが一致するか？
static inline BOOL MatchResId(LPCWSTR id1, LPCWSTR id2)
{
    if (IS_INTRESOURCE(id1) && IS_INTRESOURCE(id2))
        return id1 == id2;
    if (!IS_INTRESOURCE(id1) && !IS_INTRESOURCE(id2))
        return wcscmp(id1, id2) == 0;
    return FALSE;
}

// 実際にリソースを更新する関数
static BOOL WonRealUpdateResource(PWON_UPDATE_DATA pUpdate)
{
    LPWSTR pFileName = pUpdate->pFileName;
    PWON_RES_ENTRY pEntries = pUpdate->pEntries;

    // TODO: エントリ群をソート
    // TODO: 増加分のバイト数と更新後のファイルサイズの計算
    // TODO: ファイル作成
    // TODO: IMAGE_DIRECTORY_ENTRY_RESOURCE書き込み
    // TODO: エントリ書き込み

    return FALSE;
}

// 既存リソース読み込み用のコールバック
static BOOL CALLBACK LoadExistingResProc(HMODULE hMod, LPCWSTR lpType, LPCWSTR lpName, WORD wLang,
                                         LONG_PTR lParam)
{
    HANDLE hUpdate = (HANDLE)lParam;
    HRSRC hRes = WonFindResourceExW(hMod, lpType, lpName, wLang);
    if (hRes) {
        DWORD size = WonSizeofResource(hMod, hRes);
        HGLOBAL hGlobal = WonLoadResource(hMod, hRes);
        LPVOID pData = WonLockResource(hGlobal);
        if (pData) {
            WonUpdateResourceW(hUpdate, lpType, lpName, wLang, pData, size);
        }
    }
    return TRUE;
}

static BOOL CALLBACK LoadExistingNamesProc(HMODULE hMod, LPCWSTR lpType, LPWSTR lpName,
                                           LONG_PTR lParam)
{
    return WonEnumResourceLanguagesW(hMod, lpType, lpName, LoadExistingResProc, lParam);
}

static BOOL CALLBACK LoadExistingTypesProc(HMODULE hMod, LPWSTR lpType, LONG_PTR lParam)
{
    return WonEnumResourceNamesW(hMod, lpType, LoadExistingNamesProc, lParam);
}

HANDLE WONAPI WonBeginUpdateResourceW(LPCWSTR pFileName, BOOL bDeleteExistingResources)
{
    PWON_UPDATE_DATA pUpdate =
        (PWON_UPDATE_DATA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(WON_UPDATE_DATA));
    if (!pUpdate)
        return NULL;

    pUpdate->pFileName = _wcsdup(pFileName);
    pUpdate->bDeleteExisting = bDeleteExistingResources;

    if (!bDeleteExistingResources) {
        HMODULE hMod = LoadLibraryExW(pFileName, NULL, LOAD_LIBRARY_AS_DATAFILE);
        if (hMod) {
            // 既存のリソースをすべて内部リストにロードする
            WonEnumResourceTypesW(hMod, LoadExistingTypesProc, (LONG_PTR)pUpdate);
            FreeLibrary(hMod);
        }
    }
    return (HANDLE)pUpdate;
}

BOOL WONAPI WonUpdateResourceW(HANDLE hUpdate, LPCWSTR lpType, LPCWSTR lpName, WORD wLanguage,
                               LPVOID lpData, DWORD cbData)
{
    PWON_UPDATE_DATA pUpdate = (PWON_UPDATE_DATA)hUpdate;
    if (!pUpdate)
        return FALSE;

    // 同一リソース（Type, Name, Lang）があるか確認
    PWON_RES_ENTRY *ppNext = &pUpdate->pEntries;
    while (*ppNext) {
        PWON_RES_ENTRY pCurr = *ppNext;
        BOOL typeMatch = MatchResId(lpType, pCurr->type);
        BOOL nameMatch = MatchResId(lpName, pCurr->name);
        if (typeMatch && nameMatch && pCurr->lang == wLanguage) {
            // 一致した場合、既存のものを削除
            PWON_RES_ENTRY pDelete = pCurr;
            *ppNext = pCurr->next;
            FreeResId(pDelete->type);
            FreeResId(pDelete->name);
            if (pDelete->data)
                HeapFree(GetProcessHeap(), 0, pDelete->data);
            HeapFree(GetProcessHeap(), 0, pDelete);
            break;
        }
        ppNext = &pCurr->next;
    }

    // lpData が NULL でない場合は新規追加・更新
    if (lpData != NULL) {
        PWON_RES_ENTRY pNew =
            (PWON_RES_ENTRY)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(WON_RES_ENTRY));
        if (!pNew)
            return FALSE;

        pNew->type = DuplicateResId(lpType);
        pNew->name = DuplicateResId(lpName);
        pNew->lang = wLanguage;
        pNew->size = cbData;
        pNew->data = HeapAlloc(GetProcessHeap(), 0, cbData);
        CopyMemory(pNew->data, lpData, cbData);

        // リストの先頭に追加
        pNew->next = pUpdate->pEntries;
        pUpdate->pEntries = pNew;
    }

    return TRUE;
}

BOOL WONAPI WonEndUpdateResourceW(HANDLE hUpdate, BOOL fDiscard)
{
    PWON_UPDATE_DATA pUpdate = (PWON_UPDATE_DATA)hUpdate;
    if (!pUpdate)
        return FALSE;

    BOOL ret = TRUE;
    if (!fDiscard)
        ret = WonRealUpdateResource(pUpdate);

    // クリーンアップ
    PWON_RES_ENTRY pCurr = pUpdate->pEntries;
    while (pCurr) {
        PWON_RES_ENTRY pNext = pCurr->next;
        FreeResId(pCurr->type);
        FreeResId(pCurr->name);
        if (pCurr->data)
            HeapFree(GetProcessHeap(), 0, pCurr->data);
        HeapFree(GetProcessHeap(), 0, pCurr);
        pCurr = pNext;
    }
    free(pUpdate->pFileName);
    HeapFree(GetProcessHeap(), 0, pUpdate);
    return ret;
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
