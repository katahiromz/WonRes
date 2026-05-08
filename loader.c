// loader.c --- Win32 resource loader for WonRes
// Author: katahiromz
// License: MIT
#include <windows.h>
#include <imagehlp.h>
#include <assert.h>
#include "WonRes.h"

// Script:
// C99/Win32でFindResourceExWなどのリソースローダーを再実装してください。LoadLibraryExのLOAD_LIBRARY_AS_DATAFILEにも対応してください。
// 実行モジュール・読み込みモジュールについてx86/x64両方に対応して下さい。
// x86からx64の読み込み、x64からx86の読み込みにも対応してください。

#define LDR_IS_RESOURCE_HANDLE(h) (((ULONG_PTR)(h) & 3) != 0)
#define LDR_TO_BASE(h) ((PBYTE)((ULONG_PTR)(h) & ~3))

#define LDR_DIR_OFFSET(e) ((e).OffsetToDirectory & 0x7FFFFFFF)
#define LDR_DATA_OFFSET(e) ((e).OffsetToData & 0x7FFFFFFF)
#define LDR_NAME_OFFSET(e) ((e).NameOffset & 0x7FFFFFFF)

// リソースディレクトリのルートを取得
static PIMAGE_RESOURCE_DIRECTORY GetResourceRoot(HMODULE hModule)
{
    ULONG size;
    BOOL MappedAsImage = !LDR_IS_RESOURCE_HANDLE(hModule);
    return (PIMAGE_RESOURCE_DIRECTORY)ImageDirectoryEntryToData(
        LDR_TO_BASE(hModule), MappedAsImage, IMAGE_DIRECTORY_ENTRY_RESOURCE, &size);
}

// IDまたは名前でエントリを検索
static PIMAGE_RESOURCE_DIRECTORY_ENTRY FindEntry(PIMAGE_RESOURCE_DIRECTORY pRoot,
                                                 PIMAGE_RESOURCE_DIRECTORY pDir, LPCWSTR lpKey)
{
    PIMAGE_RESOURCE_DIRECTORY_ENTRY pEntries = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(pDir + 1);
    int low = 0, high;

    if (IS_INTRESOURCE(lpKey)) {
        // IDエントリの二分探索
        WORD id = PtrToUshort(lpKey);
        low = pDir->NumberOfNamedEntries;
        high = low + pDir->NumberOfIdEntries - 1;
        while (low <= high) {
            int mid = (low + high) / 2;
            if (pEntries[mid].Id == id)
                return &pEntries[mid];
            if (pEntries[mid].Id < id)
                low = mid + 1;
            else
                high = mid - 1;
        }
    } else {
        // 名前エントリの二分探索
        PBYTE pBase = (PBYTE)pRoot;
        size_t keyLen = wcslen(lpKey);
        low = 0;
        high = pDir->NumberOfNamedEntries - 1;
        while (low <= high) {
            int mid = (low + high) / 2;
            PIMAGE_RESOURCE_DIR_STRING_U pStr =
                (PIMAGE_RESOURCE_DIR_STRING_U)(pBase + LDR_NAME_OFFSET(pEntries[mid]));

            // 長さの差を優先して比較し、同じ長さなら中身を比較
            int res = _wcsnicmp(lpKey, pStr->NameString, min(keyLen, (size_t)pStr->Length));
            if (res == 0) {
                if (keyLen == pStr->Length)
                    return &pEntries[mid];
                res = (keyLen < pStr->Length) ? -1 : 1;
            }

            if (res > 0)
                low = mid + 1;
            else
                high = mid - 1;
        }
    }
    return NULL;
}

////////////////////////////////////////////////////////////////////////////////////
// Find resource

