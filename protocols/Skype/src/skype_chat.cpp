#include "skype.h"
#include "skype_chat.h"

enum CHAT_LIST_MENU 
{
	ICM_CANCEL,

	ICM_DETAILS,
	ICM_AUTH_REQUEST,
	ICM_CONF_INVITE,
	ICM_ROLE, ICM_ROLE_ADMIN, ICM_ROLE_SPEAKER, ICM_ROLE_WRITER, ICM_ROLE_SPECTATOR,
	ICM_ADD, ICM_KICK, ICM_BAN,
	ICM_COPY_SID, ICM_COPY_URI
};

static struct gc_item crListItems[] =
{
	{ LPGENT("&User details"),        ICM_DETAILS,            MENU_ITEM      }, 
	{ LPGENT("&Request auth"),        ICM_AUTH_REQUEST,       MENU_ITEM      },
	{ NULL,                           0,                      MENU_SEPARATOR },
	{ LPGENT("Invite to conference"), ICM_CONF_INVITE,        MENU_ITEM      },
	{ NULL,                           0,                      MENU_SEPARATOR },
	{ LPGENT("Set &role"),            ICM_ROLE,               MENU_NEWPOPUP  },
	{ LPGENT("&Master"),              ICM_ROLE_ADMIN,         MENU_POPUPITEM },
	{ LPGENT("&Helper"),              ICM_ROLE_SPEAKER,       MENU_POPUPITEM },
	{ LPGENT("&User"),                ICM_ROLE_WRITER,        MENU_POPUPITEM },
	{ LPGENT("&Listener"),            ICM_ROLE_SPECTATOR,     MENU_POPUPITEM },
	{ NULL,                           0,                      MENU_SEPARATOR },
	{ LPGENT("&Add"),                 ICM_ADD,                MENU_ITEM      },
	{ LPGENT("&Kick"),                ICM_KICK,               MENU_ITEM      },
	{ LPGENT("Outlaw (&ban)"),        ICM_BAN,                MENU_ITEM      },	
	{ NULL,                           0,                      MENU_SEPARATOR },
	{ LPGENT("Copy &skypename"),      ICM_COPY_SID,           MENU_ITEM      },
	{ LPGENT("Copy room &uri"),       ICM_COPY_URI,           MENU_ITEM      }
};

static void CheckChatMenuItem(CHAT_LIST_MENU checkedId)
{
	for (int i = 0; i < SIZEOF(crListItems); i++)
	{
		if (crListItems[i].dwID == checkedId)
		{
			if (crListItems[i].uType == MENU_ITEM)
				crListItems[i].uType = MENU_CHECK;
			else if (crListItems[i].uType == MENU_POPUPITEM)
				crListItems[i].uType = MENU_POPUPCHECK;
			break;
		}
	}
}

static void DisableChatMenuItem(CHAT_LIST_MENU disabledId)
{
	for (int i = 0; i < SIZEOF(crListItems); i++)
	{
		if (crListItems[i].dwID == disabledId)
		{
			crListItems[i].bDisabled = TRUE;
			break;
		}
	}
}

static void DisableChatMenuItems(CHAT_LIST_MENU disabledIds[])
{
	for (int i = 0; i < SIZEOF(disabledIds); i++)
		DisableChatMenuItem(disabledIds[i]);
}

static void ResetChatMenuItem()
{
	for (int i = 0; i < SIZEOF(crListItems); i++)
	{
		crListItems[i].bDisabled = FALSE;
		if (crListItems[i].uType == MENU_CHECK)
			crListItems[i].uType = MENU_ITEM;
		else if (crListItems[i].uType == MENU_POPUPCHECK)
			crListItems[i].uType = MENU_POPUPITEM;
	}
}

void CSkypeProto::InitChat()
{
	GCREGISTER gcr = {0};
	gcr.cbSize = sizeof(gcr);
	gcr.dwFlags = GC_TCHAR;
	gcr.iMaxText = 0;
	gcr.ptszModuleDispName = this->m_tszUserName;
	gcr.pszModule = this->m_szModuleName;
	::CallServiceSync(MS_GC_REGISTER, 0, (LPARAM)&gcr);

	this->HookEvent(ME_GC_EVENT, &CSkypeProto::OnGCEventHook);
	this->HookEvent(ME_GC_BUILDMENU, &CSkypeProto::OnGCMenuHook);
}

///

wchar_t *ChatRoom::Roles[] = 
{ 
	L"",			// ---
	L"Creator",		// CREATOR	= 1
	L"Master",		// ADMIN	= 2
	L"Helper",		// SPEAKER	= 3
	L"User",		// WRITER	= 4
	L"Listener",	// SPECTATOR= 5
	L"Applicant",	// APPLICANT= 6
	L"Retried",		// RETIRED	= 7
	L"Outlaw",		// OUTLAW	= 8
};

ChatRoom::ChatRoom(const wchar_t *cid) : members(1, CompareMembers) 
{
	this->cid = ::mir_wstrdup(cid);
	this->name = NULL;
	this->me = NULL;
}

ChatRoom::ChatRoom(const wchar_t *cid, const wchar_t *name, CSkypeProto *ppro) : members(1, CompareMembers) 
{
	this->cid = ::mir_wstrdup(cid);
	this->name = ::mir_wstrdup(name);
	this->ppro = ppro;
	//
	this->me = new ChatMember(ppro->login);
	this->me->SetNick(::TranslateT("me"));
	this->me->SetRank(0);
	this->me->SetStatus(ID_STATUS_OFFLINE);
	//
	this->sys = new ChatMember(L"sys");
	this->sys->SetNick(L"System");
	this->sys->SetRank(0);
	this->sys->SetStatus(ID_STATUS_OFFLINE);
}

ChatRoom::~ChatRoom()
{
	if (this->cid != NULL)
		::mir_free(this->cid);
	if (this->name != NULL)
		::mir_free(this->name);
	if (this->me != NULL)
		delete this->me;
	this->members.destroy();
}

