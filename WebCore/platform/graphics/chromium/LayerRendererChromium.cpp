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
#include "LayerRendererChromium.h"

#include "Canvas2DLayerChromium.h"
#include "GraphicsContext3D.h"
#include "LayerChromium.h"
#include "NotImplemented.h"
#include "WebGLLayerChromium.h"
#if PLATFORM(SKIA)
#include "NativeImageSkia.h"
#include "PlatformContextSkia.h"
#elif PLATFORM(CG)
#include <CoreGraphics/CGBitmapContext.h>
#endif

namespace WebCore {

static TransformationMatrix orthoMatrix(float left, float right, float bottom, float top, float nearZ, float farZ)
{
    float deltaX = right - left;
    float deltaY = top - bottom;
    float deltaZ = farZ - nearZ;
    TransformationMatrix ortho;
    if (!deltaX || !deltaY || !deltaZ)
        return ortho;
    ortho.setM11(2.0f / deltaX);
    ortho.setM41(-(right + left) / deltaX);
    ortho.setM22(2.0f / deltaY);
    ortho.setM42(-(top + bottom) / deltaY);
    ortho.setM33(-2.0f / deltaZ);
    ortho.setM43(-(nearZ + farZ) / deltaZ);
    return ortho;
}

static inline bool compareLayerZ(const LayerChromium* a, const LayerChromium* b)
{
    const TransformationMatrix& transformA = a->drawTransform();
    const TransformationMatrix& transformB = b->drawTransform();

    return transformA.m43() < transformB.m43();
}

PassRefPtr<LayerRendererChromium> LayerRendererChromium::create(PassRefPtr<GraphicsContext3D> context)
{
    if (!context)
        return 0;

    RefPtr<LayerRendererChromium> layerRenderer(adoptRef(new LayerRendererChromium(context)));
    if (!layerRenderer->hardwareCompositing())
        return 0;

    return layerRenderer.release();
}

LayerRendererChromium::LayerRendererChromium(PassRefPtr<GraphicsContext3D> context)
    : m_rootLayerTextureId(0)
    , m_rootLayerTextureWidth(0)
    , m_rootLayerTextureHeight(0)
    , m_scrollShaderProgram(0)
    , m_rootLayer(0)
    , m_scrollPosition(IntPoint(-1, -1))
    , m_currentShader(0)
    , m_context(context)
{
    m_hardwareCompositing = initializeSharedObjects();
}

LayerRendererChromium::~LayerRendererChromium()
{
    cleanupSharedObjects();
}

GraphicsContext3D* LayerRendererChromium::context()
{
    return m_context.get();
}

void LayerRendererChromium::debugGLCall(GraphicsContext3D* context, const char* command, const char* file, int line)
{
    unsigned long error = context->getError();
    if (error != GraphicsContext3D::NO_ERROR)
        LOG_ERROR("GL command failed: File: %s\n\tLine %d\n\tcommand: %s, error %x\n", file, line, command, static_cast<int>(error));
}

// Creates a canvas and an associated graphics context that the root layer will
// render into.
void LayerRendererChromium::setRootLayerCanvasSize(const IntSize& size)
{
    if (size == m_rootLayerCanvasSize)
        return;

#if PLATFORM(SKIA)
    // Create new canvas and context. OwnPtr takes care of freeing up
    // the old ones.
    m_rootLayerCanvas = new skia::PlatformCanvas(size.width(), size.height(), false);
    m_rootLayerSkiaContext = new PlatformContextSkia(m_rootLayerCanvas.get());
    m_rootLayerGraphicsContext = new GraphicsContext(reinterpret_cast<PlatformGraphicsContext*>(m_rootLayerSkiaContext.get()));
#elif PLATFORM(CG)
    // Release the previous CGBitmapContext before reallocating the backing store as a precaution.
    m_rootLayerCGContext.adoptCF(0);
    int rowBytes = 4 * size.width();
    m_rootLayerBackingStore.resize(rowBytes * size.height());
    memset(m_rootLayerBackingStore.data(), 0, m_rootLayerBackingStore.size());
    RetainPtr<CGColorSpaceRef> colorSpace(AdoptCF, CGColorSpaceCreateDeviceRGB());
    m_rootLayerCGContext.adoptCF(CGBitmapContextCreate(m_rootLayerBackingStore.data(),
                                                       size.width(), size.height(), 8, rowBytes,
                                                       colorSpace.get(),
                                                       kCGImageAlphaPremultipliedLast));
    CGContextTranslateCTM(m_rootLayerCGContext.get(), 0, size.height());
    CGContextScaleCTM(m_rootLayerCGContext.get(), 1, -1);
    m_rootLayerGraphicsContext = new GraphicsContext(m_rootLayerCGContext.get());
#else
#error "Need to implement for your platform."
#endif

    m_rootLayerCanvasSize = size;
}

void LayerRendererChromium::useShader(unsigned programId)
{
    if (programId != m_currentShader) {
        GLC(m_context, m_context->useProgram(programId));
        m_currentShader = programId;
    }
}

// This method must be called before any other updates are made to the
// root layer texture. It resizes the root layer texture and scrolls its
// contents as needed. It also sets up common GL state used by the rest
// of the layer drawing code.
void LayerRendererChromium::prepareToDrawLayers(const IntRect& visibleRect, const IntRect& contentRect, 
                                                const IntPoint& scrollPosition)
{
    ASSERT(m_hardwareCompositing);

    if (!m_rootLayer)
        return;

    makeContextCurrent();

    GLC(m_context, m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, m_rootLayerTextureId));
    
