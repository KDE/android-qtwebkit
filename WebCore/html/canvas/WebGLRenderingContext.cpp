/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#include "config.h"

#if ENABLE(3D_CANVAS)

#include "WebGLRenderingContext.h"

#include "CheckedInt.h"
#include "CanvasPixelArray.h"
#include "Console.h"
#include "DOMWindow.h"
#include "FrameView.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "NotImplemented.h"
#include "RenderBox.h"
#include "RenderLayer.h"
#include "WebGLActiveInfo.h"
#include "Uint16Array.h"
#include "WebGLBuffer.h"
#include "WebGLContextAttributes.h"
#include "WebGLFramebuffer.h"
#include "WebGLProgram.h"
#include "WebGLRenderbuffer.h"
#include "WebGLTexture.h"
#include "WebGLShader.h"
#include "WebGLUniformLocation.h"

#include <wtf/ByteArray.h>
#include <wtf/OwnArrayPtr.h>

namespace WebCore {

class WebGLStateRestorer {
public:
    WebGLStateRestorer(WebGLRenderingContext* context,
                       bool changed)
        : m_context(context)
        , m_changed(changed)
    {
    }

    ~WebGLStateRestorer()
    {
        m_context->cleanupAfterGraphicsCall(m_changed);
    }

private:
    WebGLRenderingContext* m_context;
    bool m_changed;
};

PassOwnPtr<WebGLRenderingContext> WebGLRenderingContext::create(HTMLCanvasElement* canvas, WebGLContextAttributes* attrs)
{
    HostWindow* hostWindow = canvas->document()->view()->root()->hostWindow();
    OwnPtr<GraphicsContext3D> context(GraphicsContext3D::create(attrs->attributes(), hostWindow));

    if (!context)
        return 0;
        
    return new WebGLRenderingContext(canvas, context.release());
}

WebGLRenderingContext::WebGLRenderingContext(HTMLCanvasElement* passedCanvas, PassOwnPtr<GraphicsContext3D> context)
    : CanvasRenderingContext(passedCanvas)
    , m_context(context)
    , m_needsUpdate(true)
    , m_markedCanvasDirty(false)
    , m_activeTextureUnit(0)
    , m_packAlignment(4)
    , m_unpackAlignment(4)
    , m_unpackFlipY(false)
    , m_unpackPremultiplyAlpha(false)
{
    ASSERT(m_context);

    int numCombinedTextureImageUnits = 0;
    m_context->getIntegerv(GraphicsContext3D::MAX_COMBINED_TEXTURE_IMAGE_UNITS, &numCombinedTextureImageUnits);
    m_textureUnits.resize(numCombinedTextureImageUnits);

    int numVertexAttribs = 0;
    m_context->getIntegerv(GraphicsContext3D::MAX_VERTEX_ATTRIBS, &numVertexAttribs);
    m_maxVertexAttribs = numVertexAttribs;

    int implementationColorReadFormat = GraphicsContext3D::RGBA;
    m_context->getIntegerv(GraphicsContext3D::IMPLEMENTATION_COLOR_READ_FORMAT, &implementationColorReadFormat);
    m_implementationColorReadFormat = implementationColorReadFormat;
    int implementationColorReadType = GraphicsContext3D::UNSIGNED_BYTE;
    m_context->getIntegerv(GraphicsContext3D::IMPLEMENTATION_COLOR_READ_TYPE, &implementationColorReadType);
    m_implementationColorReadType = implementationColorReadType;

    m_maxTextureSize = 0;
    m_context->getIntegerv(GraphicsContext3D::MAX_TEXTURE_SIZE, &m_maxTextureSize);
    m_maxTextureLevel = WebGLTexture::computeLevelCount(m_maxTextureSize, m_maxTextureSize);
    m_maxCubeMapTextureSize = 0;
    m_context->getIntegerv(GraphicsContext3D::MAX_CUBE_MAP_TEXTURE_SIZE, &m_maxCubeMapTextureSize);
    m_maxCubeMapTextureLevel = WebGLTexture::computeLevelCount(m_maxCubeMapTextureSize, m_maxCubeMapTextureSize);

    if (!isGLES2Compliant()) {
        createFallbackBlackTextures1x1();
        initVertexAttrib0();
    }
    m_context->reshape(canvas()->width(), canvas()->height());
    m_context->viewport(0, 0, canvas()->width(), canvas()->height());
}

WebGLRenderingContext::~WebGLRenderingContext()
{
    detachAndRemoveAllObjects();
}

void WebGLRenderingContext::markContextChanged()
{
#if USE(ACCELERATED_COMPOSITING)
    RenderBox* renderBox = canvas()->renderBox();
    if (renderBox && renderBox->hasLayer() && renderBox->layer()->hasAcceleratedCompositing())
        renderBox->layer()->rendererContentChanged();
#endif
    if (!m_markedCanvasDirty) {
        // Make sure the canvas's image buffer is allocated.
        canvas()->buffer();
        canvas()->willDraw(FloatRect(0, 0, canvas()->width(), canvas()->height()));
        m_markedCanvasDirty = true;
    }
}

bool WebGLRenderingContext::paintRenderingResultsToCanvas()
{
    if (m_markedCanvasDirty) {
        m_markedCanvasDirty = false;
        m_context->paintRenderingResultsToCanvas(this);
        return true;
    }
    return false;
}

void WebGLRenderingContext::beginPaint()
{
    if (m_markedCanvasDirty) {
        m_context->beginPaint(this);
    }
}

void WebGLRenderingContext::endPaint()
{
    if (m_markedCanvasDirty) {
        m_markedCanvasDirty = false;
        m_context->endPaint();
    }
}

void WebGLRenderingContext::reshape(int width, int height)
{
    if (m_needsUpdate) {
#if USE(ACCELERATED_COMPOSITING)
        RenderBox* renderBox = canvas()->renderBox();
        if (renderBox && renderBox->hasLayer())
            renderBox->layer()->rendererContentChanged();
#endif
        m_needsUpdate = false;
    }
    
    m_context->reshape(width, height);
}

int WebGLRenderingContext::sizeInBytes(int type, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    int result = m_context->sizeInBytes(type);
    if (result <= 0)
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);

    return result;
}

void WebGLRenderingContext::activeTexture(unsigned long texture, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (texture - GraphicsContext3D::TEXTURE0 >= m_textureUnits.size()) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return;
    }
    m_activeTextureUnit = texture - GraphicsContext3D::TEXTURE0;
    m_context->activeTexture(texture);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::attachShader(WebGLProgram* program, WebGLShader* shader, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateWebGLObject(program) || !validateWebGLObject(shader))
        return;
    m_context->attachShader(program, shader);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::bindAttribLocation(WebGLProgram* program, unsigned long index, const String& name, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateWebGLObject(program))
        return;
    m_context->bindAttribLocation(program, index, name);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::bindBuffer(unsigned long target, WebGLBuffer* buffer, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (buffer && buffer->context() != this) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return;
    }
    if (buffer && buffer->getTarget() && buffer->getTarget() != target) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return;
    }

    if (target == GraphicsContext3D::ARRAY_BUFFER)
        m_boundArrayBuffer = buffer;
    else if (target == GraphicsContext3D::ELEMENT_ARRAY_BUFFER)
        m_boundElementArrayBuffer = buffer;
    else {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return;
    }

    m_context->bindBuffer(target, buffer);
    if (buffer)
        buffer->setTarget(target);
    cleanupAfterGraphicsCall(false);
}


void WebGLRenderingContext::bindFramebuffer(unsigned long target, WebGLFramebuffer* buffer, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (buffer && buffer->context() != this) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return;
    }
    if (target != GraphicsContext3D::FRAMEBUFFER) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return;
    }
    m_framebufferBinding = buffer;
    m_context->bindFramebuffer(target, buffer);
    if (m_framebufferBinding)
        m_framebufferBinding->onBind();
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::bindRenderbuffer(unsigned long target, WebGLRenderbuffer* renderBuffer, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (renderBuffer && renderBuffer->context() != this) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return;
    }
    if (target != GraphicsContext3D::RENDERBUFFER) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return;
    }
    m_renderbufferBinding = renderBuffer;
    m_context->bindRenderbuffer(target, renderBuffer);
    cleanupAfterGraphicsCall(false);
}