void ChatRoom::CreateChatSession(bool showWindow)
{
	SEString data;

	// start chat session
	GCSESSION gcw = {0};
	gcw.cbSize = sizeof(gcw);
	gcw.iType = GCW_CHATROOM;
	gcw.dwFlags = GC_TCHAR;
	gcw.pszModule = ppro->m_szModuleName;
	gcw.ptszName = this->name;
	gcw.ptszID = this->cid;
	gcw.dwItemData = (DWORD)this;
	::CallServiceSync(MS_GC_NEWSESSION, 0, (LPARAM)&gcw);

	GCDEST gcd = { ppro->m_szModuleName, { NULL }, GC_EVENT_ADDGROUP };
	gcd.ptszID = this->cid;

	// load chat roles
	GCEVENT gce = {0};
	gce.cbSize = sizeof(GCEVENT);
	gce.dwFlags = GC_TCHAR;
	gce.pDest = &gcd;
	
	for (int i = 1; i < SIZEOF(ChatRoom::Roles) - 2; i++)
	{
		gce.ptszStatus = ::TranslateW(ChatRoom::Roles[i]);
		::CallServiceSync(MS_GC_EVENT, 0, (LPARAM)&gce);
	}

	// init [and show window]
	gcd.iType = GC_EVENT_CONTROL;
	gce.ptszStatus = NULL;
	::CallServiceSync(MS_GC_EVENT, showWindow ? SESSION_INITDONE : WINDOW_HIDDEN, (LPARAM)&gce);
	::CallServiceSync(MS_GC_EVENT, SESSION_ONLINE, (LPARAM)&gce);
}

void ChatRoom::Create(const StringList &invitedMembers, CSkypeProto *ppro, bool showWindow)
{
	SEString data;
	ChatRoom *room = NULL;

	ConversationRef conversation;
	if (ppro->CreateConference(conversation))
	{
		conversation->SetOption(CConversation::P_OPT_JOINING_ENABLED, true);
		conversation->SetOption(CConversation::P_OPT_ENTRY_LEVEL_RANK, CParticipant::WRITER);
		conversation->SetOption(CConversation::P_OPT_DISCLOSE_HISTORY, true);
		
		SEStringList consumers;
		for (size_t i = 0; i < invitedMembers.size(); i++)
		{
			data = ::mir_utf8encodeW(invitedMembers[i]);
			consumers.append(data);
		}
		conversation->AddConsumers(consumers);
	}
}

void ChatRoom::Start(const ConversationRef &conversation, bool showWindow)
{
	SEString data;

	this->CreateChatSession(showWindow);

	this->conversation = conversation;
	this->conversation.fetch();

	GC_INFO gci = {0};
	gci.Flags = BYID | HCONTACT;
	gci.pszModule = ppro->m_szModuleName;
	gci.pszID = this->cid;

	if ( !::CallServiceSync(MS_GC_GETINFO, 0, (LPARAM)&gci))
	{
		ptrW joinBlob = ::db_get_wsa(gci.hContact, ppro->m_szModuleName, SKYPE_SETTINGS_SID);
		if ( joinBlob == NULL)
		{
			this->conversation->GetPropIdentity(data);
			ptrW cid = ::mir_utf8decodeW(data);
			::db_set_ws(gci.hContact, ppro->m_szModuleName, SKYPE_SETTINGS_SID, cid);

			this->conversation->GetJoinBlob(data);
			joinBlob = ::mir_utf8decodeW(data);
			::db_set_ws(gci.hContact, ppro->m_szModuleName, "JoinBlob", joinBlob);
		}
	}

	ParticipantRefs participants;
	this->conversation->GetParticipants(participants, Conversation::CONSUMERS_AND_APPLICANTS);
	for (uint i = 0; i < participants.size(); i++)
	{
		auto participant = participants[i];

		participant->GetPropIdentity(data);
		ptrW sid = ::mir_utf8decodeW(data);

		ChatMember member(sid);
		member.SetRank(participant->GetUintProp(Participant::P_RANK));
				
		Contact::Ref contact;
		this->ppro->GetContact(data, contact);

		Contact::AVAILABILITY status;
		contact->GetPropAvailability(status);
		member.SetStatus(CSkypeProto::SkypeToMirandaStatus(status));

		contact->GetPropFullname(data);
		if (data.length() != 0)
		{
			ptrW nick = ::mir_utf8decodeW(data);
			member.SetNick(nick);
		}

		member.SetPaticipant(participant);
		/*member.participant.fetch();
		member.participant->SetOnChangedCallback(&ChatRoom::OnParticipantChanged, this);*/

		this->AddMember(member, NULL, NULL);
	}
}

void ChatRoom::LeaveChat()
{
	this->conversation->RetireFrom();

	GCDEST gcd = { ppro->m_szModuleName, { NULL }, GC_EVENT_CONTROL };
	gcd.ptszID = this->cid;

	GCEVENT gce = {0};
	gce.cbSize = sizeof(GCEVENT);
	gce.dwFlags = GC_TCHAR;
	gce.pDest = &gcd;
	::CallServiceSync(MS_GC_EVENT, SESSION_OFFLINE, (LPARAM)&gce);
	::CallServiceSync(MS_GC_EVENT, SESSION_TERMINATE, (LPARAM)&gce);
}

void ChatRoom::SendEvent(const ChatMember &item, int eventType, DWORD timestamp, DWORD flags, DWORD itemData, const wchar_t *status, const wchar_t *message)
{
	GCDEST gcd = { ppro->m_szModuleName, { NULL }, eventType };
	gcd.ptszID = this->cid;

	bool isMe = this->IsMe(item);

	GCEVENT gce = {0};
	gce.cbSize = sizeof(gce);
	gce.dwFlags = GC_TCHAR | flags;
	gce.pDest = &gcd;
	gce.ptszUID = item.GetSid();
	gce.ptszNick = !isMe ? item.GetNick() : ::TranslateT("me");
	gce.bIsMe = isMe;
	gce.dwItemData = itemData;
	gce.ptszStatus = status;
	gce.ptszText = message;
	gce.time = timestamp;

	::CallServiceSync(MS_GC_EVENT, 0, (LPARAM)&gce);
}

void ChatRoom::SendEvent(const wchar_t *sid, int eventType, DWORD timestamp, DWORD flags, DWORD itemData, const wchar_t *status, const wchar_t *message)
{
	if (this->IsMe(sid))
		this->SendEvent(*this->me, eventType, timestamp, flags, itemData, status, message);
	else if (this->IsSys(sid))
		this->SendEvent(*this->sys, eventType, timestamp, flags, itemData, status, message);
	else
	{
		ChatMember search(sid);
		ChatMember *member = this->members.find(&search);
		if (member != NULL)
			this->SendEvent(*member, eventType, timestamp, flags, itemData, status, message);
	}
}

bool ChatRoom::IsMe(const wchar_t *sid) const
{
	return ::lstrcmpi(this->ppro->login, sid) == 0;
}

