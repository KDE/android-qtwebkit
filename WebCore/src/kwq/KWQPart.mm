/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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

#include <qwidget.h>
#include <kwqdebug.h>
#include <part.h>


// class Part ==================================================================

KParts::Part::Part()
{
    _logNotYetImplemented();
}


KParts::Part::~Part()
{
    _logNotYetImplemented();
}


static QWidget *theWidget = new QWidget();
QWidget *KParts::Part::widget()
{
    _logNotYetImplemented();
    return theWidget;
}


void KParts::Part::setWindowCaption(const QString &)
{
    _logNotYetImplemented();
}


// class ReadOnlyPart ==================================================================

KParts::ReadOnlyPart::ReadOnlyPart()
{
    _logNotYetImplemented();
}


KParts::ReadOnlyPart::~ReadOnlyPart()
{
    _logNotYetImplemented();
}


static const KURL emptyURL = KURL();
const KURL &KParts::ReadOnlyPart::url() const
{
    // must override
    _logNeverImplemented();
    return emptyURL;
}
