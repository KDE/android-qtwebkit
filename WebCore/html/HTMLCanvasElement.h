/*
 * Copyright (C) 2004, 2006, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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

#ifndef HTMLCanvasElement_h
#define HTMLCanvasElement_h

#include "FloatRect.h"
#include "HTMLElement.h"
#include "IntSize.h"

namespace WebCore {

class CanvasContextAttributes;
class CanvasRenderingContext;
class GraphicsContext;
class HTMLCanvasElement;
class ImageBuffer;
class IntSize;

class CanvasObserver {
public:
    virtual ~CanvasObserver() { }

    virtual void canvasChanged(HTMLCanvasElement*, const FloatRect& changedRect) = 0;
    virtual void canvasResized(HTMLCanvasElement*) = 0;
    virtual void canvasDestroyed(HTMLCanvasElement*) = 0;
};

class HTMLCanvasElement : public HTMLElement {
public:
    static PassRefPtr<HTMLCanvasElement> create(Document*);
    static PassRefPtr<HTMLCanvasElement> create(const QualifiedName&, Document*);
    virtual ~HTMLCanvasElement();

    void setObserver(CanvasObserver* observer) { m_observer = observer; }

    // Attributes and functions exposed to script
    int width() const { return size().width(); }
    int height() const { return size().height(); }

    const IntSize& size() const { return m_size; }

    void setWidth(int);
    void setHeight(int);

    void setSize(const IntSize& newSize)
    { 
        if (newSize == size())
            return;
        m_ignoreReset = true; 
        setWidth(newSize.width());
        setHeight(newSize.height());
        m_ignoreReset = false;
        reset();
    }

    CanvasRenderingContext* getContext(const String&, CanvasContextAttributes* attributes = 0);

    String toDataURL(const String& mimeType, const double* quality, ExceptionCode&);
    String toDataURL(const String& mimeType, ExceptionCode& ec) { return toDataURL(mimeType, 0, ec); }

    // Used for rendering
    void willDraw(const FloatRect&);

    void paint(GraphicsContext*, const IntRect&);

    GraphicsContext* drawingContext() const;

    CanvasRenderingContext* renderingContext() const { return m_context.get(); }

    ImageBuffer* buffer() const;

    IntRect convertLogicalToDevice(const FloatRect&) const;
    IntSize convertLogicalToDevice(const FloatSize&) const;
    IntPoint convertLogicalToDevice(const FloatPoint&) const;

    const SecurityOrigin& securityOrigin() const;
    void setOriginTainted() { m_originClean = false; }
    bool originClean() const { return m_originClean; }

    CSSStyleSelector* styleSelector();

    AffineTransform baseTransform() const;

#if ENABLE(3D_CANVAS)    
    bool is3D() const;
#endif

    void makeRenderingResultsAvailable();

private:
    HTMLCanvasElement(const QualifiedName&, Document*);

#if ENABLE(DASHBOARD_SUPPORT)
    virtual HTMLTagStatus endTagRequirement() const;
    virtual int tagPriority() const;
#endif

    virtual void parseMappedAttribute(Attribute*);
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);

    virtual void recalcStyle(StyleChange);

    void reset();

    void createImageBuffer() const;

    void setSurfaceSize(const IntSize&);
    bool hasCreatedImageBuffer() const { return m_hasCreatedImageBuffer; }

    CanvasObserver* m_observer;

    IntSize m_size;

    OwnPtr<CanvasRenderingContext> m_context;

    bool m_rendererIsCanvas;

    bool m_ignoreReset;
    FloatRect m_dirtyRect;

    float m_pageScaleFactor;
    bool m_originClean;

    // m_createdImageBuffer means we tried to malloc the buffer.  We didn't necessarily get it.
    mutable bool m_hasCreatedImageBuffer;
    mutable OwnPtr<ImageBuffer> m_imageBuffer;
};

} //namespace

#endif
