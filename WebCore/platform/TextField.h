/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#ifndef TextField_h
#define TextField_h

#include "GraphicsTypes.h"
#include "PlatformString.h"
#include "TextDirection.h"
#include "Widget.h"

namespace WebCore {
    class Color;
}

#ifdef __OBJC__
@class WebCoreTextFieldController;
NSControlSize ControlSizeForFont(const WebCore::Font&);
#else
class WebCoreTextFieldController;
#endif

class TextField : public Widget {
public:
    enum Type { Normal, Password, Search };

    TextField(Type);
    ~TextField();

    void setColors(const WebCore::Color& background, const WebCore::Color& foreground);

    void setAlignment(WebCore::HorizontalAlignment);

    void setCursorPosition(int);
    int cursorPosition() const;

    void setEdited(bool);
    bool edited() const;

    void setFont(const WebCore::Font&);
    
    void setMaxLength(int);
    int maxLength() const;

    void setReadOnly(bool);
    bool isReadOnly() const;

    void setText(const WebCore::String&);
    WebCore::String text() const;

    void setWritingDirection(WebCore::TextDirection);
    
    void selectAll();
    bool hasSelectedText() const;
    
    int selectionStart() const;
    WebCore::String selectedText() const;
    void setSelection(int, int);
    
    IntSize sizeForCharacterWidth(int numCharacters) const;
    int baselinePosition(int height) const;
    
    virtual FocusPolicy focusPolicy() const;
    virtual bool checksDescendantsForFocus() const;

    Type type() const { return m_type; }
    
    void setLiveSearch(bool liveSearch);
    void setAutoSaveName(const WebCore::String& name);
    void setMaxResults(int maxResults);
    void setPlaceholderString(const WebCore::String& placeholder);
    void addSearchResult();

private:
    Type m_type;
    WebCoreTextFieldController *m_controller;
};

#ifdef __OBJC__
NSTextAlignment TextAlignment(WebCore::HorizontalAlignment);
#endif

#endif
