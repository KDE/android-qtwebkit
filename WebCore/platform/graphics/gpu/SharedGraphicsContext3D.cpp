/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
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

#include "config.h"

#if ENABLE(ACCELERATED_2D_CANVAS)

#include "SharedGraphicsContext3D.h"

#include "AffineTransform.h"
#include "Color.h"
#include "Extensions3D.h"
#include "FloatRect.h"
#include "IntSize.h"
#include "SolidFillShader.h"
#include "TexShader.h"

#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

// static
PassRefPtr<SharedGraphicsContext3D> SharedGraphicsContext3D::create(HostWindow* hostWindow)
{
    GraphicsContext3D::Attributes attr;
    attr.canRecoverFromContextLoss = false; // Canvas contexts can not handle lost contexts.
    RefPtr<GraphicsContext3D> context = GraphicsContext3D::create(attr, hostWindow);
    if (!context)
        return 0;
    OwnPtr<SolidFillShader> solidFillShader = SolidFillShader::create(context.get());
    if (!solidFillShader)
        return 0;
    OwnPtr<TexShader> texShader = TexShader::create(context.get());
    if (!texShader)
        return 0;
    return adoptRef(new SharedGraphicsContext3D(context.release(), solidFillShader.release(), texShader.release()));
}

SharedGraphicsContext3D::SharedGraphicsContext3D(PassRefPtr<GraphicsContext3D> context, PassOwnPtr<SolidFillShader> solidFillShader, PassOwnPtr<TexShader> texShader)
    : m_context(context)
    , m_bgraSupported(false)
    , m_quadVertices(0)
    , m_solidFillShader(solidFillShader)
    , m_texShader(texShader)
{
    allContexts()->add(this);
    Extensions3D* extensions = m_context->getExtensions();
    m_bgraSupported = extensions->supports("GL_EXT_texture_format_BGRA8888") && extensions->supports("GL_EXT_read_format_bgra");
    if (m_bgraSupported) {
        extensions->ensureEnabled("GL_EXT_texture_format_BGRA8888");
        extensions->ensureEnabled("GL_EXT_read_format_bgra");
    }
}

SharedGraphicsContext3D::~SharedGraphicsContext3D()
{
    m_context->deleteBuffer(m_quadVertices);
    allContexts()->remove(this);
}

void SharedGraphicsContext3D::makeContextCurrent()
{
    m_context->makeContextCurrent();
}

void SharedGraphicsContext3D::scissor(const FloatRect& rect)
{
    m_context->scissor(rect.x(), rect.y(), rect.width(), rect.height());
}

void SharedGraphicsContext3D::enable(GC3Denum capacity)
{
    m_context->enable(capacity);
}

void SharedGraphicsContext3D::disable(GC3Denum capacity)
{
    m_context->disable(capacity);
}

void SharedGraphicsContext3D::clearColor(const Color& color)
{
    float rgba[4];
    color.getRGBA(rgba[0], rgba[1], rgba[2], rgba[3]);
    m_context->clearColor(rgba[0], rgba[1], rgba[2], rgba[3]);
}

void SharedGraphicsContext3D::clear(GC3Dbitfield mask)
{
    m_context->clear(mask);
}

void SharedGraphicsContext3D::drawArrays(GC3Denum mode, GC3Dint first, GC3Dsizei count)
{
    m_context->drawArrays(mode, first, count);
}

GC3Denum SharedGraphicsContext3D::getError()
{
    return m_context->getError();
}

void SharedGraphicsContext3D::getIntegerv(GC3Denum pname, GC3Dint* value)
{
    m_context->getIntegerv(pname, value);
}

void SharedGraphicsContext3D::flush()
{
    m_context->flush();
}

Platform3DObject SharedGraphicsContext3D::createFramebuffer()
{
    return m_context->createFramebuffer();
}

Platform3DObject SharedGraphicsContext3D::createTexture()
{
    return m_context->createTexture();
}

void SharedGraphicsContext3D::deleteFramebuffer(Platform3DObject framebuffer)
{
    m_context->deleteFramebuffer(framebuffer);
}

void SharedGraphicsContext3D::deleteTexture(Platform3DObject texture)
{
    m_context->deleteTexture(texture);
}

void SharedGraphicsContext3D::framebufferTexture2D(GC3Denum target, GC3Denum attachment, GC3Denum textarget, Platform3DObject texture, GC3Dint level)
{
    m_context->framebufferTexture2D(target, attachment, textarget, texture, level);
}

void SharedGraphicsContext3D::texParameteri(GC3Denum target, GC3Denum pname, GC3Dint param)
{
    m_context->texParameteri(target, pname, param);
}

bool SharedGraphicsContext3D::texImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, const void* pixels)
{
    if (!pixels)
        return m_context->texImage2DResourceSafe(target, level, internalformat, width, height, border, format, type);
    return m_context->texImage2D(target, level, internalformat, width, height, border, format, type, pixels);
}

