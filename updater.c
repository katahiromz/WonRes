// updater.c --- Win32 resource updater for WonRes
// Author: katahiromz
// License: MIT
#include <windows.h>
#include <imagehlp.h>
#include <string.h>
#include <time.h>
#include "WonRes.h"
#ifdef WONRES_ENABLE_CRYPTO
#include "WonCryptoP.h"
#endif

// Script: C99/Win32でBeginUpdateResourceWなどのリソース更新関数を再実装してください。
// 実行モジュール・更新モジュールについてx86/x64両方に対応して下さい。
// x86からx64の書き込み、x64からx86の書き込みにも対応してください。

// ヘルパー：アライメント計算
#define ALIGN_UP(size, align) (((size) + (align) - 1) & ~((align) - 1))

// リソースエントリの内部構造体
typedef struct WON_RES_ENTRY {
    LPWSTR type;
    LPWSTR name;
    WORD lang;
    DWORD size;
    LPVOID data;
    struct WON_RES_ENTRY *next;
} WON_RES_ENTRY, *PWON_RES_ENTRY;

// リソース更新用の構造体
typedef struct WON_UPDATE_DATA {
    LPWSTR pFileName;
    BOOL bDeleteExisting;
    PWON_RES_ENTRY pEntries;
} WON_UPDATE_DATA, *PWON_UPDATE_DATA;

// ヘルパー：IDが文字列かどうかを判定
static inline BOOL IsNamedId(LPCWSTR id) { return !IS_INTRESOURCE(id); }

static inline LPWSTR DuplicateString(LPCWSTR psz)
{
    if (!psz)
        return NULL;
    size_t cch = lstrlenW(psz);
    size_t cb = (cch + 1) * sizeof(WCHAR);
    LPWSTR pszNew = HeapAlloc(GetProcessHeap(), 0, cb);
    if (!pszNew)
        return NULL;
    CopyMemory(pszNew, psz, cb);
    return pszNew;
}

// ヘルパー：リソースID/名前の複製
static inline LPWSTR DuplicateResId(LPCWSTR pszId)
{
    if (IS_INTRESOURCE(pszId))
        return (LPWSTR)pszId;
    LPWSTR psz = DuplicateString(pszId);
    if (!psz)
        return NULL;
    return _wcsupr(psz);
}

// ヘルパー：リソースID/名前の解放
static inline void FreeResId(LPWSTR pszId)
{
    if (!IS_INTRESOURCE(pszId))
        HeapFree(GetProcessHeap(), 0, pszId);
}

// リソースIDが一致するか？
static inline BOOL MatchResId(LPCWSTR id1, LPCWSTR id2)
{
    if (IS_INTRESOURCE(id1) && IS_INTRESOURCE(id2))
        return id1 == id2;
    if (!IS_INTRESOURCE(id1) && !IS_INTRESOURCE(id2))
        return _wcsicmp(id1, id2) == 0;
    return FALSE;
}

static int CompareResIdForDirectory(LPCWSTR a, LPCWSTR b)
{
    // 1. 文字列ID（Named）と数値ID（Integer）の比較
    if (IsNamedId(a) && !IsNamedId(b))
        return -1; // 名前が先
    if (!IsNamedId(a) && IsNamedId(b))
        return 1; // IDが後

    // 2. 両方が文字列IDの場合
    if (IsNamedId(a) && IsNamedId(b))
        return _wcsicmp(a, b);

    // 3. 両方が数値IDの場合
    WORD idA = PtrToUshort(a);
    WORD idB = PtrToUshort(b);
    if (idA < idB)
        return -1;
    if (idA > idB)
        return 1;
    return 0;
}

static int CompareResEntry(const void *pa, const void *pb)
{
    PWON_RES_ENTRY a = *(PWON_RES_ENTRY *)pa;
    PWON_RES_ENTRY b = *(PWON_RES_ENTRY *)pb;
    int ret;

    ret = CompareResIdForDirectory(a->type, b->type);
    if (ret)
        return ret;

    ret = CompareResIdForDirectory(a->name, b->name);
    if (ret)
        return ret;

    if (a->lang < b->lang)
        return -1;
    if (a->lang > b->lang)
        return +1;
    return 0;
}

