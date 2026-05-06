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
    wprintf(L"Language: %u\n", wLang);
    return TRUE;
}

BOOL CALLBACK LangEnumProcW(
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

    WonEnumResourceLanguagesA(
        mod,
        (LPCSTR)RT_GROUP_ICON,
        MAKEINTRESOURCEA(100),
        LangEnumProcA,
        0);

    WonEnumResourceLanguagesW(
        mod,
        RT_GROUP_ICON,
        MAKEINTRESOURCEW(100),
        LangEnumProcW,
        0);

    FreeLibrary(mod);
}
