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

#ifndef KWQSIGNAL_H
#define KWQSIGNAL_H

#include "KWQSlot.h"
#include "KWQValueList.h"

class KWQSignal {
public:
    KWQSignal(QObject *, const char *name);
    ~KWQSignal();
    
    void connect(const KWQSlot &);
    void disconnect(const KWQSlot &);
    
    void call() const; // should be "emit"; can't be due to define in qobject.h
    void call(bool) const;
    void call(int) const;
    void call(const QString &) const;
    void call(KIO::Job *) const;
    void call(khtml::DocLoader *, khtml::CachedObject *) const;
    void call(KIO::Job *, const char *data, int size) const;
    void call(KIO::Job *, const KURL &) const;
    void call(KIO::Job *, void *) const;
    
private:
    // forbid copying and assignment
    KWQSignal(const KWQSignal &);
    KWQSignal &operator=(const KWQSignal &);
    
    QObject *_object;
    KWQSignal *_next;
    const char *_name;
    QValueList<KWQSlot> _slots;
    
    friend class QObject;
};

#endif