// 実際にリソースを更新する関数
static BOOL WonRealUpdateResource(PWON_UPDATE_DATA pUpdate)
{
    BOOL bSuccess = FALSE;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    PBYTE pOldFile = NULL, pNewFile = NULL, pNewRsrc = NULL;
    DWORD cbOldFile = 0, cbNewFile = 0;
    PWON_RES_ENTRY *ppSorted = NULL;
    PIMAGE_RESOURCE_DATA_ENTRY *ppDataEntries = NULL;

    DWORD count = 0;
    for (PWON_RES_ENTRY p = pUpdate->pEntries; p; p = p->next)
        ++count;

    if (count == 0 && !pUpdate->bDeleteExisting)
        return TRUE;

    // sort entries
    ppSorted = (PWON_RES_ENTRY *)HeapAlloc(GetProcessHeap(), 0,
                                           sizeof(PWON_RES_ENTRY *) * (count ? count : 1));
    if (!ppSorted)
        goto cleanup;

    {
        DWORD i = 0;
        for (PWON_RES_ENTRY p = pUpdate->pEntries; p; p = p->next)
            ppSorted[i++] = p;
    }

    qsort(ppSorted, count, sizeof(PWON_RES_ENTRY *), CompareResEntry);

    // analyze tree
    DWORD cTypes = 0, cbStrings = 0;

    for (DWORD i = 0; i < count; ++i) {
        if (i == 0 || !MatchResId(ppSorted[i]->type, ppSorted[i - 1]->type)) {
            ++cTypes;
            if (IsNamedId(ppSorted[i]->type)) {
                DWORD cb = sizeof(WORD) + (DWORD)(wcslen(ppSorted[i]->type) * sizeof(WCHAR));
                cbStrings += ALIGN_UP(cb, 4);
            }
        }
        if (i == 0 || !MatchResId(ppSorted[i]->type, ppSorted[i - 1]->type) ||
            !MatchResId(ppSorted[i]->name, ppSorted[i - 1]->name)) {
            if (IsNamedId(ppSorted[i]->name)) {
                DWORD cb = sizeof(WORD) + (DWORD)(wcslen(ppSorted[i]->name) * sizeof(WCHAR));
                cbStrings += ALIGN_UP(cb, 4);
            }
        }
    }

    // directory sizing
    DWORD cbRoot =
        sizeof(IMAGE_RESOURCE_DIRECTORY) + cTypes * sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY);
    DWORD cbTypeDirs = 0, cbNameDirs = 0;

    for (DWORD i = 0; i < count;) {
        DWORD iType = i;
        while (i < count && MatchResId(ppSorted[i]->type, ppSorted[iType]->type))
            ++i;

        DWORD cThisNames = 0;
        for (DWORD j = iType; j < i;) {
            DWORD jName = j;
            while (j < i && MatchResId(ppSorted[j]->name, ppSorted[jName]->name))
                ++j;
            ++cThisNames;
            DWORD cLangs = j - jName;
            cbNameDirs +=
                sizeof(IMAGE_RESOURCE_DIRECTORY) + cLangs * sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY);
        }
        cbTypeDirs +=
            sizeof(IMAGE_RESOURCE_DIRECTORY) + cThisNames * sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY);
    }

    DWORD cbDataEntries = count * sizeof(IMAGE_RESOURCE_DATA_ENTRY);
    DWORD offStrings = ALIGN_UP(cbRoot + cbTypeDirs + cbNameDirs + cbDataEntries, 4);
    DWORD offData = ALIGN_UP(offStrings + cbStrings, 8);
    DWORD cbTotal = offData;

    for (DWORD i = 0; i < count; ++i)
        cbTotal += ALIGN_UP(ppSorted[i]->size, 4);

    // allocate resource section buffer
    pNewRsrc = (PBYTE)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cbTotal);
    if (!pNewRsrc)
        goto cleanup;

    size_t cbDataEntriesBuf = sizeof(PIMAGE_RESOURCE_DATA_ENTRY) * (count ? count : 1);
    ppDataEntries = (PIMAGE_RESOURCE_DATA_ENTRY *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                                            cbDataEntriesBuf);
    if (!ppDataEntries)
        goto cleanup;

    // build resource tree
    DWORD offTypeDir = cbRoot, offNameDir = cbRoot + cbTypeDirs;
    DWORD offDataEntry = cbRoot + cbTypeDirs + cbNameDirs;
    DWORD offString = offStrings, offRawData = offData;

    PIMAGE_RESOURCE_DIRECTORY pRoot = (PIMAGE_RESOURCE_DIRECTORY)pNewRsrc;
    PIMAGE_RESOURCE_DIRECTORY_ENTRY pRootEntries =
        (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(pNewRsrc + sizeof(IMAGE_RESOURCE_DIRECTORY));

    pRoot->MajorVersion = 4;
    pRoot->MinorVersion = 0;
    pRoot->TimeDateStamp = (DWORD)time(NULL);
    pRoot->Characteristics = 0;

    DWORD iRootEntry = 0, iDataEntry = 0;

    for (DWORD i = 0; i < count;) {
        DWORD iType = i;
        while (i < count && MatchResId(ppSorted[i]->type, ppSorted[iType]->type))
            ++i;

        DWORD cThisNames = 0;
        for (DWORD j = iType; j < i;) {
            DWORD jName = j;
            while (j < i && MatchResId(ppSorted[j]->name, ppSorted[jName]->name))
                ++j;
            ++cThisNames;
        }

        DWORD thisTypeDirOff = offTypeDir;
        offTypeDir +=
            sizeof(IMAGE_RESOURCE_DIRECTORY) + cThisNames * sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY);

        PIMAGE_RESOURCE_DIRECTORY_ENTRY pTypeEntry = &pRootEntries[iRootEntry++];

        if (IsNamedId(ppSorted[iType]->type)) {
            WORD cch = (WORD)wcslen(ppSorted[iType]->type);
            pTypeEntry->NameOffset = offString;
            pTypeEntry->NameIsString = TRUE;
            *(PWORD)(pNewRsrc + offString) = cch;
            memcpy(pNewRsrc + offString + sizeof(WORD), ppSorted[iType]->type, cch * sizeof(WCHAR));
            offString += sizeof(WORD) + cch * sizeof(WCHAR);
            offString = ALIGN_UP(offString, 4);
            ++pRoot->NumberOfNamedEntries;
        } else {
            pTypeEntry->Id = PtrToUshort(ppSorted[iType]->type);
            ++pRoot->NumberOfIdEntries;
        }

        pTypeEntry->OffsetToData = thisTypeDirOff | IMAGE_RESOURCE_DATA_IS_DIRECTORY;

        PIMAGE_RESOURCE_DIRECTORY pTypeDir = (PIMAGE_RESOURCE_DIRECTORY)(pNewRsrc + thisTypeDirOff);
        ZeroMemory(pTypeDir, sizeof(IMAGE_RESOURCE_DIRECTORY));

        PIMAGE_RESOURCE_DIRECTORY_ENTRY pTypeEntries =
            (PIMAGE_RESOURCE_DIRECTORY_ENTRY)((PBYTE)pTypeDir + sizeof(IMAGE_RESOURCE_DIRECTORY));

        DWORD iTypeEntry = 0;
        for (DWORD j = iType; j < i;) {
            DWORD jName = j;
            while (j < i && MatchResId(ppSorted[j]->name, ppSorted[jName]->name))
                ++j;

            DWORD cLangs = j - jName;
            DWORD thisLangDirOff = offNameDir;
            offNameDir +=
                sizeof(IMAGE_RESOURCE_DIRECTORY) + cLangs * sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY);

            PIMAGE_RESOURCE_DIRECTORY_ENTRY pNameEntry = &pTypeEntries[iTypeEntry++];
            ZeroMemory(pNameEntry, sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY));

            if (IsNamedId(ppSorted[jName]->name)) {
                WORD cch = (WORD)wcslen(ppSorted[jName]->name);
                pNameEntry->NameOffset = offString;
                pNameEntry->NameIsString = TRUE;
                *(PWORD)(pNewRsrc + offString) = cch;
                memcpy(pNewRsrc + offString + sizeof(WORD), ppSorted[jName]->name,
                       cch * sizeof(WCHAR));
                offString += sizeof(WORD) + cch * sizeof(WCHAR);
                offString = ALIGN_UP(offString, 4);
                ++pTypeDir->NumberOfNamedEntries;
            } else {
                pNameEntry->Id = PtrToUshort(ppSorted[jName]->name);
                ++pTypeDir->NumberOfIdEntries;
            }

            pNameEntry->OffsetToData = thisLangDirOff | IMAGE_RESOURCE_DATA_IS_DIRECTORY;

            PIMAGE_RESOURCE_DIRECTORY pLangDir =
                (PIMAGE_RESOURCE_DIRECTORY)(pNewRsrc + thisLangDirOff);
            ZeroMemory(pLangDir, sizeof(IMAGE_RESOURCE_DIRECTORY));

            PIMAGE_RESOURCE_DIRECTORY_ENTRY pLangEntries =
                (PIMAGE_RESOURCE_DIRECTORY_ENTRY)((PBYTE)pLangDir +
                                                  sizeof(IMAGE_RESOURCE_DIRECTORY));

            for (DWORD k = 0; k < cLangs; ++k) {
                PWON_RES_ENTRY pRes = ppSorted[jName + k];

                PIMAGE_RESOURCE_DIRECTORY_ENTRY pLangEntry = &pLangEntries[k];
                pLangEntry->Id = pRes->lang;
                pLangEntry->OffsetToData = offDataEntry;
                ++pLangDir->NumberOfIdEntries;

                PIMAGE_RESOURCE_DATA_ENTRY pDataEntry =
                    (PIMAGE_RESOURCE_DATA_ENTRY)(pNewRsrc + offDataEntry);
                ppDataEntries[iDataEntry++] = pDataEntry;

                pDataEntry->OffsetToData = offRawData;
                pDataEntry->Size = pRes->size;
                pDataEntry->CodePage = 0;
                pDataEntry->Reserved = 0;

                memcpy(pNewRsrc + offRawData, pRes->data, pRes->size);

                offDataEntry += sizeof(IMAGE_RESOURCE_DATA_ENTRY);
                offRawData += ALIGN_UP(pRes->size, 4);
            }
        }
    }

    // load file
    hFile = CreateFileW(pUpdate->pFileName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        goto cleanup;

    cbOldFile = GetFileSize(hFile, NULL);
    if (cbOldFile == INVALID_FILE_SIZE || cbOldFile == 0)
        goto cleanup;

    pOldFile = (PBYTE)HeapAlloc(GetProcessHeap(), 0, cbOldFile);
    if (!pOldFile)
        goto cleanup;

    {
        DWORD cbRead;
        if (!ReadFile(hFile, pOldFile, cbOldFile, &cbRead, NULL) || cbRead != cbOldFile)
            goto cleanup;
    }

    // parse PE (x86/x64 両対応)
    PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)pOldFile;
    if (pDos->e_magic != IMAGE_DOS_SIGNATURE)
        goto cleanup;

    PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(pOldFile + pDos->e_lfanew);
    if (pNt->Signature != IMAGE_NT_SIGNATURE)
        goto cleanup;

    BOOL b64 = (pNt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC);

    DWORD fileAlign, secAlign;
    PIMAGE_SECTION_HEADER pSections;

    if (b64) {
        PIMAGE_NT_HEADERS64 pNt64 = (PIMAGE_NT_HEADERS64)pNt;
        fileAlign = pNt64->OptionalHeader.FileAlignment;
        secAlign = pNt64->OptionalHeader.SectionAlignment;
        pSections = IMAGE_FIRST_SECTION(pNt64);
    } else {
        PIMAGE_NT_HEADERS32 pNt32 = (PIMAGE_NT_HEADERS32)pNt;
        fileAlign = pNt32->OptionalHeader.FileAlignment;
        secAlign = pNt32->OptionalHeader.SectionAlignment;
        pSections = IMAGE_FIRST_SECTION(pNt32);
    }

    WORD nSections = pNt->FileHeader.NumberOfSections;

    // 既存の .rsrc セクションを探す
    int iOldRsrc = -1;

    // まずリソースデータディレクトリの VirtualAddress で照合
    DWORD oldRsrcVA = 0;
    if (b64)
        oldRsrcVA = ((PIMAGE_NT_HEADERS64)pNt)
                        ->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE]
                        .VirtualAddress;
    else
        oldRsrcVA = ((PIMAGE_NT_HEADERS32)pNt)
                        ->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE]
                        .VirtualAddress;

    if (oldRsrcVA != 0) {
        for (WORD i = 0; i < nSections; i++) {
            if (pSections[i].VirtualAddress == oldRsrcVA) {
                iOldRsrc = (int)i;
                break;
            }
        }
    }

    // VA で見つからない場合はセクション名 ".rsrc" で照合
    if (iOldRsrc < 0) {
        for (WORD i = 0; i < nSections; i++) {
            if (memcmp(pSections[i].Name, ".rsrc\0\0\0", IMAGE_SIZEOF_SHORT_NAME) == 0) {
                iOldRsrc = (int)i;
                break;
            }
        }
    }

    // 新セクションの VirtualAddress を計算
    // .rsrc の直前セクション（元VAが .rsrc より小さいもの）の末尾をベースにする。
    // これにより後続セクションとの間にギャップが生じないよう連続配置できる。
    DWORD oldRsrcVAForSplit = (iOldRsrc >= 0) ? pSections[iOldRsrc].VirtualAddress : MAXDWORD;
    DWORD newVA;
    {
        DWORD prevVAEnd = 0;
        for (WORD i = 0; i < nSections; i++) {
            if (i == (WORD)iOldRsrc)
                continue;
            if (pSections[i].VirtualAddress >= oldRsrcVAForSplit)
                continue; // .rsrc 以降のセクションは除外
            DWORD vsz = max(pSections[i].Misc.VirtualSize, pSections[i].SizeOfRawData);
            DWORD end = pSections[i].VirtualAddress + vsz;
            if (end > prevVAEnd)
                prevVAEnd = end;
        }
        // .rsrc が先頭セクション、または .rsrc が存在しない場合のフォールバック
        if (prevVAEnd == 0) {
            for (WORD i = 0; i < nSections; i++) {
                if (i == (WORD)iOldRsrc)
                    continue;
                DWORD vsz = max(pSections[i].Misc.VirtualSize, pSections[i].SizeOfRawData);
                DWORD end = pSections[i].VirtualAddress + vsz;
                if (end > prevVAEnd)
                    prevVAEnd = end;
            }
        }
        if (prevVAEnd == 0 && nSections > 0)
            prevVAEnd = pSections[0].VirtualAddress + pSections[0].SizeOfRawData;
        newVA = ALIGN_UP(prevVAEnd, secAlign);
    }

    // 新セクションのファイルオフセット (newRaw) を計算
    // 既存 .rsrc がファイル末尾にある場合はその位置を再利用し、
    // そうでなければ全セクションの後ろに追加する。
    DWORD newRaw;
    {
        DWORD maxOtherRawEnd = 0;
        for (WORD i = 0; i < nSections; i++) {
            if (i == (WORD)iOldRsrc)
                continue;
            DWORD end = pSections[i].PointerToRawData + pSections[i].SizeOfRawData;
            if (end > maxOtherRawEnd)
                maxOtherRawEnd = end;
        }

        // 既存 .rsrc が他セクションより後ろ（ファイル末尾）なら再利用
        BOOL bReuseOldSlot =
            (iOldRsrc >= 0) && (pSections[iOldRsrc].PointerToRawData >= maxOtherRawEnd);

        if (bReuseOldSlot) {
            // 旧 .rsrc のファイル位置をそのまま使う（旧データは上書きされる）
            newRaw = pSections[iOldRsrc].PointerToRawData;
        } else {
            // 全セクションのファイル末尾に追加
            DWORD maxAllRawEnd = maxOtherRawEnd;
            if (iOldRsrc < 0) {
                // .rsrc がなかった場合は全セクション考慮
                for (WORD i = 0; i < nSections; i++) {
                    DWORD end = pSections[i].PointerToRawData + pSections[i].SizeOfRawData;
                    if (end > maxAllRawEnd)
                        maxAllRawEnd = end;
                }
            }
            newRaw = ALIGN_UP(maxAllRawEnd, fileAlign);
        }
    }

    DWORD cbRaw = ALIGN_UP(cbTotal, fileAlign);
    cbNewFile = newRaw + cbRaw;

    pNewFile = (PBYTE)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cbNewFile);
    if (!pNewFile)
        goto cleanup;

    // 旧ファイルの内容を新バッファへコピー（新 .rsrc 開始位置まで）
    // bReuseOldSlot の場合: newRaw = 旧 .rsrc 先頭 → 旧データを含まずにコピー
    // 追加の場合          : newRaw >= cbOldFile → 旧ファイル全体をコピー
    {
        DWORD cbCopy = (newRaw < cbOldFile) ? newRaw : cbOldFile;
        memcpy(pNewFile, pOldFile, cbCopy);
    }

    // refresh pointers（pNewFile ベースで再取得）
    pDos = (PIMAGE_DOS_HEADER)pNewFile;
    pNt = (PIMAGE_NT_HEADERS)(pNewFile + pDos->e_lfanew);
    if (b64)
        pSections = IMAGE_FIRST_SECTION((PIMAGE_NT_HEADERS64)pNt);
    else
        pSections = IMAGE_FIRST_SECTION((PIMAGE_NT_HEADERS32)pNt);

    // 既存 .rsrc セクションヘッダを更新、なければ新規追加
    PIMAGE_SECTION_HEADER pRsrcSec;

    if (iOldRsrc >= 0) {
        // 既存ヘッダを再利用（NumberOfSections は変えない）
        pRsrcSec = &pSections[iOldRsrc];
    } else {
        // 新規追加：ヘッダ領域に空きがあるか確認
        PBYTE pSecEnd = (PBYTE)&pSections[nSections + 1];
        DWORD cbHeaders = b64 ? ((PIMAGE_NT_HEADERS64)pNt)->OptionalHeader.SizeOfHeaders
                              : ((PIMAGE_NT_HEADERS32)pNt)->OptionalHeader.SizeOfHeaders;
        if ((DWORD)(pSecEnd - pNewFile) > cbHeaders)
            goto cleanup;

        pRsrcSec = &pSections[nSections];
        ZeroMemory(pRsrcSec, sizeof(*pRsrcSec));
        ++pNt->FileHeader.NumberOfSections;
    }

    // セクションヘッダを設定
    memcpy(pRsrcSec->Name, ".rsrc\0\0\0", 8);
    pRsrcSec->Misc.VirtualSize = cbTotal;
    pRsrcSec->VirtualAddress = newVA;
    pRsrcSec->SizeOfRawData = cbRaw;
    pRsrcSec->PointerToRawData = newRaw;
    pRsrcSec->PointerToRelocations = 0;
    pRsrcSec->PointerToLinenumbers = 0;
    pRsrcSec->NumberOfRelocations = 0;
    pRsrcSec->NumberOfLinenumbers = 0;
    pRsrcSec->Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ;

    // patch RVA：各 IMAGE_RESOURCE_DATA_ENTRY の OffsetToData に newVA を加算
    for (DWORD i = 0; i < count; ++i)
        ppDataEntries[i]->OffsetToData += newVA;

    // copy .rsrc
    CopyMemory(pNewFile + newRaw, pNewRsrc, cbTotal);

    // 後続セクション（元VAが旧 .rsrc より大きかったもの）のVAを
    // 新 .rsrc の末尾から連続するよう再配置する。
    // これにより ReactOS のセクション連続性チェック（section.c:811）を満たす。
    // ※ リロケーション情報を持つセクションは通常 .reloc のみで、
    //   そのセクション自体の内容は変わらないため RVA の書き換えは不要。
    DWORD sizeImage;
    {
        DWORD nextVA = ALIGN_UP(newVA + ALIGN_UP(cbTotal, secAlign), secAlign);
        for (WORD i = 0; i < pNt->FileHeader.NumberOfSections; i++) {
            if (&pSections[i] == pRsrcSec)
                continue;
            if (pSections[i].VirtualAddress <= oldRsrcVAForSplit)
                continue; // .rsrc より前のセクションはそのまま
            DWORD vsz = max(pSections[i].Misc.VirtualSize, pSections[i].SizeOfRawData);
            pSections[i].VirtualAddress = nextVA;
            nextVA = ALIGN_UP(nextVA + ALIGN_UP(vsz, secAlign), secAlign);
        }
        // SizeOfImage は全セクション中の最大VA末尾から算出
        DWORD maxVAEnd = 0;
        for (WORD i = 0; i < pNt->FileHeader.NumberOfSections; i++) {
            DWORD vsz = max(pSections[i].Misc.VirtualSize, pSections[i].SizeOfRawData);
            DWORD end = pSections[i].VirtualAddress + vsz;
            if (end > maxVAEnd)
                maxVAEnd = end;
        }
        sizeImage = ALIGN_UP(maxVAEnd, secAlign);
    }

    // update PE headers
    if (b64) {
        PIMAGE_NT_HEADERS64 pNt64 = (PIMAGE_NT_HEADERS64)pNt;
        pNt64->OptionalHeader.SizeOfImage = sizeImage;
        pNt64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress = newVA;
        pNt64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].Size = cbTotal;
        pNt64->OptionalHeader.CheckSum = 0;
    } else {
        PIMAGE_NT_HEADERS32 pNt32 = (PIMAGE_NT_HEADERS32)pNt;
        pNt32->OptionalHeader.SizeOfImage = sizeImage;
        pNt32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress = newVA;
        pNt32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].Size = cbTotal;
        pNt32->OptionalHeader.CheckSum = 0;
    }

    // チェックサムを計算して設定
    DWORD dwHeaderSum, dwCheckSum;
    if (CheckSumMappedFile(pNewFile, cbNewFile, &dwHeaderSum, &dwCheckSum)) {
        if (b64) {
            ((PIMAGE_NT_HEADERS64)pNt)->OptionalHeader.CheckSum = dwCheckSum;
        } else {
            ((PIMAGE_NT_HEADERS32)pNt)->OptionalHeader.CheckSum = dwCheckSum;
        }
    }

    // write file
    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    {
        DWORD cbWritten;
        if (!WriteFile(hFile, pNewFile, cbNewFile, &cbWritten, NULL) || cbWritten != cbNewFile)
            goto cleanup;
    }

    // ファイルを新サイズに切り詰める（旧 .rsrc を再利用した場合にファイルが縮小する）
    SetFilePointer(hFile, cbNewFile, NULL, FILE_BEGIN);
    SetEndOfFile(hFile);

    bSuccess = TRUE;

