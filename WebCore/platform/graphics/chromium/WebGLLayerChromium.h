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


#ifndef WebGLLayerChromium_h
#define WebGLLayerChromium_h

#if USE(ACCELERATED_COMPOSITING)

#include "LayerChromium.h"

namespace WebCore {

class GraphicsContext3D;

// A Layer that contains a WebGL element.
class WebGLLayerChromium : public LayerChromium {
public:
    static PassRefPtr<WebGLLayerChromium> create(GraphicsLayerChromium* owner = 0);
    virtual bool drawsContent() { return m_context; }
    virtual bool ownsTexture() { return true; }
    virtual void updateTextureContents(unsigned);
    virtual unsigned textureId();
    virtual unsigned shaderProgramId() { return m_shaderProgramId; }

    void setContext(const GraphicsContext3D* context);

    static void setShaderProgramId(unsigned shaderProgramId) { m_shaderProgramId = shaderProgramId; }

private:
    WebGLLayerChromium(GraphicsLayerChromium* owner);
    GraphicsContext3D* m_context;
    unsigned m_textureId;
    bool m_textureChanged;

    static unsigned m_shaderProgramId;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
