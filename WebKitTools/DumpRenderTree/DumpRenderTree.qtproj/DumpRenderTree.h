/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DUMPRENDERTREE_H
#define DUMPRENDERTREE_H

#include <QList>
#include <QObject>
#include <QTextStream>
#include <QSocketNotifier>
class QUrl;
class QFile;
class QWebPage;
class QWebFrame;

class LayoutTestController;
class EventSender;

namespace WebCore {

class DumpRenderTree : public QObject {
Q_OBJECT

public:
    DumpRenderTree();
    ~DumpRenderTree();

    // Initialize in multi-file mode, used by run-webkit-tests.
    void open();

    // Initialize in single-file mode.
    void open(const QUrl& url);

    void resetJSObjects();
   
public Q_SLOTS:
    void initJSObjects();
    void readStdin(int);
    void maybeDump(bool);
    void dump();

Q_SIGNALS:
    void quit();

private:
    QWebPage *page;
    QWebFrame *frame;
    
    LayoutTestController *m_controller;
    EventSender *m_eventSender;

    QFile *m_stdin;
    QSocketNotifier* m_notifier;

    bool m_loading;
};

}

#endif