HRSRC WONAPI WonFindResourceExW(HMODULE hModule, LPCWSTR lpType, LPCWSTR lpName, WORD wLanguage)
{
    LANGID aLangIds[16];
    INT iLangId, cLangIds = 0;

    PIMAGE_RESOURCE_DIRECTORY root = GetResourceRoot(hModule);
    if (!root) {
        SetLastError(ERROR_RESOURCE_DATA_NOT_FOUND);
        return NULL;
    }

    // Level 1: Type
    PIMAGE_RESOURCE_DIRECTORY_ENTRY e = FindEntry(root, root, lpType);
    if (!e || !e->DataIsDirectory) {
        SetLastError(ERROR_RESOURCE_TYPE_NOT_FOUND);
        return NULL;
    }

    // Level 2: Name
    PIMAGE_RESOURCE_DIRECTORY dir = (PIMAGE_RESOURCE_DIRECTORY)((PBYTE)root + LDR_DIR_OFFSET(*e));
    e = FindEntry(root, dir, lpName);
    if (!e || !e->DataIsDirectory) {
        SetLastError(ERROR_RESOURCE_NAME_NOT_FOUND);
        return NULL;
    }

    // Level 3: Language
    aLangIds[cLangIds++] = wLanguage;
    aLangIds[cLangIds++] = MAKELANGID(PRIMARYLANGID(wLanguage), SUBLANG_NEUTRAL);
    aLangIds[cLangIds++] = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
    if (PRIMARYLANGID(wLanguage) == LANG_NEUTRAL) {
        LANGID wUserLangId = GetUserDefaultLangID();
        LANGID wSysLangId = GetSystemDefaultLangID();
        if (SUBLANGID(wLanguage) != SUBLANG_SYS_DEFAULT) {
            aLangIds[cLangIds++] = LANGIDFROMLCID(GetThreadLocale());
            aLangIds[cLangIds++] = wUserLangId;
            aLangIds[cLangIds++] = MAKELANGID(PRIMARYLANGID(wUserLangId), SUBLANG_NEUTRAL);
        }
        aLangIds[cLangIds++] = wSysLangId;
        aLangIds[cLangIds++] = MAKELANGID(PRIMARYLANGID(wSysLangId), SUBLANG_NEUTRAL);
        aLangIds[cLangIds++] = MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT);
    }
    assert(cLangIds < _countof(aLangIds));

    dir = (PIMAGE_RESOURCE_DIRECTORY)((PBYTE)root + LDR_DIR_OFFSET(*e));
    for (iLangId = 0; iLangId < cLangIds; ++iLangId) {
        e = FindEntry(root, dir, (LPCWSTR)UlongToPtr(aLangIds[iLangId]));
        if (e)
            return (HRSRC)(ULONG_PTR)e;
    }

    PIMAGE_RESOURCE_DIRECTORY_ENTRY pEntries = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(dir + 1);
    if (dir->NumberOfIdEntries == 0)
        return NULL;
    return (HRSRC)(ULONG_PTR)&pEntries[0];
}

HRSRC WONAPI WonFindResourceExA(HMODULE hModule, LPCSTR lpType, LPCSTR lpName, WORD wLanguage)
{
    LPCWSTR pszTypeW, pszNameW;
    WCHAR szTypeW[MAX_RES_ID_LEN], szNameW[MAX_RES_ID_LEN];

    if (IS_INTRESOURCE(lpType)) {
        pszTypeW = (LPCWSTR)lpType;
    } else {
        if (!MultiByteToWideChar(CP_ACP, 0, lpType, -1, szTypeW, _countof(szTypeW)))
            return NULL;
        szTypeW[_countof(szTypeW) - 1] = UNICODE_NULL;
        pszTypeW = szTypeW;
    }

    if (IS_INTRESOURCE(lpName)) {
        pszNameW = (LPCWSTR)lpName;
    } else {
        if (!MultiByteToWideChar(CP_ACP, 0, lpName, -1, szNameW, _countof(szNameW)))
            return NULL;
        szNameW[_countof(szNameW) - 1] = UNICODE_NULL;
        pszNameW = szNameW;
    }

    return WonFindResourceExW(hModule, pszTypeW, pszNameW, wLanguage);
}

HRSRC WONAPI WonFindResourceA(HMODULE hModule, LPCSTR lpType, LPCSTR lpName)
{
    return WonFindResourceExA(hModule, lpType, lpName, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL));
}

HRSRC WONAPI WonFindResourceW(HMODULE hModule, LPCWSTR lpType, LPCWSTR lpName)
{
    return WonFindResourceExW(hModule, lpType, lpName, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL));
}

////////////////////////////////////////////////////////////////////////////////////
// Size of resource

DWORD WONAPI WonSizeofResource(HMODULE hModule, HRSRC hrsrc)
{
    if (!hrsrc)
        return 0;
    PIMAGE_RESOURCE_DATA_ENTRY pData =
        (PIMAGE_RESOURCE_DATA_ENTRY)((PBYTE)GetResourceRoot(hModule) +
                                     LDR_DATA_OFFSET(*(PIMAGE_RESOURCE_DIRECTORY_ENTRY)hrsrc));
    return pData->Size;
}

