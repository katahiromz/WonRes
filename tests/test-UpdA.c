#include <windows.h>
#include <stdio.h>
#include "WonRes.h"

int main(void)
{
    HANDLE hUpdate = BeginUpdateResourceA("test-Langs.exe", FALSE);
    char sz[] = "This is a test";
    UpdateResourceA(hUpdate, (LPSTR)RT_RCDATA, "Test", 0, sz, (DWORD)sizeof(sz));
    EndUpdateResourceA(hUpdate, FALSE);
    return 0;
}