void WebGLRenderingContext::bindTexture(unsigned long target, WebGLTexture* texture, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (texture && texture->context() != this) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return;
    }
    int maxLevel = 0;
    if (target == GraphicsContext3D::TEXTURE_2D) {
        m_textureUnits[m_activeTextureUnit].m_texture2DBinding = texture;
        maxLevel = m_maxTextureLevel;
    } else if (target == GraphicsContext3D::TEXTURE_CUBE_MAP) {
        m_textureUnits[m_activeTextureUnit].m_textureCubeMapBinding = texture;
        maxLevel = m_maxCubeMapTextureLevel;
    } else {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return;
    }
    m_context->bindTexture(target, texture);
    if (!isGLES2Compliant() && texture)
        texture->setTarget(target, maxLevel);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::blendColor(double red, double green, double blue, double alpha)
{
    m_context->blendColor(red, green, blue, alpha);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::blendEquation( unsigned long mode )
{
    if (!isGLES2Compliant()) {
        if (!validateBlendEquation(mode))
            return;
    }
    m_context->blendEquation(mode);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::blendEquationSeparate(unsigned long modeRGB, unsigned long modeAlpha)
{
    if (!isGLES2Compliant()) {
        if (!validateBlendEquation(modeRGB) || !validateBlendEquation(modeAlpha))
            return;
    }
    m_context->blendEquationSeparate(modeRGB, modeAlpha);
    cleanupAfterGraphicsCall(false);
}


void WebGLRenderingContext::blendFunc(unsigned long sfactor, unsigned long dfactor)
{
    m_context->blendFunc(sfactor, dfactor);
    cleanupAfterGraphicsCall(false);
}       

void WebGLRenderingContext::blendFuncSeparate(unsigned long srcRGB, unsigned long dstRGB, unsigned long srcAlpha, unsigned long dstAlpha)
{
    m_context->blendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::bufferData(unsigned long target, int size, unsigned long usage, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    WebGLBuffer* buffer = validateBufferDataParameters(target, usage);
    if (!buffer)
        return;
    if (!buffer->associateBufferData(size)) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }

    m_context->bufferData(target, size, usage);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::bufferData(unsigned long target, ArrayBuffer* data, unsigned long usage, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    WebGLBuffer* buffer = validateBufferDataParameters(target, usage);
    if (!buffer)
        return;
    if (!buffer->associateBufferData(data)) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }

    m_context->bufferData(target, data, usage);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::bufferData(unsigned long target, ArrayBufferView* data, unsigned long usage, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    WebGLBuffer* buffer = validateBufferDataParameters(target, usage);
    if (!buffer)
        return;
    if (!buffer->associateBufferData(data)) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }

    m_context->bufferData(target, data, usage);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::bufferSubData(unsigned long target, long offset, ArrayBuffer* data, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    WebGLBuffer* buffer = validateBufferDataParameters(target, GraphicsContext3D::STATIC_DRAW);
    if (!buffer)
        return;
    if (!buffer->associateBufferSubData(offset, data)) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }

    m_context->bufferSubData(target, offset, data);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::bufferSubData(unsigned long target, long offset, ArrayBufferView* data, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    WebGLBuffer* buffer = validateBufferDataParameters(target, GraphicsContext3D::STATIC_DRAW);
    if (!buffer)
        return;
    if (!buffer->associateBufferSubData(offset, data)) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }

    m_context->bufferSubData(target, offset, data);
    cleanupAfterGraphicsCall(false);
}

unsigned long WebGLRenderingContext::checkFramebufferStatus(unsigned long target)
{
    if (!isGLES2Compliant()) {
        if (target != GraphicsContext3D::FRAMEBUFFER) {
            m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
            return 0;
        }
    }
    if (!m_framebufferBinding || !m_framebufferBinding->object())
        return GraphicsContext3D::FRAMEBUFFER_COMPLETE;
    return m_context->checkFramebufferStatus(target);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::clear(unsigned long mask)
{
    if (!isGLES2Compliant()) {
        if (mask & ~(GraphicsContext3D::COLOR_BUFFER_BIT | GraphicsContext3D::DEPTH_BUFFER_BIT | GraphicsContext3D::STENCIL_BUFFER_BIT)) {
            m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
            return;
        }
    }
    m_context->clear(mask);
    cleanupAfterGraphicsCall(true);
}

void WebGLRenderingContext::clearColor(double r, double g, double b, double a)
{
    if (isnan(r))
        r = 0;
    if (isnan(g))
        g = 0;
    if (isnan(b))
        b = 0;
    if (isnan(a))
        a = 1;
    m_context->clearColor(r, g, b, a);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::clearDepth(double depth)
{
    m_context->clearDepth(depth);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::clearStencil(long s)
{
    m_context->clearStencil(s);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::colorMask(bool red, bool green, bool blue, bool alpha)
{
    m_context->colorMask(red, green, blue, alpha);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::compileShader(WebGLShader* shader, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateWebGLObject(shader))
        return;
    m_context->compileShader(shader);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::copyTexImage2D(unsigned long target, long level, unsigned long internalformat, long x, long y, unsigned long width, unsigned long height, long border)
{
    if (!validateTexFuncParameters(target, level, internalformat, width, height, border, internalformat, GraphicsContext3D::UNSIGNED_BYTE))
        return;
    if (!isGLES2Compliant()) {
        if (m_framebufferBinding && m_framebufferBinding->object()
            && !isTexInternalFormatColorBufferCombinationValid(internalformat,
                                                               m_framebufferBinding->getColorBufferFormat())) {
            m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
            return;
        }
        if (level && WebGLTexture::isNPOT(width, height)) {
            m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
            return;
        }
    }
    m_context->copyTexImage2D(target, level, internalformat, x, y, width, height, border);
    // FIXME: if the framebuffer is not complete, none of the below should be executed.
    WebGLTexture* tex = getTextureBinding(target);
    if (!isGLES2Compliant()) {
        if (tex)
            tex->setLevelInfo(target, level, internalformat, width, height, GraphicsContext3D::UNSIGNED_BYTE);
    }
    if (m_framebufferBinding && tex)
        m_framebufferBinding->onAttachedObjectChange(tex);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::copyTexSubImage2D(unsigned long target, long level, long xoffset, long yoffset, long x, long y, unsigned long width, unsigned long height)
{
    if (!isGLES2Compliant()) {
        WebGLTexture* tex = getTextureBinding(target);
        if (m_framebufferBinding && m_framebufferBinding->object() && tex
            && !isTexInternalFormatColorBufferCombinationValid(tex->getInternalFormat(),
                                                               m_framebufferBinding->getColorBufferFormat())) {
            m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
            return;
        }
    }
    m_context->copyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
    cleanupAfterGraphicsCall(false);
}

PassRefPtr<WebGLBuffer> WebGLRenderingContext::createBuffer()
{
    RefPtr<WebGLBuffer> o = WebGLBuffer::create(this);
    addObject(o.get());
    return o;
}
        
PassRefPtr<WebGLFramebuffer> WebGLRenderingContext::createFramebuffer()
{
    RefPtr<WebGLFramebuffer> o = WebGLFramebuffer::create(this);
    addObject(o.get());
    return o;
}

PassRefPtr<WebGLTexture> WebGLRenderingContext::createTexture()
{
    RefPtr<WebGLTexture> o = WebGLTexture::create(this);
    addObject(o.get());
    return o;
}

PassRefPtr<WebGLProgram> WebGLRenderingContext::createProgram()
{
    RefPtr<WebGLProgram> o = WebGLProgram::create(this);
    addObject(o.get());
    return o;
}

PassRefPtr<WebGLRenderbuffer> WebGLRenderingContext::createRenderbuffer()
{
    RefPtr<WebGLRenderbuffer> o = WebGLRenderbuffer::create(this);
    addObject(o.get());
    return o;
}

PassRefPtr<WebGLShader> WebGLRenderingContext::createShader(unsigned long type, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (type != GraphicsContext3D::VERTEX_SHADER && type != GraphicsContext3D::FRAGMENT_SHADER) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return 0;
    }
    
    RefPtr<WebGLShader> o = WebGLShader::create(this, static_cast<GraphicsContext3D::WebGLEnumType>(type));
    addObject(o.get());
    return o;
}

void WebGLRenderingContext::cullFace(unsigned long mode)
{
    m_context->cullFace(mode);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::deleteBuffer(WebGLBuffer* buffer)
{
    if (!buffer)
        return;
    
    buffer->deleteObject();

    if (!isGLES2Compliant()) {
        VertexAttribState& state = m_vertexAttribState[0];
        if (buffer == state.bufferBinding) {
            state.bufferBinding = m_vertexAttrib0Buffer;
            state.bytesPerElement = 0;
            state.size = 4;
            state.type = GraphicsContext3D::FLOAT;
            state.normalized = false;
            state.stride = 16;
            state.originalStride = 0;
            state.offset = 0;
        }
    }
}

void WebGLRenderingContext::deleteFramebuffer(WebGLFramebuffer* framebuffer)
{
    if (!framebuffer)
        return;
    
    framebuffer->deleteObject();
}

void WebGLRenderingContext::deleteProgram(WebGLProgram* program)
{
    if (!program)
        return;
    
    program->deleteObject();
}

void WebGLRenderingContext::deleteRenderbuffer(WebGLRenderbuffer* renderbuffer)
{
    if (!renderbuffer)
        return;
    
    renderbuffer->deleteObject();
    if (m_framebufferBinding)
        m_framebufferBinding->onAttachedObjectChange(renderbuffer);
}

void WebGLRenderingContext::deleteShader(WebGLShader* shader)
{
    if (!shader)
        return;
    
    shader->deleteObject();
}

void WebGLRenderingContext::deleteTexture(WebGLTexture* texture)
{
    if (!texture)
        return;
    
    texture->deleteObject();
    if (m_framebufferBinding)
        m_framebufferBinding->onAttachedObjectChange(texture);
}

void WebGLRenderingContext::depthFunc(unsigned long func)
{
    m_context->depthFunc(func);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::depthMask(bool flag)
{
    m_context->depthMask(flag);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::depthRange(double zNear, double zFar)
{
    m_context->depthRange(zNear, zFar);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::detachShader(WebGLProgram* program, WebGLShader* shader, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateWebGLObject(program) || !validateWebGLObject(shader))
        return;
    m_context->detachShader(program, shader);
    cleanupAfterGraphicsCall(false);
}


void WebGLRenderingContext::disable(unsigned long cap)
{
    if (!isGLES2Compliant()) {
        if (!validateCapability(cap))
            return;
    }
    m_context->disable(cap);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::disableVertexAttribArray(unsigned long index, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (index >= m_maxVertexAttribs) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }
    
    if (index < m_vertexAttribState.size())
        m_vertexAttribState[index].enabled = false;

    if (index > 0 || isGLES2Compliant()) {
        m_context->disableVertexAttribArray(index);
        cleanupAfterGraphicsCall(false);
    }
}

bool WebGLRenderingContext::validateElementArraySize(unsigned long count, unsigned long type, long offset)
{
    if (!m_boundElementArrayBuffer)
        return false;

    if (offset < 0)
        return false;

    unsigned long uoffset = static_cast<unsigned long>(offset);

    if (type == GraphicsContext3D::UNSIGNED_SHORT) {
        // For an unsigned short array, offset must be divisible by 2 for alignment reasons.
        if (uoffset & 1)
            return false;

        // Make uoffset an element offset.
        uoffset /= 2;

        unsigned long n = m_boundElementArrayBuffer->byteLength() / 2;
        if (uoffset > n || count > n - uoffset)
            return false;
    } else if (type == GraphicsContext3D::UNSIGNED_BYTE) {
        unsigned long n = m_boundElementArrayBuffer->byteLength();
        if (uoffset > n || count > n - uoffset)
            return false;
    }
    return true;
}

bool WebGLRenderingContext::validateIndexArrayConservative(unsigned long type, long& numElementsRequired)
{
    // Performs conservative validation by caching a maximum index of
    // the given type per element array buffer. If all of the bound
    // array buffers have enough elements to satisfy that maximum
    // index, skips the expensive per-draw-call iteration in
    // validateIndexArrayPrecise.

    long maxIndex = m_boundElementArrayBuffer->getCachedMaxIndex(type);
    if (maxIndex < 0) {
        // Compute the maximum index in the entire buffer for the given type of index.
        switch (type) {
        case GraphicsContext3D::UNSIGNED_BYTE: {
            unsigned numElements = m_boundElementArrayBuffer->byteLength();
            if (!numElements)
                maxIndex = 0;
            else {
                const unsigned char* p = static_cast<const unsigned char*>(m_boundElementArrayBuffer->elementArrayBuffer()->data());
                for (unsigned i = 0; i < numElements; i++)
                    maxIndex = max(maxIndex, static_cast<long>(p[i]));
            }
            break;
        }
        case GraphicsContext3D::UNSIGNED_SHORT: {
            unsigned numElements = m_boundElementArrayBuffer->byteLength() / sizeof(unsigned short);
            if (!numElements)
                maxIndex = 0;
            else {
                const unsigned short* p = static_cast<const unsigned short*>(m_boundElementArrayBuffer->elementArrayBuffer()->data());
                for (unsigned i = 0; i < numElements; i++)
                    maxIndex = max(maxIndex, static_cast<long>(p[i]));
            }
            break;
        }
        default:
            return false;
        }
        m_boundElementArrayBuffer->setCachedMaxIndex(type, maxIndex);
    }

    if (maxIndex >= 0) {
        // The number of required elements is one more than the maximum
        // index that will be accessed.
        numElementsRequired = maxIndex + 1;
        return true;
    }

    return false;
}

bool WebGLRenderingContext::validateIndexArrayPrecise(unsigned long count, unsigned long type, long offset, long& numElementsRequired)
{
    long lastIndex = -1;

    if (!m_boundElementArrayBuffer)
        return false;

    unsigned long uoffset = static_cast<unsigned long>(offset);
    unsigned long n = count;

    if (type == GraphicsContext3D::UNSIGNED_SHORT) {
        // Make uoffset an element offset.
        uoffset /= 2;
        const unsigned short* p = static_cast<const unsigned short*>(m_boundElementArrayBuffer->elementArrayBuffer()->data()) + uoffset;
        while (n-- > 0) {
            if (*p > lastIndex)
                lastIndex = *p;
            ++p;
        }
    } else if (type == GraphicsContext3D::UNSIGNED_BYTE) {
        const unsigned char* p = static_cast<const unsigned char*>(m_boundElementArrayBuffer->elementArrayBuffer()->data()) + uoffset;
        while (n-- > 0) {
            if (*p > lastIndex)
                lastIndex = *p;
            ++p;
        }
    }    
        
    // Then set the last index in the index array and make sure it is valid.
    numElementsRequired = lastIndex + 1;
    return numElementsRequired > 0;
}

bool WebGLRenderingContext::validateRenderingState(long numElementsRequired)
{
    if (!m_currentProgram)
        return false;

    int numAttribStates = static_cast<int>(m_vertexAttribState.size());

    // Look in each enabled vertex attrib and check if they've been bound to a buffer.
    for (int i = 0; i < numAttribStates; ++i) {
        if (m_vertexAttribState[i].enabled
            && (!m_vertexAttribState[i].bufferBinding || !m_vertexAttribState[i].bufferBinding->object()))
            return false;
    }

    // Look in each consumed vertex attrib (by the current program) and find the smallest buffer size
    long smallestNumElements = LONG_MAX;
    int numActiveAttribLocations = m_currentProgram->numActiveAttribLocations();
    for (int i = 0; i < numActiveAttribLocations; ++i) {
        int loc = m_currentProgram->getActiveAttribLocation(i);
        if (loc >=0 && loc < numAttribStates) {
            const VertexAttribState& state = m_vertexAttribState[loc];
            if (state.enabled) {
                // Avoid off-by-one errors in numElements computation.
                // For the last element, we will only touch the data for the
                // element and nothing beyond it.
                long bytesRemaining = state.bufferBinding->byteLength() - state.offset;
                long numElements = 0;
                if (bytesRemaining >= state.bytesPerElement)
                    numElements = 1 + (bytesRemaining - state.bytesPerElement) / state.stride;
                if (numElements < smallestNumElements)
                    smallestNumElements = numElements;
            }
        }
    }
    
    if (smallestNumElements == LONG_MAX)
        smallestNumElements = 0;
    
    return numElementsRequired <= smallestNumElements;
}

bool WebGLRenderingContext::validateWebGLObject(CanvasObject* object)
{
    if (!object) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return false;
    }
    if (object->context() != this) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return false;
    }
    return true;
}

void WebGLRenderingContext::drawArrays(unsigned long mode, long first, long count, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);

    if (!validateDrawMode(mode))
        return;

    if (first < 0 || count < 0) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }

    // Ensure we have a valid rendering state
    CheckedInt<int32_t> checkedFirst(first);
    CheckedInt<int32_t> checkedCount(count);
    CheckedInt<int32_t> checkedSum = checkedFirst + checkedCount;
    if (!checkedSum.valid() || !validateRenderingState(checkedSum.value())) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return;
    }

    bool vertexAttrib0Simulated = false;
    if (!isGLES2Compliant()) {
        vertexAttrib0Simulated = simulateVertexAttrib0(first + count - 1);
        handleNPOTTextures(true);
    }
    m_context->drawArrays(mode, first, count);
    if (!isGLES2Compliant()) {
        handleNPOTTextures(false);
        if (vertexAttrib0Simulated)
            restoreStatesAfterVertexAttrib0Simulation();
    }
    cleanupAfterGraphicsCall(true);
}

void WebGLRenderingContext::drawElements(unsigned long mode, long count, unsigned long type, long offset, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);

    if (!validateDrawMode(mode))
        return;

    switch (type) {
    case GraphicsContext3D::UNSIGNED_BYTE:
    case GraphicsContext3D::UNSIGNED_SHORT:
        break;
    default:
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return;
    }

    if (count < 0 || offset < 0) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }

    // Ensure we have a valid rendering state
    long numElements;
    
    if (!validateElementArraySize(count, type, offset)) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return;
    }

    if (!validateIndexArrayConservative(type, numElements) || !validateRenderingState(numElements))
        if (!validateIndexArrayPrecise(count, type, offset, numElements) || !validateRenderingState(numElements)) {
            m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
            return;
        }

    bool vertexAttrib0Simulated = false;
    if (!isGLES2Compliant()) {
        vertexAttrib0Simulated = simulateVertexAttrib0(numElements);
        handleNPOTTextures(true);
    }
    m_context->drawElements(mode, count, type, offset);
    if (!isGLES2Compliant()) {
        handleNPOTTextures(false);
        if (vertexAttrib0Simulated)
            restoreStatesAfterVertexAttrib0Simulation();
    }
    cleanupAfterGraphicsCall(true);
}

void WebGLRenderingContext::enable(unsigned long cap)
{
    if (!isGLES2Compliant()) {
        if (!validateCapability(cap))
            return;
    }
    m_context->enable(cap);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::enableVertexAttribArray(unsigned long index, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (index >= m_maxVertexAttribs) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }
    
    if (index >= m_vertexAttribState.size())
        m_vertexAttribState.resize(index + 1);
        
    m_vertexAttribState[index].enabled = true;
    
    m_context->enableVertexAttribArray(index);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::finish()
{
    m_context->finish();
    cleanupAfterGraphicsCall(true);
}


void WebGLRenderingContext::flush()
{
    m_context->flush();
    cleanupAfterGraphicsCall(true);
}

void WebGLRenderingContext::framebufferRenderbuffer(unsigned long target, unsigned long attachment, unsigned long renderbuffertarget, WebGLRenderbuffer* buffer, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateFramebufferFuncParameters(target, attachment))
        return;
    if (renderbuffertarget != GraphicsContext3D::RENDERBUFFER) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return;
    }
    if (buffer && buffer->context() != this) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return;
    }
    // Don't allow the default framebuffer to be mutated; all current
    // implementations use an FBO internally in place of the default
    // FBO.
    if (!m_framebufferBinding || !m_framebufferBinding->object()) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return;
    }
    if (buffer && buffer->object()) {
        bool isConflicted = false;
        switch (attachment) {
        case GraphicsContext3D::DEPTH_ATTACHMENT:
            if (m_framebufferBinding->isDepthStencilAttached() || m_framebufferBinding->isStencilAttached())
                isConflicted = true;
            if (buffer->getInternalFormat() != GraphicsContext3D::DEPTH_COMPONENT16)
                isConflicted = true;
            break;
        case GraphicsContext3D::STENCIL_ATTACHMENT:
            if (m_framebufferBinding->isDepthStencilAttached() || m_framebufferBinding->isDepthAttached())
                isConflicted = true;
            if (buffer->getInternalFormat() != GraphicsContext3D::STENCIL_INDEX8)
                isConflicted = true;
            break;
        case GraphicsContext3D::DEPTH_STENCIL_ATTACHMENT:
            if (m_framebufferBinding->isDepthAttached() || m_framebufferBinding->isStencilAttached())
                isConflicted = true;
            if (buffer->getInternalFormat() != GraphicsContext3D::DEPTH_STENCIL)
                isConflicted = true;
            break;
        }
        if (isConflicted) {
            m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
            return;
        }
    }
    m_context->framebufferRenderbuffer(target, attachment, renderbuffertarget, buffer);
    m_framebufferBinding->setAttachment(attachment, buffer);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::framebufferTexture2D(unsigned long target, unsigned long attachment, unsigned long textarget, WebGLTexture* texture, long level, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateFramebufferFuncParameters(target, attachment))
        return;
    if (level) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }
    if (texture && texture->context() != this) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return;
    }
    // Don't allow the default framebuffer to be mutated; all current
    // implementations use an FBO internally in place of the default
    // FBO.
    if (!m_framebufferBinding || !m_framebufferBinding->object()) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return;
    }
    m_context->framebufferTexture2D(target, attachment, textarget, texture, level);
    m_framebufferBinding->setAttachment(attachment, texture);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::frontFace(unsigned long mode)
{
    m_context->frontFace(mode);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::generateMipmap(unsigned long target)
{
    RefPtr<WebGLTexture> tex;
    if (!isGLES2Compliant()) {
        if (target == GraphicsContext3D::TEXTURE_2D)
            tex = m_textureUnits[m_activeTextureUnit].m_texture2DBinding;
        else if (target == GraphicsContext3D::TEXTURE_CUBE_MAP)
            tex = m_textureUnits[m_activeTextureUnit].m_textureCubeMapBinding;
        if (tex && !tex->canGenerateMipmaps()) {
            m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
            return;
        }
    }
    m_context->generateMipmap(target);
    if (!isGLES2Compliant()) {
        if (tex)
            tex->generateMipmapLevelInfo();
    }
    cleanupAfterGraphicsCall(false);
}

PassRefPtr<WebGLActiveInfo> WebGLRenderingContext::getActiveAttrib(WebGLProgram* program, unsigned long index, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    ActiveInfo info;
    if (!validateWebGLObject(program))
        return 0;
    if (!m_context->getActiveAttrib(program, index, info)) {
        return 0;
    }
    return WebGLActiveInfo::create(info.name, info.type, info.size);
}

PassRefPtr<WebGLActiveInfo> WebGLRenderingContext::getActiveUniform(WebGLProgram* program, unsigned long index, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    ActiveInfo info;
    if (!validateWebGLObject(program))
        return 0;
    if (!m_context->getActiveUniform(program, index, info)) {
        return 0;
    }
    if (!isGLES2Compliant()) {
        if (info.size > 1 && !info.name.endsWith("[0]"))
            info.name.append("[0]");
    }
    return WebGLActiveInfo::create(info.name, info.type, info.size);
}

bool WebGLRenderingContext::getAttachedShaders(WebGLProgram* program, Vector<WebGLShader*>& shaderObjects, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    shaderObjects.clear();
    if (!validateWebGLObject(program))
        return false;
    int numShaders = 0;
    m_context->getProgramiv(program, GraphicsContext3D::ATTACHED_SHADERS, &numShaders);
    if (numShaders) {
        OwnArrayPtr<unsigned int> shaders(new unsigned int[numShaders]);
        int count;
        m_context->getAttachedShaders(program, numShaders, &count, shaders.get());
        if (count != numShaders)
            return false;
        shaderObjects.resize(numShaders);
        for (int ii = 0; ii < numShaders; ++ii) {
            WebGLShader* shader = findShader(shaders[ii]);
            if (!shader) {
                shaderObjects.clear();
                return false;
            }
            shaderObjects[ii] = shader;
        }
    }
    return true;
}

int WebGLRenderingContext::getAttribLocation(WebGLProgram* program, const String& name)
{
    return m_context->getAttribLocation(program, name);
}

WebGLGetInfo WebGLRenderingContext::getBufferParameter(unsigned long target, unsigned long pname, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (target != GraphicsContext3D::ARRAY_BUFFER && target != GraphicsContext3D::ELEMENT_ARRAY_BUFFER) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return WebGLGetInfo();
    }

    if (pname != GraphicsContext3D::BUFFER_SIZE && pname != GraphicsContext3D::BUFFER_USAGE) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return WebGLGetInfo();
    }

    WebGLStateRestorer(this, false);
    int value;
    m_context->getBufferParameteriv(target, pname, &value);
    if (pname == GraphicsContext3D::BUFFER_SIZE)
        return WebGLGetInfo(static_cast<long>(value));
    else
        return WebGLGetInfo(static_cast<unsigned long>(value));
}

