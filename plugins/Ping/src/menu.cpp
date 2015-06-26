#include "common.h"

HANDLE hMenuDisable, hMenuGraph, hMenuEdit;
HANDLE hEventMenuBuild;

void InitMenus()
{
	// main menu
	CLISTMENUITEM mi = { 0 };
	mi.flags = CMIF_TCHAR;
	mi.hParentMenu = Menu_CreateRoot(MO_MAIN, LPGENT("Ping"), 500099900);

	mi.position = 2000060000;
	mi.hIcon = hIconResponding;
	mi.ptszName = LPGENT("Enable all pings");
	mi.pszService = PLUG "/EnableAll";
	Menu_AddMainMenuItem(&mi);

	mi.position = 2000060001;
	mi.hIcon = hIconDisabled;
	mi.ptszName = LPGENT("Disable all pings");
	mi.pszService = PLUG "/DisableAll";
	Menu_AddMainMenuItem(&mi);
}
