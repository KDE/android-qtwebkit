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

#include "config.h"

#if ENABLE(3D_CANVAS)

#include "WebGraphicsContext3DDefaultImpl.h"

#include "app/gfx/gl/gl_bindings.h"
#include "app/gfx/gl/gl_context.h"
#include "NotImplemented.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/text/CString.h>

#include <stdio.h>
#include <string.h>

namespace WebKit {

enum {
    IMPLEMENTATION_COLOR_READ_FORMAT = 0x8B9B,
    IMPLEMENTATION_COLOR_READ_TYPE =  0x8B9A,
    MAX_VERTEX_UNIFORM_VECTORS = 0x8DFB,
    MAX_VARYING_VECTORS = 0x8DFC,
    MAX_FRAGMENT_UNIFORM_VECTORS = 0x8DFD
};

WebGraphicsContext3DDefaultImpl::VertexAttribPointerState::VertexAttribPointerState()
    : enabled(false)
    , buffer(0)
    , indx(0)
    , size(0)
    , type(0)
    , normalized(false)
    , stride(0)
    , offset(0)
{
}

WebGraphicsContext3DDefaultImpl::WebGraphicsContext3DDefaultImpl()
    : m_initialized(false)
    , m_texture(0)
    , m_fbo(0)
    , m_depthStencilBuffer(0)
    , m_multisampleFBO(0)
    , m_multisampleDepthStencilBuffer(0)
    , m_multisampleColorBuffer(0)
    , m_boundFBO(0)
#ifdef FLIP_FRAMEBUFFER_VERTICALLY
    , m_scanline(0)
#endif
    , m_boundArrayBuffer(0)
    , m_fragmentCompiler(0)
    , m_vertexCompiler(0)
{
}

WebGraphicsContext3DDefaultImpl::~WebGraphicsContext3DDefaultImpl()
{
    if (m_initialized) {
        makeContextCurrent();

        if (m_attributes.antialias) {
            glDeleteRenderbuffersEXT(1, &m_multisampleColorBuffer);
            if (m_attributes.depth || m_attributes.stencil)
                glDeleteRenderbuffersEXT(1, &m_multisampleDepthStencilBuffer);
            glDeleteFramebuffersEXT(1, &m_multisampleFBO);
        } else {
            if (m_attributes.depth || m_attributes.stencil)
                glDeleteRenderbuffersEXT(1, &m_depthStencilBuffer);
        }
        glDeleteTextures(1, &m_texture);
#ifdef FLIP_FRAMEBUFFER_VERTICALLY
        if (m_scanline)
            delete[] m_scanline;
#endif
        glDeleteFramebuffersEXT(1, &m_fbo);

        m_glContext->Destroy();

        angleDestroyCompilers();
    }
}

bool WebGraphicsContext3DDefaultImpl::initialize(WebGraphicsContext3D::Attributes attributes, WebView* webView, bool renderDirectlyToWebView)
{
    if (renderDirectlyToWebView) {
        // This mode isn't supported with the in-process implementation yet. (FIXME)
        return false;
    }

    if (!gfx::GLContext::InitializeOneOff())
        return false;

    m_glContext = WTF::adoptPtr(gfx::GLContext::CreateOffscreenGLContext(0));
    if (!m_glContext)
        return false;

    m_attributes = attributes;
    validateAttributes();

    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);

    if (!angleCreateCompilers()) {
        angleDestroyCompilers();
        return false;
    }

    m_initialized = true;
    return true;
}

void WebGraphicsContext3DDefaultImpl::validateAttributes()
{
    const char* extensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));

    if (m_attributes.stencil) {
        if (strstr(extensions, "GL_EXT_packed_depth_stencil")) {
            if (!m_attributes.depth)
                m_attributes.depth = true;
        } else
            m_attributes.stencil = false;
    }
    if (m_attributes.antialias) {
        bool isValidVendor = true;
#if PLATFORM(CG)
        // Currently in Mac we only turn on antialias if vendor is NVIDIA.
        const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
        if (!strstr(vendor, "NVIDIA"))
            isValidVendor = false;
#endif
        if (!isValidVendor || !strstr(extensions, "GL_EXT_framebuffer_multisample"))
            m_attributes.antialias = false;
    }
    // FIXME: instead of enforcing premultipliedAlpha = true, implement the
    // correct behavior when premultipliedAlpha = false is requested.
    m_attributes.premultipliedAlpha = true;
}

bool WebGraphicsContext3DDefaultImpl::makeContextCurrent()
{
    return m_glContext->MakeCurrent();
}

int WebGraphicsContext3DDefaultImpl::width()
{
    return m_cachedWidth;
}

int WebGraphicsContext3DDefaultImpl::height()
{
    return m_cachedHeight;
}

int WebGraphicsContext3DDefaultImpl::sizeInBytes(int type)
{
    switch (type) {
    case GL_BYTE:
        return sizeof(GLbyte);
    case GL_UNSIGNED_BYTE:
        return sizeof(GLubyte);
    case GL_SHORT:
        return sizeof(GLshort);
    case GL_UNSIGNED_SHORT:
        return sizeof(GLushort);
    case GL_INT:
        return sizeof(GLint);
    case GL_UNSIGNED_INT:
        return sizeof(GLuint);
    case GL_FLOAT:
        return sizeof(GLfloat);
    }
    return 0;
}

bool WebGraphicsContext3DDefaultImpl::isGLES2Compliant()
{
    return false;
}

bool WebGraphicsContext3DDefaultImpl::isGLES2NPOTStrict()
{
    return false;
}

bool WebGraphicsContext3DDefaultImpl::isErrorGeneratedOnOutOfBoundsAccesses()
{
    return false;
}

unsigned int WebGraphicsContext3DDefaultImpl::getPlatformTextureId()
{
    ASSERT_NOT_REACHED();
    return 0;
}

void WebGraphicsContext3DDefaultImpl::prepareTexture()
{
    ASSERT_NOT_REACHED();
}

