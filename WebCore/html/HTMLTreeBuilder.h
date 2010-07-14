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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef HTMLTreeBuilder_h
#define HTMLTreeBuilder_h

#include "Element.h"
#include "FragmentScriptingPermission.h"
#include "HTMLConstructionSite.h"
#include "HTMLElementStack.h"
#include "HTMLFormattingElementList.h"
#include "HTMLTokenizer.h"
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/unicode/Unicode.h>

namespace WebCore {

class AtomicHTMLToken;
class Document;
class DocumentFragment;
class Frame;
class HTMLToken;
class HTMLDocument;
class LegacyHTMLTreeBuilder;
class Node;

class HTMLTreeBuilder : public Noncopyable {
public:
    // FIXME: Replace constructors with create() functions returning PassOwnPtrs
    HTMLTreeBuilder(HTMLTokenizer*, HTMLDocument*, bool reportErrors);
    HTMLTreeBuilder(HTMLTokenizer*, DocumentFragment*, FragmentScriptingPermission);
    ~HTMLTreeBuilder();

    void setPaused(bool paused) { m_isPaused = paused; }
    bool isPaused() const { return m_isPaused; }

    // The token really should be passed as a const& since it's never modified.
    void constructTreeFromToken(HTMLToken&);
    // Must be called when parser is paused before calling the parser again.
    PassRefPtr<Element> takeScriptToProcess(int& scriptStartLine);

    // Done, close any open tags, etc.
    void finished();

    static HTMLTokenizer::State adjustedLexerState(HTMLTokenizer::State, const AtomicString& tagName, Frame*);

    // FIXME: This is a dirty, rotten hack to keep HTMLFormControlElement happy
    // until we stop using the legacy parser. DO NOT CALL THIS METHOD.
    LegacyHTMLTreeBuilder* legacyTreeBuilder() const { return m_legacyTreeBuilder.get(); }

private:
    class FakeInsertionMode;
    class ExternalCharacterTokenBuffer;
    // Represents HTML5 "insertion mode"
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/parsing.html#insertion-mode
    enum InsertionMode {
        InitialMode,
        BeforeHTMLMode,
        BeforeHeadMode,
        InHeadMode,
        InHeadNoscriptMode,
        AfterHeadMode,
        InBodyMode,
        TextMode,
        InTableMode,
        InTableTextMode,
        InCaptionMode,
        InColumnGroupMode,
        InTableBodyMode,
        InRowMode,
        InCellMode,
        InSelectMode,
        InSelectInTableMode,
        InForeignContentMode,
        AfterBodyMode,
        InFramesetMode,
        AfterFramesetMode,
        AfterAfterBodyMode,
        AfterAfterFramesetMode,
    };

    void passTokenToLegacyParser(HTMLToken&);

    void processToken(AtomicHTMLToken&);

    void processDoctypeToken(AtomicHTMLToken&);
    void processStartTag(AtomicHTMLToken&);
    void processEndTag(AtomicHTMLToken&);
    void processComment(AtomicHTMLToken&);
    void processCharacter(AtomicHTMLToken&);
    void processEndOfFile(AtomicHTMLToken&);

    bool processStartTagForInHead(AtomicHTMLToken&);
    void processStartTagForInBody(AtomicHTMLToken&);
    void processStartTagForInTable(AtomicHTMLToken&);
    void processEndTagForInBody(AtomicHTMLToken&);
    void processEndTagForInTable(AtomicHTMLToken&);
    void processEndTagForInTableBody(AtomicHTMLToken&);
    void processEndTagForInRow(AtomicHTMLToken&);
    void processEndTagForInCell(AtomicHTMLToken&);

    void processIsindexStartTagForInBody(AtomicHTMLToken&);
    bool processBodyEndTagForInBody(AtomicHTMLToken&);
    bool processTableEndTagForInTable();
    bool processCaptionEndTagForInCaption();
    bool processColgroupEndTagForInColumnGroup();
    bool processTrEndTagForInRow();
    // FIXME: This function should be inlined into its one call site or it
    // needs to assert which tokens it can be called with.
    void processAnyOtherEndTagForInBody(AtomicHTMLToken&);

