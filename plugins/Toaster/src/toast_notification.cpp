#include "stdafx.h"

using namespace Microsoft::WRL;

ToastNotification::ToastNotification(_In_ wchar_t* text, _In_ wchar_t* caption, _In_ wchar_t* imagePath)
	: _text(text), _caption(caption), _imagePath(imagePath)
{}

ToastNotification::~ToastNotification()
{
}

HRESULT ToastNotification::Initialize()
{
	CHECKHR(Windows::Foundation::GetActivationFactory(StringReferenceWrapper(RuntimeClass_Windows_UI_Notifications_ToastNotificationManager).Get(), &notificationManager));
	CHECKHR(notificationManager->CreateToastNotifierWithId(StringReferenceWrapper(::AppUserModelID).Get(), &notifier))
	return Create(&notification);
}

HRESULT ToastNotification::CreateXml(_Outptr_ ABI::Windows::Data::Xml::Dom::IXmlDocument** xml)
{
	ComPtr<ABI::Windows::Data::Xml::Dom::IXmlDocumentIO> xmlDocument;
	CHECKHR(Windows::Foundation::ActivateInstance(StringReferenceWrapper(RuntimeClass_Windows_Data_Xml_Dom_XmlDocument).Get(), &xmlDocument));

	HXML xmlToast = xmlCreateNode(L"toast", NULL, 0);

	HXML xmlAudioNode = xmlAddChild(xmlToast, L"audio", NULL);
	xmlAddAttr(xmlAudioNode, L"silent", L"true");

	HXML xmlVisualNode = xmlAddChild(xmlToast, L"visual", NULL);

	HXML xmlBindingNode = xmlAddChild(xmlVisualNode, L"binding", NULL);
	xmlAddAttr(xmlBindingNode, L"template", L"ToastImageAndText02");
	if (_imagePath)
	{
		HXML xmlImageNode = xmlAddChild(xmlBindingNode, L"image", NULL);
		xmlAddAttr(xmlImageNode, L"id", L"1");
		xmlAddAttr(xmlImageNode, L"src", CMString(FORMAT, L"file:///%s", _imagePath));
	}

	HXML xmlTitleNode = xmlAddChild(xmlBindingNode, L"text", _caption != NULL ? _caption : L"Miranda NG");
	xmlAddAttr(xmlTitleNode, L"id", L"1");
	if (_text)
	{
		HXML xmlTextNode = xmlAddChild(xmlBindingNode, L"text", _text);
		xmlAddAttr(xmlTextNode, L"id", L"2");
	}

	int nLength;
	TCHAR *xtmp = xmlToString(xmlToast, &nLength);
	xmlDestroyNode(xmlToast);

	CHECKHR(xmlDocument->LoadXml(StringReferenceWrapper(xtmp, nLength).Get()));

	return xmlDocument.CopyTo(xml);
}

HRESULT ToastNotification::Create(_Outptr_ ABI::Windows::UI::Notifications::IToastNotification** _notification)
{
	ComPtr<ABI::Windows::Data::Xml::Dom::IXmlDocument> xml;
	CHECKHR(CreateXml(&xml));

	ComPtr<ABI::Windows::UI::Notifications::IToastNotificationFactory> factory;
	CHECKHR(Windows::Foundation::GetActivationFactory(StringReferenceWrapper(RuntimeClass_Windows_UI_Notifications_ToastNotification).Get(), &factory));

	return factory->CreateToastNotification(xml.Get(), _notification);
}

HRESULT ToastNotification::Show(_In_ ToastHandlerData* thd)
{
	ComPtr<ToastEventHandler> eventHandler(new ToastEventHandler(thd));

	CHECKHR(notification->add_Activated(eventHandler.Get(), &_ertActivated));
	CHECKHR(notification->add_Dismissed(eventHandler.Get(), &_ertDismissed));
	CHECKHR(notification->add_Failed(eventHandler.Get(), &_ertFailed));

	return notifier->Show(notification.Get());
}

HRESULT ToastNotification::Hide()
{
	return notifier->Hide(notification.Get());
}

