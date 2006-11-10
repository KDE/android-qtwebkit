/*
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2006 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FrameQt.h"

#include "Element.h"
#include "RenderObject.h"
#include "RenderWidget.h"
#include "RenderLayer.h"
#include "Page.h"
#include "Document.h"
#include "HTMLElement.h"
#include "DOMWindow.h"
#include "EditorClientQt.h"
#include "FrameLoadRequest.h"
#include "FrameLoaderClientQt.h"
#include "DOMImplementation.h"
#include "ResourceHandleInternal.h"
#include "Document.h"
#include "Settings.h"
#include "Plugin.h"
#include "FrameView.h"
#include "FramePrivate.h"
#include "GraphicsContext.h"
#include "HTMLDocument.h"
#include "ResourceHandle.h"
#include "FrameLoader.h"
#include "PlatformMouseEvent.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformWheelEvent.h"
#include "MouseEventWithHitTestResults.h"
#include "SelectionController.h"
#include "kjs_proxy.h"
#include "TypingCommand.h"
#include "JSLock.h"
#include "kjs_window.h"
#include "runtime_root.h"

#include <QScrollArea>

#define notImplemented() do { fprintf(stderr, "FIXME: UNIMPLEMENTED: %s:%d\n", __FILE__, __LINE__); } while(0)

namespace WebCore {

// FIXME: Turned this off to fix buildbot. This function be either deleted or used.
#if 0
static void doScroll(const RenderObject* r, bool isHorizontal, int multiplier)
{
    // FIXME: The scrolling done here should be done in the default handlers
    // of the elements rather than here in the part.
    if (!r)
        return;

    //broken since it calls scroll on scrollbars
    //and we have none now
    //r->scroll(direction, KWQScrollWheel, multiplier);
    if (!r->layer())
        return;

    int x = r->layer()->scrollXOffset();
    int y = r->layer()->scrollYOffset();
    if (isHorizontal)
        x += multiplier;
    else
        y += multiplier;

    r->layer()->scrollToOffset(x, y, true, true);
}
#endif

FrameQt::FrameQt(Page* page, Element* ownerElement,
                 FrameQtClient* frameClient,
                 EditorClient* editorClient)
    : Frame(page, ownerElement, (editorClient ? editorClient : new EditorClientQt()))
    , m_bindingRoot(0)
{
    Settings* settings = new Settings;
    settings->setAutoLoadImages(true);
    settings->setMinFontSize(5);
    settings->setMinLogicalFontSize(5);
    settings->setShouldPrintBackgrounds(true);
    settings->setIsJavaScriptEnabled(true);

    settings->setMediumFixedFontSize(14);
    settings->setMediumFontSize(14);
    settings->setSerifFontName("Times New Roman");
    settings->setSansSerifFontName("Arial");
    settings->setFixedFontName("Courier");
    settings->setStdFontName("Arial");

    setSettings(settings);

    m_client = frameClient;
    m_client->setFrame(this);
    //FIXME: rework once frameloaderclientqt is even close to working
    loader()->setClient(new FrameLoaderClientQt());
}

FrameQt::~FrameQt()
{
    setView(0);
    loader()->clearRecordedFormValues();

    loader()->cancelAndClear();
}

void FrameQt::runJavaScriptAlert(String const& message)
{
    m_client->runJavaScriptAlert(message);
}

bool FrameQt::runJavaScriptConfirm(String const& message)
{
    return m_client->runJavaScriptConfirm(message);
}

bool FrameQt::locationbarVisible()
{
    return m_client->locationbarVisible();
}

bool FrameQt::passWheelEventToChildWidget(Node*)
{
    notImplemented();
    return false;
}

bool FrameQt::passSubframeEventToSubframe(MouseEventWithHitTestResults& mev, Frame*)
{
    notImplemented();
    return false;
}

bool FrameQt::passMouseDownEventToWidget(Widget*)
{
    notImplemented();
    return false;
}

bool FrameQt::isLoadTypeReload()
{
    notImplemented();
    return false;
}

bool FrameQt::menubarVisible()
{
    return m_client->menubarVisible();
}

bool FrameQt::personalbarVisible()
{
    return m_client->personalbarVisible();
}

bool FrameQt::statusbarVisible()
{
    return m_client->statusbarVisible();
}

bool FrameQt::toolbarVisible()
{
    return m_client->toolbarVisible();
}

Range* FrameQt::markedTextRange() const
{
    // FIXME: Handle selections.
    return 0;
}

String FrameQt::mimeTypeForFileName(const String&) const
{
    notImplemented();
    return String();
}

void FrameQt::markMisspellingsInAdjacentWords(const VisiblePosition&)
{
    notImplemented();
}

void FrameQt::markMisspellings(const Selection&)
{
    notImplemented();
}

bool FrameQt::lastEventIsMouseUp() const
{
    // no-op
    return false;
}

void FrameQt::saveDocumentState()
{
    // FIXME: Implement this as soon a KPart is created...
}

void FrameQt::restoreDocumentState()
{
    // FIXME: Implement this as soon a KPart is created...
}

void FrameQt::scheduleClose()
{
    // no-op
}

void FrameQt::unfocusWindow()
{
    if (!view())
        return;

    if (!tree()->parent())
        view()->qwidget()->clearFocus();
}

void FrameQt::focusWindow()
{
    if (!view())
        return;

    if (!tree()->parent())
        view()->qwidget()->setFocus();
}

void FrameQt::addMessageToConsole(const String& message, unsigned lineNumber, const String& sourceID)
{
    qDebug("[FrameQt::addMessageToConsole] message=%s lineNumber=%d sourceID=%s",
           qPrintable(QString(message)), lineNumber, qPrintable(QString(sourceID)));
}

bool FrameQt::runJavaScriptPrompt(const String& message, const String& defaultValue, String& result)
{
    return m_client->runJavaScriptPrompt(message, defaultValue, result);
}

KJS::Bindings::Instance* FrameQt::getEmbedInstanceForWidget(Widget*)
{
    notImplemented();
    return 0;
}

KJS::Bindings::Instance* FrameQt::getObjectInstanceForWidget(Widget*)
{
    notImplemented();
    return 0;
}

KJS::Bindings::Instance* FrameQt::getAppletInstanceForWidget(Widget*)
{
    notImplemented();
    return 0;
}

KJS::Bindings::RootObject* FrameQt::bindingRootObject()
{
    ASSERT(javaScriptEnabled());

    if (!m_bindingRoot) {
        KJS::JSLock lock;
        m_bindingRoot = new KJS::Bindings::RootObject(0); // The root gets deleted by JavaScriptCore.

        KJS::JSObject* win = KJS::Window::retrieveWindow(this);
        m_bindingRoot->setRootObjectImp(win);
        m_bindingRoot->setInterpreter(scriptProxy()->interpreter());
        addPluginRootObject(m_bindingRoot);
    }

    return m_bindingRoot;
}

void FrameQt::addPluginRootObject(KJS::Bindings::RootObject* root)
{
    m_rootObjects.append(root);
}

void FrameQt::registerCommandForUndo(PassRefPtr<EditCommand>)
{
    // FIXME: Implement this in the FrameQtClient, to be used within WebKitPart.
}

void FrameQt::registerCommandForRedo(PassRefPtr<EditCommand>)
{
    // FIXME: Implement this in the FrameQtClient, to be used within WebKitPart.
}

void FrameQt::clearUndoRedoOperations()
{
    // FIXME: Implement this in the FrameQtClient, to be used within WebKitPart.
}

void FrameQt::issueUndoCommand()
{
    notImplemented();
}

void FrameQt::issueRedoCommand()
{
    notImplemented();
}

void FrameQt::issueCutCommand()
{
    notImplemented();
}

void FrameQt::issueCopyCommand()
{
    notImplemented();
}

void FrameQt::issuePasteCommand()
{
    notImplemented();
}

void FrameQt::issuePasteAndMatchStyleCommand()
{
    notImplemented();
}

void FrameQt::issueTransposeCommand()
{
    notImplemented();
}

void FrameQt::respondToChangedSelection(const Selection& oldSelection, bool closeTyping)
{
    // TODO: If we want continous spell checking, we need to implement this.
}

void FrameQt::respondToChangedContents(const Selection& endingSelection)
{
    // TODO: If we want accessibility, we need to implement this.
}

bool FrameQt::shouldChangeSelection(const Selection& oldSelection, const Selection& newSelection, EAffinity affinity, bool stillSelecting) const
{
    // no-op
    return true;
}

bool FrameQt::canPaste() const
{
    // FIXME: Implement this in the FrameQtClient, to be used within WebKitPart.
    notImplemented();
    return false;
}

bool FrameQt::canRedo() const
{
    // FIXME: Implement this in the FrameQtClient, to be used within WebKitPart.
    notImplemented();
    return false;
}

bool FrameQt::canUndo() const
{
    // FIXME: Implement this in the FrameQtClient, to be used within WebKitPart.
    notImplemented();
    return false;
}

void FrameQt::print()
{
    notImplemented();
}

bool FrameQt::shouldInterruptJavaScript()
{
    notImplemented();
    return false;
}

bool FrameQt::keyEvent(const PlatformKeyboardEvent& keyEvent)
{
    bool result;

    // Check for cases where we are too early for events -- possible unmatched key up
    // from pressing return in the location bar.
    Document* doc = document();
    if (!doc)
        return false;

    Node* node = doc->focusNode();
    if (!node) {
        if (doc->isHTMLDocument())
            node = doc->body();
        else
            node = doc->documentElement();

        if (!node)
            return false;
    }

    if (!keyEvent.isKeyUp())
        loader()->resetMultipleFormSubmissionProtection();

    result = !EventTargetNodeCast(node)->dispatchKeyEvent(keyEvent);

    // FIXME: FrameMac has a keyDown/keyPress hack here which we are not copying.
    return result;
}

void FrameQt::setFrameGeometry(const IntRect& r)
{
    setFrameGeometry(QRect(r));
}

FrameQtClient* FrameQt::client() const
{
    return m_client;
}

void FrameQt::createNewWindow(const FrameLoadRequest& request, const WindowFeatures& args, Frame*& frame)
{
    notImplemented();
}

void FrameQt::goBackOrForward(int)
{
    notImplemented();
}

KURL FrameQt::historyURL(int distance)
{
    notImplemented();
    return KURL();
}

}

// vim: ts=4 sw=4 et
