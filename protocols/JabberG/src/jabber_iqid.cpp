/*

Jabber Protocol Plugin for Miranda IM
Copyright (C) 2002-04  Santithorn Bunchua
Copyright (C) 2005-12  George Hazan
Copyright (C) 2007     Maxim Mluhov
Copyright (C) 2012-13  Miranda NG Project

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

#include "jabber.h"
#include "jabber_list.h"
#include "jabber_iq.h"
#include "jabber_caps.h"
#include "jabber_privacy.h"

void CJabberProto::OnIqResultServerDiscoInfo(HXML iqNode)
{
	if ( !iqNode)
		return;

	const TCHAR *type = xmlGetAttrValue(iqNode, _T("type"));
	int i;

	if ( !_tcscmp(type, _T("result"))) {
		HXML query = xmlGetChildByTag(iqNode, "query", "xmlns", JABBER_FEAT_DISCO_INFO);
		if ( !query)
			return;

		HXML identity;
		for (i = 1; (identity = xmlGetNthChild(query, _T("identity"), i)) != NULL; i++) {
			const TCHAR *identityCategory = xmlGetAttrValue(identity, _T("category"));
			const TCHAR *identityType = xmlGetAttrValue(identity, _T("type"));
			const TCHAR *identityName = xmlGetAttrValue(identity, _T("name"));
			if (identityCategory && identityType && !_tcscmp(identityCategory, _T("pubsub")) && !_tcscmp(identityType, _T("pep"))) {
				m_bPepSupported = TRUE;

				EnableMenuItems(TRUE);
				RebuildInfoFrame();
			}
			else if (identityCategory && identityType && identityName &&
				!_tcscmp(identityCategory, _T("server")) &&
				!_tcscmp(identityType, _T("im")) &&
				!_tcscmp(identityName, _T("Google Talk"))) {
					m_ThreadInfo->jabberServerCaps |= JABBER_CAPS_PING;
					m_bGoogleTalk = true;

					// Google Shared Status
					m_ThreadInfo->send(
						XmlNodeIq(m_iqManager.AddHandler(&CJabberProto::OnIqResultGoogleSharedStatus, JABBER_IQ_TYPE_GET))
							<< XQUERY(JABBER_FEAT_GTALK_SHARED_STATUS) << XATTR(_T("version"), _T("2")));
			}
		}

		if (m_ThreadInfo) {
			HXML feature;
			for (i = 1; (feature = xmlGetNthChild(query, _T("feature"), i)) != NULL; i++) {
				const TCHAR *featureName = xmlGetAttrValue(feature, _T("var"));
				if (!featureName)
					continue;

				for (int j = 0; g_JabberFeatCapPairs[j].szFeature; j++)
					if ( !_tcscmp(g_JabberFeatCapPairs[j].szFeature, featureName)) {
						m_ThreadInfo->jabberServerCaps |= g_JabberFeatCapPairs[j].jcbCap;
						break;
					}
			}
		}

		OnProcessLoginRq(m_ThreadInfo, JABBER_LOGIN_SERVERINFO);
}	}

void CJabberProto::OnIqResultNestedRosterGroups(HXML iqNode, CJabberIqInfo* pInfo)
{
	const TCHAR *szGroupDelimeter = NULL;
	BOOL bPrivateStorageSupport = FALSE;

	if (iqNode && pInfo->GetIqType() == JABBER_IQ_TYPE_RESULT) {
		bPrivateStorageSupport = TRUE;
		szGroupDelimeter = XPathFmt(iqNode, _T("query[@xmlns='%s']/roster[@xmlns='%s']"), JABBER_FEAT_PRIVATE_STORAGE, JABBER_FEAT_NESTED_ROSTER_GROUPS);
		if (szGroupDelimeter && !szGroupDelimeter[0])
			szGroupDelimeter = NULL; // "" as roster delimeter is not supported :)
	}

	// global fuckup
	if ( !m_ThreadInfo)
		return;

	// is our default delimiter?
	if ((!szGroupDelimeter && bPrivateStorageSupport) || (szGroupDelimeter && _tcscmp(szGroupDelimeter, _T("\\"))))
		m_ThreadInfo->send(
			XmlNodeIq(_T("set"), SerialNext()) << XQUERY(JABBER_FEAT_PRIVATE_STORAGE)
				<< XCHILD(_T("roster"), _T("\\")) << XATTR(_T("xmlns"), JABBER_FEAT_NESTED_ROSTER_GROUPS));

	// roster request
	TCHAR *szUserData = mir_tstrdup(szGroupDelimeter ? szGroupDelimeter : _T("\\"));
	m_ThreadInfo->send(
		XmlNodeIq(m_iqManager.AddHandler(&CJabberProto::OnIqResultGetRoster, JABBER_IQ_TYPE_GET, NULL, 0, -1, (void*)szUserData))
			<< XCHILDNS(_T("query"), JABBER_FEAT_IQ_ROSTER));
}

void CJabberProto::OnIqResultNotes(HXML iqNode, CJabberIqInfo* pInfo)
{
	if (iqNode && pInfo->GetIqType() == JABBER_IQ_TYPE_RESULT) {
		HXML hXmlData = XPathFmt(iqNode, _T("query[@xmlns='%s']/storage[@xmlns='%s']"),
			JABBER_FEAT_PRIVATE_STORAGE, JABBER_FEAT_MIRANDA_NOTES);
		if (hXmlData) m_notes.LoadXml(hXmlData);
	}
}

void CJabberProto::OnProcessLoginRq(ThreadData* info, DWORD rq)
{
	if (info == NULL)
		return;

	info->dwLoginRqs |= rq;

	DWORD dwMask = JABBER_LOGIN_ROSTER | JABBER_LOGIN_BOOKMARKS | JABBER_LOGIN_SERVERINFO;
	if ((info->dwLoginRqs & dwMask) == dwMask && !(info->dwLoginRqs & JABBER_LOGIN_BOOKMARKS_AJ)) {
		if (info->jabberServerCaps & JABBER_CAPS_ARCHIVE_AUTO)
			EnableArchive(m_options.EnableMsgArchive != 0);

		if (jabberChatDllPresent && m_options.AutoJoinBookmarks) {
			LIST<JABBER_LIST_ITEM> ll(10);
			LISTFOREACH(i, this, LIST_BOOKMARK)
			{
				JABBER_LIST_ITEM *item = ListGetItemPtrFromIndex(i);
				if (item != NULL && !lstrcmp(item->type, _T("conference")) && item->bAutoJoin)
					ll.insert(item);
			}

			for (int j=0; j < ll.getCount(); j++) {
				JABBER_LIST_ITEM *item = ll[j];

				TCHAR room[256], *server, *p;
				TCHAR text[128];
				_tcsncpy(text, item->jid, SIZEOF(text));
				_tcsncpy(room, text, SIZEOF(room));
				p = _tcstok(room, _T("@"));
				server = _tcstok(NULL, _T("@"));
				if (item->nick && item->nick[0] != 0)
					GroupchatJoinRoom(server, p, item->nick, item->password, true);
				else {
					TCHAR *nick = JabberNickFromJID(m_szJabberJID);
					GroupchatJoinRoom(server, p, nick, item->password, true);
					mir_free(nick);
				}
			}

			ll.destroy();
		}

		OnProcessLoginRq(info, JABBER_LOGIN_BOOKMARKS_AJ);
}	}

void CJabberProto::OnLoggedIn()
{
	m_bJabberOnline = TRUE;
	m_tmJabberLoggedInTime = time(0);

	m_ThreadInfo->dwLoginRqs = 0;

	// XEP-0083 support
	{
		CJabberIqInfo* pIqInfo = m_iqManager.AddHandler(&CJabberProto::OnIqResultNestedRosterGroups, JABBER_IQ_TYPE_GET);
		// ugly hack to prevent hangup during login process
		pIqInfo->SetTimeout(30000);
		m_ThreadInfo->send(
			XmlNodeIq(pIqInfo) << XQUERY(JABBER_FEAT_PRIVATE_STORAGE)
				<< XCHILDNS(_T("roster"), JABBER_FEAT_NESTED_ROSTER_GROUPS));
	}

	// Server-side notes
	{
		m_ThreadInfo->send(
			XmlNodeIq(m_iqManager.AddHandler(&CJabberProto::OnIqResultNotes, JABBER_IQ_TYPE_GET))
				<< XQUERY(JABBER_FEAT_PRIVATE_STORAGE)
				<< XCHILDNS(_T("storage"), JABBER_FEAT_MIRANDA_NOTES));
	}

	int iqId = SerialNext();
	IqAdd(iqId, IQ_PROC_DISCOBOOKMARKS, &CJabberProto::OnIqResultDiscoBookmarks);
	m_ThreadInfo->send(
		XmlNodeIq(_T("get"), iqId) << XQUERY(JABBER_FEAT_PRIVATE_STORAGE)
			<< XCHILDNS(_T("storage"), _T("storage:bookmarks")));

	m_bPepSupported = FALSE;
	m_ThreadInfo->jabberServerCaps = JABBER_RESOURCE_CAPS_NONE;
	iqId = SerialNext();
	IqAdd(iqId, IQ_PROC_NONE, &CJabberProto::OnIqResultServerDiscoInfo);
	m_ThreadInfo->send( XmlNodeIq(_T("get"), iqId, _A2T(m_ThreadInfo->server)) << XQUERY(JABBER_FEAT_DISCO_INFO));

	QueryPrivacyLists(m_ThreadInfo);

	ptrA szServerName( getStringA("LastLoggedServer"));
	if (szServerName == NULL || strcmp(m_ThreadInfo->server, szServerName))
		SendGetVcard(m_szJabberJID);

	setString("LastLoggedServer", m_ThreadInfo->server);
	m_pepServices.ResetPublishAll();
}

void CJabberProto::OnIqResultGetAuth(HXML iqNode)
{
	// RECVED: result of the request for authentication method
	// ACTION: send account authentication information to log in
	Log("<iq/> iqIdGetAuth");

	HXML queryNode;
	const TCHAR *type;
	if ((type=xmlGetAttrValue(iqNode, _T("type"))) == NULL) return;
	if ((queryNode=xmlGetChild(iqNode , "query")) == NULL) return;

	if ( !lstrcmp(type, _T("result"))) {
		int iqId = SerialNext();
		IqAdd(iqId, IQ_PROC_NONE, &CJabberProto::OnIqResultSetAuth);

		XmlNodeIq iq(_T("set"), iqId);
		HXML query = iq << XQUERY(_T("jabber:iq:auth"));
		query << XCHILD(_T("username"), m_ThreadInfo->username);
		if (xmlGetChild(queryNode, "digest") != NULL && m_szStreamId) {
			char* str = mir_utf8encodeT(m_ThreadInfo->password);
			char text[200];
			mir_snprintf(text, SIZEOF(text), "%s%s", m_szStreamId, str);
			mir_free(str);
         if ((str=JabberSha1(text)) != NULL) {
				query << XCHILD(_T("digest"), _A2T(str));
				mir_free(str);
			}
		}
		else if (xmlGetChild(queryNode, "password") != NULL)
			query << XCHILD(_T("password"), m_ThreadInfo->password);
		else {
			Log("No known authentication mechanism accepted by the server.");

			m_ThreadInfo->send("</stream:stream>");
			return;
		}

		if (xmlGetChild(queryNode , "resource") != NULL)
			query << XCHILD(_T("resource"), m_ThreadInfo->resource);

		m_ThreadInfo->send(iq);
	}
	else if ( !lstrcmp(type, _T("error"))) {
 		m_ThreadInfo->send("</stream:stream>");

		TCHAR text[128];
		mir_sntprintf(text, SIZEOF(text), _T("%s %s."), TranslateT("Authentication failed for"), m_ThreadInfo->username);
		MsgPopup(NULL, text, TranslateT("Jabber Authentication"));
		JLoginFailed(LOGINERR_WRONGPASSWORD);
		m_ThreadInfo = NULL;	// To disallow auto reconnect
}	}

void CJabberProto::OnIqResultSetAuth(HXML iqNode)
{
	const TCHAR *type;

	// RECVED: authentication result
	// ACTION: if successfully logged in, continue by requesting roster list and set my initial status
	Log("<iq/> iqIdSetAuth");
	if ((type=xmlGetAttrValue(iqNode, _T("type"))) == NULL) return;

	if ( !lstrcmp(type, _T("result"))) {
		DBVARIANT dbv;
		if (getTString("Nick", &dbv))
			setTString("Nick", m_ThreadInfo->username);
		else
			db_free(&dbv);

		OnLoggedIn();
	}
	// What to do if password error? etc...
	else if ( !lstrcmp(type, _T("error"))) {
		TCHAR text[128];

		m_ThreadInfo->send("</stream:stream>");
		mir_sntprintf(text, SIZEOF(text), _T("%s %s."), TranslateT("Authentication failed for"), m_ThreadInfo->username);
		MsgPopup(NULL, text, TranslateT("Jabber Authentication"));
		JLoginFailed(LOGINERR_WRONGPASSWORD);
		m_ThreadInfo = NULL;	// To disallow auto reconnect
}	}

void CJabberProto::OnIqResultBind(HXML iqNode, CJabberIqInfo* pInfo)
{
	if ( !m_ThreadInfo || !iqNode)
		return;
	if (pInfo->GetIqType() == JABBER_IQ_TYPE_RESULT) {
		LPCTSTR szJid = XPathT(iqNode, "bind[@xmlns='urn:ietf:params:xml:ns:xmpp-bind']/jid");
		if (szJid) {
			if ( !_tcsncmp(m_ThreadInfo->fullJID, szJid, SIZEOF(m_ThreadInfo->fullJID)))
				Log("Result Bind: %S confirmed ", m_ThreadInfo->fullJID);
			else {
				Log("Result Bind: %S changed to %S", m_ThreadInfo->fullJID, szJid);
				_tcsncpy(m_ThreadInfo->fullJID, szJid, SIZEOF(m_ThreadInfo->fullJID));
			}
		}
		if (m_ThreadInfo->bIsSessionAvailable)
			m_ThreadInfo->send(
				XmlNodeIq(m_iqManager.AddHandler(&CJabberProto::OnIqResultSession, JABBER_IQ_TYPE_SET))
				<< XCHILDNS(_T("session"), _T("urn:ietf:params:xml:ns:xmpp-session")));
		else
			OnLoggedIn();
	}
	else {
		//rfc3920 page 39
		m_ThreadInfo->send("</stream:stream>");
		m_ThreadInfo = NULL;	// To disallow auto reconnect
	}
}

void CJabberProto::OnIqResultSession(HXML iqNode, CJabberIqInfo* pInfo)
{
	if (pInfo->GetIqType() == JABBER_IQ_TYPE_RESULT)
		OnLoggedIn();
}

void CJabberProto::GroupchatJoinByHContact(HANDLE hContact, bool autojoin)
{
	ptrT roomjid( db_get_tsa(hContact, m_szModuleName, "ChatRoomID"));
	if (roomjid == NULL)
		return;

	TCHAR *room = roomjid;
	TCHAR *server = _tcschr(roomjid, '@');
	if ( !server)
		return;

	server[0] = 0; server++;

	ptrT nick( db_get_tsa(hContact, m_szModuleName, "MyNick"));
	if (nick == NULL) {
		nick = JabberNickFromJID(m_szJabberJID);
		if (nick == NULL)
			return;
	}

	GroupchatJoinRoom(server, room, nick, ptrT(JGetStringCrypt(hContact, "LoginPassword")), autojoin);
}

/////////////////////////////////////////////////////////////////////////////////////////
// JabberIqResultGetRoster - populates LIST_ROSTER and creates contact for any new rosters

void CJabberProto::OnIqResultGetRoster(HXML iqNode, CJabberIqInfo* pInfo)
{
	Log("<iq/> iqIdGetRoster");
	TCHAR *szGroupDelimeter = (TCHAR *)pInfo->GetUserData();
	if (pInfo->GetIqType() != JABBER_IQ_TYPE_RESULT) {
		mir_free(szGroupDelimeter);
		return;
	}

	HXML queryNode = xmlGetChild(iqNode , "query");
	if (queryNode == NULL) {
		mir_free(szGroupDelimeter);
		return;
	}

	if (lstrcmp(xmlGetAttrValue(queryNode, _T("xmlns")), JABBER_FEAT_IQ_ROSTER)) {
		mir_free(szGroupDelimeter);
		return;
	}

	if ( !_tcscmp(szGroupDelimeter, _T("\\"))) {
		mir_free(szGroupDelimeter);
		szGroupDelimeter = NULL;
	}

	TCHAR *nick;
	int i;
	LIST<void> chatRooms(10);
	OBJLIST<JABBER_HTTP_AVATARS> *httpavatars = new OBJLIST<JABBER_HTTP_AVATARS>(20, JABBER_HTTP_AVATARS::compare);

	for (i=0; ; i++) {
		BOOL bIsTransport=FALSE;

		HXML itemNode = xmlGetChild(queryNode ,i);
		if ( !itemNode)
			break;

		if (_tcscmp(xmlGetName(itemNode), _T("item")))
			continue;

		const TCHAR *str = xmlGetAttrValue(itemNode, _T("subscription")), *name;

		JABBER_SUBSCRIPTION sub;
		if (str == NULL) sub = SUB_NONE;
		else if ( !_tcscmp(str, _T("both"))) sub = SUB_BOTH;
		else if ( !_tcscmp(str, _T("to"))) sub = SUB_TO;
		else if ( !_tcscmp(str, _T("from"))) sub = SUB_FROM;
		else sub = SUB_NONE;

		const TCHAR *jid = xmlGetAttrValue(itemNode, _T("jid"));
		if (jid == NULL)
			continue;
		if (_tcschr(jid, '@') == NULL)
			bIsTransport = TRUE;

		if ((name = xmlGetAttrValue(itemNode, _T("name"))) != NULL)
			nick = mir_tstrdup(name);
		else
			nick = JabberNickFromJID(jid);

		if (nick == NULL)
			continue;

		JABBER_LIST_ITEM *item = ListAdd(LIST_ROSTER, jid);
		item->subscription = sub;

		mir_free(item->nick); item->nick = nick;

		HXML groupNode = xmlGetChild(itemNode , "group");
		replaceStrT(item->group, xmlGetText(groupNode));

		// check group delimiters:
		if (item->group && szGroupDelimeter) {
			TCHAR *szPos = NULL;
			while (szPos = _tcsstr(item->group, szGroupDelimeter)) {
				*szPos = 0;
				szPos += _tcslen(szGroupDelimeter);
				TCHAR *szNewGroup = (TCHAR *)mir_alloc(sizeof(TCHAR) * (_tcslen(item->group) + _tcslen(szPos) + 2));
				_tcscpy(szNewGroup, item->group);
				_tcscat(szNewGroup, _T("\\"));
				_tcscat(szNewGroup, szPos);
				mir_free(item->group);
				item->group = szNewGroup;
			}
		}

		HANDLE hContact = HContactFromJID(jid);
		if (hContact == NULL) {
			// Received roster has a new JID.
			// Add the jid (with empty resource) to Miranda contact list.
			hContact = DBCreateContact(jid, nick, FALSE, FALSE);
		}

		if (name != NULL) {
			DBVARIANT dbNick;
			if ( !getTString(hContact, "Nick", &dbNick)) {
				if (lstrcmp(nick, dbNick.ptszVal) != 0)
					db_set_ts(hContact, "CList", "MyHandle", nick);
				else
					db_unset(hContact, "CList", "MyHandle");

				db_free(&dbNick);
			}
			else db_set_ts(hContact, "CList", "MyHandle", nick);
		}
		else db_unset(hContact, "CList", "MyHandle");

		if ( isChatRoom(hContact)) {
			GCSESSION gcw = {0};
			gcw.cbSize = sizeof(GCSESSION);
			gcw.iType = GCW_CHATROOM;
			gcw.pszModule = m_szModuleName;
			gcw.dwFlags = GC_TCHAR;
			gcw.ptszID = jid;
			gcw.ptszName = NEWTSTR_ALLOCA(jid);

			TCHAR *p = (TCHAR*)_tcschr(gcw.ptszName, '@');
			if (p)
				*p = 0;

			CallServiceSync(MS_GC_NEWSESSION, 0, (LPARAM)&gcw);

			db_unset(hContact, "CList", "Hidden");
			chatRooms.insert(hContact);
		}
		else UpdateSubscriptionInfo(hContact, item);

		if ( !m_options.IgnoreRosterGroups) {
			if (item->group != NULL) {
				JabberContactListCreateGroup(item->group);

				// Don't set group again if already correct, or Miranda may show wrong group count in some case
				DBVARIANT dbv;
				if ( !db_get_ts(hContact, "CList", "Group", &dbv)) {
					if (lstrcmp(dbv.ptszVal, item->group))
						db_set_ts(hContact, "CList", "Group", item->group);
					db_free(&dbv);
				}
				else db_set_ts(hContact, "CList", "Group", item->group);
			}
			else
				db_unset(hContact, "CList", "Group");
		}

		if (hContact != NULL) {
			if (bIsTransport)
				setByte(hContact, "IsTransport", TRUE);
			else
				setByte(hContact, "IsTransport", FALSE);
		}

		const TCHAR *imagepath = xmlGetAttrValue(itemNode, _T("vz:img"));
		if (imagepath)
			httpavatars->insert(new JABBER_HTTP_AVATARS(imagepath, hContact));
	}

	if (httpavatars->getCount())
		ForkThread(&CJabberProto::LoadHttpAvatars, httpavatars);
	else
		delete httpavatars;

	// Delete orphaned contacts (if roster sync is enabled)
	if (m_options.RosterSync == TRUE) {
		for (HANDLE hContact = db_find_first(m_szModuleName); hContact; ) {
			HANDLE hNext = db_find_next(hContact, m_szModuleName);
			ptrT jid( db_get_tsa(hContact, m_szModuleName, "jid"));
			if (jid != NULL && !ListGetItemPtr(LIST_ROSTER, jid)) {
				Log("Syncing roster: preparing to delete %S (hContact=0x%x)", jid, hContact);
				CallService(MS_DB_CONTACT_DELETE, (WPARAM)hContact, 0);
			}
			hContact = hNext;
		}
	}

	EnableMenuItems(TRUE);

	Log("Status changed via THREADSTART");
	m_bModeMsgStatusChangePending = FALSE;
	SetServerStatus(m_iDesiredStatus);

	if (m_options.AutoJoinConferences)
		for (i=0; i < chatRooms.getCount(); i++)
			GroupchatJoinByHContact((HANDLE)chatRooms[i], true);

	chatRooms.destroy();

	//UI_SAFE_NOTIFY(m_pDlgJabberJoinGroupchat, WM_JABBER_CHECK_ONLINE);
	//UI_SAFE_NOTIFY(m_pDlgBookmarks, WM_JABBER_CHECK_ONLINE);
	UI_SAFE_NOTIFY_HWND(m_hwndJabberAddBookmark, WM_JABBER_CHECK_ONLINE);
	WindowNotify(WM_JABBER_CHECK_ONLINE);

	UI_SAFE_NOTIFY(m_pDlgServiceDiscovery, WM_JABBER_TRANSPORT_REFRESH);

	if (szGroupDelimeter)
		mir_free(szGroupDelimeter);

	OnProcessLoginRq(m_ThreadInfo, JABBER_LOGIN_ROSTER);
	RebuildInfoFrame();
}

void CJabberProto::OnIqResultGetRegister(HXML iqNode)
{
	// RECVED: result of the request for (agent) registration mechanism
	// ACTION: activate (agent) registration input dialog
	Log("<iq/> iqIdGetRegister");

	HXML queryNode;
	const TCHAR *type;
	if ((type = xmlGetAttrValue(iqNode, _T("type"))) == NULL) return;
	if ((queryNode = xmlGetChild(iqNode , "query")) == NULL) return;

	if ( !lstrcmp(type, _T("result"))) {
		if (m_hwndAgentRegInput)
			SendMessage(m_hwndAgentRegInput, WM_JABBER_REGINPUT_ACTIVATE, 1 /*success*/, (LPARAM)xi.copyNode(iqNode));
	}
	else if ( !lstrcmp(type, _T("error"))) {
		if (m_hwndAgentRegInput) {
			HXML errorNode = xmlGetChild(iqNode , "error");
			TCHAR *str = JabberErrorMsg(errorNode);
			SendMessage(m_hwndAgentRegInput, WM_JABBER_REGINPUT_ACTIVATE, 0 /*error*/, (LPARAM)str);
			mir_free(str);
}	}	}