cleanup:
    if (ppDataEntries)
        HeapFree(GetProcessHeap(), 0, ppDataEntries);
    if (ppSorted)
        HeapFree(GetProcessHeap(), 0, ppSorted);
    if (pNewRsrc)
        HeapFree(GetProcessHeap(), 0, pNewRsrc);
    if (pOldFile)
        HeapFree(GetProcessHeap(), 0, pOldFile);
    if (pNewFile)
        HeapFree(GetProcessHeap(), 0, pNewFile);
    if (hFile != INVALID_HANDLE_VALUE)
        CloseHandle(hFile);

    return bSuccess;
}

// 既存リソース読み込み用のコールバック
static BOOL CALLBACK LoadExistingResProc(HMODULE hMod, LPCWSTR lpType, LPCWSTR lpName, WORD wLang,
                                         LONG_PTR lParam)
{
    HANDLE hUpdate = (HANDLE)lParam;
    HRSRC hRes = WonFindResourceExW(hMod, lpType, lpName, wLang);
    if (hRes) {
        DWORD size = WonSizeofResource(hMod, hRes);
        HGLOBAL hGlobal = WonLoadResource(hMod, hRes);
        if (!hGlobal)
            return FALSE;

        LPVOID pData = WonLockResource(hGlobal);
        BOOL bOk = pData && WonUpdateResourceW(hUpdate, lpType, lpName, wLang, pData, size);

        // pData either aliases the module image (nothing to free) or, if
        // this source module itself has WonRes-encrypted resources, is a
        // decrypted heap buffer -- either way WonUpdateResourceW above has
        // already copied whatever it needed out of pData, so it's safe to
        // release now regardless of success/failure.
        WonFreeResource(hGlobal);
        return bOk;
    }
    return TRUE;
}

