#include <windows.h>
#include <stdio.h>
#include "WonRes.h"

BOOL CALLBACK LangEnumProcA(
    HMODULE hModule,
    LPCSTR lpType,
    LPCSTR lpName,
    WORD wLang,
    LONG_PTR lParam)
{
    wprintf(L"    Language: %u\n", wLang);
    return TRUE;
}

BOOL CALLBACK NameEnumProcA(
    HMODULE hModule,
    LPCSTR lpType,
    LPSTR lpName,
    LONG_PTR lParam)
{
    if (IS_INTRESOURCE(lpName))
        wprintf(L"  Name: %u\n", (UINT)(ULONG_PTR)lpName);
    else
        wprintf(L"  Name: %hs\n", lpName);

    return WonEnumResourceLanguagesA(
        hModule,
        lpType,
        lpName,
        LangEnumProcA,
        0);
}

BOOL CALLBACK TypeEnumProcA(
    HMODULE hModule,
    LPSTR lpType,
    LONG_PTR lParam)
{
    if (IS_INTRESOURCE(lpType))
        wprintf(L"Type: %u\n", (UINT)(ULONG_PTR)lpType);
    else
        wprintf(L"Type: %hs\n", lpType);

    return WonEnumResourceNamesA(
        hModule,
        lpType,
        NameEnumProcA,
        0);
}

int main(int argc, char **argv)
{
    HMODULE hModule = LoadLibraryExA(argv[1], NULL, LOAD_LIBRARY_AS_DATAFILE);
    BOOL ret = WonEnumResourceTypesA(hModule, TypeEnumProcA, 0);
    FreeLibrary(hModule);
    return ret ? 0 : 1;
}
