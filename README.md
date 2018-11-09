# wxWebViewBlink

Bring a modern and lightweight WebView control to wxWidgets on Microsoft Windows.

The web engine is based on weolar's [MiniBlink](https://github.com/weolar/miniblink49), a lite version of [Blink](https://www.chromium.org/blink) core of Chromium.

### Compatibility
Support all Windows starting from Windows XP.
Linked to system origin msvcrt.dll directly, NOT rely on VS runtime library.
Passed all wxWebView test except Find in page and Print feature, you can try change [WebView sample](https://github.com/wxWidgets/wxWidgets/tree/master/samples/webview) in wxWidgets to use wxWebViewBlink.

### Usage
1. Download compiled binary release from [here](https://github.com/imReker/miniblink49/releases).
   Or, you can compile by yourself from [my own fork](https://github.com/imReker/miniblink49/tree/wxWidgets) of MiniBlink.

2. Define `USE_WEBVIEW_MINIBLINK=1` in your project configuration.

3. Include `WebViewMiniBlink.h` in your code.

4. Create instance of wxWebViewBlink by using `wxWebViewBackendMiniBlink` backend:
```
    const char* backend =
#if USE_WEBVIEW_MINIBLINK
    wxWebViewBackendMiniBlink;
#else
    wxWebViewBackendDefault;
#endif
    wxWebView::New(this, ID_WEBVIEW, wxEmptyString, wxDefaultPosition, GetClientSize(), backend);
```
5. Put node.dll in your app's directory.
6. If you think the binary of MiniBlink is huge, use [UPX](https://upx.github.io/) to compress it to a much smaller one (~5MiB).
