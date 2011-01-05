/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef EditingStyle_h
#define EditingStyle_h

#include "WritingDirection.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class CSSStyleDeclaration;
class CSSComputedStyleDeclaration;
class CSSMutableStyleDeclaration;
class Node;
class Position;
class RenderStyle;

class EditingStyle : public RefCounted<EditingStyle> {
public:

    enum ShouldPreserveWritingDirection { PreserveWritingDirection, DoNotPreserveWritingDirection };
    static float NoFontDelta;

    static PassRefPtr<EditingStyle> create()
    {
        return adoptRef(new EditingStyle());
    }

    static PassRefPtr<EditingStyle> create(Node* node)
    {
        return adoptRef(new EditingStyle(node));
    }

    static PassRefPtr<EditingStyle> create(const Position& position)
    {
        return adoptRef(new EditingStyle(position));
    }

    static PassRefPtr<EditingStyle> create(const CSSStyleDeclaration* style)
    {
        return adoptRef(new EditingStyle(style));
    }

    ~EditingStyle();

    CSSMutableStyleDeclaration* style() { return m_mutableStyle.get(); }
    bool textDirection(WritingDirection&) const;
    bool isEmpty() const;
    void setStyle(PassRefPtr<CSSMutableStyleDeclaration>);
    void overrideWithStyle(const CSSMutableStyleDeclaration*);
    void clear();
    PassRefPtr<EditingStyle> copy() const;
    PassRefPtr<EditingStyle> extractAndRemoveBlockProperties();
    void removeBlockProperties();
    void removeStyleAddedByNode(Node*);
    void removeStyleConflictingWithStyleOfNode(Node*);
    void removeNonEditingProperties();
    void prepareToApplyAt(const Position&, ShouldPreserveWritingDirection = DoNotPreserveWritingDirection);

    float fontSizeDelta() const { return m_fontSizeDelta; }
    bool hasFontSizeDelta() const { return m_fontSizeDelta != NoFontDelta; }

private:
    EditingStyle();
    EditingStyle(Node*);
    EditingStyle(const Position&);
    EditingStyle(const CSSStyleDeclaration*);
    void init(Node*);
    void removeTextFillAndStrokeColorsIfNeeded(RenderStyle*);
    void replaceFontSizeByKeywordIfPossible(RenderStyle*, CSSComputedStyleDeclaration*);
    void extractFontSizeDelta();

    RefPtr<CSSMutableStyleDeclaration> m_mutableStyle;
    bool m_shouldUseFixedDefaultFontSize;
    float m_fontSizeDelta;
};

PassRefPtr<EditingStyle> editingStyleIncludingTypingStyle(const Position&);
    
} // namespace WebCore

#endif // EditingStyle_h
