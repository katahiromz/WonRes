#include <windows.h>
#include <stdio.h>
#include "WonRes.h"

BOOL CALLBACK NameEnumProcA(
    HMODULE hModule,
    LPCSTR lpType,
    LPSTR lpName,
    LONG_PTR lParam)
{
    if (IS_INTRESOURCE(lpName))
        wprintf(L"Name ID: %u\n", (UINT)(ULONG_PTR)lpName);
    else
        wprintf(L"Name Str: %hs\n", lpName);

    return TRUE;
}

BOOL CALLBACK NameEnumProcW(
    HMODULE hModule,
    LPCWSTR lpType,
    LPWSTR lpName,
    LONG_PTR lParam)
{
    if (IS_INTRESOURCE(lpName))
        wprintf(L"Name ID: %u\n", (UINT)(ULONG_PTR)lpName);
    else
        wprintf(L"Name Str: %ls\n", lpName);

    return TRUE;
}

int main(void)
{
    HMODULE mod = LoadLibraryExW(L"user32.dll", NULL, LOAD_LIBRARY_AS_DATAFILE);

    WonEnumResourceNamesA(
        mod,
        (LPCSTR)RT_GROUP_ICON,
        NameEnumProcA,
        0);

    WonEnumResourceNamesW(
        mod,
        RT_GROUP_ICON,
        NameEnumProcW,
        0);

    FreeLibrary(mod);
}