    bool skipScroll = false;

    // If the size of the visible area has changed then allocate a new texture
    // to store the contents of the root layer and adjust the projection matrix
    // and viewport.
    int visibleRectWidth = visibleRect.width();
    int visibleRectHeight = visibleRect.height();
    if (visibleRectWidth != m_rootLayerTextureWidth || visibleRectHeight != m_rootLayerTextureHeight) {
        m_rootLayerTextureWidth = visibleRect.width();
        m_rootLayerTextureHeight = visibleRect.height();

        m_projectionMatrix = orthoMatrix(0, visibleRectWidth, visibleRectHeight, 0, -1000, 1000);
        GLC(m_context, m_context->texImage2D(GraphicsContext3D::TEXTURE_2D, 0, GraphicsContext3D::RGBA, m_rootLayerTextureWidth, m_rootLayerTextureHeight, 0, GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, 0));

        // The root layer texture was just resized so its contents are not
        // useful for scrolling.
        skipScroll = true;
    }

    // The GL viewport covers the entire visible area, including the scrollbars.
    GLC(m_context, m_context->viewport(0, 0, visibleRectWidth, visibleRectHeight));

    // Bind the common vertex attributes used for drawing all the layers.
    LayerChromium::prepareForDraw(layerSharedValues());

    // FIXME: These calls can be made once, when the compositor context is initialized.
    GLC(m_context, m_context->disable(GraphicsContext3D::DEPTH_TEST));
    GLC(m_context, m_context->disable(GraphicsContext3D::CULL_FACE));
    GLC(m_context, m_context->depthFunc(GraphicsContext3D::LEQUAL));
    GLC(m_context, m_context->clearStencil(0));
    // Blending disabled by default. Root layer alpha channel on Windows is incorrect when Skia uses ClearType. 
    GLC(m_context, m_context->disable(GraphicsContext3D::BLEND)); 

    if (m_scrollPosition == IntPoint(-1, -1)) {
        m_scrollPosition = scrollPosition;
        skipScroll = true;
    }

    IntPoint scrollDelta = toPoint(scrollPosition - m_scrollPosition);

    // Scrolling larger than the contentRect size does not preserve any of the pixels, so there is
    // no need to copy framebuffer pixels back into the texture.
    if (abs(scrollDelta.y()) > contentRect.height() || abs(scrollDelta.x()) > contentRect.width())
        skipScroll = true;

