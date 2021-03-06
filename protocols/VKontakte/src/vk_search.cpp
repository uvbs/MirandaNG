/*
Copyright (c) 2013-15 Miranda NG project (http://miranda-ng.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2
of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "stdafx.h"

HANDLE CVkProto::SearchBasic(const TCHAR* id)
{
	ForkThread(&CVkProto::SearchBasicThread, (void *)id);
	return (HANDLE)1;
}

HANDLE CVkProto::SearchByEmail(const TCHAR* email)
{
	ForkThread(&CVkProto::SearchByMailThread, (void *)email);
	return (HANDLE)1;
}

HANDLE CVkProto::SearchByName(const TCHAR* nick, const TCHAR* firstName, const TCHAR* lastName)
{
	PROTOSEARCHBYNAME * psr = new (PROTOSEARCHBYNAME);

	psr->pszFirstName = mir_tstrdup(firstName);
	psr->pszLastName = mir_tstrdup(lastName);
	psr->pszNick = mir_tstrdup(nick);

	ForkThread(&CVkProto::SearchThread, (void *)psr);
	return (HANDLE)1;
}

void CVkProto::SearchBasicThread(void* id)
{
	debugLogA("CVkProto::OnSearchBasicThread");
	if (!IsOnline())
		return;
	AsyncHttpRequest *pReq = new AsyncHttpRequest(this, REQUEST_GET, "/method/users.get.json", true, &CVkProto::OnSearch)
		<< TCHAR_PARAM("user_ids", (TCHAR *)id)
		<< CHAR_PARAM("fields", "nickname, domain")
		<< VER_API;
	pReq->pUserInfo = NULL;
	Push(pReq);
}

void CVkProto::SearchByMailThread(void* email)
{
	debugLogA("CVkProto::OnSearchBasicThread");
	if (!IsOnline())
		return;
	AsyncHttpRequest *pReq = new AsyncHttpRequest(this, REQUEST_GET, "/method/account.lookupContacts.json", true, &CVkProto::OnSearchByMail)
		<< TCHAR_PARAM("contacts", (TCHAR *)email)
		<< CHAR_PARAM("service", "email")
		<< VER_API;
	Push(pReq);
}

void __cdecl CVkProto::SearchThread(void* p)
{
	PROTOSEARCHBYNAME *pParam = (PROTOSEARCHBYNAME *)p;
		
	TCHAR arg[200];
	mir_sntprintf(arg, _T("%s %s %s"), pParam->pszFirstName, pParam->pszNick, pParam->pszLastName);
	debugLog(_T("CVkProto::SearchThread %s"), arg);
	if (!IsOnline())
		return;

	AsyncHttpRequest *pReq = new AsyncHttpRequest(this, REQUEST_GET, "/method/users.search.json", true, &CVkProto::OnSearch)
		<< TCHAR_PARAM("q", (TCHAR *)arg)
		<< CHAR_PARAM("fields", "nickname, domain")
		<< INT_PARAM("count", 200)
		<< VER_API;
	pReq->pUserInfo = p;
	Push(pReq);
}

void CVkProto::FreeProtoShearchStruct(PROTOSEARCHBYNAME *pParam)
{
	if (!pParam)
		return;
	
	mir_free(pParam->pszFirstName);
	mir_free(pParam->pszLastName);
	mir_free(pParam->pszNick);
	delete pParam;
}

void CVkProto::OnSearch(NETLIBHTTPREQUEST *reply, AsyncHttpRequest *pReq)
{
	PROTOSEARCHBYNAME *pParam = (PROTOSEARCHBYNAME *)pReq->pUserInfo;
	debugLogA("CVkProto::OnSearch %d", reply->resultCode);
	if (reply->resultCode != 200) {
		FreeProtoShearchStruct(pParam);
		ProtoBroadcastAck(0, ACKTYPE_SEARCH, ACKRESULT_SUCCESS, (HANDLE)1);
		return;
	}

	JSONNode jnRoot;
	const JSONNode &jnResponse = CheckJsonResponse(pReq, reply, jnRoot);
	if (!jnResponse) {
		FreeProtoShearchStruct(pParam);
		ProtoBroadcastAck(0, ACKTYPE_SEARCH, ACKRESULT_SUCCESS, (HANDLE)1);
		return;
	}

	const JSONNode &jnItems = !jnResponse["items"] ? jnResponse : jnResponse["items"];
	for (auto it = jnItems.begin(); it != jnItems.end(); ++it) {
		const JSONNode &jnRecord = (*it);
		if (!jnRecord)
			break;
		
		PROTOSEARCHRESULT psr = { sizeof(psr) };
		psr.flags = PSR_TCHAR;

		CMString Id(FORMAT, _T("%d"), jnRecord["id"].as_int());
		CMString FirstName(jnRecord["first_name"].as_mstring());
		CMString LastName(jnRecord["last_name"].as_mstring());
		CMString Nick(jnRecord["nickname"].as_mstring());
		CMString Domain(jnRecord["domain"].as_mstring());

		psr.id.t = mir_tstrdup(Id);
		psr.firstName.t = mir_tstrdup(FirstName);
		psr.lastName.t = mir_tstrdup(LastName);
		psr.nick.t = Nick.IsEmpty() ? mir_tstrdup(Domain) : mir_tstrdup(Nick);
		
		bool filter = true;
		if (pParam) {
			if (psr.firstName.t && pParam->pszFirstName)
				filter = tlstrstr(psr.firstName.t, pParam->pszFirstName) && filter;
			if (psr.lastName.t && pParam->pszLastName)
				filter = tlstrstr(psr.lastName.t, pParam->pszLastName) && filter;
			if (psr.nick.t && pParam->pszNick)
				filter = tlstrstr(psr.nick.t, pParam->pszNick) && filter;
		}

		if (filter)
			ProtoBroadcastAck(0, ACKTYPE_SEARCH, ACKRESULT_DATA, (HANDLE)1, (LPARAM)&psr);
	}

	ProtoBroadcastAck(0, ACKTYPE_SEARCH, ACKRESULT_SUCCESS, (HANDLE)1);
	FreeProtoShearchStruct(pParam);
}

void CVkProto::OnSearchByMail(NETLIBHTTPREQUEST *reply, AsyncHttpRequest *pReq)
{
	debugLogA("CVkProto::OnSearch %d", reply->resultCode);
	if (reply->resultCode != 200) {
		ProtoBroadcastAck(0, ACKTYPE_SEARCH, ACKRESULT_SUCCESS, (HANDLE)1);
		return;
	}

	JSONNode jnRoot;
	const JSONNode &jnResponse = CheckJsonResponse(pReq, reply, jnRoot);
	if (!jnResponse) {
		ProtoBroadcastAck(0, ACKTYPE_SEARCH, ACKRESULT_SUCCESS, (HANDLE)1);
		return;
	}

	const JSONNode &jnItems = jnResponse["found"];
	if (!jnItems) {
		ProtoBroadcastAck(0, ACKTYPE_SEARCH, ACKRESULT_SUCCESS, (HANDLE)1);
		return;
	}
	
	for (auto it = jnItems.begin(); it != jnItems.end(); ++it) {
		const JSONNode &jnRecord = (*it);
		if (!jnRecord)
			break;

		PROTOSEARCHRESULT psr = { sizeof(psr) };
		psr.flags = PSR_TCHAR;
		
		CMString Id(FORMAT, _T("%d"), jnRecord["id"].as_int());
		CMString FirstName(jnRecord["first_name"].as_mstring());
		CMString LastName(jnRecord["last_name"].as_mstring());
		CMString Nick(jnRecord["nickname"].as_mstring());
		CMString Email(jnRecord["contact"].as_mstring());


		psr.id.t = mir_tstrdup(Id);
		psr.firstName.t = mir_tstrdup(FirstName);
		psr.lastName.t = mir_tstrdup(LastName);
		psr.nick.t = Nick.IsEmpty() ? mir_tstrdup(Email) : mir_tstrdup(Nick);
		psr.email.t = mir_tstrdup(Email);
			
		ProtoBroadcastAck(0, ACKTYPE_SEARCH, ACKRESULT_DATA, (HANDLE)1, (LPARAM)&psr);
	}

	ProtoBroadcastAck(0, ACKTYPE_SEARCH, ACKRESULT_SUCCESS, (HANDLE)1);
}