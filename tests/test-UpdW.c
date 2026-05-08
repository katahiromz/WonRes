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
    ret = WonUpdateResourceW(hUpdate, (LPWSTR)RT_RCDATA, L"Test1", 0, sz, (DWORD)sizeof(sz));
    printf("ret: %d\n", ret);
    ret = WonUpdateResourceW(hUpdate, (LPWSTR)RT_RCDATA, L"Test2", 0, sz, (DWORD)sizeof(sz));
    printf("ret: %d\n", ret);
    ret = WonEndUpdateResourceW(hUpdate, FALSE);
    printf("ret: %d\n", ret);

    HINSTANCE mod = LoadLibraryExW(szPath, NULL, LOAD_LIBRARY_AS_DATAFILE);
    HRSRC hRsrc = FindResourceExW(mod, (LPCWSTR)RT_RCDATA, L"Test2", 0);
    DWORD err = GetLastError();
    printf("hRsrc: %p\n", hRsrc);
    printf("err: %ld\n", err);
    DWORD size = SizeofResource(mod, hRsrc);
    printf("size: %ld\n", size);
    HGLOBAL hGlobal = LoadResource(mod, hRsrc);
    printf("hGlobal: %p\n", hGlobal);
    LPVOID pv = LockResource(hGlobal);
    printf("pv: %p\n", pv);
    ret = pv && (memcmp(pv, sz, sizeof(sz)) == 0) && size == sizeof(sz);
    FreeLibrary(mod);
    printf("ret: %d\n", ret);

    return ret ? 0 : 1;
}