    // Scroll the backbuffer
    if (!skipScroll && (scrollDelta.x() || scrollDelta.y())) {
        // Scrolling works as follows: We render a quad with the current root layer contents
        // translated by the amount the page has scrolled since the last update and then read the
        // pixels of the content area (visible area excluding the scroll bars) back into the
        // root layer texture. The newly exposed area will be filled by a subsequent drawLayersIntoRect call
        TransformationMatrix scrolledLayerMatrix;

        scrolledLayerMatrix.translate3d(0.5 * visibleRect.width() - scrollDelta.x(),
            0.5 * visibleRect.height() + scrollDelta.y(), 0);
        scrolledLayerMatrix.scale3d(1, -1, 1);

        useShader(m_scrollShaderProgram);
        GLC(m_context, m_context->uniform1i(m_scrollShaderSamplerLocation, 0));
        LayerChromium::drawTexturedQuad(m_context.get(), m_projectionMatrix, scrolledLayerMatrix,
                                        visibleRect.width(), visibleRect.height(), 1,
                                        m_scrollShaderMatrixLocation, -1);

        GLC(m_context, m_context->copyTexSubImage2D(GraphicsContext3D::TEXTURE_2D, 0, 0, 0, 0, 0, contentRect.width(), contentRect.height()));
    }

    m_scrollPosition = scrollPosition;
}

void LayerRendererChromium::updateRootLayerTextureRect(const IntRect& updateRect)
{
    ASSERT(m_hardwareCompositing);

    if (!m_rootLayer)
        return;

    GLC(m_context, m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, m_rootLayerTextureId));

    // Update the root layer texture.
    ASSERT((updateRect.right()  <= m_rootLayerTextureWidth)
           && (updateRect.bottom() <= m_rootLayerTextureHeight));

#if PLATFORM(SKIA)
    // Get the contents of the updated rect.
    const SkBitmap bitmap = m_rootLayerCanvas->getDevice()->accessBitmap(false);
    ASSERT(bitmap.width() == updateRect.width() && bitmap.height() == updateRect.height());
    void* pixels = bitmap.getPixels();
#elif PLATFORM(CG)
    // Get the contents of the updated rect.
    ASSERT(static_cast<int>(CGBitmapContextGetWidth(m_rootLayerCGContext.get())) == updateRect.width() && static_cast<int>(CGBitmapContextGetHeight(m_rootLayerCGContext.get())) == updateRect.height());
    void* pixels = m_rootLayerBackingStore.data();
#else
#error "Need to implement for your platform."
#endif
    // Copy the contents of the updated rect to the root layer texture.
    GLC(m_context, m_context->texSubImage2D(GraphicsContext3D::TEXTURE_2D, 0, updateRect.x(), updateRect.y(), updateRect.width(), updateRect.height(), GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, pixels));
}

