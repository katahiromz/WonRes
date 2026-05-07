// WonRes.h --- Win32 resource loader
// Author: katahiromz
// License: MIT

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WONAPI
    #define WONAPI WINAPI
#endif

#ifndef MAX_RES_ID_LEN
    #define MAX_RES_ID_LEN 256
#endif

BOOL WONAPI WonEnumResourceTypesA(HMODULE hModule, ENUMRESTYPEPROCA lpEnumFunc, LONG_PTR lParam);
BOOL WONAPI WonEnumResourceTypesW(HMODULE hModule, ENUMRESTYPEPROCW lpEnumFunc, LONG_PTR lParam);

BOOL WONAPI WonEnumResourceNamesA(HMODULE hModule, LPCSTR lpType, ENUMRESNAMEPROCA lpEnumFunc,
                                  LONG_PTR lParam);
BOOL WONAPI WonEnumResourceNamesW(HMODULE hModule, LPCWSTR lpType, ENUMRESNAMEPROCW lpEnumFunc,
                                  LONG_PTR lParam);

BOOL WONAPI WonEnumResourceLanguagesA(
    HMODULE hModule,
    LPCSTR lpType,
    LPCSTR lpName,
    ENUMRESLANGPROCA lpEnumFunc,
    LONG_PTR lParam);

BOOL WONAPI WonEnumResourceLanguagesW(
    HMODULE hModule,
    LPCWSTR lpType,
    LPCWSTR lpName,
    ENUMRESLANGPROCW lpEnumFunc,
    LONG_PTR lParam);

HRSRC WONAPI WonFindResourceA(HMODULE hModule, LPCSTR lpType, LPCSTR lpName);
HRSRC WONAPI WonFindResourceW(HMODULE hModule, LPCWSTR lpType, LPCWSTR lpName);

HRSRC WONAPI WonFindResourceExA(HMODULE hModule, LPCSTR lpType, LPCSTR lpName, WORD wLanguage);
HRSRC WONAPI WonFindResourceExW(HMODULE hModule, LPCWSTR lpType, LPCWSTR lpName, WORD wLanguage);

DWORD WONAPI WonSizeofResource(HMODULE hModule, HRSRC hrsrc);
HGLOBAL WONAPI WonLoadResource(HMODULE hModule, HRSRC hrsrc);
LPVOID WONAPI WonLockResource(HGLOBAL hResData);

HANDLE WONAPI WonBeginUpdateResourceA(LPCSTR pFileName, BOOL bDeleteExistingResources);
HANDLE WONAPI WonBeginUpdateResourceW(LPCWSTR pFileName, BOOL bDeleteExistingResources);
BOOL WONAPI WonEndUpdateResourceA(HANDLE hUpdate, BOOL fDiscard);
BOOL WONAPI WonEndUpdateResourceW(HANDLE hUpdate, BOOL fDiscard);

BOOL WONAPI WonUpdateResourceA(
    HANDLE hUpdate,
    LPCSTR lpType,
    LPCSTR lpName,
    WORD wLanguage,
    LPVOID lpData,
    DWORD cbData);
BOOL WONAPI WonUpdateResourceW(
    HANDLE hUpdate,
    LPCWSTR lpType,
    LPCWSTR lpName,
    WORD wLanguage,
    LPVOID lpData,
    DWORD cbData);

#ifdef UNICODE
    #define WonEnumResourceTypes WonEnumResourceTypesW
    #define WonEnumResourceNames WonEnumResourceNamesW
    #define WonEnumResourceLanguages WonEnumResourceLanguagesW
    #define WonFindResource WonFindResourceW
    #define WonFindResourceEx WonFindResourceExW
    #define WonBeginUpdateResource WonBeginUpdateResourceW
    #define WonUpdateResource WonUpdateResourceW
    #define WonEndUpdateResource WonEndUpdateResourceW
#else
    #define WonEnumResourceTypes WonEnumResourceTypesA
    #define WonEnumResourceNames WonEnumResourceNamesA
    #define WonEnumResourceLanguages WonEnumResourceLanguagesA
    #define WonFindResource WonFindResourceA
    #define WonFindResourceEx WonFindResourceExA
    #define WonBeginUpdateResource WonBeginUpdateResourceA
    #define WonUpdateResource WonUpdateResourceA
    #define WonEndUpdateResource WonEndUpdateResourceA
#endif

#ifdef __cplusplus
} // extern "C"
#endif
