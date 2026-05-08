#include <windows.h>
#include <stdio.h>
#include "WonRes.h"

int main(int argc, char **argv)
{
    char buf[5];
    int ret1 = LoadStringA(NULL, 100, buf, 5);
    printf("ret1: %d\n", ret1);
    printf("%s\n", buf);
    int ret2 = lstrcmpA(buf, "test");
    printf("ret2: %d\n", ret2);
    return (ret1 && !ret2) ? 0 : 1;
}