bool ChatRoom::IsMe(const ChatMember &item) const
{
	return ::lstrcmpi(this->ppro->login, item.GetSid()) == 0;
}

bool ChatRoom::IsSys(const wchar_t *sid) const
{
	return ::lstrcmpi(L"sys", sid) == 0;
}

bool ChatRoom::IsSys(const ChatMember &item) const
{
	return ::lstrcmpi(L"sys", item.GetSid()) == 0;
}

ChatMember *ChatRoom::FindChatMember(const wchar_t *sid)
{
	if ( !IsMe(sid))
	{
		ChatMember search(sid);
		return this->members.find(&search);
	}
	else
		return this->me;
}

void ChatRoom::AddMember(const ChatMember &item, const ChatMember &author, DWORD timestamp)
{
	if ( !this->IsMe(item))
	{
		ChatMember *member = this->FindChatMember(item.GetSid());
		if (member != NULL)
		{
			this->UpdateMember(item, timestamp);
		}
		else
		{
			ChatMember *newMember = new ChatMember(item);
			newMember->participant.fetch();
			newMember->participant->SetOnChangedCallback(&ChatRoom::OnParticipantChanged, this);
			this->members.insert(newMember);

			this->SendEvent(item, GC_EVENT_JOIN, timestamp, GCEF_ADDTOLOG, 0, ::TranslateW(ChatRoom::Roles[item.GetRank()]));
			this->SendEvent(item, GC_EVENT_SETCONTACTSTATUS, timestamp, 0, item.GetStatus());
		}
	}
	else
	{
		if (!this->me->participant)
		{
			this->me->participant = item.participant;
			this->me->participant.fetch();
			this->me->participant->SetOnChangedCallback(&ChatRoom::OnParticipantChanged, this);
		}
		if (this->me->GetRank() != item.GetRank())
		{
			this->SendEvent(*this->me, GC_EVENT_REMOVESTATUS, timestamp, 0, 0, ::TranslateW(ChatRoom::Roles[this->me->GetRank()]));
			this->SendEvent(*this->me, GC_EVENT_ADDSTATUS, timestamp, !this->me->GetRank() ? 0 : GCEF_ADDTOLOG, 0, ::TranslateW(ChatRoom::Roles[item.GetRank()]), author == NULL ? this->sys->GetNick() : author.GetNick());
			this->me->SetRank(item.GetRank());
		}
	}
}

void ChatRoom::UpdateMemberNick(ChatMember *member, const wchar_t *nick, DWORD timestamp)
{
	if (::lstrcmp(member->GetNick(), nick) != 0)
	{
		this->SendEvent(*member, GC_EVENT_NICK, timestamp, GCEF_ADDTOLOG, 0, nick);
		member->SetNick(nick);
	}
}

void ChatRoom::UpdateMemberRole(ChatMember *member, int role, const ChatMember &author, DWORD timestamp)
{
	if (member->GetRank() != role)
	{
		this->SendEvent(*member, GC_EVENT_REMOVESTATUS, timestamp, 0, 0, ::TranslateW(ChatRoom::Roles[member->GetRank()]));
		this->SendEvent(*member, GC_EVENT_ADDSTATUS, timestamp, GCEF_ADDTOLOG, 0, ::TranslateW(ChatRoom::Roles[role]), author == NULL ? this->sys->GetNick() : author.GetNick());
		member->SetRank(role);
	}
}

void ChatRoom::UpdateMemberStatus(ChatMember *member, int status, DWORD timestamp)
{
	if (member->GetStatus() != status)
	{
		this->SendEvent(*member, GC_EVENT_SETCONTACTSTATUS, timestamp, 0, status);
		member->SetStatus(status);
	}
}

void ChatRoom::UpdateMember(const wchar_t *sid, const wchar_t *nick, int role, int status, DWORD timestamp)
{
	ChatMember search(sid);
	ChatMember *member = this->members.find(&search);
	if (member != NULL)
	{
		this->UpdateMemberNick(member, nick, timestamp);
		this->UpdateMemberRole(member, role, NULL, timestamp);
		this->UpdateMemberStatus(member, status, timestamp);
	}
}

void ChatRoom::UpdateMember(const ChatMember &item, DWORD timestamp)
{
	ChatMember *member = this->FindChatMember(item.GetSid());
	if (member != NULL)
	{
		ptrW nick(item.GetNick());
		if (::lstrcmp(member->GetNick(), nick) != 0)
		{
			this->SendEvent(*member, GC_EVENT_NICK, timestamp, GCEF_ADDTOLOG, 0, nick);
			member->SetNick(nick);
		}
		if (member->GetRank() != item.GetRank())
		{
			this->SendEvent(*member, GC_EVENT_REMOVESTATUS, timestamp, 0, 0, ::TranslateW(ChatRoom::Roles[member->GetRank()]));
			this->SendEvent(*member, GC_EVENT_ADDSTATUS, timestamp, GCEF_ADDTOLOG, 0, ::TranslateW(ChatRoom::Roles[item.GetRank()]));
			member->SetRank(item.GetRank());
		}
		if (member->GetStatus() != item.GetStatus())
		{
			this->SendEvent(*member, GC_EVENT_SETCONTACTSTATUS, timestamp, 0, item.GetStatus());
			member->SetStatus(item.GetStatus());
		}
	}
}

void ChatRoom::KickMember(const ChatMember &item, const ChatMember *author, DWORD timestamp)
{
	if ( !this->IsMe(item))
	{
		ChatMember *member = this->FindChatMember(item.GetSid());
		if (member != NULL)
		{
			this->SendEvent(*member, GC_EVENT_KICK, timestamp, GCEF_ADDTOLOG, 0, author->GetNick());
			this->members.remove(member);
			delete member;
		}
	}
	else
	{
		this->SendEvent(*this->me, GC_EVENT_KICK, timestamp, GCEF_ADDTOLOG, 0, author->GetNick());
		this->me->SetRank(/*RETIRED= */7);
	}
}

void ChatRoom::KickMember(const wchar_t *sid, const wchar_t *author, DWORD timestamp)
{
	ChatMember member(sid);
	this->KickMember(member, this->FindChatMember(author), timestamp);
}