static int createTextureObject(GLenum target)
{
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(target, texture);
    glTexParameterf(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return texture;
}

void WebGraphicsContext3DDefaultImpl::reshape(int width, int height)
{
    m_cachedWidth = width;
    m_cachedHeight = height;
    makeContextCurrent();

    GLenum target = GL_TEXTURE_2D;

    if (!m_texture) {
        // Generate the texture object
        m_texture = createTextureObject(target);
        // Generate the framebuffer object
        glGenFramebuffersEXT(1, &m_fbo);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
        m_boundFBO = m_fbo;
        if (m_attributes.depth || m_attributes.stencil)
            glGenRenderbuffersEXT(1, &m_depthStencilBuffer);
        // Generate the multisample framebuffer object
        if (m_attributes.antialias) {
            glGenFramebuffersEXT(1, &m_multisampleFBO);
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_multisampleFBO);
            m_boundFBO = m_multisampleFBO;
            glGenRenderbuffersEXT(1, &m_multisampleColorBuffer);
            if (m_attributes.depth || m_attributes.stencil)
                glGenRenderbuffersEXT(1, &m_multisampleDepthStencilBuffer);
        }
    }

    GLint internalColorFormat, colorFormat, internalDepthStencilFormat = 0;
    if (m_attributes.alpha) {
        internalColorFormat = GL_RGBA8;
        colorFormat = GL_RGBA;
    } else {
        internalColorFormat = GL_RGB8;
        colorFormat = GL_RGB;
    }
    if (m_attributes.stencil || m_attributes.depth) {
        // We don't allow the logic where stencil is required and depth is not.
        // See GraphicsContext3DInternal constructor.
        if (m_attributes.stencil && m_attributes.depth)
            internalDepthStencilFormat = GL_DEPTH24_STENCIL8_EXT;
        else
            internalDepthStencilFormat = GL_DEPTH_COMPONENT;
    }

    bool mustRestoreFBO = false;

    // Resize multisampling FBO
    if (m_attributes.antialias) {
        GLint maxSampleCount;
        glGetIntegerv(GL_MAX_SAMPLES_EXT, &maxSampleCount);
        GLint sampleCount = std::min(8, maxSampleCount);
        if (m_boundFBO != m_multisampleFBO) {
            mustRestoreFBO = true;
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_multisampleFBO);
        }
        glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_multisampleColorBuffer);
        glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, sampleCount, internalColorFormat, width, height);
        glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, m_multisampleColorBuffer);
        if (m_attributes.stencil || m_attributes.depth) {
            glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_multisampleDepthStencilBuffer);
            glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, sampleCount, internalDepthStencilFormat, width, height);
            if (m_attributes.stencil)
                glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_multisampleDepthStencilBuffer);
            if (m_attributes.depth)
                glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_multisampleDepthStencilBuffer);
        }
        glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
        GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
        if (status != GL_FRAMEBUFFER_COMPLETE_EXT) {
            printf("GraphicsContext3D: multisampling framebuffer was incomplete\n");

            // FIXME: cleanup.
            notImplemented();
        }
    }

    // Resize regular FBO
    if (m_boundFBO != m_fbo) {
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
        mustRestoreFBO = true;
    }
    glBindTexture(target, m_texture);
    glTexImage2D(target, 0, internalColorFormat, width, height, 0, colorFormat, GL_UNSIGNED_BYTE, 0);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, target, m_texture, 0);
    glBindTexture(target, 0);
    if (!m_attributes.antialias && (m_attributes.stencil || m_attributes.depth)) {
        glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_depthStencilBuffer);
        glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, internalDepthStencilFormat, width, height);
        if (m_attributes.stencil)
            glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_depthStencilBuffer);
        if (m_attributes.depth)
            glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_depthStencilBuffer);
        glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
    }
    GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    if (status != GL_FRAMEBUFFER_COMPLETE_EXT) {
        printf("WebGraphicsContext3DDefaultImpl: framebuffer was incomplete\n");

        // FIXME: cleanup.
        notImplemented();
    }

    if (m_attributes.antialias) {
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_multisampleFBO);
        if (m_boundFBO == m_multisampleFBO)
            mustRestoreFBO = false;
    }

    // Initialize renderbuffers to 0.
    GLboolean colorMask[] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE}, depthMask = GL_TRUE, stencilMask = GL_TRUE;
    GLboolean isScissorEnabled = GL_FALSE;
    GLboolean isDitherEnabled = GL_FALSE;
    GLbitfield clearMask = GL_COLOR_BUFFER_BIT;
    glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if (m_attributes.depth) {
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
        glDepthMask(GL_TRUE);
        clearMask |= GL_DEPTH_BUFFER_BIT;
    }
    if (m_attributes.stencil) {
        glGetBooleanv(GL_STENCIL_WRITEMASK, &stencilMask);
        glStencilMask(GL_TRUE);
        clearMask |= GL_STENCIL_BUFFER_BIT;
    }
    isScissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
    glDisable(GL_SCISSOR_TEST);
    isDitherEnabled = glIsEnabled(GL_DITHER);
    glDisable(GL_DITHER);

    glClear(clearMask);

    glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
    if (m_attributes.depth)
        glDepthMask(depthMask);
    if (m_attributes.stencil)
        glStencilMask(stencilMask);
    if (isScissorEnabled)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);
    if (isDitherEnabled)
        glEnable(GL_DITHER);
    else
        glDisable(GL_DITHER);

    if (mustRestoreFBO)
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_boundFBO);

#ifdef FLIP_FRAMEBUFFER_VERTICALLY
    if (m_scanline) {
        delete[] m_scanline;
        m_scanline = 0;
    }
    m_scanline = new unsigned char[width * 4];
#endif // FLIP_FRAMEBUFFER_VERTICALLY
}

#ifdef FLIP_FRAMEBUFFER_VERTICALLY
void WebGraphicsContext3DDefaultImpl::flipVertically(unsigned char* framebuffer,
                                                     unsigned int width,
                                                     unsigned int height)
{
    unsigned char* scanline = m_scanline;
    if (!scanline)
        return;
    unsigned int rowBytes = width * 4;
    unsigned int count = height / 2;
    for (unsigned int i = 0; i < count; i++) {
        unsigned char* rowA = framebuffer + i * rowBytes;
        unsigned char* rowB = framebuffer + (height - i - 1) * rowBytes;
        // FIXME: this is where the multiplication of the alpha
        // channel into the color buffer will need to occur if the
        // user specifies the "premultiplyAlpha" flag in the context
        // creation attributes.
        memcpy(scanline, rowB, rowBytes);
        memcpy(rowB, rowA, rowBytes);
        memcpy(rowA, scanline, rowBytes);
    }
}
#endif

