// formatmsg.c --- Win32 message-table formatter for WonRes
// Author: katahiromz
// License: MIT
//
// FormatMessage's insert-sequence engine (%1, %n!printf-style-spec!, %0,
// width control, FORMAT_MESSAGE_IGNORE_INSERTS/ARGUMENT_ARRAY, ...) is
// intricate and entirely orthogonal to *where the message text comes
// from*. Rather than reimplementing that engine, this file only teaches
// FORMAT_MESSAGE_FROM_HMODULE to go through Won's resource loader --
// WonFindResourceExW/A + WonLoadResource + WonLockResource, which is
// where any WONRES_ENABLE_CRYPTO decryption already happens -- to find
// the raw message text in the module's RT_MESSAGETABLE resource, and then
// hands that text to the real FormatMessageA/W as a plain
// FORMAT_MESSAGE_FROM_STRING source, letting it do 100% of the actual
// formatting exactly as it would for any other string source. Any other
// dwFlags combination (FORMAT_MESSAGE_FROM_STRING, FORMAT_MESSAGE_FROM_SYSTEM,
// ...) has nothing Won-specific to add and is forwarded untouched.
//
// The RT_MESSAGETABLE resource itself is just a MESSAGE_RESOURCE_DATA
// followed by MESSAGE_RESOURCE_BLOCK entries and then MESSAGE_RESOURCE_ENTRY
// records -- all public, documented structures from <winnt.h> (unlike the
// icon/cursor group directory, there's no private layout to worry about
// here), so it's parsed directly.
#include <windows.h>
#include <imagehlp.h>
#include "WonRes.h"

// Locates the MESSAGE_RESOURCE_ENTRY for dwMessageId within an already
// locked/decrypted MESSAGE_RESOURCE_DATA buffer. Returns NULL if the
// buffer is NULL (caller passes the WonLockResource result straight
// through, so this also covers "encrypted resource that failed to
// decrypt") or the ID simply isn't present.
static PMESSAGE_RESOURCE_ENTRY FindMessageEntry(PMESSAGE_RESOURCE_DATA pData, DWORD dwMessageId)
{
    if (!pData)
        return NULL;

    for (DWORD i = 0; i < pData->NumberOfBlocks; i++) {
        const MESSAGE_RESOURCE_BLOCK *pBlock = &pData->Blocks[i];
        if (dwMessageId < pBlock->LowId || dwMessageId > pBlock->HighId)
            continue;

        // Entries for LowId..HighId are packed back-to-back with no gaps;
        // walk them one by one until we reach dwMessageId.
        PBYTE p = (PBYTE)pData + pBlock->OffsetToEntries;
        for (DWORD id = pBlock->LowId; id <= pBlock->HighId; id++) {
            PMESSAGE_RESOURCE_ENTRY pEntry = (PMESSAGE_RESOURCE_ENTRY)p;
            if (id == dwMessageId)
                return pEntry;
            p += pEntry->Length;
        }
        break; // IDs never appear in more than one block
    }

    return NULL;
}