void ChatRoom::RemoveMember(const ChatMember &item, DWORD timestamp)
{
	if ( !this->IsMe(item))
	{
		ChatMember *member = this->FindChatMember(item.GetSid());
		if (member != NULL)
		{
			this->SendEvent(*member, GC_EVENT_QUIT, timestamp);
			this->members.remove(member);
			delete member;
		}
	}
	else
		this->LeaveChat();
}

void ChatRoom::RemoveMember(const wchar_t *sid, DWORD timestamp)
{
	ChatMember member(sid);
	this->RemoveMember(member, timestamp);
}

void ChatRoom::OnEvent(const ConversationRef &conversation, const MessageRef &message)
{
	uint messageType;
	messageType = message->GetUintProp(Message::P_TYPE);

	switch (messageType)
	{
	case CMessage::POSTED_EMOTE:
	case CMessage::POSTED_TEXT:
		{
			SEString data;

			message->GetPropAuthor(data);
			ptrW sid = ::mir_utf8decodeW(data);

			message->GetPropBodyXml(data);
			ptrW text =::mir_utf8decodeW(CSkypeProto::RemoveHtml(data));

			uint timestamp;
			message->GetPropTimestamp(timestamp);
			
			this->SendEvent(
				sid, 
				messageType == CMessage::POSTED_TEXT ? GC_EVENT_MESSAGE : GC_EVENT_ACTION,
				timestamp,
				GCEF_ADDTOLOG,
				0,
				NULL,
				text);
		}
		break;

	case Message::ADDED_CONSUMERS:
	case Message::ADDED_APPLICANTS:
		{
			SEString data;

			/*Message::CONSUMPTION_STATUS status;
			message->GetPropConsumptionStatus(status);
			if (status == Message::UNCONSUMED_NORMAL)*/
			{
				uint timestamp;
				message->GetPropTimestamp(timestamp);

				message->GetPropAuthor(data);
				ChatMember *author = this->FindChatMember((wchar_t *)ptrW(::mir_utf8decodeW(data)));

				ParticipantRefs participants;
				conversation->GetParticipants(participants);
				for (size_t i = 0; i < participants.size(); i++)
				{					
					participants[i]->GetPropIdentity(data);
					ptrW sid(::mir_utf8decodeW(data));
					if (this->FindChatMember(sid) == NULL)
					{
						ChatMember member(sid);
						member.SetRank(participants[i]->GetUintProp(Participant::P_RANK));

						Contact::Ref contact;
						this->ppro->GetContact(data, contact);

						Contact::AVAILABILITY status;
						contact->GetPropAvailability(status);
						member.SetStatus(CSkypeProto::SkypeToMirandaStatus(status));

						contact->GetPropFullname(data);
						ptrW nick(::mir_utf8decodeW(data));
						if (data.length() != 0)
						{
							ptrW nick = ::mir_utf8decodeW(data);
							member.SetNick(nick);
						}
						
						member.participant = participants[i];
						/*member.participant.fetch();
						member.participant->SetOnChangedCallback(&ChatRoom::OnParticipantChanged, this);*/

						this->AddMember(member, *author, timestamp);
					}
				}

				// do not remove
				//message->GetPropIdentities(data);
				//char *identities = ::mir_strdup(data);
				//if (identities)
				//{
				//	char *identity = ::strtok(identities, " ");
				//	if (identity != NULL)
				//	{
				//		do
				//		{
				//			Contact::Ref contact;
				//			this->ppro->GetContact(identity, contact);

				//			contact->GetIdentity(data);
				//			ptrW sid = ::mir_utf8decodeW(data);

				//			ChatMember *member = new ChatMember(sid);
				//			//todo: fix rank
				//			
				//			member->rank = 
				//				messageType == Message::ADDED_APPLICANTS ? 
				//				Participant::APPLICANT : 
				//				Participant::SPEAKER;
				//				//conversation->GetUintProp(Conversation::P_OPT_ENTRY_LEVEL_RANK);
				//				//participants[i]->GetUintProp(Participant::P_RANK);

				//			Contact::AVAILABILITY status;
				//			contact->GetPropAvailability(status);
				//			member->status = CSkypeProto::SkypeToMirandaStatus(status);

				//			contact->GetPropFullname(data);
				//			member->nick = ::mir_utf8decodeW(data);

				//			this->AddMember(member, timestamp);

				//			identity = ::strtok(NULL, " ");
				//		}
				//		while (identity != NULL);
				//	}
				//	::mir_free(identities);
				//}
			}
		}
		break;

	case Message::RETIRED_OTHERS:
		{
			SEString data;

			/*Message::CONSUMPTION_STATUS status;
			message->GetPropConsumptionStatus(status);
			if (status == Message::UNCONSUMED_NORMAL)*/
			{
				uint timestamp;
				message->GetPropTimestamp(timestamp);

				message->GetPropAuthor(data);
				ptrW author = ::mir_utf8decodeW(data);

				message->GetPropIdentities(data);
				char *identities = ::mir_strdup(data);
				if (identities)
				{
					char *identity = ::strtok(identities, " ");
					if (identity != NULL)
					{
						do
						{
							ptrW sid(::mir_utf8decodeW(identity));
							this->KickMember(sid, author, timestamp);

							identity = ::strtok(NULL, " ");
						}
						while (identity != NULL);
					}
					::mir_free(identities);
				}
			}
		}
		break;

	case Message::RETIRED:
		{
			SEString data;

			Message::CONSUMPTION_STATUS status;
			message->GetPropConsumptionStatus(status);
			if (status == Message::UNCONSUMED_NORMAL)
			{
				uint timestamp;
				message->GetPropTimestamp(timestamp);

				message->GetPropAuthor(data);
				ptrW sid = ::mir_utf8decodeW(data);

				this->RemoveMember(sid, timestamp);
			}
		}
		break;

	case Message::SET_RANK:
		{
			SEString data;
		}
		break;
	//		message->GetPropBodyXml(data);
	//		ptrA text = ::mir_strdup(data);
	//		int i = 0;

	//		/*Message::CONSUMPTION_STATUS status;
	//		message->GetPropConsumptionStatus(status);
	//		if (status == Message::UNCONSUMED_NORMAL)*/
	//		{
	//			message->GetPropAuthor(data);	
	//			ptrW sid = ::mir_utf8decodeW(data);

	//			ChatMember search(sid);
	//			ChatMember *member = this->FindChatMember(sid);
	//			if (member != NULL)
	//			{
	//				uint timestamp;
	//				message->GetPropTimestamp(timestamp);

	//				message->GetPropBodyXml(data);	
	//				ptrW rank = ::mir_utf8decodeW(data);

	//				member->SetRank(0);
	//			}
	//		}
	//	}
	//	break;

	case CMessage::STARTED_LIVESESSION:
		{
			SEString data;

			Message::CONSUMPTION_STATUS status;
			message->GetPropConsumptionStatus(status);
			if (status != Message::UNCONSUMED_NORMAL)
				break;

			message->GetPropAuthor(data);	
			ptrW sid = ::mir_utf8decodeW(data);

			uint timestamp;
			message->GetPropTimestamp(timestamp);
			
			this->SendEvent(
				sid, 
				GC_EVENT_INFORMATION, 
				timestamp,
				GCEF_ADDTOLOG, 
				0, 
				NULL, 
				::TranslateT("Incoming group call received"));
		}
		break;

	case CMessage::ENDED_LIVESESSION:
		{
			SEString data;

			Message::CONSUMPTION_STATUS status;
			message->GetPropConsumptionStatus(status);
			if (status != Message::UNCONSUMED_NORMAL)
				break;

			message->GetPropAuthor(data);	
			ptrW sid = ::mir_utf8decodeW(data);

			uint timestamp;
			message->GetPropTimestamp(timestamp);
			
			this->SendEvent(
				sid, 
				GC_EVENT_INFORMATION, 
				timestamp,
				GCEF_ADDTOLOG, 
				0, 
				NULL, 
				::TranslateT("Incoming group call finished"));
		}
		break;
	}
}

