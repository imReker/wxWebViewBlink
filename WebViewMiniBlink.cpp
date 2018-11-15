#include "StdAfx.h"

#if wxUSE_WEBVIEW && USE_WEBVIEW_MINIBLINK
#include <wx/filename.h>
#include <wx/filesys.h>
#include <wx/uri.h>
#include <wx/stdpaths.h>
#include <wx/app.h>
#include <wx/module.h>
#include <wx/private/jsscriptwrapper.h>
#include <WinInet.h>

#include "WebViewMiniBlink.h"

//Convenience function for error conversion
#define WX_ERROR_CASE(error, wxerror) \
        case error: \
            event.SetString(#error); \
            event.SetInt(wxerror); \
            break;

extern const char wxWebViewBackendMiniBlink[] = "wxWebViewMiniBlink";

wxIMPLEMENT_DYNAMIC_CLASS(wxWebViewMiniBlink, wxWebView);

bool wxWebViewMiniBlink::Create(wxWindow* parent,
	wxWindowID id,
	const wxString& url,
	const wxPoint& pos,
	const wxSize& size,
	long style,
	const wxString& name)
{
	m_busy = false;

	if (!wxControl::Create(parent, id, pos, size, style | wxNO_BORDER | wxCLIP_CHILDREN,
		wxDefaultValidator, name))
	{
		return false;
	}
	if (!Init())
	{
		return false;
	}

	wxSize clientSize = GetClientSize();
	m_webview = wkeCreateWebWindow(WKE_WINDOW_TYPE_CONTROL, GetHWND(), 0, 0, clientSize.GetWidth(), clientSize.GetHeight());

	m_historyLoadingFromList = false;
	m_historyEnabled = true;
	m_historyPosition = -1;

	wkeOnTitleChanged(m_webview, &wxWebViewMiniBlink::OnTitleChanged, this);
	wkeOnNavigation(m_webview, &wxWebViewMiniBlink::OnNavigation, this);
	wkeOnDocumentReady2(m_webview, &wxWebViewMiniBlink::OnDocumentReady, this);
	wkeOnLoadUrlBegin(m_webview, &wxWebViewMiniBlink::OnLoadUrlBegin, this);
	wkeOnLoadUrlEnd(m_webview, &wxWebViewMiniBlink::OnLoadUrlEnd, this);
	wkeOnLoadingFinish(m_webview, &wxWebViewMiniBlink::OnLoadingFinish, this);
	wkeOnOtherLoad(m_webview, &wxWebViewMiniBlink::OnOtherLoad, this);
	wkeOnCreateView(m_webview, &wxWebViewMiniBlink::OnCreateWindow, this);
	wkeOnDidCreateScriptContext(m_webview, &wxWebViewMiniBlink::OnCreateScriptContext, this);

	wkeLoadW(m_webview, url);

	this->Bind(wxEVT_SIZE, &wxWebViewMiniBlink::OnSize, this);

	wkeShowWindow(m_webview, true);
	return true;
}

wxWebViewMiniBlink::~wxWebViewMiniBlink()
{
	if (m_webview)
		wkeDestroyWebWindow(m_webview);
}

bool wxWebViewMiniBlink::Init()
{
	if (wkeIsInitialize && wkeIsInitialize())
		return true;

	wkeInitialize();
	return wkeIsInitialize();
}

void wxWebViewMiniBlink::Shutdown()
{
	if (wkeIsInitialize && wkeIsInitialize())
	{
		wkeShutdown();
	}
}

wxString wxWebViewMiniBlink::ConvertFrameId(wkeWebView webView, wkeWebFrameHandle frame)
{
    if ( !frame || wkeIsMainFrame(webView, frame) )
        return wxEmptyString;
    
    return wxString::Format("%i", (int)frame);
}

void wxWebViewMiniBlink::OnTitleChanged(wkeWebView /*webView*/, void* param, const wkeString title)
{
	wxWebViewMiniBlink* obj = (wxWebViewMiniBlink*)param;
	wxWebViewEvent event(wxEVT_WEBVIEW_TITLE_CHANGED, obj->GetId(), obj->GetCurrentURL(), wxEmptyString);
	event.SetString(wkeGetStringW(title));
	event.SetEventObject(obj);
	obj->HandleWindowEvent(event);
}

bool wxWebViewMiniBlink::OnNavigation(wkeWebView webView, void* param, wkeNavigationType navigationType, const wkeString url)
{
	wxWebViewMiniBlink* obj = (wxWebViewMiniBlink*)param;

	wkeTempCallbackInfo* info = wkeGetTempCallbackInfo(webView);
	return obj->OnNavigationInternal(navigationType, wkeGetStringW(url), info->frame);
}

void wxWebViewMiniBlink::OnDocumentReady(wkeWebView webView, void* param, wkeWebFrameHandle frameId)
{
	wxWebViewMiniBlink* obj = (wxWebViewMiniBlink*)param;

	wxString url = wxString::FromUTF8(wkeGetFrameUrl(webView, frameId));
	wxWebViewEvent event(wxEVT_WEBVIEW_NAVIGATED, obj->GetId(), url, wxWebViewMiniBlink::ConvertFrameId(webView, frameId));
	event.SetEventObject(obj);
	obj->HandleWindowEvent(event);
	if (frameId == nullptr || wkeIsMainFrame(webView, frameId))
	{
		//As we are complete we also add to the history list, but not if the
		//page is not the main page, ie it is a subframe
		wxString currentUrl = obj->GetCurrentURL();
		if (obj->m_historyEnabled && !obj->m_historyLoadingFromList &&
			(url == currentUrl ||
			(currentUrl.substr(0, 4) == "file" &&
				wxFileSystem::URLToFileName(currentUrl).GetFullPath() == url)))
		{
			//If we are not at the end of the list, then erase everything
			//between us and the end before adding the new page
			if (obj->m_historyPosition != static_cast<int>(obj->m_historyList.size()) - 1)
			{
				obj->m_historyList.erase(obj->m_historyList.begin() + obj->m_historyPosition + 1,
					obj->m_historyList.end());
			}
			wxSharedPtr<wxWebViewHistoryItem> item(new wxWebViewHistoryItem(url, obj->GetCurrentTitle()));
			obj->m_historyList.push_back(item);
			obj->m_historyPosition++;
		}
		//Reset as we are done now
		obj->m_historyLoadingFromList = false;
	}
}

bool wxWebViewMiniBlink::OnLoadUrlBegin(wkeWebView /*webView*/, void* param, const char* url, wkeNetJob job)
{
	wxWebViewMiniBlink* obj = (wxWebViewMiniBlink*)param;

	wxString uri = wxURI::Unescape(wxString::FromUTF8(url));
	wxSharedPtr<wxWebViewHandler> handler;
	wxVector<wxSharedPtr<wxWebViewHandler> > handlers = obj->GetHandlers();

	//See if we match one of the additional handlers
	for (wxVector<wxSharedPtr<wxWebViewHandler> >::iterator it = handlers.begin();
		it != handlers.end(); ++it)
	{
		if (uri.substr(0, (*it)->GetName().length()) == (*it)->GetName())
		{
			handler = (*it);
		}
	}
	//If we found a handler we can then use it to load the file directly
	//ourselves
	if (handler)
	{
		wxFSFile* file = handler->GetFile(uri);
		if (file)
		{
			wxString mime = file->GetMimeType();
			wkeNetSetMIMEType(job, const_cast<char*>(mime.ToUTF8().data()));
			size_t size = file->GetStream()->GetLength();
			char *buffer = new char[size];
			file->GetStream()->Read(buffer, size);
			wkeNetSetData(job, buffer, size);
			delete[] buffer;
			return true;
		}
		else
		{
			//TODO(reker): 404
		}
	}
	return false;
}

void wxWebViewMiniBlink::OnLoadUrlEnd(wkeWebView /*webView*/, void* /*param*/, const char* /*url*/, wkeNetJob /*job*/, void* /*buf*/, int /*len*/)
{}

void wxWebViewMiniBlink::OnLoadingFinish(wkeWebView webView, void* param, const wkeString url, wkeLoadingResult result, const wkeString failedReason)
{
	wxWebViewMiniBlink* obj = (wxWebViewMiniBlink*)param;

	obj->m_busy = false;
	wkeTempCallbackInfo* info = wkeGetTempCallbackInfo(webView);
	wxString frame = ConvertFrameId(webView, info->frame);
	wxString currentUrl = wkeGetStringW(url);
	if (currentUrl.length() == 0)
	{
		if (info->frame == nullptr || wkeIsMainFrame(webView, info->frame))
		{
			currentUrl = wxString::FromUTF8(wkeGetURL(webView));
		}
		else
		{
			currentUrl = wxString::FromUTF8(wkeGetFrameUrl(webView, info->frame));
		}
	}

	if (result == WKE_LOADING_SUCCEEDED)
	{
		wxWebViewEvent levent(wxEVT_WEBVIEW_LOADED, obj->GetId(), currentUrl, frame);
		levent.SetEventObject(obj);
		obj->HandleWindowEvent(levent);
		return;
	}

	wxString reason = wkeGetStringW(failedReason);
	wxWebViewNavigationError type = wxWEBVIEW_NAV_ERR_OTHER;

	if (result == WKE_LOADING_FAILED)
	{
		unsigned long code;
		reason.substr(14, 2).ToULong(&code);
		switch (code)
		{
		case 1: //CURLE_UNSUPPORTED_PROTOCOL
		case 3: //CURLE_URL_MALFORMAT
			type = wxWEBVIEW_NAV_ERR_REQUEST;
			break;
		case 6: //CURLE_COULDNT_RESOLVE_HOST
		case 7: //CURLE_COULDNT_CONNECT
		case 9: //CURLE_REMOTE_ACCESS_DENIED
		case 22: //CURLE_HTTP_RETURNED_ERROR
		case 55: //CURLE_SEND_ERROR
		case 56: //CURLE_RECV_ERROR
			type = wxWEBVIEW_NAV_ERR_CONNECTION;
			break;
		}
	}
	else
	{
		type = wxWEBVIEW_NAV_ERR_USER_CANCELLED;
	}

	wxWebViewEvent event(wxEVT_WEBVIEW_ERROR,
		obj->GetId(),
		wkeGetStringW(url), frame);
	event.SetString(reason);
	event.SetInt(type);

	obj->HandleWindowEvent(event);
}

void wxWebViewMiniBlink::OnOtherLoad(wkeWebView webView, void* param, wkeOtherLoadType type, wkeTempCallbackInfo* info)
{
	wxWebViewMiniBlink* obj = (wxWebViewMiniBlink*)param;

	if (type == WKE_DID_NAVIGATE_IN_PAGE)
	{
		obj->OnNavigationInternal(WKE_NAVIGATION_TYPE_LINKCLICK, wxString::FromUTF8(wkeGetFrameUrl(webView, info->frame)), info->frame);
		obj->OnDocumentReady(webView, param, info->frame);

		obj->m_busy = false;
	}
	else if (type == WKE_DID_GET_RESPONSE_DETAILS &&
		(info->willSendRequestInfo->resourceType & 0xFFFFFFFE) == 0)
	{
		if (info->willSendRequestInfo->httpResponseCode >= 400 &&
				info->willSendRequestInfo->httpResponseCode < 600)
		{
			wxWebViewEvent event(wxEVT_WEBVIEW_ERROR, obj->GetId(), wkeGetStringW(info->willSendRequestInfo->url), ConvertFrameId(webView, info->frame));
			event.SetEventObject(obj);

			switch (info->willSendRequestInfo->httpResponseCode)
			{
				// 400 Error codes
				WX_ERROR_CASE(HTTP_STATUS_BAD_REQUEST, wxWEBVIEW_NAV_ERR_REQUEST);
				WX_ERROR_CASE(HTTP_STATUS_DENIED, wxWEBVIEW_NAV_ERR_AUTH);
				WX_ERROR_CASE(HTTP_STATUS_PAYMENT_REQ, wxWEBVIEW_NAV_ERR_OTHER);
				WX_ERROR_CASE(HTTP_STATUS_FORBIDDEN, wxWEBVIEW_NAV_ERR_AUTH);
				WX_ERROR_CASE(HTTP_STATUS_NOT_FOUND, wxWEBVIEW_NAV_ERR_NOT_FOUND);
				WX_ERROR_CASE(HTTP_STATUS_BAD_METHOD, wxWEBVIEW_NAV_ERR_REQUEST);
				WX_ERROR_CASE(HTTP_STATUS_NONE_ACCEPTABLE, wxWEBVIEW_NAV_ERR_OTHER);
				WX_ERROR_CASE(HTTP_STATUS_PROXY_AUTH_REQ, wxWEBVIEW_NAV_ERR_AUTH);
				WX_ERROR_CASE(HTTP_STATUS_REQUEST_TIMEOUT, wxWEBVIEW_NAV_ERR_CONNECTION);
				WX_ERROR_CASE(HTTP_STATUS_CONFLICT, wxWEBVIEW_NAV_ERR_REQUEST);
				WX_ERROR_CASE(HTTP_STATUS_GONE, wxWEBVIEW_NAV_ERR_NOT_FOUND);
				WX_ERROR_CASE(HTTP_STATUS_LENGTH_REQUIRED, wxWEBVIEW_NAV_ERR_REQUEST);
				WX_ERROR_CASE(HTTP_STATUS_PRECOND_FAILED, wxWEBVIEW_NAV_ERR_REQUEST);
				WX_ERROR_CASE(HTTP_STATUS_REQUEST_TOO_LARGE, wxWEBVIEW_NAV_ERR_REQUEST);
				WX_ERROR_CASE(HTTP_STATUS_URI_TOO_LONG, wxWEBVIEW_NAV_ERR_REQUEST);
				WX_ERROR_CASE(HTTP_STATUS_UNSUPPORTED_MEDIA, wxWEBVIEW_NAV_ERR_REQUEST);
				WX_ERROR_CASE(HTTP_STATUS_RETRY_WITH, wxWEBVIEW_NAV_ERR_OTHER);

				// 500 - Error codes
				WX_ERROR_CASE(HTTP_STATUS_SERVER_ERROR, wxWEBVIEW_NAV_ERR_CONNECTION);
				WX_ERROR_CASE(HTTP_STATUS_NOT_SUPPORTED, wxWEBVIEW_NAV_ERR_CONNECTION);
				WX_ERROR_CASE(HTTP_STATUS_BAD_GATEWAY, wxWEBVIEW_NAV_ERR_CONNECTION);
				WX_ERROR_CASE(HTTP_STATUS_SERVICE_UNAVAIL, wxWEBVIEW_NAV_ERR_CONNECTION);
				WX_ERROR_CASE(HTTP_STATUS_GATEWAY_TIMEOUT, wxWEBVIEW_NAV_ERR_CONNECTION);
				WX_ERROR_CASE(HTTP_STATUS_VERSION_NOT_SUP, wxWEBVIEW_NAV_ERR_REQUEST);
			default:
				event.SetInt(wxWEBVIEW_NAV_ERR_OTHER);
			}
			obj->HandleWindowEvent(event);
		}
	}
}

wkeWebView wxWebViewMiniBlink::OnCreateWindow(wkeWebView webView, void* param, wkeNavigationType navigationType, const wkeString url, const wkeWindowFeatures* windowFeatures)
{
	wxWebViewMiniBlink* obj = (wxWebViewMiniBlink*)param;

	wkeTempCallbackInfo* info = wkeGetTempCallbackInfo(webView);
	wxString frame = ConvertFrameId(webView, info->frame);
	wxWebViewEvent event(wxEVT_WEBVIEW_NEWWINDOW,
		obj->GetId(),
		wkeGetStringW(url),
		frame);

	obj->HandleWindowEvent(event);
	return nullptr;
}


void wxWebViewMiniBlink::OnCreateScriptContext(wkeWebView webView, void* param, wkeWebFrameHandle frameId, void* context, int extensionGroup, int worldId)
{
}

void wxWebViewMiniBlink::ShowDevTool(const wxString& path)
{
	wkeSetDebugConfig(m_webview, "showDevTools", path.ToUTF8());
}

void wxWebViewMiniBlink::SetProxy(const wxString& proxy)
{
    wxURI uri;
    if (proxy.IsEmpty()) {
        m_proxy.type = WKE_PROXY_NONE;
    }
    else if (!uri.Create(proxy.Lower())) {
        return;
    }
    strcpy_s(m_proxy.hostname, uri.GetServer().ToUTF8());
    unsigned long port;
    if (!uri.GetPort().ToCULong(&port)) {
        return;
    }
    m_proxy.port = port;
    if (uri.GetScheme().StartsWith("socks5")) {
        if (uri.GetScheme().IsSameAs("socks5")) {
            m_proxy.type = WKE_PROXY_SOCKS5;
        }
        else if (uri.GetScheme().IsSameAs("socks5h")) {
            m_proxy.type = WKE_PROXY_SOCKS5HOSTNAME;
        }
        else {
            return;
        }
        strcpy_s(m_proxy.username, uri.GetUser().ToUTF8());
        strcpy_s(m_proxy.password, uri.GetPassword().ToUTF8());
    }
    else if (uri.GetScheme().IsSameAs("socks4a")) {
        m_proxy.type = WKE_PROXY_SOCKS4A;
    }
    else if (uri.GetScheme().IsSameAs("socks4")) {
        m_proxy.type = WKE_PROXY_SOCKS4;
    }
    else if (uri.GetScheme().IsSameAs("http")) {
        m_proxy.type = WKE_PROXY_HTTP;
    }
    else {
        return;
    }
    wkeSetProxy(&m_proxy);
}

wxString wxWebViewMiniBlink::GetProxy()
{
    switch (m_proxy.type) {
    case WKE_PROXY_NONE:
        break;
    case WKE_PROXY_HTTP:
        return wxString::Format("http://%s:%i", m_proxy.hostname, m_proxy.port);
    case WKE_PROXY_SOCKS4:
        return wxString::Format("socks4://%s:%i", m_proxy.hostname, m_proxy.port);
    case WKE_PROXY_SOCKS4A:
        return wxString::Format("socks4a://%s:%i", m_proxy.hostname, m_proxy.port);
    case WKE_PROXY_SOCKS5:
    case WKE_PROXY_SOCKS5HOSTNAME:
        return wxString::Format("socks5%s://%s%s%s@%s:%i",
            m_proxy.type == WKE_PROXY_SOCKS5HOSTNAME ? "h" : "",
            strlen(m_proxy.username) > 0 ? m_proxy.username : "",
            strlen(m_proxy.password) > 0 ? ":" : "",
            strlen(m_proxy.password) > 0 ? m_proxy.password : "",
            m_proxy.hostname,
            m_proxy.port);
    }
    return wxEmptyString;
}

void wxWebViewMiniBlink::OnSize(wxSizeEvent& event)
{
	wxSize size = GetClientSize();
	if (m_webview)
	{
		wkeResize(m_webview, size.GetWidth(), size.GetHeight());
	}

	event.Skip();
}

void wxWebViewMiniBlink::SetPageSource(const wxString& /*pageSource*/)
{}

void wxWebViewMiniBlink::SetPageText(const wxString& /*pageText*/)
{}

void* wxWebViewMiniBlink::GetNativeBackend() const
{
	return m_webview;
}

bool wxWebViewMiniBlink::CanGoForward() const
{
	if (m_historyEnabled)
		return m_historyPosition != static_cast<int>(m_historyList.size()) - 1;
	else
		return false;
}

bool wxWebViewMiniBlink::CanGoBack() const
{
	if (m_historyEnabled)
		return m_historyPosition > 0;
	else
		return false;
}

void wxWebViewMiniBlink::LoadHistoryItem(wxSharedPtr<wxWebViewHistoryItem> item)
{
	int pos = -1;
	for (unsigned int i = 0; i < m_historyList.size(); i++)
	{
		//We compare the actual pointers to find the correct item
		if (m_historyList[i].get() == item.get())
			pos = i;
	}
	wxASSERT_MSG(pos != static_cast<int>(m_historyList.size()),
		"invalid history item");
	m_historyLoadingFromList = true;
	LoadURL(item->GetUrl());
	m_historyPosition = pos;
}

wxVector<wxSharedPtr<wxWebViewHistoryItem> > wxWebViewMiniBlink::GetBackwardHistory()
{
	wxVector<wxSharedPtr<wxWebViewHistoryItem> > backhist;
	//As we don't have std::copy or an iterator constructor in the wxwidgets
	//native vector we construct it by hand
	for (int i = 0; i < m_historyPosition; i++)
	{
		backhist.push_back(m_historyList[i]);
	}
	return backhist;
}

wxVector<wxSharedPtr<wxWebViewHistoryItem> > wxWebViewMiniBlink::GetForwardHistory()
{
	wxVector<wxSharedPtr<wxWebViewHistoryItem> > forwardhist;
	//As we don't have std::copy or an iterator constructor in the wxwidgets
	//native vector we construct it by hand
	for (int i = m_historyPosition + 1; i < static_cast<int>(m_historyList.size()); i++)
	{
		forwardhist.push_back(m_historyList[i]);
	}
	return forwardhist;
}

void wxWebViewMiniBlink::GoBack()
{
	LoadHistoryItem(m_historyList[m_historyPosition - 1]);
}

void wxWebViewMiniBlink::GoForward()
{
	LoadHistoryItem(m_historyList[m_historyPosition + 1]);
}

void wxWebViewMiniBlink::LoadURL(const wxString& url)
{
	/*if (!url.StartsWith("http"))
	{
		wkeLoadFileW(m_webview, url);
	}
	else
	{*/
		wkeLoadW(m_webview, url);
	//}
}

void wxWebViewMiniBlink::ClearHistory()
{
	m_historyList.clear();
	m_historyPosition = -1;
}

void wxWebViewMiniBlink::EnableHistory(bool enable)
{
	m_historyEnabled = enable;
}

void wxWebViewMiniBlink::Stop()
{
	wkeStopLoading(m_webview);
}

void wxWebViewMiniBlink::Reload(wxWebViewReloadFlags flags)
{
	if (flags == wxWEBVIEW_RELOAD_NO_CACHE)
	{
		wkeReload(m_webview);
	}
	else
	{
		wkeReload(m_webview);
	}
}

wxString wxWebViewMiniBlink::GetPageSource() const
{
	wxString result;
	(const_cast<wxWebViewMiniBlink*>(this))->RunScript("$('html').html();", &result);
	return result;
}

wxString wxWebViewMiniBlink::GetPageText() const
{
	wxString result;
	(const_cast<wxWebViewMiniBlink*>(this))->RunScript("$('body').text();", &result);
	return result;
}

wxString wxWebViewMiniBlink::GetCurrentURL() const
{
	return wxString::FromUTF8(wkeGetURL(m_webview));
}

wxString wxWebViewMiniBlink::GetCurrentTitle() const
{
	return wkeGetTitleW(m_webview);
}

void wxWebViewMiniBlink::Print()
{
	return;
}

void wxWebViewMiniBlink::Cut()
{
	wkeEditorCut(m_webview);
}

void wxWebViewMiniBlink::Copy()
{
	wkeEditorCopy(m_webview);
}

void wxWebViewMiniBlink::Paste()
{
	wkeEditorPaste(m_webview);
}

void wxWebViewMiniBlink::Undo()
{
	wkeEditorUndo(m_webview);
}

void wxWebViewMiniBlink::Redo()
{
	wkeEditorRedo(m_webview);
}

bool wxWebViewMiniBlink::HasSelection() const
{
	return wkeHasSelection(m_webview);
}

void wxWebViewMiniBlink::SelectAll()
{
	wkeEditorSelectAll(m_webview);
}

void wxWebViewMiniBlink::DeleteSelection()
{
	wkeEditorDelete(m_webview);
}

void wxWebViewMiniBlink::ClearSelection()
{
	wkeEditorUnSelect(m_webview);
}

wxString wxWebViewMiniBlink::GetSelectedText() const
{
	return wkeGetSelectedTextW(m_webview);
}

wxString wxWebViewMiniBlink::GetSelectedSource() const
{
	return wkeGetSelectedSourceW(m_webview);
}

// Run the given script synchronously and return its result in output.
bool wxWebViewMiniBlink::RunScriptInternal(const wxString& javascript, wxString* output)
{
	jsExecState es = wkeGlobalExec(m_webview);
	jsValue value = wkeRunJSW(m_webview, javascript, false);
	jsExceptionInfo* exp = jsGetLastErrorIfException(es);
	if (exp)
	{
		if (output)
			*output = exp->message;
		return false;
	}
	if (output)
	{
		*output = wxString(jsToTempStringW(es, value));
	}
	return true;
}

bool wxWebViewMiniBlink::OnNavigationInternal(wkeNavigationType navigationType, const wxString& url, wkeWebFrameHandle frame)
{
	m_busy = true;

	wxWebViewEvent event(wxEVT_WEBVIEW_NAVIGATING, GetId(), url, ConvertFrameId(m_webview, frame));
	event.SetEventObject(this);
	event.SetExtraLong(navigationType);
	HandleWindowEvent(event);
	if (!event.IsAllowed())
	{
		m_busy = false;
		return false;
	}
	return true;
}

bool wxWebViewMiniBlink::RunScript(const wxString& javascript, wxString* output)
{
	wxJSScriptWrapper wrapJS(javascript, &m_runScriptCount);

	// This string is also used as an error indicator: it's cleared if there is
	// no error or used in the warning message below if there is one.
	wxString result;
	if (RunScriptInternal(wrapJS.GetWrappedCode(), &result)
		&& result == wxS("true"))
	{
		if (RunScriptInternal(wrapJS.GetOutputCode(), &result))
		{
			if (output)
				*output = result;
			result.clear();
		}

		RunScriptInternal(wrapJS.GetCleanUpCode());
	}

	if (!result.empty())
	{
		wxLogWarning(_("Error running JavaScript: %s"), result);
		return false;
	}

	return true;
}

bool wxWebViewMiniBlink::IsBusy() const
{
	return m_busy;
}

void wxWebViewMiniBlink::SetEditable(bool enable)
{
	//wkeSetEditable(enable);
	wxString mode = enable ? "\"on\"" : "\"off\"";
	RunScript("document.designMode = " + mode);
}

void wxWebViewMiniBlink::DoSetPage(const wxString& html, const wxString& baseUrl)
{
	if (baseUrl.empty())
		wkeLoadHTML(m_webview, html.ToUTF8());
	else
		wkeLoadHtmlWithBaseUrl(m_webview, html.ToUTF8(), baseUrl.ToUTF8());
}

wxWebViewZoom wxWebViewMiniBlink::GetZoom() const
{
	float zoom = wkeGetZoomFactor(m_webview);

	// arbitrary way to map float zoom to our common zoom enum
	if (zoom <= 0.65f)
	{
		return wxWEBVIEW_ZOOM_TINY;
	}
	if (zoom <= 0.90f)
	{
		return wxWEBVIEW_ZOOM_SMALL;
	}
	if (zoom <= 1.15f)
	{
		return wxWEBVIEW_ZOOM_MEDIUM;
	}
	if (zoom <= 1.45f)
	{
		return wxWEBVIEW_ZOOM_LARGE;
	}
	return wxWEBVIEW_ZOOM_LARGEST;
}

void wxWebViewMiniBlink::SetZoom(wxWebViewZoom zoom)
{
	float mapzoom = 1.0f;
	// arbitrary way to map our common zoom enum to float zoom
	switch (zoom)
	{
	case wxWEBVIEW_ZOOM_TINY:
		mapzoom = 0.6f;
		break;
	case wxWEBVIEW_ZOOM_SMALL:
		mapzoom = 0.8f;
		break;
	case wxWEBVIEW_ZOOM_MEDIUM:
		mapzoom = 1.0f;
		break;
	case wxWEBVIEW_ZOOM_LARGE:
		mapzoom = 1.3f;
		break;
	case wxWEBVIEW_ZOOM_LARGEST:
		mapzoom = 1.6f;
		break;
	default:
		wxFAIL;
	}
	wkeSetZoomFactor(m_webview, mapzoom);
}

void wxWebViewMiniBlink::SetZoomType(wxWebViewZoomType type)
{
	// there is only one supported zoom type at the moment so this setter
	// does nothing beyond checking sanity
	wxASSERT(type == wxWEBVIEW_ZOOM_TYPE_LAYOUT);
}

wxWebViewZoomType wxWebViewMiniBlink::GetZoomType() const
{
	return wxWEBVIEW_ZOOM_TYPE_LAYOUT;
}

bool wxWebViewMiniBlink::CanSetZoomType(wxWebViewZoomType type) const
{
	return type == wxWEBVIEW_ZOOM_TYPE_LAYOUT;
}

void wxWebViewMiniBlink::RegisterHandler(wxSharedPtr<wxWebViewHandler> handler)
{
	m_handlerList.push_back(handler);
}

class wxWebViewMiniBlinkModule : public wxModule
{
public:
	wxWebViewMiniBlinkModule()
	{}
	virtual bool OnInit() wxOVERRIDE
	{
		// Register with wxWebView
		wxWebView::RegisterFactory(wxWebViewBackendMiniBlink,
			wxSharedPtr<wxWebViewFactory>(new wxWebViewFactoryMiniBlink));

		return true;
	};
	virtual void OnExit() wxOVERRIDE
	{
		wxWebViewMiniBlink::Shutdown();
	};
private:
	wxDECLARE_DYNAMIC_CLASS(wxWebViewMiniBlinkModule);
};

wxIMPLEMENT_DYNAMIC_CLASS(wxWebViewMiniBlinkModule, wxModule);

#endif // wxUSE_WEBVIEW && wxUSE_WEBVIEW_CHROMIUM