static BOOL CALLBACK LoadExistingNamesProc(HMODULE hMod, LPCWSTR lpType, LPWSTR lpName,
                                           LONG_PTR lParam)
{
    return WonEnumResourceLanguagesW(hMod, lpType, lpName, LoadExistingResProc, lParam);
}

static BOOL CALLBACK LoadExistingTypesProc(HMODULE hMod, LPWSTR lpType, LONG_PTR lParam)
{
    return WonEnumResourceNamesW(hMod, lpType, LoadExistingNamesProc, lParam);
}

HANDLE WONAPI WonBeginUpdateResourceW(LPCWSTR pFileName, BOOL bDeleteExistingResources)
{
    PWON_UPDATE_DATA pUpdate =
        (PWON_UPDATE_DATA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(WON_UPDATE_DATA));
    if (!pUpdate)
        return NULL;

    if (!pFileName || !pFileName[0])
    {
        HeapFree(GetProcessHeap(), 0, pUpdate);
        return NULL;
    }

    pUpdate->pFileName = DuplicateString(pFileName);
    pUpdate->bDeleteExisting = bDeleteExistingResources;
    if (!pUpdate->pFileName)
    {
        HeapFree(GetProcessHeap(), 0, pUpdate);
        return NULL;
    }

    HMODULE hMod = LoadLibraryExW(pFileName, NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (!hMod) {
        HeapFree(GetProcessHeap(), 0, pUpdate->pFileName);
        HeapFree(GetProcessHeap(), 0, pUpdate);
        return NULL;
    }

    if (!bDeleteExistingResources) {
        // 既存のリソースをすべて内部リストにロードする
        if (!WonEnumResourceTypesW(hMod, LoadExistingTypesProc, (LONG_PTR)pUpdate)) {
            FreeLibrary(hMod);
            // WonEndUpdateResourceW(..., fDiscard=TRUE) と同じ後始末を
            // 借りて中断する: pFileName に加え、失敗するまでに部分的に
            // 集まっていた pEntries（type/name/data 込み）も一緒に、漏れなく
            // 解放できる。以前はここで pUpdate 自体しか解放しておらず、
            // pFileName・pEntries・hMod (FreeLibrary 漏れ) がリークしていた。
            WonEndUpdateResourceW((HANDLE)pUpdate, TRUE);
            return NULL;
        }
    }

    FreeLibrary(hMod);

    return (HANDLE)pUpdate;
}

BOOL WONAPI WonUpdateResourceW(HANDLE hUpdate, LPCWSTR lpType, LPCWSTR lpName, WORD wLanguage,
                               LPVOID lpData, DWORD cbData)
{
    PWON_UPDATE_DATA pUpdate = (PWON_UPDATE_DATA)hUpdate;
    if (!pUpdate)
        return FALSE;

    // 同一リソース（Type, Name, Lang）があるか確認
    PWON_RES_ENTRY *ppNext = &pUpdate->pEntries;
    while (*ppNext) {
        PWON_RES_ENTRY pCurr = *ppNext;
        BOOL typeMatch = MatchResId(lpType, pCurr->type);
        BOOL nameMatch = MatchResId(lpName, pCurr->name);
        if (typeMatch && nameMatch && pCurr->lang == wLanguage) {
            // 一致した場合、既存のものを削除
            PWON_RES_ENTRY pDelete = pCurr;
            *ppNext = pCurr->next;
            FreeResId(pDelete->type);
            FreeResId(pDelete->name);
            if (pDelete->data)
                HeapFree(GetProcessHeap(), 0, pDelete->data);
            HeapFree(GetProcessHeap(), 0, pDelete);
            break;
        }
        ppNext = &pCurr->next;
    }

    // lpData が NULL でない場合は新規追加・更新
    if (lpData) {
        PWON_RES_ENTRY pNew =
            (PWON_RES_ENTRY)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(WON_RES_ENTRY));
        if (!pNew)
            return FALSE;

        pNew->type = DuplicateResId(lpType);
        pNew->name = DuplicateResId(lpName);
        if ((!IS_INTRESOURCE(lpType) && !pNew->type) || (!IS_INTRESOURCE(lpName) && !pNew->name)) {
            FreeResId(pNew->type);
            FreeResId(pNew->name);
            HeapFree(GetProcessHeap(), 0, pNew);
            return FALSE;
        }
        pNew->lang = wLanguage;
        pNew->size = cbData;
        if (cbData) {
            pNew->data = HeapAlloc(GetProcessHeap(), 0, cbData);
            if (!pNew->data) {
                FreeResId(pNew->type);
                FreeResId(pNew->name);
                HeapFree(GetProcessHeap(), 0, pNew);
                return FALSE;
            }
            CopyMemory(pNew->data, lpData, cbData);
        }

        // リストの先頭に追加
        pNew->next = pUpdate->pEntries;
        pUpdate->pEntries = pNew;
    }

    return TRUE;
}