PassRefPtr<WebGLContextAttributes> WebGLRenderingContext::getContextAttributes()
{
    // We always need to return a new WebGLContextAttributes object to
    // prevent the user from mutating any cached version.
    return WebGLContextAttributes::create(m_context->getContextAttributes());
}

unsigned long WebGLRenderingContext::getError()
{
    return m_context->getError();
}

WebGLGetInfo WebGLRenderingContext::getFramebufferAttachmentParameter(unsigned long target, unsigned long attachment, unsigned long pname, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateFramebufferFuncParameters(target, attachment))
        return WebGLGetInfo();
    switch (pname) {
    case GraphicsContext3D::FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
    case GraphicsContext3D::FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
    case GraphicsContext3D::FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
    case GraphicsContext3D::FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
        break;
    default:
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return WebGLGetInfo();
    }

    if (!m_framebufferBinding || !m_framebufferBinding->object()) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return WebGLGetInfo();
    }

    if (pname != GraphicsContext3D::FRAMEBUFFER_ATTACHMENT_OBJECT_NAME) {
        WebGLStateRestorer(this, false);
        int value;
        m_context->getFramebufferAttachmentParameteriv(target, attachment, pname, &value);
        if (pname == GraphicsContext3D::FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE)
            return WebGLGetInfo(static_cast<unsigned long>(value));
        else
            return WebGLGetInfo(static_cast<long>(value));
    } else {
        WebGLStateRestorer(this, false);
        int type = 0;
        m_context->getFramebufferAttachmentParameteriv(target, attachment, GraphicsContext3D::FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
        int value = 0;
        m_context->getFramebufferAttachmentParameteriv(target, attachment, GraphicsContext3D::FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &value);
        switch (type) {
        case GraphicsContext3D::RENDERBUFFER:
            return WebGLGetInfo(PassRefPtr<WebGLRenderbuffer>(findRenderbuffer(static_cast<Platform3DObject>(value))));
        case GraphicsContext3D::TEXTURE:
            return WebGLGetInfo(PassRefPtr<WebGLTexture>(findTexture(static_cast<Platform3DObject>(value))));
        default:
            // FIXME: raise exception?
            return WebGLGetInfo();
        }
    }
}

WebGLGetInfo WebGLRenderingContext::getParameter(unsigned long pname, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    WebGLStateRestorer(this, false);
    switch (pname) {
    case GraphicsContext3D::ACTIVE_TEXTURE:
        return getUnsignedLongParameter(pname);
    case GraphicsContext3D::ALIASED_LINE_WIDTH_RANGE:
        return getWebGLFloatArrayParameter(pname);
    case GraphicsContext3D::ALIASED_POINT_SIZE_RANGE:
        return getWebGLFloatArrayParameter(pname);
    case GraphicsContext3D::ALPHA_BITS:
        return getLongParameter(pname);
    case GraphicsContext3D::ARRAY_BUFFER_BINDING:
        return WebGLGetInfo(PassRefPtr<WebGLBuffer>(m_boundArrayBuffer));
    case GraphicsContext3D::BLEND:
        return getBooleanParameter(pname);
    case GraphicsContext3D::BLEND_COLOR:
        return getWebGLFloatArrayParameter(pname);
    case GraphicsContext3D::BLEND_DST_ALPHA:
        return getUnsignedLongParameter(pname);
    case GraphicsContext3D::BLEND_DST_RGB:
        return getUnsignedLongParameter(pname);
    case GraphicsContext3D::BLEND_EQUATION_ALPHA:
        return getUnsignedLongParameter(pname);
    case GraphicsContext3D::BLEND_EQUATION_RGB:
        return getUnsignedLongParameter(pname);
    case GraphicsContext3D::BLEND_SRC_ALPHA:
        return getUnsignedLongParameter(pname);
    case GraphicsContext3D::BLEND_SRC_RGB:
        return getUnsignedLongParameter(pname);
    case GraphicsContext3D::BLUE_BITS:
        return getLongParameter(pname);
    case GraphicsContext3D::COLOR_CLEAR_VALUE:
        return getWebGLFloatArrayParameter(pname);
    case GraphicsContext3D::COLOR_WRITEMASK:
        return getBooleanArrayParameter(pname);
    case GraphicsContext3D::COMPRESSED_TEXTURE_FORMATS:
        // Defined as null in the spec
        return WebGLGetInfo();
    case GraphicsContext3D::CULL_FACE:
        return getBooleanParameter(pname);
    case GraphicsContext3D::CULL_FACE_MODE:
        return getUnsignedLongParameter(pname);
    case GraphicsContext3D::CURRENT_PROGRAM:
        return WebGLGetInfo(PassRefPtr<WebGLProgram>(m_currentProgram));
    case GraphicsContext3D::DEPTH_BITS:
        return getLongParameter(pname);
    case GraphicsContext3D::DEPTH_CLEAR_VALUE:
        return getFloatParameter(pname);
    case GraphicsContext3D::DEPTH_FUNC:
        return getUnsignedLongParameter(pname);
    case GraphicsContext3D::DEPTH_RANGE:
        return getWebGLFloatArrayParameter(pname);
    case GraphicsContext3D::DEPTH_TEST:
        return getBooleanParameter(pname);
    case GraphicsContext3D::DEPTH_WRITEMASK:
        return getBooleanParameter(pname);
    case GraphicsContext3D::DITHER:
        return getBooleanParameter(pname);
    case GraphicsContext3D::ELEMENT_ARRAY_BUFFER_BINDING:
        return WebGLGetInfo(PassRefPtr<WebGLBuffer>(m_boundElementArrayBuffer));
    case GraphicsContext3D::FRAMEBUFFER_BINDING:
        return WebGLGetInfo(PassRefPtr<WebGLFramebuffer>(m_framebufferBinding));
    case GraphicsContext3D::FRONT_FACE:
        return getUnsignedLongParameter(pname);
    case GraphicsContext3D::GENERATE_MIPMAP_HINT:
        return getUnsignedLongParameter(pname);
    case GraphicsContext3D::GREEN_BITS:
        return getLongParameter(pname);
    case GraphicsContext3D::IMPLEMENTATION_COLOR_READ_FORMAT:
        return getLongParameter(pname);
    case GraphicsContext3D::IMPLEMENTATION_COLOR_READ_TYPE:
        return getLongParameter(pname);
    case GraphicsContext3D::LINE_WIDTH:
        return getFloatParameter(pname);
    case GraphicsContext3D::MAX_COMBINED_TEXTURE_IMAGE_UNITS:
        return getLongParameter(pname);
    case GraphicsContext3D::MAX_CUBE_MAP_TEXTURE_SIZE:
        return getLongParameter(pname);
    case GraphicsContext3D::MAX_FRAGMENT_UNIFORM_VECTORS:
        return getLongParameter(pname);
    case GraphicsContext3D::MAX_RENDERBUFFER_SIZE:
        return getLongParameter(pname);
    case GraphicsContext3D::MAX_TEXTURE_IMAGE_UNITS:
        return getLongParameter(pname);
    case GraphicsContext3D::MAX_TEXTURE_SIZE:
        return getLongParameter(pname);
    case GraphicsContext3D::MAX_VARYING_VECTORS:
        return getLongParameter(pname);
    case GraphicsContext3D::MAX_VERTEX_ATTRIBS:
        return getLongParameter(pname);
    case GraphicsContext3D::MAX_VERTEX_TEXTURE_IMAGE_UNITS:
        return getLongParameter(pname);
    case GraphicsContext3D::MAX_VERTEX_UNIFORM_VECTORS:
        return getLongParameter(pname);
    case GraphicsContext3D::MAX_VIEWPORT_DIMS:
        return getWebGLIntArrayParameter(pname);
    case GraphicsContext3D::NUM_COMPRESSED_TEXTURE_FORMATS:
        // WebGL 1.0 specifies that there are no compressed texture formats.
        return WebGLGetInfo(static_cast<long>(0));
    case GraphicsContext3D::NUM_SHADER_BINARY_FORMATS:
        // FIXME: should we always return 0 for this?
        return getLongParameter(pname);
    case GraphicsContext3D::PACK_ALIGNMENT:
        return getLongParameter(pname);
    case GraphicsContext3D::POLYGON_OFFSET_FACTOR:
        return getFloatParameter(pname);
    case GraphicsContext3D::POLYGON_OFFSET_FILL:
        return getBooleanParameter(pname);
    case GraphicsContext3D::POLYGON_OFFSET_UNITS:
        return getFloatParameter(pname);
    case GraphicsContext3D::RED_BITS:
        return getLongParameter(pname);
    case GraphicsContext3D::RENDERBUFFER_BINDING:
        return WebGLGetInfo(PassRefPtr<WebGLRenderbuffer>(m_renderbufferBinding));
    case GraphicsContext3D::SAMPLE_BUFFERS:
        return getLongParameter(pname);
    case GraphicsContext3D::SAMPLE_COVERAGE_INVERT:
        return getBooleanParameter(pname);
    case GraphicsContext3D::SAMPLE_COVERAGE_VALUE:
        return getFloatParameter(pname);
    case GraphicsContext3D::SAMPLES:
        return getLongParameter(pname);
    case GraphicsContext3D::SCISSOR_BOX:
        return getWebGLIntArrayParameter(pname);
    case GraphicsContext3D::SCISSOR_TEST:
        return getBooleanParameter(pname);
    case GraphicsContext3D::STENCIL_BACK_FAIL:
        return getUnsignedLongParameter(pname);
    case GraphicsContext3D::STENCIL_BACK_FUNC:
        return getUnsignedLongParameter(pname);
    case GraphicsContext3D::STENCIL_BACK_PASS_DEPTH_FAIL:
        return getUnsignedLongParameter(pname);
    case GraphicsContext3D::STENCIL_BACK_PASS_DEPTH_PASS:
        return getUnsignedLongParameter(pname);
    case GraphicsContext3D::STENCIL_BACK_REF:
        return getLongParameter(pname);
    case GraphicsContext3D::STENCIL_BACK_VALUE_MASK:
        return getUnsignedLongParameter(pname);
    case GraphicsContext3D::STENCIL_BACK_WRITEMASK:
        return getUnsignedLongParameter(pname);
    case GraphicsContext3D::STENCIL_BITS:
        return getLongParameter(pname);
    case GraphicsContext3D::STENCIL_CLEAR_VALUE:
        return getLongParameter(pname);
    case GraphicsContext3D::STENCIL_FAIL:
        return getUnsignedLongParameter(pname);
    case GraphicsContext3D::STENCIL_FUNC:
        return getUnsignedLongParameter(pname);
    case GraphicsContext3D::STENCIL_PASS_DEPTH_FAIL:
        return getUnsignedLongParameter(pname);
    case GraphicsContext3D::STENCIL_PASS_DEPTH_PASS:
        return getUnsignedLongParameter(pname);
    case GraphicsContext3D::STENCIL_REF:
        return getLongParameter(pname);
    case GraphicsContext3D::STENCIL_TEST:
        return getBooleanParameter(pname);
    case GraphicsContext3D::STENCIL_VALUE_MASK:
        return getUnsignedLongParameter(pname);
    case GraphicsContext3D::STENCIL_WRITEMASK:
        return getUnsignedLongParameter(pname);
    case GraphicsContext3D::SUBPIXEL_BITS:
        return getLongParameter(pname);
    case GraphicsContext3D::TEXTURE_BINDING_2D:
        return WebGLGetInfo(PassRefPtr<WebGLTexture>(m_textureUnits[m_activeTextureUnit].m_texture2DBinding));
    case GraphicsContext3D::TEXTURE_BINDING_CUBE_MAP:
        return WebGLGetInfo(PassRefPtr<WebGLTexture>(m_textureUnits[m_activeTextureUnit].m_textureCubeMapBinding));
    case GraphicsContext3D::UNPACK_ALIGNMENT:
        // FIXME: should this be "long" in the spec?
        return getIntParameter(pname);
    case GraphicsContext3D::UNPACK_FLIP_Y_WEBGL:
        return WebGLGetInfo(m_unpackFlipY);
    case GraphicsContext3D::UNPACK_PREMULTIPLY_ALPHA_WEBGL:
        return WebGLGetInfo(m_unpackPremultiplyAlpha);
    case GraphicsContext3D::VIEWPORT:
        return getWebGLIntArrayParameter(pname);
    default:
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return WebGLGetInfo();
    }
}

