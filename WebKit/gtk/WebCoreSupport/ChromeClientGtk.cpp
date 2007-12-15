/*
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2007 Christian Dywan <christian@twotoasts.de>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "ChromeClientGtk.h"

#include "FloatRect.h"
#include "IntRect.h"
#include "PlatformString.h"
#include "CString.h"
#include "HitTestResult.h"
#include "KURL.h"
#include "webkitwebview.h"
#include "webkitprivate.h"
#include "NotImplemented.h"
#include "WindowFeatures.h"

using namespace WebCore;

namespace WebKit {
ChromeClient::ChromeClient(WebKitWebView* webView)
    : m_webView(webView)
{
}

void ChromeClient::chromeDestroyed()
{
    notImplemented();
}

FloatRect ChromeClient::windowRect()
{
    notImplemented();
    return FloatRect();
}

void ChromeClient::setWindowRect(const FloatRect& r)
{
    notImplemented();
}

FloatRect ChromeClient::pageRect()
{
    notImplemented();
    return FloatRect();
}

float ChromeClient::scaleFactor()
{
    notImplemented();
    return 1.0;
}

void ChromeClient::focus()
{
    notImplemented();
}

void ChromeClient::unfocus()
{
    notImplemented();
}

Page* ChromeClient::createWindow(Frame*, const FrameLoadRequest&, const WindowFeatures& features)
{
    if (features.dialog) {
        notImplemented();
        return 0;
    } else {
        /* TODO: FrameLoadRequest is not used */
        WebKitWebView* webView = WEBKIT_WEB_VIEW_GET_CLASS(m_webView)->create_web_view(m_webView);
        if (!webView)
            return 0;

        WebKitWebViewPrivate* privateData = WEBKIT_WEB_VIEW_GET_PRIVATE(webView);
        return privateData->corePage;
    }
}

void ChromeClient::show()
{
    notImplemented();
}

bool ChromeClient::canRunModal()
{
    notImplemented();
    return false;
}

void ChromeClient::runModal()
{
    notImplemented();
}

void ChromeClient::setToolbarsVisible(bool)
{
    notImplemented();
}

bool ChromeClient::toolbarsVisible()
{
    notImplemented();
    return false;
}

void ChromeClient::setStatusbarVisible(bool)
{
    notImplemented();
}

bool ChromeClient::statusbarVisible()
{
    notImplemented();
    return false;
}

void ChromeClient::setScrollbarsVisible(bool)
{
    notImplemented();
}

bool ChromeClient::scrollbarsVisible() {
    notImplemented();
    return false;
}

void ChromeClient::setMenubarVisible(bool)
{
    notImplemented();
}

bool ChromeClient::menubarVisible()
{
    notImplemented();
    return false;
}

void ChromeClient::setResizable(bool)
{
    notImplemented();
}

void ChromeClient::closeWindowSoon()
{
    notImplemented();
}

bool ChromeClient::canTakeFocus(FocusDirection)
{
    notImplemented();
    return true;
}

void ChromeClient::takeFocus(FocusDirection)
{
    notImplemented();
}

bool ChromeClient::canRunBeforeUnloadConfirmPanel()
{
    notImplemented();
    return false;
}

bool ChromeClient::runBeforeUnloadConfirmPanel(const WebCore::String&, WebCore::Frame*)
{
    notImplemented();
    return false;
}

void ChromeClient::addMessageToConsole(const WebCore::String& message, unsigned int lineNumber, const WebCore::String& sourceId)
{
    gboolean retval;
    g_signal_emit_by_name(m_webView, "console-message", message.utf8().data(), lineNumber, sourceId.utf8().data(), &retval);
}

void ChromeClient::runJavaScriptAlert(Frame* frame, const String& message)
{
    gboolean retval;
    g_signal_emit_by_name(m_webView, "script-alert", kit(frame), message.utf8().data(), &retval);
}

bool ChromeClient::runJavaScriptConfirm(Frame* frame, const String& message)
{
    gboolean retval;
    gboolean didConfirm;
    g_signal_emit_by_name(m_webView, "script-confirm", kit(frame), message.utf8().data(), &didConfirm, &retval);
    return didConfirm == TRUE;
}

bool ChromeClient::runJavaScriptPrompt(Frame* frame, const String& message, const String& defaultValue, String& result)
{
    gboolean retval;
    gchar* value = 0;
    g_signal_emit_by_name(m_webView, "script-prompt", kit(frame), message.utf8().data(), defaultValue.utf8().data(), &value, &retval);
    if (value) {
        result = String::fromUTF8(value);
        g_free(value);
        return true;
    }
    return false;
}

void ChromeClient::setStatusbarText(const String& string)
{
    CString stringMessage = string.utf8();
    g_signal_emit_by_name(m_webView, "status-bar-text-changed", stringMessage.data());
}

bool ChromeClient::shouldInterruptJavaScript()
{
    notImplemented();
    return false;
}

bool ChromeClient::tabsToLinks() const
{
    return true;
}

IntRect ChromeClient::windowResizerRect() const
{
    notImplemented();
    return IntRect();
}

void ChromeClient::addToDirtyRegion(const IntRect&)
{
    notImplemented();
}

void ChromeClient::scrollBackingStore(int dx, int dy, const IntRect& scrollViewRect, const IntRect& clipRect)
{
    notImplemented();
}

void ChromeClient::updateBackingStore()
{
    notImplemented();
}

void ChromeClient::mouseDidMoveOverElement(const HitTestResult& hit, unsigned modifierFlags)
{
    // check if the element is a link...
    bool isLink = hit.isLiveLink();
    if (isLink) {
        KURL url = hit.absoluteLinkURL();
        if (!url.isEmpty() && url != m_hoveredLinkURL) {
            CString titleString = hit.title().utf8();
            DeprecatedCString urlString = url.prettyURL().utf8();
            g_signal_emit_by_name(m_webView, "hovering-over-link", titleString.data(), urlString.data());
            m_hoveredLinkURL = url;
        }
    } else if (!isLink && !m_hoveredLinkURL.isEmpty()) {
        g_signal_emit_by_name(m_webView, "hovering-over-link", 0, 0);
        m_hoveredLinkURL = KURL();
    }
}

void ChromeClient::setToolTip(const String& toolTip)
{
#if GTK_CHECK_VERSION(2,12,0)
    if (toolTip.isEmpty())
        g_object_set(G_OBJECT(m_webView), "has-tooltip", FALSE, NULL);
    else
        gtk_widget_set_tooltip_text(GTK_WIDGET(m_webView), toolTip.utf8().data());
#else
    // TODO: Support older GTK+ versions
    // See http://bugs.webkit.org/show_bug.cgi?id=15793
    notImplemented();
#endif
}

void ChromeClient::print(Frame* frame)
{
    webkit_web_frame_print(kit(frame));
}

unsigned long long ChromeClient::requestQuotaIncreaseForNewDatabase(Frame*, const SecurityOriginData&, const String&, unsigned long long)
{
    notImplemented();
    return 0;
}

unsigned long long ChromeClient::requestQuotaIncreaseForDatabaseOperation(Frame*, const SecurityOriginData&, const String&, unsigned long long)
{
    notImplemented();
    return 0;
}
        
}
