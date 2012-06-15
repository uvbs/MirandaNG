/*
   Splash Screen Plugin for Miranda-IM (www.miranda-im.org)
   (c) 2004-2007 nullbie, (c) 2005-2007 Thief

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

   File name      : $URL: http://svn.miranda.im/mainrepo/splashscreen/trunk/src/headers.h $
   Revision       : $Rev: 1587 $
   Last change on : $Date: 2010-04-09 14:01:30 +0400 (Пт, 09 апр 2010) $
   Last change by : $Author: Thief $
*/

#ifndef HEADERS_H
#define HEADERS_H

#define MIRANDA_VER    0x0A00

#define _WIN32_WINNT 0x0500
#define WINVER 0x0400
#define AC_SRC_ALPHA  0x01
#define OEMRESOURCE
#define _CRT_SECURE_NO_DEPRECATE

#include <windows.h>
#include <stdio.h>
#include <commctrl.h>
#include <time.h>
#include <mmsystem.h>

// Miranda API headers
#include <win2k.h>
#include <newpluginapi.h>
#include <m_database.h>
#include <m_options.h>
#include <m_utils.h>
#include <m_langpack.h>
#include <m_system.h>
#include <m_png.h>
#include "m_updater.h"
#include "m_splash.h"

// Common headers
#include "version.h"
#include "resource.h"
#include "bitmap_funcs.h"

#ifdef _DEBUG
	#include <m_clist.h>
	#include <m_skin.h>
	#include <m_popup.h>
	#include "debug.h"
#endif

// Internal defines
#define SPLASH_CLASS "MirandaSplash"
#define MODNAME "SplashScreen"
#define WM_LOADED (WM_USER + 10)

#define tmemcpy wmemcpy

struct SPLASHOPTS
{
	byte active;
	byte playsnd;
	byte loopsnd;
	byte runonce;
	byte fadein;
	byte fadeout;
	byte inheritGS;
	byte random;
	byte showversion;
	byte fontsize;
	int showtime;
	int fosteps;
	int fisteps;
};

extern HWND hwndSplash;
extern SPLASHOPTS options;
extern TCHAR szSplashFile[MAX_PATH], szSoundFile[MAX_PATH], szPrefix[128];
extern TCHAR *szMirDir;
extern char szVersion[MAX_PATH];
extern BOOL bserviceinvoked, bmodulesloaded, png2dibavail;
extern HANDLE hSplashThread;
extern HINSTANCE hInst;

extern BOOL (WINAPI *MyUpdateLayeredWindow)
   (HWND hwnd, HDC hdcDST, POINT *pptDst, SIZE *psize, HDC hdcSrc, POINT *pptSrc,
    COLORREF crKey, BLENDFUNCTION *pblend, DWORD dwFlags);
// png2dib interface
typedef BOOL ( *pfnConvertPng2dib )( char*, size_t, BITMAPINFOHEADER** );
extern pfnConvertPng2dib png2dibConvertor;

extern int OptInit(WPARAM wParam, LPARAM lParam);
extern BOOL ShowSplash(BOOL bpreview);
extern VOID ReadIniConfig();
extern INT_PTR ShowSplashService(WPARAM wparam,LPARAM lparam);
#ifdef _DEBUG
	extern INT_PTR TestService(WPARAM wParam,LPARAM lParam);
#endif

#endif //HEADERS_H