bool WebGraphicsContext3DDefaultImpl::readBackFramebuffer(unsigned char* pixels, size_t bufferSize)
{
    if (bufferSize != static_cast<size_t>(4 * width() * height()))
        return false;

    makeContextCurrent();

    // Earlier versions of this code used the GPU to flip the
    // framebuffer vertically before reading it back for compositing
    // via software. This code was quite complicated, used a lot of
    // GPU memory, and didn't provide an obvious speedup. Since this
    // vertical flip is only a temporary solution anyway until Chrome
    // is fully GPU composited, it wasn't worth the complexity.

    bool mustRestoreFBO = false;
    if (m_attributes.antialias) {
        glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, m_multisampleFBO);
        glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, m_fbo);
        glBlitFramebufferEXT(0, 0, m_cachedWidth, m_cachedHeight, 0, 0, m_cachedWidth, m_cachedHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
        mustRestoreFBO = true;
    } else {
        if (m_boundFBO != m_fbo) {
            mustRestoreFBO = true;
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
        }
    }

    GLint packAlignment = 4;
    bool mustRestorePackAlignment = false;
    glGetIntegerv(GL_PACK_ALIGNMENT, &packAlignment);
    if (packAlignment > 4) {
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        mustRestorePackAlignment = true;
    }

    // FIXME: OpenGL ES 2 does not support GL_BGRA so this fails when
    // using that backend.
    glReadPixels(0, 0, m_cachedWidth, m_cachedHeight, GL_BGRA, GL_UNSIGNED_BYTE, pixels);

    if (mustRestorePackAlignment)
        glPixelStorei(GL_PACK_ALIGNMENT, packAlignment);

    if (mustRestoreFBO)
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_boundFBO);

#ifdef FLIP_FRAMEBUFFER_VERTICALLY
    if (pixels)
        flipVertically(pixels, m_cachedWidth, m_cachedHeight);
#endif

    return true;
}

void WebGraphicsContext3DDefaultImpl::synthesizeGLError(unsigned long error)
{
    m_syntheticErrors.add(error);
}

bool WebGraphicsContext3DDefaultImpl::supportsBGRA()
{
    // Supported since OpenGL 1.2. However, glTexImage2D() must be modified
    // to translate the internalFormat from GL_BGRA to GL_RGBA, since the
    // former is not accepted by desktop GL. Return false until this is done.
    return false;
}

bool WebGraphicsContext3DDefaultImpl::supportsMapSubCHROMIUM()
{
    // We don't claim support for this extension at this time
    return false;
}

void* WebGraphicsContext3DDefaultImpl::mapBufferSubDataCHROMIUM(unsigned target, int offset, int size, unsigned access)
{
    return 0;
}

void WebGraphicsContext3DDefaultImpl::unmapBufferSubDataCHROMIUM(const void* mem)
{
}

void* WebGraphicsContext3DDefaultImpl::mapTexSubImage2DCHROMIUM(unsigned target, int level, int xoffset, int yoffset, int width, int height, unsigned format, unsigned type, unsigned access)
{
    return 0;
}

void WebGraphicsContext3DDefaultImpl::unmapTexSubImage2DCHROMIUM(const void* mem)
{
}

bool WebGraphicsContext3DDefaultImpl::supportsCopyTextureToParentTextureCHROMIUM()
{
    // We don't claim support for this extension at this time
    return false;
}

void WebGraphicsContext3DDefaultImpl::copyTextureToParentTextureCHROMIUM(unsigned id, unsigned id2)
{
}

// Helper macros to reduce the amount of code.

#define DELEGATE_TO_GL(name, glname)                                           \
void WebGraphicsContext3DDefaultImpl::name()                                   \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname();                                                              \
}

#define DELEGATE_TO_GL_1(name, glname, t1)                                     \
void WebGraphicsContext3DDefaultImpl::name(t1 a1)                              \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname(a1);                                                            \
}

#define DELEGATE_TO_GL_1R(name, glname, t1, rt)                                \
rt WebGraphicsContext3DDefaultImpl::name(t1 a1)                                \
{                                                                              \
    makeContextCurrent();                                                      \
    return gl##glname(a1);                                                     \
}

#define DELEGATE_TO_GL_2(name, glname, t1, t2)                                 \
void WebGraphicsContext3DDefaultImpl::name(t1 a1, t2 a2)                       \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname(a1, a2);                                                        \
}

#define DELEGATE_TO_GL_2R(name, glname, t1, t2, rt)                            \
rt WebGraphicsContext3DDefaultImpl::name(t1 a1, t2 a2)                         \
{                                                                              \
    makeContextCurrent();                                                      \
    return gl##glname(a1, a2);                                                 \
}

#define DELEGATE_TO_GL_3(name, glname, t1, t2, t3)                             \
void WebGraphicsContext3DDefaultImpl::name(t1 a1, t2 a2, t3 a3)                \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname(a1, a2, a3);                                                    \
}

#define DELEGATE_TO_GL_4(name, glname, t1, t2, t3, t4)                         \
void WebGraphicsContext3DDefaultImpl::name(t1 a1, t2 a2, t3 a3, t4 a4)         \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname(a1, a2, a3, a4);                                                \
}

#define DELEGATE_TO_GL_5(name, glname, t1, t2, t3, t4, t5)                     \
void WebGraphicsContext3DDefaultImpl::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5)  \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname(a1, a2, a3, a4, a5);                                            \
}

#define DELEGATE_TO_GL_6(name, glname, t1, t2, t3, t4, t5, t6)                 \
void WebGraphicsContext3DDefaultImpl::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6) \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname(a1, a2, a3, a4, a5, a6);                                        \
}

#define DELEGATE_TO_GL_7(name, glname, t1, t2, t3, t4, t5, t6, t7)             \
void WebGraphicsContext3DDefaultImpl::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7) \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname(a1, a2, a3, a4, a5, a6, a7);                                    \
}

#define DELEGATE_TO_GL_8(name, glname, t1, t2, t3, t4, t5, t6, t7, t8)         \
void WebGraphicsContext3DDefaultImpl::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8) \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname(a1, a2, a3, a4, a5, a6, a7, a8);                                \
}

#define DELEGATE_TO_GL_9(name, glname, t1, t2, t3, t4, t5, t6, t7, t8, t9)     \
void WebGraphicsContext3DDefaultImpl::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8, t9 a9) \
{                                                                              \
    makeContextCurrent();                                                      \
    gl##glname(a1, a2, a3, a4, a5, a6, a7, a8, a9);                            \
}

