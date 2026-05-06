#include <windows.h>
#include <stdio.h>
#include "WonRes.h"

BOOL CALLBACK MyEnumProc(
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
    WonEnumResourceTypesW(mod, MyEnumProc, 0);
    FreeLibrary(mod);
    return 0;
}