WebGLGetInfo WebGLRenderingContext::getProgramParameter(WebGLProgram* program, unsigned long pname, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateWebGLObject(program))
        return WebGLGetInfo();

    WebGLStateRestorer(this, false);
    int value = 0;
    switch (pname) {
    case GraphicsContext3D::DELETE_STATUS:
    case GraphicsContext3D::VALIDATE_STATUS:
        m_context->getProgramiv(program, pname, &value);
        return WebGLGetInfo(static_cast<bool>(value));
    case GraphicsContext3D::LINK_STATUS:
        if (program->isLinkFailureFlagSet())
            return WebGLGetInfo(false);
        m_context->getProgramiv(program, pname, &value);
        return WebGLGetInfo(static_cast<bool>(value));
    case GraphicsContext3D::INFO_LOG_LENGTH:
    case GraphicsContext3D::ATTACHED_SHADERS:
    case GraphicsContext3D::ACTIVE_ATTRIBUTES:
    case GraphicsContext3D::ACTIVE_ATTRIBUTE_MAX_LENGTH:
    case GraphicsContext3D::ACTIVE_UNIFORMS:
    case GraphicsContext3D::ACTIVE_UNIFORM_MAX_LENGTH:
        m_context->getProgramiv(program, pname, &value);
        return WebGLGetInfo(static_cast<long>(value));
    default:
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return WebGLGetInfo();
    }
}

String WebGLRenderingContext::getProgramInfoLog(WebGLProgram* program, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateWebGLObject(program))
        return "";
    WebGLStateRestorer(this, false);
    return m_context->getProgramInfoLog(program);
}

WebGLGetInfo WebGLRenderingContext::getRenderbufferParameter(unsigned long target, unsigned long pname, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (target != GraphicsContext3D::RENDERBUFFER) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return WebGLGetInfo();
    }

    WebGLStateRestorer(this, false);
    int value = 0;
    switch (pname) {
    case GraphicsContext3D::RENDERBUFFER_WIDTH:
    case GraphicsContext3D::RENDERBUFFER_HEIGHT:
    case GraphicsContext3D::RENDERBUFFER_RED_SIZE:
    case GraphicsContext3D::RENDERBUFFER_GREEN_SIZE:
    case GraphicsContext3D::RENDERBUFFER_BLUE_SIZE:
    case GraphicsContext3D::RENDERBUFFER_ALPHA_SIZE:
    case GraphicsContext3D::RENDERBUFFER_DEPTH_SIZE:
    case GraphicsContext3D::RENDERBUFFER_STENCIL_SIZE:
        m_context->getRenderbufferParameteriv(target, pname, &value);
        return WebGLGetInfo(static_cast<long>(value));
    case GraphicsContext3D::RENDERBUFFER_INTERNAL_FORMAT:
        if (!m_renderbufferBinding) {
            m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
            return WebGLGetInfo();
        }
        return WebGLGetInfo(m_renderbufferBinding->getInternalFormat());
    default:
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return WebGLGetInfo();
    }
}

WebGLGetInfo WebGLRenderingContext::getShaderParameter(WebGLShader* shader, unsigned long pname, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateWebGLObject(shader))
        return WebGLGetInfo();
    WebGLStateRestorer(this, false);
    int value = 0;
    switch (pname) {
    case GraphicsContext3D::DELETE_STATUS:
    case GraphicsContext3D::COMPILE_STATUS:
        m_context->getShaderiv(shader, pname, &value);
        return WebGLGetInfo(static_cast<bool>(value));
    case GraphicsContext3D::SHADER_TYPE:
        m_context->getShaderiv(shader, pname, &value);
        return WebGLGetInfo(static_cast<unsigned long>(value));
    case GraphicsContext3D::INFO_LOG_LENGTH:
    case GraphicsContext3D::SHADER_SOURCE_LENGTH:
        m_context->getShaderiv(shader, pname, &value);
        return WebGLGetInfo(static_cast<long>(value));
    default:
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return WebGLGetInfo();
    }
}

String WebGLRenderingContext::getShaderInfoLog(WebGLShader* shader, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateWebGLObject(shader))
        return "";
    WebGLStateRestorer(this, false);
    return m_context->getShaderInfoLog(shader);
}

String WebGLRenderingContext::getShaderSource(WebGLShader* shader, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateWebGLObject(shader))
        return "";
    WebGLStateRestorer(this, false);
    return m_context->getShaderSource(shader);
}

String WebGLRenderingContext::getString(unsigned long name)
{
    WebGLStateRestorer(this, false);
    return m_context->getString(name);
}

WebGLGetInfo WebGLRenderingContext::getTexParameter(unsigned long target, unsigned long pname, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (target != GraphicsContext3D::TEXTURE_2D
        && target != GraphicsContext3D::TEXTURE_CUBE_MAP) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return WebGLGetInfo();
    }
    WebGLStateRestorer(this, false);
    int value = 0;
    switch (pname) {
    case GraphicsContext3D::TEXTURE_MAG_FILTER:
    case GraphicsContext3D::TEXTURE_MIN_FILTER:
    case GraphicsContext3D::TEXTURE_WRAP_S:
    case GraphicsContext3D::TEXTURE_WRAP_T:
        m_context->getTexParameteriv(target, pname, &value);
        return WebGLGetInfo(static_cast<unsigned long>(value));
    default:
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return WebGLGetInfo();
    }
}

WebGLGetInfo WebGLRenderingContext::getUniform(WebGLProgram* program, const WebGLUniformLocation* uniformLocation, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateWebGLObject(program))
        return WebGLGetInfo();
    if (!uniformLocation) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return WebGLGetInfo();
    }
    if (uniformLocation->program() != program) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return WebGLGetInfo();
    }
    long location = uniformLocation->location();
    
    WebGLStateRestorer(this, false);
    // FIXME: make this more efficient using WebGLUniformLocation and caching types in it
    int activeUniforms = 0;
    m_context->getProgramiv(program, GraphicsContext3D::ACTIVE_UNIFORMS, &activeUniforms);
    for (int i = 0; i < activeUniforms; i++) {
        ActiveInfo info;
        if (!m_context->getActiveUniform(program, i, info))
            return WebGLGetInfo();
        // Strip "[0]" from the name if it's an array.
        if (info.size > 1)
            info.name = info.name.left(info.name.length() - 3);
        // If it's an array, we need to iterate through each element, appending "[index]" to the name.
        for (int index = 0; index < info.size; ++index) {
            String name = info.name;
            if (info.size > 1 && index >= 1) {
                name.append('[');
                name.append(String::number(index));
                name.append(']');
            }
            // Now need to look this up by name again to find its location
            long loc = m_context->getUniformLocation(program, name);
            if (loc == location) {
                // Found it. Use the type in the ActiveInfo to determine the return type.
                GraphicsContext3D::WebGLEnumType baseType;
                unsigned length;
                switch (info.type) {
                case GraphicsContext3D::BOOL:
                    baseType = GraphicsContext3D::BOOL;
                    length = 1;
                    break;
                case GraphicsContext3D::BOOL_VEC2:
                    baseType = GraphicsContext3D::BOOL;
                    length = 2;
                    break;
                case GraphicsContext3D::BOOL_VEC3:
                    baseType = GraphicsContext3D::BOOL;
                    length = 3;
                    break;
                case GraphicsContext3D::BOOL_VEC4:
                    baseType = GraphicsContext3D::BOOL;
                    length = 4;
                    break;
                case GraphicsContext3D::INT:
                    baseType = GraphicsContext3D::INT;
                    length = 1;
                    break;
                case GraphicsContext3D::INT_VEC2:
                    baseType = GraphicsContext3D::INT;
                    length = 2;
                    break;
                case GraphicsContext3D::INT_VEC3:
                    baseType = GraphicsContext3D::INT;
                    length = 3;
                    break;
                case GraphicsContext3D::INT_VEC4:
                    baseType = GraphicsContext3D::INT;
                    length = 4;
                    break;
                case GraphicsContext3D::FLOAT:
                    baseType = GraphicsContext3D::FLOAT;
                    length = 1;
                    break;
                case GraphicsContext3D::FLOAT_VEC2:
                    baseType = GraphicsContext3D::FLOAT;
                    length = 2;
                    break;
                case GraphicsContext3D::FLOAT_VEC3:
                    baseType = GraphicsContext3D::FLOAT;
                    length = 3;
                    break;
                case GraphicsContext3D::FLOAT_VEC4:
                    baseType = GraphicsContext3D::FLOAT;
                    length = 4;
                    break;
                case GraphicsContext3D::FLOAT_MAT2:
                    baseType = GraphicsContext3D::FLOAT;
                    length = 4;
                    break;
                case GraphicsContext3D::FLOAT_MAT3:
                    baseType = GraphicsContext3D::FLOAT;
                    length = 9;
                    break;
                case GraphicsContext3D::FLOAT_MAT4:
                    baseType = GraphicsContext3D::FLOAT;
                    length = 16;
                    break;
                default:
                    // Can't handle this type
                    // FIXME: what to do about samplers?
                    m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
                    return WebGLGetInfo();
                }
                switch (baseType) {
                case GraphicsContext3D::FLOAT: {
                    float value[16] = {0};
                    m_context->getUniformfv(program, location, value);
                    if (length == 1)
                        return WebGLGetInfo(value[0]);
                    return WebGLGetInfo(Float32Array::create(value, length));
                }
                case GraphicsContext3D::INT: {
                    int value[16] = {0};
                    m_context->getUniformiv(program, location, value);
                    if (length == 1)
                        return WebGLGetInfo(static_cast<long>(value[0]));
                    return WebGLGetInfo(Int32Array::create(value, length));
                }
                case GraphicsContext3D::BOOL: {
                    int value[16] = {0};
                    m_context->getUniformiv(program, location, value);
                    if (length > 1) {
                        unsigned char boolValue[16] = {0};
                        for (unsigned j = 0; j < length; j++)
                            boolValue[j] = static_cast<bool>(value[j]);
                        return WebGLGetInfo(Uint8Array::create(boolValue, length));
                    }
                    return WebGLGetInfo(static_cast<bool>(value[0]));
                }
                default:
                    notImplemented();
                }
            }
        }
    }
    // If we get here, something went wrong in our unfortunately complex logic above
    m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
    return WebGLGetInfo();
}

PassRefPtr<WebGLUniformLocation> WebGLRenderingContext::getUniformLocation(WebGLProgram* program, const String& name, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateWebGLObject(program))
        return 0;
    WebGLStateRestorer(this, false);
    long uniformLocation = m_context->getUniformLocation(program, name);
    if (uniformLocation == -1)
        return 0;
    return WebGLUniformLocation::create(program, uniformLocation);
}

WebGLGetInfo WebGLRenderingContext::getVertexAttrib(unsigned long index, unsigned long pname, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    WebGLStateRestorer(this, false);
    if (index >= m_maxVertexAttribs) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return WebGLGetInfo();
    }
    switch (pname) {
    case GraphicsContext3D::VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
        if (!isGLES2Compliant() && !index && m_vertexAttribState[0].bufferBinding == m_vertexAttrib0Buffer
            || index >= m_vertexAttribState.size()
            || !m_vertexAttribState[index].bufferBinding
            || !m_vertexAttribState[index].bufferBinding->object())
            return WebGLGetInfo();
        return WebGLGetInfo(PassRefPtr<WebGLBuffer>(m_vertexAttribState[index].bufferBinding));
    case GraphicsContext3D::VERTEX_ATTRIB_ARRAY_ENABLED:
        if (index >= m_vertexAttribState.size())
            return WebGLGetInfo(false);
        return WebGLGetInfo(m_vertexAttribState[index].enabled);
    case GraphicsContext3D::VERTEX_ATTRIB_ARRAY_NORMALIZED:
        if (index >= m_vertexAttribState.size())
            return WebGLGetInfo(false);
        return WebGLGetInfo(m_vertexAttribState[index].normalized);
    case GraphicsContext3D::VERTEX_ATTRIB_ARRAY_SIZE:
        if (index >= m_vertexAttribState.size())
            return WebGLGetInfo(static_cast<long>(4));
        return WebGLGetInfo(m_vertexAttribState[index].size);
    case GraphicsContext3D::VERTEX_ATTRIB_ARRAY_STRIDE:
        if (index >= m_vertexAttribState.size())
            return WebGLGetInfo(static_cast<long>(0));
        return WebGLGetInfo(m_vertexAttribState[index].originalStride);
    case GraphicsContext3D::VERTEX_ATTRIB_ARRAY_TYPE:
        if (index >= m_vertexAttribState.size())
            return WebGLGetInfo(static_cast<unsigned long>(GraphicsContext3D::FLOAT));
        return WebGLGetInfo(m_vertexAttribState[index].type);
    case GraphicsContext3D::CURRENT_VERTEX_ATTRIB:
        if (index >= m_vertexAttribState.size()) {
            float value[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
            return WebGLGetInfo(Float32Array::create(value, 4));
        }
        return WebGLGetInfo(Float32Array::create(m_vertexAttribState[index].value, 4));
    default:
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return WebGLGetInfo();
    }
}

long WebGLRenderingContext::getVertexAttribOffset(unsigned long index, unsigned long pname)
{
    long result = m_context->getVertexAttribOffset(index, pname);
    cleanupAfterGraphicsCall(false);
    return result;
}

void WebGLRenderingContext::hint(unsigned long target, unsigned long mode)
{
    if (!isGLES2Compliant()) {
        if (target != GraphicsContext3D::GENERATE_MIPMAP_HINT) {
            m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
            return;
        }
    }
    m_context->hint(target, mode);
    cleanupAfterGraphicsCall(false);
}

bool WebGLRenderingContext::isBuffer(WebGLBuffer* buffer)
{
    if (!buffer)
        return false;

    return m_context->isBuffer(buffer);
}

bool WebGLRenderingContext::isEnabled(unsigned long cap)
{
    if (!isGLES2Compliant()) {
        if (!validateCapability(cap))
            return false;
    }
    return m_context->isEnabled(cap);
}

bool WebGLRenderingContext::isFramebuffer(WebGLFramebuffer* framebuffer)
{
    if (!framebuffer)
        return false;

    return m_context->isFramebuffer(framebuffer);
}

bool WebGLRenderingContext::isProgram(WebGLProgram* program)
{
    if (!program)
        return false;

    return m_context->isProgram(program);
}

bool WebGLRenderingContext::isRenderbuffer(WebGLRenderbuffer* renderbuffer)
{
    if (!renderbuffer)
        return false;

    return m_context->isRenderbuffer(renderbuffer);
}

bool WebGLRenderingContext::isShader(WebGLShader* shader)
{
    if (!shader)
        return false;

    return m_context->isShader(shader);
}

bool WebGLRenderingContext::isTexture(WebGLTexture* texture)
{
    if (!texture)
        return false;

    return m_context->isTexture(texture);
}

