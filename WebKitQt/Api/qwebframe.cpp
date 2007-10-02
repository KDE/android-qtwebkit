/*
    Copyright (C) 2007 Trolltech ASA
    Copyright (C) 2007 Staikos Computing Services Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/
#include "qwebframe.h"
#include "qwebpage.h"
#include "qwebpage_p.h"
#include "qwebframe_p.h"

#include "FocusController.h"
#include "FrameLoaderClientQt.h"
#include "Frame.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "Page.h"
#include "ResourceRequest.h"
#include "SelectionController.h"
#include "PlatformScrollBar.h"

#include "markup.h"
#include "RenderTreeAsText.h"
#include "Element.h"
#include "Document.h"
#include "DragData.h"
#include "RenderObject.h"
#include "GraphicsContext.h"
#include "PlatformScrollBar.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"

#include "bindings/runtime.h"
#include "bindings/runtime_root.h"
#include "kjs_proxy.h"
#include "kjs_window.h"
#include "kjs_binding.h"
#include "ExecState.h"
#include "object.h"

#include "wtf/HashMap.h"

#include <qdebug.h>
#include <qevent.h>
#include <qpainter.h>

using namespace WebCore;

void QWebFramePrivate::init(QWebFrame *qframe, WebCore::Page *page, QWebFrameData *frameData)
{
    q = qframe;

    frameLoaderClient = new FrameLoaderClientQt();
    frame = new Frame(page, frameData->ownerElement, frameLoaderClient);
    frameLoaderClient->setFrame(qframe, frame.get());

    frameView = new FrameView(frame.get());
    frameView->deref();
    frameView->setQWebFrame(qframe);
    if (!frameData->allowsScrolling)
        frameView->setScrollbarsMode(ScrollbarAlwaysOff);
    if (frameData->marginWidth != -1)
        frameView->setMarginWidth(frameData->marginWidth);
    if (frameData->marginHeight != -1)
        frameView->setMarginHeight(frameData->marginHeight);

    frame->setView(frameView.get());
    frame->init();
    eventHandler = frame->eventHandler();
}

QWebFrame *QWebFramePrivate::parentFrame()
{
    return qobject_cast<QWebFrame*>(q->parent());
}

WebCore::PlatformScrollbar *QWebFramePrivate::horizontalScrollBar() const
{
    Q_ASSERT(frameView);
    return frameView->horizontalScrollBar();
}

WebCore::PlatformScrollbar *QWebFramePrivate::verticalScrollBar() const
{
    Q_ASSERT(frameView);
    return frameView->verticalScrollBar();
}

QWebFrame::QWebFrame(QWebPage *parent, QWebFrameData *frameData)
    : QObject(parent)
    , d(new QWebFramePrivate)
{
    d->page = parent;
    d->init(this, parent->d->page, frameData);

    if (!frameData->url.isEmpty()) {
        ResourceRequest request(frameData->url, frameData->referrer);
        d->frame->loader()->load(request, frameData->name);
    }
}

QWebFrame::QWebFrame(QWebFrame *parent, QWebFrameData *frameData)
    : QObject(parent)
    , d(new QWebFramePrivate)
{
    d->page = parent->d->page;
    d->init(this, parent->d->page->d->page, frameData);
}

QWebFrame::~QWebFrame()
{
    Q_ASSERT(d->frame == 0);
    Q_ASSERT(d->frameView == 0);
    delete d;
}

void QWebFrame::addToJSWindowObject(const QByteArray &name, QObject *object)
{
      KJS::JSLock lock;
      KJS::Window *window = KJS::Window::retrieveWindow(d->frame.get());
      KJS::Bindings::RootObject *root = d->frame->bindingRootObject();
      if (!window) {
          qDebug() << "Warning: couldn't get window object";
          return;
      }

      KJS::JSObject *runtimeObject =
        KJS::Bindings::Instance::createRuntimeObject(KJS::Bindings::Instance::QtLanguage,
                                                     object, root);

      window->put(window->interpreter()->globalExec(), KJS::Identifier(name.constData()), runtimeObject);
}


QString QWebFrame::markup() const
{
    if (!d->frame->document())
        return QString();
    return createMarkup(d->frame->document());
}

QString QWebFrame::innerText() const
{
    if (d->frameView->layoutPending())
        d->frameView->layout();

    Element *documentElement = d->frame->document()->documentElement();
    return documentElement->innerText();
}

QString QWebFrame::renderTreeDump() const
{
    if (d->frameView->layoutPending())
        d->frameView->layout();

    return externalRepresentation(d->frame->renderer());
}

QString QWebFrame::title() const
{
    if (d->frame->document())
        return d->frame->document()->title();
    else return QString();
}

QString QWebFrame::name() const
{
    return d->frame->tree()->name();
}

QWebPage * QWebFrame::page() const
{
    return d->page;
}

QString QWebFrame::selectedText() const
{
    return d->frame->selectedText();
}

QList<QWebFrame*> QWebFrame::childFrames() const
{
    QList<QWebFrame*> rc;
    if (d->frame) {
        FrameTree *tree = d->frame->tree();
        for (Frame *child = tree->firstChild(); child; child = child->tree()->nextSibling()) {
            FrameLoader *loader = child->loader();
            FrameLoaderClientQt *client = static_cast<FrameLoaderClientQt*>(loader->client());
            if (client)
                rc.append(client->webFrame());
        }

    }
    return rc;
}


Qt::ScrollBarPolicy QWebFrame::verticalScrollBarPolicy() const
{
    return (Qt::ScrollBarPolicy) d->frameView->vScrollbarMode();
}

void QWebFrame::setVerticalScrollBarPolicy(Qt::ScrollBarPolicy policy)
{
    Q_ASSERT(ScrollbarAuto == Qt::ScrollBarAsNeeded);
    Q_ASSERT(ScrollbarAlwaysOff == Qt::ScrollBarAlwaysOff);
    Q_ASSERT(ScrollbarAlwaysOn == Qt::ScrollBarAlwaysOn);
    d->frameView->setVScrollbarMode((ScrollbarMode)policy);
}

Qt::ScrollBarPolicy QWebFrame::horizontalScrollBarPolicy() const
{
    return (Qt::ScrollBarPolicy) d->frameView->hScrollbarMode();
}

void QWebFrame::setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy policy)
{
    d->frameView->setHScrollbarMode((ScrollbarMode)policy);
}

void QWebFrame::render(QPainter *painter, const QRect &source)
{
    if (!d->frameView || !d->frame->renderer())
        return;

    layout();

    GraphicsContext ctx(painter);
    d->frameView->paint(&ctx, source);
}

void QWebFrame::layout()
{
    if (d->frameView->needsLayout()) {
        d->frameView->layout();
    }

    foreach (QWebFrame *child, childFrames()) {
        child->layout();
    }
}

QPoint QWebFrame::pos() const
{
    Q_ASSERT(d->frameView);
    return d->frameView->frameGeometry().topLeft();
}

QRect QWebFrame::geometry() const
{
    Q_ASSERT(d->frameView);
    return d->frameView->frameGeometry();
}

QString QWebFrame::evaluateJavaScript(const QString& scriptSource)
{
    KJSProxy *proxy = d->frame->scriptProxy();
    QString rc;
    if (proxy) {
        KJS::JSValue *v = proxy->evaluate(String(), 0, scriptSource);
        if (v) {
            rc = String(v->toString(proxy->interpreter()->globalExec()));
        }
    }
    return rc;
}

void QWebFrame::mouseMoveEvent(QMouseEvent *ev)
{
    if (!d->frameView)
        return;

    d->eventHandler->handleMouseMoveEvent(PlatformMouseEvent(ev, 0));
    const int xOffset =
        d->horizontalScrollBar() ? d->horizontalScrollBar()->value() : 0;
    const int yOffset =
        d->verticalScrollBar() ? d->verticalScrollBar()->value() : 0;
    IntPoint pt(ev->x() + xOffset, ev->y() + yOffset);
    WebCore::HitTestResult result = d->eventHandler->hitTestResultAtPoint(pt, false);
    WebCore::Element *link = result.URLElement();
    if (link != d->lastHoverElement) {
        d->lastHoverElement = link;
        emit hoveringOverLink(result.absoluteLinkURL().prettyURL(), result.title());
    }
}

void QWebFrame::mousePressEvent(QMouseEvent *ev)
{
    if (!d->eventHandler)
        return;

    if (ev->button() == Qt::RightButton)
        d->eventHandler->sendContextMenuEvent(PlatformMouseEvent(ev, 1));
    else
        d->eventHandler->handleMousePressEvent(PlatformMouseEvent(ev, 1));

    //FIXME need to keep track of subframe focus for key events!
    d->page->setFocus();
}

void QWebFrame::mouseDoubleClickEvent(QMouseEvent *ev)
{
    if (!d->eventHandler)
        return;

    d->eventHandler->handleMousePressEvent(PlatformMouseEvent(ev, 2));

    //FIXME need to keep track of subframe focus for key events!
    d->page->setFocus();
}

void QWebFrame::mouseReleaseEvent(QMouseEvent *ev)
{
    if (!d->frameView)
        return;

    d->eventHandler->handleMouseReleaseEvent(PlatformMouseEvent(ev, 0));

    //FIXME need to keep track of subframe focus for key events!
    d->page->setFocus();
}

void QWebFrame::wheelEvent(QWheelEvent *ev)
{
    PlatformWheelEvent wkEvent(ev);
    bool accepted = false;
    if (d->eventHandler)
        accepted = d->eventHandler->handleWheelEvent(wkEvent);

    ev->setAccepted(accepted);

    //FIXME need to keep track of subframe focus for key events!
    d->page->setFocus();
}

