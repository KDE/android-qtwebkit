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

#include "config.h"
#include "HTML5Tokenizer.h"

#include "Element.h"
#include "Frame.h"
#include "HTML5Lexer.h"
#include "HTML5ScriptRunner.h"
#include "HTML5TreeBuilder.h"
#include "HTMLDocument.h"
#include "Node.h"
#include "NotImplemented.h"

namespace WebCore {

HTML5Tokenizer::HTML5Tokenizer(HTMLDocument* document, bool reportErrors)
    : Tokenizer()
    , m_document(document)
    , m_lexer(new HTML5Lexer)
    , m_scriptRunner(new HTML5ScriptRunner(document, this))
    , m_treeBuilder(new HTML5TreeBuilder(m_lexer.get(), document, reportErrors))
    , m_wasWaitingOnScriptsDuringFinish(false)
{
    begin();
}

HTML5Tokenizer::~HTML5Tokenizer()
{
}

void HTML5Tokenizer::begin()
{
}

void HTML5Tokenizer::pumpLexer()
{
    ASSERT(!m_treeBuilder->isPaused());
    while (m_lexer->nextToken(m_source, m_token)) {
        m_treeBuilder->constructTreeFromToken(m_token);
        m_token.clear();

        if (!m_treeBuilder->isPaused())
            continue;

        // The parser will pause itself when waiting on a script to load or run.
        // ScriptRunner executes scripts at the right times and handles reentrancy.
        bool shouldContinueParsing = m_scriptRunner->execute(m_treeBuilder->takeScriptToProcess());
        m_treeBuilder->setPaused(!shouldContinueParsing);
        if (!shouldContinueParsing)
            return;
    }
}

void HTML5Tokenizer::write(const SegmentedString& source, bool)
{
    // HTML5Tokenizer::executeScript is responsible for handling saving m_source before re-entry.
    m_source.append(source);
    if (!m_treeBuilder->isPaused())
        pumpLexer();
}

void HTML5Tokenizer::end()
{
    m_source.close();
    if (!m_treeBuilder->isPaused())
        pumpLexer();
    m_treeBuilder->finished();
}

void HTML5Tokenizer::finish()
{
    // finish() indicates we will not receive any more data. If we are waiting on
    // an external script to load, we can't finish parsing quite yet.
    if (isWaitingForScripts()) {
        // FIXME: We might want to use real state enum instead of a bool here.
        m_wasWaitingOnScriptsDuringFinish = true;
        return;
    }
    // We can't call m_source.close() yet as we may have a <script> execution
    // pending which will call document.write().  No more data off the network though.
    end();
}

int HTML5Tokenizer::executingScript() const
{
    return m_scriptRunner->inScriptExecution();
}

bool HTML5Tokenizer::isWaitingForScripts() const
{
    return m_treeBuilder->isPaused();
}

void HTML5Tokenizer::resumeParsingAfterScriptExecution()
{
    ASSERT(!m_scriptRunner->inScriptExecution());
    ASSERT(!m_treeBuilder->isPaused());
    pumpLexer();
    ASSERT(m_treeBuilder->isPaused() || m_source.isEmpty());
    if (m_source.isEmpty() && m_wasWaitingOnScriptsDuringFinish)
        end(); // The document already finished parsing we were just waiting on scripts when finished() was called.
}

void HTML5Tokenizer::watchForLoad(CachedResource* cachedScript)
{
    ASSERT(!cachedScript->isLoaded());
    // addClient would call notifyFinished if the load were complete.
    // Callers do not expect to be re-entered from this call, so they should
    // not an already-loaded CachedResource.
    cachedScript->addClient(this);
}

void HTML5Tokenizer::stopWatchingForLoad(CachedResource* cachedScript)
{
    cachedScript->removeClient(this);
}

void HTML5Tokenizer::executeScript(const ScriptSourceCode& sourceCode)
{
    ASSERT(m_scriptRunner->inScriptExecution());
    if (!m_document->frame())
        return;

    SegmentedString oldInsertionPoint = m_source;
    m_source = SegmentedString();
    m_document->frame()->script()->executeScript(sourceCode);
    // Append oldInsertionPoint onto the new (likely empty) m_source instead of
    // oldInsertionPoint.prepent(m_source) as that would ASSERT if
    // m_source.escaped() (it had characters pushed back onto it).
    m_source.append(oldInsertionPoint);
}

void HTML5Tokenizer::notifyFinished(CachedResource* cachedResource)
{
    ASSERT(!m_scriptRunner->inScriptExecution());
    ASSERT(m_treeBuilder->isPaused());
    bool shouldContinueParsing = m_scriptRunner->executeScriptsWaitingForLoad(cachedResource);
    m_treeBuilder->setPaused(!shouldContinueParsing);
    if (shouldContinueParsing)
        resumeParsingAfterScriptExecution();
}

void HTML5Tokenizer::executeScriptsWaitingForStylesheets()
{
    // Ignore calls unless we have a script blocking the parser waiting on a
    // stylesheet load.  Otherwise we are currently parsing and this
    // is a re-entrant call from encountering a </ style> tag.
    if (!m_scriptRunner->hasScriptsWaitingForStylesheets())
        return;
    ASSERT(!m_scriptRunner->inScriptExecution());
    ASSERT(m_treeBuilder->isPaused());
    bool shouldContinueParsing = m_scriptRunner->executeScriptsWaitingForStylesheets();
    m_treeBuilder->setPaused(!shouldContinueParsing);
    if (shouldContinueParsing)
        resumeParsingAfterScriptExecution();
}

}