void LayerRendererChromium::drawLayers(const IntRect& visibleRect, const IntRect& contentRect)
{
    ASSERT(m_hardwareCompositing);

    m_context->clearColor(0, 0, 1, 1);
    m_context->clear(GraphicsContext3D::COLOR_BUFFER_BIT | GraphicsContext3D::DEPTH_BUFFER_BIT);

    GLC(m_context, m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, m_rootLayerTextureId));

    // Render the root layer using a quad that takes up the entire visible area of the window.
    // We reuse the shader program used by ContentLayerChromium.
    const ContentLayerChromium::SharedValues* contentLayerValues = contentLayerSharedValues();
    useShader(contentLayerValues->contentShaderProgram());
    GLC(m_context, m_context->uniform1i(contentLayerValues->shaderSamplerLocation(), 0));
    // Mask out writes to alpha channel: ClearType via Skia results in invalid
    // zero alpha values on text glyphs. The root layer is always opaque.
    GLC(m_context, m_context->colorMask(true, true, true, false));
    TransformationMatrix layerMatrix;
    layerMatrix.translate3d(visibleRect.width() * 0.5f, visibleRect.height() * 0.5f, 0);
    LayerChromium::drawTexturedQuad(m_context.get(), m_projectionMatrix, layerMatrix,
                                    visibleRect.width(), visibleRect.height(), 1,
                                    contentLayerValues->shaderMatrixLocation(), contentLayerValues->shaderAlphaLocation());
    GLC(m_context, m_context->colorMask(true, true, true, true));

    // If culling is enabled then we will cull the backface.
    GLC(m_context, m_context->cullFace(GraphicsContext3D::BACK));
    // The orthographic projection is setup such that Y starts at zero and
    // increases going down the page so we need to adjust the winding order of
    // front facing triangles.
    GLC(m_context, m_context->frontFace(GraphicsContext3D::CW));

    // The shader used to render layers returns pre-multiplied alpha colors
    // so we need to send the blending mode appropriately.
    GLC(m_context, m_context->enable(GraphicsContext3D::BLEND));
    GLC(m_context, m_context->blendFunc(GraphicsContext3D::ONE, GraphicsContext3D::ONE_MINUS_SRC_ALPHA));

    // Set the root visible/content rects --- used by subsequent drawLayers calls.
    m_rootVisibleRect = visibleRect;
    m_rootContentRect = contentRect;

    // Traverse the layer tree and update the layer transforms.
    float opacity = 1;
    const Vector<RefPtr<LayerChromium> >& sublayers = m_rootLayer->getSublayers();
    size_t i;
    TransformationMatrix identityMatrix;
    for (i = 0; i < sublayers.size(); i++)
        updateLayersRecursive(sublayers[i].get(), identityMatrix, opacity);

    // Enable scissoring to avoid rendering composited layers over the scrollbars.
    GLC(m_context, m_context->enable(GraphicsContext3D::SCISSOR_TEST));
    IntRect scissorRect(contentRect);

    // The scissorRect should not include the scroll offset.
    scissorRect.move(-m_scrollPosition.x(), -m_scrollPosition.y());
    scissorToRect(scissorRect);

    // Clear the stencil buffer to 0.
    GLC(m_context, m_context->clear(GraphicsContext3D::STENCIL_BUFFER_BIT));
    // Disable writes to the stencil buffer.
    GLC(m_context, m_context->stencilMask(0));

    // Traverse the layer tree one more time to draw the layers.
    for (size_t i = 0; i < sublayers.size(); i++)
        drawLayersRecursive(sublayers[i].get());

    GLC(m_context, m_context->disable(GraphicsContext3D::SCISSOR_TEST));
    GLC(m_context, m_context->disable(GraphicsContext3D::BLEND));
}

void LayerRendererChromium::finish()
{
    m_context->finish();
}

void LayerRendererChromium::present()
{
    // We're done! Time to swapbuffers!

    // Note that currently this has the same effect as swapBuffers; we should
    // consider exposing a different entry point on GraphicsContext3D.
    m_context->prepareTexture();
}

void LayerRendererChromium::getFramebufferPixels(void *pixels, const IntRect& rect)
{
    ASSERT(rect.right() <= rootLayerTextureSize().width()
           && rect.bottom() <= rootLayerTextureSize().height());

    if (!pixels)
        return;

    makeContextCurrent();

    GLC(m_context, m_context->readPixels(rect.x(), rect.y(), rect.width(), rect.height(),
                                         GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, pixels));
}

// FIXME: This method should eventually be replaced by a proper texture manager.
unsigned LayerRendererChromium::createLayerTexture()
{
    unsigned textureId = 0;
    GLC(m_context, textureId = m_context->createTexture());
    GLC(m_context, m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, textureId));
    // Do basic linear filtering on resize.
    GLC(m_context, m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR));
    GLC(m_context, m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR));
    // NPOT textures in GL ES only work when the wrap mode is set to GraphicsContext3D::CLAMP_TO_EDGE.
    GLC(m_context, m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE));
    GLC(m_context, m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE));
    return textureId;
}

void LayerRendererChromium::deleteLayerTexture(unsigned textureId)
{
    if (!textureId)
        return;

    GLC(m_context, m_context->deleteTexture(textureId));
}

