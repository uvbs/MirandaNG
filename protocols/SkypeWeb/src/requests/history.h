/*
Copyright (c) 2015 Miranda NG project (http://miranda-ng.org)

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
#ifndef _SKYPE_REQUEST_HISTORY_H_
#define _SKYPE_REQUEST_HISTORY_H_

class SyncHistoryFirstRequest : public HttpRequest
{
public:
	SyncHistoryFirstRequest(int pageSize, LoginInfo &li) :
	  HttpRequest(REQUEST_GET, FORMAT, "%s/v1/users/ME/conversations", li.endpoint.szServer)
	{
		Url
			<< INT_VALUE("startTime", 0)
			<< INT_VALUE("pageSize", pageSize)
			<< CHAR_VALUE("view", "msnp24Equivalent")
			<< CHAR_VALUE("targetType", "Passport|Skype|Lync");

		Headers
			<< CHAR_VALUE("Accept", "application/json, text/javascript")
			<< FORMAT_VALUE("RegistrationToken", "registrationToken=%s", li.endpoint.szToken)
			<< CHAR_VALUE("Content-Type", "application/json; charset = UTF-8");
	}

	SyncHistoryFirstRequest(const char *url, LoginInfo &li) :
		HttpRequest(REQUEST_GET, url)
	{

		Headers
			<< CHAR_VALUE("Accept", "application/json, text/javascript")
			<< FORMAT_VALUE("RegistrationToken", "registrationToken=%s", li.endpoint.szToken)
			<< CHAR_VALUE("Content-Type", "application/json; charset = UTF-8");
	}
};

class GetHistoryRequest : public HttpRequest
{
public:
	GetHistoryRequest(const char *username, int pageSize, bool isChat, LONGLONG timestamp, LoginInfo &li) :
	  HttpRequest(REQUEST_GET, FORMAT, "%s/v1/users/ME/conversations/%d:%s/messages", li.endpoint.szServer, isChat ? 19 : 8, ptrA(mir_urlEncode(username)))
	{
		Url
			<< LONG_VALUE("startTime", timestamp)
			<< INT_VALUE("pageSize", pageSize)
			<< CHAR_VALUE("view", "msnp24Equivalent")
			<< CHAR_VALUE("targetType", "Passport|Skype|Lync|Thread");

		Headers
			<< CHAR_VALUE("Accept", "application/json, text/javascript")
			<< FORMAT_VALUE("RegistrationToken", "registrationToken=%s", li.endpoint.szToken)
			<< CHAR_VALUE("Content-Type", "application/json; charset = UTF-8");
	}
};

class GetHistoryOnUrlRequest : public HttpRequest
{
public:
	GetHistoryOnUrlRequest(const char *url, LoginInfo &li) :
		HttpRequest(REQUEST_GET, url)
	{
		Headers
			<< CHAR_VALUE("Accept", "application/json, text/javascript")
			<< FORMAT_VALUE("RegistrationToken", "registrationToken=%s", li.endpoint.szToken)
			<< CHAR_VALUE("Content-Type", "application/json; charset = UTF-8");
	}
};

#endif //_SKYPE_REQUEST_HISTORY_H_