BOOL WONAPI WonEndUpdateResourceW(HANDLE hUpdate, BOOL fDiscard)
{
    PWON_UPDATE_DATA pUpdate = (PWON_UPDATE_DATA)hUpdate;
    if (!pUpdate)
        return FALSE;

    BOOL ret = TRUE;
    if (!fDiscard)
        ret = WonRealUpdateResource(pUpdate);

    // クリーンアップ
    PWON_RES_ENTRY pCurr = pUpdate->pEntries;
    while (pCurr) {
        PWON_RES_ENTRY pNext = pCurr->next;
        FreeResId(pCurr->type);
        FreeResId(pCurr->name);
        if (pCurr->data)
            HeapFree(GetProcessHeap(), 0, pCurr->data);
        HeapFree(GetProcessHeap(), 0, pCurr);
        pCurr = pNext;
    }
    HeapFree(GetProcessHeap(), 0, pUpdate->pFileName);
    HeapFree(GetProcessHeap(), 0, pUpdate);
    return ret;
}

HANDLE WONAPI WonBeginUpdateResourceA(LPCSTR pFileName, BOOL bDeleteExistingResources)
{
    WCHAR szFileNameW[MAX_PATH];
    if (!MultiByteToWideChar(CP_ACP, 0, pFileName, -1, szFileNameW, _countof(szFileNameW)))
        return NULL;
    szFileNameW[_countof(szFileNameW) - 1] = UNICODE_NULL;
    return WonBeginUpdateResourceW(szFileNameW, bDeleteExistingResources);
}