// Returns true if any part of the layer falls within the visibleRect
bool LayerRendererChromium::isLayerVisible(LayerChromium* layer, const TransformationMatrix& matrix, const IntRect& visibleRect)
{
    // Form the matrix used by the shader to map the corners of the layer's
    // bounds into clip space.
    TransformationMatrix renderMatrix = matrix;
    renderMatrix.scale3d(layer->bounds().width(), layer->bounds().height(), 1);
    renderMatrix.multiply(m_projectionMatrix);

    FloatRect layerRect(-0.5, -0.5, 1, 1);
    FloatRect mappedRect = renderMatrix.mapRect(layerRect);

    // The layer is visible if it intersects any part of a rectangle whose origin
    // is at (-1, -1) and size is 2x2.
    return mappedRect.intersects(FloatRect(-1, -1, 2, 2));
}

// Recursively walks the layer tree starting at the given node and updates the 
// transform and opacity values.
void LayerRendererChromium::updateLayersRecursive(LayerChromium* layer, const TransformationMatrix& parentMatrix, float opacity)
{
    // Compute the new matrix transformation that will be applied to this layer and
    // all its sublayers. It's important to remember that the layer's position
    // is the position of the layer's anchor point. Also, the coordinate system used
    // assumes that the origin is at the lower left even though the coordinates the browser
    // gives us for the layers are for the upper left corner. The Y flip happens via
    // the orthographic projection applied at render time.
    // The transformation chain for the layer is (using the Matrix x Vector order):
    // M = M[p] * Tr[l] * M[l] * Tr[c]
    // Where M[p] is the parent matrix passed down to the function
    //       Tr[l] is the translation matrix locating the layer's anchor point
    //       Tr[c] is the translation offset between the anchor point and the center of the layer
    //       M[l] is the layer's matrix (applied at the anchor point)
    // This transform creates a coordinate system whose origin is the center of the layer.
    // Note that the final matrix used by the shader for the layer is P * M * S . This final product
    // is computed in drawTexturedQuad().
    // Where: P is the projection matrix
    //        M is the layer's matrix computed above
    //        S is the scale adjustment (to scale up to the layer size)
    IntSize bounds = layer->bounds();
    FloatPoint anchorPoint = layer->anchorPoint();
    FloatPoint position = layer->position();

    // Offset between anchor point and the center of the quad.
    float centerOffsetX = (0.5 - anchorPoint.x()) * bounds.width();
    float centerOffsetY = (0.5 - anchorPoint.y()) * bounds.height();

    // M = M[p]
    TransformationMatrix localMatrix = parentMatrix;
    // M = M[p] * Tr[l]
    localMatrix.translate3d(position.x(), position.y(), layer->anchorPointZ());
    // M = M[p] * Tr[l] * M[l]
    localMatrix.multLeft(layer->transform());
    // M = M[p] * Tr[l] * M[l] * Tr[c]
    localMatrix.translate3d(centerOffsetX, centerOffsetY, -layer->anchorPointZ());

    // Calculate the layer's opacity.
    opacity *= layer->opacity();

    layer->setDrawTransform(localMatrix);
    layer->setDrawOpacity(opacity);

    // Flatten to 2D if the layer doesn't preserve 3D.
    if (!layer->preserves3D()) {
        localMatrix.setM13(0);
        localMatrix.setM23(0);
        localMatrix.setM31(0);
        localMatrix.setM32(0);
        localMatrix.setM33(1);
        localMatrix.setM34(0);
        localMatrix.setM43(0);
    }

    // Apply the sublayer transform at the center of the layer.
    localMatrix.multLeft(layer->sublayerTransform());

    // The origin of the sublayers is actually the bottom left corner of the layer
    // (or top left when looking it it from the browser's pespective) instead of the center.
    // The matrix passed down to the sublayers is therefore:
    // M[s] = M * Tr[-center]
    localMatrix.translate3d(-bounds.width() * 0.5, -bounds.height() * 0.5, 0);

    const Vector<RefPtr<LayerChromium> >& sublayers = layer->getSublayers();
    for (size_t i = 0; i < sublayers.size(); i++)
        updateLayersRecursive(sublayers[i].get(), localMatrix, opacity);

    layer->setLayerRenderer(this);
}

