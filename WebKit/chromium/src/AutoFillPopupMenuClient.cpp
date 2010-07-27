/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AutoFillPopupMenuClient.h"

#include "CSSStyleSelector.h"
#include "CSSValueKeywords.h"
#include "Chrome.h"
#include "FrameView.h"
#include "HTMLInputElement.h"
#include "RenderTheme.h"
#include "WebNode.h"
#include "WebString.h"
#include "WebVector.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"

using namespace WebCore;

namespace WebKit {

AutoFillPopupMenuClient::AutoFillPopupMenuClient()
    : m_separatorIndex(-1)
    , m_selectedIndex(-1)
    , m_textField(0)
    , m_AutocompleteModeEnabled(false)
{
}

AutoFillPopupMenuClient::~AutoFillPopupMenuClient()
{
}

unsigned AutoFillPopupMenuClient::getSuggestionsCount() const
{
    return m_names.size() + ((m_separatorIndex == -1) ? 0 : 1);
}

WebString AutoFillPopupMenuClient::getSuggestion(unsigned listIndex) const
{
    int index = convertListIndexToInternalIndex(listIndex);
    if (index == -1)
        return WebString();

    ASSERT(index >= 0 && static_cast<size_t>(index) < m_names.size());
    return m_names[index];
}

WebString AutoFillPopupMenuClient::getLabel(unsigned listIndex) const
{
    int index = convertListIndexToInternalIndex(listIndex);
    if (index == -1)
        return WebString();

    ASSERT(index >= 0 && static_cast<size_t>(index) < m_labels.size());
    return m_labels[index];
}

WebString AutoFillPopupMenuClient::getIcon(unsigned listIndex) const
{
    int index = convertListIndexToInternalIndex(listIndex);
    if (index == -1)
        return WebString();

    ASSERT(index >= 0 && static_cast<size_t>(index) < m_icons.size());
    return m_icons[index];
}

void AutoFillPopupMenuClient::removeSuggestionAtIndex(unsigned listIndex)
{
    if (!canRemoveSuggestionAtIndex(listIndex))
        return;

    int index = convertListIndexToInternalIndex(listIndex);

    ASSERT(static_cast<unsigned>(index) < m_names.size());

    m_names.remove(index);
    m_labels.remove(index);
    m_icons.remove(index);
    m_uniqueIDs.remove(index);

    // Shift the separator index if necessary.
    if (m_separatorIndex != -1)
        m_separatorIndex--;
}

bool AutoFillPopupMenuClient::canRemoveSuggestionAtIndex(unsigned listIndex)
{
    // Only allow deletion of items before the separator and those that don't
    // have a label (autocomplete).
    int index = convertListIndexToInternalIndex(listIndex);
    return m_labels[index].isEmpty() && (m_separatorIndex == -1 || listIndex < static_cast<unsigned>(m_separatorIndex));
}

void AutoFillPopupMenuClient::valueChanged(unsigned listIndex, bool fireEvents)
{
    // DEPRECATED: Will be removed once AutoFill and Autocomplete merge is
    // completed.
    if (m_AutocompleteModeEnabled) {
        m_textField->setValue(getSuggestion(listIndex));

        WebViewImpl* webView = getWebView();
        if (!webView)
            return;

        EditorClientImpl* editor =
            static_cast<EditorClientImpl*>(webView->page()->editorClient());
        ASSERT(editor);
        editor->onAutocompleteSuggestionAccepted(
            static_cast<HTMLInputElement*>(m_textField.get()));
    } else {
      WebViewImpl* webView = getWebView();
      if (!webView)
          return;

      if (m_separatorIndex != -1 && listIndex > static_cast<unsigned>(m_separatorIndex))
          --listIndex;

      ASSERT(listIndex < m_names.size());

      webView->client()->didAcceptAutoFillSuggestion(WebNode(getTextField()),
                                                     m_names[listIndex],
                                                     m_labels[listIndex],
                                                     m_uniqueIDs[listIndex],
                                                     listIndex);
    }
}

void AutoFillPopupMenuClient::selectionChanged(unsigned listIndex, bool fireEvents)
{
    WebViewImpl* webView = getWebView();
    if (!webView)
        return;

    if (m_separatorIndex != -1 && listIndex > static_cast<unsigned>(m_separatorIndex))
        --listIndex;

    ASSERT(listIndex < m_names.size());

    webView->client()->didSelectAutoFillSuggestion(WebNode(getTextField()),
                                                   m_names[listIndex],
                                                   m_labels[listIndex],
                                                   m_uniqueIDs[listIndex]);
}

void AutoFillPopupMenuClient::selectionCleared()
{
    WebViewImpl* webView = getWebView();
    if (webView)
        webView->client()->didClearAutoFillSelection(WebNode(getTextField()));
}

String AutoFillPopupMenuClient::itemText(unsigned listIndex) const
{
    return getSuggestion(listIndex);
}

String AutoFillPopupMenuClient::itemLabel(unsigned listIndex) const
{
    return getLabel(listIndex);
}

String AutoFillPopupMenuClient::itemIcon(unsigned listIndex) const
{
    return getIcon(listIndex);
}

PopupMenuStyle AutoFillPopupMenuClient::itemStyle(unsigned listIndex) const
{
    return *m_style;
}

PopupMenuStyle AutoFillPopupMenuClient::menuStyle() const
{
    return *m_style;
}

int AutoFillPopupMenuClient::clientPaddingLeft() const
{
    // Bug http://crbug.com/7708 seems to indicate the style can be 0.
    RenderStyle* style = textFieldStyle();
    if (!style)
       return 0;

    return RenderTheme::defaultTheme()->popupInternalPaddingLeft(style);
}

int AutoFillPopupMenuClient::clientPaddingRight() const
{
    // Bug http://crbug.com/7708 seems to indicate the style can be 0.
    RenderStyle* style = textFieldStyle();
    if (!style)
        return 0;

    return RenderTheme::defaultTheme()->popupInternalPaddingRight(style);
}

void AutoFillPopupMenuClient::popupDidHide()
{
    WebViewImpl* webView = getWebView();
    if (!webView)
        return;

    webView->autoFillPopupDidHide();
    webView->client()->didClearAutoFillSelection(WebNode(getTextField()));
}

bool AutoFillPopupMenuClient::itemIsSeparator(unsigned listIndex) const
{
    return (m_separatorIndex != -1 && static_cast<unsigned>(m_separatorIndex) == listIndex);
}

void AutoFillPopupMenuClient::setTextFromItem(unsigned listIndex)
{
    m_textField->setValue(getSuggestion(listIndex));
}

FontSelector* AutoFillPopupMenuClient::fontSelector() const
{
    return m_textField->document()->styleSelector()->fontSelector();
}

HostWindow* AutoFillPopupMenuClient::hostWindow() const
{
    return m_textField->document()->view()->hostWindow();
}

PassRefPtr<Scrollbar> AutoFillPopupMenuClient::createScrollbar(
    ScrollbarClient* client,
    ScrollbarOrientation orientation,
    ScrollbarControlSize size)
{
    return Scrollbar::createNativeScrollbar(client, orientation, size);
}

void AutoFillPopupMenuClient::initialize(
    HTMLInputElement* textField,
    const WebVector<WebString>& names,
    const WebVector<WebString>& labels,
    const WebVector<WebString>& icons,
    const WebVector<int>& uniqueIDs,
    int separatorIndex)
{
    ASSERT(names.size() == labels.size());
    ASSERT(names.size() == icons.size());
    ASSERT(names.size() == uniqueIDs.size());
    ASSERT(separatorIndex < static_cast<int>(names.size()));

    m_selectedIndex = -1;
    m_textField = textField;

    // The suggestions must be set before initializing the
    // AutoFillPopupMenuClient.
    setSuggestions(names, labels, icons, uniqueIDs, separatorIndex);

    FontDescription fontDescription;
    RenderTheme::defaultTheme()->systemFont(CSSValueWebkitControl,
                                            fontDescription);
    RenderStyle* style = m_textField->computedStyle();
    fontDescription.setComputedSize(style->fontDescription().computedSize());

    Font font(fontDescription, 0, 0);
    font.update(textField->document()->styleSelector()->fontSelector());
    // The direction of text in popup menu is set the same as the direction of
    // the input element: textField.
    m_style.set(new PopupMenuStyle(Color::black, Color::white, font, true,
                                   Length(WebCore::Fixed),
                                   textField->renderer()->style()->direction()));
}

void AutoFillPopupMenuClient::setSuggestions(const WebVector<WebString>& names,
                                             const WebVector<WebString>& labels,
                                             const WebVector<WebString>& icons,
                                             const WebVector<int>& uniqueIDs,
                                             int separatorIndex)
{
    ASSERT(names.size() == labels.size());
    ASSERT(names.size() == icons.size());
    ASSERT(names.size() == uniqueIDs.size());
    ASSERT(separatorIndex < static_cast<int>(names.size()));

    m_names.clear();
    m_labels.clear();
    m_icons.clear();
    m_uniqueIDs.clear();
    for (size_t i = 0; i < names.size(); ++i) {
        m_names.append(names[i]);
        m_labels.append(labels[i]);
        m_icons.append(icons[i]);
        m_uniqueIDs.append(uniqueIDs[i]);
    }

    m_separatorIndex = separatorIndex;

    // Try to preserve selection if possible.
    if (getSelectedIndex() >= static_cast<int>(names.size()))
        setSelectedIndex(-1);
}

int AutoFillPopupMenuClient::convertListIndexToInternalIndex(unsigned listIndex) const
{
    if (listIndex == static_cast<unsigned>(m_separatorIndex))
        return -1;

    if (m_separatorIndex == -1 || listIndex < static_cast<unsigned>(m_separatorIndex))
        return listIndex;
    return listIndex - 1;
}

WebViewImpl* AutoFillPopupMenuClient::getWebView() const
{
    Frame* frame = m_textField->document()->frame();
    if (!frame)
        return 0;

    Page* page = frame->page();
    if (!page)
        return 0;

    return static_cast<ChromeClientImpl*>(page->chrome()->client())->webView();
}

RenderStyle* AutoFillPopupMenuClient::textFieldStyle() const
{
    RenderStyle* style = m_textField->computedStyle();
    if (!style) {
        // It seems we can only have a 0 style in a TextField if the
        // node is detached, in which case we the popup should not be
        // showing.  Please report this in http://crbug.com/7708 and
        // include the page you were visiting.
        ASSERT_NOT_REACHED();
    }
    return style;
}

} // namespace WebKit