void WebGraphicsContext3DDefaultImpl::activeTexture(unsigned long texture)
{
    // FIXME: query number of textures available.
    if (texture < GL_TEXTURE0 || texture > GL_TEXTURE0+32)
        // FIXME: raise exception.
        return;

    makeContextCurrent();
    glActiveTexture(texture);
}

DELEGATE_TO_GL_2(attachShader, AttachShader, WebGLId, WebGLId)

DELEGATE_TO_GL_3(bindAttribLocation, BindAttribLocation, WebGLId, unsigned long, const char*)

void WebGraphicsContext3DDefaultImpl::bindBuffer(unsigned long target, WebGLId buffer)
{
    makeContextCurrent();
    if (target == GL_ARRAY_BUFFER)
        m_boundArrayBuffer = buffer;
    glBindBuffer(target, buffer);
}

void WebGraphicsContext3DDefaultImpl::bindFramebuffer(unsigned long target, WebGLId framebuffer)
{
    makeContextCurrent();
    if (!framebuffer)
        framebuffer = (m_attributes.antialias ? m_multisampleFBO : m_fbo);
    if (framebuffer != m_boundFBO) {
        glBindFramebufferEXT(target, framebuffer);
        m_boundFBO = framebuffer;
    }
}

DELEGATE_TO_GL_2(bindRenderbuffer, BindRenderbufferEXT, unsigned long, WebGLId)

DELEGATE_TO_GL_2(bindTexture, BindTexture, unsigned long, WebGLId)

DELEGATE_TO_GL_4(blendColor, BlendColor, double, double, double, double)

DELEGATE_TO_GL_1(blendEquation, BlendEquation, unsigned long)

DELEGATE_TO_GL_2(blendEquationSeparate, BlendEquationSeparate, unsigned long, unsigned long)

DELEGATE_TO_GL_2(blendFunc, BlendFunc, unsigned long, unsigned long)

DELEGATE_TO_GL_4(blendFuncSeparate, BlendFuncSeparate, unsigned long, unsigned long, unsigned long, unsigned long)

DELEGATE_TO_GL_4(bufferData, BufferData, unsigned long, int, const void*, unsigned long)

DELEGATE_TO_GL_4(bufferSubData, BufferSubData, unsigned long, long, int, const void*)

DELEGATE_TO_GL_1R(checkFramebufferStatus, CheckFramebufferStatusEXT, unsigned long, unsigned long)

DELEGATE_TO_GL_1(clear, Clear, unsigned long)

DELEGATE_TO_GL_4(clearColor, ClearColor, double, double, double, double)

DELEGATE_TO_GL_1(clearDepth, ClearDepth, double)

DELEGATE_TO_GL_1(clearStencil, ClearStencil, long)

DELEGATE_TO_GL_4(colorMask, ColorMask, bool, bool, bool, bool)

void WebGraphicsContext3DDefaultImpl::compileShader(WebGLId shader)
{
    makeContextCurrent();

    ShaderSourceMap::iterator result = m_shaderSourceMap.find(shader);
    if (result == m_shaderSourceMap.end()) {
        // Passing down to gl driver to generate the correct error; or the case
        // where the shader deletion is delayed when it's attached to a program.
        glCompileShader(shader);
        return;
    }
    ShaderSourceEntry& entry = result->second;

    if (!angleValidateShaderSource(entry))
        return; // Shader didn't validate, don't move forward with compiling translated source

    int shaderLength = entry.translatedSource ? strlen(entry.translatedSource) : 0;
    glShaderSource(shader, 1, const_cast<const char**>(&entry.translatedSource), &shaderLength);
    glCompileShader(shader);

#ifndef NDEBUG
    int compileStatus;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);
    // ASSERT that ANGLE generated GLSL will be accepted by OpenGL
    ASSERT(compileStatus == GL_TRUE);
#endif
}

void WebGraphicsContext3DDefaultImpl::copyTexImage2D(unsigned long target, long level, unsigned long internalformat,
                                                     long x, long y, unsigned long width, unsigned long height, long border)
{
    makeContextCurrent();

    if (m_attributes.antialias && m_boundFBO == m_multisampleFBO) {
        glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, m_multisampleFBO);
        glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, m_fbo);
        glBlitFramebufferEXT(x, y, x + width, y + height, x, y, x + width, y + height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
    }

    glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);

    if (m_attributes.antialias && m_boundFBO == m_multisampleFBO)
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_boundFBO);
}

void WebGraphicsContext3DDefaultImpl::copyTexSubImage2D(unsigned long target, long level, long xoffset, long yoffset,
                                                        long x, long y, unsigned long width, unsigned long height)
{
    makeContextCurrent();

    if (m_attributes.antialias && m_boundFBO == m_multisampleFBO) {
        glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, m_multisampleFBO);
        glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, m_fbo);
        glBlitFramebufferEXT(x, y, x + width, y + height, x, y, x + width, y + height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
    }

    glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);

    if (m_attributes.antialias && m_boundFBO == m_multisampleFBO)
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_boundFBO);
}

DELEGATE_TO_GL_1(cullFace, CullFace, unsigned long)

DELEGATE_TO_GL_1(depthFunc, DepthFunc, unsigned long)

DELEGATE_TO_GL_1(depthMask, DepthMask, bool)

DELEGATE_TO_GL_2(depthRange, DepthRange, double, double)

DELEGATE_TO_GL_2(detachShader, DetachShader, WebGLId, WebGLId)

DELEGATE_TO_GL_1(disable, Disable, unsigned long)

void WebGraphicsContext3DDefaultImpl::disableVertexAttribArray(unsigned long index)
{
    makeContextCurrent();
    if (index < NumTrackedPointerStates)
        m_vertexAttribPointerState[index].enabled = false;
    glDisableVertexAttribArray(index);
}

DELEGATE_TO_GL_3(drawArrays, DrawArrays, unsigned long, long, long)

void WebGraphicsContext3DDefaultImpl::drawElements(unsigned long mode, unsigned long count, unsigned long type, long offset)
{
    makeContextCurrent();
    glDrawElements(mode, count, type,
                   reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
}

DELEGATE_TO_GL_1(enable, Enable, unsigned long)

void WebGraphicsContext3DDefaultImpl::enableVertexAttribArray(unsigned long index)
{
    makeContextCurrent();
    if (index < NumTrackedPointerStates)
        m_vertexAttribPointerState[index].enabled = true;
    glEnableVertexAttribArray(index);
}

DELEGATE_TO_GL(finish, Finish)

DELEGATE_TO_GL(flush, Flush)

void WebGraphicsContext3DDefaultImpl::framebufferRenderbuffer(unsigned long target, unsigned long attachment,
                                                              unsigned long renderbuffertarget, WebGLId buffer)
{
    makeContextCurrent();
    if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
        glFramebufferRenderbufferEXT(target, GL_DEPTH_ATTACHMENT, renderbuffertarget, buffer);
        glFramebufferRenderbufferEXT(target, GL_STENCIL_ATTACHMENT, renderbuffertarget, buffer);
    } else
        glFramebufferRenderbufferEXT(target, attachment, renderbuffertarget, buffer);
}