void WebGLRenderingContext::lineWidth(double width)
{
    m_context->lineWidth((float) width);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::linkProgram(WebGLProgram* program, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateWebGLObject(program))
        return;
    if (!isGLES2Compliant()) {
        Vector<WebGLShader*> shaders;
        bool succeed = getAttachedShaders(program, shaders, ec);
        if (succeed) {
            bool vShader = false;
            bool fShader = false;
            for (size_t ii = 0; ii < shaders.size() && (!vShader || !fShader); ++ii) {
                if (shaders[ii]->getType() == GraphicsContext3D::VERTEX_SHADER)
                    vShader = true;
                else if (shaders[ii]->getType() == GraphicsContext3D::FRAGMENT_SHADER)
                    fShader = true;
            }
            if (!vShader || !fShader)
                succeed = false;
        }
        if (!succeed) {
            program->setLinkFailureFlag(true);
            return;
        }
        program->setLinkFailureFlag(false);
    }

    m_context->linkProgram(program);
    program->cacheActiveAttribLocations();
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::pixelStorei(unsigned long pname, long param)
{
    switch (pname) {
    case GraphicsContext3D::UNPACK_FLIP_Y_WEBGL:
        m_unpackFlipY = param;
        break;
    case GraphicsContext3D::UNPACK_PREMULTIPLY_ALPHA_WEBGL:
        m_unpackPremultiplyAlpha = param;
        break;
    case GraphicsContext3D::PACK_ALIGNMENT:
    case GraphicsContext3D::UNPACK_ALIGNMENT:
        m_context->pixelStorei(pname, param);
        if (param == 1 || param == 2 || param == 4 || param == 8) {
            if (pname == GraphicsContext3D::PACK_ALIGNMENT)
                m_packAlignment = static_cast<int>(param);
            else // GraphicsContext3D::UNPACK_ALIGNMENT:
                m_unpackAlignment = static_cast<int>(param);
        }
        break;
    default:
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return;
    }
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::polygonOffset(double factor, double units)
{
    m_context->polygonOffset((float) factor, (float) units);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::readPixels(long x, long y, long width, long height, unsigned long format, unsigned long type, ArrayBufferView* pixels)
{
    // Validate input parameters.
    unsigned long componentsPerPixel, bytesPerComponent;
    if (!m_context->computeFormatAndTypeParameters(format, type, &componentsPerPixel, &bytesPerComponent)) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return;
    }
    if (!pixels) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }
    if (width < 0 || height < 0) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }
    if (!((format == GraphicsContext3D::RGBA && type == GraphicsContext3D::UNSIGNED_BYTE) || (format == m_implementationColorReadFormat && type == m_implementationColorReadType))) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return;
    }
    // Validate array type against pixel type.
    if (type == GraphicsContext3D::UNSIGNED_BYTE && !pixels->isUnsignedByteArray()
        || type != GraphicsContext3D::UNSIGNED_BYTE && !pixels->isUnsignedShortArray()) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return;
    }
    // Calculate array size, taking into consideration of PACK_ALIGNMENT.
    unsigned long bytesPerRow = componentsPerPixel * bytesPerComponent * width;
    unsigned long padding = 0;
    unsigned long residualBytes = bytesPerRow % m_packAlignment;
    if (residualBytes) {
        padding = m_packAlignment - residualBytes;
        bytesPerRow += padding;
    }
    // The last row needs no padding.
    unsigned long totalBytes = bytesPerRow * height - padding;
    unsigned long num = totalBytes / bytesPerComponent;
    if (pixels->length() < num) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return;
    }
    void* data = pixels->baseAddress();
    m_context->readPixels(x, y, width, height, format, type, data);
#if PLATFORM(CG)
    // FIXME: remove this section when GL driver bug on Mac is fixed, i.e.,
    // when alpha is off, readPixels should set alpha to 255 instead of 0.
    if ((format == GraphicsContext3D::ALPHA || format == GraphicsContext3D::RGBA) && !m_context->getContextAttributes().alpha) {
        if (type == GraphicsContext3D::UNSIGNED_BYTE) {
            unsigned char* pixels = reinterpret_cast<unsigned char*>(data);
            for (long iy = 0; iy < height; ++iy) {
                for (long ix = 0; ix < width; ++ix) {
                    pixels[componentsPerPixel - 1] = 255;
                    pixels += componentsPerPixel;
                }
                pixels += padding;
            }
        }
        // FIXME: check whether we need to do the same with UNSIGNED_SHORT.
    }
#endif
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::releaseShaderCompiler()
{
    m_context->releaseShaderCompiler();
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::renderbufferStorage(unsigned long target, unsigned long internalformat, unsigned long width, unsigned long height)
{
    switch (internalformat) {
    case GraphicsContext3D::DEPTH_COMPONENT16:
    case GraphicsContext3D::RGBA4:
    case GraphicsContext3D::RGB5_A1:
    case GraphicsContext3D::RGB565:
    case GraphicsContext3D::STENCIL_INDEX8:
    case GraphicsContext3D::DEPTH_STENCIL:
        m_context->renderbufferStorage(target, internalformat, width, height);
        if (m_renderbufferBinding) {
            m_renderbufferBinding->setInternalFormat(internalformat);
            if (m_framebufferBinding)
                m_framebufferBinding->onAttachedObjectChange(m_renderbufferBinding.get());
        }
        cleanupAfterGraphicsCall(false);
        break;
    default:
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
    }
}

void WebGLRenderingContext::sampleCoverage(double value, bool invert)
{
    m_context->sampleCoverage((float) value, invert);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::scissor(long x, long y, unsigned long width, unsigned long height)
{
    m_context->scissor(x, y, width, height);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::shaderSource(WebGLShader* shader, const String& string, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateWebGLObject(shader))
        return;
    m_context->shaderSource(shader, string);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::stencilFunc(unsigned long func, long ref, unsigned long mask)
{
    m_context->stencilFunc(func, ref, mask);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::stencilFuncSeparate(unsigned long face, unsigned long func, long ref, unsigned long mask)
{
    m_context->stencilFuncSeparate(face, func, ref, mask);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::stencilMask(unsigned long mask)
{
    m_context->stencilMask(mask);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::stencilMaskSeparate(unsigned long face, unsigned long mask)
{
    m_context->stencilMaskSeparate(face, mask);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::stencilOp(unsigned long fail, unsigned long zfail, unsigned long zpass)
{
    m_context->stencilOp(fail, zfail, zpass);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::stencilOpSeparate(unsigned long face, unsigned long fail, unsigned long zfail, unsigned long zpass)
{
    m_context->stencilOpSeparate(face, fail, zfail, zpass);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::texImage2DBase(unsigned target, unsigned level, unsigned internalformat,
                                           unsigned width, unsigned height, unsigned border,
                                           unsigned format, unsigned type, void* pixels, ExceptionCode& ec)
{
    // FIXME: For now we ignore any errors returned
    ec = 0;
    if (!validateTexFuncParameters(target, level, internalformat, width, height, border, format, type))
        return;
    if (!isGLES2Compliant()) {
        if (level && WebGLTexture::isNPOT(width, height)) {
            m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
            return;
        }
    }
    m_context->texImage2D(target, level, internalformat, width, height,
                          border, format, type, pixels);
    WebGLTexture* tex = getTextureBinding(target);
    if (!isGLES2Compliant()) {
        if (tex)
            tex->setLevelInfo(target, level, internalformat, width, height, type);
    }
    if (m_framebufferBinding && tex)
        m_framebufferBinding->onAttachedObjectChange(tex);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::texImage2DImpl(unsigned target, unsigned level, unsigned internalformat,
                                           unsigned format, unsigned type, Image* image,
                                           bool flipY, bool premultiplyAlpha, ExceptionCode& ec)
{
    ec = 0;
    Vector<uint8_t> data;
    if (!m_context->extractImageData(image, format, type, flipY, premultiplyAlpha, data)) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }
    if (m_unpackAlignment != 1)
        m_context->pixelStorei(GraphicsContext3D::UNPACK_ALIGNMENT, 1);
    texImage2DBase(target, level, internalformat, image->width(), image->height(), 0,
                   format, type, data.data(), ec);
    if (m_unpackAlignment != 1)
        m_context->pixelStorei(GraphicsContext3D::UNPACK_ALIGNMENT, m_unpackAlignment);
}

void WebGLRenderingContext::texImage2D(unsigned target, unsigned level, unsigned internalformat,
                                       unsigned width, unsigned height, unsigned border,
                                       unsigned format, unsigned type, ArrayBufferView* pixels, ExceptionCode& ec)
{
    if (!validateTexFuncData(width, height, format, type, pixels))
        return;
    void* data = pixels ? pixels->baseAddress() : 0;
    Vector<uint8_t> tempData;
    bool changeUnpackAlignment = false;
    if (pixels && (m_unpackFlipY || m_unpackPremultiplyAlpha)) {
        if (!m_context->extractTextureData(width, height, format, type,
                                           m_unpackAlignment,
                                           m_unpackFlipY, m_unpackPremultiplyAlpha,
                                           pixels,
                                           tempData))
            return;
        data = tempData.data();
        changeUnpackAlignment = true;
    }
    if (changeUnpackAlignment)
        m_context->pixelStorei(GraphicsContext3D::UNPACK_ALIGNMENT, 1);
    texImage2DBase(target, level, internalformat, width, height, border,
                   format, type, data, ec);
    if (changeUnpackAlignment)
        m_context->pixelStorei(GraphicsContext3D::UNPACK_ALIGNMENT, m_unpackAlignment);
}

void WebGLRenderingContext::texImage2D(unsigned target, unsigned level, unsigned internalformat,
                                       unsigned format, unsigned type, ImageData* pixels, ExceptionCode& ec)
{
    ec = 0;
    Vector<uint8_t> data;
    if (!m_context->extractImageData(pixels, format, type, m_unpackFlipY, m_unpackPremultiplyAlpha, data)) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }
    if (m_unpackAlignment != 1)
        m_context->pixelStorei(GraphicsContext3D::UNPACK_ALIGNMENT, 1);
    texImage2DBase(target, level, internalformat, pixels->width(), pixels->height(), 0,
                   format, type, data.data(), ec);
    if (m_unpackAlignment != 1)
        m_context->pixelStorei(GraphicsContext3D::UNPACK_ALIGNMENT, m_unpackAlignment);
}

void WebGLRenderingContext::texImage2D(unsigned target, unsigned level, unsigned internalformat,
                                       unsigned format, unsigned type, HTMLImageElement* image, ExceptionCode& ec)
{
    ec = 0;
    if (!image || !image->cachedImage()) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }
    texImage2DImpl(target, level, internalformat, format, type, image->cachedImage()->image(),
                   m_unpackFlipY, m_unpackPremultiplyAlpha, ec);
}

void WebGLRenderingContext::texImage2D(unsigned target, unsigned level, unsigned internalformat,
                                       unsigned format, unsigned type, HTMLCanvasElement* canvas, ExceptionCode& ec)
{
    ec = 0;
    if (!canvas || !canvas->buffer()) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }
    texImage2DImpl(target, level, internalformat, format, type, canvas->buffer()->image(),
                   m_unpackFlipY, m_unpackPremultiplyAlpha, ec);
}

void WebGLRenderingContext::texImage2D(unsigned target, unsigned level, unsigned internalformat,
                                       unsigned format, unsigned type, HTMLVideoElement* video, ExceptionCode& ec)
{
    // FIXME: Need to implement this call
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(internalformat);
    UNUSED_PARAM(format);
    UNUSED_PARAM(type);
    UNUSED_PARAM(video);

    ec = 0;
    cleanupAfterGraphicsCall(false);
}

// Obsolete texImage2D entry points -- to be removed shortly. (FIXME)

void WebGLRenderingContext::texImage2D(unsigned target, unsigned level, ImageData* pixels,
                                       ExceptionCode& ec)
{
    printWarningToConsole("Calling obsolete texImage2D(GLenum target, GLint level, ImageData pixels)");
    texImage2D(target, level, pixels, 0, 0, ec);
}

void WebGLRenderingContext::texImage2D(unsigned target, unsigned level, ImageData* pixels,
                                       bool flipY, ExceptionCode& ec)
{
    printWarningToConsole("Calling obsolete texImage2D(GLenum target, GLint level, ImageData pixels, GLboolean flipY)");
    texImage2D(target, level, pixels, flipY, 0, ec);
}

void WebGLRenderingContext::texImage2D(unsigned target, unsigned level, ImageData* pixels,
                                       bool flipY, bool premultiplyAlpha, ExceptionCode& ec)
{
    printWarningToConsole("Calling obsolete texImage2D(GLenum target, GLint level, ImageData pixels, GLboolean flipY, GLboolean premultiplyAlpha)");
    ec = 0;
    Vector<uint8_t> data;
    if (!m_context->extractImageData(pixels, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, flipY, premultiplyAlpha, data)) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }
    texImage2DBase(target, level, GraphicsContext3D::RGBA, pixels->width(), pixels->height(), 0,
                   GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, data.data(), ec);
}


void WebGLRenderingContext::texImage2D(unsigned target, unsigned level, HTMLImageElement* image,
                                       ExceptionCode& ec)
{
    printWarningToConsole("Calling obsolete texImage2D(GLenum target, GLint level, HTMLImageElement image)");
    texImage2D(target, level, image, 0, 0, ec);
}

void WebGLRenderingContext::texImage2D(unsigned target, unsigned level, HTMLImageElement* image,
                                       bool flipY, ExceptionCode& ec)
{
    printWarningToConsole("Calling obsolete texImage2D(GLenum target, GLint level, HTMLImageElement image, GLboolean flipY)");
    texImage2D(target, level, image, flipY, 0, ec);
}

void WebGLRenderingContext::texImage2D(unsigned target, unsigned level, HTMLImageElement* image,
                                       bool flipY, bool premultiplyAlpha, ExceptionCode& ec)
{
    printWarningToConsole("Calling obsolete texImage2D(GLenum target, GLint level, HTMLImageElement image, GLboolean flipY, GLboolean premultiplyAlpha)");
    ec = 0;
    if (!image || !image->cachedImage()) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }
    texImage2DImpl(target, level, GraphicsContext3D::RGBA, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, image->cachedImage()->image(), flipY, premultiplyAlpha, ec);
}

void WebGLRenderingContext::texImage2D(unsigned target, unsigned level, HTMLCanvasElement* canvas,
                                       ExceptionCode& ec)
{
    printWarningToConsole("Calling obsolete texImage2D(GLenum target, GLint level, HTMLCanvasElement canvas)");
    texImage2D(target, level, canvas, 0, 0, ec);
}

void WebGLRenderingContext::texImage2D(unsigned target, unsigned level, HTMLCanvasElement* canvas,
                                       bool flipY, ExceptionCode& ec)
{
    printWarningToConsole("Calling obsolete texImage2D(GLenum target, GLint level, HTMLCanvasElement canvas, GLboolean flipY)");
    texImage2D(target, level, canvas, flipY, 0, ec);
}

void WebGLRenderingContext::texImage2D(unsigned target, unsigned level, HTMLCanvasElement* canvas,
                                       bool flipY, bool premultiplyAlpha, ExceptionCode& ec)
{
    printWarningToConsole("Calling obsolete texImage2D(GLenum target, GLint level, HTMLCanvasElement canvas, GLboolean flipY, GLboolean premultiplyAlpha)");
    ec = 0;
    if (!canvas || !canvas->buffer()) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }
    texImage2DImpl(target, level, GraphicsContext3D::RGBA, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, canvas->buffer()->image(), flipY, premultiplyAlpha, ec);
}

void WebGLRenderingContext::texImage2D(unsigned target, unsigned level, HTMLVideoElement* video,
                                       ExceptionCode& ec)
{
    texImage2D(target, level, video, 0, 0, ec);
}

void WebGLRenderingContext::texImage2D(unsigned target, unsigned level, HTMLVideoElement* video,
                                       bool flipY, ExceptionCode& ec)
{
    texImage2D(target, level, video, flipY, 0, ec);
}

void WebGLRenderingContext::texImage2D(unsigned target, unsigned level, HTMLVideoElement* video,
                                       bool flipY, bool premultiplyAlpha, ExceptionCode& ec)
{
    // FIXME: Need implement this call
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(video);
    UNUSED_PARAM(flipY);
    UNUSED_PARAM(premultiplyAlpha);
    
    ec = 0;
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::texParameter(unsigned long target, unsigned long pname, float paramf, int parami, bool isFloat)
{
    if (!isGLES2Compliant()) {
        RefPtr<WebGLTexture> tex = 0;
        switch (target) {
        case GraphicsContext3D::TEXTURE_2D:
            tex = m_textureUnits[m_activeTextureUnit].m_texture2DBinding;
            break;
        case GraphicsContext3D::TEXTURE_CUBE_MAP:
            tex = m_textureUnits[m_activeTextureUnit].m_textureCubeMapBinding;
            break;
        default:
            m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
            return;
        }
        switch (pname) {
        case GraphicsContext3D::TEXTURE_MIN_FILTER:
        case GraphicsContext3D::TEXTURE_MAG_FILTER:
            break;
        case GraphicsContext3D::TEXTURE_WRAP_S:
        case GraphicsContext3D::TEXTURE_WRAP_T:
            if (isFloat && paramf != GraphicsContext3D::CLAMP_TO_EDGE && paramf != GraphicsContext3D::MIRRORED_REPEAT && paramf != GraphicsContext3D::REPEAT
                || !isFloat && parami != GraphicsContext3D::CLAMP_TO_EDGE && parami != GraphicsContext3D::MIRRORED_REPEAT && parami != GraphicsContext3D::REPEAT) {
                m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
                return;
            }
            break;
        default:
            m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
            return;
        }
        if (tex) {
            if (isFloat)
                tex->setParameterf(pname, paramf);
            else
                tex->setParameteri(pname, parami);
        }
    }
    if (isFloat)
        m_context->texParameterf(target, pname, paramf);
    else
        m_context->texParameteri(target, pname, parami);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::texParameterf(unsigned target, unsigned pname, float param)
{
    texParameter(target, pname, param, 0, true);
}

void WebGLRenderingContext::texParameteri(unsigned target, unsigned pname, int param)
{
    texParameter(target, pname, 0, param, false);
}

void WebGLRenderingContext::texSubImage2DBase(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                                              unsigned width, unsigned height,
                                              unsigned format, unsigned type, void* pixels, ExceptionCode& ec)
{
    // FIXME: For now we ignore any errors returned
    ec = 0;
    if (!validateTexFuncFormatAndType(format, type))
        return;

    m_context->texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::texSubImage2DImpl(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                                              unsigned format, unsigned type,
                                              Image* image, bool flipY, bool premultiplyAlpha, ExceptionCode& ec)
{
    ec = 0;
    Vector<uint8_t> data;
    if (!m_context->extractImageData(image, format, type, flipY, premultiplyAlpha, data)) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }
    texSubImage2DBase(target, level, xoffset, yoffset, image->width(), image->height(),
                      format, type, data.data(), ec);
}

void WebGLRenderingContext::texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                                          unsigned width, unsigned height,
                                          unsigned format, unsigned type, ArrayBufferView* pixels, ExceptionCode& ec)
{
    if (!validateTexFuncData(width, height, format, type, pixels))
        return;
    void* data = pixels ? pixels->baseAddress() : 0;
    Vector<uint8_t> tempData;
    bool changeUnpackAlignment = false;
    if (pixels && (m_unpackFlipY || m_unpackPremultiplyAlpha)) {
        if (!m_context->extractTextureData(width, height, format, type,
                                           m_unpackAlignment,
                                           m_unpackFlipY, m_unpackPremultiplyAlpha,
                                           pixels,
                                           tempData))
            return;
        data = tempData.data();
        changeUnpackAlignment = true;
    }
    if (changeUnpackAlignment)
        m_context->pixelStorei(GraphicsContext3D::UNPACK_ALIGNMENT, 1);
    texSubImage2DBase(target, level, xoffset, yoffset, width, height, format, type, data, ec);
    if (changeUnpackAlignment)
        m_context->pixelStorei(GraphicsContext3D::UNPACK_ALIGNMENT, m_unpackAlignment);
}

void WebGLRenderingContext::texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                                          unsigned format, unsigned type, ImageData* pixels, ExceptionCode& ec)
{
    ec = 0;
    Vector<uint8_t> data;
    if (!m_context->extractImageData(pixels, format, type, m_unpackFlipY, m_unpackPremultiplyAlpha, data)) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }
    texSubImage2DBase(target, level, xoffset, yoffset, pixels->width(), pixels->height(),
                      format, type, data.data(), ec);
}

void WebGLRenderingContext::texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                                          unsigned format, unsigned type, HTMLImageElement* image, ExceptionCode& ec)
{
    ec = 0;
    if (!image || !image->cachedImage()) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }
    texSubImage2DImpl(target, level, xoffset, yoffset, format, type, image->cachedImage()->image(),
                      m_unpackFlipY, m_unpackPremultiplyAlpha, ec);
}

