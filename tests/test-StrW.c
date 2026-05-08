#include <windows.h>
#include <stdio.h>
#include "WonRes.h"

int main(int argc, char **argv)
{
    WCHAR buf[5];
    int ret1 = LoadStringW(NULL, 100, buf, 5);
    printf("ret1: %d\n", ret1);
    printf("%ls\n", buf);
    int ret2 = lstrcmpW(buf, L"test");
    printf("ret2: %d\n", ret2);
    return (ret1 && !ret2) ? 0 : 1;
}
