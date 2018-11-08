#pragma once
#ifndef _WX_WEBVIEWMINIBLINK_H_
#define _WX_WEBVIEWMINIBLINK_H_

#if USE_WEBVIEW_MINIBLINK
#include <wx/defs.h>
#include <wx/webview.h>
#include <wx/timer.h>
#include "wkedefine.h"

extern const char wxWebViewBackendMiniBlink[];

class wxWebViewMiniBlink : public wxWebView
{
public:
	wxWebViewMiniBlink() {}

	wxWebViewMiniBlink(wxWindow* parent,
		wxWindowID id,
		const wxString& url = wxWebViewDefaultURLStr,
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize,
		long style = 0,
		const wxString& name = wxWebViewNameStr)
	{
		Create(parent, id, url, pos, size, style, name);
	}

	~wxWebViewMiniBlink();

	void ShowDevTool(const wxString& path);

	void OnSize(wxSizeEvent &event);

	void SetPageSource(const wxString& pageSource);

	void SetPageText(const wxString& pageText);

	bool Create(wxWindow* parent,
		wxWindowID id,
		const wxString& url = wxWebViewDefaultURLStr,
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize,
		long style = 0,
		const wxString& name = wxWebViewNameStr) wxOVERRIDE;

	virtual void LoadURL(const wxString& url) wxOVERRIDE;
	virtual void LoadHistoryItem(wxSharedPtr<wxWebViewHistoryItem> item) wxOVERRIDE;
	virtual wxVector<wxSharedPtr<wxWebViewHistoryItem> > GetBackwardHistory() wxOVERRIDE;
	virtual wxVector<wxSharedPtr<wxWebViewHistoryItem> > GetForwardHistory() wxOVERRIDE;

	virtual bool CanGoForward() const wxOVERRIDE;
	virtual bool CanGoBack() const wxOVERRIDE;
	virtual void GoBack() wxOVERRIDE;
	virtual void GoForward() wxOVERRIDE;
	virtual void ClearHistory() wxOVERRIDE;
	virtual void EnableHistory(bool enable = true) wxOVERRIDE;
	virtual void Stop() wxOVERRIDE;
	virtual void Reload(wxWebViewReloadFlags flags = wxWEBVIEW_RELOAD_DEFAULT) wxOVERRIDE;

	virtual wxString GetPageSource() const wxOVERRIDE;
	virtual wxString GetPageText() const wxOVERRIDE;

	virtual bool IsBusy() const wxOVERRIDE;
	virtual wxString GetCurrentURL() const wxOVERRIDE;
	virtual wxString GetCurrentTitle() const wxOVERRIDE;

	virtual void SetZoomType(wxWebViewZoomType type) wxOVERRIDE;
	virtual wxWebViewZoomType GetZoomType() const wxOVERRIDE;
	virtual bool CanSetZoomType(wxWebViewZoomType type) const wxOVERRIDE;

	virtual void Print() wxOVERRIDE;

	virtual wxWebViewZoom GetZoom() const wxOVERRIDE;
	virtual void SetZoom(wxWebViewZoom zoom) wxOVERRIDE;

	virtual void* GetNativeBackend() const wxOVERRIDE;

	virtual long Find(const wxString& WXUNUSED(text), int WXUNUSED(flags) = wxWEBVIEW_FIND_DEFAULT) wxOVERRIDE { return wxNOT_FOUND; }

	//Clipboard functions
	virtual bool CanCut() const wxOVERRIDE { return true; }
	virtual bool CanCopy() const wxOVERRIDE { return true; }
	virtual bool CanPaste() const wxOVERRIDE { return true; }
	virtual void Cut() wxOVERRIDE;
	virtual void Copy() wxOVERRIDE;
	virtual void Paste() wxOVERRIDE;

