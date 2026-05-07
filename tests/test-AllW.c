#include <windows.h>
#include <stdio.h>
#include "WonRes.h"

BOOL CALLBACK LangEnumProcW(
    HMODULE hModule,
    LPCWSTR lpType,
    LPCWSTR lpName,
    WORD wLang,
    LONG_PTR lParam)
{
    wprintf(L"    Language: %u\n", wLang);
    return TRUE;
}

BOOL CALLBACK NameEnumProcW(
    HMODULE hModule,
    LPCWSTR lpType,
    LPWSTR lpName,
    LONG_PTR lParam)
{
    if (IS_INTRESOURCE(lpName))
        wprintf(L"  Name: %u\n", (UINT)(ULONG_PTR)lpName);
    else
        wprintf(L"  Name: %ls\n", lpName);

    return WonEnumResourceLanguagesW(
        hModule,
        lpType,
        lpName,
        LangEnumProcW,
        0);
}

BOOL CALLBACK TypeEnumProcW(
    HMODULE hModule,
    LPWSTR lpType,
    LONG_PTR lParam)
{
    if (IS_INTRESOURCE(lpType))
        wprintf(L"Type: %u\n", (UINT)(ULONG_PTR)lpType);
    else
        wprintf(L"Type: %ls\n", lpType);

    return WonEnumResourceNamesW(
        hModule,
        lpType,
        NameEnumProcW,
        0);
}

int main(int argc, char **argv)
{
    HMODULE hModule = LoadLibraryExA(argv[1], NULL, LOAD_LIBRARY_AS_DATAFILE);
    BOOL ret = WonEnumResourceTypesW(hModule, TypeEnumProcW, 0);
    FreeLibrary(hModule);
    return ret ? 0 : 1;
}
