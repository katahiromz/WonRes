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
					BOOL fCompressed = (pbmih->biCompression == BI_RLE4 ||
					                    pbmih->biCompression == BI_RLE8);
					if (fCompressed)
					{
						// CreateDIBitmap builds a bitmap matching the
						// *current display*, so GDI has to color-match
						// every decoded pixel against whatever the display
						// can currently show -- on anything other than a
						// plain truecolor desktop that shows up as
						// washed-out/blotchy colors. Decode straight into
						// a DIB section of our own choosing (24-bit
						// BI_RGB) via SetDIBits instead, so GDI converts
						// each palette entry straight to its exact RGB
						// value with no device-dependent approximation.
						BITMAPINFOHEADER bmihDst = { sizeof(bmihDst) };
						bmihDst.biWidth = pbmih->biWidth;
						bmihDst.biHeight = pbmih->biHeight;
						bmihDst.biPlanes = 1;
						bmihDst.biBitCount = 24;
						bmihDst.biCompression = BI_RGB;

						LPVOID pvBits = NULL;
						hbm = CreateDIBSection(hdc, (BITMAPINFO *)&bmihDst, DIB_RGB_COLORS,
						                       &pvBits, NULL, 0);
						if (hbm)
						{
							// lpbmi here is still the *source* description
							// (pbmi, with its original biBitCount/
							// biCompression/color table) -- SetDIBits
							// decodes from that format into hbm's (the
							// 24-bit target's).
							if (!SetDIBits(hdc, hbm, 0, pbmih->biHeight, pbBits, pbmi,
							               DIB_RGB_COLORS))
							{
								DeleteObject(hbm);
								hbm = NULL;
							}
						}
					}
					else
					{
						// CreateDIBitmap goes through GDI's normal DIB
						// engine, matching real LoadBitmap's own device-
						// dependent-bitmap behavior for the uncompressed
						// case (LoadBitmap has no DIB-section option at
						// all -- it always returns a DDB).
						hbm = CreateDIBitmap(hdc, pbmih, CBM_INIT, pbBits, pbmi, DIB_RGB_COLORS);
					}

					ReleaseDC(NULL, hdc);
				}
			}
		}
	}

	// Safe now in every case (success, or any of the checks above failing):
	// CreateDIBitmap/CreateDIBSection+SetDIBits both copy the pixel data
	// into the new HBITMAP during the call and don't keep a reference to
	// lpData/hglb afterwards.
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