DELEGATE_TO_GL_5(framebufferTexture2D, FramebufferTexture2DEXT, unsigned long, unsigned long, unsigned long, WebGLId, long)

DELEGATE_TO_GL_1(frontFace, FrontFace, unsigned long)

void WebGraphicsContext3DDefaultImpl::generateMipmap(unsigned long target)
{
    makeContextCurrent();
    if (glGenerateMipmapEXT)
        glGenerateMipmapEXT(target);
    // FIXME: provide alternative code path? This will be unpleasant
    // to implement if glGenerateMipmapEXT is not available -- it will
    // require a texture readback and re-upload.
}

bool WebGraphicsContext3DDefaultImpl::getActiveAttrib(WebGLId program, unsigned long index, ActiveInfo& info)
{
    makeContextCurrent();
    if (!program) {
        synthesizeGLError(GL_INVALID_VALUE);
        return false;
    }
    GLint maxNameLength = -1;
    glGetProgramiv(program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxNameLength);
    if (maxNameLength < 0)
        return false;
    GLchar* name = 0;
    if (!tryFastMalloc(maxNameLength * sizeof(GLchar)).getValue(name)) {
        synthesizeGLError(GL_OUT_OF_MEMORY);
        return false;
    }
    GLsizei length = 0;
    GLint size = -1;
    GLenum type = 0;
    glGetActiveAttrib(program, index, maxNameLength,
                      &length, &size, &type, name);
    if (size < 0) {
        fastFree(name);
        return false;
    }
    info.name = WebString::fromUTF8(name, length);
    info.type = type;
    info.size = size;
    fastFree(name);
    return true;
}

bool WebGraphicsContext3DDefaultImpl::getActiveUniform(WebGLId program, unsigned long index, ActiveInfo& info)
{
    makeContextCurrent();
    GLint maxNameLength = -1;
    glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxNameLength);
    if (maxNameLength < 0)
        return false;
    GLchar* name = 0;
    if (!tryFastMalloc(maxNameLength * sizeof(GLchar)).getValue(name)) {
        synthesizeGLError(GL_OUT_OF_MEMORY);
        return false;
    }
    GLsizei length = 0;
    GLint size = -1;
    GLenum type = 0;
    glGetActiveUniform(program, index, maxNameLength,
                       &length, &size, &type, name);
    if (size < 0) {
        fastFree(name);
        return false;
    }
    info.name = WebString::fromUTF8(name, length);
    info.type = type;
    info.size = size;
    fastFree(name);
    return true;
}

DELEGATE_TO_GL_4(getAttachedShaders, GetAttachedShaders, WebGLId, int, int*, unsigned int*)

DELEGATE_TO_GL_2R(getAttribLocation, GetAttribLocation, WebGLId, const char*, int)

DELEGATE_TO_GL_2(getBooleanv, GetBooleanv, unsigned long, unsigned char*)

DELEGATE_TO_GL_3(getBufferParameteriv, GetBufferParameteriv, unsigned long, unsigned long, int*)

WebGraphicsContext3D::Attributes WebGraphicsContext3DDefaultImpl::getContextAttributes()
{
    return m_attributes;
}

unsigned long WebGraphicsContext3DDefaultImpl::getError()
{
    if (m_syntheticErrors.size() > 0) {
        ListHashSet<unsigned long>::iterator iter = m_syntheticErrors.begin();
        unsigned long err = *iter;
        m_syntheticErrors.remove(iter);
        return err;
    }

    makeContextCurrent();
    return glGetError();
}

DELEGATE_TO_GL_2(getFloatv, GetFloatv, unsigned long, float*)

void WebGraphicsContext3DDefaultImpl::getFramebufferAttachmentParameteriv(unsigned long target, unsigned long attachment,
                                                                          unsigned long pname, int* value)
{
    makeContextCurrent();
    if (attachment == GL_DEPTH_STENCIL_ATTACHMENT)
        attachment = GL_DEPTH_ATTACHMENT; // Or GL_STENCIL_ATTACHMENT, either works.
    glGetFramebufferAttachmentParameterivEXT(target, attachment, pname, value);
}

void WebGraphicsContext3DDefaultImpl::getIntegerv(unsigned long pname, int* value)
{
    // Need to emulate IMPLEMENTATION_COLOR_READ_FORMAT/TYPE for GL.  Any valid
    // combination should work, but GL_RGB/GL_UNSIGNED_BYTE might be the most
    // useful for desktop WebGL users.
    // Need to emulate MAX_FRAGMENT/VERTEX_UNIFORM_VECTORS and MAX_VARYING_VECTORS
    // because desktop GL's corresponding queries return the number of components
    // whereas GLES2 return the number of vectors (each vector has 4 components).
    // Therefore, the value returned by desktop GL needs to be divided by 4.
    makeContextCurrent();
    switch (pname) {
    case IMPLEMENTATION_COLOR_READ_FORMAT:
        *value = GL_RGB;
        break;
    case IMPLEMENTATION_COLOR_READ_TYPE:
        *value = GL_UNSIGNED_BYTE;
        break;
    case MAX_FRAGMENT_UNIFORM_VECTORS:
        glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, value);
        *value /= 4;
        break;
    case MAX_VERTEX_UNIFORM_VECTORS:
        glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, value);
        *value /= 4;
        break;
    case MAX_VARYING_VECTORS:
        glGetIntegerv(GL_MAX_VARYING_FLOATS, value);
        *value /= 4;
        break;
    default:
        glGetIntegerv(pname, value);
    }
}

DELEGATE_TO_GL_3(getProgramiv, GetProgramiv, WebGLId, unsigned long, int*)

