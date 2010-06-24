/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RawDataDocumentParser_h
#define RawDataDocumentParser_h

#include "DocumentParser.h"

namespace WebCore {

// FIXME: It seems wrong that RawDataDocumentParser is a subclass of
// DocumentParser.  RawDataDocumentParser, just wants to override an earlier
// version of write() before the data is decoded.  Seems the decoding could
// move into the base-class DocumentParser, and then RawDataDocumentParser
// would just be short-circuting.  That could simplify some of the
// DocumentWriter logic.
class RawDataDocumentParser : public DocumentParser {
public:
    RawDataDocumentParser(Document* document)
        : DocumentParser(document)
    {
    }

protected:
    virtual void finish()
    {
        if (!m_parserStopped)
            m_document->finishedParsing();
    }

private:
    virtual void write(const SegmentedString&, bool)
    {
        // <https://bugs.webkit.org/show_bug.cgi?id=25397>: JS code can always call document.write, we need to handle it.
        ASSERT_NOT_REACHED();
    }

    virtual bool finishWasCalled()
    {
        // finish() always calls document()->finishedParsing() so we will be
        // deleted after finish().
        return false;
    }

    virtual bool isWaitingForScripts() const { return false; }

    virtual bool wantsRawData() const { return true; }
    virtual bool writeRawData(const char*, int) { return false; }
};

};

#endif // RawDataDocumentParser_h
