#include <windows.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "WonRes.h"

int main(void)
{
    CHAR szPath[MAX_PATH];
    GetModuleFileNameA(NULL, szPath, _countof(szPath));
    LPSTR pch = strrchr(szPath, '\\');
    *pch = 0;
    lstrcatA(szPath, "\\test-Langs.exe");
    HANDLE hUpdate = WonBeginUpdateResourceA(szPath, TRUE);
    printf("hUpdate: %p\n", hUpdate);
    char sz[] = "This is a test";
    BOOL ret;
    ret = WonUpdateResourceA(hUpdate, (LPSTR)RT_RCDATA, "Test", 0, sz, (DWORD)sizeof(sz));
    printf("ret: %d\n", ret);
    ret = WonEndUpdateResourceA(hUpdate, FALSE);
    printf("ret: %d\n", ret);

    HINSTANCE mod = LoadLibraryExA(szPath, NULL, LOAD_LIBRARY_AS_DATAFILE);
    HRSRC hRsrc = WonFindResourceExA(mod, (LPCSTR)RT_RCDATA, "Test", 0);
    printf("hRsrc: %p\n", hRsrc);
    DWORD size = WonSizeofResource(mod, hRsrc);
    printf("size: %ld\n", size);
    HGLOBAL hGlobal = WonLoadResource(mod, hRsrc);
    printf("hGlobal: %p\n", hGlobal);
    LPVOID pv = WonLockResource(hGlobal);
    printf("pv: %p\n", pv);
    ret = pv && (memcmp(pv, sz, sizeof(sz)) == 0) && size == sizeof(sz);
    FreeLibrary(mod);
    printf("ret: %d\n", ret);

    return ret ? 0 : 1;
}