WebString WebGraphicsContext3DDefaultImpl::getProgramInfoLog(WebGLId program)
{
    makeContextCurrent();
    GLint logLength;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
    if (!logLength)
        return WebString();
    GLchar* log = 0;
    if (!tryFastMalloc(logLength * sizeof(GLchar)).getValue(log))
        return WebString();
    GLsizei returnedLogLength;
    glGetProgramInfoLog(program, logLength, &returnedLogLength, log);
    ASSERT(logLength == returnedLogLength + 1);
    WebString res = WebString::fromUTF8(log, returnedLogLength);
    fastFree(log);
    return res;
}

DELEGATE_TO_GL_3(getRenderbufferParameteriv, GetRenderbufferParameterivEXT, unsigned long, unsigned long, int*)

void WebGraphicsContext3DDefaultImpl::getShaderiv(WebGLId shader, unsigned long pname, int* value)
{
    makeContextCurrent();

    ShaderSourceMap::iterator result = m_shaderSourceMap.find(shader);
    if (result != m_shaderSourceMap.end()) {
        ShaderSourceEntry& entry = result->second;
        switch (pname) {
        case GL_COMPILE_STATUS:
            if (!entry.isValid) {
                *value = 0;
                return;
            }
            break;
        case GL_INFO_LOG_LENGTH:
            if (!entry.isValid) {
                *value = entry.log ? strlen(entry.log) : 0;
                if (*value)
                    (*value)++;
                return;
            }
            break;
        case GL_SHADER_SOURCE_LENGTH:
            *value = entry.source ? strlen(entry.source) : 0;
            if (*value)
                (*value)++;
            return;
        }
    }

    glGetShaderiv(shader, pname, value);
}

WebString WebGraphicsContext3DDefaultImpl::getShaderInfoLog(WebGLId shader)
{
    makeContextCurrent();

    ShaderSourceMap::iterator result = m_shaderSourceMap.find(shader);
    if (result != m_shaderSourceMap.end()) {
        ShaderSourceEntry& entry = result->second;
        if (!entry.isValid) {
            if (!entry.log)
                return WebString();
            WebString res = WebString::fromUTF8(entry.log, strlen(entry.log));
            return res;
        }
    }

    GLint logLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength <= 1)
        return WebString();
    GLchar* log = 0;
    if (!tryFastMalloc(logLength * sizeof(GLchar)).getValue(log))
        return WebString();
    GLsizei returnedLogLength;
    glGetShaderInfoLog(shader, logLength, &returnedLogLength, log);
    ASSERT(logLength == returnedLogLength + 1);
    WebString res = WebString::fromUTF8(log, returnedLogLength);
    fastFree(log);
    return res;
}

WebString WebGraphicsContext3DDefaultImpl::getShaderSource(WebGLId shader)
{
    makeContextCurrent();

    ShaderSourceMap::iterator result = m_shaderSourceMap.find(shader);
    if (result != m_shaderSourceMap.end()) {
        ShaderSourceEntry& entry = result->second;
        if (!entry.source)
            return WebString();
        WebString res = WebString::fromUTF8(entry.source, strlen(entry.source));
        return res;
    }

    GLint logLength = 0;
    glGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &logLength);
    if (logLength <= 1)
        return WebString();
    GLchar* log = 0;
    if (!tryFastMalloc(logLength * sizeof(GLchar)).getValue(log))
        return WebString();
    GLsizei returnedLogLength;
    glGetShaderSource(shader, logLength, &returnedLogLength, log);
    ASSERT(logLength == returnedLogLength + 1);
    WebString res = WebString::fromUTF8(log, returnedLogLength);
    fastFree(log);
    return res;
}

WebString WebGraphicsContext3DDefaultImpl::getString(unsigned long name)
{
    makeContextCurrent();
    return WebString::fromUTF8(reinterpret_cast<const char*>(glGetString(name)));
}

DELEGATE_TO_GL_3(getTexParameterfv, GetTexParameterfv, unsigned long, unsigned long, float*)

DELEGATE_TO_GL_3(getTexParameteriv, GetTexParameteriv, unsigned long, unsigned long, int*)

DELEGATE_TO_GL_3(getUniformfv, GetUniformfv, WebGLId, long, float*)

DELEGATE_TO_GL_3(getUniformiv, GetUniformiv, WebGLId, long, int*)

DELEGATE_TO_GL_2R(getUniformLocation, GetUniformLocation, WebGLId, const char*, long)

DELEGATE_TO_GL_3(getVertexAttribfv, GetVertexAttribfv, unsigned long, unsigned long, float*)

DELEGATE_TO_GL_3(getVertexAttribiv, GetVertexAttribiv, unsigned long, unsigned long, int*)

long WebGraphicsContext3DDefaultImpl::getVertexAttribOffset(unsigned long index, unsigned long pname)
{
    makeContextCurrent();
    void* pointer;
    glGetVertexAttribPointerv(index, pname, &pointer);
    return reinterpret_cast<long>(pointer);
}

DELEGATE_TO_GL_2(hint, Hint, unsigned long, unsigned long)

DELEGATE_TO_GL_1R(isBuffer, IsBuffer, WebGLId, bool)

DELEGATE_TO_GL_1R(isEnabled, IsEnabled, unsigned long, bool)

DELEGATE_TO_GL_1R(isFramebuffer, IsFramebufferEXT, WebGLId, bool)

DELEGATE_TO_GL_1R(isProgram, IsProgram, WebGLId, bool)

DELEGATE_TO_GL_1R(isRenderbuffer, IsRenderbufferEXT, WebGLId, bool)

DELEGATE_TO_GL_1R(isShader, IsShader, WebGLId, bool)

DELEGATE_TO_GL_1R(isTexture, IsTexture, WebGLId, bool)

DELEGATE_TO_GL_1(lineWidth, LineWidth, double)

DELEGATE_TO_GL_1(linkProgram, LinkProgram, WebGLId)

DELEGATE_TO_GL_2(pixelStorei, PixelStorei, unsigned long, long)

DELEGATE_TO_GL_2(polygonOffset, PolygonOffset, double, double)

void WebGraphicsContext3DDefaultImpl::readPixels(long x, long y, unsigned long width, unsigned long height, unsigned long format, unsigned long type, void* pixels)
{
    makeContextCurrent();
    // FIXME: remove the two glFlush calls when the driver bug is fixed, i.e.,
    // all previous rendering calls should be done before reading pixels.
    glFlush();
    if (m_attributes.antialias && m_boundFBO == m_multisampleFBO) {
        glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, m_multisampleFBO);
        glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, m_fbo);
        glBlitFramebufferEXT(x, y, x + width, y + height, x, y, x + width, y + height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
        glFlush();
    }

    glReadPixels(x, y, width, height, format, type, pixels);

    if (m_attributes.antialias && m_boundFBO == m_multisampleFBO)
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_boundFBO);
}

