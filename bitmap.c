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

	HBITMAP hbm = NULL;
	LPVOID lpData = WonLockResource(hglb);
	if (lpData) // NULL here means an encrypted resource that failed to decrypt
	{
		DWORD cbData = WonSizeofResource(hModule, hrsrc);
		BITMAPINFO *pbmi = (BITMAPINFO *)lpData;
		BITMAPINFOHEADER *pbmih = &pbmi->bmiHeader;

		if (cbData >= sizeof(BITMAPINFOHEADER) && pbmih->biSize >= sizeof(BITMAPINFOHEADER))
		{
			DWORD dwColorTableSize = WonpGetColorTableSize(pbmih);
			DWORD dwHeaderAndTable = pbmih->biSize + dwColorTableSize;

			if (dwHeaderAndTable <= cbData)
			{
				LPBYTE pbBits = (LPBYTE)lpData + dwHeaderAndTable;

				HDC hdc = GetDC(NULL);
				if (hdc)
				{
					// CreateDIBitmap goes through GDI's normal DIB engine, so
					// it decodes BI_RLE4/BI_RLE8 source data transparently --
					// no special-casing needed here for RLE-compressed bitmaps.
					hbm = CreateDIBitmap(hdc, pbmih, CBM_INIT, pbBits, pbmi, DIB_RGB_COLORS);
					ReleaseDC(NULL, hdc);
				}
			}
		}
	}

	// Safe now in every case (success, or any of the checks above failing):
	// CreateDIBitmap copies the pixel data into the new HBITMAP during the
	// call and doesn't keep a reference to lpData/hglb afterwards.
	WonFreeResource(hglb);

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