void WebGLRenderingContext::texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                                          unsigned format, unsigned type, HTMLCanvasElement* canvas, ExceptionCode& ec)
{
    ec = 0;
    if (!canvas || !canvas->buffer()) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }
    texSubImage2DImpl(target, level, xoffset, yoffset, format, type, canvas->buffer()->image(),
                      m_unpackFlipY, m_unpackPremultiplyAlpha, ec);
}

void WebGLRenderingContext::texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                                          unsigned format, unsigned type, HTMLVideoElement* video, ExceptionCode& ec)
{
    // FIXME: Need to implement this call
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(xoffset);
    UNUSED_PARAM(yoffset);
    UNUSED_PARAM(format);
    UNUSED_PARAM(type);
    UNUSED_PARAM(video);
    ec = 0;
    cleanupAfterGraphicsCall(false);
}

// Obsolete texSubImage2D entry points -- to be removed shortly. (FIXME)

void WebGLRenderingContext::texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                                          ImageData* pixels, ExceptionCode& ec)
{
    printWarningToConsole("Calling obsolete texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, ImageData pixels)");
    texSubImage2D(target, level, xoffset, yoffset, pixels, 0, 0, ec);
}

void WebGLRenderingContext::texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                                          ImageData* pixels, bool flipY, ExceptionCode& ec)
{
    printWarningToConsole("Calling obsolete texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, ImageData pixels, GLboolean flipY)");
    texSubImage2D(target, level, xoffset, yoffset, pixels, flipY, 0, ec);
}

void WebGLRenderingContext::texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                                          ImageData* pixels, bool flipY, bool premultiplyAlpha, ExceptionCode& ec)
{
    printWarningToConsole("Calling obsolete texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, ImageData pixels, GLboolean flipY, GLboolean premultiplyAlpha)");
    ec = 0;
    Vector<uint8_t> data;
    if (!m_context->extractImageData(pixels, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, flipY, premultiplyAlpha, data)) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }
    texSubImage2DBase(target, level, xoffset, yoffset, pixels->width(), pixels->height(),
                      GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, data.data(), ec);
}

void WebGLRenderingContext::texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                                          HTMLImageElement* image, ExceptionCode& ec)
{
    printWarningToConsole("Calling obsolete texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, HTMLImageElement image)");
    texSubImage2D(target, level, xoffset, yoffset, image, 0, 0, ec);
}

void WebGLRenderingContext::texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                                          HTMLImageElement* image, bool flipY, ExceptionCode& ec)
{
    printWarningToConsole("Calling obsolete texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, HTMLImageElement image, GLboolean flipY)");
    texSubImage2D(target, level, xoffset, yoffset, image, flipY, 0, ec);
}

void WebGLRenderingContext::texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                                          HTMLImageElement* image, bool flipY, bool premultiplyAlpha, ExceptionCode& ec)
{
    printWarningToConsole("Calling obsolete texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, HTMLImageElement image, GLboolean flipY, GLboolean premultiplyAlpha)");
    ec = 0;
    if (!image || !image->cachedImage()) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }
    texSubImage2DImpl(target, level, xoffset, yoffset, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, image->cachedImage()->image(),
                      flipY, premultiplyAlpha, ec);
}

void WebGLRenderingContext::texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                                          HTMLCanvasElement* canvas, ExceptionCode& ec)
{
    printWarningToConsole("Calling obsolete texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, HTMLCanvasElement canvas)");
    texSubImage2D(target, level, xoffset, yoffset, canvas, 0, 0, ec);
}

void WebGLRenderingContext::texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                                          HTMLCanvasElement* canvas, bool flipY, ExceptionCode& ec)
{
    printWarningToConsole("Calling obsolete texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, HTMLCanvasElement canvas, GLboolean flipY)");
    texSubImage2D(target, level, xoffset, yoffset, canvas, flipY, 0, ec);
}

void WebGLRenderingContext::texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                                          HTMLCanvasElement* canvas, bool flipY, bool premultiplyAlpha, ExceptionCode& ec)
{
    printWarningToConsole("Calling obsolete texSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, HTMLCanvasElement canvas, GLboolean flipY, GLboolean premultiplyAlpha)");
    ec = 0;
    if (!canvas || !canvas->buffer()) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }
    texSubImage2DImpl(target, level, xoffset, yoffset, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, canvas->buffer()->image(),
                      flipY, premultiplyAlpha, ec);
}

void WebGLRenderingContext::texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                                          HTMLVideoElement* video, ExceptionCode& ec)
{
    texSubImage2D(target, level, xoffset, yoffset, video, 0, 0, ec);
}

void WebGLRenderingContext::texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                                          HTMLVideoElement* video, bool flipY, ExceptionCode& ec)
{
    texSubImage2D(target, level, xoffset, yoffset, video, flipY, 0, ec);
}

void WebGLRenderingContext::texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                                          HTMLVideoElement* video, bool flipY, bool premultiplyAlpha, ExceptionCode& ec)
{
    // FIXME: Need to implement this call
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(xoffset);
    UNUSED_PARAM(yoffset);
    UNUSED_PARAM(video);
    UNUSED_PARAM(flipY);
    UNUSED_PARAM(premultiplyAlpha);
    ec = 0;
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform1f(const WebGLUniformLocation* location, float x, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!location)
        return;

    if (location->program() != m_currentProgram) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return;
    }

    m_context->uniform1f(location->location(), x);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform1fv(const WebGLUniformLocation* location, Float32Array* v, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateUniformParameters(location, v, 1))
        return;

    m_context->uniform1fv(location->location(), v->data(), v->length());
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform1fv(const WebGLUniformLocation* location, float* v, int size, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateUniformParameters(location, v, size, 1))
        return;

    m_context->uniform1fv(location->location(), v, size);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform1i(const WebGLUniformLocation* location, int x, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!location)
        return;

    if (location->program() != m_currentProgram) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return;
    }

    m_context->uniform1i(location->location(), x);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform1iv(const WebGLUniformLocation* location, Int32Array* v, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateUniformParameters(location, v, 1))
        return;

    m_context->uniform1iv(location->location(), v->data(), v->length());
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform1iv(const WebGLUniformLocation* location, int* v, int size, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateUniformParameters(location, v, size, 1))
        return;

    m_context->uniform1iv(location->location(), v, size);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform2f(const WebGLUniformLocation* location, float x, float y, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!location)
        return;

    if (location->program() != m_currentProgram) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return;
    }

    m_context->uniform2f(location->location(), x, y);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform2fv(const WebGLUniformLocation* location, Float32Array* v, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateUniformParameters(location, v, 2))
        return;

    m_context->uniform2fv(location->location(), v->data(), v->length() / 2);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform2fv(const WebGLUniformLocation* location, float* v, int size, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateUniformParameters(location, v, size, 2))
        return;

    m_context->uniform2fv(location->location(), v, size / 2);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform2i(const WebGLUniformLocation* location, int x, int y, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!location)
        return;

    if (location->program() != m_currentProgram) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return;
    }

    m_context->uniform2i(location->location(), x, y);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform2iv(const WebGLUniformLocation* location, Int32Array* v, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateUniformParameters(location, v, 2))
        return;

    m_context->uniform2iv(location->location(), v->data(), v->length() / 2);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform2iv(const WebGLUniformLocation* location, int* v, int size, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateUniformParameters(location, v, size, 2))
        return;

    m_context->uniform2iv(location->location(), v, size / 2);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform3f(const WebGLUniformLocation* location, float x, float y, float z, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!location)
        return;

    if (location->program() != m_currentProgram) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return;
    }

    m_context->uniform3f(location->location(), x, y, z);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform3fv(const WebGLUniformLocation* location, Float32Array* v, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateUniformParameters(location, v, 3))
        return;

    m_context->uniform3fv(location->location(), v->data(), v->length() / 3);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform3fv(const WebGLUniformLocation* location, float* v, int size, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateUniformParameters(location, v, size, 3))
        return;

    m_context->uniform3fv(location->location(), v, size / 3);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform3i(const WebGLUniformLocation* location, int x, int y, int z, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!location)
        return;

    if (location->program() != m_currentProgram) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return;
    }

    m_context->uniform3i(location->location(), x, y, z);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform3iv(const WebGLUniformLocation* location, Int32Array* v, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateUniformParameters(location, v, 3))
        return;

    m_context->uniform3iv(location->location(), v->data(), v->length() / 3);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform3iv(const WebGLUniformLocation* location, int* v, int size, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateUniformParameters(location, v, size, 3))
        return;

    m_context->uniform3iv(location->location(), v, size / 3);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform4f(const WebGLUniformLocation* location, float x, float y, float z, float w, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!location)
        return;

    if (location->program() != m_currentProgram) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return;
    }

    m_context->uniform4f(location->location(), x, y, z, w);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform4fv(const WebGLUniformLocation* location, Float32Array* v, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateUniformParameters(location, v, 4))
        return;

    m_context->uniform4fv(location->location(), v->data(), v->length() / 4);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform4fv(const WebGLUniformLocation* location, float* v, int size, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateUniformParameters(location, v, size, 4))
        return;

    m_context->uniform4fv(location->location(), v, size / 4);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform4i(const WebGLUniformLocation* location, int x, int y, int z, int w, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!location)
        return;

    if (location->program() != m_currentProgram) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return;
    }

    m_context->uniform4i(location->location(), x, y, z, w);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform4iv(const WebGLUniformLocation* location, Int32Array* v, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateUniformParameters(location, v, 4))
        return;

    m_context->uniform4iv(location->location(), v->data(), v->length() / 4);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniform4iv(const WebGLUniformLocation* location, int* v, int size, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateUniformParameters(location, v, size, 4))
        return;

    m_context->uniform4iv(location->location(), v, size / 4);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniformMatrix2fv(const WebGLUniformLocation* location, bool transpose, Float32Array* v, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateUniformMatrixParameters(location, transpose, v, 4))
        return;
    m_context->uniformMatrix2fv(location->location(), transpose, v->data(), v->length() / 4);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniformMatrix2fv(const WebGLUniformLocation* location, bool transpose, float* v, int size, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateUniformMatrixParameters(location, transpose, v, size, 4))
        return;
    m_context->uniformMatrix2fv(location->location(), transpose, v, size / 4);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniformMatrix3fv(const WebGLUniformLocation* location, bool transpose, Float32Array* v, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateUniformMatrixParameters(location, transpose, v, 9))
        return;
    m_context->uniformMatrix3fv(location->location(), transpose, v->data(), v->length() / 9);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniformMatrix3fv(const WebGLUniformLocation* location, bool transpose, float* v, int size, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateUniformMatrixParameters(location, transpose, v, size, 9))
        return;
    m_context->uniformMatrix3fv(location->location(), transpose, v, size / 9);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniformMatrix4fv(const WebGLUniformLocation* location, bool transpose, Float32Array* v, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateUniformMatrixParameters(location, transpose, v, 16))
        return;
    m_context->uniformMatrix4fv(location->location(), transpose, v->data(), v->length() / 16);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::uniformMatrix4fv(const WebGLUniformLocation* location, bool transpose, float* v, int size, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateUniformMatrixParameters(location, transpose, v, size, 16))
        return;
    m_context->uniformMatrix4fv(location->location(), transpose, v, size / 16);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::useProgram(WebGLProgram* program, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (program && program->context() != this) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return;
    }
    m_currentProgram = program;
    m_context->useProgram(program);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::validateProgram(WebGLProgram* program, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (!validateWebGLObject(program))
        return;
    m_context->validateProgram(program);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::vertexAttrib1f(unsigned long index, float v0)
{
    vertexAttribfImpl(index, 1, v0, 0.0f, 0.0f, 1.0f);
}

void WebGLRenderingContext::vertexAttrib1fv(unsigned long index, Float32Array* v)
{
    vertexAttribfvImpl(index, v, 1);
}

void WebGLRenderingContext::vertexAttrib1fv(unsigned long index, float* v, int size)
{
    vertexAttribfvImpl(index, v, size, 1);
}

void WebGLRenderingContext::vertexAttrib2f(unsigned long index, float v0, float v1)
{
    vertexAttribfImpl(index, 2, v0, v1, 0.0f, 1.0f);
}

void WebGLRenderingContext::vertexAttrib2fv(unsigned long index, Float32Array* v)
{
    vertexAttribfvImpl(index, v, 2);
}

void WebGLRenderingContext::vertexAttrib2fv(unsigned long index, float* v, int size)
{
    vertexAttribfvImpl(index, v, size, 2);
}

void WebGLRenderingContext::vertexAttrib3f(unsigned long index, float v0, float v1, float v2)
{
    vertexAttribfImpl(index, 3, v0, v1, v2, 1.0f);
}

void WebGLRenderingContext::vertexAttrib3fv(unsigned long index, Float32Array* v)
{
    vertexAttribfvImpl(index, v, 3);
}

void WebGLRenderingContext::vertexAttrib3fv(unsigned long index, float* v, int size)
{
    vertexAttribfvImpl(index, v, size, 3);
}

void WebGLRenderingContext::vertexAttrib4f(unsigned long index, float v0, float v1, float v2, float v3)
{
    vertexAttribfImpl(index, 4, v0, v1, v2, v3);
}

void WebGLRenderingContext::vertexAttrib4fv(unsigned long index, Float32Array* v)
{
    vertexAttribfvImpl(index, v, 4);
}

void WebGLRenderingContext::vertexAttrib4fv(unsigned long index, float* v, int size)
{
    vertexAttribfvImpl(index, v, size, 4);
}

void WebGLRenderingContext::vertexAttribPointer(unsigned long index, long size, unsigned long type, bool normalized, long stride, long offset, ExceptionCode& ec)
{
    if (index >= m_maxVertexAttribs) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }
    if (size < 1 || size > 4 || stride < 0 || offset < 0) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }
    if (!m_boundArrayBuffer) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return;
    }
    // Determine the number of elements the bound buffer can hold, given the offset, size, type and stride
    long bytesPerElement = size * sizeInBytes(type, ec);
    if (bytesPerElement <= 0)
        return;

    if (index >= m_vertexAttribState.size())
        m_vertexAttribState.resize(index + 1);

    long validatedStride = bytesPerElement;
    if (stride != 0) {
        if ((long) stride < bytesPerElement) {
            m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
            return;
        }
        validatedStride = stride;
    }
    m_vertexAttribState[index].bufferBinding = m_boundArrayBuffer;
    m_vertexAttribState[index].bytesPerElement = bytesPerElement;
    m_vertexAttribState[index].size = size;
    m_vertexAttribState[index].type = type;
    m_vertexAttribState[index].normalized = normalized;
    m_vertexAttribState[index].stride = validatedStride;
    m_vertexAttribState[index].originalStride = stride;
    m_vertexAttribState[index].offset = offset;
    m_context->vertexAttribPointer(index, size, type, normalized, stride, offset);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::viewport(long x, long y, unsigned long width, unsigned long height)
{
    if (isnan(x))
        x = 0;
    if (isnan(y))
        y = 0;
    if (isnan(width))
        width = 100;
    if (isnan(height))
        height = 100;
    m_context->viewport(x, y, width, height);
    cleanupAfterGraphicsCall(false);
}