BOOL WONAPI WonUpdateResourceA(HANDLE hUpdate, LPCSTR lpType, LPCSTR lpName, WORD wLanguage,
                               LPVOID lpData, DWORD cbData)
{
    WCHAR szTypeW[MAX_RES_ID_LEN], szNameW[MAX_RES_ID_LEN];
    LPWSTR pszTypeW, pszNameW;

    if (IS_INTRESOURCE(lpType)) {
        pszTypeW = MAKEINTRESOURCEW(PtrToUshort(lpType));
    } else {
        if (!MultiByteToWideChar(CP_ACP, 0, lpType, -1, szTypeW, _countof(szTypeW)))
            return FALSE;

        szTypeW[_countof(szTypeW) - 1] = UNICODE_NULL;
        pszTypeW = szTypeW;
    }

    if (IS_INTRESOURCE(lpName)) {
        pszNameW = MAKEINTRESOURCEW(PtrToUshort(lpName));
    } else {
        if (!MultiByteToWideChar(CP_ACP, 0, lpName, -1, szNameW, _countof(szNameW)))
            return FALSE;

        szNameW[_countof(szNameW) - 1] = UNICODE_NULL;
        pszNameW = szNameW;
    }

    return WonUpdateResourceW(hUpdate, pszTypeW, pszNameW, wLanguage, lpData, cbData);
}