    void processCharacterBuffer(ExternalCharacterTokenBuffer&);

    void processFakeStartTag(const QualifiedName&, PassRefPtr<NamedNodeMap> attributes = 0);
    void processFakeEndTag(const QualifiedName&);
    void processFakeCharacters(const String&);
    void processFakePEndTagIfPInScope();

    void processGenericRCDATAStartTag(AtomicHTMLToken&);
    void processGenericRawTextStartTag(AtomicHTMLToken&);
    void processScriptStartTag(AtomicHTMLToken&);

    // Default processing for the different insertion modes.
    void defaultForInitial();
    void defaultForBeforeHTML();
    void defaultForBeforeHead();
    void defaultForInHead();
    void defaultForInHeadNoscript();
    void defaultForAfterHead();
    void defaultForInTableText();

    void processUsingSecondaryInsertionModeAndAdjustInsertionMode(AtomicHTMLToken&);

    PassRefPtr<NamedNodeMap> attributesForIsindexInput(AtomicHTMLToken&);

    HTMLElementStack::ElementRecord* furthestBlockForFormattingElement(Element*);
    void reparentChildren(Element* oldParent, Element* newParent);
    void callTheAdoptionAgency(AtomicHTMLToken&);

    void closeTheCell();

    template <bool shouldClose(const Element*)>
    void processCloseWhenNestedTag(AtomicHTMLToken&);

    bool m_framesetOk;

    // FIXME: Implement error reporting.
    void parseError(AtomicHTMLToken&) { }

    void handleScriptStartTag();
    void handleScriptEndTag(Element*, int scriptStartLine);

    InsertionMode insertionMode() const { return m_insertionMode; }
    void setInsertionMode(InsertionMode mode)
    {
        m_insertionMode = mode;
        m_isFakeInsertionMode = false;
    }

    bool isFakeInsertionMode() { return m_isFakeInsertionMode; }
    void setFakeInsertionMode(InsertionMode mode)
    {
        m_insertionMode = mode;
        m_isFakeInsertionMode = true;
    }

    void setSecondaryInsertionMode(InsertionMode);

    void setInsertionModeAndEnd(InsertionMode, bool foreign); // Helper for resetInsertionModeAppropriately
    void resetInsertionModeAppropriately();

    static bool isScriptingFlagEnabled(Frame* frame);

    Document* m_document;
    HTMLConstructionSite m_tree;

    bool m_reportErrors;
    bool m_isPaused;
    bool m_isFakeInsertionMode;

    // FIXME: InsertionModes should be a separate object to prevent direct
    // manipulation of these variables.  For now, be careful to always use
    // setInsertionMode and never set m_insertionMode directly.
    InsertionMode m_insertionMode;
    InsertionMode m_originalInsertionMode;
    InsertionMode m_secondaryInsertionMode;

    // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#pending-table-character-tokens
    Vector<UChar> m_pendingTableCharacters;

    // HTML5 spec requires that we be able to change the state of the tokenizer
    // from within parser actions.
    HTMLTokenizer* m_tokenizer;

    // We're re-using logic from the old LegacyHTMLTreeBuilder while this class is being written.
    OwnPtr<LegacyHTMLTreeBuilder> m_legacyTreeBuilder;

    // These members are intentionally duplicated as the first set is a hack
    // on top of the legacy parser which will eventually be removed.
    RefPtr<Element> m_lastScriptElement; // FIXME: Hack for <script> support on top of the old parser.
    int m_lastScriptElementStartLine; // FIXME: Hack for <script> support on top of the old parser.

    RefPtr<Element> m_scriptToProcess; // <script> tag which needs processing before resuming the parser.
    int m_scriptToProcessStartLine; // Starting line number of the script tag needing processing.

    // FIXME: FragmentScriptingPermission is a HACK for platform/Pasteboard.
    // FragmentScriptingNotAllowed causes the Parser to remove children
    // from <script> tags (so javascript doesn't show up in pastes).
    FragmentScriptingPermission m_fragmentScriptingPermission;
    bool m_isParsingFragment;
};

}

#endif