void WebGLRenderingContext::removeObject(CanvasObject* object)
{
    m_canvasObjects.remove(object);
}

void WebGLRenderingContext::addObject(CanvasObject* object)
{
    removeObject(object);
    m_canvasObjects.add(object);
}

void WebGLRenderingContext::detachAndRemoveAllObjects()
{
    HashSet<RefPtr<CanvasObject> >::iterator pend = m_canvasObjects.end();
    for (HashSet<RefPtr<CanvasObject> >::iterator it = m_canvasObjects.begin(); it != pend; ++it)
        (*it)->detachContext();
        
    m_canvasObjects.clear();
}

WebGLTexture* WebGLRenderingContext::findTexture(Platform3DObject obj)
{
    if (!obj)
        return 0;
    HashSet<RefPtr<CanvasObject> >::iterator pend = m_canvasObjects.end();
    for (HashSet<RefPtr<CanvasObject> >::iterator it = m_canvasObjects.begin(); it != pend; ++it) {
        if ((*it)->isTexture() && (*it)->object() == obj)
            return reinterpret_cast<WebGLTexture*>((*it).get());
    }
    return 0;
}

WebGLRenderbuffer* WebGLRenderingContext::findRenderbuffer(Platform3DObject obj)
{
    if (!obj)
        return 0;
    HashSet<RefPtr<CanvasObject> >::iterator pend = m_canvasObjects.end();
    for (HashSet<RefPtr<CanvasObject> >::iterator it = m_canvasObjects.begin(); it != pend; ++it) {
        if ((*it)->isRenderbuffer() && (*it)->object() == obj)
            return reinterpret_cast<WebGLRenderbuffer*>((*it).get());
    }
    return 0;
}

WebGLBuffer* WebGLRenderingContext::findBuffer(Platform3DObject obj)
{
    if (!obj)
        return 0;
    HashSet<RefPtr<CanvasObject> >::iterator pend = m_canvasObjects.end();
    for (HashSet<RefPtr<CanvasObject> >::iterator it = m_canvasObjects.begin(); it != pend; ++it) {
        if ((*it)->isBuffer() && (*it)->object() == obj)
            return reinterpret_cast<WebGLBuffer*>((*it).get());
    }
    return 0;
}

WebGLShader* WebGLRenderingContext::findShader(Platform3DObject obj)
{
    if (!obj)
        return 0;
    HashSet<RefPtr<CanvasObject> >::iterator pend = m_canvasObjects.end();
    for (HashSet<RefPtr<CanvasObject> >::iterator it = m_canvasObjects.begin(); it != pend; ++it) {
        if ((*it)->isShader() && (*it)->object() == obj)
            return reinterpret_cast<WebGLShader*>((*it).get());
    }
    return 0;
}

WebGLGetInfo WebGLRenderingContext::getBooleanParameter(unsigned long pname)
{
    unsigned char value;
    m_context->getBooleanv(pname, &value);
    return WebGLGetInfo(static_cast<bool>(value));
}

WebGLGetInfo WebGLRenderingContext::getBooleanArrayParameter(unsigned long pname)
{
    if (pname != GraphicsContext3D::COLOR_WRITEMASK) {
        notImplemented();
        return WebGLGetInfo(0, 0);
    }
    unsigned char value[4] = {0};
    m_context->getBooleanv(pname, value);
    bool boolValue[4];
    for (int ii = 0; ii < 4; ++ii)
        boolValue[ii] = static_cast<bool>(value[ii]);
    return WebGLGetInfo(boolValue, 4);
}

WebGLGetInfo WebGLRenderingContext::getFloatParameter(unsigned long pname)
{
    float value;
    m_context->getFloatv(pname, &value);
    return WebGLGetInfo(static_cast<float>(value));
}

WebGLGetInfo WebGLRenderingContext::getIntParameter(unsigned long pname)
{
    return getLongParameter(pname);
}

WebGLGetInfo WebGLRenderingContext::getLongParameter(unsigned long pname)
{
    int value;
    m_context->getIntegerv(pname, &value);
    return WebGLGetInfo(static_cast<long>(value));
}

WebGLGetInfo WebGLRenderingContext::getUnsignedLongParameter(unsigned long pname)
{
    int value;
    m_context->getIntegerv(pname, &value);
    return WebGLGetInfo(static_cast<unsigned long>(value));
}

WebGLGetInfo WebGLRenderingContext::getWebGLFloatArrayParameter(unsigned long pname)
{
    float value[4] = {0};
    m_context->getFloatv(pname, value);
    unsigned length = 0;
    switch (pname) {
    case GraphicsContext3D::ALIASED_POINT_SIZE_RANGE:
    case GraphicsContext3D::ALIASED_LINE_WIDTH_RANGE:
    case GraphicsContext3D::DEPTH_RANGE:
        length = 2;
        break;
    case GraphicsContext3D::BLEND_COLOR:
    case GraphicsContext3D::COLOR_CLEAR_VALUE:
        length = 4;
        break;
    default:
        notImplemented();
    }
    return WebGLGetInfo(Float32Array::create(value, length));
}

WebGLGetInfo WebGLRenderingContext::getWebGLIntArrayParameter(unsigned long pname)
{
    int value[4] = {0};
    m_context->getIntegerv(pname, value);
    unsigned length = 0;
    switch (pname) {
    case GraphicsContext3D::MAX_VIEWPORT_DIMS:
        length = 2;
        break;
    case GraphicsContext3D::SCISSOR_BOX:
    case GraphicsContext3D::VIEWPORT:
        length = 4;
        break;
    default:
        notImplemented();
    }
    return WebGLGetInfo(Int32Array::create(value, length));
}

bool WebGLRenderingContext::isGLES2Compliant()
{
    return m_context->isGLES2Compliant();
}

void WebGLRenderingContext::handleNPOTTextures(bool prepareToDraw)
{
    bool resetActiveUnit = false;
    for (unsigned ii = 0; ii < m_textureUnits.size(); ++ii) {
        if (m_textureUnits[ii].m_texture2DBinding && m_textureUnits[ii].m_texture2DBinding->needToUseBlackTexture()
            || m_textureUnits[ii].m_textureCubeMapBinding && m_textureUnits[ii].m_textureCubeMapBinding->needToUseBlackTexture()) {
            if (ii != m_activeTextureUnit) {
                m_context->activeTexture(ii);
                resetActiveUnit = true;
            } else if (resetActiveUnit) {
                m_context->activeTexture(ii);
                resetActiveUnit = false;
            }
            WebGLTexture* tex2D;
            WebGLTexture* texCubeMap;
            if (prepareToDraw) {
                tex2D = m_blackTexture2D.get();
                texCubeMap = m_blackTextureCubeMap.get();
            } else {
                tex2D = m_textureUnits[ii].m_texture2DBinding.get();
                texCubeMap = m_textureUnits[ii].m_textureCubeMapBinding.get();
            }
            if (m_textureUnits[ii].m_texture2DBinding && m_textureUnits[ii].m_texture2DBinding->needToUseBlackTexture())
                m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, tex2D);
            if (m_textureUnits[ii].m_textureCubeMapBinding && m_textureUnits[ii].m_textureCubeMapBinding->needToUseBlackTexture())
                m_context->bindTexture(GraphicsContext3D::TEXTURE_CUBE_MAP, texCubeMap);
        }
    }
    if (resetActiveUnit)
        m_context->activeTexture(m_activeTextureUnit);
}

void WebGLRenderingContext::createFallbackBlackTextures1x1()
{
    unsigned char black[] = {0, 0, 0, 255};
    m_blackTexture2D = createTexture();
    m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, m_blackTexture2D.get());
    m_context->texImage2D(GraphicsContext3D::TEXTURE_2D, 0, GraphicsContext3D::RGBA, 1, 1,
                          0, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, black);
    m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, 0);
    m_blackTextureCubeMap = createTexture();
    m_context->bindTexture(GraphicsContext3D::TEXTURE_CUBE_MAP, m_blackTextureCubeMap.get());
    m_context->texImage2D(GraphicsContext3D::TEXTURE_CUBE_MAP_POSITIVE_X, 0, GraphicsContext3D::RGBA, 1, 1,
                          0, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, black);
    m_context->texImage2D(GraphicsContext3D::TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GraphicsContext3D::RGBA, 1, 1,
                          0, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, black);
    m_context->texImage2D(GraphicsContext3D::TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GraphicsContext3D::RGBA, 1, 1,
                          0, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, black);
    m_context->texImage2D(GraphicsContext3D::TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GraphicsContext3D::RGBA, 1, 1,
                          0, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, black);
    m_context->texImage2D(GraphicsContext3D::TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GraphicsContext3D::RGBA, 1, 1,
                          0, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, black);
    m_context->texImage2D(GraphicsContext3D::TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GraphicsContext3D::RGBA, 1, 1,
                          0, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, black);
    m_context->bindTexture(GraphicsContext3D::TEXTURE_CUBE_MAP, 0);
}

bool WebGLRenderingContext::isTexInternalFormatColorBufferCombinationValid(unsigned long texInternalFormat,
                                                                           unsigned long colorBufferFormat)
{
    switch (colorBufferFormat) {
    case GraphicsContext3D::ALPHA:
        if (texInternalFormat == GraphicsContext3D::ALPHA)
            return true;
        break;
    case GraphicsContext3D::RGB:
        if (texInternalFormat == GraphicsContext3D::LUMINANCE
            || texInternalFormat == GraphicsContext3D::RGB)
            return true;
        break;
    case GraphicsContext3D::RGBA:
        return true;
    }
    return false;
}

WebGLTexture* WebGLRenderingContext::getTextureBinding(unsigned long target)
{
    RefPtr<WebGLTexture> tex = 0;
    switch (target) {
    case GraphicsContext3D::TEXTURE_2D:
        tex = m_textureUnits[m_activeTextureUnit].m_texture2DBinding;
        break;
    case GraphicsContext3D::TEXTURE_CUBE_MAP_POSITIVE_X:
    case GraphicsContext3D::TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GraphicsContext3D::TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GraphicsContext3D::TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GraphicsContext3D::TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GraphicsContext3D::TEXTURE_CUBE_MAP_NEGATIVE_Z:
        tex = m_textureUnits[m_activeTextureUnit].m_textureCubeMapBinding;
        break;
    }
    if (tex && tex->object())
        return tex.get();
    return 0;
}