void CJabberProto::OnIqResultSetRegister(HXML iqNode)
{
	// RECVED: result of registration process
	// ACTION: notify of successful agent registration
	Log("<iq/> iqIdSetRegister");

	const TCHAR *type, *from;
	if ((type = xmlGetAttrValue(iqNode, _T("type"))) == NULL) return;
	if ((from = xmlGetAttrValue(iqNode, _T("from"))) == NULL) return;

	if ( !lstrcmp(type, _T("result"))) {
		HANDLE hContact = HContactFromJID(from);
		if (hContact != NULL)
			setByte(hContact, "IsTransport", TRUE);

		if (m_hwndRegProgress)
			SendMessage(m_hwndRegProgress, WM_JABBER_REGDLG_UPDATE, 100, (LPARAM)TranslateT("Registration successful"));
	}
	else if ( !lstrcmp(type, _T("error"))) {
		if (m_hwndRegProgress) {
			HXML errorNode = xmlGetChild(iqNode , "error");
			TCHAR *str = JabberErrorMsg(errorNode);
			SendMessage(m_hwndRegProgress, WM_JABBER_REGDLG_UPDATE, 100, (LPARAM)str);
			mir_free(str);
}	}	}

/////////////////////////////////////////////////////////////////////////////////////////
// JabberIqResultGetVcard - processes the server-side v-card

