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
class Node;

class HTMLTreeBuilder : public Noncopyable {
public:
    static PassOwnPtr<HTMLTreeBuilder> create(HTMLTokenizer* tokenizer, HTMLDocument* document, bool reportErrors)
    {
        return adoptPtr(new HTMLTreeBuilder(tokenizer, document, reportErrors));
    }
    static PassOwnPtr<HTMLTreeBuilder> create(HTMLTokenizer* tokenizer, DocumentFragment* fragment, Element* contextElement, FragmentScriptingPermission scriptingPermission)
    {
        return adoptPtr(new HTMLTreeBuilder(tokenizer, fragment, contextElement, scriptingPermission));
    }
    ~HTMLTreeBuilder();

    void detach();

    void setPaused(bool paused) { m_isPaused = paused; }
    bool isPaused() const { return m_isPaused; }

    // The token really should be passed as a const& since it's never modified.
    void constructTreeFromToken(HTMLToken&);
    // Must be called when parser is paused before calling the parser again.
    PassRefPtr<Element> takeScriptToProcess(int& scriptStartLine);

    // Done, close any open tags, etc.
    void finished();

    static HTMLTokenizer::State adjustedLexerState(HTMLTokenizer::State, const AtomicString& tagName, Frame*);

    static bool scriptEnabled(Frame*);
    static bool pluginsEnabled(Frame*);

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

    HTMLTreeBuilder(HTMLTokenizer*, HTMLDocument*, bool reportErrors);
    HTMLTreeBuilder(HTMLTokenizer*, DocumentFragment*, Element* contextElement, FragmentScriptingPermission);

    bool isParsingFragment() const { return !!m_fragmentContext.fragment(); }

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
    void processFakePEndTagIfPInButtonScope();

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

    class FragmentParsingContext : public Noncopyable {
    public:
        FragmentParsingContext();
        FragmentParsingContext(DocumentFragment*, Element* contextElement, FragmentScriptingPermission);
        ~FragmentParsingContext();

        Document* document() const;
        DocumentFragment* fragment() const { return m_fragment; }
        Element* contextElement() const { ASSERT(m_fragment); return m_contextElement; }
        FragmentScriptingPermission scriptingPermission() const { ASSERT(m_fragment); return m_scriptingPermission; }

        void finished();

    private:
        RefPtr<Document> m_dummyDocumentForFragmentParsing;
        DocumentFragment* m_fragment;
        Element* m_contextElement;

        // FragmentScriptingNotAllowed causes the Parser to remove children
        // from <script> tags (so javascript doesn't show up in pastes).
        FragmentScriptingPermission m_scriptingPermission;
    };

    FragmentParsingContext m_fragmentContext;

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

    RefPtr<Element> m_scriptToProcess; // <script> tag which needs processing before resuming the parser.
    int m_scriptToProcessStartLine; // Starting line number of the script tag needing processing.

    // FIXME: We probably want to remove this member.  Originally, it was
    // created to service the legacy tree builder, but it seems to be used for
    // some other things now.
    int m_lastScriptElementStartLine;
};

// FIXME: Move these functions to a more appropriate place.

// Converts the specified string to a floating number.
// If the conversion fails, the return value is false. Take care that leading
// or trailing unnecessary characters make failures.  This returns false for an
// empty string input.
// The double* parameter may be 0.
bool parseToDoubleForNumberType(const String&, double*);
// Converts the specified number to a string. This is an implementation of
// HTML5's "algorithm to convert a number to a string" for NUMBER/RANGE types.
String serializeForNumberType(double);

}

#endif
