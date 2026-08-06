// bitmap.cpp --- LoadBitmap() emulation on top of WonRes
// Author: katahiromz
// License: MIT
//////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include "WonRes.h"

//////////////////////////////////////////////////////////////////////////////

static DWORD WonpGetColorTableSize(const BITMAPINFOHEADER *pbmih)
{
	if (pbmih->biBitCount > 8)
	{
		if (pbmih->biCompression == BI_BITFIELDS)
			return 3 * sizeof(DWORD);
		return 0;
	}

	DWORD dwColors = pbmih->biClrUsed ? pbmih->biClrUsed : (1u << pbmih->biBitCount);
	return dwColors * sizeof(RGBQUAD);
}

static HBITMAP WonpBuildBitmapFromResource(HMODULE hModule, HRSRC hrsrc)
{
	if (!hrsrc)
		return NULL;

	HGLOBAL hglb = WonLoadResource(hModule, hrsrc);
	if (!hglb)
		return NULL;

	LPVOID lpData = WonLockResource(hglb);
	if (!lpData)
		return NULL;

	DWORD cbData = WonSizeofResource(hModule, hrsrc);
	if (cbData < sizeof(BITMAPINFOHEADER))
		return NULL;

	BITMAPINFO *pbmi = (BITMAPINFO *)lpData;
	BITMAPINFOHEADER *pbmih = &pbmi->bmiHeader;

	if (pbmih->biSize < sizeof(BITMAPINFOHEADER))
		return NULL;

	DWORD dwColorTableSize = WonpGetColorTableSize(pbmih);
	DWORD dwHeaderAndTable = pbmih->biSize + dwColorTableSize;
	if (dwHeaderAndTable > cbData)
		return NULL;

	LPBYTE pbBits = (LPBYTE)lpData + dwHeaderAndTable;

	HDC hdc = GetDC(NULL);
	if (!hdc)
		return NULL;

	HBITMAP hbm = CreateDIBitmap(hdc, pbmih, CBM_INIT, pbBits, pbmi, DIB_RGB_COLORS);

	ReleaseDC(NULL, hdc);

	return hbm;
}

//////////////////////////////////////////////////////////////////////////////

HBITMAP WONAPI WonLoadBitmapA(HINSTANCE hInstance, LPCSTR lpBitmapName)
{
	HMODULE hModule = (HMODULE)hInstance;

	HRSRC hrsrc = WonFindResourceA(hModule, lpBitmapName, (LPCSTR)RT_BITMAP);
	return WonpBuildBitmapFromResource(hModule, hrsrc);
}

HBITMAP WONAPI WonLoadBitmapW(HINSTANCE hInstance, LPCWSTR lpBitmapName)
{
	HMODULE hModule = (HMODULE)hInstance;

	HRSRC hrsrc = WonFindResourceW(hModule, lpBitmapName, (LPCWSTR)RT_BITMAP);
	return WonpBuildBitmapFromResource(hModule, hrsrc);
}

//////////////////////////////////////////////////////////////////////////////