void CJabberProto::OnIqResultGetVcardPhoto(const TCHAR *jid, HXML n, HANDLE hContact, BOOL& hasPhoto)
{
	Log("JabberIqResultGetVcardPhoto: %d", hasPhoto);
	if (hasPhoto)
		return;

	HXML o = xmlGetChild(n, "BINVAL");
	LPCTSTR ptszBinval = xmlGetText(o);
	if (o == NULL || ptszBinval == NULL)
		return;

	unsigned bufferLen;
	ptrA buffer((char*)mir_base64_decode( _T2A(ptszBinval), &bufferLen));
	if (buffer == NULL)
		return;

	const TCHAR *szPicType = JabberGetPictureType(n, buffer);
	if (szPicType == NULL)
		return;

	TCHAR szAvatarFileName[MAX_PATH];
	GetAvatarFileName(hContact, szAvatarFileName, SIZEOF(szAvatarFileName));

	Log("Picture file name set to %S", szAvatarFileName);
	HANDLE hFile = CreateFile(szAvatarFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return;

	Log("Writing %d bytes", bufferLen);
	DWORD nWritten;
	bool bRes = WriteFile(hFile, buffer, bufferLen, &nWritten, NULL) != 0;
	CloseHandle(hFile);
	if (!bRes)
		return;

	Log("%d bytes written", nWritten);
	if (hContact == NULL) {
		hasPhoto = TRUE;
		CallService(MS_AV_SETMYAVATART, (WPARAM)m_szModuleName, (LPARAM)szAvatarFileName);

		Log("My picture saved to %S", szAvatarFileName);
	}
	else {
		DBVARIANT dbv;
		if ( !getTString(hContact, "jid", &dbv)) {
			JABBER_LIST_ITEM *item = ListGetItemPtr(LIST_ROSTER, jid);
			if (item == NULL) {
				item = ListAdd(LIST_VCARD_TEMP, jid); // adding to the temp list to store information about photo
				item->bUseResource = TRUE;
			}
			if (item != NULL) {
				hasPhoto = TRUE;
				if (item->photoFileName && _tcscmp(item->photoFileName, szAvatarFileName))
					DeleteFile(item->photoFileName);
				replaceStrT(item->photoFileName, szAvatarFileName);
				Log("Contact's picture saved to %S", szAvatarFileName);
				OnIqResultGotAvatar(hContact, o, szPicType);
			}

			db_free(&dbv);
	}	}

	if ( !hasPhoto)
		DeleteFile(szAvatarFileName);
}

static TCHAR* sttGetText(HXML node, char* tag)
{
	HXML n = xmlGetChild(node , tag);
	if (n == NULL)
		return NULL;

	return (TCHAR*)xmlGetText(n);
}

void CJabberProto::OnIqResultGetVcard(HXML iqNode)
{
	HXML vCardNode, m, n, o;
	const TCHAR *type, *jid;
	HANDLE hContact;
	TCHAR text[128];
	DBVARIANT dbv;

	Log("<iq/> iqIdGetVcard");
	if ((type = xmlGetAttrValue(iqNode, _T("type"))) == NULL) return;
	if ((jid = xmlGetAttrValue(iqNode, _T("from"))) == NULL) return;
	int id = JabberGetPacketID(iqNode);

	if (id == m_nJabberSearchID) {
		m_nJabberSearchID = -1;

		if ((vCardNode = xmlGetChild(iqNode , "vCard")) != NULL) {
			if ( !lstrcmp(type, _T("result"))) {
				JABBER_SEARCH_RESULT jsr = { 0 };
				jsr.hdr.cbSize = sizeof(JABBER_SEARCH_RESULT);
				jsr.hdr.flags = PSR_TCHAR;
				jsr.hdr.nick = sttGetText(vCardNode, "NICKNAME");
				jsr.hdr.firstName = sttGetText(vCardNode, "FN");
				jsr.hdr.lastName = _T("");
				jsr.hdr.email = sttGetText(vCardNode, "EMAIL");
				_tcsncpy(jsr.jid, jid, SIZEOF(jsr.jid));
				jsr.jid[ SIZEOF(jsr.jid)-1 ] = '\0';
				ProtoBroadcastAck(NULL, ACKTYPE_SEARCH, ACKRESULT_DATA, (HANDLE)id, (LPARAM)&jsr);
				ProtoBroadcastAck(NULL, ACKTYPE_SEARCH, ACKRESULT_SUCCESS, (HANDLE)id, 0);
			}
			else if ( !lstrcmp(type, _T("error")))
				ProtoBroadcastAck(NULL, ACKTYPE_SEARCH, ACKRESULT_SUCCESS, (HANDLE)id, 0);
		}
		else ProtoBroadcastAck(NULL, ACKTYPE_SEARCH, ACKRESULT_SUCCESS, (HANDLE)id, 0);
		return;
	}

	size_t len = _tcslen(m_szJabberJID);
	if ( !_tcsnicmp(jid, m_szJabberJID, len) && (jid[len]=='/' || jid[len]=='\0')) {
		hContact = NULL;
		Log("Vcard for myself");
	}
	else {
		if ((hContact = HContactFromJID(jid)) == NULL)
			return;
		Log("Other user's vcard");
	}

	if ( !lstrcmp(type, _T("result"))) {
		BOOL hasFn, hasNick, hasGiven, hasFamily, hasMiddle, hasBday, hasGender;
		BOOL hasPhone, hasFax, hasCell, hasUrl;
		BOOL hasHome, hasHomeStreet, hasHomeStreet2, hasHomeLocality, hasHomeRegion, hasHomePcode, hasHomeCtry;
		BOOL hasWork, hasWorkStreet, hasWorkStreet2, hasWorkLocality, hasWorkRegion, hasWorkPcode, hasWorkCtry;
		BOOL hasOrgname, hasOrgunit, hasRole, hasTitle;
		BOOL hasDesc, hasPhoto;
		int nEmail, nPhone, nYear, nMonth, nDay;

		hasFn = hasNick = hasGiven = hasFamily = hasMiddle = hasBday = hasGender = FALSE;
		hasPhone = hasFax = hasCell = hasUrl = FALSE;
		hasHome = hasHomeStreet = hasHomeStreet2 = hasHomeLocality = hasHomeRegion = hasHomePcode = hasHomeCtry = FALSE;
		hasWork = hasWorkStreet = hasWorkStreet2 = hasWorkLocality = hasWorkRegion = hasWorkPcode = hasWorkCtry = FALSE;
		hasOrgname = hasOrgunit = hasRole = hasTitle = FALSE;
		hasDesc = hasPhoto = FALSE;
		nEmail = nPhone = 0;

		if ((vCardNode = xmlGetChild(iqNode , "vCard")) != NULL) {
			for (int i=0; ; i++) {
				n = xmlGetChild(vCardNode ,i);
				if ( !n)
					break;
				if (xmlGetName(n) == NULL) continue;
				if ( !_tcscmp(xmlGetName(n), _T("FN"))) {
					if (xmlGetText(n) != NULL) {
						hasFn = TRUE;
						setTString(hContact, "FullName", xmlGetText(n));
					}
				}
				else if ( !lstrcmp(xmlGetName(n), _T("NICKNAME"))) {
					if (xmlGetText(n) != NULL) {
						hasNick = TRUE;
						setTString(hContact, "Nick", xmlGetText(n));
					}
				}
				else if ( !lstrcmp(xmlGetName(n), _T("N"))) {
					// First/Last name
					if ( !hasGiven && !hasFamily && !hasMiddle) {
						if ((m=xmlGetChild(n, "GIVEN")) != NULL && xmlGetText(m) != NULL) {
							hasGiven = TRUE;
							setTString(hContact, "FirstName", xmlGetText(m));
						}
						if ((m=xmlGetChild(n, "FAMILY")) != NULL && xmlGetText(m) != NULL) {
							hasFamily = TRUE;
							setTString(hContact, "LastName", xmlGetText(m));
						}
						if ((m=xmlGetChild(n, "MIDDLE")) != NULL && xmlGetText(m) != NULL) {
							hasMiddle = TRUE;
							setTString(hContact, "MiddleName", xmlGetText(m));
					}	}
				}
				else if ( !lstrcmp(xmlGetName(n), _T("EMAIL"))) {
					// E-mail address(es)
					if ((m=xmlGetChild(n, "USERID")) == NULL)	// Some bad client put e-mail directly in <EMAIL/> instead of <USERID/>
						m = n;
					if (xmlGetText(m) != NULL) {
						char text[100];
						if (hContact != NULL) {
							if (nEmail == 0)
								strcpy(text, "e-mail");
							else
								sprintf(text, "e-mail%d", nEmail-1);
						}
						else sprintf(text, "e-mail%d", nEmail);
						setTString(hContact, text, xmlGetText(m));

						if (hContact == NULL) {
							sprintf(text, "e-mailFlag%d", nEmail);
							int nFlag = 0;
							if (xmlGetChild(n, "HOME") != NULL) nFlag |= JABBER_VCEMAIL_HOME;
							if (xmlGetChild(n, "WORK") != NULL) nFlag |= JABBER_VCEMAIL_WORK;
							if (xmlGetChild(n, "INTERNET") != NULL) nFlag |= JABBER_VCEMAIL_INTERNET;
							if (xmlGetChild(n, "X400") != NULL) nFlag |= JABBER_VCEMAIL_X400;
							setWord(text, nFlag);
						}
						nEmail++;
					}
				}
				else if ( !lstrcmp(xmlGetName(n), _T("BDAY"))) {
					// Birthday
					if ( !hasBday && xmlGetText(n) != NULL) {
						if (hContact != NULL) {
							if (_stscanf(xmlGetText(n), _T("%d-%d-%d"), &nYear, &nMonth, &nDay) == 3) {
								hasBday = TRUE;
								setWord(hContact, "BirthYear", (WORD)nYear);
								setByte(hContact, "BirthMonth", (BYTE) nMonth);
								setByte(hContact, "BirthDay", (BYTE) nDay);

								SYSTEMTIME sToday = {0};
								GetLocalTime(&sToday);
								int nAge = sToday.wYear - nYear;
								if (sToday.wMonth < nMonth || (sToday.wMonth == nMonth && sToday.wDay < nDay))
									nAge--;
								if (nAge)
									setWord(hContact, "Age", (WORD)nAge);
							}
						}
						else {
							hasBday = TRUE;
							setTString("BirthDate", xmlGetText(n));
					}	}
				}
				else if ( !lstrcmp(xmlGetName(n), _T("GENDER"))) {
					// Gender
					if ( !hasGender && xmlGetText(n) != NULL) {
						if (hContact != NULL) {
							if (xmlGetText(n)[0] && strchr("mMfF", xmlGetText(n)[0]) != NULL) {
								hasGender = TRUE;
								setByte(hContact, "Gender", (BYTE) toupper(xmlGetText(n)[0]));
							}
						}
						else {
							hasGender = TRUE;
							setTString("GenderString", xmlGetText(n));
					}	}
				}
				else if ( !lstrcmp(xmlGetName(n), _T("ADR"))) {
					if ( !hasHome && xmlGetChild(n, "HOME") != NULL) {
						// Home address
						hasHome = TRUE;
						if ((m=xmlGetChild(n, "STREET")) != NULL && xmlGetText(m) != NULL) {
							hasHomeStreet = TRUE;
							if (hContact != NULL) {
								if ((o=xmlGetChild(n, "EXTADR")) != NULL && xmlGetText(o) != NULL)
									mir_sntprintf(text, SIZEOF(text), _T("%s\r\n%s"), xmlGetText(m), xmlGetText(o));
								else if ((o=xmlGetChild(n, "EXTADD")) != NULL && xmlGetText(o) != NULL)
									mir_sntprintf(text, SIZEOF(text), _T("%s\r\n%s"), xmlGetText(m), xmlGetText(o));
								else
									_tcsncpy(text, xmlGetText(m), SIZEOF(text));
								text[SIZEOF(text)-1] = '\0';
								setTString(hContact, "Street", text);
							}
							else {
								setTString(hContact, "Street", xmlGetText(m));
								if ((m=xmlGetChild(n, "EXTADR")) == NULL)
									m = xmlGetChild(n, "EXTADD");
								if (m != NULL && xmlGetText(m) != NULL) {
									hasHomeStreet2 = TRUE;
									setTString(hContact, "Street2", xmlGetText(m));
						}	}	}

						if ((m=xmlGetChild(n, "LOCALITY")) != NULL && xmlGetText(m) != NULL) {
							hasHomeLocality = TRUE;
							setTString(hContact, "City", xmlGetText(m));
						}
						if ((m=xmlGetChild(n, "REGION")) != NULL && xmlGetText(m) != NULL) {
							hasHomeRegion = TRUE;
							setTString(hContact, "State", xmlGetText(m));
						}
						if ((m=xmlGetChild(n, "PCODE")) != NULL && xmlGetText(m) != NULL) {
							hasHomePcode = TRUE;
							setTString(hContact, "ZIP", xmlGetText(m));
						}
						if ((m=xmlGetChild(n, "CTRY")) == NULL || xmlGetText(m) == NULL)	// Some bad client use <COUNTRY/> instead of <CTRY/>
							m = xmlGetChild(n, "COUNTRY");
						if (m != NULL && xmlGetText(m) != NULL) {
							hasHomeCtry = TRUE;
							setTString(hContact, "Country", xmlGetText(m));
					}	}

					if ( !hasWork && xmlGetChild(n, "WORK") != NULL) {
						// Work address
						hasWork = TRUE;
						if ((m=xmlGetChild(n, "STREET")) != NULL && xmlGetText(m) != NULL) {
							hasWorkStreet = TRUE;
							if (hContact != NULL) {
								if ((o=xmlGetChild(n, "EXTADR")) != NULL && xmlGetText(o) != NULL)
									mir_sntprintf(text, SIZEOF(text), _T("%s\r\n%s"), xmlGetText(m), xmlGetText(o));
								else if ((o=xmlGetChild(n, "EXTADD")) != NULL && xmlGetText(o) != NULL)
									mir_sntprintf(text, SIZEOF(text), _T("%s\r\n%s"), xmlGetText(m), xmlGetText(o));
								else
									_tcsncpy(text, xmlGetText(m), SIZEOF(text));
								text[SIZEOF(text)-1] = '\0';
								setTString(hContact, "CompanyStreet", text);
							}
							else {
								setTString(hContact, "CompanyStreet", xmlGetText(m));
								if ((m=xmlGetChild(n, "EXTADR")) == NULL)
									m = xmlGetChild(n, "EXTADD");
								if (m != NULL && xmlGetText(m) != NULL) {
									hasWorkStreet2 = TRUE;
									setTString(hContact, "CompanyStreet2", xmlGetText(m));
						}	}	}

						if ((m=xmlGetChild(n, "LOCALITY")) != NULL && xmlGetText(m) != NULL) {
							hasWorkLocality = TRUE;
							setTString(hContact, "CompanyCity", xmlGetText(m));
						}
						if ((m=xmlGetChild(n, "REGION")) != NULL && xmlGetText(m) != NULL) {
							hasWorkRegion = TRUE;
							setTString(hContact, "CompanyState", xmlGetText(m));
						}
						if ((m=xmlGetChild(n, "PCODE")) != NULL && xmlGetText(m) != NULL) {
							hasWorkPcode = TRUE;
							setTString(hContact, "CompanyZIP", xmlGetText(m));
						}
						if ((m=xmlGetChild(n, "CTRY")) == NULL || xmlGetText(m) == NULL)	// Some bad client use <COUNTRY/> instead of <CTRY/>
							m = xmlGetChild(n, "COUNTRY");
						if (m != NULL && xmlGetText(m) != NULL) {
							hasWorkCtry = TRUE;
							setTString(hContact, "CompanyCountry", xmlGetText(m));
					}	}
				}
				else if ( !lstrcmp(xmlGetName(n), _T("TEL"))) {
					// Telephone/Fax/Cellular
					if ((m=xmlGetChild(n, "NUMBER")) != NULL && xmlGetText(m) != NULL) {
						if (hContact != NULL) {
							if ( !hasFax && xmlGetChild(n, "FAX") != NULL) {
								hasFax = TRUE;
								setTString(hContact, "Fax", xmlGetText(m));
							}
							else if ( !hasCell && xmlGetChild(n, "CELL") != NULL) {
								hasCell = TRUE;
								setTString(hContact, "Cellular", xmlGetText(m));
							}
							else if ( !hasPhone &&
								(xmlGetChild(n, "HOME") != NULL ||
								 xmlGetChild(n, "WORK") != NULL ||
								 xmlGetChild(n, "VOICE") != NULL ||
								 (xmlGetChild(n, "FAX") == NULL &&
								  xmlGetChild(n, "PAGER") == NULL &&
								  xmlGetChild(n, "MSG") == NULL &&
								  xmlGetChild(n, "CELL") == NULL &&
								  xmlGetChild(n, "VIDEO") == NULL &&
								  xmlGetChild(n, "BBS") == NULL &&
								  xmlGetChild(n, "MODEM") == NULL &&
								  xmlGetChild(n, "ISDN") == NULL &&
								  xmlGetChild(n, "PCS") == NULL))) {
								hasPhone = TRUE;
								setTString(hContact, "Phone", xmlGetText(m));
							}
						}
						else {
							char text[ 100 ];
							sprintf(text, "Phone%d", nPhone);
							setTString(text, xmlGetText(m));

							sprintf(text, "PhoneFlag%d", nPhone);
							int nFlag = 0;
							if (xmlGetChild(n,"HOME") != NULL) nFlag |= JABBER_VCTEL_HOME;
							if (xmlGetChild(n,"WORK") != NULL) nFlag |= JABBER_VCTEL_WORK;
							if (xmlGetChild(n,"VOICE") != NULL) nFlag |= JABBER_VCTEL_VOICE;
							if (xmlGetChild(n,"FAX") != NULL) nFlag |= JABBER_VCTEL_FAX;
							if (xmlGetChild(n,"PAGER") != NULL) nFlag |= JABBER_VCTEL_PAGER;
							if (xmlGetChild(n,"MSG") != NULL) nFlag |= JABBER_VCTEL_MSG;
							if (xmlGetChild(n,"CELL") != NULL) nFlag |= JABBER_VCTEL_CELL;
							if (xmlGetChild(n,"VIDEO") != NULL) nFlag |= JABBER_VCTEL_VIDEO;
							if (xmlGetChild(n,"BBS") != NULL) nFlag |= JABBER_VCTEL_BBS;
							if (xmlGetChild(n,"MODEM") != NULL) nFlag |= JABBER_VCTEL_MODEM;
							if (xmlGetChild(n,"ISDN") != NULL) nFlag |= JABBER_VCTEL_ISDN;
							if (xmlGetChild(n,"PCS") != NULL) nFlag |= JABBER_VCTEL_PCS;
							setWord(text, nFlag);
							nPhone++;
					}	}
				}
				else if ( !lstrcmp(xmlGetName(n), _T("URL"))) {
					// Homepage
					if ( !hasUrl && xmlGetText(n) != NULL) {
						hasUrl = TRUE;
						setTString(hContact, "Homepage", xmlGetText(n));
					}
				}
				else if ( !lstrcmp(xmlGetName(n), _T("ORG"))) {
					if ( !hasOrgname && !hasOrgunit) {
						if ((m=xmlGetChild(n,"ORGNAME")) != NULL && xmlGetText(m) != NULL) {
							hasOrgname = TRUE;
							setTString(hContact, "Company", xmlGetText(m));
						}
						if ((m=xmlGetChild(n,"ORGUNIT")) != NULL && xmlGetText(m) != NULL) {	// The real vCard can have multiple <ORGUNIT/> but we will only display the first one
							hasOrgunit = TRUE;
							setTString(hContact, "CompanyDepartment", xmlGetText(m));
					}	}
				}
				else if ( !lstrcmp(xmlGetName(n), _T("ROLE"))) {
					if ( !hasRole && xmlGetText(n) != NULL) {
						hasRole = TRUE;
						setTString(hContact, "Role", xmlGetText(n));
					}
				}
				else if ( !lstrcmp(xmlGetName(n), _T("TITLE"))) {
					if ( !hasTitle && xmlGetText(n) != NULL) {
						hasTitle = TRUE;
						setTString(hContact, "CompanyPosition", xmlGetText(n));
					}
				}
				else if ( !lstrcmp(xmlGetName(n), _T("DESC"))) {
					if ( !hasDesc && xmlGetText(n) != NULL) {
						hasDesc = TRUE;
						TCHAR *szMemo = JabberUnixToDosT(xmlGetText(n));
						setTString(hContact, "About", szMemo);
						mir_free(szMemo);
					}
				}
				else if ( !lstrcmp(xmlGetName(n), _T("PHOTO")))
					OnIqResultGetVcardPhoto(jid, n, hContact, hasPhoto);
		}	}

		if (hasFn && !hasNick) {
			ptrT nick( db_get_tsa(hContact, m_szModuleName, "Nick"));
			ptrT jidNick( JabberNickFromJID(jid));
			if (!nick || (jidNick && !_tcsicmp(nick, jidNick)))
				setTString(hContact, "Nick", ptrT( db_get_tsa(hContact, m_szModuleName, "FullName")));
		}
		if ( !hasFn)
			delSetting(hContact, "FullName");
		// We are not deleting "Nick"
//		if ( !hasNick)
//			delSetting(hContact, "Nick");
		if ( !hasGiven)
			delSetting(hContact, "FirstName");
		if ( !hasFamily)
			delSetting(hContact, "LastName");
		if ( !hasMiddle)
			delSetting(hContact, "MiddleName");
		if (hContact != NULL) {
			while (true) {
				if (nEmail <= 0)
					delSetting(hContact, "e-mail");
				else {
					char text[ 100 ];
					sprintf(text, "e-mail%d", nEmail-1);
					if ( db_get_s(hContact, m_szModuleName, text, &dbv)) break;
					db_free(&dbv);
					delSetting(hContact, text);
				}
				nEmail++;
			}
		}
		else {
			while (true) {
				char text[ 100 ];
				sprintf(text, "e-mail%d", nEmail);
				if ( getString(text, &dbv)) break;
				db_free(&dbv);
				delSetting(text);
				sprintf(text, "e-mailFlag%d", nEmail);
				delSetting(text);
				nEmail++;
		}	}

		if ( !hasBday) {
			delSetting(hContact, "BirthYear");
			delSetting(hContact, "BirthMonth");
			delSetting(hContact, "BirthDay");
			delSetting(hContact, "BirthDate");
			delSetting(hContact, "Age");
		}
		if ( !hasGender) {
			if (hContact != NULL)
				delSetting(hContact, "Gender");
			else
				delSetting("GenderString");
		}
		if (hContact != NULL) {
			if ( !hasPhone)
				delSetting(hContact, "Phone");
			if ( !hasFax)
				delSetting(hContact, "Fax");
			if ( !hasCell)
				delSetting(hContact, "Cellular");
		}
		else {
			while (true) {
				char text[ 100 ];
				sprintf(text, "Phone%d", nPhone);
				if ( getString(text, &dbv)) break;
				db_free(&dbv);
				delSetting(text);
				sprintf(text, "PhoneFlag%d", nPhone);
				delSetting(text);
				nPhone++;
		}	}

		if ( !hasHomeStreet)
			delSetting(hContact, "Street");
		if ( !hasHomeStreet2 && hContact == NULL)
			delSetting(hContact, "Street2");
		if ( !hasHomeLocality)
			delSetting(hContact, "City");
		if ( !hasHomeRegion)
			delSetting(hContact, "State");
		if ( !hasHomePcode)
			delSetting(hContact, "ZIP");
		if ( !hasHomeCtry)
			delSetting(hContact, "Country");
		if ( !hasWorkStreet)
			delSetting(hContact, "CompanyStreet");
		if ( !hasWorkStreet2 && hContact == NULL)
			delSetting(hContact, "CompanyStreet2");
		if ( !hasWorkLocality)
			delSetting(hContact, "CompanyCity");
		if ( !hasWorkRegion)
			delSetting(hContact, "CompanyState");
		if ( !hasWorkPcode)
			delSetting(hContact, "CompanyZIP");
		if ( !hasWorkCtry)
			delSetting(hContact, "CompanyCountry");
		if ( !hasUrl)
			delSetting(hContact, "Homepage");
		if ( !hasOrgname)
			delSetting(hContact, "Company");
		if ( !hasOrgunit)
			delSetting(hContact, "CompanyDepartment");
		if ( !hasRole)
			delSetting(hContact, "Role");
		if ( !hasTitle)
			delSetting(hContact, "CompanyPosition");
		if ( !hasDesc)
			delSetting(hContact, "About");

		if (id == m_ThreadInfo->resolveID) {
			const TCHAR *p = _tcschr(jid, '@');
			ResolveTransportNicks((p != NULL) ?  p+1 : jid);
		}
		else {
			if ((hContact = HContactFromJID(jid)) != NULL)
				ProtoBroadcastAck(hContact, ACKTYPE_GETINFO, ACKRESULT_SUCCESS, (HANDLE)1, 0);
			WindowNotify(WM_JABBER_REFRESH_VCARD);
		}
	}
	else if ( !lstrcmp(type, _T("error"))) {
		if ((hContact = HContactFromJID(jid)) != NULL)
			ProtoBroadcastAck(hContact, ACKTYPE_GETINFO, ACKRESULT_FAILED, (HANDLE)1, 0);
	}
}

void CJabberProto::OnIqResultSetVcard(HXML iqNode)
{
	Log("<iq/> iqIdSetVcard");
	if ( !xmlGetAttrValue(iqNode, _T("type")))
		return;

	WindowNotify(WM_JABBER_REFRESH_VCARD);
}

void CJabberProto::OnIqResultSetSearch(HXML iqNode)
{
	HXML queryNode, n;
	const TCHAR *type, *jid;
	int i, id;
	JABBER_SEARCH_RESULT jsr;

	Log("<iq/> iqIdGetSearch");
	if ((type = xmlGetAttrValue(iqNode, _T("type"))) == NULL) return;
	if ((id = JabberGetPacketID(iqNode)) == -1) return;

	if ( !lstrcmp(type, _T("result"))) {
		if ((queryNode=xmlGetChild(iqNode , "query")) == NULL) return;
		jsr.hdr.cbSize = sizeof(JABBER_SEARCH_RESULT);
		for (i=0; ; i++) {
			HXML itemNode = xmlGetChild(queryNode ,i);
			if ( !itemNode)
				break;

			if ( !lstrcmp(xmlGetName(itemNode), _T("item"))) {
				if ((jid=xmlGetAttrValue(itemNode, _T("jid"))) != NULL) {
					_tcsncpy(jsr.jid, jid, SIZEOF(jsr.jid));
					jsr.jid[ SIZEOF(jsr.jid)-1] = '\0';
					jsr.hdr.id  = (TCHAR*)jid;
					Log("Result jid = %S", jid);
					if ((n=xmlGetChild(itemNode , "nick")) != NULL && xmlGetText(n) != NULL)
						jsr.hdr.nick = (TCHAR*)xmlGetText(n);
					else
						jsr.hdr.nick = _T("");
					if ((n=xmlGetChild(itemNode , "first")) != NULL && xmlGetText(n) != NULL)
						jsr.hdr.firstName = (TCHAR*)xmlGetText(n);
					else
						jsr.hdr.firstName = _T("");
					if ((n=xmlGetChild(itemNode , "last")) != NULL && xmlGetText(n) != NULL)
						jsr.hdr.lastName = (TCHAR*)xmlGetText(n);
					else
						jsr.hdr.lastName = _T("");
					if ((n=xmlGetChild(itemNode , "email")) != NULL && xmlGetText(n) != NULL)
						jsr.hdr.email = (TCHAR*)xmlGetText(n);
					else
						jsr.hdr.email = _T("");
					jsr.hdr.flags = PSR_TCHAR;
					ProtoBroadcastAck(NULL, ACKTYPE_SEARCH, ACKRESULT_DATA, (HANDLE)id, (LPARAM)&jsr);
		}	}	}

		ProtoBroadcastAck(NULL, ACKTYPE_SEARCH, ACKRESULT_SUCCESS, (HANDLE)id, 0);
	}
	else if ( !lstrcmp(type, _T("error")))
		ProtoBroadcastAck(NULL, ACKTYPE_SEARCH, ACKRESULT_SUCCESS, (HANDLE)id, 0);
}

void CJabberProto::OnIqResultExtSearch(HXML iqNode)
{
	HXML queryNode;
	const TCHAR *type;
	int id;

	Log("<iq/> iqIdGetExtSearch");
	if ((type=xmlGetAttrValue(iqNode, _T("type"))) == NULL) return;
	if ((id = JabberGetPacketID(iqNode)) == -1) return;

	if ( !lstrcmp(type, _T("result"))) {
		if ((queryNode=xmlGetChild(iqNode , "query")) == NULL) return;
		if ((queryNode=xmlGetChild(queryNode , "x")) == NULL) return;
		for (int i=0; ; i++) {
			HXML itemNode = xmlGetChild(queryNode ,i);
			if ( !itemNode)
				break;
			if (lstrcmp(xmlGetName(itemNode), _T("item")))
				continue;

			JABBER_SEARCH_RESULT jsr = { 0 };
			jsr.hdr.cbSize = sizeof(JABBER_SEARCH_RESULT);
			jsr.hdr.flags = PSR_TCHAR;
//			jsr.hdr.firstName = "";

			for (int j=0; ; j++) {
				HXML fieldNode = xmlGetChild(itemNode ,j);
				if ( !fieldNode)
					break;

				if (lstrcmp(xmlGetName(fieldNode), _T("field")))
					continue;

				const TCHAR *fieldName = xmlGetAttrValue(fieldNode, _T("var"));
				if (fieldName == NULL)
					continue;

				HXML n = xmlGetChild(fieldNode , "value");
				if (n == NULL)
					continue;

				if ( !lstrcmp(fieldName, _T("jid"))) {
					_tcsncpy(jsr.jid, xmlGetText(n), SIZEOF(jsr.jid));
					jsr.jid[SIZEOF(jsr.jid)-1] = '\0';
					Log("Result jid = %S", jsr.jid);
				}
				else if ( !lstrcmp(fieldName, _T("nickname")))
					jsr.hdr.nick = (xmlGetText(n) != NULL) ? (TCHAR*)xmlGetText(n) : _T("");
				else if ( !lstrcmp(fieldName, _T("fn")))
					jsr.hdr.firstName = (xmlGetText(n) != NULL) ? (TCHAR*)xmlGetText(n) : _T("");
				else if ( !lstrcmp(fieldName, _T("given")))
					jsr.hdr.firstName = (xmlGetText(n) != NULL) ? (TCHAR*)xmlGetText(n) : _T("");
				else if ( !lstrcmp(fieldName, _T("family")))
					jsr.hdr.lastName = (xmlGetText(n) != NULL) ? (TCHAR*)xmlGetText(n) : _T("");
				else if ( !lstrcmp(fieldName, _T("email")))
					jsr.hdr.email = (xmlGetText(n) != NULL) ? (TCHAR*)xmlGetText(n) : _T("");
			}

			ProtoBroadcastAck(NULL, ACKTYPE_SEARCH, ACKRESULT_DATA, (HANDLE)id, (LPARAM)&jsr);
		}

		ProtoBroadcastAck(NULL, ACKTYPE_SEARCH, ACKRESULT_SUCCESS, (HANDLE)id, 0);
	}
	else if ( !lstrcmp(type, _T("error")))
		ProtoBroadcastAck(NULL, ACKTYPE_SEARCH, ACKRESULT_SUCCESS, (HANDLE)id, 0);
}

void CJabberProto::OnIqResultSetPassword(HXML iqNode)
{
	Log("<iq/> iqIdSetPassword");

	const TCHAR *type = xmlGetAttrValue(iqNode, _T("type"));
	if (type == NULL)
		return;

	if ( !lstrcmp(type, _T("result"))) {
		_tcsncpy(m_ThreadInfo->password, m_ThreadInfo->newPassword, SIZEOF(m_ThreadInfo->password));
		MessageBox(NULL, TranslateT("Password is successfully changed. Don't forget to update your password in the Jabber protocol option."), TranslateT("Change Password"), MB_OK|MB_ICONINFORMATION|MB_SETFOREGROUND);
	}
	else if ( !lstrcmp(type, _T("error")))
		MessageBox(NULL, TranslateT("Password cannot be changed."), TranslateT("Change Password"), MB_OK|MB_ICONSTOP|MB_SETFOREGROUND);
}
/*
void CJabberProto::OnIqResultDiscoAgentItems(HXML iqNode, void *userdata)
{
	if ( !m_options.EnableAvatars)
		return;
}
*/
void CJabberProto::OnIqResultGetVCardAvatar(HXML iqNode)
{
	const TCHAR *type;

	Log("<iq/> OnIqResultGetVCardAvatar");

	const TCHAR *from = xmlGetAttrValue(iqNode, _T("from"));
	if (from == NULL)
		return;
	HANDLE hContact = HContactFromJID(from);
	if (hContact == NULL)
		return;

	if ((type = xmlGetAttrValue(iqNode, _T("type"))) == NULL) return;
	if (_tcscmp(type, _T("result"))) return;

	HXML vCard = xmlGetChild(iqNode , "vCard");
	if (vCard == NULL) return;
	vCard = xmlGetChild(vCard , "PHOTO");
	if (vCard == NULL) return;

	if (xmlGetChildCount(vCard) == 0) {
		delSetting(hContact, "AvatarHash");
		DBVARIANT dbv = {0};
		if ( !getTString(hContact, "AvatarSaved", &dbv)) {
			db_free(&dbv);
			delSetting(hContact, "AvatarSaved");
			ProtoBroadcastAck(hContact, ACKTYPE_AVATAR, ACKRESULT_SUCCESS, NULL, NULL);
		}

		return;
	}

	const TCHAR *mimeType = xmlGetText( xmlGetChild(vCard , "TYPE"));
	HXML n = xmlGetChild(vCard , "BINVAL");
	if (n == NULL)
		return;

	setByte(hContact, "AvatarXVcard", 1);
	OnIqResultGotAvatar(hContact, n, mimeType);
}

void CJabberProto::OnIqResultGetClientAvatar(HXML iqNode)
{
	const TCHAR *type;

	Log("<iq/> iqIdResultGetClientAvatar");

	const TCHAR *from = xmlGetAttrValue(iqNode, _T("from"));
	if (from == NULL)
		return;
	HANDLE hContact = HContactFromJID(from);
	if (hContact == NULL)
		return;

	HXML n = NULL;
	if ((type = xmlGetAttrValue(iqNode, _T("type"))) != NULL && !_tcscmp(type, _T("result"))) {
		HXML queryNode = xmlGetChild(iqNode , "query");
		if (queryNode != NULL) {
			const TCHAR *xmlns = xmlGetAttrValue(queryNode, _T("xmlns"));
			if ( !lstrcmp(xmlns, JABBER_FEAT_AVATAR))
				n = xmlGetChild(queryNode , "data");
		}
	}

	if (n == NULL) {
		TCHAR szJid[JABBER_MAX_JID_LEN];
		lstrcpyn(szJid, from, SIZEOF(szJid));
		TCHAR *res = _tcschr(szJid, _T('/'));
		if (res != NULL)
			*res = 0;

		// Try server stored avatar
		int iqId = SerialNext();
		IqAdd(iqId, IQ_PROC_NONE, &CJabberProto::OnIqResultGetServerAvatar);

		XmlNodeIq iq(_T("get"), iqId, szJid);
		iq << XQUERY(JABBER_FEAT_SERVER_AVATAR);
		m_ThreadInfo->send(iq);

		return;
	}

	const TCHAR *mimeType = mimeType = xmlGetAttrValue(n, _T("mimetype"));

	OnIqResultGotAvatar(hContact, n, mimeType);
}


void CJabberProto::OnIqResultGetServerAvatar(HXML iqNode)
{
	const TCHAR *type;

	Log("<iq/> iqIdResultGetServerAvatar");

	const TCHAR *from = xmlGetAttrValue(iqNode, _T("from"));
	if (from == NULL)
		return;
	HANDLE hContact = HContactFromJID(from);
	if (hContact == NULL)
		return;

	HXML n = NULL;
	if ((type = xmlGetAttrValue(iqNode, _T("type"))) != NULL && !_tcscmp(type, _T("result"))) {
		HXML queryNode = xmlGetChild(iqNode , "query");
		if (queryNode != NULL) {
			const TCHAR *xmlns = xmlGetAttrValue(queryNode, _T("xmlns"));
			if ( !lstrcmp(xmlns, JABBER_FEAT_SERVER_AVATAR))
				n = xmlGetChild(queryNode , "data");
		}
	}

	if (n == NULL) {
		TCHAR szJid[JABBER_MAX_JID_LEN];
		lstrcpyn(szJid, from, SIZEOF(szJid));
		TCHAR *res = _tcschr(szJid, _T('/'));
		if (res != NULL)
			*res = 0;

		// Try VCard photo
		int iqId = SerialNext();
		IqAdd(iqId, IQ_PROC_NONE, &CJabberProto::OnIqResultGetVCardAvatar);

		XmlNodeIq iq(_T("get"), iqId, szJid);
		iq << XCHILDNS(_T("vCard"), JABBER_FEAT_VCARD_TEMP);
		m_ThreadInfo->send(iq);

		return;
	}

	const TCHAR *mimeType = xmlGetAttrValue(n, _T("mimetype"));

	OnIqResultGotAvatar(hContact, n, mimeType);
}


void CJabberProto::OnIqResultGotAvatar(HANDLE hContact, HXML n, const TCHAR *mimeType)
{
	unsigned resultLen;
	ptrA body((char*)mir_base64_decode( _T2A(xmlGetText(n)), &resultLen));

	int pictureType;
	if (mimeType != NULL) {
		     if ( !lstrcmp(mimeType, _T("image/jpeg"))) pictureType = PA_FORMAT_JPEG;
		else if ( !lstrcmp(mimeType, _T("image/png")))  pictureType = PA_FORMAT_PNG;
		else if ( !lstrcmp(mimeType, _T("image/gif")))  pictureType = PA_FORMAT_GIF;
		else if ( !lstrcmp(mimeType, _T("image/bmp")))  pictureType = PA_FORMAT_BMP;
		else {
LBL_ErrFormat:
			Log("Invalid mime type specified for picture: %S", mimeType);
			return;
	}	}
	else if ((pictureType = JabberGetPictureType(body)) == PA_FORMAT_UNKNOWN)
		goto LBL_ErrFormat;

	TCHAR tszFileName[ MAX_PATH ];

	PROTO_AVATAR_INFORMATIONT AI;
	AI.cbSize = sizeof AI;
	AI.format = pictureType;
	AI.hContact = hContact;

	if (getByte(hContact, "AvatarType", PA_FORMAT_UNKNOWN) != (unsigned char)pictureType) {
		GetAvatarFileName(hContact, tszFileName, SIZEOF(tszFileName));
		DeleteFile(tszFileName);
	}

	setByte(hContact, "AvatarType", pictureType);

	char buffer[ 41 ];
	mir_sha1_byte_t digest[20];
	mir_sha1_ctx sha;
	mir_sha1_init(&sha);
	mir_sha1_append(&sha, (mir_sha1_byte_t*)(char*)body, resultLen);
	mir_sha1_finish(&sha, digest);
	for (int i=0; i<20; i++)
		sprintf(buffer+(i<<1), "%02x", digest[i]);

	GetAvatarFileName(hContact, tszFileName, SIZEOF(tszFileName));
	_tcsncpy(AI.filename, tszFileName, SIZEOF(AI.filename));

	FILE* out = _tfopen(tszFileName, _T("wb"));
	if (out != NULL) {
		fwrite(body, resultLen, 1, out);
		fclose(out);
		setString(hContact, "AvatarSaved", buffer);
		ProtoBroadcastAck(hContact, ACKTYPE_AVATAR, ACKRESULT_SUCCESS, HANDLE(&AI), NULL);
		Log("Broadcast new avatar: %s",AI.filename);
	}
	else ProtoBroadcastAck(hContact, ACKTYPE_AVATAR, ACKRESULT_FAILED, HANDLE(&AI), NULL);
}

/////////////////////////////////////////////////////////////////////////////////////////
// Bookmarks

void CJabberProto::OnIqResultDiscoBookmarks(HXML iqNode)
{
	HXML storageNode;//, nickNode, passNode;
	const TCHAR *type, *jid, *name;

	// RECVED: list of bookmarks
	// ACTION: refresh bookmarks dialog
	Log("<iq/> iqIdGetBookmarks");
	if ((type = xmlGetAttrValue(iqNode, _T("type"))) == NULL) return;

	if ( !lstrcmp(type, _T("result"))) {
		if (m_ThreadInfo && !(m_ThreadInfo->jabberServerCaps & JABBER_CAPS_PRIVATE_STORAGE)) {
			m_ThreadInfo->jabberServerCaps |= JABBER_CAPS_PRIVATE_STORAGE;
			EnableMenuItems(TRUE);
		}

		if (storageNode = XPathT(iqNode, "query/storage[@xmlns='storage:bookmarks']")) {
			ListRemoveList(LIST_BOOKMARK);

			HXML itemNode;
			for (int i = 0; itemNode = xmlGetChild(storageNode, i); i++) {
				if (name = xmlGetName(itemNode)) {
					if ( !_tcscmp(name, _T("conference")) && (jid = xmlGetAttrValue(itemNode, _T("jid")))) {
						JABBER_LIST_ITEM *item = ListAdd(LIST_BOOKMARK, jid);
						item->name = mir_tstrdup(xmlGetAttrValue(itemNode, _T("name")));
						item->type = mir_tstrdup(_T("conference"));
						item->bUseResource = TRUE;
						item->nick = mir_tstrdup(XPathT(itemNode, "nick"));
						item->password = mir_tstrdup(XPathT(itemNode, "password"));

						const TCHAR *autoJ = xmlGetAttrValue(itemNode, _T("autojoin"));
						if (autoJ != NULL)
							item->bAutoJoin = (!lstrcmp(autoJ, _T("true")) || !lstrcmp(autoJ, _T("1"))) ? true : false;
					}
					else if ( !_tcscmp(name, _T("url")) && (jid = xmlGetAttrValue(itemNode, _T("url") ))) {
						JABBER_LIST_ITEM *item = ListAdd(LIST_BOOKMARK, jid);
						item->bUseResource = TRUE;
						item->name = mir_tstrdup(xmlGetAttrValue(itemNode, _T("name")));
						item->type = mir_tstrdup(_T("url"));
					}
				}
			}

			UI_SAFE_NOTIFY(m_pDlgBookmarks, WM_JABBER_REFRESH);
			m_ThreadInfo->bBookmarksLoaded = TRUE;
			OnProcessLoginRq(m_ThreadInfo, JABBER_LOGIN_BOOKMARKS);
		}
	}
	else if ( !lstrcmp(type, _T("error"))) {
		if (m_ThreadInfo->jabberServerCaps & JABBER_CAPS_PRIVATE_STORAGE) {
			m_ThreadInfo->jabberServerCaps &= ~JABBER_CAPS_PRIVATE_STORAGE;
			EnableMenuItems(TRUE);
			UI_SAFE_NOTIFY(m_pDlgBookmarks, WM_JABBER_ACTIVATE);
			return;
		}
}	}

void CJabberProto::SetBookmarkRequest (XmlNodeIq& iq)
{
	HXML query = iq << XQUERY(JABBER_FEAT_PRIVATE_STORAGE);
	HXML storage = query << XCHILDNS(_T("storage"), _T("storage:bookmarks"));

	LISTFOREACH(i, this, LIST_BOOKMARK)
	{
		JABBER_LIST_ITEM *item = ListGetItemPtrFromIndex(i);
		if (item == NULL)
			continue;

		if (item->jid == NULL)
			continue;
		if ( !lstrcmp(item->type, _T("conference"))) {
			HXML itemNode = storage << XCHILD(_T("conference")) << XATTR(_T("jid"), item->jid);
			if (item->name)
				itemNode << XATTR(_T("name"), item->name);
			if (item->bAutoJoin)
				itemNode << XATTRI(_T("autojoin"), 1);
			if (item->nick)
				itemNode << XCHILD(_T("nick"), item->nick);
			if (item->password)
				itemNode << XCHILD(_T("password"), item->password);
		}
		if ( !lstrcmp(item->type, _T("url"))) {
			HXML itemNode = storage << XCHILD(_T("url")) << XATTR(_T("url"), item->jid);
			if (item->name)
				itemNode << XATTR(_T("name"), item->name);
		}
	}
}

void CJabberProto::OnIqResultSetBookmarks(HXML iqNode)
{
	// RECVED: server's response
	// ACTION: refresh bookmarks list dialog

	Log("<iq/> iqIdSetBookmarks");

	const TCHAR *type = xmlGetAttrValue(iqNode, _T("type"));
	if (type == NULL)
		return;

	if ( !lstrcmp(type, _T("result"))) {
		UI_SAFE_NOTIFY(m_pDlgBookmarks, WM_JABBER_REFRESH);
	}
	else if ( !lstrcmp(type, _T("error"))) {
		HXML errorNode = xmlGetChild(iqNode , "error");
		TCHAR *str = JabberErrorMsg(errorNode);
		MessageBox(NULL, str, TranslateT("Jabber Bookmarks Error"), MB_OK|MB_SETFOREGROUND);
		mir_free(str);
		UI_SAFE_NOTIFY(m_pDlgBookmarks, WM_JABBER_ACTIVATE);
}	}

// last activity (XEP-0012) support
void CJabberProto::OnIqResultLastActivity(HXML iqNode, CJabberIqInfo* pInfo)
{
	JABBER_RESOURCE_STATUS *r = ResourceInfoFromJID(pInfo->m_szFrom);
	if ( !r)
		return;

	time_t lastActivity = -1;
	if (pInfo->m_nIqType == JABBER_IQ_TYPE_RESULT) {
		LPCTSTR szSeconds = XPathT(iqNode, "query[@xmlns='jabber:iq:last']/@seconds");
		if (szSeconds) {
			int nSeconds = _ttoi(szSeconds);
			if (nSeconds > 0)
				lastActivity = time(0) - nSeconds;
		}

		LPCTSTR szLastStatusMessage = XPathT(iqNode, "query[@xmlns='jabber:iq:last']");
		if (szLastStatusMessage) // replace only if it exists
			replaceStrT(r->statusMessage, szLastStatusMessage);
	}

	r->idleStartTime = lastActivity;

	JabberUserInfoUpdate(pInfo->GetHContact());
}

// entity time (XEP-0202) support
void CJabberProto::OnIqResultEntityTime(HXML pIqNode, CJabberIqInfo* pInfo)
{
	if ( !pInfo->m_hContact)
		return;

	if (pInfo->m_nIqType == JABBER_IQ_TYPE_RESULT) {
		LPCTSTR szTzo = XPathFmt(pIqNode, _T("time[@xmlns='%s']/tzo"), JABBER_FEAT_ENTITY_TIME);
		if (szTzo && szTzo[0]) {
			LPCTSTR szMin = _tcschr(szTzo, ':');
			int nTz = _ttoi(szTzo) * -2;
			nTz += (nTz < 0 ? -1 : 1) * (szMin ? _ttoi(szMin + 1) / 30 : 0);

			TIME_ZONE_INFORMATION tzinfo;
			if (GetTimeZoneInformation(&tzinfo) == TIME_ZONE_ID_DAYLIGHT)
				nTz -= tzinfo.DaylightBias / 30;

			setByte(pInfo->m_hContact, "Timezone", (signed char)nTz);

			LPCTSTR szTz = XPathFmt(pIqNode, _T("time[@xmlns='%s']/tz"), JABBER_FEAT_ENTITY_TIME);
			if (szTz)
				setTString(pInfo->m_hContact, "TzName", szTz);
			else
				delSetting(pInfo->m_hContact, "TzName");
			return;
		}
	}
	else if (pInfo->m_nIqType == JABBER_IQ_TYPE_ERROR)
	{
		if (getWord(pInfo->m_hContact, "Status", ID_STATUS_OFFLINE) == ID_STATUS_OFFLINE)
			return;
	}

	delSetting(pInfo->m_hContact, "Timezone");
	delSetting(pInfo->m_hContact, "TzName");
}
