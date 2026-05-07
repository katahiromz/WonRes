#include <windows.h>
#include <stdio.h>
#include "WonRes.h"

int main(void)
{
    HANDLE hUpdate = WonBeginUpdateResourceW(L"test-Langs.exe", FALSE);
    printf("hUpdate: %p\n", hUpdate);
    char sz[] = "This is a test";
    BOOL ret;
    ret = WonUpdateResourceW(hUpdate, (LPWSTR)RT_RCDATA, L"Test", 0, sz, (DWORD)sizeof(sz));
    printf("ret: %d\n", ret);
    ret = WonEndUpdateResourceW(hUpdate, FALSE);
    printf("ret: %d\n", ret);

    HINSTANCE mod = LoadLibraryExW(L"test-Langs.exe", NULL, LOAD_LIBRARY_AS_DATAFILE);
    HRSRC hRsrc = FindResourceExW(mod, (LPCWSTR)RT_RCDATA, L"Test", 0);
    FreeLibrary(mod);
    printf("hRsrc: %p\n", hRsrc);

    return 0;
}
