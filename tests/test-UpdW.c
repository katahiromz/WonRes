#include <windows.h>
#include <stdio.h>
#include "WonRes.h"

int main(void)
{
    HANDLE hUpdate = BeginUpdateResourceW(L"test-Langs.exe", FALSE);
    char sz[] = "This is a test";
    UpdateResourceW(hUpdate, (LPWSTR)RT_RCDATA, L"Test", 0, sz, (DWORD)sizeof(sz));
    EndUpdateResourceW(hUpdate, FALSE);
    return 0;
}
