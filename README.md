# wxWebViewBlink

Bring a modern and lightweight WebView control to wxWidgets on Microsoft Windows.

The web engine is based on weolar's [MiniBlink](https://github.com/weolar/miniblink49), a lite version of [Blink](https://www.chromium.org/blink) core of Chromium.

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
4. If you think the binary of MiniBlink is huge, use [UPX](https://upx.github.io/) to compress it to a much smaller one(~8MB).