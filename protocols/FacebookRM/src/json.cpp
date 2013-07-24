/*

Facebook plugin for Miranda Instant Messenger
_____________________________________________

Copyright � 2009-11 Michal Zelinka, 2011-13 Robert P�sel

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "common.h"

int facebook_json_parser::parse_buddy_list(void* data, List::List< facebook_user >* buddy_list)
{
	facebook_user* current = NULL;
	std::string jsonData = static_cast< std::string* >(data)->substr(9);

	JSONNODE *root = json_parse(jsonData.c_str());
	if (root == NULL)
		return EXIT_FAILURE;

	JSONNODE *payload = json_get(root, "payload");
	if (payload == NULL)
		return EXIT_FAILURE;

	JSONNODE *list = json_get(payload, "buddy_list");
	if (list == NULL)
		return EXIT_FAILURE;

	// Set all contacts in map to offline
	for (List::Item< facebook_user >* i = buddy_list->begin(); i != NULL; i = i->next) {
		i->data->status_id = ID_STATUS_OFFLINE;
	}
	
	// Load last active times
	JSONNODE *lastActive = json_get(list, "last_active_times");
	if (lastActive != NULL) {
		for (unsigned int i = 0; i < json_size(lastActive); i++) {
			JSONNODE *it = json_at(lastActive, i);
			char *id = json_name(it);
			
			current = buddy_list->find(id);
			if (current == NULL) {
				buddy_list->insert(std::make_pair(id, new facebook_user()));
				current = buddy_list->find(id);
				current->user_id = id;
			}

			current->last_active = json_as_int(it);
		}
	}
	
	// Find mobile friends
	JSONNODE *mobileFriends = json_get(list, "mobile_friends");
	if (mobileFriends != NULL) {		
		for (unsigned int i = 0; i < json_size(mobileFriends); i++) {
			JSONNODE *it = json_at(mobileFriends, i);
			char *id = json_name(it);
			
			current = buddy_list->find(id);
			if (current == NULL) {
				buddy_list->insert(std::make_pair(id, new facebook_user()));
				current = buddy_list->find(id);
				current->user_id = id;
			}

			current->status_id = ID_STATUS_ONTHEPHONE;
		}
	}

	// Find now awailable contacts
	JSONNODE *nowAvailable = json_get(list, "nowAvailableList");
	if (nowAvailable != NULL) {		
		for (unsigned int i = 0; i < json_size(nowAvailable); i++) {
			JSONNODE *it = json_at(nowAvailable, i);
			char *id = json_name(it);
			
			current = buddy_list->find(id);
			if (current == NULL) {
				buddy_list->insert(std::make_pair(id, new facebook_user()));
				current = buddy_list->find(id);
				current->user_id = id;
			}

			current->status_id = ID_STATUS_ONLINE;
			
			// In new version of Facebook "i" means "offline"
			JSONNODE *idle = json_get(it, "i");
			if (idle != NULL && json_as_bool(idle))
				current->status_id = ID_STATUS_OFFLINE;
		}
	}

	// Get aditional informations about contacts (if available)
	JSONNODE *userInfos = json_get(list, "userInfos");
	if (userInfos != NULL) {		
		for (unsigned int i = 0; i < json_size(userInfos); i++) {
			JSONNODE *it = json_at(userInfos, i);
			char *id = json_name(it);
			
			current = buddy_list->find(id);
			if (current == NULL)
				continue;

			JSONNODE *name = json_get(it, "name");
			JSONNODE *thumbSrc = json_get(it, "thumbSrc");

			if (name != NULL)
				current->real_name = utils::text::slashu_to_utf8(utils::text::special_expressions_decode(json_as_string(name)));
			if (thumbSrc != NULL)			
				current->image_url = utils::text::slashu_to_utf8(utils::text::special_expressions_decode(json_as_string(thumbSrc)));
		}
	}

	json_delete(root);

	return EXIT_SUCCESS;
}

int facebook_json_parser::parse_friends(void* data, std::map< std::string, facebook_user* >* friends)
{
	std::string jsonData = static_cast< std::string* >(data)->substr(9);
	
	JSONNODE *root = json_parse(jsonData.c_str());
	if (root == NULL)
		return EXIT_FAILURE;

	JSONNODE *payload = json_get(root, "payload");
	if (payload == NULL)
		return EXIT_FAILURE;

	for (unsigned int i = 0; i < json_size(payload); i++) {
		JSONNODE *it = json_at(payload, i);
		char *id = json_name(it);

		JSONNODE *name = json_get(it, "name");
		JSONNODE *thumbSrc = json_get(it, "thumbSrc");
		JSONNODE *gender = json_get(it, "gender");
		//JSONNODE *vanity = json_get(it, "vanity"); // username
		//JSONNODE *uri = json_get(it, "uri"); // profile url
		//JSONNODE *is_friend = json_get(it, "is_friend"); // e.g. "True"
		//JSONNODE *type = json_get(it, "type"); // e.g. "friend" (classic contact) or "user" (disabled/deleted account)

		facebook_user *fbu = new facebook_user();

		fbu->user_id = id;
		if (name)
			fbu->real_name = utils::text::slashu_to_utf8(utils::text::special_expressions_decode(json_as_string(name)));
		if (thumbSrc)
			fbu->image_url = utils::text::slashu_to_utf8(utils::text::special_expressions_decode(json_as_string(thumbSrc)));

		if (gender)
			switch (json_as_int(gender)) {
			case 1: // female
				fbu->gender = 70;
				break;
			case 2: // male
				fbu-> gender = 77;
				break;
			}

		friends->insert(std::make_pair(id, fbu));
	}

	return EXIT_SUCCESS;
}


int facebook_json_parser::parse_notifications(void *data, std::vector< facebook_notification* > *notifications) 
{
	std::string jsonData = static_cast< std::string* >(data)->substr(9);
	
	JSONNODE *root = json_parse(jsonData.c_str());
	if (root == NULL)
		return EXIT_FAILURE;

	JSONNODE *payload = json_get(root, "payload");
	if (payload == NULL)
		return EXIT_FAILURE;

	JSONNODE *list = json_get(payload, "notifications");
	if (list == NULL)
		return EXIT_FAILURE;

	for (unsigned int i = 0; i < json_size(list); i++) {
		JSONNODE *it = json_at(list, i);
		char *id = json_name(it);

		JSONNODE *markup = json_get(it, "markup");
		JSONNODE *unread = json_get(it, "unread");
		//JSONNODE *time = json_get(it, "time");

		// Ignore empty and old notifications
		if (markup == NULL || unread == NULL || json_as_int(unread) == 0)
			continue;

		std::string text = utils::text::slashu_to_utf8(utils::text::special_expressions_decode(json_as_string(markup)));

		facebook_notification* notification = new facebook_notification();

		notification->id = id;
		notification->link = utils::text::source_get_value(&text, 3, "<a ", "href=\"", "\"");
		notification->text = utils::text::remove_html(utils::text::source_get_value(&text, 1, "<abbr"));		

		notifications->push_back(notification);
	}

	return EXIT_SUCCESS;
}

bool ignore_duplicits(FacebookProto *proto, std::string mid, std::string text) {
	std::map<std::string, bool>::iterator it = proto->facy.messages_ignore.find(mid);
	if (it != proto->facy.messages_ignore.end()) {
		std::string msg = "????? Ignoring duplicit/sent message\n" + text;
		proto->Log(msg.c_str());

		it->second = true; // mark to delete it at the end
		return true;
	}
					
	// remember this id to ignore duplicits
	proto->facy.messages_ignore.insert(std::make_pair(mid, true));
	return false;
}

int facebook_json_parser::parse_messages(void* data, std::vector< facebook_message* >* messages, std::vector< facebook_notification* >* notifications)
{
	std::string jsonData = static_cast< std::string* >(data)->substr(9);
		
	JSONNODE *root = json_parse(jsonData.c_str());
	if (root == NULL)
		return EXIT_FAILURE;

	JSONNODE *ms = json_get(root, "ms");
	if (ms == NULL)
		return EXIT_FAILURE;

	for (unsigned int i = 0; i < json_size(ms); i++) {
		JSONNODE *it = json_at(ms, i);
		
		JSONNODE *type = json_get(it, "type");
		if (type == NULL)
			continue;

		std::string t = json_as_string(type);
		if (t == "msg" || t == "offline_msg") {
			// direct message
			
			JSONNODE *from = json_get(it, "from");
			if (from == NULL)
				continue;

			char *from_id = json_as_string(from);

			JSONNODE *msg = json_get(it, "msg");
			if (msg == NULL)
				continue;

			JSONNODE *text = json_get(msg, "text");
			JSONNODE *messageId = json_get(msg, "messageId");
			JSONNODE *time = json_get(it, "time");
			// JSONNODE *tab_type = json_get(it, "tab_type"); // e.g. "friend"

			if (text == NULL || messageId == NULL)
				continue;

			std::string message_id = json_as_string(messageId);
			std::string message_text = json_as_string(text);

			// ignore duplicits or messages sent from miranda
			if (ignore_duplicits(proto, message_id, message_text))
				continue;

			message_text = utils::text::trim(utils::text::special_expressions_decode(utils::text::slashu_to_utf8(message_text)), true);
			if (message_text.empty())
				continue;			

			JSONNODE *truncated = json_get(msg, "truncated");
			if (truncated != NULL && json_as_int(truncated) == 1) {
				// If we got truncated message, we can ignore it, because we should get it again as "messaging" type
				std::string msg = "????? We got truncated message so we ignore it\n";
				msg += utils::text::special_expressions_decode(utils::text::slashu_to_utf8(message_text));
				proto->Log(msg.c_str());
			} else if (from_id == proto->facy.self_.user_id) {
				// Outgoing message
				JSONNODE *to = json_get(it, "to");
				if (to == NULL)
					continue;

				// TODO: put also outgoing messages into messages array and process it elsewhere
				HANDLE hContact = proto->ContactIDToHContact(json_as_string(to));
				if (!hContact) // TODO: add this contact?
					continue;

				DBEVENTINFO dbei = {0};
				dbei.cbSize = sizeof(dbei);
				dbei.eventType = EVENTTYPE_MESSAGE;
				dbei.flags = DBEF_SENT | DBEF_UTF;
				dbei.szModule = proto->m_szModuleName;

				bool local_time = proto->getByte(FACEBOOK_KEY_LOCAL_TIMESTAMP, 0) != 0;
				dbei.timestamp = local_time || time == NULL ? ::time(NULL) : utils::time::fix_timestamp(json_as_int(time));

				dbei.cbBlob = (DWORD)message_text.length() + 1;
				dbei.pBlob = (PBYTE)message_text.c_str();
				db_event_add(hContact, &dbei);

				continue;
			} else {
				// Incomming message
  				facebook_message* message = new facebook_message();
				message->message_text = message_text;
				message->time = utils::time::fix_timestamp(json_as_int(time));
				message->user_id = from_id;
				message->message_id = message_id;

				messages->push_back(message);
			}

		} else if (t == "messaging") {
			// messages

			JSONNODE *type = json_get(it, "event");
			if (type == NULL)
				continue;

			std::string t = json_as_string(type);

			if (t == "read_receipt") {
				// user read message
				JSONNODE *reader = json_get(it, "reader");
				JSONNODE *time = json_get(it, "time");

				if (reader == NULL || time == NULL)
					continue;

				// TODO: add check for chat contacts
				HANDLE hContact = proto->ContactIDToHContact(json_as_string(reader));
				if (hContact) {
					TCHAR ttime[64], tstr[100];
					_tcsftime(ttime, SIZEOF(ttime), _T("%X"), utils::conversion::fbtime_to_timeinfo(json_as_int(time)));
					mir_sntprintf(tstr, SIZEOF(tstr), TranslateT("Message read: %s"), ttime);

					CallService(MS_MSG_SETSTATUSTEXT, (WPARAM)hContact, (LPARAM)tstr);
				}
			} else if (t == "deliver") {
				// inbox message (multiuser or direct)

				JSONNODE *msg = json_get(it, "message");

				JSONNODE *sender_fbid = json_get(msg, "sender_fbid");
				JSONNODE *sender_name = json_get(msg, "sender_name");
				JSONNODE *body = json_get(msg, "body");
				JSONNODE *tid = json_get(msg, "tid");
				JSONNODE *mid = json_get(msg, "mid");
				JSONNODE *timestamp = json_get(msg, "timestamp");

				if (sender_fbid == NULL || body == NULL || mid == NULL)
					continue;

				std::string id = json_as_string(sender_fbid);
				std::string message_id = json_as_string(mid);
				std::string message_text = json_as_string(body);

				// Ignore messages from myself
				if (id == proto->facy.self_.user_id)
					continue;

				// Ignore duplicits or messages sent from miranda
				if (body == NULL || ignore_duplicits(proto, message_id, message_text))
					continue;

				message_text = utils::text::trim(utils::text::special_expressions_decode(utils::text::slashu_to_utf8(message_text)), true);
				if (message_text.empty())
					continue;

				facebook_message* message = new facebook_message();
				message->message_text = message_text;
				message->sender_name = utils::text::special_expressions_decode(utils::text::slashu_to_utf8(id));
				message->time = utils::time::fix_timestamp(json_as_int(timestamp));
				message->user_id = id; // TODO: Check if we have contact with this ID in friendlist and otherwise do something different?
				message->message_id = message_id;

				messages->push_back(message);
			}
		} else if (t == "thread_msg") {
			// multiuser message

			JSONNODE *from_name = json_get(it, "from_name");
			JSONNODE *to_name_ = json_get(it, "to_name");
			if (to_name_ == NULL)
				continue;
			JSONNODE *to_name = json_get(to_name_, "__html");
			JSONNODE *to_id = json_get(it, "to");
			JSONNODE *from_id = json_get(it, "from");

			JSONNODE *msg = json_get(it, "msg");
			JSONNODE *text = json_get(msg, "text");
			JSONNODE *messageId = json_get(msg, "messageId");

			if (from_id == NULL || text == NULL || messageId == NULL)
				continue;

			std::string id = json_as_string(from_id);
			std::string message_id = json_as_string(messageId);
			std::string message_text = json_as_string(text);

			// Ignore messages from myself
			if (id == proto->facy.self_.user_id)
				continue;
				
			// Ignore duplicits or messages sent from miranda
			if (ignore_duplicits(proto, message_id, message_text))
				continue;

			message_text = utils::text::trim(utils::text::special_expressions_decode(utils::text::slashu_to_utf8(message_text)), true);
			if (message_text.empty())
				continue;

			std::string title = utils::text::special_expressions_decode(utils::text::slashu_to_utf8(json_as_string(to_name)));
			std::string url = "/?action=read&sk=inbox&page&query&tid=" + id;
			std::string popup_text = utils::text::special_expressions_decode(utils::text::slashu_to_utf8(json_as_string(from_name)));
			popup_text += ": " + message_text;

			proto->Log("      Got multichat message");
		    
			TCHAR* szTitle = mir_utf8decodeT(title.c_str());
			TCHAR* szText = mir_utf8decodeT(popup_text.c_str());
			proto->NotifyEvent(szTitle, szText, NULL, FACEBOOK_EVENT_OTHER, &url);
			mir_free(szTitle);
			mir_free(szText);
		} else if (t == "notification_json") {
			// event notification

			if (!proto->getByte(FACEBOOK_KEY_EVENT_NOTIFICATIONS_ENABLE, DEFAULT_EVENT_NOTIFICATIONS_ENABLE))
				continue;

			JSONNODE *nodes = json_get(it, "nodes");
			for (unsigned int i = 0; i < json_size(nodes); i++) {
				JSONNODE *itNodes = json_at(nodes, i);

				//JSONNODE *text = json_get(itNodes, "title/text");
				JSONNODE *text_ = json_get(itNodes, "unaggregatedTitle");
				if (text_ == NULL)
					continue;
				JSONNODE *text = json_get(text_, "text");
				JSONNODE *url = json_get(itNodes, "url");
				JSONNODE *alert_id = json_get(itNodes, "alert_id");
				
				JSONNODE *time_ = json_get(it, "timestamp");
				if (time_ == NULL)
					continue;
				JSONNODE *time = json_get(time_, "time");
				if (time == NULL || text == NULL || url == NULL || alert_id == NULL)
					continue;

				unsigned long timestamp = json_as_int(time);
				if (timestamp > proto->facy.last_notification_time_) {
					// Only new notifications
					proto->facy.last_notification_time_ = timestamp;

					facebook_notification* notification = new facebook_notification();
					notification->text = utils::text::slashu_to_utf8(json_as_string(text));
  					notification->link = utils::text::special_expressions_decode(json_as_string(url));					
					notification->id = json_as_string(alert_id);

					std::string::size_type pos = notification->id.find(":");
					if (pos != std::string::npos)
						notification->id = notification->id.substr(pos+1);

					notifications->push_back(notification);
				}
			}
		} else if (t == "typ") {
			// chat typing notification

			JSONNODE *from = json_get(it, "from");
			if (from == NULL)
				continue;

			facebook_user fbu;
			fbu.user_id = json_as_string(from);

			HANDLE hContact = proto->AddToContactList(&fbu, CONTACT_FRIEND);
				
			if (proto->getWord(hContact, "Status", 0) == ID_STATUS_OFFLINE)
				proto->setWord(hContact, "Status", ID_STATUS_ONLINE);

			JSONNODE *st = json_get(it, "st");
			if (json_as_int(st) == 1)
				CallService(MS_PROTO_CONTACTISTYPING, (WPARAM)hContact, (LPARAM)60);
			else
				CallService(MS_PROTO_CONTACTISTYPING, (WPARAM)hContact, (LPARAM)PROTOTYPE_CONTACTTYPING_OFF);
		} else if (t == "privacy_changed") {
			// settings changed

			JSONNODE *event_type = json_get(it, "event");
			JSONNODE *event_data = json_get(it, "data");

			if (event_type == NULL || event_data == NULL)
				continue;

			std::string t = json_as_string(event_type);
			if (t == "visibility_update") {
				// change of chat status
				JSONNODE *visibility = json_get(event_data, "visibility");
				
				bool isVisible = (visibility != NULL) && json_as_bool(visibility);
				proto->Log("      Requested chat switch to %s", isVisible ? "Online" : "Offline");
				proto->SetStatus(isVisible ? ID_STATUS_ONLINE : ID_STATUS_INVISIBLE);
			}				
		}
		else
			continue;
	}

	// remove received messages from map
	for (std::map<std::string, bool>::iterator it = proto->facy.messages_ignore.begin(); it != proto->facy.messages_ignore.end(); ++it) {
		if (it->second)
			proto->facy.messages_ignore.erase(it);
	}

	return EXIT_SUCCESS;
}
