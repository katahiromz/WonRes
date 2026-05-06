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
    if (IS_INTRESOURCE(lpType))
        wprintf(L"Type: %d\n", LOWORD(lpType));
    else
        wprintf(L"Type: %hs\n", lpType);

    if (IS_INTRESOURCE(lpName))
        wprintf(L"Name: %d\n", LOWORD(lpName));
    else
        wprintf(L"Name: %hs\n", lpName);

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
    if (IS_INTRESOURCE(lpType))
        wprintf(L"Type: %d\n", LOWORD(lpType));
    else
        wprintf(L"Type: %ls\n", lpType);

    if (IS_INTRESOURCE(lpName))
        wprintf(L"Name: %d\n", LOWORD(lpName));
    else
        wprintf(L"Name: %ls\n", lpName);

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
