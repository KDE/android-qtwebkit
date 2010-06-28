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

#ifndef HTMLDocumentParser_h
#define HTMLDocumentParser_h

#include "CachedResourceClient.h"
#include "FragmentScriptingPermission.h"
#include "HTMLInputStream.h"
#include "HTMLScriptRunnerHost.h"
#include "HTMLToken.h"
#include "ScriptableDocumentParser.h"
#include "SegmentedString.h"
#include "Timer.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class Document;
class DocumentFragment;
class HTMLDocument;
class HTMLParserScheduler;
class HTMLTokenizer;
class HTMLScriptRunner;
class HTMLTreeBuilder;
class HTMLPreloadScanner;
class LegacyHTMLTreeBuilder;
class ScriptController;
class ScriptSourceCode;

class HTMLDocumentParser :  public ScriptableDocumentParser, HTMLScriptRunnerHost, CachedResourceClient {
public:
    // FIXME: These constructors should be made private and replaced by create() methods.
    HTMLDocumentParser(HTMLDocument*, bool reportErrors);
    HTMLDocumentParser(DocumentFragment*, FragmentScriptingPermission);
    virtual ~HTMLDocumentParser();

    // Exposed for HTMLParserScheduler
    void resumeParsingAfterYield();

    static void parseDocumentFragment(const String&, DocumentFragment*, FragmentScriptingPermission = FragmentScriptingAllowed);

private:
    // DocumentParser
    virtual void insert(const SegmentedString&);
    virtual void append(const SegmentedString&);
    virtual void finish();
    virtual bool finishWasCalled();
    virtual bool processingData() const;
    virtual void stopParsing();
    virtual bool isWaitingForScripts() const;
    virtual bool isExecutingScript() const;
    virtual void executeScriptsWaitingForStylesheets();
    virtual int lineNumber() const;
    virtual int columnNumber() const;
    // FIXME: HTMLFormControlElement accesses the LegacyHTMLTreeBuilder via this method.
    // Remove this when the LegacyHTMLTreeBuilder is no longer used.
    virtual LegacyHTMLTreeBuilder* htmlTreeBuilder() const;

    // HTMLScriptRunnerHost
    virtual void watchForLoad(CachedResource*);
    virtual void stopWatchingForLoad(CachedResource*);
    virtual bool shouldLoadExternalScriptFromSrc(const AtomicString&);
    virtual HTMLInputStream& inputStream() { return m_input; }

    // CachedResourceClient
    virtual void notifyFinished(CachedResource*);

    void willPumpLexer();
    void didPumpLexer();

    enum SynchronousMode {
        AllowYield,
        ForceSynchronous,
    };
    void pumpTokenizer(SynchronousMode);
    void pumpTokenizerIfPossible(SynchronousMode);

    bool runScriptsForPausedTreeBuilder();
    void resumeParsingAfterScriptExecution();

    void begin();
    void attemptToEnd();
    void endIfDelayed();
    void end();

    bool isScheduledForResume() const;
    bool inScriptExecution() const;
    bool inWrite() const { return m_writeNestingLevel > 0; }
    bool shouldDelayEnd() const { return inWrite() || isWaitingForScripts() || inScriptExecution() || isScheduledForResume(); }

    ScriptController* script() const;

    HTMLInputStream m_input;

    // We hold m_token here because it might be partially complete.
    HTMLToken m_token;

    OwnPtr<HTMLTokenizer> m_tokenizer;
    OwnPtr<HTMLScriptRunner> m_scriptRunner;
    OwnPtr<HTMLTreeBuilder> m_treeBuilder;
    OwnPtr<HTMLPreloadScanner> m_preloadScanner;
    OwnPtr<HTMLParserScheduler> m_parserScheduler;

    bool m_endWasDelayed;
    int m_writeNestingLevel;
};

}

#endif
