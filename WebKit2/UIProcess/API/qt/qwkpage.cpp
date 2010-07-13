/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "qwkpage.h"
#include "qwkpage_p.h"

#include "ClientImpl.h"
#include "LocalizedStrings.h"
#include "WebEventFactoryQt.h"
#include "WKStringQt.h"
#include "WKURLQt.h"
#include <QAction>
#include <QApplication>
#include <QGraphicsSceneMouseEvent>
#include <QStyle>
#include <QtDebug>
#include <WebKit2/WKFrame.h>
#include <WebKit2/WKRetainPtr.h>

using namespace WebKit;
using namespace WebCore;

QWKPagePrivate::QWKPagePrivate(QWKPage* qq, WKPageNamespaceRef namespaceRef)
    : q(qq)
    , createNewPageFn(0)
{
    memset(actions, 0, sizeof(actions));
    page = toWK(namespaceRef)->createWebPage();
    page->setPageClient(this);
}

QWKPagePrivate::~QWKPagePrivate()
{
    page->close();
}

void QWKPagePrivate::init(const QSize& viewportSize, DrawingAreaProxy* proxy)
{
    page->initializeWebPage(IntSize(viewportSize), proxy);
}

void QWKPagePrivate::toolTipChanged(const String&, const String& newTooltip)
{
    emit q->statusBarMessage(QString(newTooltip));
}

void QWKPagePrivate::paint(QPainter* painter, QRect area)
{
    painter->save();

    painter->setBrush(Qt::white);
    painter->drawRect(area);

    if (page->isValid() && page->drawingArea())
        page->drawingArea()->paint(IntRect(area), painter);

    painter->restore();
}

void QWKPagePrivate::keyPressEvent(QKeyEvent* ev)
{
    WebKeyboardEvent keyboardEvent = WebEventFactory::createWebKeyboardEvent(ev);
    page->keyEvent(keyboardEvent);
}

void QWKPagePrivate::keyReleaseEvent(QKeyEvent* ev)
{
    WebKeyboardEvent keyboardEvent = WebEventFactory::createWebKeyboardEvent(ev);
    page->keyEvent(keyboardEvent);
}

void QWKPagePrivate::mouseMoveEvent(QGraphicsSceneMouseEvent* ev)
{
    // For some reason mouse press results in mouse hover (which is
    // converted to mouse move for WebKit). We ignore these hover
    // events by comparing lastPos with newPos.
    // NOTE: lastPos from the event always comes empty, so we work
    // around that here.
    static QPointF lastPos = QPointF();
    if (lastPos == ev->pos())
        return;
    lastPos = ev->pos();

    WebMouseEvent mouseEvent = WebEventFactory::createWebMouseEvent(ev, 0);
    page->mouseEvent(mouseEvent);
}

void QWKPagePrivate::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
    if (tripleClickTimer.isActive() && (ev->pos() - tripleClick).manhattanLength() < QApplication::startDragDistance()) {
        WebMouseEvent mouseEvent = WebEventFactory::createWebMouseEvent(ev, 3);
        page->mouseEvent(mouseEvent);
        return;
    }

    WebMouseEvent mouseEvent = WebEventFactory::createWebMouseEvent(ev, 1);
    page->mouseEvent(mouseEvent);
}

void QWKPagePrivate::mouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
{
    WebMouseEvent mouseEvent = WebEventFactory::createWebMouseEvent(ev, 0);
    page->mouseEvent(mouseEvent);
}

void QWKPagePrivate::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* ev)
{
    WebMouseEvent mouseEvent = WebEventFactory::createWebMouseEvent(ev, 2);
    page->mouseEvent(mouseEvent);

    tripleClickTimer.start(QApplication::doubleClickInterval(), q);
    tripleClick = ev->pos().toPoint();
}

void QWKPagePrivate::wheelEvent(QGraphicsSceneWheelEvent* ev)
{
    WebWheelEvent wheelEvent = WebEventFactory::createWebWheelEvent(ev);
    page->wheelEvent(wheelEvent);
}

void QWKPagePrivate::updateAction(QWKPage::WebAction action)
{
#ifdef QT_NO_ACTION
    Q_UNUSED(action)
#else
    QAction* a = actions[action];
    if (!a)
        return;

    RefPtr<WebKit::WebFrameProxy> mainFrame = page->mainFrame();
    if (!mainFrame)
        return;

    bool enabled = a->isEnabled();
    bool checked = a->isChecked();

    switch (action) {
    case QWKPage::Back:
        enabled = page->canGoBack();
        break;
    case QWKPage::Forward:
        enabled = page->canGoForward();
        break;
    case QWKPage::Stop:
        enabled = !(WebFrameProxy::LoadStateFinished == mainFrame->loadState());
        break;
    case QWKPage::Reload:
        enabled = (WebFrameProxy::LoadStateFinished == mainFrame->loadState());
        break;
    default:
        break;
    }

    a->setEnabled(enabled);

    if (a->isCheckable())
        a->setChecked(checked);
#endif // QT_NO_ACTION
}

void QWKPagePrivate::updateNavigationActions()
{
    updateAction(QWKPage::Back);
    updateAction(QWKPage::Forward);
    updateAction(QWKPage::Stop);
    updateAction(QWKPage::Reload);
}

