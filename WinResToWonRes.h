// WinResToWonRes.h --- Replace Win32 resource functions with WonRes functions
// Author: katahiromz
// License: MIT
#pragma once

#ifndef __WONRES__
	#include "WonRes.h"
#endif

#undef BeginUpdateResource
#undef BeginUpdateResourceA
#undef BeginUpdateResourceW
#undef CreateDialog
#undef CreateDialogA
#undef CreateDialogIndirect
#undef CreateDialogIndirectA
#undef CreateDialogIndirectParam
#undef CreateDialogIndirectParamA
#undef CreateDialogIndirectParamW
#undef CreateDialogIndirectW
#undef CreateDialogParam
#undef CreateDialogParamA
#undef CreateDialogParamW
#undef CreateDialogW
#undef DialogBox
#undef DialogBoxA
#undef DialogBoxIndirect
#undef DialogBoxIndirectA
#undef DialogBoxIndirectParam
#undef DialogBoxIndirectParamA
#undef DialogBoxIndirectParamW
#undef DialogBoxIndirectW
#undef DialogBoxParam
#undef DialogBoxParamA
#undef DialogBoxParamW
#undef DialogBoxW
#undef EndUpdateResource
#undef EndUpdateResourceA
#undef EndUpdateResourceW
#undef EnumResourceLanguages
#undef EnumResourceLanguagesA
#undef EnumResourceLanguagesW
#undef EnumResourceNames
#undef EnumResourceNamesA
#undef EnumResourceNamesW
#undef EnumResourceTypes
#undef EnumResourceTypesA
#undef EnumResourceTypesW
#undef FindResource
#undef FindResourceA
#undef FindResourceEx
#undef FindResourceExA
#undef FindResourceExW
#undef FindResourceW
#undef FormatMessage
#undef FormatMessageA
#undef FormatMessageW
#undef FreeResource
#undef LoadAccelerators
#undef LoadAcceleratorsA
#undef LoadAcceleratorsW
#undef LoadBitmap
#undef LoadBitmapA
#undef LoadBitmapW
#undef LoadCursor
#undef LoadCursorA
#undef LoadCursorW
#undef LoadIcon
#undef LoadIconA
#undef LoadIconW
#undef LoadImage
#undef LoadImageA
#undef LoadImageW
#undef LoadMenu
#undef LoadMenuA
#undef LoadMenuIndirect
#undef LoadMenuIndirectA
#undef LoadMenuIndirectW
#undef LoadMenuW
#undef LoadResource
#undef LoadString
#undef LoadStringA
#undef LoadStringW
#undef LockResource
#undef SizeofResource
#undef UpdateResource
#undef UpdateResourceA
#undef UpdateResourceW

#define BeginUpdateResource WonBeginUpdateResource
#define BeginUpdateResourceA WonBeginUpdateResourceA
#define BeginUpdateResourceW WonBeginUpdateResourceW
#define CreateDialog WonCreateDialog
#define CreateDialogA WonCreateDialogA
#define CreateDialogIndirect WonCreateDialogIndirect
#define CreateDialogIndirectA WonCreateDialogIndirectA
#define CreateDialogIndirectParam WonCreateDialogIndirectParam
#define CreateDialogIndirectParamA WonCreateDialogIndirectParamA
#define CreateDialogIndirectParamW WonCreateDialogIndirectParamW
#define CreateDialogIndirectW WonCreateDialogIndirectW
#define CreateDialogParam WonCreateDialogParam
#define CreateDialogParamA WonCreateDialogParamA
#define CreateDialogParamW WonCreateDialogParamW
#define CreateDialogW WonCreateDialogW
#define DialogBox WonDialogBox
#define DialogBoxA WonDialogBoxA
#define DialogBoxIndirect WonDialogBoxIndirect
#define DialogBoxIndirectA WonDialogBoxIndirectA
#define DialogBoxIndirectParam WonDialogBoxIndirectParam
#define DialogBoxIndirectParamA WonDialogBoxIndirectParamA
#define DialogBoxIndirectParamW WonDialogBoxIndirectParamW
#define DialogBoxIndirectW WonDialogBoxIndirectW
#define DialogBoxParam WonDialogBoxParam
#define DialogBoxParamA WonDialogBoxParamA
#define DialogBoxParamW WonDialogBoxParamW
#define DialogBoxW WonDialogBoxW
#define EndUpdateResource WonEndUpdateResource
#define EndUpdateResourceA WonEndUpdateResourceA
#define EndUpdateResourceW WonEndUpdateResourceW
#define EnumResourceLanguages WonEnumResourceLanguages
#define EnumResourceLanguagesA WonEnumResourceLanguagesA
#define EnumResourceLanguagesW WonEnumResourceLanguagesW
#define EnumResourceNames WonEnumResourceNames
#define EnumResourceNamesA WonEnumResourceNamesA
#define EnumResourceNamesW WonEnumResourceNamesW
#define EnumResourceTypes WonEnumResourceTypes
#define EnumResourceTypesA WonEnumResourceTypesA
#define EnumResourceTypesW WonEnumResourceTypesW
#define FindResource WonFindResource
#define FindResourceA WonFindResourceA
#define FindResourceEx WonFindResourceEx
#define FindResourceExA WonFindResourceExA
#define FindResourceExW WonFindResourceExW
#define FindResourceW WonFindResourceW
#define FormatMessage WonFormatMessage
#define FormatMessageA WonFormatMessageA
#define FormatMessageW WonFormatMessageW
#define FreeResource WonFreeResource
#define LoadAccelerators WonLoadAccelerators
#define LoadAcceleratorsA WonLoadAcceleratorsA
#define LoadAcceleratorsW WonLoadAcceleratorsW
#define LoadBitmap WonLoadBitmap
#define LoadBitmapA WonLoadBitmapA
#define LoadBitmapW WonLoadBitmapW
#define LoadCursor WonLoadCursor
#define LoadCursorA WonLoadCursorA
#define LoadCursorW WonLoadCursorW
#define LoadIcon WonLoadIcon
#define LoadIconA WonLoadIconA
#define LoadIconW WonLoadIconW
#define LoadImage WonLoadImage
#define LoadImageA WonLoadImageA
#define LoadImageW WonLoadImageW
#define LoadMenu WonLoadMenu
#define LoadMenuA WonLoadMenuA
#define LoadMenuIndirect(lpMenuTemplate) WonLoadMenuIndirect((const MENUTEMPLATE*)(lpMenuTemplate))
#define LoadMenuIndirectA(lpMenuTemplate) WonLoadMenuIndirectA((const MENUTEMPLATEA*)(lpMenuTemplate))
#define LoadMenuIndirectW(lpMenuTemplate) WonLoadMenuIndirectW((const MENUTEMPLATEW*)(lpMenuTemplate))
#define LoadMenuW WonLoadMenuW
#define LoadResource WonLoadResource
#define LoadString WonLoadString
#define LoadStringA WonLoadStringA
#define LoadStringW WonLoadStringW
#define LockResource WonLockResource
#define SizeofResource WonSizeofResource
#define UpdateResource WonUpdateResource
#define UpdateResourceA WonUpdateResourceA
#define UpdateResourceW WonUpdateResourceW
