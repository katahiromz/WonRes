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

HRSRC WONAPI WonFindResource(HMODULE hModule, LPCTSTR lpName, LPCTSTR lpType);
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

INT WONAPI WonLoadString(HINSTANCE hInstance, UINT uID, LPTSTR lpBuffer, INT nBufferMax);
HBITMAP WONAPI WonLoadBitmap(HINSTANCE hInstance, LPCTSTR lpBitmapName);
HMENU WONAPI WonLoadMenu(HINSTANCE hInstance, LPCTSTR lpMenuName);
HICON WONAPI WonLoadIcon(HINSTANCE hInstance, LPCTSTR lpIconName);
HCURSOR WONAPI WonLoadCursor(HINSTANCE hInstance, LPTCSTR lpCursorName);

INT_PTR WONAPI WonDialogBox(HINSTANCE hInstance, LPCTSTR lpTemplateName, HWND hWndParent,
                            DLGPROC lpDialogFunc);
HWND WONAPI WonCreateDialog(HINSTANCE hInstance, LPCTSTR lpTemplateName, HWND hWndParent,
                            DLGPROC lpDialogFunc);

BOOL WONAPI WonFreeResource(HGLOBAL hGlobal);
```

License: MIT