BOOL WONAPI WonEndUpdateResourceA(HANDLE hUpdate, BOOL fDiscard)
{
    return WonEndUpdateResourceW(hUpdate, fDiscard);
}

#ifdef WONRES_ENABLE_CRYPTO
////////////////////////////////////////////////////////////////////////////////////
// Encrypted resource writing

BOOL WONAPI WonUpdateResourceEncryptedW(HANDLE hUpdate, LPCWSTR lpType, LPCWSTR lpName,
                                        WORD wLanguage, LPVOID lpData, DWORD cbData)
{
    // A NULL/zero-length payload deletes the resource, same as
    // WonUpdateResourceW -- nothing to encrypt in that case.
    if (!lpData || cbData == 0)
        return WonUpdateResourceW(hUpdate, lpType, lpName, wLanguage, lpData, cbData);

    PBYTE pbBlob = NULL;
    DWORD cbBlob = 0;
    if (!WonCryptEncryptBuffer((const BYTE *)lpData, cbData, &pbBlob, &cbBlob))
        return FALSE; // no encryption key configured, or an internal failure

    BOOL bResult = WonUpdateResourceW(hUpdate, lpType, lpName, wLanguage, pbBlob, cbBlob);
    HeapFree(GetProcessHeap(), 0, pbBlob);
    return bResult;
}

