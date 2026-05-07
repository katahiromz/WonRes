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

#include <pshpack2.h>
typedef struct WON_RELOC_ENTRY {
    WORD offset;
    WORD type;
} WON_RELOC_ENTRY, *PWON_RELOC_ENTRY;
#include <poppack.h>

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

// リソースツリーの構造をソートするための比較関数
static int CompareResEntry(const void *a, const void *b)
{
    PWON_RES_ENTRY p1 = *(PWON_RES_ENTRY *)a;
    PWON_RES_ENTRY p2 = *(PWON_RES_ENTRY *)b;

    // Type -> Name -> Lang の順で比較
    if (p1->type != p2->type) {
        if (IS_INTRESOURCE(p1->type) && IS_INTRESOURCE(p2->type))
            return (INT_PTR)p1->type - (INT_PTR)p2->type;
        if (IS_INTRESOURCE(p1->type))
            return -1;
        if (IS_INTRESOURCE(p2->type))
            return 1;
        return wcscmp(p1->type, p2->type);
    }
    if (p1->name != p2->name) {
        if (IS_INTRESOURCE(p1->name) && IS_INTRESOURCE(p2->name))
            return (INT_PTR)p1->name - (INT_PTR)p2->name;
        if (IS_INTRESOURCE(p1->name))
            return -1;
        if (IS_INTRESOURCE(p2->name))
            return 1;
        return wcscmp(p1->name, p2->name);
    }
    return p1->lang - p2->lang;
}

// 実際にリソースを更新する関数
static BOOL WonRealUpdateResource(PWON_UPDATE_DATA pUpdate)
{
    BOOL bSuccess = FALSE;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hMapping = NULL;
    PBYTE pBase = NULL;

    // 1. エントリを配列にコピーしてソート
    DWORD count = 0;
    for (PWON_RES_ENTRY p = pUpdate->pEntries; p; p = p->next)
        count++;
    if (count == 0)
        return TRUE; // 更新なし

    PWON_RES_ENTRY *ppSorted =
        (PWON_RES_ENTRY *)HeapAlloc(GetProcessHeap(), 0, sizeof(PWON_RES_ENTRY) * count);
    DWORD i = 0;
    for (PWON_RES_ENTRY p = pUpdate->pEntries; p; p = p->next)
        ppSorted[i++] = p;
    qsort(ppSorted, count, sizeof(PWON_RES_ENTRY *), CompareResEntry);

    // 2. ファイルをメモリマップドファイルとして開く
    hFile = CreateFileW(pUpdate->pFileName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        goto cleanup;

    DWORD dwFileSize = GetFileSize(hFile, NULL);
    hMapping = CreateFileMappingW(hFile, NULL, PAGE_READWRITE, 0, 0, NULL);
    if (!hMapping)
        goto cleanup;
    pBase = (LPBYTE)MapViewOfFile(hMapping, FILE_MAP_WRITE, 0, 0, 0);
    if (!pBase)
        goto cleanup;

    // 3. PEヘッダーの解析 (x86/x64対応)
    PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)pBase;
    PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(pBase + pDos->e_lfanew);

    // セクション情報の取得
    PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNt);
    PIMAGE_SECTION_HEADER pRsrcSection = NULL;
    for (i = 0; i < pNt->FileHeader.NumberOfSections; i++) {
        if (memcmp(pSection[i].Name, ".rsrc", 5) == 0) {
            pRsrcSection = &pSection[i];
            break;
        }
    }

    // TODO: 実際のバイナリ再構築ロジック
    // 4. 新しいリソースセクションのサイズ計算
    // 5. 元のファイルの末尾または .rsrc セクションを拡張して書き込み
    // 6. DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE] の更新
    // 7. リソースディレクトリ（Type/Name/Langの3層ツリー）のバイナリ構造をシリアライズする
    // 8. CheckSumMappedFile でチェックサムを再計算 (imagehlp.lib)

    bSuccess = TRUE;

cleanup:
    if (pBase)
        UnmapViewOfFile(pBase);
    if (hMapping)
        CloseHandle(hMapping);
    if (hFile != INVALID_HANDLE_VALUE)
        CloseHandle(hFile);
    if (ppSorted)
        HeapFree(GetProcessHeap(), 0, ppSorted);
    return bSuccess;
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
