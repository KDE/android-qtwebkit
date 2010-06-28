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
 
#ifndef ScriptableDocumentParser_h
#define ScriptableDocumentParser_h

#include "DecodedDataDocumentParser.h"

namespace WebCore {

class SegmentedString;
class XSSAuditor;

class ScriptableDocumentParser : public DecodedDataDocumentParser {
public:
    // Only used by Document::open for deciding if its safe to act on a
    // JavaScript document.open() call right now, or it should be ignored.
    virtual bool isExecutingScript() const { return false; }

    // FIXME: Only the HTMLDocumentParser ever blocks script execution on
    // stylesheet load, which is likely a bug in the XMLDocumentParser.
    virtual void executeScriptsWaitingForStylesheets() { }

    virtual bool isWaitingForScripts() const = 0;

    // These are used to expose the current line/column to the scripting system.
    virtual int lineNumber() const = 0;
    virtual int columnNumber() const = 0;

    XSSAuditor* xssAuditor() const { return m_xssAuditor; }
    void setXSSAuditor(XSSAuditor* auditor) { m_xssAuditor = auditor; }

    // Exposed for LegacyHTMLTreeBuilder::reportErrorToConsole
    virtual bool processingContentWrittenByScript() const { return false; }

protected:
    ScriptableDocumentParser(Document*, bool viewSourceMode = false);

private:
    virtual ScriptableDocumentParser* asScriptableDocumentParser() { return this; }

    // The XSSAuditor associated with this document parser.
    XSSAuditor* m_xssAuditor;
};

}

#endif // ScriptableDocumentParser_h