void SharedGraphicsContext3D::texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, const void* pixels)
{
    m_context->texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

void SharedGraphicsContext3D::readPixels(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, void* data)
{
    m_context->readPixels(x, y, width, height, format, type, data);
}

bool SharedGraphicsContext3D::supportsBGRA()
{
    return m_bgraSupported;
}

Texture* SharedGraphicsContext3D::createTexture(NativeImagePtr ptr, Texture::Format format, int width, int height)
{
    RefPtr<Texture> texture = m_textures.get(ptr);
    if (texture)
        return texture.get();

    texture = Texture::create(m_context.get(), format, width, height);
    Texture* t = texture.get();
    m_textures.set(ptr, texture);
    return t;
}

Texture* SharedGraphicsContext3D::getTexture(NativeImagePtr ptr)
{
    RefPtr<Texture> texture = m_textures.get(ptr);
    return texture ? texture.get() : 0;
}

void SharedGraphicsContext3D::removeTextureFor(NativeImagePtr ptr)
{
    TextureHashMap::iterator it = m_textures.find(ptr);
    if (it != m_textures.end())
        m_textures.remove(it);
}

// static
void SharedGraphicsContext3D::removeTexturesFor(NativeImagePtr ptr)
{
    for (HashSet<SharedGraphicsContext3D*>::iterator it = allContexts()->begin(); it != allContexts()->end(); ++it)
        (*it)->removeTextureFor(ptr);
}

// static
HashSet<SharedGraphicsContext3D*>* SharedGraphicsContext3D::allContexts()
{
    DEFINE_STATIC_LOCAL(HashSet<SharedGraphicsContext3D*>, allContextsSet, ());
    return &allContextsSet;
}


PassRefPtr<Texture> SharedGraphicsContext3D::createTexture(Texture::Format format, int width, int height)
{
    return Texture::create(m_context.get(), format, width, height);
}

void SharedGraphicsContext3D::applyCompositeOperator(CompositeOperator op)
{
    switch (op) {
    case CompositeClear:
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::ZERO, GraphicsContext3D::ZERO);
        break;
    case CompositeCopy:
        m_context->disable(GraphicsContext3D::BLEND);
        break;
    case CompositeSourceOver:
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::ONE, GraphicsContext3D::ONE_MINUS_SRC_ALPHA);
        break;
    case CompositeSourceIn:
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::DST_ALPHA, GraphicsContext3D::ZERO);
        break;
    case CompositeSourceOut:
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::ONE_MINUS_DST_ALPHA, GraphicsContext3D::ZERO);
        break;
    case CompositeSourceAtop:
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::DST_ALPHA, GraphicsContext3D::ONE_MINUS_SRC_ALPHA);
        break;
    case CompositeDestinationOver:
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::ONE_MINUS_DST_ALPHA, GraphicsContext3D::ONE);
        break;
    case CompositeDestinationIn:
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::ZERO, GraphicsContext3D::SRC_ALPHA);
        break;
    case CompositeDestinationOut:
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::ZERO, GraphicsContext3D::ONE_MINUS_SRC_ALPHA);
        break;
    case CompositeDestinationAtop:
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::ONE_MINUS_DST_ALPHA, GraphicsContext3D::SRC_ALPHA);
        break;
    case CompositeXOR:
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::ONE_MINUS_DST_ALPHA, GraphicsContext3D::ONE_MINUS_SRC_ALPHA);
        break;
    case CompositePlusDarker:
    case CompositeHighlight:
        // unsupported
        m_context->disable(GraphicsContext3D::BLEND);
        break;
    case CompositePlusLighter:
        m_context->enable(GraphicsContext3D::BLEND);
        m_context->blendFunc(GraphicsContext3D::ONE, GraphicsContext3D::ONE);
        break;
    }
}

void SharedGraphicsContext3D::useQuadVertices()
{
    if (!m_quadVertices) {
        float vertices[] = { 0.0f, 0.0f, 1.0f,
                             1.0f, 0.0f, 1.0f,
                             0.0f, 1.0f, 1.0f,
                             1.0f, 1.0f, 1.0f };
        m_quadVertices = m_context->createBuffer();
        m_context->bindBuffer(GraphicsContext3D::ARRAY_BUFFER, m_quadVertices);
        m_context->bufferData(GraphicsContext3D::ARRAY_BUFFER, sizeof(vertices), vertices, GraphicsContext3D::STATIC_DRAW);
    } else {
        m_context->bindBuffer(GraphicsContext3D::ARRAY_BUFFER, m_quadVertices);
    }
}

void SharedGraphicsContext3D::setActiveTexture(GC3Denum textureUnit)
{
    m_context->activeTexture(textureUnit);
}

void SharedGraphicsContext3D::bindTexture(GC3Denum target, Platform3DObject texture)
{
    m_context->bindTexture(target, texture);
}

void SharedGraphicsContext3D::useFillSolidProgram(const AffineTransform& transform, const Color& color)
{
    m_solidFillShader->use(transform, color);
}

void SharedGraphicsContext3D::useTextureProgram(const AffineTransform& transform, const AffineTransform& texTransform, float alpha)
{
    m_texShader->use(transform, texTransform, 0, alpha);
}

void SharedGraphicsContext3D::bindFramebuffer(Platform3DObject framebuffer)
{
    m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, framebuffer);
}

void SharedGraphicsContext3D::setViewport(const IntSize& size)
{
    m_context->viewport(0, 0, size.width(), size.height());
}

bool SharedGraphicsContext3D::paintsIntoCanvasBuffer() const
{
    return m_context->paintsIntoCanvasBuffer();
}

} // namespace WebCore

#endif
