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
    BOOL ret1;
    ret1 = WonUpdateResourceA(hUpdate, (LPSTR)RT_RCDATA, "Test1", 0, sz, (DWORD)sizeof(sz));
    printf("ret1: %d\n", ret1);
    ret1 = WonUpdateResourceA(hUpdate, (LPSTR)RT_RCDATA, "Test2", 0, sz, (DWORD)sizeof(sz));
    printf("ret1: %d\n", ret1);
    ret1 = WonEndUpdateResourceA(hUpdate, FALSE);
    printf("ret1: %d\n", ret1);

    HINSTANCE mod = LoadLibraryExA(szPath, NULL, LOAD_LIBRARY_AS_DATAFILE);
    HRSRC hRsrc = FindResourceExA(mod, (LPCSTR)RT_RCDATA, "Test2", 0);
    DWORD err = GetLastError();
    printf("hRsrc: %p\n", hRsrc);
    printf("err: %ld\n", err);
    DWORD size = SizeofResource(mod, hRsrc);
    printf("size: %ld\n", size);
    HGLOBAL hGlobal = LoadResource(mod, hRsrc);
    printf("hGlobal: %p\n", hGlobal);
    LPVOID pv = LockResource(hGlobal);
    printf("pv: %p\n", pv);
    BOOL ret2 = pv && (memcmp(pv, sz, sizeof(sz)) == 0) && size == sizeof(sz);
    FreeLibrary(mod);
    printf("ret2: %d\n", ret2);

    DWORD dw;
    BOOL ret3 = GetBinaryTypeA(szPath, &dw);
    printf("ret3: %d\n", ret3);
    printf("err: %d\n", GetLastError());

    return (ret1 && ret2 && ret3) ? 0 : 1;
}
