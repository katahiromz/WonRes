// loader.c --- Win32 resource loader for WonRes
// Author: katahiromz
// License: MIT
#include <windows.h>
#include <imagehlp.h>
#include "WonRes.h"

// Script: C99/Win32でFindResourceExWなどのリソースローダーを再実装してください。LoadLibraryExのLOAD_LIBRARY_AS_DATAFILEにも対応してください。

#define LDR_IS_RESOURCE_HANDLE(h) (((ULONG_PTR)(h) & 3) != 0)
#define LDR_TO_BASE(h) ((PBYTE)((ULONG_PTR)(h) & ~3))

// リソースIDの最大長
#define MAX_RES_ID_LEN 256

// リソースディレクトリのルートを取得
static PIMAGE_RESOURCE_DIRECTORY GetResourceRoot(HMODULE hModule)
{
    ULONG size;
    // ImageDirectoryEntryToData は RVA を解決して実際のアドレスを返す
    // LOAD_LIBRARY_AS_DATAFILE の場合も適切にオフセットを計算する
    return (PIMAGE_RESOURCE_DIRECTORY)ImageDirectoryEntryToData(
        LDR_TO_BASE(hModule),
        LDR_IS_RESOURCE_HANDLE(hModule) ? FALSE : TRUE,
        IMAGE_DIRECTORY_ENTRY_RESOURCE,
        &size);
}

// IDまたは名前でエントリを検索
static PIMAGE_RESOURCE_DIRECTORY_ENTRY FindEntry(PIMAGE_RESOURCE_DIRECTORY pDir, LPCWSTR lpKey)
{
    PIMAGE_RESOURCE_DIRECTORY_ENTRY pEntries = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(pDir + 1);
    int count = pDir->NumberOfNamedEntries + pDir->NumberOfIdEntries;

    for (int i = 0; i < count; i++) {
        if (IS_INTRESOURCE(lpKey)) {
            if (!pEntries[i].NameIsString && pEntries[i].Id == PtrToUshort(lpKey))
                return &pEntries[i];
        } else {
            if (pEntries[i].NameIsString) {
                PBYTE pBase = (PBYTE)GetResourceRoot((HMODULE)LDR_TO_BASE(pDir));
                PIMAGE_RESOURCE_DIR_STRING_U pStr = (PIMAGE_RESOURCE_DIR_STRING_U)(pBase + pEntries[i].NameOffset);
                if (wcsnicmp(lpKey, pStr->NameString, pStr->Length) == 0 &&
                    lpKey[pStr->Length] == UNICODE_NULL)
                {
                    return &pEntries[i];
                }
            }
        }
    }
    return NULL;
}

static LPCWSTR GetResourceIdOrName(PIMAGE_RESOURCE_DIRECTORY_ENTRY pEntry, PBYTE pResourceBase)
{
    if (pEntry->NameIsString)
        return (LPCWSTR)(pResourceBase + pEntry->NameOffset);

    return MAKEINTRESOURCEW(pEntry->Id);
}

////////////////////////////////////////////////////////////////////////////////////
// Find resource

HRSRC WONAPI WonFindResourceExW(HMODULE hModule, LPCWSTR lpType, LPCWSTR lpName, WORD wLanguage)
{
    PIMAGE_RESOURCE_DIRECTORY base = GetResourceRoot(hModule);
    if (!base) return NULL;

    PIMAGE_RESOURCE_DIRECTORY_ENTRY e = FindEntry(base, lpType);
    if (!e || !e->DataIsDirectory) return NULL;

    base = (PIMAGE_RESOURCE_DIRECTORY)((PBYTE)base + e->OffsetToDirectory);
    e = FindEntry(base, lpName);
    if (!e || !e->DataIsDirectory) return NULL;

    base = (PIMAGE_RESOURCE_DIRECTORY)((PBYTE)base + e->OffsetToDirectory);
    e = FindEntry(base, (LPCWSTR)(ULONG_PTR)wLanguage);

    return (HRSRC)e;
}

