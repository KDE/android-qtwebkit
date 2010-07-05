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

#ifndef qwkpage_p_h
#define qwkpage_p_h

#include "DrawingAreaProxy.h"
#include "PageClient.h"
#include "qwkpage.h"
#include "WebPageNamespace.h"
#include "WebPageProxy.h"
#include <QBasicTimer>
#include <wtf/RefPtr.h>
#include <QGraphicsView>
#include <QKeyEvent>

class QWKPagePrivate : public WebKit::PageClient {
public:
    QWKPagePrivate(QWKPage*, WKPageNamespaceRef);
    ~QWKPagePrivate();

    static QWKPagePrivate* get(QWKPage* page) { return page->d; }

    void init(const QSize& viewportSize, WebKit::DrawingAreaProxy*);

    virtual void processDidExit() {}
    virtual void processDidRevive() {}
    virtual void takeFocus(bool direction) {}
    virtual void toolTipChanged(const WebCore::String&, const WebCore::String&);

    void paint(QPainter* painter, QRect);

    void keyPressEvent(QKeyEvent*);
    void keyReleaseEvent(QKeyEvent*);
    void mouseMoveEvent(QGraphicsSceneMouseEvent*);
    void mousePressEvent(QGraphicsSceneMouseEvent*);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent*);
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent*);
    void wheelEvent(QGraphicsSceneWheelEvent*);

    void updateAction(QWKPage::WebAction action);
    void updateNavigationActions();
    void updateEditorActions();

    void _q_webActionTriggered(bool checked);

    QAction* actions[QWKPage::WebActionCount];

    QWKPage* q;
    RefPtr<WebKit::WebPageProxy> page;

    QWKPage::CreateNewPageFn createNewPageFn;

    QPoint tripleClick;
    QBasicTimer tripleClickTimer;
};

#endif /* qkpage_p_h */
