#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "WonRes.h"

int main(void)
{
    CHAR szPath[MAX_PATH];
    GetModuleFileNameA(NULL, szPath, _countof(szPath));

    HINSTANCE mod = LoadLibraryExA(szPath, NULL, LOAD_LIBRARY_AS_DATAFILE);
    printf("mod: %p\n", mod);
    HRSRC hRsrc = WonFindResourceExA(mod, (LPCSTR)RT_MENU, MAKEINTRESOURCEA(1), MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT));
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