void ChatRoom::OnParticipantChanged(const ParticipantRef &participant, int prop)
{
	if (prop == Participant::P_RANK)
	{
		Participant::RANK rank;
		participant->GetPropRank(rank);
	
		SEString identity;
		participant->GetPropIdentity(identity);

		ptrW sid(::mir_utf8decodeW(identity));
		ChatMember *member = this->FindChatMember(sid);
		if (member != NULL)
			this->UpdateMemberRole(member, rank);
	}
}

///

void CSkypeProto::ChatValidateContact(HANDLE hItem, HWND hwndList, const StringList &contacts)
{
	if (this->IsProtoContact(hItem) && !this->IsChatRoom(hItem))
	{
		ptrW sid( ::db_get_wsa(hItem, this->m_szModuleName, SKYPE_SETTINGS_SID));
		if (sid == NULL || contacts.contains(sid))
			::SendMessage(hwndList, CLM_DELETEITEM, (WPARAM)hItem, 0);
	}
	else
		::SendMessage(hwndList, CLM_DELETEITEM, (WPARAM)hItem, 0);
}

void CSkypeProto::ChatPrepare(HANDLE hItem, HWND hwndList, const StringList &contacts)
{
	if (hItem == NULL)
		hItem = (HANDLE)::SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_ROOT, 0);

	while (hItem) 
	{
		HANDLE hItemN = (HANDLE)::SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_NEXT, (LPARAM)hItem);

		if (IsHContactGroup(hItem))
		{
			HANDLE hItemT = (HANDLE)::SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_CHILD, (LPARAM)hItem);
			if (hItemT) this->ChatPrepare(hItemT, hwndList, contacts);
		}
		else if (IsHContactContact(hItem))
			this->ChatValidateContact(hItem, hwndList, contacts);

		hItem = hItemN;
	}
}

void CSkypeProto::GetInvitedContacts(HANDLE hItem, HWND hwndList, StringList &chatTargets)
{
	if (hItem == NULL)
		hItem = (HANDLE)::SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_ROOT, 0);

	while (hItem)
	{
		if (IsHContactGroup(hItem))
		{
			HANDLE hItemT = (HANDLE)SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_CHILD, (LPARAM)hItem);
			if (hItemT)
				this->GetInvitedContacts(hItemT, hwndList, chatTargets);
		}
		else
		{
			int chk = SendMessage(hwndList, CLM_GETCHECKMARK, (WPARAM)hItem, 0);
			if (chk)
			{
				if (IsHContactInfo(hItem))
				{
					TCHAR buf[128] = _T("");
					SendMessage(hwndList, CLM_GETITEMTEXT, (WPARAM)hItem, (LPARAM)buf);

					if (buf[0]) chatTargets.insert(buf);
				}
				else
				{
					ptrW login( ::db_get_wsa(hItem, this->m_szModuleName, SKYPE_SETTINGS_SID));
					chatTargets.insert(login);
				}
			}
		}
		hItem = (HANDLE)SendMessage(hwndList, CLM_GETNEXTITEM, CLGN_NEXT, (LPARAM)hItem);
	}
}

INT_PTR CALLBACK CSkypeProto::InviteToChatProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	InviteChatParam *param = (InviteChatParam *)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);

	switch (msg)
	{
	case WM_INITDIALOG:
		TranslateDialogDefault(hwndDlg);

		SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
		param = (InviteChatParam *)lParam;
		{
			HWND hwndClist = GetDlgItem(hwndDlg, IDC_CCLIST);
			SetWindowLongPtr(hwndClist, GWL_STYLE, GetWindowLongPtr(hwndClist, GWL_STYLE) & ~CLS_HIDEOFFLINE);

			if ( !param->ppro->IsOnline())
			{
				::EnableWindow(GetDlgItem(hwndDlg, IDOK), FALSE);
				::EnableWindow(GetDlgItem(hwndDlg, IDC_ADDSCR), FALSE);
				::EnableWindow(GetDlgItem(hwndDlg, IDC_CCLIST), FALSE);
			}
		}
		break;

	case WM_CLOSE:
		::EndDialog(hwndDlg, 0);
		break;

	case WM_NOTIFY:
		{
			NMCLISTCONTROL *nmc = (NMCLISTCONTROL *)lParam;
			if (nmc->hdr.idFrom == IDC_CCLIST)
			{
				switch (nmc->hdr.code)
				{
				case CLN_NEWCONTACT:
					if (param && (nmc->flags & (CLNF_ISGROUP | CLNF_ISINFO)) == 0)
					{
						param->ppro->ChatValidateContact(nmc->hItem, nmc->hdr.hwndFrom, param->invitedContacts);
					}
					break;

				case CLN_LISTREBUILT:
					if (param)
					{
						param->ppro->ChatPrepare(NULL, nmc->hdr.hwndFrom, param->invitedContacts);
					}
					break;
				}
			}
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_ADDSCR:
			if (param->ppro->IsOnline())
			{
				wchar_t sid[SKYPE_SID_LIMIT];
				::GetDlgItemText(hwndDlg, IDC_EDITSCR, sid, SIZEOF(sid));

				CLCINFOITEM cii = {0};
				cii.cbSize = sizeof(cii);
				cii.flags = CLCIIF_CHECKBOX | CLCIIF_BELOWCONTACTS;
				cii.pszText = ::wcslwr(sid);

				HANDLE hItem = (HANDLE)::SendDlgItemMessage(
					hwndDlg,
					IDC_CCLIST,
					CLM_ADDINFOITEM,
					0,
					(LPARAM)&cii);
				::SendDlgItemMessage(hwndDlg, IDC_CCLIST, CLM_SETCHECKMARK, (LPARAM)hItem, 1);
			}
			break;

		case IDOK:
			{
				HWND hwndList = ::GetDlgItem(hwndDlg, IDC_CCLIST);

				param->invitedContacts.clear();
				param->ppro->GetInvitedContacts(NULL, hwndList, param->invitedContacts);

				if ( !param->invitedContacts.empty())
				{
					SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
					::EndDialog(hwndDlg, IDOK);
				}
				else
					param->ppro->ShowNotification(::TranslateT("You did not select any contact"));
			}
			break;

		case IDCANCEL:
			::EndDialog(hwndDlg, IDCANCEL);
			break;
		}
		break;
	}
	return FALSE;
}

