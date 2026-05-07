#include <windows.h>
#include <stdio.h>
#include "WonRes.h"

int main(void)
{
    HANDLE hUpdate = WonBeginUpdateResourceA("test-Langs.exe", FALSE);
    printf("hUpdate: %p\n", hUpdate);
    char sz[] = "This is a test";
    BOOL ret;
    ret = WonUpdateResourceA(hUpdate, (LPSTR)RT_RCDATA, "Test", 0, sz, (DWORD)sizeof(sz));
    printf("ret: %d\n", ret);
    ret = WonEndUpdateResourceA(hUpdate, FALSE);
    printf("ret: %d\n", ret);

    HINSTANCE mod = LoadLibraryExA("test-Langs.exe", NULL, LOAD_LIBRARY_AS_DATAFILE);
    HRSRC hRsrc = FindResourceExA(mod, (LPCSTR)RT_RCDATA, "Test", 0);
    FreeLibrary(mod);
    printf("hRsrc: %p\n", hRsrc);

    return 0;
}