HRSRC WONAPI WonFindResourceExA(HMODULE hModule, LPCSTR lpType, LPCSTR lpName, WORD wLanguage)
{
    LPCWSTR pszTypeW, pszNameW;
    WCHAR szTypeW[MAX_RES_ID_LEN], szNameW[MAX_RES_ID_LEN];

    if (IS_INTRESOURCE(lpType))
    {
        pszTypeW = (LPCWSTR)lpType;
    }
    else
    {
        if (!MultiByteToWideChar(CP_ACP, 0, lpType, -1, szTypeW, _countof(szTypeW))) return NULL;
        pszTypeW = szTypeW;
    }

    if (IS_INTRESOURCE(lpName))
    {
        pszNameW = (LPCWSTR)lpName;
    }
    else
    {
        if (!MultiByteToWideChar(CP_ACP, 0, lpName, -1, szNameW, _countof(szNameW))) return NULL;
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
    if (!hrsrc) return 0;
    PIMAGE_RESOURCE_DATA_ENTRY pData = (PIMAGE_RESOURCE_DATA_ENTRY)
        ((PBYTE)GetResourceRoot(hModule) + ((PIMAGE_RESOURCE_DIRECTORY_ENTRY)hrsrc)->OffsetToData);
    return pData->Size;
}

////////////////////////////////////////////////////////////////////////////////////
// Load resource

HGLOBAL WONAPI WonLoadResource(HMODULE hModule, HRSRC hrsrc)
{
    if (!hrsrc) return NULL;
    PIMAGE_RESOURCE_DATA_ENTRY pData = (PIMAGE_RESOURCE_DATA_ENTRY)
        ((PBYTE)GetResourceRoot(hModule) + ((PIMAGE_RESOURCE_DIRECTORY_ENTRY)hrsrc)->OffsetToData);

    return (HGLOBAL)(LDR_TO_BASE(hModule) + pData->OffsetToData);
}

////////////////////////////////////////////////////////////////////////////////////
// Lock resource

LPVOID WONAPI WonLockResource(HMODULE hModule, HGLOBAL hResData)
{
    return (LPVOID)hResData;
}

////////////////////////////////////////////////////////////////////////////////////
// Enum resource

BOOL WONAPI WonEnumResourceTypesW(HMODULE hModule, ENUMRESTYPEPROCW lpEnumFunc, LONG_PTR lParam)
{
    PIMAGE_RESOURCE_DIRECTORY root = GetResourceRoot(hModule);
    if (!root) return FALSE;

    PBYTE pBase = (PBYTE)root;
    PIMAGE_RESOURCE_DIRECTORY_ENTRY pEntries = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(root + 1);
    for (WORD i = 0; i < (root->NumberOfNamedEntries + root->NumberOfIdEntries); i++) {
        LPWSTR type;
        if (pEntries[i].NameIsString) {
            PIMAGE_RESOURCE_DIR_STRING_U pStr = (PIMAGE_RESOURCE_DIR_STRING_U)(pBase + pEntries[i].NameOffset);

            // 本来はNULL終端されていない可能性があるため、バッファにコピーして終端させる必要があります
            WCHAR szName[MAX_RES_ID_LEN];
            int len = (int)min((size_t)pStr->Length, _countof(szName) - 1);
            memcpy(szName, pStr->NameString, len * sizeof(WCHAR));
            szName[len] = UNICODE_NULL;

            type = szName;
        } else {
            type = MAKEINTRESOURCEW(pEntries[i].Id);
        }

        if (!lpEnumFunc(hModule, type, lParam)) break;
    }
    return TRUE;
}

BOOL WONAPI WonEnumResourceNamesW(
    HMODULE hModule,
    LPCWSTR lpType,
    ENUMRESNAMEPROCW lpEnumFunc,
    LONG_PTR lParam)
{
    PIMAGE_RESOURCE_DIRECTORY pRootDir = GetResourceRoot(hModule);
    if (!pRootDir) return FALSE;

    PBYTE pBase = (PBYTE)pRootDir;

    PIMAGE_RESOURCE_DIRECTORY_ENTRY pTypeEntry = FindEntry(pRootDir, lpType);
    if (!pTypeEntry || !pTypeEntry->DataIsDirectory) return FALSE;

    PIMAGE_RESOURCE_DIRECTORY pNameDir = (PIMAGE_RESOURCE_DIRECTORY)(pBase + pTypeEntry->OffsetToDirectory);
    PIMAGE_RESOURCE_DIRECTORY_ENTRY pNameEntries = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(pNameDir + 1);

    DWORD totalCount = pNameDir->NumberOfNamedEntries + pNameDir->NumberOfIdEntries;

    for (DWORD i = 0; i < totalCount; i++)
    {
        LPCWSTR resName = GetResourceIdOrName(&pNameEntries[i], pBase);

        if (!lpEnumFunc(hModule, lpType, (LPWSTR)resName, lParam))
            return FALSE;
    }

    return TRUE;
}

BOOL WONAPI WonEnumResourceLanguagesW(
    HMODULE hModule,
    LPCWSTR lpType,
    LPCWSTR lpName,
    ENUMRESLANGPROCW lpEnumFunc,
    LONG_PTR lParam)
{
    PIMAGE_RESOURCE_DIRECTORY pRootDir = GetResourceRoot(hModule);
    if (!pRootDir) return FALSE;

    PBYTE pBase = (PBYTE)pRootDir;

    PIMAGE_RESOURCE_DIRECTORY_ENTRY pTypeEntry = FindEntry(pRootDir, lpType);
    if (!pTypeEntry || !pTypeEntry->DataIsDirectory) return FALSE;

    PIMAGE_RESOURCE_DIRECTORY pNameDir = (PIMAGE_RESOURCE_DIRECTORY)(pBase + pTypeEntry->OffsetToDirectory);
    PIMAGE_RESOURCE_DIRECTORY_ENTRY pNameEntry = FindEntry(pNameDir, lpName);
    if (!pNameEntry || !pNameEntry->DataIsDirectory) return FALSE;

    PIMAGE_RESOURCE_DIRECTORY pLangDir = (PIMAGE_RESOURCE_DIRECTORY)(pBase + pNameEntry->OffsetToDirectory);
    PIMAGE_RESOURCE_DIRECTORY_ENTRY pLangEntries = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(pLangDir + 1);

    DWORD totalCount = pLangDir->NumberOfNamedEntries + pLangDir->NumberOfIdEntries;
    for (DWORD i = 0; i < totalCount; i++)
    {
        WORD wLang = pLangEntries[i].Id;

        if (!lpEnumFunc(hModule, lpType, lpName, wLang, lParam))
            return FALSE;
    }

    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////
// WonEnum* ANSI version

typedef struct tagENUM_W2A_DATA
{
    LPARAM lParam;
    union {
        ENUMRESTYPEPROCA fnTypeProcA;
        ENUMRESNAMEPROCA fnNameProcA;
        ENUMRESLANGPROCA fnLangProcA;
    };
} ENUM_W2A_DATA, *PENUM_W2A_DATA;

static BOOL CALLBACK
WonEnumTypeA2WProc(HMODULE hModule, LPWSTR lpszType, LONG_PTR lParam)
{
    PENUM_W2A_DATA pData = (PENUM_W2A_DATA)lParam;
    CHAR szTypeA[MAX_RES_ID_LEN];
    LPSTR pszTypeA;

    if (IS_INTRESOURCE(lpszType))
    {
        pszTypeA = MAKEINTRESOURCEA(PtrToUshort(lpszType));
    }
    else
    {
        if (!WideCharToMultiByte(CP_ACP, 0, lpszType, -1, szTypeA, _countof(szTypeA), NULL, NULL)) return FALSE;
        pszTypeA = szTypeA;
    }

    return pData->fnTypeProcA(hModule, pszTypeA, pData->lParam);
}

static BOOL CALLBACK
WonEnumNameA2WProc(HMODULE hModule, LPCWSTR lpszType, LPWSTR lpszName, LONG_PTR lParam)
{
    PENUM_W2A_DATA pData = (PENUM_W2A_DATA)lParam;
    CHAR szTypeA[MAX_RES_ID_LEN], szNameA[MAX_RES_ID_LEN];
    LPCSTR pszTypeA;
    LPSTR pszNameA;

    if (IS_INTRESOURCE(lpszType))
    {
        pszTypeA = MAKEINTRESOURCEA(PtrToUshort(lpszType));
    }
    else
    {
        if (!WideCharToMultiByte(CP_ACP, 0, lpszType, -1, szTypeA, _countof(szTypeA), NULL, NULL)) return FALSE;
        pszTypeA = szTypeA;
    }

    if (IS_INTRESOURCE(lpszName))
    {
        pszNameA = MAKEINTRESOURCEA(PtrToUshort(lpszName));
    }
    else
    {
        if (!WideCharToMultiByte(CP_ACP, 0, lpszName, -1, szNameA, _countof(szNameA), NULL, NULL)) return FALSE;
        pszNameA = szNameA;
    }

    return pData->fnNameProcA(hModule, pszTypeA, pszNameA, pData->lParam);
}

static BOOL CALLBACK
WonEnumLangA2WProc(
    HMODULE hModule,
    LPCWSTR lpszType,
    LPCWSTR lpszName,
    WORD wIDLanguage,
    LONG_PTR lParam)
{
    PENUM_W2A_DATA pData = (PENUM_W2A_DATA)lParam;
    CHAR szTypeA[MAX_RES_ID_LEN], szNameA[MAX_RES_ID_LEN];
    LPCSTR pszTypeA, pszNameA;

    if (IS_INTRESOURCE(lpszType))
    {
        pszTypeA = MAKEINTRESOURCEA(PtrToUshort(lpszType));
    }
    else
    {
        if (!WideCharToMultiByte(CP_ACP, 0, lpszType, -1, szTypeA, _countof(szTypeA), NULL, NULL)) return FALSE;
        pszTypeA = szTypeA;
    }

    if (IS_INTRESOURCE(lpszName))
    {
        pszNameA = MAKEINTRESOURCEA(PtrToUshort(lpszName));
    }
    else
    {
        if (!WideCharToMultiByte(CP_ACP, 0, lpszName, -1, szNameA, _countof(szNameA), NULL, NULL)) return FALSE;
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

BOOL WONAPI WonEnumResourceNamesA(
    HMODULE hModule,
    LPCSTR lpType,
    ENUMRESNAMEPROCA lpEnumFunc,
    LONG_PTR lParam)
{
    ENUM_W2A_DATA data;
    data.lParam = lParam;
    data.fnNameProcA = lpEnumFunc;

    WCHAR szTypeW[MAX_RES_ID_LEN];
    LPCWSTR pszTypeW;

    if (IS_INTRESOURCE(lpType))
    {
        pszTypeW = MAKEINTRESOURCEW(PtrToUshort(lpType));
    }
    else
    {
        if (!MultiByteToWideChar(CP_ACP, 0, lpType, -1, szTypeW, _countof(szTypeW))) return FALSE;
        pszTypeW = szTypeW;
    }

    return WonEnumResourceNamesW(hModule, pszTypeW, WonEnumNameA2WProc, (LPARAM)&data);
}

BOOL WONAPI WonEnumResourceLanguagesA(
    HMODULE hModule,
    LPCSTR lpType,
    LPCSTR lpName,
    ENUMRESLANGPROCA lpEnumFunc,
    LONG_PTR lParam)
{
    ENUM_W2A_DATA data;
    data.lParam = lParam;
    data.fnLangProcA = lpEnumFunc;

    WCHAR szTypeW[MAX_RES_ID_LEN], szNameW[MAX_RES_ID_LEN];
    LPCWSTR pszTypeW, pszNameW;

    if (IS_INTRESOURCE(lpType))
    {
        pszTypeW = MAKEINTRESOURCEW(PtrToUshort(lpType));
    }
    else
    {
        if (!MultiByteToWideChar(CP_ACP, 0, lpType, -1, szTypeW, _countof(szTypeW))) return FALSE;
        pszTypeW = szTypeW;
    }

    if (IS_INTRESOURCE(lpName))
    {
        pszNameW = MAKEINTRESOURCEW(PtrToUshort(lpName));
    }
    else
    {
        if (!MultiByteToWideChar(CP_ACP, 0, lpName, -1, szNameW, _countof(szNameW))) return FALSE;
        pszNameW = szNameW;
    }

    return WonEnumResourceLanguagesW(hModule, pszTypeW, pszNameW, WonEnumLangA2WProc, (LPARAM)&data);
}
