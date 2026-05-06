#include <windows.h>
#include <stdio.h>
#include "WonRes.h"

BOOL CALLBACK LangEnumProc(
    HMODULE hModule,
    LPCWSTR lpType,
    LPCWSTR lpName,
    WORD wLang,
    LONG_PTR lParam)
{
    wprintf(L"Language: %u\n", wLang);
    return TRUE;
}

int main(void)
{
    HMODULE mod = LoadLibraryW(L"user32.dll");

    WonEnumResourceLanguagesW(
        mod,
        RT_GROUP_ICON,
        MAKEINTRESOURCEW(100),
        LangEnumProc,
        0);

    FreeLibrary(mod);
}