bool CSkypeProto::IsChatRoom(HANDLE hContact)
{
	return ::db_get_b(hContact, this->m_szModuleName, "ChatRoom", 0) == 1;
}

HANDLE CSkypeProto::GetChatRoomByCid(const wchar_t *cid)
{
	HANDLE hContact = NULL;

	::EnterCriticalSection(&this->contact_search_lock);

	for (hContact = ::db_find_first(this->m_szModuleName); hContact; hContact = ::db_find_next(hContact, this->m_szModuleName))
	{
		if (this->IsChatRoom(hContact))
		{
			ptrW chatSid( ::db_get_wsa(hContact, this->m_szModuleName, "ChatRoomID"));
			if (::lstrcmpi(chatSid, cid) == 0)
				break;
		}
	}

	::LeaveCriticalSection(&this->contact_search_lock);

	return hContact;
}

void CSkypeProto::StartChat(StringList &invitedContacts)
{
	InviteChatParam *param = new InviteChatParam(NULL, invitedContacts, this);

	if (::DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_CHATROOM_INVITE), NULL, CSkypeProto::InviteToChatProc, (LPARAM)param) == IDOK && param->invitedContacts.size() > 0)
		ChatRoom::Create(param->invitedContacts, this, true);

	delete param;
}

void CSkypeProto::StartChat()
{
	StringList empty;
	return this->StartChat(empty);
}

void CSkypeProto::InviteToChatRoom(HANDLE hContact)
{
	GC_INFO gci = {0};
	gci.Flags = BYID | USERS | DATA;
	gci.pszModule = this->m_szModuleName;
	gci.pszID = ::db_get_wsa(hContact, this->m_szModuleName, "ChatRoomID");
	if ( !::CallService(MS_GC_GETINFO, 0, (LPARAM)(GC_INFO *) &gci))
	{
		ChatRoom *room = (ChatRoom *)gci.dwItemData;
		if (room != NULL && gci.pszUsers != NULL)
		{
			StringList invitedContacts(_A2T(gci.pszUsers));
			InviteChatParam *param = new InviteChatParam(NULL, invitedContacts, this);

			if (::DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_CHATROOM_INVITE), NULL, CSkypeProto::InviteToChatProc, (LPARAM)param) == IDOK && param->invitedContacts.size() > 0)
			{
				SEStringList needToAdd;
				for (size_t i = 0; i < param->invitedContacts.size(); i++)
				{
					if (!invitedContacts.contains(param->invitedContacts[i]))
						needToAdd.append((char *)_T2A(param->invitedContacts[i]));
				}
				room->conversation->AddConsumers(needToAdd);
			}
			delete param;
		}
	}	
	::mir_free(gci.pszID);
}

ChatRoom *CSkypeProto::FindChatRoom(const wchar_t *cid)
{
	GC_INFO gci = {0};
	gci.Flags = BYID | DATA;
	gci.pszModule = this->m_szModuleName;
	gci.pszID = ::mir_wstrdup(cid);

	if ( !::CallServiceSync(MS_GC_GETINFO, 0, (LPARAM)&gci))
		return (ChatRoom *)gci.dwItemData;

	return NULL;
}

void CSkypeProto::DeleteChatRoom(HANDLE hContact)
{
	ptrW cid(::db_get_wsa(hContact, this->m_szModuleName, "ChatRoomID"));
	ChatRoom *room = this->FindChatRoom(cid);
	if (room != NULL && room->conversation)
	{
		room->LeaveChat();
		room->conversation->Delete();
	}
	else
	{
		cid = ::db_get_wsa(hContact, this->m_szModuleName, SKYPE_SETTINGS_SID);
		if (cid != NULL)
		{
			ConversationRef conversation;
			if (this->GetConversationByIdentity((char *)_T2A(cid), conversation) && conversation)
			{
				conversation->RetireFrom();
				conversation->Delete();
			}
		}
	}
}