	//Undo / redo functionality
	virtual bool CanUndo() const wxOVERRIDE { return true; }
	virtual bool CanRedo() const wxOVERRIDE { return true; }
	virtual void Undo() wxOVERRIDE;
	virtual void Redo() wxOVERRIDE;

	//Editing functions
	virtual void SetEditable(bool enable = true) wxOVERRIDE;
	virtual bool IsEditable() const wxOVERRIDE { return false; }

	//Selection
	virtual bool HasSelection() const wxOVERRIDE;
	virtual void SelectAll() wxOVERRIDE;
	virtual void DeleteSelection() wxOVERRIDE;
	virtual void ClearSelection() wxOVERRIDE;
	virtual wxString GetSelectedText() const wxOVERRIDE;
	virtual wxString GetSelectedSource() const wxOVERRIDE;

	virtual bool RunScript(const wxString& javascript, wxString* output = nullptr) wxOVERRIDE;

	//Virtual File system Support
	virtual void RegisterHandler(wxSharedPtr<wxWebViewHandler> handler) wxOVERRIDE;
	virtual wxVector<wxSharedPtr<wxWebViewHandler> > GetHandlers() { return m_handlerList; }

protected:
	virtual void DoSetPage(const wxString& html, const wxString& baseUrl) wxOVERRIDE;

private:
	//History related variables, we currently use our own implementation
	wxVector<wxSharedPtr<wxWebViewHistoryItem> > m_historyList;
	int m_historyPosition;
	bool m_historyLoadingFromList;
	bool m_historyEnabled;
	wxVector<wxSharedPtr<wxWebViewHandler> > m_handlerList;

	bool m_busy;

	wkeWebView m_webview = nullptr;

	friend class wxWebViewMiniBlinkModule;

	static bool Init();
	static void Shutdown();

	static void OnTitleChanged(wkeWebView webView, void* param, const wkeString title);
	static bool OnNavigation(wkeWebView webView, void* param, wkeNavigationType navigationType, const wkeString url);
	static void OnDocumentReady(wkeWebView webView, void* param, wkeWebFrameHandle frameId);
	static bool OnLoadUrlBegin(wkeWebView webView, void* param, const char* url, wkeNetJob job);
	static void OnLoadUrlEnd(wkeWebView webView, void* param, const char* url, wkeNetJob job, void* buf, int len);
	static void OnLoadingFinish(wkeWebView webView, void* param, const wkeString url, wkeLoadingResult result, const wkeString failedReason);
	static void OnOtherLoad(wkeWebView webView, void* param, wkeOtherLoadType type, wkeTempCallbackInfo* info);
	static wkeWebView OnCreateWindow(wkeWebView webView, void* param, wkeNavigationType navigationType, const wkeString url, const wkeWindowFeatures* windowFeatures);
	static void OnCreateScriptContext(wkeWebView webView, void* param, wkeWebFrameHandle frameId, void* context, int extensionGroup, int worldId);

	static wxString ConvertFrameId(wkeWebView webView, wkeWebFrameHandle frame);

	bool RunScriptInternal(const wxString& javascript, wxString* output = nullptr);
	bool OnNavigationInternal(wkeNavigationType navigationType, const wxString& url, wkeWebFrameHandle frame);

	wxDECLARE_DYNAMIC_CLASS(wxWebViewMiniBlink);
};

class wxWebViewFactoryMiniBlink : public wxWebViewFactory
{
public:
	virtual wxWebView* Create() wxOVERRIDE { return new wxWebViewMiniBlink; }
	virtual wxWebView* Create(wxWindow* parent,
		wxWindowID id,
		const wxString& url = wxWebViewDefaultURLStr,
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize,
		long style = 0,
		const wxString& name = wxWebViewNameStr) wxOVERRIDE
	{
		return new wxWebViewMiniBlink(parent, id, url, pos, size, style, name);
	}
};

#endif // USE_WEBVIEW_MINIBLINK
#endif // _WX_WEBVIEWMINIBLINK_H_