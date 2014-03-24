/*

Miranda NG: the free IM client for Microsoft* Windows*

Copyright (c) 2012-14 Miranda NG project (http://miranda-ng.org),
Copyright (c) 2000-12 Miranda IM project,
all portions of this codebase are copyrighted to the people
listed in contributors.txt.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "commonheaders.h"
#include "../modules/langpack/langpack.h"

#define LANGPACK_BUF_SIZE 4000

static int CompareMuuids(const MUUID *p1, const MUUID *p2)
{
	return memcmp(p1, p2, sizeof(MUUID));
}

static LIST<MUUID> lMuuids(10, CompareMuuids);
static MUUID *pCurrentMuuid = NULL;
static HANDLE hevChanged = 0;

static BOOL bModuleInitialized = FALSE;

struct LangPackEntry
{
	DWORD englishHash;
	char *szLocal;
	wchar_t *wszLocal;
	MUUID *pMuuid;
	LangPackEntry* pNext;  // for langpack items with the same hash value
};

static LANGPACK_INFO langPack;

static LangPackEntry *g_pEntries;
static int g_entryCount, g_entriesAlloced;

static int IsEmpty(const char *str)
{
	for (int i = 0; str[i]; i++)
		if (str[i] != ' ' && str[i] != '\r' && str[i] != '\n')
			return 0;

	return 1;
}

static int ConvertBackslashes(char *str, UINT fileCp)
{
	int shift = 0;
	char *pstr;
	for (pstr = str; *pstr; pstr = CharNextExA(fileCp, pstr, 0)) {
		if (*pstr == '\\') {
			shift++;
			switch (pstr[1]) {
				case 'n': *pstr = '\n'; break;
				case 't': *pstr = '\t'; break;
				case 'r': *pstr = '\r'; break;
				case 's': *pstr = ' ';  break;
				default:  *pstr = pstr[1]; break;
			}
			memmove(pstr + 1, pstr + 2, strlen(pstr + 2) + 1);
		}
	}
	return shift;
}

#ifdef _DEBUG
//#pragma optimize("gt", on)
#endif

// MurmurHash2
MIR_CORE_DLL(unsigned int) mir_hash(const void * key, unsigned int len)
{
	// 'm' and 'r' are mixing constants generated offline.
	// They're not really 'magic', they just happen to work well.
	const unsigned int m = 0x5bd1e995;
	const int r = 24;

	// Initialize the hash to a 'random' value
	unsigned int h = len;

	// Mix 4 bytes at a time into the hash
	const unsigned char *data = (const unsigned char*)key;

	while (len >= 4) {
		unsigned int k = *(unsigned int*)data;

		k *= m;
		k ^= k >> r;
		k *= m;

		h *= m;
		h ^= k;

		data += 4;
		len -= 4;
	}

	// Handle the last few bytes of the input array
	switch (len) {
	case 3: h ^= data[2] << 16;
	case 2: h ^= data[1] << 8;
	case 1: h ^= data[0];
		h *= m;
	}

	// Do a few final mixes of the hash to ensure the last few
	// bytes are well-incorporated.
	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;

	return h;
}

static unsigned int __fastcall hashstrW(const char *key)
{
	if (key == NULL) return 0;
	const unsigned int len = (unsigned int)wcslen((const wchar_t*)key);
	char *buf = (char*)alloca(len + 1);
	for (unsigned i = 0; i <= len; ++i)
		buf[i] = key[i << 1];
	return mir_hash(buf, len);
}

static int SortLangPackHashesProc(LangPackEntry *arg1, LangPackEntry *arg2)
{
	if (arg1->englishHash < arg2->englishHash) return -1;
	if (arg1->englishHash > arg2->englishHash) return 1;

	return (arg1->pMuuid < arg2->pMuuid) ? -1 : 1;
}

static void swapBytes(void *p, size_t iSize)
{
	char *head = (char*)p; // here
	char *tail = head + iSize - 1;

	for (; tail > head; --tail, ++head) {
		char temp = *head;
		*head = *tail;
		*tail = temp;
	}
}

static bool EnterMuuid(const char *p, MUUID &result)
{
	if (*p++ != '{')
		return false;

	BYTE *d = (BYTE*)&result;

	for (int nBytes = 0; *p && nBytes < 24; p++) {
		if (*p == '-')
			continue;

		if (*p == '}')
			break;

		if (!isxdigit(*p))
			return false;

		if (!isxdigit(p[1]))
			return false;

		int c = 0;
		if (sscanf(p, "%2x", &c) != 1)
			return false;

		*d++ = (BYTE)c;
		nBytes++;
		p++;
	}

	if (*p != '}')
		return false;

	swapBytes(&result.a, sizeof(result.a));
	swapBytes(&result.b, sizeof(result.b));
	swapBytes(&result.c, sizeof(result.c));
	return true;
}

static void LoadLangPackFile(FILE *fp, char *line, UINT fileCp)
{
	while (!feof(fp)) {
		if (fgets(line, LANGPACK_BUF_SIZE, fp) == NULL)
			break;

		if (IsEmpty(line) || line[0] == ';' || line[0] == 0)
			continue;

		rtrim(line);

		if (line[0] == '#') {
			strlwr(line);

			if (!memcmp(line + 1, "include", 7)) {
				TCHAR tszFileName[MAX_PATH];
				TCHAR	*p = _tcsrchr(langPack.tszFullPath, '\\');
				if (p)
					*p = 0;
				mir_sntprintf(tszFileName, SIZEOF(tszFileName), _T("%s\\%S"), langPack.tszFullPath, ltrim(line + 9));
				if (p)
					*p = '\\';

				FILE *fp = _tfopen(tszFileName, _T("r"));
				if (fp) {
					line[0] = 0;
					fgets(line, LANGPACK_BUF_SIZE, fp);

					UINT fileCp = CP_ACP;
					if (strlen(line) >= 3 && line[0] == '\xef' && line[1] == '\xbb' && line[2] == '\xbf') {
						fileCp = CP_UTF8;
						fseek(fp, 3, SEEK_SET);
					}
					else {
						fileCp = langPack.codepage;
						fseek(fp, 0, SEEK_SET);
					}

					LoadLangPackFile(fp, line, fileCp);
					fclose(fp);
				}
			}
			else if (!memcmp(line + 1, "muuid", 5)) {
				MUUID t;
				if (!EnterMuuid(line + 7, t))
					continue;

				MUUID *pNew = (MUUID*)mir_alloc(sizeof(MUUID));
				memcpy(pNew, &t, sizeof(t));
				lMuuids.insert(pNew);
				pCurrentMuuid = pNew;
			}

			continue;
		}

		char cFirst = line[0];

		ConvertBackslashes(line, fileCp);

		size_t cbLen = strlen(line) - 1;
		if (cFirst == '[' && line[cbLen] == ']') {
			if (g_entryCount && g_pEntries[g_entryCount-1].wszLocal == NULL)
				g_entryCount--;

			char *pszLine = line + 1;
			line[cbLen] = '\0';
			if (++g_entryCount > g_entriesAlloced) {
				g_entriesAlloced += 128;
				g_pEntries = (LangPackEntry*)mir_realloc(g_pEntries, sizeof(LangPackEntry)*g_entriesAlloced);
			}

			LangPackEntry *E = &g_pEntries[g_entryCount - 1];
			E->englishHash = mir_hashstr(pszLine);
			E->szLocal = NULL;
			E->wszLocal = NULL;
			E->pMuuid = pCurrentMuuid;
			E->pNext = NULL;
			continue;
		}

		if (!g_entryCount)
			continue;

		LangPackEntry *E = &g_pEntries[g_entryCount - 1];
		int iNeeded = MultiByteToWideChar(fileCp, 0, line, -1, 0, 0), iOldLen;
		if (E->wszLocal == NULL) {
			iOldLen = 0;
			E->wszLocal = (wchar_t *)mir_alloc((iNeeded + 1) * sizeof(wchar_t));
			MultiByteToWideChar(fileCp, 0, line, -1, E->wszLocal, iNeeded);
		}
		else {
			iOldLen = (int)wcslen(E->wszLocal);
			E->wszLocal = (wchar_t*)mir_realloc(E->wszLocal, (sizeof(wchar_t)* (iOldLen + iNeeded + 2)));
			E->wszLocal[iOldLen++] = '\n';
		}
		MultiByteToWideChar(fileCp, 0, line, -1, E->wszLocal + iOldLen, iNeeded);
	}
}

static int LoadLangDescr(LANGPACK_INFO &lpinfo, FILE *fp, char *line, int &startOfLine, UINT &fileCp)
{
	char szLanguage[64]; szLanguage[0] = 0;

	fgets(line, LANGPACK_BUF_SIZE, fp);
	size_t lineLen = strlen(line);
	if (lineLen >= 3 && line[0] == '\xef' && line[1] == '\xbb' && line[2] == '\xbf') {
		fileCp = CP_UTF8;
		memmove(line, line + 3, lineLen - 2);
	}

	lrtrim(line);
	if (lstrcmpA(line, "Miranda Language Pack Version 1"))
		return 2;

	// headers
	while (!feof(fp)) {
		startOfLine = ftell(fp);
		if (fgets(line, LANGPACK_BUF_SIZE, fp) == NULL)
			break;

		lrtrim(line);
		if (IsEmpty(line) || line[0] == ';' || line[0] == 0)
			continue;

		if (line[0] == '[' || line[0] == '#')
			break;

		char *pszColon = strchr(line, ':');
		if (pszColon == NULL)
			return 3;

		*pszColon++ = 0;
		if (!lstrcmpA(line, "Language")) { mir_snprintf(szLanguage, sizeof(szLanguage), "%s", pszColon); lrtrim(szLanguage); }
		else if (!lstrcmpA(line, "Last-Modified-Using")) { mir_snprintf(lpinfo.szLastModifiedUsing, sizeof(lpinfo.szLastModifiedUsing), "%s", pszColon); lrtrim(lpinfo.szLastModifiedUsing); }
		else if (!lstrcmpA(line, "Authors")) { mir_snprintf(lpinfo.szAuthors, sizeof(lpinfo.szAuthors), "%s", pszColon); lrtrim(lpinfo.szAuthors); }
		else if (!lstrcmpA(line, "Author-email")) { mir_snprintf(lpinfo.szAuthorEmail, sizeof(lpinfo.szAuthorEmail), "%s", pszColon); lrtrim(lpinfo.szAuthorEmail); }
		else if (!lstrcmpA(line, "Locale")) {
			char szBuf[20], *stopped;

			lrtrim(pszColon + 1);
			USHORT langID = (USHORT)strtol(pszColon, &stopped, 16);
			lpinfo.Locale = MAKELCID(langID, 0);
			GetLocaleInfoA(lpinfo.Locale, LOCALE_IDEFAULTANSICODEPAGE, szBuf, 10);
			szBuf[5] = 0;                       // codepages have max. 5 digits
			lpinfo.codepage = atoi(szBuf);
			if (fileCp == CP_ACP)
				fileCp = lpinfo.codepage;
		}
	}

	MultiByteToWideChar(lpinfo.codepage, 0, szLanguage, -1, lpinfo.tszLanguage, SIZEOF(lpinfo.tszLanguage));

	if (!lpinfo.tszLanguage[0] && (lpinfo.Locale == 0) || !GetLocaleInfo(lpinfo.Locale, LOCALE_SENGLANGUAGE, lpinfo.tszLanguage, sizeof(lpinfo.tszLanguage))) {
		TCHAR *p = _tcschr(lpinfo.tszFileName, '_');
		lstrcpyn(lpinfo.tszLanguage, p != NULL ? p + 1 : lpinfo.tszFileName, sizeof(lpinfo.tszLanguage));
		p = _tcsrchr(lpinfo.tszLanguage, _T('.'));
		if (p != NULL) *p = '\0';
	}
	return 0;
}

MIR_CORE_DLL(int) LoadLangPack(const TCHAR *szLangPack)
{
	if (!lstrcmp(szLangPack, langPack.tszFullPath))
		return 0;

	if (g_entryCount)
		UnloadLangPackModule();

	_tcsncpy_s(langPack.tszFullPath, SIZEOF(langPack.tszFullPath), szLangPack, _TRUNCATE);
	TCHAR *p = _tcsrchr(langPack.tszFullPath, '\\');
	_tcsncpy_s(langPack.tszFileName, SIZEOF(langPack.tszFullPath), (p == NULL) ? szLangPack : p + 1, _TRUNCATE);
	CharLower(langPack.tszFileName);

	FILE *fp = _tfopen(szLangPack, _T("rt"));
	if (fp == NULL)
		return 1;

	char line[LANGPACK_BUF_SIZE] = "";
	UINT fileCp = CP_ACP;
	int startOfLine = 0;
	if (LoadLangDescr(langPack, fp, line, startOfLine, fileCp)) {
		fclose(fp);
		return 1;
	}

	// body
	fseek(fp, startOfLine, SEEK_SET);

	LoadLangPackFile(fp, line, fileCp);
	fclose(fp);
	pCurrentMuuid = NULL;

	qsort(g_pEntries, g_entryCount, sizeof(LangPackEntry), (int(*)(const void*, const void*))SortLangPackHashesProc);
	return 0;
}

MIR_CORE_DLL(int) LoadLangPackDescr(const TCHAR *szLangPack, LANGPACK_INFO *lpInfo)
{
	if (lpInfo == NULL)
		return 1;

	_tcsncpy_s(lpInfo->tszFullPath, SIZEOF(lpInfo->tszFullPath), szLangPack, _TRUNCATE);
	TCHAR *p = _tcsrchr(lpInfo->tszFullPath, '\\');
	_tcsncpy_s(lpInfo->tszFileName, SIZEOF(lpInfo->tszFullPath), (p == NULL) ? szLangPack : p+1, _TRUNCATE);
	CharLower(lpInfo->tszFileName);

	FILE *fp = _tfopen(szLangPack, _T("rt"));
	if (fp == NULL)
		return 1;

	char line[LANGPACK_BUF_SIZE] = "";
	UINT fileCp = CP_ACP;
	int startOfLine = 0;
	if (LoadLangDescr(*lpInfo, fp, line, startOfLine, fileCp)) {
		fclose(fp);
		return 1;
	}

	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

static int SortLangPackHashesProc2(LangPackEntry *arg1, LangPackEntry *arg2)
{
	if (arg1->englishHash < arg2->englishHash) return -1;
	if (arg1->englishHash > arg2->englishHash) return 1;
	return 0;
}

static char *LangPackTranslateString(MUUID *pUuid, const char *szEnglish, const int W)
{
	if (g_entryCount == 0 || szEnglish == NULL)
		return (char*)szEnglish;

	LangPackEntry key, *entry;
	key.englishHash = W ? hashstrW(szEnglish) : mir_hashstr(szEnglish);
	entry = (LangPackEntry*)bsearch(&key, g_pEntries, g_entryCount, sizeof(LangPackEntry), (int(*)(const void*, const void*))SortLangPackHashesProc2);
	if (entry == NULL)
		return (char*)szEnglish;

	// try to find the exact match, otherwise the first entry will be returned
	if (pUuid) {
		for (LangPackEntry *p = entry->pNext; p != NULL; p = p->pNext) {
			if (p->pMuuid == pUuid) {
				entry = p;
				break;
			}
		}
	}

	if (W)
		return (char*)entry->wszLocal;

	if (entry->szLocal == NULL && entry->wszLocal != NULL)
		entry->szLocal = mir_u2a_cp(entry->wszLocal, langPack.codepage);
	return entry->szLocal;
}

MIR_CORE_DLL(int) Langpack_GetDefaultCodePage()
{
	return langPack.codepage;
}

MIR_CORE_DLL(int) Langpack_GetDefaultLocale()
{
	return (langPack.Locale == 0) ? LOCALE_USER_DEFAULT : langPack.Locale;
}

MIR_CORE_DLL(TCHAR*) Langpack_PcharToTchar(const char *pszStr)
{
	if (pszStr == NULL)
		return NULL;

	int len = (int)strlen(pszStr);
	TCHAR *result = (TCHAR*)alloca((len + 1)*sizeof(TCHAR));
	MultiByteToWideChar(Langpack_GetDefaultCodePage(), 0, pszStr, -1, result, len);
	result[len] = 0;
	return mir_wstrdup(TranslateW(result));
}

/////////////////////////////////////////////////////////////////////////////////////////

MIR_CORE_DLL(char*) TranslateA_LP(const char *str, int hLangpack)
{
	return (char*)LangPackTranslateString(Langpack_LookupUuid(hLangpack), str, FALSE);
}

MIR_CORE_DLL(WCHAR*) TranslateW_LP(const WCHAR *str, int hLangpack)
{
	return (WCHAR*)LangPackTranslateString(Langpack_LookupUuid(hLangpack), (LPCSTR)str, TRUE);
}

MIR_CORE_DLL(void) TranslateMenu_LP(HMENU hMenu, int hLangpack)
{
	MUUID *uuid = Langpack_LookupUuid(hLangpack);

	MENUITEMINFO mii = { sizeof(mii) };
	for (int i = GetMenuItemCount(hMenu) - 1; i >= 0; i--) {
		TCHAR str[256];
		mii.fMask = MIIM_TYPE | MIIM_SUBMENU;
		mii.dwTypeData = (TCHAR*)str;
		mii.cch = SIZEOF(str);
		GetMenuItemInfo(hMenu, i, TRUE, &mii);

		if (mii.cch && mii.dwTypeData) {
			TCHAR *result = (TCHAR*)LangPackTranslateString(uuid, (const char*)mii.dwTypeData, TRUE);
			if (result != mii.dwTypeData) {
				mii.dwTypeData = result;
				mii.fMask = MIIM_TYPE;
				SetMenuItemInfo(hMenu, i, TRUE, &mii);
			}
		}

		if (mii.hSubMenu != NULL)
			TranslateMenu_LP(mii.hSubMenu, hLangpack);
	}
}

static void TranslateWindow(MUUID *pUuid, HWND hwnd)
{
	TCHAR title[2048];
	GetWindowText(hwnd, title, SIZEOF(title));

	TCHAR *result = (TCHAR*)LangPackTranslateString(pUuid, (const char*)title, TRUE);
	if (result != title)
		SetWindowText(hwnd, result);
}

struct LANGPACKTRANSLATEDIALOG
{
	HWND hwndDlg;
	int  hLangpack;
};

static BOOL CALLBACK TranslateDialogEnumProc(HWND hwnd, LPARAM lParam)
{
	int hLangpack = (int)lParam;
	TCHAR szClass[32];
	int id = GetDlgCtrlID(hwnd);

	MUUID *uuid = Langpack_LookupUuid(hLangpack);

	GetClassName(hwnd, szClass, SIZEOF(szClass));
	if (!lstrcmpi(szClass, _T("static")) || !lstrcmpi(szClass, _T("hyperlink")) || !lstrcmpi(szClass, _T("button")) || !lstrcmpi(szClass, _T("MButtonClass")) || !lstrcmpi(szClass, _T("MHeaderbarCtrl")))
		TranslateWindow(uuid, hwnd);
	else if (!lstrcmpi(szClass, _T("edit"))) {
		if (GetWindowLongPtr(hwnd, GWL_STYLE) & ES_READONLY)
			TranslateWindow(uuid, hwnd);
	}
	return TRUE;
}

MIR_CORE_DLL(void) TranslateDialog_LP(HWND hDlg, int hLangpack)
{
	TranslateWindow(Langpack_LookupUuid(hLangpack), hDlg);
	EnumChildWindows(hDlg, TranslateDialogEnumProc, hLangpack);
}

/////////////////////////////////////////////////////////////////////////////////////////

MIR_CORE_DLL(MUUID*) Langpack_LookupUuid(WPARAM wParam)
{
	int idx = (wParam >> 16) & 0xFFFF;
	return (idx > 0 && idx <= lMuuids.getCount()) ? lMuuids[idx - 1] : NULL;
}

MIR_CORE_DLL(int) Langpack_MarkPluginLoaded(PLUGININFOEX *pInfo)
{
	int idx = lMuuids.getIndex(&pInfo->uuid);
	if (idx == -1)
		return 0;

	return (idx + 1) << 16;
}

MIR_CORE_DLL(void) Langpack_SortDuplicates(void)
{
	if (g_entryCount == 0)
		return;

	LangPackEntry *s = g_pEntries + 1, *d = s, *pLast = g_pEntries;
	DWORD dwSavedHash = g_pEntries->englishHash;
	bool bSortNeeded = false;

	for (int i = 1; i < g_entryCount; i++, s++) {
		if (s->englishHash != dwSavedHash) {
			pLast = d;
			if (s != d)
				*d++ = *s;
			else
				d++;
			dwSavedHash = s->englishHash;
		}
		else {
			bSortNeeded = true;
			LangPackEntry *p = (LangPackEntry*)mir_alloc(sizeof(LangPackEntry));
			*p = *s;
			pLast->pNext = p; pLast = p;
		}
	}

	if (bSortNeeded) {
		g_entryCount = (int)(d - g_pEntries);
		qsort(g_pEntries, g_entryCount, sizeof(LangPackEntry), (int(*)(const void*, const void*))SortLangPackHashesProc);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////

MIR_CORE_DLL(int) LoadLangPackModule(void)
{
	bModuleInitialized = TRUE;

	ZeroMemory(&langPack, sizeof(langPack));

	hevChanged = CreateHookableEvent(ME_LANGPACK_CHANGED);
	return 0;
}

void UnloadLangPackModule()
{
	if (!bModuleInitialized) return;

	int i;
	for (i = 0; i < lMuuids.getCount(); i++)
		mir_free(lMuuids[i]);
	lMuuids.destroy();

	LangPackEntry *p = g_pEntries;
	for (i = 0; i < g_entryCount; i++, p++) {
		if (p->pNext != NULL) {
			for (LangPackEntry *p1 = p->pNext; p1 != NULL;) {
				LangPackEntry *p2 = p1; p1 = p1->pNext;
				mir_free(p2->szLocal);
				mir_free(p2->wszLocal);
				mir_free(p2);
			}
		}

		mir_free(p->szLocal);
		mir_free(p->wszLocal);
	}

	if (g_entryCount) {
		mir_free(g_pEntries);
		g_pEntries = 0;
		g_entryCount = g_entriesAlloced = 0;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////

MIR_CORE_DLL(void) ReloadLangpack(TCHAR *pszStr)
{
	if (pszStr == NULL)
		pszStr = langPack.tszFileName;

	UnloadLangPackModule();
	LoadLangPack(pszStr);
	Langpack_SortDuplicates();

	NotifyEventHooks(hevChanged, 0, 0);
}