////////////////////////////////////////////////////////////////////////////////////
// Load resource

HGLOBAL WONAPI WonLoadResource(HMODULE hModule, HRSRC hrsrc)
{
    if (!hrsrc)
        return NULL;

    PBYTE pBase = LDR_TO_BASE(hModule);
    PIMAGE_RESOURCE_DATA_ENTRY pData =
        (PIMAGE_RESOURCE_DATA_ENTRY)((PBYTE)GetResourceRoot(hModule) +
                                     LDR_DATA_OFFSET(*(PIMAGE_RESOURCE_DIRECTORY_ENTRY)hrsrc));

    if (LDR_IS_RESOURCE_HANDLE(hModule)) {
        // LOAD_LIBRARY_AS_DATAFILE
        // の場合、RVAをファイルオフセットベースのVAに変換
        PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)ImageNtHeader(pBase);
        if (!pNt)
            return NULL;

        PIMAGE_SECTION_HEADER pSection = NULL;
        return (HGLOBAL)ImageRvaToVa(pNt, pBase, pData->OffsetToData, &pSection);
    }

    // 通常のロード（イメージとしてマップされている）場合は RVA を足すだけ
    return (HGLOBAL)(pBase + pData->OffsetToData);
}

////////////////////////////////////////////////////////////////////////////////////
// Lock resource

LPVOID WONAPI WonLockResource(HGLOBAL hResData) { return (LPVOID)hResData; }

////////////////////////////////////////////////////////////////////////////////////
// Enum resource

BOOL WONAPI WonEnumResourceTypesW(HMODULE hModule, ENUMRESTYPEPROCW lpEnumFunc, LONG_PTR lParam)
{
    if (!hModule)
        return FALSE;
    PIMAGE_RESOURCE_DIRECTORY root = GetResourceRoot(hModule);
    if (!root)
        return FALSE;

    PBYTE pBase = (PBYTE)root;
    PIMAGE_RESOURCE_DIRECTORY_ENTRY pEntries = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(root + 1);
    for (INT i = 0; i < (root->NumberOfNamedEntries + root->NumberOfIdEntries); ++i) {
        LPWSTR type;
        if (pEntries[i].NameIsString) {
            PIMAGE_RESOURCE_DIR_STRING_U pStr =
                (PIMAGE_RESOURCE_DIR_STRING_U)(pBase + LDR_NAME_OFFSET(pEntries[i]));

            // 本来はNULL終端されていない可能性があるため、バッファにコピーして終端させる必要があります
            WCHAR szName[MAX_RES_ID_LEN];
            int len = (int)min((size_t)pStr->Length, _countof(szName) - 1);
            memcpy(szName, pStr->NameString, len * sizeof(WCHAR));
            szName[len] = UNICODE_NULL;

            type = szName;
        } else {
            type = MAKEINTRESOURCEW(pEntries[i].Id);
        }

        if (!lpEnumFunc(hModule, type, lParam))
            return FALSE;
    }
    return TRUE;
}

BOOL WONAPI WonEnumResourceNamesW(HMODULE hModule, LPCWSTR lpType, ENUMRESNAMEPROCW lpEnumFunc,
                                  LONG_PTR lParam)
{
    if (!hModule)
        return FALSE;
    PIMAGE_RESOURCE_DIRECTORY pRootDir = GetResourceRoot(hModule);
    if (!pRootDir)
        return FALSE;

    PBYTE pBase = (PBYTE)pRootDir;

    PIMAGE_RESOURCE_DIRECTORY_ENTRY pTypeEntry = FindEntry(pRootDir, pRootDir, lpType);
    if (!pTypeEntry || !pTypeEntry->DataIsDirectory)
        return FALSE;

    PIMAGE_RESOURCE_DIRECTORY pNameDir =
        (PIMAGE_RESOURCE_DIRECTORY)(pBase + LDR_DIR_OFFSET(*pTypeEntry));
    PIMAGE_RESOURCE_DIRECTORY_ENTRY pNameEntries = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(pNameDir + 1);

    DWORD totalCount = pNameDir->NumberOfNamedEntries + pNameDir->NumberOfIdEntries;

    for (DWORD i = 0; i < totalCount; i++) {
        WCHAR szName[MAX_RES_ID_LEN];
        LPWSTR resName;

        if (pNameEntries[i].NameIsString) {
            PIMAGE_RESOURCE_DIR_STRING_U pStr =
                (PIMAGE_RESOURCE_DIR_STRING_U)(pBase + LDR_NAME_OFFSET(pNameEntries[i]));
            int len = (int)min((size_t)pStr->Length, _countof(szName) - 1);
            memcpy(szName, pStr->NameString, len * sizeof(WCHAR));
            szName[len] = UNICODE_NULL;
            resName = szName;
        } else {
            resName = MAKEINTRESOURCEW(pNameEntries[i].Id);
        }

        if (!lpEnumFunc(hModule, lpType, resName, lParam))
            return FALSE;
    }

    return TRUE;
}