void WebGraphicsContext3DDefaultImpl::releaseShaderCompiler()
{
}

void WebGraphicsContext3DDefaultImpl::renderbufferStorage(unsigned long target,
                                                          unsigned long internalformat,
                                                          unsigned long width,
                                                          unsigned long height)
{
    makeContextCurrent();
    switch (internalformat) {
    case GL_DEPTH_STENCIL:
        internalformat = GL_DEPTH24_STENCIL8_EXT;
        break;
    case GL_DEPTH_COMPONENT16:
        internalformat = GL_DEPTH_COMPONENT;
        break;
    case GL_RGBA4:
    case GL_RGB5_A1:
        internalformat = GL_RGBA;
        break;
    case 0x8D62: // GL_RGB565
        internalformat = GL_RGB;
        break;
    }
    glRenderbufferStorageEXT(target, internalformat, width, height);
}

DELEGATE_TO_GL_2(sampleCoverage, SampleCoverage, double, bool)

DELEGATE_TO_GL_4(scissor, Scissor, long, long, unsigned long, unsigned long)

void WebGraphicsContext3DDefaultImpl::shaderSource(WebGLId shader, const char* string)
{
    makeContextCurrent();
    GLint length = string ? strlen(string) : 0;
    ShaderSourceMap::iterator result = m_shaderSourceMap.find(shader);
    if (result != m_shaderSourceMap.end()) {
        ShaderSourceEntry& entry = result->second;
        if (entry.source) {
            fastFree(entry.source);
            entry.source = 0;
        }
        if (!tryFastMalloc((length + 1) * sizeof(char)).getValue(entry.source))
            return; // FIXME: generate an error?
        memcpy(entry.source, string, (length + 1) * sizeof(char));
    } else
        glShaderSource(shader, 1, &string, &length);
}

DELEGATE_TO_GL_3(stencilFunc, StencilFunc, unsigned long, long, unsigned long)

DELEGATE_TO_GL_4(stencilFuncSeparate, StencilFuncSeparate, unsigned long, unsigned long, long, unsigned long)

DELEGATE_TO_GL_1(stencilMask, StencilMask, unsigned long)

DELEGATE_TO_GL_2(stencilMaskSeparate, StencilMaskSeparate, unsigned long, unsigned long)

DELEGATE_TO_GL_3(stencilOp, StencilOp, unsigned long, unsigned long, unsigned long)

DELEGATE_TO_GL_4(stencilOpSeparate, StencilOpSeparate, unsigned long, unsigned long, unsigned long, unsigned long)

DELEGATE_TO_GL_9(texImage2D, TexImage2D, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, const void*)

DELEGATE_TO_GL_3(texParameterf, TexParameterf, unsigned, unsigned, float);

DELEGATE_TO_GL_3(texParameteri, TexParameteri, unsigned, unsigned, int);

DELEGATE_TO_GL_9(texSubImage2D, TexSubImage2D, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, const void*)

DELEGATE_TO_GL_2(uniform1f, Uniform1f, long, float)

DELEGATE_TO_GL_3(uniform1fv, Uniform1fv, long, int, float*)

DELEGATE_TO_GL_2(uniform1i, Uniform1i, long, int)

DELEGATE_TO_GL_3(uniform1iv, Uniform1iv, long, int, int*)

DELEGATE_TO_GL_3(uniform2f, Uniform2f, long, float, float)

DELEGATE_TO_GL_3(uniform2fv, Uniform2fv, long, int, float*)

DELEGATE_TO_GL_3(uniform2i, Uniform2i, long, int, int)

DELEGATE_TO_GL_3(uniform2iv, Uniform2iv, long, int, int*)

DELEGATE_TO_GL_4(uniform3f, Uniform3f, long, float, float, float)

DELEGATE_TO_GL_3(uniform3fv, Uniform3fv, long, int, float*)

DELEGATE_TO_GL_4(uniform3i, Uniform3i, long, int, int, int)

DELEGATE_TO_GL_3(uniform3iv, Uniform3iv, long, int, int*)

DELEGATE_TO_GL_5(uniform4f, Uniform4f, long, float, float, float, float)

DELEGATE_TO_GL_3(uniform4fv, Uniform4fv, long, int, float*)

DELEGATE_TO_GL_5(uniform4i, Uniform4i, long, int, int, int, int)

DELEGATE_TO_GL_3(uniform4iv, Uniform4iv, long, int, int*)

DELEGATE_TO_GL_4(uniformMatrix2fv, UniformMatrix2fv, long, int, bool, const float*)

DELEGATE_TO_GL_4(uniformMatrix3fv, UniformMatrix3fv, long, int, bool, const float*)

DELEGATE_TO_GL_4(uniformMatrix4fv, UniformMatrix4fv, long, int, bool, const float*)

DELEGATE_TO_GL_1(useProgram, UseProgram, WebGLId)

DELEGATE_TO_GL_1(validateProgram, ValidateProgram, WebGLId)

DELEGATE_TO_GL_2(vertexAttrib1f, VertexAttrib1f, unsigned long, float)

DELEGATE_TO_GL_2(vertexAttrib1fv, VertexAttrib1fv, unsigned long, const float*)

DELEGATE_TO_GL_3(vertexAttrib2f, VertexAttrib2f, unsigned long, float, float)

DELEGATE_TO_GL_2(vertexAttrib2fv, VertexAttrib2fv, unsigned long, const float*)

DELEGATE_TO_GL_4(vertexAttrib3f, VertexAttrib3f, unsigned long, float, float, float)

DELEGATE_TO_GL_2(vertexAttrib3fv, VertexAttrib3fv, unsigned long, const float*)

DELEGATE_TO_GL_5(vertexAttrib4f, VertexAttrib4f, unsigned long, float, float, float, float)

DELEGATE_TO_GL_2(vertexAttrib4fv, VertexAttrib4fv, unsigned long, const float*)

