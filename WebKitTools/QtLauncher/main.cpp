/*
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Simon Hausmann <hausmann@kde.org>
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

#include <wtf/Platform.h>

#include <kapplication.h>
#include <kcmdlineargs.h>
#include <kdebug.h>

#include <Document.h>

#include <FrameQt.h>
#include <page/Page.h>

#include <QVBoxLayout>
#include <QDir>

using namespace WebCore;

static KCmdLineOptions options[] =
{
    { "+file",        "File to load", 0 },
    KCmdLineLastOption
};

int main(int argc, char **argv)
{
    KCmdLineArgs::init(argc, argv, "testunity", "testunity",
                       "unity testcase app", "0.1");
    KCmdLineArgs::addCmdLineOptions(options);
    KApplication app;
    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
     
    QWidget topLevel;
    QBoxLayout *l = new QVBoxLayout(&topLevel);
        
    FrameQt *f = new FrameQt(&topLevel);

    l->addWidget(f->view()->qwidget());
    l->activate();
    f->view()->qwidget()->show();

    topLevel.show();

    QString url;

    if (args->count() == 0)
        url = QString("%1/%2").arg(QDir::homePath()).arg(QLatin1String("index.html"));
    else
        url = args->arg(0);

    f->openURL(KURL(url.toLatin1()));
    
    app.exec();
    delete f;
    return 0;
}
