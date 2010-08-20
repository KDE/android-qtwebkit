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

#if USE(ACCELERATED_COMPOSITING)

#include "CanvasLayerChromium.h"

#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"
#include <GLES2/gl2.h>

namespace WebCore {

CanvasLayerChromium::SharedValues::SharedValues()
    : m_canvasShaderProgram(0)
    , m_shaderSamplerLocation(-1)
    , m_shaderMatrixLocation(-1)
    , m_shaderAlphaLocation(-1)
    , m_initialized(false)
{
    char vertexShaderString[] =
        "attribute vec4 a_position;   \n"
        "attribute vec2 a_texCoord;   \n"
        "uniform mat4 matrix;         \n"
        "varying vec2 v_texCoord;     \n"
        "void main()                  \n"
        "{                            \n"
        "  gl_Position = matrix * a_position; \n"
        "  v_texCoord = a_texCoord;   \n"
        "}                            \n";

    // Canvas layers need to be flipped vertically and their colors shouldn't be
    // swizzled.
    char fragmentShaderString[] =
        "precision mediump float;                            \n"
        "varying vec2 v_texCoord;                            \n"
        "uniform sampler2D s_texture;                        \n"
        "uniform float alpha;                                \n"
        "void main()                                         \n"
        "{                                                   \n"
        "  vec4 texColor = texture2D(s_texture, vec2(v_texCoord.x, 1.0 - v_texCoord.y)); \n"
        "  gl_FragColor = vec4(texColor.x, texColor.y, texColor.z, texColor.w) * alpha; \n"
        "}                                                   \n";

    m_canvasShaderProgram = createShaderProgram(vertexShaderString, fragmentShaderString);
    if (!m_canvasShaderProgram) {
        LOG_ERROR("CanvasLayerChromium: Failed to create shader program");
        return;
    }

    m_shaderSamplerLocation = glGetUniformLocation(m_canvasShaderProgram, "s_texture");
    m_shaderMatrixLocation = glGetUniformLocation(m_canvasShaderProgram, "matrix");
    m_shaderAlphaLocation = glGetUniformLocation(m_canvasShaderProgram, "alpha");
    ASSERT(m_shaderSamplerLocation != -1);
    ASSERT(m_shaderMatrixLocation != -1);
    ASSERT(m_shaderAlphaLocation != -1);

    m_initialized = true;
}

CanvasLayerChromium::SharedValues::~SharedValues()
{
    if (m_canvasShaderProgram)
        GLC(glDeleteProgram(m_canvasShaderProgram));
}

PassRefPtr<CanvasLayerChromium> CanvasLayerChromium::create(GraphicsLayerChromium* owner)
{
    return adoptRef(new CanvasLayerChromium(owner));
}

CanvasLayerChromium::CanvasLayerChromium(GraphicsLayerChromium* owner)
    : LayerChromium(owner)
    , m_context(0)
    , m_textureId(0)
    , m_textureChanged(false)
{
}

void CanvasLayerChromium::updateContents()
{
    ASSERT(m_context);
    if (m_textureChanged) {
        glBindTexture(GL_TEXTURE_2D, m_textureId);
        // Set the min-mag filters to linear and wrap modes to GL_CLAMP_TO_EDGE
        // to get around NPOT texture limitations of GLES.
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        m_textureChanged = false;
    }
    // Update the contents of the texture used by the compositor.
    if (m_contentsDirty) {
        if (m_prepareTextureCallback)
            m_prepareTextureCallback->willPrepareTexture();
        m_context->prepareTexture();
        m_contentsDirty = false;
    }
}

void CanvasLayerChromium::setContext(const GraphicsContext3D* context)
{
    m_context = const_cast<GraphicsContext3D*>(context);

    unsigned int textureId = m_context->platformTexture();
    if (textureId != m_textureId)
        m_textureChanged = true;
    m_textureId = textureId;
}

void CanvasLayerChromium::draw()
{
    ASSERT(layerRenderer());
    const CanvasLayerChromium::SharedValues* sv = layerRenderer()->canvasLayerSharedValues();
    ASSERT(sv && sv->initialized());
    GLC(glActiveTexture(GL_TEXTURE0));
    GLC(glBindTexture(GL_TEXTURE_2D, m_textureId));
    layerRenderer()->useShader(sv->canvasShaderProgram());
    GLC(glUniform1i(sv->shaderSamplerLocation(), 0));
    drawTexturedQuad(layerRenderer()->projectionMatrix(), drawTransform(),
                     bounds().width(), bounds().height(), drawOpacity(),
                     sv->shaderMatrixLocation(), sv->shaderAlphaLocation());
}

}
#endif // USE(ACCELERATED_COMPOSITING)