// Does a quick draw of the given layer into the stencil buffer. If decrement
// is true then it decrements the current stencil values otherwise it increments them.
void LayerRendererChromium::drawLayerIntoStencilBuffer(LayerChromium* layer, bool decrement)
{
    // Enable writes to the stencil buffer and increment the stencil values
    // by one for every pixel under the current layer.
    GLC(m_context, m_context->stencilMask(0xff));
    GLC(m_context, m_context->stencilFunc(GraphicsContext3D::ALWAYS, 1, 0xff));
    unsigned stencilOp = (decrement ? GraphicsContext3D::DECR : GraphicsContext3D::INCR);
    GLC(m_context, m_context->stencilOp(stencilOp, stencilOp, stencilOp));

    GLC(m_context, m_context->colorMask(false, false, false, false));

    layer->drawAsMask();

    // Disable writes to the stencil buffer.
    GLC(m_context, m_context->stencilMask(0));
    GLC(m_context, m_context->colorMask(true, true, true, true));
}

// Recursively walk the layer tree and draw the layers.
void LayerRendererChromium::drawLayersRecursive(LayerChromium* layer)
{
    static bool depthTestEnabledForSubtree = false;
    static int currentStencilValue = 0;

    // Check if the layer falls within the visible bounds of the page.
    IntRect layerRect = layer->getDrawRect();
    bool isLayerVisible = m_currentScissorRect.intersects(layerRect);

    // Enable depth testing for this layer and all its descendants if preserves3D is set.
    bool mustClearDepth = false;
    if (layer->preserves3D()) {
        if (!depthTestEnabledForSubtree) {
            GLC(m_context, m_context->enable(GraphicsContext3D::DEPTH_TEST));
            depthTestEnabledForSubtree = true;

            // Need to clear the depth buffer when we're done rendering this subtree.
            mustClearDepth = true;
        }
    }

    if (isLayerVisible)
        drawLayer(layer);

    // FIXME: We should check here if the layer has descendants that draw content
    // before we setup for clipping.
    IntRect previousScissorRect = m_currentScissorRect;
    bool mustResetScissorRect = false;
    bool didStencilDraw = false;
    if (layer->masksToBounds()) {
        // If the layer isn't rotated then we can use scissoring otherwise we need
        // to clip using the stencil buffer.
        if (layer->drawTransform().isIdentityOrTranslation()) {
            IntRect currentScissorRect = previousScissorRect;
            currentScissorRect.intersect(layerRect);
            if (currentScissorRect != previousScissorRect) {
                scissorToRect(currentScissorRect);
                mustResetScissorRect = true;
            }
        } else if (currentStencilValue < ((1 << m_numStencilBits) - 1)) {
            // Clipping using the stencil buffer works as follows: When we encounter
            // a clipping layer we increment the stencil buffer values for all the pixels
            // the layer touches. As a result 1's will be stored in the stencil buffer for pixels under
            // the first clipping layer found in a traversal, 2's for pixels in the intersection
            // of two nested clipping layers, etc. When the sublayers of a clipping layer are drawn
            // we turn on stencil testing to render only pixels that have the correct stencil
            // value (one that matches the value of currentStencilValue). As the recursion unravels,
            // we decrement the stencil buffer values for each clipping layer. When the entire layer tree
            // is rendered, the stencil values should be all back to zero. An 8 bit stencil buffer
            // will allow us up to 255 nested clipping layers which is hopefully enough.
            if (!currentStencilValue)
                GLC(m_context, m_context->enable(GraphicsContext3D::STENCIL_TEST));

            drawLayerIntoStencilBuffer(layer, false);

            currentStencilValue++;
            didStencilDraw = true;
        }
    }
    // Sublayers will render only if the value in the stencil buffer is equal to
    // currentStencilValue.
    if (didStencilDraw) {
        // The sublayers will render only if the stencil test passes.
        GLC(m_context, m_context->stencilFunc(GraphicsContext3D::EQUAL, currentStencilValue, 0xff));
    }

    // If we're using depth testing then we need to sort the children in Z to
    // get the transparency to work properly.
    if (depthTestEnabledForSubtree) {
        const Vector<RefPtr<LayerChromium> >& sublayers = layer->getSublayers();
        Vector<LayerChromium*> sublayerList;
        size_t i;
        for (i = 0; i < sublayers.size(); i++)
            sublayerList.append(sublayers[i].get());

        // Sort by the z coordinate of the layer center so that layers further away
        // are drawn first.
        std::stable_sort(sublayerList.begin(), sublayerList.end(), compareLayerZ);

        for (i = 0; i < sublayerList.size(); i++)
            drawLayersRecursive(sublayerList[i]);
    } else {
        const Vector<RefPtr<LayerChromium> >& sublayers = layer->getSublayers();
        for (size_t i = 0; i < sublayers.size(); i++)
            drawLayersRecursive(sublayers[i].get());
    }

    if (didStencilDraw) {
        // Draw into the stencil buffer subtracting 1 for every pixel hit
        // effectively removing this mask
        drawLayerIntoStencilBuffer(layer, true);
        currentStencilValue--;
        if (!currentStencilValue) {
            // Disable stencil testing.
            GLC(m_context, m_context->disable(GraphicsContext3D::STENCIL_TEST));
            GLC(m_context, m_context->stencilFunc(GraphicsContext3D::ALWAYS, 0, 0xff));
        }
    }

    if (mustResetScissorRect) {
        scissorToRect(previousScissorRect);
    }

    if (mustClearDepth) {
        GLC(m_context, m_context->disable(GraphicsContext3D::DEPTH_TEST));
        GLC(m_context, m_context->clear(GraphicsContext3D::DEPTH_BUFFER_BIT));
        depthTestEnabledForSubtree = false;
    }
}