#ifndef QT_NO_ACTION
void QWKPagePrivate::_q_webActionTriggered(bool checked)
{
    QAction* a = qobject_cast<QAction*>(q->sender());
    if (!a)
        return;
    QWKPage::WebAction action = static_cast<QWKPage::WebAction>(a->data().toInt());
    q->triggerAction(action, checked);
}
#endif // QT_NO_ACTION

QWKPage::QWKPage(WKPageNamespaceRef namespaceRef)
    : d(new QWKPagePrivate(this, namespaceRef))
{
    WKPageLoaderClient loadClient = {
        0,      /* version */
        this,   /* clientInfo */
        qt_wk_didStartProvisionalLoadForFrame,
        qt_wk_didReceiveServerRedirectForProvisionalLoadForFrame,
        qt_wk_didFailProvisionalLoadWithErrorForFrame,
        qt_wk_didCommitLoadForFrame,
        qt_wk_didFinishLoadForFrame,
        qt_wk_didFailLoadWithErrorForFrame,
        qt_wk_didReceiveTitleForFrame,
        qt_wk_didFirstLayoutForFrame,
        qt_wk_didFirstVisuallyNonEmptyLayoutForFrame,
        qt_wk_didStartProgress,
        qt_wk_didChangeProgress,
        qt_wk_didFinishProgress,
        qt_wk_didBecomeUnresponsive,
        qt_wk_didBecomeResponsive
    };
    WKPageSetPageLoaderClient(pageRef(), &loadClient);

    WKPageUIClient uiClient = {
        0,      /* version */
        this,   /* clientInfo */
        qt_wk_createNewPage,
        qt_wk_showPage,
        qt_wk_close,
        qt_wk_runJavaScriptAlert,
    };
    WKPageSetPageUIClient(pageRef(), &uiClient);
}

QWKPage::~QWKPage()
{
    delete d;
    WKPageTerminate(pageRef());
}

void QWKPage::timerEvent(QTimerEvent* ev)
{
    int timerId = ev->timerId();
    if (timerId == d->tripleClickTimer.timerId())
        d->tripleClickTimer.stop();
    else
        QObject::timerEvent(ev);
}

WKPageRef QWKPage::pageRef() const
{
    return toRef(d->page.get());
}

void QWKPage::setCreateNewPageFunction(CreateNewPageFn function)
{
    d->createNewPageFn = function;
}

void QWKPage::load(const QUrl& url)
{
    WKRetainPtr<WKURLRef> wkurl(WKURLCreateWithQUrl(url));
    WKPageLoadURL(pageRef(), wkurl.get());
}

void QWKPage::setUrl(const QUrl& url)
{
    load(url);
}

QUrl QWKPage::url() const
{
    WKRetainPtr<WKFrameRef> frame = WKPageGetMainFrame(pageRef());
    if (!frame)
        return QUrl();
    return WKURLCopyQUrl(WKFrameGetURL(frame.get()));
}

QString QWKPage::title() const
{
    return WKStringCopyQString(WKPageGetTitle(pageRef()));
}

void QWKPage::setViewportSize(const QSize& size)
{
    if (d->page->drawingArea())
        d->page->drawingArea()->setSize(IntSize(size));
}

#ifndef QT_NO_ACTION
void QWKPage::triggerAction(WebAction action, bool)
{
    switch (action) {
    case Back:
        d->page->goBack();
        break;
    case Forward:
        d->page->goForward();
        break;
    case Stop:
        d->page->stopLoading();
        break;
    case Reload:
        d->page->reload(/* reloadFromOrigin */ true);
        break;
    default:
        break;
    }
}
#endif // QT_NO_ACTION

#ifndef QT_NO_ACTION
QAction* QWKPage::action(WebAction action) const
{
    if (action == QWKPage::NoWebAction || action >= WebActionCount)
        return 0;

    if (d->actions[action])
        return d->actions[action];

    QString text;
    QIcon icon;
    QStyle* style = qobject_cast<QApplication*>(QCoreApplication::instance())->style();
    bool checkable = false;

    switch (action) {
    case Back:
        text = contextMenuItemTagGoBack();
        icon = style->standardIcon(QStyle::SP_ArrowBack);
        break;
    case Forward:
        text = contextMenuItemTagGoForward();
        icon = style->standardIcon(QStyle::SP_ArrowForward);
        break;
    case Stop:
        text = contextMenuItemTagStop();
        icon = style->standardIcon(QStyle::SP_BrowserStop);
        break;
    case Reload:
        text = contextMenuItemTagReload();
        icon = style->standardIcon(QStyle::SP_BrowserReload);
        break;
    default:
        return 0;
        break;
    }

    if (text.isEmpty())
        return 0;

    QAction* a = new QAction(d->q);
    a->setText(text);
    a->setData(action);
    a->setCheckable(checkable);
    a->setIcon(icon);

    connect(a, SIGNAL(triggered(bool)), this, SLOT(_q_webActionTriggered(bool)));

    d->actions[action] = a;
    d->updateAction(action);
    return a;
}
#endif // QT_NO_ACTION

#include "moc_qwkpage.cpp"