DWORD WONAPI WonFormatMessageW(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId,
                               DWORD dwLanguageId, LPWSTR lpBuffer, DWORD nSize,
                               va_list *Arguments)
{
    // Nothing Won-specific to do for FROM_STRING/FROM_SYSTEM/etc. sources.
    if (!(dwFlags & FORMAT_MESSAGE_FROM_HMODULE))
        return FormatMessageW(dwFlags, lpSource, dwMessageId, dwLanguageId, lpBuffer, nSize,
                              Arguments);

    // Per FormatMessage's own documented behavior: a NULL lpSource together
    // with FORMAT_MESSAGE_FROM_HMODULE means "search the current process's
    // own module".
    HINSTANCE hInstance = lpSource ? (HINSTANCE)lpSource : GetModuleHandleW(NULL);

    HRSRC hRsrc = WonFindResourceExW(hInstance, (LPWSTR)RT_MESSAGETABLE, MAKEINTRESOURCEW(1),
                                     (WORD)dwLanguageId);
    if (!hRsrc) {
        SetLastError(ERROR_MR_MID_NOT_FOUND);
        return 0;
    }

    HGLOBAL hGlobal = WonLoadResource(hInstance, hRsrc);
    if (!hGlobal) {
        SetLastError(ERROR_MR_MID_NOT_FOUND);
        return 0;
    }

    PMESSAGE_RESOURCE_ENTRY pEntry =
        FindMessageEntry((PMESSAGE_RESOURCE_DATA)WonLockResource(hGlobal), dwMessageId);
    if (!pEntry) {
        WonFreeResource(hGlobal);
        SetLastError(ERROR_MR_MID_NOT_FOUND);
        return 0;
    }

    LPWSTR pszSrc, pszConverted = NULL;
    if (pEntry->Flags & MESSAGE_RESOURCE_UNICODE) {
        pszSrc = (LPWSTR)pEntry->Text;
    } else {
        int cch = MultiByteToWideChar(CP_ACP, 0, (LPCSTR)pEntry->Text, -1, NULL, 0);
        if (cch <= 0) {
            WonFreeResource(hGlobal);
            SetLastError(ERROR_MR_MID_NOT_FOUND);
            return 0;
        }
        pszConverted = (LPWSTR)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)cch * sizeof(WCHAR));
        if (!pszConverted) {
            WonFreeResource(hGlobal);
            return 0;
        }
        MultiByteToWideChar(CP_ACP, 0, (LPCSTR)pEntry->Text, -1, pszConverted, cch);
        pszSrc = pszConverted;
    }

    // Swap FROM_HMODULE for FROM_STRING; everything else (ALLOCATE_BUFFER,
    // IGNORE_INSERTS, ARGUMENT_ARRAY, the width-mask low byte, ...) passes
    // through untouched so the real engine sees the caller's original intent.
    DWORD dwNewFlags =
        (dwFlags & ~(DWORD)(FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_FROM_SYSTEM)) |
        FORMAT_MESSAGE_FROM_STRING;

    DWORD ret = FormatMessageW(dwNewFlags, pszSrc, 0, 0, lpBuffer, nSize, Arguments);

    if (pszConverted)
        HeapFree(GetProcessHeap(), 0, pszConverted);
    WonFreeResource(hGlobal); // only now: pEntry/pszSrc may point straight into hGlobal above
    return ret;
}

DWORD WONAPI WonFormatMessageA(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId,
                               DWORD dwLanguageId, LPSTR lpBuffer, DWORD nSize,
                               va_list *Arguments)
{
    if (!(dwFlags & FORMAT_MESSAGE_FROM_HMODULE))
        return FormatMessageA(dwFlags, lpSource, dwMessageId, dwLanguageId, lpBuffer, nSize,
                              Arguments);

    HINSTANCE hInstance = lpSource ? (HINSTANCE)lpSource : GetModuleHandleW(NULL);

    HRSRC hRsrc = WonFindResourceExA(hInstance, (LPSTR)RT_MESSAGETABLE, MAKEINTRESOURCEA(1),
                                     (WORD)dwLanguageId);
    if (!hRsrc) {
        SetLastError(ERROR_MR_MID_NOT_FOUND);
        return 0;
    }

    HGLOBAL hGlobal = WonLoadResource(hInstance, hRsrc);
    if (!hGlobal) {
        SetLastError(ERROR_MR_MID_NOT_FOUND);
        return 0;
    }

    PMESSAGE_RESOURCE_ENTRY pEntry =
        FindMessageEntry((PMESSAGE_RESOURCE_DATA)WonLockResource(hGlobal), dwMessageId);
    if (!pEntry) {
        WonFreeResource(hGlobal);
        SetLastError(ERROR_MR_MID_NOT_FOUND);
        return 0;
    }

    LPSTR pszSrc, pszConverted = NULL;
    if (pEntry->Flags & MESSAGE_RESOURCE_UNICODE) {
        int cch =
            WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)pEntry->Text, -1, NULL, 0, NULL, NULL);
        if (cch <= 0) {
            WonFreeResource(hGlobal);
            SetLastError(ERROR_MR_MID_NOT_FOUND);
            return 0;
        }
        pszConverted = (LPSTR)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)cch);
        if (!pszConverted) {
            WonFreeResource(hGlobal);
            return 0;
        }
        WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)pEntry->Text, -1, pszConverted, cch, NULL, NULL);
        pszSrc = pszConverted;
    } else {
        pszSrc = (LPSTR)pEntry->Text;
    }

    DWORD dwNewFlags =
        (dwFlags & ~(DWORD)(FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_FROM_SYSTEM)) |
        FORMAT_MESSAGE_FROM_STRING;

    DWORD ret = FormatMessageA(dwNewFlags, pszSrc, 0, 0, lpBuffer, nSize, Arguments);

    if (pszConverted)
        HeapFree(GetProcessHeap(), 0, pszConverted);
    WonFreeResource(hGlobal);
    return ret;
}