void LayerRendererChromium::drawLayer(LayerChromium* layer)
{
    IntSize bounds = layer->bounds();

    if (layer->drawsContent()) {
        // Update the contents of the layer if necessary.
        if (layer->contentsDirty()) {
            // Update the backing texture contents for any dirty portion of the layer.
            layer->updateContents();
            m_context->makeContextCurrent();
        }

        if (layer->doubleSided())
            m_context->disable(GraphicsContext3D::CULL_FACE);
        else
            m_context->enable(GraphicsContext3D::CULL_FACE);

        layer->draw();
    }

    // Draw the debug border if there is one.
    layer->drawDebugBorder();
}

// Sets the scissor region to the given rectangle. The coordinate system for the
// scissorRect has its origin at the top left corner of the current visible rect.
void LayerRendererChromium::scissorToRect(const IntRect& scissorRect)
{
    // Compute the lower left corner of the scissor rect.
    int bottom = std::max(m_rootVisibleRect.height() - scissorRect.bottom(), 0);
    GLC(m_context, m_context->scissor(scissorRect.x(), bottom, scissorRect.width(), scissorRect.height()));
    m_currentScissorRect = scissorRect;
}

bool LayerRendererChromium::makeContextCurrent()
{
    m_context->makeContextCurrent();
    return true;
}

// Checks whether a given size is within the maximum allowed texture size range.
bool LayerRendererChromium::checkTextureSize(const IntSize& textureSize)
{
    if (textureSize.width() > m_maxTextureSize || textureSize.height() > m_maxTextureSize)
        return false;
    return true;
}

void LayerRendererChromium::resizeOnscreenContent(const IntSize& size)
{
    if (m_context)
        m_context->reshape(size.width(), size.height());
}

