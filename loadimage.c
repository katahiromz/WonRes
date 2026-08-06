// loadimage.c --- Win32 generic image loader (LoadImage) for WonRes
// Author: katahiromz
// License: MIT
//
// LoadImage is really three different loaders behind one entry point,
// selected by uType:
//   - IMAGE_ICON/IMAGE_CURSOR: exactly the two-hop RT_GROUP_ICON/RT_ICON
//     (or RT_GROUP_CURSOR/RT_CURSOR) lookup already used by
//     WonLoadIcon/WonLoadCursor -- LookupIconIdFromDirectoryEx to pick the
//     best variant, then CreateIconFromResourceEx to build the handle --
//     just parameterized by the caller's own cx/cy/flags instead of the
//     system metric defaults.
//   - IMAGE_BITMAP: find the RT_BITMAP resource, which is nothing more
//     than a BITMAPINFO (header + optional color table) immediately
//     followed by the pixel bits -- i.e. a DIB without the BITMAPFILEHEADER
//     -- and hand it to CreateDIBitmap (or CreateDIBSection, for
//     LR_CREATEDIBSECTION) to build the HBITMAP. This is the documented
//     technique for turning a loaded RT_BITMAP resource into a real
//     bitmap handle.
// All resource access goes through WonFindResourceW/A + WonLoadResource +
// WonLockResource -- which is where any WONRES_ENABLE_CRYPTO decryption
// already happens -- same as everywhere else in WonRes.
//
// LR_LOADFROMFILE has nothing to do with PE resources at all (it loads an
// .ico/.cur/.bmp straight off disk) and is forwarded to the real
// LoadImageA/W untouched, as is hInstance == NULL with an int resource
// name (the OS's own predefined OEM icon/cursor, same special case as
// WonLoadIcon/WonLoadCursor).
//
// Known gaps vs. the real LoadImage: LR_LOADTRANSPARENT and
// LR_LOADMAP3DCOLORS color-remapping for IMAGE_BITMAP aren't implemented
// (the bitmap loads at its resource's native colors); LR_SHARED doesn't
// provide real handle caching -- every call still allocates a fresh
// handle, as if LR_SHARED had been cleared.
#include <windows.h>
#include <imagehlp.h>
#include "WonRes.h"

// Internal helper defined in anicursor.c: falls back to the animated
// (RT_ANIICON/RT_ANICURSOR, i.e. .ani/RIFF) form of the resource via a
// short-lived temp file. Not part of the public WonRes.h surface.
extern HANDLE WonpLoadAnimatedIconOrCursorW(HINSTANCE hInstance, LPCWSTR lpName, BOOL fIcon,
                                            INT cx, INT cy, UINT fuLoad);

////////////////////////////////////////////////////////////////////////////////////
// IMAGE_BITMAP: RT_BITMAP resource bytes -> HBITMAP

static DWORD DibColorTableSize(const BITMAPINFOHEADER *pbih)
{
    if (pbih->biBitCount > 8)
        return (pbih->biCompression == BI_BITFIELDS) ? 3 * sizeof(DWORD) : 0;

    DWORD nColors = pbih->biClrUsed ? pbih->biClrUsed : (1u << pbih->biBitCount);
    return nColors * sizeof(RGBQUAD);
}

static HBITMAP CreateBitmapFromResourceBits(LPBYTE pResData, DWORD cbResData, UINT fuLoad)
{
    if (!pResData || cbResData < sizeof(BITMAPINFOHEADER))
        return NULL;

    PBITMAPINFO pbmi = (PBITMAPINFO)pResData;
    DWORD cbHeaderAndColors = pbmi->bmiHeader.biSize + DibColorTableSize(&pbmi->bmiHeader);
    if (cbHeaderAndColors > cbResData)
        return NULL; // corrupt/truncated resource

    LPBYTE pBits = pResData + cbHeaderAndColors;

    HDC hdcScreen = GetDC(NULL);
    if (!hdcScreen)
        return NULL;

    // CreateDIBSection hands back a raw pixel buffer sized/strided from
    // biWidth/biBitCount directly -- it does not decode BI_RLE4/BI_RLE8,
    // so copying compressed source bytes into it would just corrupt the
    // image. CreateDIBitmap, on the other hand, goes through GDI's normal
    // DIB engine and decodes RLE transparently. So RLE-compressed source
    // data always goes through CreateDIBitmap, regardless of whether the
    // caller asked for LR_CREATEDIBSECTION.
    BOOL fCompressed = (pbmi->bmiHeader.biCompression == BI_RLE8 ||
                        pbmi->bmiHeader.biCompression == BI_RLE4);

    HBITMAP hbm;
    if ((fuLoad & LR_CREATEDIBSECTION) && !fCompressed) {
        LPVOID pvBits = NULL;
        hbm = CreateDIBSection(hdcScreen, pbmi, DIB_RGB_COLORS, &pvBits, NULL, 0);
        if (hbm && pvBits)
            CopyMemory(pvBits, pBits, cbResData - cbHeaderAndColors);
    } else {
        hbm = CreateDIBitmap(hdcScreen, &pbmi->bmiHeader, CBM_INIT, pBits, pbmi, DIB_RGB_COLORS);
    }

    ReleaseDC(NULL, hdcScreen);
    return hbm;
}