void WebGraphicsContext3DDefaultImpl::vertexAttribPointer(unsigned long indx, int size, int type, bool normalized,
                                                          unsigned long stride, unsigned long offset)
{
    makeContextCurrent();

    if (m_boundArrayBuffer <= 0) {
        // FIXME: raise exception.
        // LogMessagef(("bufferData: no buffer bound"));
        return;
    }

    if (indx < NumTrackedPointerStates) {
        VertexAttribPointerState& state = m_vertexAttribPointerState[indx];
        state.buffer = m_boundArrayBuffer;
        state.indx = indx;
        state.size = size;
        state.type = type;
        state.normalized = normalized;
        state.stride = stride;
        state.offset = offset;
    }

    glVertexAttribPointer(indx, size, type, normalized, stride,
                          reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
}

DELEGATE_TO_GL_4(viewport, Viewport, long, long, unsigned long, unsigned long)

unsigned WebGraphicsContext3DDefaultImpl::createBuffer()
{
    makeContextCurrent();
    GLuint o;
    glGenBuffersARB(1, &o);
    return o;
}

unsigned WebGraphicsContext3DDefaultImpl::createFramebuffer()
{
    makeContextCurrent();
    GLuint o = 0;
    glGenFramebuffersEXT(1, &o);
    return o;
}

unsigned WebGraphicsContext3DDefaultImpl::createProgram()
{
    makeContextCurrent();
    return glCreateProgram();
}

unsigned WebGraphicsContext3DDefaultImpl::createRenderbuffer()
{
    makeContextCurrent();
    GLuint o;
    glGenRenderbuffersEXT(1, &o);
    return o;
}

unsigned WebGraphicsContext3DDefaultImpl::createShader(unsigned long shaderType)
{
    makeContextCurrent();
    ASSERT(shaderType == GL_VERTEX_SHADER || shaderType == GL_FRAGMENT_SHADER);
    unsigned shader = glCreateShader(shaderType);
    if (shader) {
        ShaderSourceEntry entry;
        entry.type = shaderType;
        m_shaderSourceMap.set(shader, entry);
    }
    return shader;
}

unsigned WebGraphicsContext3DDefaultImpl::createTexture()
{
    makeContextCurrent();
    GLuint o;
    glGenTextures(1, &o);
    return o;
}

void WebGraphicsContext3DDefaultImpl::deleteBuffer(unsigned buffer)
{
    makeContextCurrent();
    glDeleteBuffersARB(1, &buffer);
}

void WebGraphicsContext3DDefaultImpl::deleteFramebuffer(unsigned framebuffer)
{
    makeContextCurrent();
    glDeleteFramebuffersEXT(1, &framebuffer);
}

void WebGraphicsContext3DDefaultImpl::deleteProgram(unsigned program)
{
    makeContextCurrent();
    glDeleteProgram(program);
}

void WebGraphicsContext3DDefaultImpl::deleteRenderbuffer(unsigned renderbuffer)
{
    makeContextCurrent();
    glDeleteRenderbuffersEXT(1, &renderbuffer);
}

void WebGraphicsContext3DDefaultImpl::deleteShader(unsigned shader)
{
    makeContextCurrent();
    glDeleteShader(shader);
    m_shaderSourceMap.remove(shader);
}

void WebGraphicsContext3DDefaultImpl::deleteTexture(unsigned texture)
{
    makeContextCurrent();
    glDeleteTextures(1, &texture);
}

bool WebGraphicsContext3DDefaultImpl::angleCreateCompilers()
{
    if (!ShInitialize())
        return false;

    TBuiltInResource resource;
    resource.MaxVertexAttribs = 0;
    getIntegerv(GL_MAX_VERTEX_ATTRIBS, &resource.MaxVertexAttribs);
    resource.MaxVertexUniformVectors = 0;
    getIntegerv(MAX_VERTEX_UNIFORM_VECTORS,
                &resource.MaxVertexUniformVectors);
    resource.MaxVaryingVectors = 0;
    getIntegerv(MAX_VARYING_VECTORS,
                &resource.MaxVaryingVectors);
    resource.MaxVertexTextureImageUnits = 0;
    getIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &resource.MaxVertexTextureImageUnits);
    resource.MaxCombinedTextureImageUnits = 0;
    getIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &resource.MaxCombinedTextureImageUnits);
    resource.MaxTextureImageUnits = 0;
    getIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &resource.MaxTextureImageUnits);
    resource.MaxFragmentUniformVectors = 0;
    getIntegerv(MAX_FRAGMENT_UNIFORM_VECTORS,
                &resource.MaxFragmentUniformVectors);
    // Always set to 1 for OpenGL ES.
    resource.MaxDrawBuffers = 1;

    m_fragmentCompiler = ShConstructCompiler(EShLangFragment, EShSpecWebGL, &resource);
    m_vertexCompiler = ShConstructCompiler(EShLangVertex, EShSpecWebGL, &resource);
    return (m_fragmentCompiler && m_vertexCompiler);
}

void WebGraphicsContext3DDefaultImpl::angleDestroyCompilers()
{
    if (m_fragmentCompiler) {
        ShDestruct(m_fragmentCompiler);
        m_fragmentCompiler = 0;
    }
    if (m_vertexCompiler) {
        ShDestruct(m_vertexCompiler);
        m_vertexCompiler = 0;
    }
}

bool WebGraphicsContext3DDefaultImpl::angleValidateShaderSource(ShaderSourceEntry& entry)
{
    entry.isValid = false;
    if (entry.translatedSource) {
        fastFree(entry.translatedSource);
        entry.translatedSource = 0;
    }
    if (entry.log) {
        fastFree(entry.log);
        entry.log = 0;
    }

    ShHandle compiler = 0;
    switch (entry.type) {
    case GL_FRAGMENT_SHADER:
        compiler = m_fragmentCompiler;
        break;
    case GL_VERTEX_SHADER:
        compiler = m_vertexCompiler;
        break;
    }
    if (!compiler)
        return false;

    if (!ShCompile(compiler, &entry.source, 1, EShOptObjectCode)) {
        int logSize = 0;
        ShGetInfo(compiler, SH_INFO_LOG_LENGTH, &logSize);
        if (logSize > 1 && tryFastMalloc(logSize * sizeof(char)).getValue(entry.log))
            ShGetInfoLog(compiler, entry.log);
        return false;
    }

    int length = 0;
    ShGetInfo(compiler, SH_OBJECT_CODE_LENGTH, &length);
    if (length > 1) {
        if (!tryFastMalloc(length * sizeof(char)).getValue(entry.translatedSource))
            return false;
        ShGetObjectCode(compiler, entry.translatedSource);
    }
    entry.isValid = true;
    return true;
}

} // namespace WebKit

#endif // ENABLE(3D_CANVAS)