bool LayerRendererChromium::initializeSharedObjects()
{
    makeContextCurrent();

    // Vertex and fragment shaders for rendering the scrolled root layer quad.
    // They differ from a regular content layer shader in that they don't swizzle
    // the colors or take an alpha value.
    char scrollVertexShaderString[] =
        "attribute vec4 a_position;   \n"
        "attribute vec2 a_texCoord;   \n"
        "uniform mat4 matrix;         \n"
        "varying vec2 v_texCoord;     \n"
        "void main()                  \n"
        "{                            \n"
        "  gl_Position = matrix * a_position; \n"
        "  v_texCoord = a_texCoord;   \n"
        "}                            \n";
    char scrollFragmentShaderString[] =
        "precision mediump float;                            \n"
        "varying vec2 v_texCoord;                            \n"
        "uniform sampler2D s_texture;                        \n"
        "void main()                                         \n"
        "{                                                   \n"
        "  vec4 texColor = texture2D(s_texture, v_texCoord); \n"
        "  gl_FragColor = vec4(texColor.x, texColor.y, texColor.z, texColor.w); \n"
        "}                                                   \n";

    m_scrollShaderProgram = LayerChromium::createShaderProgram(m_context.get(), scrollVertexShaderString, scrollFragmentShaderString);
    if (!m_scrollShaderProgram) {
        LOG_ERROR("LayerRendererChromium: Failed to create scroll shader program");
        cleanupSharedObjects();
        return false;
    }

    GLC(m_context, m_scrollShaderSamplerLocation = m_context->getUniformLocation(m_scrollShaderProgram, "s_texture"));
    GLC(m_context, m_scrollShaderMatrixLocation = m_context->getUniformLocation(m_scrollShaderProgram, "matrix"));
    if (m_scrollShaderSamplerLocation == -1 || m_scrollShaderMatrixLocation == -1) {
        LOG_ERROR("Failed to initialize scroll shader.");
        cleanupSharedObjects();
        return false;
    }

    // Create a texture object to hold the contents of the root layer.
    m_rootLayerTextureId = createLayerTexture();
    if (!m_rootLayerTextureId) {
        LOG_ERROR("Failed to create texture for root layer");
        cleanupSharedObjects();
        return false;
    }
    // Turn off filtering for the root layer to avoid blurring from the repeated
    // writes and reads to the framebuffer that happen while scrolling.
    GLC(m_context, m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, m_rootLayerTextureId));
    GLC(m_context, m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::NEAREST));
    GLC(m_context, m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::NEAREST));

    // Get the max texture size supported by the system.
    GLC(m_context, m_context->getIntegerv(GraphicsContext3D::MAX_TEXTURE_SIZE, &m_maxTextureSize));

    // Get the number of bits available in the stencil buffer.
    GLC(m_context, m_context->getIntegerv(GraphicsContext3D::STENCIL_BITS, &m_numStencilBits));

    m_layerSharedValues = adoptPtr(new LayerChromium::SharedValues(m_context.get()));
    m_contentLayerSharedValues = adoptPtr(new ContentLayerChromium::SharedValues(m_context.get()));
    m_canvasLayerSharedValues = adoptPtr(new CanvasLayerChromium::SharedValues(m_context.get()));
    m_videoLayerSharedValues = adoptPtr(new VideoLayerChromium::SharedValues(m_context.get()));
    m_pluginLayerSharedValues = adoptPtr(new PluginLayerChromium::SharedValues(m_context.get()));

    if (!m_layerSharedValues->initialized() || !m_contentLayerSharedValues->initialized() || !m_canvasLayerSharedValues->initialized()
        || !m_videoLayerSharedValues->initialized() || !m_pluginLayerSharedValues->initialized()) {
        cleanupSharedObjects();
        return false;
    }

    return true;
}

void LayerRendererChromium::cleanupSharedObjects()
{
    makeContextCurrent();

    m_layerSharedValues.clear();
    m_contentLayerSharedValues.clear();
    m_canvasLayerSharedValues.clear();
    m_videoLayerSharedValues.clear();
    m_pluginLayerSharedValues.clear();

    if (m_scrollShaderProgram) {
        GLC(m_context, m_context->deleteProgram(m_scrollShaderProgram));
        m_scrollShaderProgram = 0;
    }

    if (m_rootLayerTextureId) {
        deleteLayerTexture(m_rootLayerTextureId);
        m_rootLayerTextureId = 0;
    }
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