int __cdecl CSkypeProto::OnGCEventHook(WPARAM, LPARAM lParam)
{
	GCHOOK *gch = (GCHOOK *)lParam;
	if (!gch) return 1;

	if (::strcmp(gch->pDest->pszModule, this->m_szModuleName) != 0)
		return 0;

	ChatRoom *room = this->FindChatRoom(gch->pDest->ptszID);
	if (room == NULL)
		return 0;

	switch (gch->pDest->iType)
	{
	case GC_USER_MESSAGE:
		if (gch->ptszText && gch->ptszText[0])
		{
			CMessage::Ref message;
			ptrA text(::mir_utf8encodeW(gch->ptszText));
			room->conversation->PostText((char *)text, message);
		}
		break;

	/*case GC_USER_CHANMGR:
		if (this->GetConversationByIdentity(::mir_utf8encodeW(cid), conversation, false))
		{
			StringList invitedContacts(this->GetChatUsers(cid));
			this->InviteConactsToChat(conversation, invitedContacts); 
		}
		break;*/

	case GC_USER_PRIVMESS:
		{
			::CallService(MS_MSG_SENDMESSAGE, (WPARAM)this->GetContactBySid(gch->ptszUID), 0);
		}
		break;

	case GC_USER_LOGMENU:
	case GC_USER_NICKLISTMENU:
		switch (gch->dwData)
		{
		case CHAT_LIST_MENU::ICM_ROLE_ADMIN:
		case CHAT_LIST_MENU::ICM_ROLE_SPEAKER:
		case CHAT_LIST_MENU::ICM_ROLE_WRITER:
		case CHAT_LIST_MENU::ICM_ROLE_SPECTATOR:
			{
				ChatMember *member = room->FindChatMember(gch->ptszUID);
				if (member != NULL)
				{
					Participant::RANK rank;
					switch (gch->dwData)
					{
						case CHAT_LIST_MENU::ICM_ROLE_ADMIN:
							rank = Participant::ADMIN;
							break;

						case CHAT_LIST_MENU::ICM_ROLE_SPEAKER:
							rank = Participant::SPEAKER;
							break;

						case CHAT_LIST_MENU::ICM_ROLE_WRITER:
							rank = Participant::WRITER;
							break;

						case CHAT_LIST_MENU::ICM_ROLE_SPECTATOR:
							rank = Participant::SPECTATOR;
							break;
					}
					if (member->participant && member->participant->SetRankTo(rank))
						room->UpdateMemberRole(member, rank, *room->me);
				}
			}
			break;

		case CHAT_LIST_MENU::ICM_ADD:
			{
				ChatMember *member = room->FindChatMember(gch->ptszUID);
				if (member != NULL)
				{
					SEStringList consumers;
					consumers.append((char *)ptrA(::mir_utf8encodeW(gch->ptszUID)));
					room->conversation->AddConsumers(consumers);
				}
			}
			break;

		case CHAT_LIST_MENU::ICM_KICK:
			{
				ChatMember *member = room->FindChatMember(gch->ptszUID);
				if (member != NULL)
				{
					if (member->participant && member->participant->Retire())
						room->KickMember(gch->ptszUID, room->me->GetSid());
				}
			}
			break;

		case CHAT_LIST_MENU::ICM_BAN:
			{
				ChatMember *member = room->FindChatMember(gch->ptszUID);
				if (member != NULL && member->participant)
				{
					member->participant->SetRankTo(Participant::OUTLAW);
					if (member->participant->Retire())
						room->KickMember(gch->ptszUID, room->me->GetSid());
				}
			}
			break;

		case CHAT_LIST_MENU::ICM_AUTH_REQUEST:
			{
				CContact::Ref contact;
				SEString sid((char *)ptrA(::mir_utf8encodeW(gch->ptszUID)));
				if (this->GetContact(sid, contact))
				{
					this->AuthRequest(
						this->AddContact(contact),
						LPGENT("Hi! I\'d like to add you to my contact list"));
				}
			}
			break;

		case CHAT_LIST_MENU::ICM_DETAILS:
			::CallService(MS_USERINFO_SHOWDIALOG, (WPARAM)this->GetContactBySid(gch->ptszUID), 0);
			break;

		case CHAT_LIST_MENU::ICM_COPY_SID:
			{
				HANDLE hContact = this->GetContactBySid(gch->ptszUID);
				if (!hContact)
				{
					ptrW sid = ::db_get_wsa(hContact, this->m_szModuleName, SKYPE_SETTINGS_SID);
					if (sid != NULL)
						CSkypeProto::CopyToClipboard(sid);
				}
			}
			break;

		case CHAT_LIST_MENU::ICM_COPY_URI:
			{
				SEString data;
				room->conversation->GetJoinBlob(data);
				ptrW blob = ::mir_utf8decodeW(data);
				
				wchar_t uri[MAX_PATH];
				::mir_sntprintf(uri, SIZEOF(uri), L"skype:?chat&blob=%s", blob);

				CSkypeProto::CopyToClipboard(uri);
			}
			break;
		}
		break;

	//case GC_USER_TYPNOTIFY:
		//break;
	}
	return 0;
}

int __cdecl CSkypeProto::OnGCMenuHook(WPARAM, LPARAM lParam)
{
	GCMENUITEMS *gcmi = (GCMENUITEMS*) lParam;

	if (::stricmp(gcmi->pszModule, this->m_szModuleName) != 0)
		return 0;

	ChatRoom *room = this->FindChatRoom(gcmi->pszID);
	if (room == NULL)
		return 0;

	ResetChatMenuItem();

	if (room->me->GetRank() > Participant::ADMIN || room->me->GetRank() == 0)
	{
		DisableChatMenuItem(ICM_ROLE);
		DisableChatMenuItem(ICM_ADD);
		DisableChatMenuItem(ICM_KICK);
		DisableChatMenuItem(ICM_BAN);
	}

	//todo: add other case
	if (room->me->GetRank() >= Participant::APPLICANT)
	{
		DisableChatMenuItem(ICM_CONF_INVITE);
	}	

	ChatMember *member = room->FindChatMember(gcmi->pszUID);
	if (member != NULL)
	{
		if (member->GetRank() == Participant::CREATOR)
		{
			DisableChatMenuItem(ICM_ROLE);
			DisableChatMenuItem(ICM_ADD);
			DisableChatMenuItem(ICM_KICK);
			DisableChatMenuItem(ICM_BAN);
		}
		
		if (member->GetRank() <= Participant::SPECTATOR)
		{
			CHAT_LIST_MENU type = (CHAT_LIST_MENU)(ICM_ROLE + member->GetRank() - 1);
			CheckChatMenuItem(type);
			DisableChatMenuItem(type);

			DisableChatMenuItem(ICM_ADD);
		}
		
		if (member->GetRank() > Participant::SPECTATOR)
			DisableChatMenuItem(ICM_ROLE);

		HANDLE hContact = this->GetContactBySid(gcmi->pszUID);
		if (hContact == NULL)
			DisableChatMenuItem(ICM_DETAILS);
		else if(::db_get_b(hContact, this->m_szModuleName, "Auth", 0) == 0)
			DisableChatMenuItem(ICM_AUTH_REQUEST);
	}
	else
	{
		DisableChatMenuItem(ICM_DETAILS);
		DisableChatMenuItem(ICM_AUTH_REQUEST);
		DisableChatMenuItem(ICM_ROLE);
		DisableChatMenuItem(ICM_ADD);
		DisableChatMenuItem(ICM_KICK);
		DisableChatMenuItem(ICM_BAN);
		DisableChatMenuItem(ICM_COPY_SID);
	}

	gcmi->nItems = SIZEOF(crListItems);
	gcmi->Item = crListItems;

	return 0;
}