BOOL WONAPI WonUpdateResourceEncryptedA(HANDLE hUpdate, LPCSTR lpType, LPCSTR lpName,
                                        WORD wLanguage, LPVOID lpData, DWORD cbData)
{
    WCHAR szTypeW[MAX_RES_ID_LEN], szNameW[MAX_RES_ID_LEN];
    LPWSTR pszTypeW, pszNameW;

    if (IS_INTRESOURCE(lpType)) {
        pszTypeW = MAKEINTRESOURCEW(PtrToUshort(lpType));
    } else {
        if (!MultiByteToWideChar(CP_ACP, 0, lpType, -1, szTypeW, _countof(szTypeW)))
            return FALSE;
        szTypeW[_countof(szTypeW) - 1] = UNICODE_NULL;
        pszTypeW = szTypeW;
    }

    if (IS_INTRESOURCE(lpName)) {
        pszNameW = MAKEINTRESOURCEW(PtrToUshort(lpName));
    } else {
        if (!MultiByteToWideChar(CP_ACP, 0, lpName, -1, szNameW, _countof(szNameW)))
            return FALSE;
        szNameW[_countof(szNameW) - 1] = UNICODE_NULL;
        pszNameW = szNameW;
    }

    return WonUpdateResourceEncryptedW(hUpdate, pszTypeW, pszNameW, wLanguage, lpData, cbData);
}
#endif
