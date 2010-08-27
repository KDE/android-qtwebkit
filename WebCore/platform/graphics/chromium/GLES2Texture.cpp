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

#include "GLES2Texture.h"

#include "GraphicsContext3D.h"
#include "IntRect.h"
#include <wtf/OwnArrayPtr.h>

namespace WebCore {


GLES2Texture::GLES2Texture(GraphicsContext3D* context, PassOwnPtr<Vector<unsigned int> > tileTextureIds, Format format, int width, int height, int maxTextureSize)
    : m_context(context)
    , m_format(format)
    , m_tiles(maxTextureSize, width, height, true)
    , m_tileTextureIds(tileTextureIds)
{
}

GLES2Texture::~GLES2Texture()
{
    for (unsigned int i = 0; i < m_tileTextureIds->size(); i++)
        m_context->deleteTexture(m_tileTextureIds->at(i));
}

static void convertFormat(GraphicsContext3D* context, GLES2Texture::Format format, unsigned int* glFormat, unsigned int* glType, bool* swizzle)
{
    *swizzle = false;
    switch (format) {
    case GLES2Texture::RGBA8:
        *glFormat = GraphicsContext3D::RGBA;
        *glType = GraphicsContext3D::UNSIGNED_BYTE;
        break;
    case GLES2Texture::BGRA8:
        if (context->supportsBGRA()) {
            *glFormat = GraphicsContext3D::BGRA_EXT;
            *glType = GraphicsContext3D::UNSIGNED_BYTE;
        } else {
            *glFormat = GraphicsContext3D::RGBA;
            *glType = GraphicsContext3D::UNSIGNED_BYTE;
            *swizzle = true;
        }
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

PassRefPtr<GLES2Texture> GLES2Texture::create(GraphicsContext3D* context, Format format, int width, int height)
{
    int maxTextureSize = 0;
    context->getIntegerv(GraphicsContext3D::MAX_TEXTURE_SIZE, &maxTextureSize);

    TilingData tiling(maxTextureSize, width, height, true);
    int numTiles = tiling.numTiles();

    OwnPtr<Vector<unsigned int> > textureIds(new Vector<unsigned int>(numTiles));
    textureIds->fill(0, numTiles);

    for (int i = 0; i < numTiles; i++) {
        int textureId = context->createTexture();
        if (!textureId) {
            for (int i = 0; i < numTiles; i++)
                context->deleteTexture(textureIds->at(i));
            return 0;
        }
        textureIds->at(i) = textureId;

        IntRect tileBoundsWithBorder = tiling.tileBoundsWithBorder(i);

    unsigned int glFormat = 0;
    unsigned int glType = 0;
    bool swizzle;
    convertFormat(context, format, &glFormat, &glType, &swizzle);
    context->bindTexture(GraphicsContext3D::TEXTURE_2D, textureId);
    context->texImage2D(GraphicsContext3D::TEXTURE_2D, 0, glFormat,
            tileBoundsWithBorder.width(),
            tileBoundsWithBorder.height(),
            0, glFormat, glType, 0);
    }
    return adoptRef(new GLES2Texture(context, textureIds.leakPtr(), format, width, height, maxTextureSize));
}

template <bool swizzle>
static uint32_t* copySubRect(uint32_t* src, int srcX, int srcY, uint32_t* dst, int width, int height, int srcStride)
{
    uint32_t* srcOffset = src + srcX + srcY * srcStride;

    if (!swizzle && width == srcStride)
        return srcOffset;

    uint32_t* dstPixel = dst;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width ; x++) {
            uint32_t pixel = srcOffset[x + y * srcStride];
            if (swizzle)
                *dstPixel = pixel & 0xFF00FF00 | ((pixel & 0x00FF0000) >> 16) | ((pixel & 0x000000FF) << 16);
            else
                *dstPixel = pixel;
            dstPixel++;
        }
    }
    return dst;
}

void GLES2Texture::load(void* pixels)
{
    uint32_t* pixels32 = static_cast<uint32_t*>(pixels);
    unsigned int glFormat = 0;
    unsigned int glType = 0;
    bool swizzle;
    convertFormat(m_context, m_format, &glFormat, &glType, &swizzle);
    if (swizzle) {
        ASSERT(glFormat == GraphicsContext3D::RGBA && glType == GraphicsContext3D::UNSIGNED_BYTE);
        // FIXME:  This could use PBO's to save doing an extra copy here.
    }
    OwnArrayPtr<uint32_t> tempBuff(new uint32_t[m_tiles.maxTextureSize() * m_tiles.maxTextureSize()]);

    for (int i = 0; i < m_tiles.numTiles(); i++) {
        IntRect tileBoundsWithBorder = m_tiles.tileBoundsWithBorder(i);

        uint32_t* uploadBuff = 0;
        if (swizzle) {
            uploadBuff = copySubRect<true>(
            pixels32, tileBoundsWithBorder.x(), tileBoundsWithBorder.y(),
            tempBuff.get(), tileBoundsWithBorder.width(), tileBoundsWithBorder.height(), m_tiles.totalSizeX());
        } else {
            uploadBuff = copySubRect<false>(
            pixels32, tileBoundsWithBorder.x(), tileBoundsWithBorder.y(),
            tempBuff.get(), tileBoundsWithBorder.width(), tileBoundsWithBorder.height(), m_tiles.totalSizeX());
        }

        m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, m_tileTextureIds->at(i));
        m_context->texSubImage2D(GraphicsContext3D::TEXTURE_2D, 0, 0, 0,
            tileBoundsWithBorder.width(),
            tileBoundsWithBorder.height(), glFormat, glType, uploadBuff);
    }
}

void GLES2Texture::bindTile(int tile)
{
    m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, m_tileTextureIds->at(tile));
    m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR);
    m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR);
    m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE);
    m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE);
}

}
