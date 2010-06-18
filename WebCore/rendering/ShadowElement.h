/*
 * Copyright (C) 2008, 2009, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ShadowElement_h
#define ShadowElement_h

#include "HTMLDivElement.h"
#include "HTMLInputElement.h"

namespace WebCore {

template<class BaseElement>
class ShadowElement : public BaseElement {
protected:
    ShadowElement(const QualifiedName& name, Node* shadowParent)
        : BaseElement(name, shadowParent->document())
        , m_shadowParent(shadowParent)
    {
    }

    Node* shadowParent() const { return m_shadowParent; }

private:
    virtual bool isShadowNode() const { return true; }
    virtual Node* shadowParentNode() { return m_shadowParent; }

    Node* m_shadowParent;
};

class ShadowBlockElement : public ShadowElement<HTMLDivElement> {
public:
    static PassRefPtr<ShadowBlockElement> create(Node*);
    static PassRefPtr<ShadowBlockElement> createForPart(Node*, PseudoId);
    static bool partShouldHaveStyle(const RenderObject* parentRenderer, PseudoId pseudoId);
    void layoutAsPart(const IntRect& partRect);
    void updateStyleForPart(PseudoId);

protected:
    ShadowBlockElement(Node*);

private:
    static PassRefPtr<RenderStyle> createStyleForPart(RenderObject*, PseudoId);
};

class ShadowInputElement : public ShadowElement<HTMLInputElement> {
public:
    static PassRefPtr<ShadowInputElement> create(Node*);
protected:
    ShadowInputElement(Node*);
};

} // namespace WebCore

#endif // ShadowElement_h
