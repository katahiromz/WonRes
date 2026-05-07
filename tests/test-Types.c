#include <windows.h>
#include <stdio.h>
#include "WonRes.h"

BOOL CALLBACK MyEnumProcA(
    HMODULE hModule,
    LPSTR lpType,
    LONG_PTR lParam)
{
    if (IS_INTRESOURCE(lpType))
        wprintf(L"Type ID: %u\n", (UINT)(ULONG_PTR)lpType);
    else
        wprintf(L"Type Name: %hs\n", lpType);

    return TRUE;
}


BOOL CALLBACK MyEnumProcW(
    HMODULE hModule,
    LPWSTR lpType,
    LONG_PTR lParam)
{
    if (IS_INTRESOURCE(lpType))
        wprintf(L"Type ID: %u\n", (UINT)(ULONG_PTR)lpType);
    else
        wprintf(L"Type Name: %ls\n", lpType);

    return TRUE;
}

int main(void)
{
    HMODULE mod = LoadLibraryW(L"user32.dll");
    BOOL ret1 = WonEnumResourceTypesA(mod, MyEnumProcA, 0);
    BOOL ret2 = WonEnumResourceTypesW(mod, MyEnumProcW, 0);
    FreeLibrary(mod);
    return (ret1 && ret2) ? 0 : 1;
}
