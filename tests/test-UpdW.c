#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "WonRes.h"

int main(void)
{
    WCHAR szPath[MAX_PATH];
    GetModuleFileNameW(NULL, szPath, _countof(szPath));
    LPWSTR pch = wcsrchr(szPath, '\\');
    *pch = 0;
    lstrcatW(szPath, L"\\test-Langs.exe");
    HANDLE hUpdate = WonBeginUpdateResourceW(szPath, TRUE);
    printf("hUpdate: %p\n", hUpdate);
    char sz[] = "This is a test";
    BOOL ret;
    ret = WonUpdateResourceW(hUpdate, (LPWSTR)RT_RCDATA, L"Test", 0, sz, (DWORD)sizeof(sz));
    printf("ret: %d\n", ret);
    ret = WonEndUpdateResourceW(hUpdate, FALSE);
    printf("ret: %d\n", ret);

    HINSTANCE mod = LoadLibraryExW(szPath, NULL, LOAD_LIBRARY_AS_DATAFILE);
    HRSRC hRsrc = WonFindResourceExW(mod, (LPCWSTR)RT_RCDATA, L"Test", 0);
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