bool WebGLRenderingContext::validateTexFuncFormatAndType(unsigned long format, unsigned long type)
{
    switch (format) {
    case GraphicsContext3D::ALPHA:
    case GraphicsContext3D::LUMINANCE:
    case GraphicsContext3D::LUMINANCE_ALPHA:
    case GraphicsContext3D::RGB:
    case GraphicsContext3D::RGBA:
        break;
    default:
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return false;
    }

    switch (type) {
    case GraphicsContext3D::UNSIGNED_BYTE:
    case GraphicsContext3D::UNSIGNED_SHORT_5_6_5:
    case GraphicsContext3D::UNSIGNED_SHORT_4_4_4_4:
    case GraphicsContext3D::UNSIGNED_SHORT_5_5_5_1:
        break;
    default:
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return false;
    }

    // Verify that the combination of format and type is supported.
    switch (format) {
    case GraphicsContext3D::ALPHA:
    case GraphicsContext3D::LUMINANCE:
    case GraphicsContext3D::LUMINANCE_ALPHA:
        if (type != GraphicsContext3D::UNSIGNED_BYTE) {
            m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
            return false;
        }
        break;
    case GraphicsContext3D::RGB:
        if (type != GraphicsContext3D::UNSIGNED_BYTE
            && type != GraphicsContext3D::UNSIGNED_SHORT_5_6_5) {
            m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
            return false;
        }
        break;
    case GraphicsContext3D::RGBA:
        if (type != GraphicsContext3D::UNSIGNED_BYTE
            && type != GraphicsContext3D::UNSIGNED_SHORT_4_4_4_4
            && type != GraphicsContext3D::UNSIGNED_SHORT_5_5_5_1) {
            m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
            return false;
        }
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    return true;
}

bool WebGLRenderingContext::validateTexFuncParameters(unsigned long target, long level,
                                                      unsigned long internalformat,
                                                      long width, long height, long border,
                                                      unsigned long format, unsigned long type)
{
    // We absolutely have to validate the format and type combination.
    // The texImage2D entry points taking HTMLImage, etc. will produce
    // temporary data based on this combination, so it must be legal.
    if (!validateTexFuncFormatAndType(format, type))
        return false;

    if (width < 0 || height < 0 || level < 0) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return false;
    }

    switch (target) {
    case GraphicsContext3D::TEXTURE_2D:
        if (width > m_maxTextureSize || height > m_maxTextureSize || level > m_maxTextureLevel) {
            m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
            return false;
        }
        break;
    case GraphicsContext3D::TEXTURE_CUBE_MAP_POSITIVE_X:
    case GraphicsContext3D::TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GraphicsContext3D::TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GraphicsContext3D::TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GraphicsContext3D::TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GraphicsContext3D::TEXTURE_CUBE_MAP_NEGATIVE_Z:
        if (width != height || width > m_maxCubeMapTextureSize || level > m_maxCubeMapTextureLevel) {
            m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
            return false;
        }
        break;
    default:
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return false;
    }

    if (format != internalformat) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return false;
    }

    if (border) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return false;
    }

    return true;
}

bool WebGLRenderingContext::validateTexFuncData(long width, long height,
                                                unsigned long format, unsigned long type,
                                                ArrayBufferView* pixels)
{
    if (!pixels)
        return true;

    if (!validateTexFuncFormatAndType(format, type))
        return false;

    switch (type) {
    case GraphicsContext3D::UNSIGNED_BYTE:
        if (!pixels->isUnsignedByteArray()) {
            m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
            return false;
        }
        break;
    case GraphicsContext3D::UNSIGNED_SHORT_5_6_5:
    case GraphicsContext3D::UNSIGNED_SHORT_4_4_4_4:
    case GraphicsContext3D::UNSIGNED_SHORT_5_5_5_1:
        if (!pixels->isUnsignedShortArray()) {
            m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
            return false;
        }
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    unsigned long componentsPerPixel, bytesPerComponent;
    if (!m_context->computeFormatAndTypeParameters(format, type, &componentsPerPixel, &bytesPerComponent)) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return false;
    }

    if (!width || !height)
        return true;
    unsigned int validRowBytes = width * componentsPerPixel * bytesPerComponent;
    unsigned int totalRowBytes = validRowBytes;
    unsigned int remainder = validRowBytes % m_unpackAlignment;
    if (remainder)
        totalRowBytes += (m_unpackAlignment - remainder);
    unsigned int totalBytesRequired = (height - 1) * totalRowBytes + validRowBytes;
    if (pixels->byteLength() < totalBytesRequired) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return false;
    }
    return true;
}

bool WebGLRenderingContext::validateDrawMode(unsigned long mode)
{
    switch (mode) {
    case GraphicsContext3D::POINTS:
    case GraphicsContext3D::LINE_STRIP:
    case GraphicsContext3D::LINE_LOOP:
    case GraphicsContext3D::LINES:
    case GraphicsContext3D::TRIANGLE_STRIP:
    case GraphicsContext3D::TRIANGLE_FAN:
    case GraphicsContext3D::TRIANGLES:
        return true;
    default:
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return false;
    }
}

void WebGLRenderingContext::printWarningToConsole(const String& message)
{
    canvas()->document()->frame()->domWindow()->console()->addMessage(HTMLMessageSource, LogMessageType, WarningMessageLevel,
                                                                      message, 0, canvas()->document()->url().string());
}

bool WebGLRenderingContext::validateFramebufferFuncParameters(unsigned long target, unsigned long attachment)
{
    if (target != GraphicsContext3D::FRAMEBUFFER) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return false;
    }
    switch (attachment) {
    case GraphicsContext3D::COLOR_ATTACHMENT0:
    case GraphicsContext3D::DEPTH_ATTACHMENT:
    case GraphicsContext3D::STENCIL_ATTACHMENT:
    case GraphicsContext3D::DEPTH_STENCIL_ATTACHMENT:
        break;
    default:
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return false;
    }
    return true;
}

bool WebGLRenderingContext::validateBlendEquation(unsigned long mode)
{
    switch (mode) {
    case GraphicsContext3D::FUNC_ADD:
    case GraphicsContext3D::FUNC_SUBTRACT:
    case GraphicsContext3D::FUNC_REVERSE_SUBTRACT:
        return true;
    default:
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return false;
    }
}

bool WebGLRenderingContext::validateCapability(unsigned long cap)
{
    switch (cap) {
    case GraphicsContext3D::BLEND:
    case GraphicsContext3D::CULL_FACE:
    case GraphicsContext3D::DEPTH_TEST:
    case GraphicsContext3D::DITHER:
    case GraphicsContext3D::POLYGON_OFFSET_FILL:
    case GraphicsContext3D::SAMPLE_ALPHA_TO_COVERAGE:
    case GraphicsContext3D::SAMPLE_COVERAGE:
    case GraphicsContext3D::SCISSOR_TEST:
    case GraphicsContext3D::STENCIL_TEST:
        return true;
    default:
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return false;
    }
}

bool WebGLRenderingContext::validateUniformParameters(const WebGLUniformLocation* location, Float32Array* v, int requiredMinSize)
{
    if (!v) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return false;
    }
    return validateUniformMatrixParameters(location, false, v->data(), v->length(), requiredMinSize);
}

bool WebGLRenderingContext::validateUniformParameters(const WebGLUniformLocation* location, Int32Array* v, int requiredMinSize)
{
    if (!v) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return false;
    }
    return validateUniformMatrixParameters(location, false, v->data(), v->length(), requiredMinSize);
}

bool WebGLRenderingContext::validateUniformParameters(const WebGLUniformLocation* location, void* v, int size, int requiredMinSize)
{
    return validateUniformMatrixParameters(location, false, v, size, requiredMinSize);
}

bool WebGLRenderingContext::validateUniformMatrixParameters(const WebGLUniformLocation* location, bool transpose, Float32Array* v, int requiredMinSize)
{
    if (!v) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return false;
    }
    return validateUniformMatrixParameters(location, transpose, v->data(), v->length(), requiredMinSize);
}

bool WebGLRenderingContext::validateUniformMatrixParameters(const WebGLUniformLocation* location, bool transpose, void* v, int size, int requiredMinSize)
{
    if (!location)
        return false;
    if (location->program() != m_currentProgram) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return false;
    }
    if (!v) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return false;
    }
    if (transpose) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return false;
    }
    if (size < requiredMinSize) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return false;
    }
    return true;
}

WebGLBuffer* WebGLRenderingContext::validateBufferDataParameters(unsigned long target, unsigned long usage)
{
    WebGLBuffer* buffer = 0;
    switch (target) {
    case GraphicsContext3D::ELEMENT_ARRAY_BUFFER:
        buffer = m_boundElementArrayBuffer.get();
        break;
    case GraphicsContext3D::ARRAY_BUFFER:
        buffer = m_boundArrayBuffer.get();
        break;
    default:
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
        return 0;
    }
    if (!buffer) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return 0;
    }
    switch (usage) {
    case GraphicsContext3D::STREAM_DRAW:
    case GraphicsContext3D::STATIC_DRAW:
    case GraphicsContext3D::DYNAMIC_DRAW:
        return buffer;
    }
    m_context->synthesizeGLError(GraphicsContext3D::INVALID_ENUM);
    return 0;
}

void WebGLRenderingContext::vertexAttribfImpl(unsigned long index, int expectedSize, float v0, float v1, float v2, float v3)
{
    if (index >= m_maxVertexAttribs) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }
    // In GL, we skip setting vertexAttrib0 values.
    if (index || isGLES2Compliant()) {
        switch (expectedSize) {
        case 1:
            m_context->vertexAttrib1f(index, v0);
            break;
        case 2:
            m_context->vertexAttrib2f(index, v0, v1);
            break;
        case 3:
            m_context->vertexAttrib3f(index, v0, v1, v2);
            break;
        case 4:
            m_context->vertexAttrib4f(index, v0, v1, v2, v3);
            break;
        }
        cleanupAfterGraphicsCall(false);
    }
    if (index >= m_vertexAttribState.size())
        m_vertexAttribState.resize(index + 1);
    m_vertexAttribState[index].value[0] = v0;
    m_vertexAttribState[index].value[1] = v1;
    m_vertexAttribState[index].value[2] = v2;
    m_vertexAttribState[index].value[3] = v3;
}

void WebGLRenderingContext::vertexAttribfvImpl(unsigned long index, Float32Array* v, int expectedSize)
{
    if (!v) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }
    vertexAttribfvImpl(index, v->data(), v->length(), expectedSize);
}

void WebGLRenderingContext::vertexAttribfvImpl(unsigned long index, float* v, int size, int expectedSize)
{
    if (!v) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }
    if (size < expectedSize) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }
    if (index >= m_maxVertexAttribs) {
        m_context->synthesizeGLError(GraphicsContext3D::INVALID_VALUE);
        return;
    }
    // In GL, we skip setting vertexAttrib0 values.
    if (index || isGLES2Compliant()) {
        switch (expectedSize) {
        case 1:
            m_context->vertexAttrib1fv(index, v);
            break;
        case 2:
            m_context->vertexAttrib2fv(index, v);
            break;
        case 3:
            m_context->vertexAttrib3fv(index, v);
            break;
        case 4:
            m_context->vertexAttrib4fv(index, v);
            break;
        }
        cleanupAfterGraphicsCall(false);
    }
    if (index >= m_vertexAttribState.size())
        m_vertexAttribState.resize(index + 1);
    m_vertexAttribState[index].initValue();
    for (int ii = 0; ii < expectedSize; ++ii)
        m_vertexAttribState[index].value[ii] = v[ii];
}

void WebGLRenderingContext::initVertexAttrib0()
{
    m_vertexAttribState.resize(1);
    m_vertexAttrib0Buffer = createBuffer();
    m_context->bindBuffer(GraphicsContext3D::ARRAY_BUFFER, m_vertexAttrib0Buffer.get());
    m_context->bufferData(GraphicsContext3D::ARRAY_BUFFER, 0, GraphicsContext3D::DYNAMIC_DRAW);
    m_context->vertexAttribPointer(0, 4, GraphicsContext3D::FLOAT, false, 0, 0);
    m_vertexAttribState[0].bufferBinding = m_vertexAttrib0Buffer;
    m_context->bindBuffer(GraphicsContext3D::ARRAY_BUFFER, 0);
    m_context->enableVertexAttribArray(0);
    m_vertexAttrib0BufferSize = 0;
    m_vertexAttrib0BufferValue[0] = 0.0f;
    m_vertexAttrib0BufferValue[1] = 0.0f;
    m_vertexAttrib0BufferValue[2] = 0.0f;
    m_vertexAttrib0BufferValue[3] = 1.0f;
}

bool WebGLRenderingContext::simulateVertexAttrib0(long numVertex)
{
    const VertexAttribState& state = m_vertexAttribState[0];
    if (state.enabled || !m_currentProgram || !m_currentProgram->object()
        || !m_currentProgram->isUsingVertexAttrib0())
        return false;
    m_context->bindBuffer(GraphicsContext3D::ARRAY_BUFFER, m_vertexAttrib0Buffer.get());
    long bufferDataSize = (numVertex + 1) * 4 * sizeof(float);
    if (bufferDataSize > m_vertexAttrib0BufferSize
        || state.value[0] != m_vertexAttrib0BufferValue[0]
        || state.value[1] != m_vertexAttrib0BufferValue[1]
        || state.value[2] != m_vertexAttrib0BufferValue[2]
        || state.value[3] != m_vertexAttrib0BufferValue[3]) {
        RefPtr<Float32Array> bufferData = Float32Array::create((numVertex + 1) * 4);
        for (long ii = 0; ii < numVertex + 1; ++ii) {
            bufferData->set(ii * 4, state.value[0]);
            bufferData->set(ii * 4 + 1, state.value[1]);
            bufferData->set(ii * 4 + 2, state.value[2]);
            bufferData->set(ii * 4 + 3, state.value[3]);
        }
        m_context->bufferData(GraphicsContext3D::ARRAY_BUFFER, bufferData.get(), GraphicsContext3D::DYNAMIC_DRAW);
        m_vertexAttrib0BufferSize = bufferDataSize;
        m_vertexAttrib0BufferValue[0] = state.value[0];
        m_vertexAttrib0BufferValue[1] = state.value[1];
        m_vertexAttrib0BufferValue[2] = state.value[2];
        m_vertexAttrib0BufferValue[3] = state.value[3];
    }
    m_context->vertexAttribPointer(0, 4, GraphicsContext3D::FLOAT, false, 0, 0);
    return true;
}

void WebGLRenderingContext::restoreStatesAfterVertexAttrib0Simulation()
{
    const VertexAttribState& state = m_vertexAttribState[0];
    if (state.bufferBinding != m_vertexAttrib0Buffer) {
        m_context->bindBuffer(GraphicsContext3D::ARRAY_BUFFER, state.bufferBinding.get());
        m_context->vertexAttribPointer(0, state.size, state.type, state.normalized, state.originalStride, state.offset);
    }
    m_context->bindBuffer(GraphicsContext3D::ARRAY_BUFFER, m_boundArrayBuffer.get());
}

} // namespace WebCore

#endif // ENABLE(3D_CANVAS)
