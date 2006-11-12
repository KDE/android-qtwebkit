/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef Editor_Client_h
#define Editor_Client_h

#include "AbstractShared.h"

namespace WebCore {

class CSSStyleDeclaration;
class Range;
class HTMLElement;

class EditorClient : public AbstractShared {
public:
    virtual bool shouldDeleteRange(Range*) = 0;
    virtual bool shouldShowDeleteInterface(HTMLElement*) = 0;
    
    virtual bool isContinuousSpellCheckingEnabled() = 0;
    virtual bool isGrammarCheckingEnabled() = 0;
    virtual int spellCheckerDocumentTag() = 0;

    virtual bool shouldBeginEditing(Range*) = 0;
    virtual bool shouldEndEditing(Range*) = 0;
//  virtual bool shouldInsertNode(Node*, Range* replacingRange, WebViewInsertAction) = 0;
//  virtual bool shouldInsertText(NSString*, Range *replacingRange, WebViewInsertAction) = 0;
//  virtual bool shouldChangeSelectedRange(Range* fromRange, Range* toRange, NSSelectionAffinity, bool stillSelecting) = 0;
    virtual bool shouldApplyStyle(CSSStyleDeclaration*, Range*) = 0;
//  virtual bool shouldChangeTypingStyle(CSSStyleDeclaration* fromStyle, CSSStyleDeclaration* toStyle) = 0;
//  virtual bool doCommandBySelector(SEL selector) = 0;
//
    virtual void didBeginEditing() = 0;
    virtual void respondToChangedContents() = 0;
    virtual void didEndEditing() = 0;
//  virtual void webViewDidChangeTypingStyle:(NSNotification *)notification = 0;
//  virtual void webViewDidChangeSelection:(NSNotification *)notification = 0;
//  virtual NSUndoManager* undoManagerForWebView:(WebView *)webView = 0;
};

}

#endif // Editor_Client_h
