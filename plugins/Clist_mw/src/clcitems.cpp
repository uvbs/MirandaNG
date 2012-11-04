/*

Miranda IM: the free IM client for Microsoft* Windows*

Copyright 2000-2003 Miranda ICQ/IM project, 
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
#include "m_clc.h"
#include "clc.h"
#include "clist.h"
#include "m_metacontacts.h"


extern int ( *saveAddItemToGroup )( ClcGroup *group, int iAboveItem );
extern int ( *saveAddInfoItemToGroup )(ClcGroup *group,int flags,const TCHAR *pszText);
extern ClcGroup* ( *saveAddGroup )(HWND hwnd,struct ClcData *dat,const TCHAR *szName,DWORD flags,int groupId,int calcTotalMembers);
extern void (*saveFreeContact)(struct ClcContact *p);
extern void (*saveFreeGroup)(ClcGroup *p);

//routines for managing adding/removal of items in the list, including sorting

extern void ClearClcContactCache(struct ClcData *dat,HANDLE hContact);

void AddSubcontacts(struct ClcContact * cont)
{
	int subcount,i,j;
	HANDLE hsub;
	pClcCacheEntry cacheEntry;
	cacheEntry = GetContactFullCacheEntry(cont->hContact);
	OutputDebugStringA("Proceed AddSubcontacts\r\n");
	subcount = (int)CallService(MS_MC_GETNUMCONTACTS,(WPARAM)cont->hContact,0);
	cont->SubExpanded = DBGetContactSettingByte(cont->hContact,"CList","Expanded",0);
	cont->isSubcontact = 0;
	cont->subcontacts = (struct ClcContact *) mir_realloc(cont->subcontacts, sizeof(struct ClcContact)*subcount);
	cont->SubAllocated = subcount;
	i = 0;
	for (j = 0; j<subcount; j++)
	{
		hsub = (HANDLE)CallService(MS_MC_GETSUBCONTACT,(WPARAM)cont->hContact,j);
		cacheEntry = GetContactFullCacheEntry(hsub);		
		if (!(DBGetContactSettingByte(NULL,"CLC","MetaHideOfflineSub",1) && DBGetContactSettingByte(NULL,"CList","HideOffline",SETTING_HIDEOFFLINE_DEFAULT))||
			cacheEntry->status != ID_STATUS_OFFLINE )
		{
			cont->subcontacts[i].hContact = cacheEntry->hContact;
			cont->subcontacts[i].iImage = CallService(MS_CLIST_GETCONTACTICON,(WPARAM)cacheEntry->hContact,0);
			memset(cont->subcontacts[i].iExtraImage, 0xFF, sizeof(cont->subcontacts[i].iExtraImage));
			cont->subcontacts[i].proto = cacheEntry->szProto;
			lstrcpyn(cont->subcontacts[i].szText,cacheEntry->tszName,SIZEOF(cont->subcontacts[i].szText));
			cont->subcontacts[i].type = CLCIT_CONTACT;
			//cont->flags = 0;//CONTACTF_ONLINE;
			cont->subcontacts[i].isSubcontact = 1;
			i++;
		}
	}
	cont->SubAllocated = i;
	if (!i) mir_free(cont->subcontacts);
}

void FreeContact(struct ClcContact *p)
{
	if ( p->SubAllocated && !p->isSubcontact)
		mir_free(p->subcontacts);

	saveFreeContact( p );
}

int AddItemToGroup(ClcGroup *group,int iAboveItem)
{
	iAboveItem = saveAddItemToGroup( group, iAboveItem );
	ClearRowByIndexCache();
	return iAboveItem;
}

ClcGroup *AddGroup(HWND hwnd,struct ClcData *dat,const TCHAR *szName,DWORD flags,int groupId,int calcTotalMembers)
{
	ClcGroup* result;

	ClearRowByIndexCache();	
	dat->needsResort = 1;
	result = saveAddGroup( hwnd, dat, szName, flags, groupId, calcTotalMembers);
	ClearRowByIndexCache();
	return result;
}

void FreeGroup(ClcGroup *group)
{
	saveFreeGroup( group );
	ClearRowByIndexCache();
}

int AddInfoItemToGroup(ClcGroup *group,int flags,const TCHAR *pszText)
{
	int i = saveAddInfoItemToGroup( group, flags, pszText );
	ClearRowByIndexCache();
	return i;
}

static struct ClcContact * AddContactToGroup(struct ClcData *dat,ClcGroup *group,pClcCacheEntry cacheEntry)
{
	char *szProto;
	WORD apparentMode;
	DWORD idleMode;
	HANDLE hContact;
	DBVARIANT dbv;
	int i;
	char AdvancedService[255] = {0};
	int img  = -1; 
	int basicIcon = 0;
	
	if (cacheEntry == NULL || group == NULL || dat == NULL)
		return NULL;

	hContact = cacheEntry->hContact;

	dat->needsResort = 1;
	for (i = group->cl.count-1;i>=0;i--)
		if (group->cl.items[i]->type != CLCIT_INFO || !(group->cl.items[i]->flags&CLCIIF_BELOWCONTACTS)) break;
	i = AddItemToGroup(group,i+1);
	group->cl.items[i]->type = CLCIT_CONTACT;
	group->cl.items[i]->SubAllocated = 0;
	group->cl.items[i]->isSubcontact = 0;
	group->cl.items[i]->subcontacts = NULL;

	_snprintf(AdvancedService,sizeof(AdvancedService),"%s%s",cacheEntry->szProto,"/GetAdvancedStatusIcon");

	if (ServiceExists(AdvancedService))
		img = CallService(AdvancedService,(WPARAM)hContact, 0);

	if (img == -1 || !(LOWORD(img)))
		img = CallService(MS_CLIST_GETCONTACTICON,(WPARAM)hContact,0);

	group->cl.items[i]->iImage = img;
	
	cacheEntry = GetContactFullCacheEntry(hContact);
	group->cl.items[i]->hContact = hContact;
	
	//cacheEntry->ClcContact = &(group->cl.items[i]);
	//SetClcContactCacheItem(dat,hContact,&(group->cl.items[i]));

	szProto = cacheEntry->szProto;
	if (szProto != NULL&&!pcli->pfnIsHiddenMode(dat,cacheEntry->status))
		group->cl.items[i]->flags |= CONTACTF_ONLINE;
	apparentMode = szProto != NULL?cacheEntry->ApparentMode:0;
	if (apparentMode == ID_STATUS_OFFLINE)	group->cl.items[i]->flags |= CONTACTF_INVISTO;
	else if (apparentMode == ID_STATUS_ONLINE) group->cl.items[i]->flags |= CONTACTF_VISTO;
	else if (apparentMode) group->cl.items[i]->flags |= CONTACTF_VISTO|CONTACTF_INVISTO;
	if (cacheEntry->NotOnList) group->cl.items[i]->flags |= CONTACTF_NOTONLIST;
	idleMode = szProto != NULL?cacheEntry->IdleTS:0;
	if (idleMode) group->cl.items[i]->flags |= CONTACTF_IDLE;

	lstrcpyn(group->cl.items[i]->szText,cacheEntry->tszName, SIZEOF(group->cl.items[i]->szText));
	group->cl.items[i]->proto = szProto;

	if (dat->style & CLS_SHOWSTATUSMESSAGES) {
		if (!DBGetContactSettingTString(hContact, "CList", "StatusMsg", &dbv)) {
			int j;
			lstrcpyn(group->cl.items[i]->szStatusMsg, dbv.ptszVal, SIZEOF(group->cl.items[i]->szStatusMsg));
			for (j = (int)_tcslen(group->cl.items[i]->szStatusMsg)-1;j>=0;j--) {
				if (group->cl.items[i]->szStatusMsg[j] == '\r' || group->cl.items[i]->szStatusMsg[j] == '\n' || group->cl.items[i]->szStatusMsg[j] == '\t') {
					group->cl.items[i]->szStatusMsg[j] = ' ';
				}
			}
			DBFreeVariant(&dbv);
			if (_tcslen(group->cl.items[i]->szStatusMsg)>0) {
				group->cl.items[i]->flags |= CONTACTF_STATUSMSG;
			}
		}	
	}

	ClearRowByIndexCache();
	return group->cl.items[i];
}

void AddContactToTree(HWND hwnd,struct ClcData *dat,HANDLE hContact,int updateTotalCount,int checkHideOffline)
{
	if (FindItem(hwnd,dat,hContact,NULL,NULL,NULL) == 1)
		return;

	pClcCacheEntry cacheEntry = GetContactFullCacheEntry(hContact);
	if (cacheEntry == NULL)
		return;

	char *szProto = cacheEntry->szProto;
	
	dat->needsResort = 1;
	ClearRowByIndexCache();
	ClearClcContactCache(dat,hContact);
	
	WORD status;
	DWORD style = GetWindowLongPtr(hwnd,GWL_STYLE);
	if (style & CLS_NOHIDEOFFLINE) checkHideOffline = 0;
	if (checkHideOffline) {
		if (szProto == NULL) status = ID_STATUS_OFFLINE;
		else status = cacheEntry->status;
	}

	ClcGroup *group;
	if (lstrlen(cacheEntry->tszGroup) == 0)
		group = &dat->list;
	else {
		group = AddGroup(hwnd,dat,cacheEntry->tszGroup,(DWORD)-1,0,0);
		if (group == NULL) {
			DWORD groupFlags;
			int i;
			if ( !(style & CLS_HIDEEMPTYGROUPS))
				return;

			if (checkHideOffline && pcli->pfnIsHiddenMode(dat,status)) {
				for (i = 1;;i++) {
					TCHAR *szGroupName = pcli->pfnGetGroupName(i, &groupFlags);
					if (szGroupName == NULL) 
						return;   //never happens
					if ( !lstrcmp(szGroupName,cacheEntry->tszGroup))
						break;
				}
				if (groupFlags & GROUPF_HIDEOFFLINE)
					return;
			}
			for (i = 1;; i++) {
				TCHAR *szGroupName = pcli->pfnGetGroupName(i, &groupFlags);
				if (szGroupName == NULL) 
					return;   //never happens
				if (!lstrcmp(szGroupName,cacheEntry->tszGroup))
					break;
				size_t len = lstrlen(szGroupName);
				if (!_tcsncmp(szGroupName,cacheEntry->tszGroup,len) && cacheEntry->tszGroup[len] == '\\')
					AddGroup(hwnd,dat,szGroupName,groupFlags,i,1);
			}
			group = AddGroup(hwnd,dat,cacheEntry->tszGroup,groupFlags,i,1);
		}
	}

	if (checkHideOffline) {
		if (pcli->pfnIsHiddenMode(dat,status) && (style&CLS_HIDEOFFLINE || group->hideOffline)) {
			if (updateTotalCount) group->totalMembers++;
			return;
		}
	}
	ClcContact *cont = AddContactToGroup(dat,group,cacheEntry);
	if (cont && cont->proto) {	
		cont->SubAllocated = 0;
		if (strcmp(cont->proto,"MetaContacts") == 0)
			AddSubcontacts(cont);
	}
	if (updateTotalCount)
		group->totalMembers++;
	ClearRowByIndexCache();
}

extern ClcGroup* ( *saveRemoveItemFromGroup )(HWND hwnd,ClcGroup *group,struct ClcContact *contact,int updateTotalCount);

ClcGroup *RemoveItemFromGroup(HWND hwnd,ClcGroup *group,struct ClcContact *contact,int updateTotalCount)
{
	ClearRowByIndexCache();
	if (contact->type == CLCIT_CONTACT) {
		struct ClcData* dat = (struct ClcData*)GetWindowLongPtr(hwnd,0);
		ClearClcContactCache(dat,contact->hContact);
	}

	group = saveRemoveItemFromGroup(hwnd, group, contact, updateTotalCount);
	
	ClearRowByIndexCache();
	return group;
}

void DeleteItemFromTree(HWND hwnd,HANDLE hItem)
{
	struct ClcContact *contact;
	ClcGroup *group;
	struct ClcData *dat = (struct ClcData*)GetWindowLongPtr(hwnd,0);
	
	ClearRowByIndexCache();
	dat->needsResort = 1;
	
	if (!FindItem(hwnd,dat,hItem,&contact,&group,NULL)) {
		DBVARIANT dbv;
		int i,nameOffset;
		if (!IsHContactContact(hItem)) return;
		ClearClcContactCache(dat,hItem);

		if (DBGetContactSettingTString(hItem,"CList","Group",&dbv)) return;

		//decrease member counts of all parent groups too
		group = &dat->list;
		nameOffset = 0;
		for (i = 0;;i++) {
			if (group->scanIndex == group->cl.count)
				break;

			if (group->cl.items[i]->type == CLCIT_GROUP) {
				int len = lstrlen(group->cl.items[i]->szText);
				if (!_tcsncmp(group->cl.items[i]->szText,dbv.ptszVal+nameOffset,len) && (dbv.ptszVal[nameOffset+len] == '\\' || dbv.pszVal[nameOffset+len] == '\0')) {
					group->totalMembers--;
					if (dbv.pszVal[nameOffset+len] == '\0') 
						break;
				}
			}
		}
		mir_free(dbv.pszVal);
	}
	else RemoveItemFromGroup(hwnd,group,contact,1);

	ClearRowByIndexCache();
}



void RebuildEntireList(HWND hwnd,struct ClcData *dat)
{
//	char *szProto;
	DWORD style = GetWindowLongPtr(hwnd,GWL_STYLE);
	HANDLE hContact;
	struct ClcContact * cont;
	ClcGroup *group;
	//DBVARIANT dbv;
	int tick = GetTickCount();

	ClearRowByIndexCache();
	ClearClcContactCache(dat,INVALID_HANDLE_VALUE);

	dat->list.expanded = 1;
	dat->list.hideOffline = DBGetContactSettingByte(NULL,"CLC","HideOfflineRoot",0);
	memset( &dat->list.cl, 0, sizeof( dat->list.cl ));
	dat->list.cl.increment = 30;
	dat->needsResort = 1;
	dat->selection = -1;
	{
		int i;
		TCHAR *szGroupName;
		DWORD groupFlags;

		for (i = 1;;i++) {
			szGroupName = pcli->pfnGetGroupName(i,&groupFlags);
			if (szGroupName == NULL)
				break;

			AddGroup(hwnd,dat,szGroupName,groupFlags,i,0);
		}
	}

	hContact = db_find_first();
	while(hContact) {
		
		pClcCacheEntry cacheEntry;
		cont = NULL;
		cacheEntry = GetContactFullCacheEntry(hContact);
		//cacheEntry->ClcContact = NULL;
		ClearClcContactCache(dat,hContact);
		if (cacheEntry == NULL)
			MessageBoxA(0,"Fail To Get CacheEntry for hContact","!!!!!!!!",0);

		if (style&CLS_SHOWHIDDEN || !cacheEntry->bIsHidden) {
			if (lstrlen(cacheEntry->tszGroup) == 0)
				group = &dat->list;
			else {
				group = AddGroup(hwnd,dat,cacheEntry->tszGroup,(DWORD)-1,0,0);
				//mir_free(dbv.pszVal);
			}

			if (group != NULL) {
				group->totalMembers++;
				if (!(style&CLS_NOHIDEOFFLINE) && (style&CLS_HIDEOFFLINE || group->hideOffline)) {
					//szProto = (char*)CallService(MS_PROTO_GETCONTACTBASEPROTO,(WPARAM)hContact,0);
					if (cacheEntry->szProto == NULL) {
						if (!pcli->pfnIsHiddenMode(dat,ID_STATUS_OFFLINE)||cacheEntry->noHiddenOffline)
							cont = AddContactToGroup(dat,group,cacheEntry);
					}
					else
						if (!pcli->pfnIsHiddenMode(dat,cacheEntry->status)||cacheEntry->noHiddenOffline)
							cont = AddContactToGroup(dat,group,cacheEntry);
				}
				else cont = AddContactToGroup(dat,group,cacheEntry);
			}
		}
		if (cont && cont->proto) {	
			cont->SubAllocated = 0;
			if (strcmp(cont->proto,"MetaContacts") == 0)
				AddSubcontacts(cont);
		}
		hContact = db_find_next(hContact);
	}

	if (style&CLS_HIDEEMPTYGROUPS) {
		group = &dat->list;
		group->scanIndex = 0;
		for (;;) {
			if (group->scanIndex == group->cl.count) {
				group = group->parent;
				if (group == NULL)
					break;
			}
			else if (group->cl.items[group->scanIndex]->type == CLCIT_GROUP) {
				if (group->cl.items[group->scanIndex]->group->cl.count == 0) {
					group = RemoveItemFromGroup(hwnd,group,group->cl.items[group->scanIndex],0);
				}
				else {
					group = group->cl.items[group->scanIndex]->group;
					group->scanIndex = 0;
				}
				continue;
			}
			group->scanIndex++;
		}
	}

	SortCLC(hwnd,dat,0);
	tick = GetTickCount()-tick;
	{
	char buf[255];
	//sprintf(buf,"%s %s took %i ms",__FILE__,__LINE__,tick);
	sprintf(buf,"RebuildEntireList %d \r\n",tick);

	OutputDebugStringA(buf);
	DBWriteContactSettingDword((HANDLE)0,"CLUI","PF:Last RebuildEntireList Time:",tick);
	}	
}


int GetNewSelection(ClcGroup *group, int selection, int direction)
{
	int lastcount = 0, count = 0;//group->cl.count;
	ClcGroup *topgroup = group;
	if (selection<0) {
		return 0;
	}
	group->scanIndex = 0;
	for (;;) {
		if (group->scanIndex == group->cl.count) {
			group = group->parent;
			if (group == NULL)
				break;

			group->scanIndex++;
			continue;
		}
		if (count>=selection) return count;
		lastcount = count;
		count++;
		if ((group->cl.items[group->scanIndex]->type == CLCIT_CONTACT) && (group->cl.items[group->scanIndex]->flags & CONTACTF_STATUSMSG)) {
			count++;
		}
		if (!direction) {
			if (count>selection) return lastcount;
		}
		if (group->cl.items[group->scanIndex]->type == CLCIT_GROUP && (group->cl.items[group->scanIndex]->group->expanded)) {
			group = group->cl.items[group->scanIndex]->group;
			group->scanIndex = 0;
			continue;
		}
		group->scanIndex++;
	}
	return lastcount;
 }

int GetGroupContentsCount(ClcGroup *group,int visibleOnly)
{
	int count = 0;//group->cl.count;
	ClcGroup *topgroup = group;

	group->scanIndex = 0;
	for (;;) {
		if (group->scanIndex == group->cl.count) {
			if (group == topgroup) break;
			group = group->parent;
			group->scanIndex++;
			continue;
		}

		count++;
		if ((group->cl.items[group->scanIndex]->type == CLCIT_CONTACT) && (group->cl.items[group->scanIndex]->flags & CONTACTF_STATUSMSG))
			count++;

		if (group->cl.items[group->scanIndex]->type == CLCIT_GROUP && (!visibleOnly || group->cl.items[group->scanIndex]->group->expanded)) {
			group = group->cl.items[group->scanIndex]->group;
			group->scanIndex = 0;
			continue;
		}
		group->scanIndex++;
	}
	return count;
}

extern void ( *saveSortCLC )(HWND hwnd,struct ClcData *dat,int useInsertionSort);

void SortCLC(HWND hwnd,struct ClcData *dat,int useInsertionSort)
{
#ifdef _DEBUG
	DWORD tick = GetTickCount();
#endif	
	int oldSort = dat->needsResort;
	saveSortCLC(hwnd, dat, useInsertionSort);
	if ( oldSort )
		ClearRowByIndexCache();

#ifdef _DEBUG
	{
		char buf[255];
		//sprintf(buf,"%s %s took %i ms",__FILE__,__LINE__,tick);
		tick = GetTickCount()-tick;
		if (tick > 5) {
			sprintf(buf,"SortCLC %d \r\n",tick);
			OutputDebugStringA(buf);
			DBWriteContactSettingDword((HANDLE)0,"CLUI","PF:Last SortCLC Time:",tick);
		}
	}
#endif	
}

struct SavedContactState_t
{
	HANDLE hContact;
	WORD iExtraImage[EXTRA_ICON_COUNT];
	int checked;
};

struct SavedGroupState_t
{
	int groupId, expanded;
};

struct SavedInfoState_t
{
	int parentId;
	struct ClcContact contact;
};

void SaveStateAndRebuildList(HWND hwnd,struct ClcData *dat)
{
	NMCLISTCONTROL nm;
	int i,j;
	struct SavedGroupState_t *savedGroup = NULL;
	int savedGroupCount = 0,savedGroupAlloced = 0;
	struct SavedContactState_t *savedContact = NULL;
	int savedContactCount = 0,savedContactAlloced = 0;
	struct SavedInfoState_t *savedInfo = NULL;
	int savedInfoCount = 0,savedInfoAlloced = 0;
	ClcGroup *group;
	struct ClcContact *contact;

	int tick = GetTickCount();
	int allocstep = 1024;
	pcli->pfnHideInfoTip(hwnd,dat);
	KillTimer(hwnd,TIMERID_INFOTIP);
	KillTimer(hwnd,TIMERID_RENAME);
	pcli->pfnEndRename(hwnd,dat,1);

	group = &dat->list;
	group->scanIndex = 0;
	for (;;) {
		if (group->scanIndex == group->cl.count) {
			group = group->parent;
			if (group == NULL)
				break;
		}
		else if (group->cl.items[group->scanIndex]->type == CLCIT_GROUP) {
			group = group->cl.items[group->scanIndex]->group;
			group->scanIndex = 0;
			if (++savedGroupCount>savedGroupAlloced) {
				savedGroupAlloced += allocstep;
				savedGroup = (struct SavedGroupState_t*)mir_realloc(savedGroup,sizeof(struct SavedGroupState_t)*savedGroupAlloced);
			}
			savedGroup[savedGroupCount-1].groupId = group->groupId;
			savedGroup[savedGroupCount-1].expanded = group->expanded;
			continue;
		}
		else if (group->cl.items[group->scanIndex]->type == CLCIT_CONTACT) {			
			if (++savedContactCount>savedContactAlloced) {
				savedContactAlloced += allocstep;
				savedContact = (struct SavedContactState_t*)mir_realloc(savedContact,sizeof(struct SavedContactState_t)*savedContactAlloced);
			}
			savedContact[savedContactCount-1].hContact = group->cl.items[group->scanIndex]->hContact;
			memcpy(savedContact[savedContactCount-1].iExtraImage, group->cl.items[group->scanIndex]->iExtraImage, sizeof(contact->iExtraImage));
			savedContact[savedContactCount-1].checked = group->cl.items[group->scanIndex]->flags & CONTACTF_CHECKED;
			if (group->cl.items[group->scanIndex]->SubAllocated>0)
			{
				int l;
				for (l = 0; l<group->cl.items[group->scanIndex]->SubAllocated; l++)
				{
					if (++savedContactCount>savedContactAlloced) {
						savedContactAlloced += allocstep;
						savedContact = (struct SavedContactState_t*)mir_realloc(savedContact,sizeof(struct SavedContactState_t)*savedContactAlloced);
					}
					savedContact[savedContactCount-1].hContact = group->cl.items[group->scanIndex]->subcontacts[l].hContact;
					memcpy(savedContact[savedContactCount-1].iExtraImage, group->cl.items[group->scanIndex]->subcontacts[l].iExtraImage, sizeof(contact->iExtraImage));
					savedContact[savedContactCount-1].checked = group->cl.items[group->scanIndex]->subcontacts[l].flags&CONTACTF_CHECKED;
				}
			}
		}
		else if (group->cl.items[group->scanIndex]->type == CLCIT_INFO) {
			if (++savedInfoCount>savedInfoAlloced) {
				savedInfoAlloced += allocstep;
				savedInfo = (struct SavedInfoState_t*)mir_realloc(savedInfo,sizeof(struct SavedInfoState_t)*savedInfoAlloced);
			}
			if (group->parent == NULL)
				savedInfo[savedInfoCount-1].parentId = -1;
			else
				savedInfo[savedInfoCount-1].parentId = group->groupId;
			savedInfo[savedInfoCount-1].contact = *group->cl.items[group->scanIndex];
		}
		group->scanIndex++;
	}

	pcli->pfnFreeGroup(&dat->list);
	RebuildEntireList(hwnd,dat);
	
	group = &dat->list;
	group->scanIndex = 0;
	for (;;) {
		if (group->scanIndex == group->cl.count) {
			group = group->parent;
			if (group == NULL)
				break;
		}
		else if (group->cl.items[group->scanIndex]->type == CLCIT_GROUP) {
			group = group->cl.items[group->scanIndex]->group;
			group->scanIndex = 0;
			for (i = 0;i<savedGroupCount;i++)
				if (savedGroup[i].groupId == group->groupId) {
					group->expanded = savedGroup[i].expanded;
					break;
				}
			continue;
		}
		else if (group->cl.items[group->scanIndex]->type == CLCIT_CONTACT) {
			for (i = 0;i<savedContactCount;i++)
				if (savedContact[i].hContact == group->cl.items[group->scanIndex]->hContact) {
					memcpy(group->cl.items[group->scanIndex]->iExtraImage, savedContact[i].iExtraImage, sizeof(contact->iExtraImage));
					if (savedContact[i].checked)
						group->cl.items[group->scanIndex]->flags |= CONTACTF_CHECKED;
					break;	
				}
			if (group->cl.items[group->scanIndex]->SubAllocated>0)
			{
				for (int l = 0; l<group->cl.items[group->scanIndex]->SubAllocated; l++)
					for (i = 0;i<savedContactCount;i++)
						if (savedContact[i].hContact == group->cl.items[group->scanIndex]->subcontacts[l].hContact) {
							memcpy(group->cl.items[group->scanIndex]->subcontacts[l].iExtraImage, savedContact[i].iExtraImage, sizeof(contact->iExtraImage));
							if (savedContact[i].checked)
								group->cl.items[group->scanIndex]->subcontacts[l].flags |= CONTACTF_CHECKED;
							break;	
						}	
			}
		}
		group->scanIndex++;
	}
	if (savedGroup) mir_free(savedGroup);
	if (savedContact) mir_free(savedContact);
	for (i = 0;i<savedInfoCount;i++) {
		if (savedInfo[i].parentId == -1) group = &dat->list;
		else {
			if (!FindItem(hwnd,dat,(HANDLE)(savedInfo[i].parentId|HCONTACT_ISGROUP),&contact,NULL,NULL)) continue;
			group = contact->group;
		}
		j = AddInfoItemToGroup(group,savedInfo[i].contact.flags,_T(""));
		*group->cl.items[j] = savedInfo[i].contact;
	}
	if (savedInfo) mir_free(savedInfo);
	pcli->pfnRecalculateGroupCheckboxes(hwnd,dat);

	RecalcScrollBar(hwnd,dat);
	nm.hdr.code = CLN_LISTREBUILT;
	nm.hdr.hwndFrom = hwnd;
	nm.hdr.idFrom = GetDlgCtrlID(hwnd);


	//srand(GetTickCount());
	
	tick = GetTickCount()-tick;
	{
	char buf[255];
	//sprintf(buf,"%s %s took %i ms",__FILE__,__LINE__,tick);
	sprintf(buf,"SaveStateAndRebuildList %d \r\n",tick);

	OutputDebugStringA(buf);
	}	
	ClearRowByIndexCache();
	SendMessage(GetParent(hwnd),WM_NOTIFY,0,(LPARAM)&nm);
}