// Stretches hbm to cxDesired x cyDesired (whichever of the two is nonzero;
// 0 means "keep that dimension as-is") if that differs from its current
// size. Consumes hbm either way: returns a new handle, or NULL on failure
// (in which case hbm has already been destroyed).
static HBITMAP ResizeBitmapIfNeeded(HBITMAP hbm, INT cxDesired, INT cyDesired)
{
    if (!hbm || (!cxDesired && !cyDesired))
        return hbm;

    BITMAP bm;
    if (!GetObjectW(hbm, sizeof(bm), &bm))
        return hbm;

    INT cx = cxDesired ? cxDesired : bm.bmWidth;
    INT cy = cyDesired ? cyDesired : bm.bmHeight;
    if (cx == bm.bmWidth && cy == bm.bmHeight)
        return hbm;

    HDC hdcScreen = GetDC(NULL);
    if (!hdcScreen)
        return hbm;

    HBITMAP hbmNew = NULL;
    HDC hdcSrc = CreateCompatibleDC(hdcScreen);
    HDC hdcDst = CreateCompatibleDC(hdcScreen);
    if (hdcSrc && hdcDst) {
        hbmNew = CreateCompatibleBitmap(hdcScreen, cx, cy);
        if (hbmNew) {
            HGDIOBJ hOldSrc = SelectObject(hdcSrc, hbm);
            HGDIOBJ hOldDst = SelectObject(hdcDst, hbmNew);
            StretchBlt(hdcDst, 0, 0, cx, cy, hdcSrc, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
            SelectObject(hdcSrc, hOldSrc);
            SelectObject(hdcDst, hOldDst);
        }
    }
    if (hdcSrc)
        DeleteDC(hdcSrc);
    if (hdcDst)
        DeleteDC(hdcDst);
    ReleaseDC(NULL, hdcScreen);

    DeleteObject(hbm);
    return hbmNew; // NULL if anything above failed
}

////////////////////////////////////////////////////////////////////////////////////
// Shared implementation, always operating in wide-character terms (the ANSI
// entry point just converts the name once, same pattern as WonLoadIconA).

static HANDLE WonLoadImageResourceW(HINSTANCE hInstance, LPCWSTR lpName, UINT uType,
                                    INT cxDesired, INT cyDesired, UINT fuLoad)
{
    if (uType == IMAGE_ICON || uType == IMAGE_CURSOR) {
        BOOL fIcon = (uType == IMAGE_ICON);

        if (!hInstance && IS_INTRESOURCE(lpName)) // OS predefined icon/cursor
            return (HANDLE)LoadImageW(NULL, lpName, uType, cxDesired, cyDesired, fuLoad);

        INT cx = cxDesired, cy = cyDesired;
        if (!cx && !cy && (fuLoad & LR_DEFAULTSIZE)) {
            cx = GetSystemMetrics(fIcon ? SM_CXICON : SM_CXCURSOR);
            cy = GetSystemMetrics(fIcon ? SM_CYICON : SM_CYCURSOR);
        }

        LPCWSTR pGroupType = fIcon ? (LPCWSTR)RT_GROUP_ICON : (LPCWSTR)RT_GROUP_CURSOR;
        LPCWSTR pItemType = fIcon ? (LPCWSTR)RT_ICON : (LPCWSTR)RT_CURSOR;
        UINT fuColor = fuLoad & (LR_DEFAULTCOLOR | LR_MONOCHROME | LR_VGACOLOR);

        // Hop 1: pick the best-matching variant out of the group directory.
        HRSRC hRsrcGroup = WonFindResourceW(hInstance, lpName, pGroupType);
        if (!hRsrcGroup) {
            // Not a static icon/cursor -- try the animated (.ani) form
            // before giving up, exactly like the real LoadImage does.
            return WonpLoadAnimatedIconOrCursorW(hInstance, lpName, fIcon, cx, cy, fuLoad);
        }

        HGLOBAL hGlobalGroup = WonLoadResource(hInstance, hRsrcGroup);
        if (!hGlobalGroup)
            return NULL;

        LPBYTE pDir = (LPBYTE)WonLockResource(hGlobalGroup);
        INT nID = 0;
        if (pDir) // NULL here means an encrypted resource that failed to decrypt
            nID = LookupIconIdFromDirectoryEx(pDir, fIcon, cx, cy, fuColor);
        WonFreeResource(hGlobalGroup);

        if (!nID)
            return NULL;

        // Hop 2: load the chosen variant's actual pixel data and build the handle.
        HRSRC hRsrcItem = WonFindResourceW(hInstance, MAKEINTRESOURCEW(nID), pItemType);
        if (!hRsrcItem)
            return NULL;

        DWORD cbItem = WonSizeofResource(hInstance, hRsrcItem);

        HGLOBAL hGlobalItem = WonLoadResource(hInstance, hRsrcItem);
        if (!hGlobalItem)
            return NULL;

        LPBYTE pBits = (LPBYTE)WonLockResource(hGlobalItem);
        HANDLE hResult = NULL;
        if (pBits) {
            hResult = (HANDLE)CreateIconFromResourceEx(pBits, cbItem, fIcon, 0x00030000, cx, cy,
                                                        fuColor);
        }
        WonFreeResource(hGlobalItem);
        return hResult;
    }

    if (uType == IMAGE_BITMAP) {
        HRSRC hRsrc = WonFindResourceW(hInstance, lpName, (LPWSTR)RT_BITMAP);
        if (!hRsrc)
            return NULL;

        // Must be read before WonLoadResource/WonFreeResource for an encrypted
        // resource: WonSizeofResource reports the plaintext size in that case
        // (see loader.c), matching what WonLockResource will hand back below.
        DWORD cbBitmap = WonSizeofResource(hInstance, hRsrc);

        HGLOBAL hGlobal = WonLoadResource(hInstance, hRsrc);
        if (!hGlobal)
            return NULL;

        LPBYTE pBits = (LPBYTE)WonLockResource(hGlobal);
        HBITMAP hbm = pBits ? CreateBitmapFromResourceBits(pBits, cbBitmap, fuLoad) : NULL;
        WonFreeResource(hGlobal);

        return (HANDLE)ResizeBitmapIfNeeded(hbm, cxDesired, cyDesired);
    }

    SetLastError(ERROR_INVALID_PARAMETER);
    return NULL;
}

HANDLE WONAPI WonLoadImageW(HINSTANCE hInstance, LPCWSTR lpName, UINT uType, INT cxDesired,
                            INT cyDesired, UINT fuLoad)
{
    if (fuLoad & LR_LOADFROMFILE) // not a PE resource at all -- nothing Won-specific to add
        return LoadImageW(hInstance, lpName, uType, cxDesired, cyDesired, fuLoad);

    return WonLoadImageResourceW(hInstance, lpName, uType, cxDesired, cyDesired, fuLoad);
}

HANDLE WONAPI WonLoadImageA(HINSTANCE hInstance, LPCSTR lpName, UINT uType, INT cxDesired,
                            INT cyDesired, UINT fuLoad)
{
    if (fuLoad & LR_LOADFROMFILE)
        return LoadImageA(hInstance, lpName, uType, cxDesired, cyDesired, fuLoad);

    LPCWSTR pszNameW;
    WCHAR szNameW[MAX_RES_ID_LEN];

    if (IS_INTRESOURCE(lpName)) {
        pszNameW = (LPCWSTR)lpName;
    } else {
        if (!MultiByteToWideChar(CP_ACP, 0, lpName, -1, szNameW, _countof(szNameW)))
            return NULL;
        szNameW[_countof(szNameW) - 1] = UNICODE_NULL;
        pszNameW = szNameW;
    }

    return WonLoadImageResourceW(hInstance, pszNameW, uType, cxDesired, cyDesired, fuLoad);
}
