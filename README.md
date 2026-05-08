# WonRes

Win32 resource handling compatibility layer.

```c
#include "WonRes.h"

BOOL WONAPI WonEnumResourceTypes(HMODULE hModule, ENUMRESTYPEPROC lpEnumFunc, LONG_PTR lParam);
BOOL WONAPI WonEnumResourceNames(HMODULE hModule, LPCTSTR lpType, ENUMRESNAMEPROC lpEnumFunc,
                                 LONG_PTR lParam);
BOOL WONAPI WonEnumResourceLanguages(
    HMODULE hModule,
    LPCTSTR lpType,
    LPCTSTR lpName,
    ENUMRESLANGPROC lpEnumFunc,
    LONG_PTR lParam);

HRSRC WONAPI WonFindResource(HMODULE hModule, LPCTSTR lpType, LPCWSTR lpName);
HRSRC WONAPI WonFindResourceExW(HMODULE hModule, LPCTSTR lpType, LPCTSTR lpName, WORD wLanguage);

DWORD WONAPI WonSizeofResource(HMODULE hModule, HRSRC hrsrc);
HGLOBAL WONAPI WonLoadResource(HMODULE hModule, HRSRC hrsrc);
LPVOID WONAPI WonLockResource(HGLOBAL hResData);

HANDLE WONAPI WonBeginUpdateResource(LPCTSTR pFileName, BOOL bDeleteExistingResources);
BOOL WONAPI WonEndUpdateResource(HANDLE hUpdate, BOOL fDiscard);

BOOL WONAPI WonUpdateResource(
    HANDLE hUpdate,
    LPCTSTR lpType,
    LPCTSTR lpName,
    WORD wLanguage,
    LPVOID lpData,
    DWORD cbData);

int WONAPI WonLoadString(HINSTANCE hInstance, UINT uID, LPSTR lpBuffer, int nBufferMax);
```

License: MIT