BOOL WONAPI WonEnumResourceLanguagesW(HMODULE hModule, LPCWSTR lpType, LPCWSTR lpName,
                                      ENUMRESLANGPROCW lpEnumFunc, LONG_PTR lParam)
{
    if (!hModule)
        return FALSE;
    PIMAGE_RESOURCE_DIRECTORY pRootDir = GetResourceRoot(hModule);
    if (!pRootDir)
        return FALSE;

    PBYTE pBase = (PBYTE)pRootDir;

    PIMAGE_RESOURCE_DIRECTORY_ENTRY pTypeEntry = FindEntry(pRootDir, pRootDir, lpType);
    if (!pTypeEntry || !pTypeEntry->DataIsDirectory)
        return FALSE;

    PIMAGE_RESOURCE_DIRECTORY pNameDir =
        (PIMAGE_RESOURCE_DIRECTORY)(pBase + LDR_DIR_OFFSET(*pTypeEntry));
    PIMAGE_RESOURCE_DIRECTORY_ENTRY pNameEntry = FindEntry(pRootDir, pNameDir, lpName);
    if (!pNameEntry || !pNameEntry->DataIsDirectory)
        return FALSE;

    PIMAGE_RESOURCE_DIRECTORY pLangDir =
        (PIMAGE_RESOURCE_DIRECTORY)(pBase + LDR_DIR_OFFSET(*pNameEntry));
    PIMAGE_RESOURCE_DIRECTORY_ENTRY pLangEntries = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(pLangDir + 1);

    DWORD totalCount = pLangDir->NumberOfNamedEntries + pLangDir->NumberOfIdEntries;
    for (DWORD i = 0; i < totalCount; i++) {
        WORD wLang = pLangEntries[i].Id;

        if (!lpEnumFunc(hModule, lpType, lpName, wLang, lParam))
            return FALSE;
    }

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////
// WonEnum* ANSI version

typedef struct tagENUM_W2A_DATA {
    LPARAM lParam;
    union {
        ENUMRESTYPEPROCA fnTypeProcA;
        ENUMRESNAMEPROCA fnNameProcA;
        ENUMRESLANGPROCA fnLangProcA;
    };
} ENUM_W2A_DATA, *PENUM_W2A_DATA;

static BOOL CALLBACK WonEnumTypeA2WProc(HMODULE hModule, LPWSTR lpszType, LONG_PTR lParam)
{
    PENUM_W2A_DATA pData = (PENUM_W2A_DATA)lParam;
    CHAR szTypeA[MAX_RES_ID_LEN];
    LPSTR pszTypeA;

    if (IS_INTRESOURCE(lpszType)) {
        pszTypeA = MAKEINTRESOURCEA(PtrToUshort(lpszType));
    } else {
        if (!WideCharToMultiByte(CP_ACP, 0, lpszType, -1, szTypeA, _countof(szTypeA), NULL, NULL))
            return FALSE;
        szTypeA[_countof(szTypeA) - 1] = ANSI_NULL;
        pszTypeA = szTypeA;
    }

    return pData->fnTypeProcA(hModule, pszTypeA, pData->lParam);
}

static BOOL CALLBACK WonEnumNameA2WProc(HMODULE hModule, LPCWSTR lpszType, LPWSTR lpszName,
                                        LONG_PTR lParam)
{
    PENUM_W2A_DATA pData = (PENUM_W2A_DATA)lParam;
    CHAR szTypeA[MAX_RES_ID_LEN], szNameA[MAX_RES_ID_LEN];
    LPCSTR pszTypeA;
    LPSTR pszNameA;

    if (IS_INTRESOURCE(lpszType)) {
        pszTypeA = MAKEINTRESOURCEA(PtrToUshort(lpszType));
    } else {
        if (!WideCharToMultiByte(CP_ACP, 0, lpszType, -1, szTypeA, _countof(szTypeA), NULL, NULL))
            return FALSE;
        szTypeA[_countof(szTypeA) - 1] = ANSI_NULL;
        pszTypeA = szTypeA;
    }

    if (IS_INTRESOURCE(lpszName)) {
        pszNameA = MAKEINTRESOURCEA(PtrToUshort(lpszName));
    } else {
        if (!WideCharToMultiByte(CP_ACP, 0, lpszName, -1, szNameA, _countof(szNameA), NULL, NULL))
            return FALSE;
        szNameA[_countof(szNameA) - 1] = ANSI_NULL;
        pszNameA = szNameA;
    }

    return pData->fnNameProcA(hModule, pszTypeA, pszNameA, pData->lParam);
}

static BOOL CALLBACK WonEnumLangA2WProc(HMODULE hModule, LPCWSTR lpszType, LPCWSTR lpszName,
                                        WORD wIDLanguage, LONG_PTR lParam)
{
    PENUM_W2A_DATA pData = (PENUM_W2A_DATA)lParam;
    CHAR szTypeA[MAX_RES_ID_LEN], szNameA[MAX_RES_ID_LEN];
    LPCSTR pszTypeA, pszNameA;

    if (IS_INTRESOURCE(lpszType)) {
        pszTypeA = MAKEINTRESOURCEA(PtrToUshort(lpszType));
    } else {
        if (!WideCharToMultiByte(CP_ACP, 0, lpszType, -1, szTypeA, _countof(szTypeA), NULL, NULL))
            return FALSE;
        szTypeA[_countof(szTypeA) - 1] = ANSI_NULL;
        pszTypeA = szTypeA;
    }

    if (IS_INTRESOURCE(lpszName)) {
        pszNameA = MAKEINTRESOURCEA(PtrToUshort(lpszName));
    } else {
        if (!WideCharToMultiByte(CP_ACP, 0, lpszName, -1, szNameA, _countof(szNameA), NULL, NULL))
            return FALSE;
        szNameA[_countof(szNameA) - 1] = ANSI_NULL;
        pszNameA = szNameA;
    }

    return pData->fnLangProcA(hModule, pszTypeA, pszNameA, wIDLanguage, pData->lParam);
}

BOOL WONAPI WonEnumResourceTypesA(HMODULE hModule, ENUMRESTYPEPROCA lpEnumFunc, LONG_PTR lParam)
{
    ENUM_W2A_DATA data;
    data.lParam = lParam;
    data.fnTypeProcA = lpEnumFunc;

    return WonEnumResourceTypesW(hModule, WonEnumTypeA2WProc, (LPARAM)&data);
}

BOOL WONAPI WonEnumResourceNamesA(HMODULE hModule, LPCSTR lpType, ENUMRESNAMEPROCA lpEnumFunc,
                                  LONG_PTR lParam)
{
    ENUM_W2A_DATA data;
    data.lParam = lParam;
    data.fnNameProcA = lpEnumFunc;

    WCHAR szTypeW[MAX_RES_ID_LEN];
    LPCWSTR pszTypeW;

    if (IS_INTRESOURCE(lpType)) {
        pszTypeW = MAKEINTRESOURCEW(PtrToUshort(lpType));
    } else {
        if (!MultiByteToWideChar(CP_ACP, 0, lpType, -1, szTypeW, _countof(szTypeW)))
            return FALSE;
        szTypeW[_countof(szTypeW) - 1] = UNICODE_NULL;
        pszTypeW = szTypeW;
    }

    return WonEnumResourceNamesW(hModule, pszTypeW, WonEnumNameA2WProc, (LPARAM)&data);
}

BOOL WONAPI WonEnumResourceLanguagesA(HMODULE hModule, LPCSTR lpType, LPCSTR lpName,
                                      ENUMRESLANGPROCA lpEnumFunc, LONG_PTR lParam)
{
    ENUM_W2A_DATA data;
    data.lParam = lParam;
    data.fnLangProcA = lpEnumFunc;

    WCHAR szTypeW[MAX_RES_ID_LEN], szNameW[MAX_RES_ID_LEN];
    LPCWSTR pszTypeW, pszNameW;

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

    return WonEnumResourceLanguagesW(hModule, pszTypeW, pszNameW, WonEnumLangA2WProc,
                                     (LPARAM)&data);
}
