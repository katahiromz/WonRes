#include <windows.h>
#include <stdio.h>
#include "WonRes.h"

BOOL CALLBACK NameEnumProc(
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

    WonEnumResourceNamesW(
        mod,
        RT_GROUP_ICON,
        NameEnumProc,
        0);

    FreeLibrary(mod);
}