void CSkypeProto::UpdateChatUserStatus(CContact::Ref contact)
{
	CContact::AVAILABILITY availability;
	contact->GetPropAvailability(availability);

	SEString identity;
	contact->GetIdentity(identity);
	ptrW sid(::mir_utf8decodeW(identity));

	GC_INFO gci = {0};
	gci.Flags = BYINDEX | DATA;
	gci.pszModule = this->m_szModuleName;

	int count = ::CallServiceSync(MS_GC_GETSESSIONCOUNT, 0, (LPARAM)this->m_szModuleName);
	for (int i = 0; i < count ; i++)
	{
		gci.iItem = i;
		::CallServiceSync(MS_GC_GETINFO, 0, (LPARAM)&gci);

		ChatRoom *room = (ChatRoom *)gci.dwItemData;
		if (room != NULL)
		{
			ChatMember *member = room->FindChatMember(sid);
			if (member != NULL)
				room->UpdateMemberStatus(member, CSkypeProto::SkypeToMirandaStatus(availability));
		}
	}
}

void CSkypeProto:: UpdateChatUserNick(CContact::Ref contact)
{
	SEString data;

	contact->GetIdentity(data);
	ptrW sid(::mir_utf8decodeW(data));

	contact->GetPropFullname(data);
	ptrW nick(::mir_utf8decodeW(data));
	if (data.length() == 0)
		nick = (WCHAR*)sid;		

	GC_INFO gci = {0};
	gci.Flags = BYINDEX | DATA;
	gci.pszModule = this->m_szModuleName;

	int count = ::CallServiceSync(MS_GC_GETSESSIONCOUNT, 0, (LPARAM)this->m_szModuleName);
	for (int i = 0; i < count ; i++)
	{
		gci.iItem = i;
		::CallServiceSync(MS_GC_GETINFO, 0, (LPARAM)&gci);

		ChatRoom *room = (ChatRoom *)gci.dwItemData;
		if (room != NULL)
		{
			ChatMember *member = room->FindChatMember(sid);
			if (member != NULL)
				room->UpdateMemberNick(member, nick);
		}
	}
}

INT_PTR __cdecl CSkypeProto::OnJoinChat(WPARAM wParam, LPARAM)
{
	HANDLE hContact = (HANDLE)wParam;
	if (hContact)
	{
		ptrW joinBlob(::db_get_wsa(hContact, this->m_szModuleName, "JoinBlob"));		

		SEString data;
		ConversationRef conversation;

		this->GetConversationByBlob(::mir_utf8encodeW(joinBlob), conversation);
		if (conversation)
		{
			conversation->GetPropDisplayname(data);
			ptrW name(::mir_utf8decodeW(data));

			conversation->GetJoinBlob(data);
			joinBlob = ::mir_utf8decodeW(data);
			::db_set_ws(hContact, this->m_szModuleName, "JoinBlob", joinBlob);
			
			ptrW cid(::db_get_wsa(hContact, this->m_szModuleName, SKYPE_SETTINGS_SID));
			ChatRoom *room = new ChatRoom(cid, name, this);
			room->Start(conversation, true);
		}
	}
	
	return 0;
}

INT_PTR __cdecl CSkypeProto::OnLeaveChat(WPARAM wParam, LPARAM)
{
	HANDLE hContact = (HANDLE)wParam;
	if (hContact)
	{
		ptrW cid(::db_get_wsa(hContact, this->m_szModuleName, "ChatRoomID"));
		
		ChatRoom *room = this->FindChatRoom(cid);
		if (room != NULL)
			room->LeaveChat();
	}

	return 0;
}

///

void __cdecl CSkypeProto::LoadChatList(void*)
{
	this->Log(L"Updating group chats list");
	CConversation::Refs conversations;
	this->GetConversationList(conversations);

	SEString data;
	for (uint i = 0; i < conversations.size(); i++)
	{
		auto conversation = conversations[i];

		uint convoType = conversation->GetUintProp(Conversation::P_TYPE);
		if (convoType == CConversation::CONFERENCE)
		{
			bool isBookmarked;
			conversation->GetPropIsBookmarked(isBookmarked);

			CConversation::MY_STATUS status;
			conversation->GetPropMyStatus(status);
			if (status == Conversation::APPLICANT || status == Conversation::CONSUMER || isBookmarked)
			{
				conversation->GetPropIdentity(data);
				ptrW cid = ::mir_utf8decodeW(data);
				CSkypeProto::ReplaceSpecialChars(cid);

				conversation->GetPropDisplayname(data);
				ptrW name = ::mir_utf8decodeW(data);

				ChatRoom *room = new ChatRoom(cid, name, this);
				room->Start(conversation);
			}
		}
	}
}

///

void CSkypeProto::OnChatEvent(const ConversationRef &conversation, const MessageRef &message)
{
	uint messageType;
	messageType = message->GetUintProp(Message::P_TYPE);

	SEString data;
	conversation->GetPropIdentity(data);
	ptrW cid = ::mir_utf8decodeW(data);
	CSkypeProto::ReplaceSpecialChars(cid);

	ChatRoom *room = this->FindChatRoom(cid);
	if (room != NULL)
	{
		room->OnEvent(conversation, message);
	}
	else if(messageType != Message::RETIRED && messageType != Message::RETIRED_OTHERS)
	{
		SEString data;

		conversation->GetPropDisplayname(data);
		ptrW name = ::mir_utf8decodeW(data);

		ChatRoom *room = new ChatRoom(cid, name, this);
		room->Start(conversation, true);
	}
}

void CSkypeProto::OnConversationListChange(
		const ConversationRef& conversation,
		const Conversation::LIST_TYPE& type,
		const bool& added)
{
	uint convoType = conversation->GetUintProp(Conversation::P_TYPE);
	if (convoType == Conversation::CONFERENCE && type == Conversation::INBOX_CONVERSATIONS  && added)
	{
		SEString data;

		conversation->GetPropIdentity(data);
		ptrW cid = ::mir_utf8decodeW(data);
		CSkypeProto::ReplaceSpecialChars(cid);

		if ( !this->GetChatRoomByCid(cid))
		{
			conversation->GetPropDisplayname(data);
			ptrW name = ::mir_utf8decodeW(data);

			ChatRoom *room = new ChatRoom(cid, name, this);
			room->Start(conversation, true);
		}
	}
}