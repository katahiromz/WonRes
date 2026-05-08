#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "WonRes.h"

int main(void)
{
    WCHAR szPath[MAX_PATH];
    GetModuleFileNameW(NULL, szPath, _countof(szPath));

    HINSTANCE mod = LoadLibraryExW(szPath, NULL, LOAD_LIBRARY_AS_DATAFILE);
    printf("mod: %p\n", mod);
    HRSRC hRsrc = WonFindResourceExW(mod, (LPCWSTR)RT_MENU, MAKEINTRESOURCEW(1), MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT));
    printf("hRsrc: %p\n", hRsrc);
    DWORD size = WonSizeofResource(mod, hRsrc);
    printf("size: %ld\n", size);
    HGLOBAL hGlobal = WonLoadResource(mod, hRsrc);
    printf("hGlobal: %p\n", hGlobal);
    LPVOID pv = WonLockResource(hGlobal);
    printf("pv: %p\n", pv);
    BOOL ret = !!pv;
    FreeLibrary(mod);
    printf("ret: %d\n", ret);

    return ret ? 0 : 1;
}
