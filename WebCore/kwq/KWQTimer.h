/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#ifndef QTIMER_H_
#define QTIMER_H_

#include "KWQObject.h"
#include "KWQSignal.h"

#ifdef __OBJC__
@class NSTimer;
#else
class NSTimer;
#endif

class QTimer : public QObject {
public:
    QTimer();
    ~QTimer() { stop(); }
    
    bool isActive() const;
    void start(int msec, bool singleShot = false);
    void stop();
    void fire();
    
    static void singleShot(int msec, QObject *receiver, const char *member);
    
    // This is just a hack used by KWQKHTMLPart. The monitor function
    // gets called when the timer starts and when it is stopped before firing,
    // but not when the timer fires.
    void setMonitor(void (*monitorFunction)(void *context), void *context);
    NSTimer *getNSTimer() { return m_timer; }

private:    
    NSTimer *m_timer;
    void (*m_monitorFunction)(void *context);
    void *m_monitorFunctionContext;
    KWQSignal m_timeoutSignal;
};

#endif
