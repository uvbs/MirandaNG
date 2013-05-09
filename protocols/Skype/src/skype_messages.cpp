#include "skype_proto.h"

int CSkypeProto::OnMessagePreCreate(WPARAM, LPARAM lParam)
{
	MessageWindowEvent *evt = (MessageWindowEvent *)lParam;

	MessageRef message(evt->seq);
	SEBinary guid;
	if (message->GetPropGuid(guid))
	{
		evt->dbei->pBlob = (PBYTE)::mir_realloc(evt->dbei->pBlob, evt->dbei->cbBlob + guid.size());
		::memcpy((char *)&evt->dbei->pBlob[evt->dbei->cbBlob], guid.data(), guid.size());
		evt->dbei->cbBlob += (DWORD)guid.size();
	}

	return 1;
}

void CSkypeProto::OnMessageReceived(CConversation::Ref &conversation, CMessage::Ref &message)
{
	SEString data;

	CMessage::TYPE messageType;
	message->GetPropType(messageType);

	uint timestamp;
	message->GetPropTimestamp(timestamp);

	CMessage::CONSUMPTION_STATUS status;
	message->GetPropConsumptionStatus(status);
		
	message->GetPropBodyXml(data);
	char *text = CSkypeProto::RemoveHtml(data);

	message->GetPropAuthor(data);			
		
	CContact::Ref author;
	g_skype->GetContact(data, author);

	HANDLE hContact = this->AddContact(author);

	SEBinary guid;
	message->GetPropGuid(guid);

	this->RaiseMessageReceivedEvent(
		hContact,
		timestamp, 
		guid,
		text,
		status == CMessage::UNCONSUMED_NORMAL);
}

void CSkypeProto::OnMessageSended(CConversation::Ref &conversation, CMessage::Ref &message)
{
	SEString data;

	CMessage::TYPE messageType;
	message->GetPropType(messageType);

	uint timestamp;
	message->GetPropTimestamp(timestamp);

	CMessage::SENDING_STATUS sstatus;
	message->GetPropSendingStatus(sstatus);

	CMessage::CONSUMPTION_STATUS status;
	message->GetPropConsumptionStatus(status);

	message->GetPropBodyXml(data);
	char *text = CSkypeProto::RemoveHtml(data);

	CParticipant::Refs participants;
	conversation->GetParticipants(participants, CConversation::OTHER_CONSUMERS);
	participants[0]->GetPropIdentity(data);
		
	CContact::Ref receiver;
	g_skype->GetContact(data, receiver);

	HANDLE hContact = this->AddContact(receiver);
			
	this->SendBroadcast(
		hContact,
		ACKTYPE_MESSAGE,
		sstatus == CMessage::FAILED_TO_SEND ? ACKRESULT_FAILED : ACKRESULT_SUCCESS,
		(HANDLE)message->getOID(), 0);

	SEBinary guid;
	message->GetPropGuid(guid);

	this->RaiseMessageSendedEvent(
		hContact,
		timestamp,
		guid,
		text,
		status == CMessage::UNCONSUMED_NORMAL);
}

void CSkypeProto::OnMessageEvent(CConversation::Ref conversation, CMessage::Ref message)
{
	CMessage::TYPE messageType;
	message->GetPropType(messageType);

	switch (messageType)
	{
	case CMessage::POSTED_EMOTE:
	case CMessage::POSTED_TEXT:
		{
			SEString author;
			message->GetPropAuthor(author);
			
			if (::wcsicmp(mir_ptr<wchar_t>(::mir_utf8decodeW(author)), this->login) == 0)
				this->OnMessageSended(conversation, message);
			else
				this->OnMessageReceived(conversation, message);
		}
		break;

	case CMessage::STARTED_LIVESESSION:
		{
			conversation->LeaveLiveSession();

			uint timestamp;
			message->GetPropTimestamp(timestamp);

			SEString identity;
			message->GetPropAuthor(identity);

			CContact::Ref author;
			g_skype->GetContact(identity, author);

			HANDLE hContact = this->AddContact(author);
		
			char *message = ::mir_utf8encode(::Translate("Incoming call"));
		
			this->AddDBEvent(
				hContact,
				SKYPE_DB_EVENT_TYPE_CALL,
				timestamp,
				DBEF_UTF,
				(DWORD)::strlen(message) + 1,
				(PBYTE)message);
		}
	}
}