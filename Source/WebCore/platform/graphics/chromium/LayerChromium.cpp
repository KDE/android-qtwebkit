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

#include "LayerChromium.h"

#include "cc/CCLayerImpl.h"
#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"
#if USE(SKIA)
#include "NativeImageSkia.h"
#include "PlatformContextSkia.h"
#endif
#include "RenderLayerBacking.h"
#include "TextStream.h"
#include "skia/ext/platform_canvas.h"

namespace WebCore {

using namespace std;

#ifndef NDEBUG
static int s_nextLayerDebugID = 1;
#endif

PassRefPtr<LayerChromium> LayerChromium::create(GraphicsLayerChromium* owner)
{
    return adoptRef(new LayerChromium(owner));
}

LayerChromium::LayerChromium(GraphicsLayerChromium* owner)
    : m_owner(owner)
    , m_contentsDirty(false)
    , m_maskLayer(0)
    , m_ccLayerImpl(0)
    , m_superlayer(0)
#ifndef NDEBUG
    , m_debugID(s_nextLayerDebugID++)
#endif
    , m_anchorPoint(0.5, 0.5)
    , m_backgroundColor(0, 0, 0, 0)
    , m_opacity(1.0)
    , m_zPosition(0.0)
    , m_anchorPointZ(0)
    , m_clearsContext(false)
    , m_hidden(false)
    , m_masksToBounds(false)
    , m_opaque(true)
    , m_geometryFlipped(false)
    , m_needsDisplayOnBoundsChange(false)
    , m_doubleSided(true)
    , m_replicaLayer(0)
{
    ASSERT(!LayerRendererChromium::s_inPaintLayerContents);
}

LayerChromium::~LayerChromium()
{
    ASSERT(!LayerRendererChromium::s_inPaintLayerContents);
    // Our superlayer should be holding a reference to us so there should be no
    // way for us to be destroyed while we still have a superlayer.
    ASSERT(!superlayer());

    if (m_ccLayerImpl)
        m_ccLayerImpl->resetOwner();

    // Remove the superlayer reference from all sublayers.
    removeAllSublayers();
}

void LayerChromium::cleanupResources()
{
    if (m_ccLayerImpl)
        m_ccLayerImpl->cleanupResources();
}

void LayerChromium::setLayerRenderer(LayerRendererChromium* renderer)
{
    // If we're changing layer renderers then we need to free up any resources
    // allocated by the old renderer.
    if (layerRenderer() && layerRenderer() != renderer) {
        cleanupResources();
        setNeedsDisplay();
    }
    m_layerRenderer = renderer;
}

void LayerChromium::setNeedsCommit()
{
    // Call notifySyncRequired(), which for non-root layers plumbs through to
    // call setRootLayerNeedsDisplay() on the WebView, which will cause LayerRendererChromium
    // to render a frame.
    // This function has no effect on root layers.
    if (m_owner)
        m_owner->notifySyncRequired();
}

void LayerChromium::addSublayer(PassRefPtr<LayerChromium> sublayer)
{
    insertSublayer(sublayer, numSublayers());
}

void LayerChromium::insertSublayer(PassRefPtr<LayerChromium> sublayer, size_t index)
{
    index = min(index, m_sublayers.size());
    sublayer->removeFromSuperlayer();
    sublayer->setSuperlayer(this);
    m_sublayers.insert(index, sublayer);
    setNeedsCommit();
}

void LayerChromium::removeFromSuperlayer()
{
    if (m_superlayer)
        m_superlayer->removeSublayer(this);
}

void LayerChromium::removeSublayer(LayerChromium* sublayer)
{
    int foundIndex = indexOfSublayer(sublayer);
    if (foundIndex == -1)
        return;

    sublayer->setSuperlayer(0);
    m_sublayers.remove(foundIndex);
    setNeedsCommit();
}

void LayerChromium::replaceSublayer(LayerChromium* reference, PassRefPtr<LayerChromium> newLayer)
{
    ASSERT_ARG(reference, reference);
    ASSERT_ARG(reference, reference->superlayer() == this);

    if (reference == newLayer)
        return;

    int referenceIndex = indexOfSublayer(reference);
    if (referenceIndex == -1) {
        ASSERT_NOT_REACHED();
        return;
    }

    reference->removeFromSuperlayer();

    if (newLayer) {
        newLayer->removeFromSuperlayer();
        insertSublayer(newLayer, referenceIndex);
    }
}

int LayerChromium::indexOfSublayer(const LayerChromium* reference)
{
    for (size_t i = 0; i < m_sublayers.size(); i++) {
        if (m_sublayers[i] == reference)
            return i;
    }
    return -1;
}

void LayerChromium::setBounds(const IntSize& size)
{
    if (bounds() == size)
        return;

    bool firstResize = !bounds().width() && !bounds().height() && size.width() && size.height();

    m_bounds = size;

    if (firstResize)
        setNeedsDisplay(FloatRect(0, 0, bounds().width(), bounds().height()));
    else
        setNeedsCommit();
}

void LayerChromium::setFrame(const FloatRect& rect)
{
    if (rect == m_frame)
      return;

    m_frame = rect;
    setNeedsDisplay(FloatRect(0, 0, bounds().width(), bounds().height()));
}

const LayerChromium* LayerChromium::rootLayer() const
{
    const LayerChromium* layer = this;
    for (LayerChromium* superlayer = layer->superlayer(); superlayer; layer = superlayer, superlayer = superlayer->superlayer()) { }
    return layer;
}

void LayerChromium::removeAllSublayers()
{
    while (m_sublayers.size()) {
        LayerChromium* layer = m_sublayers[0].get();
        ASSERT(layer->superlayer());
        layer->removeFromSuperlayer();
    }
}

void LayerChromium::setSublayers(const Vector<RefPtr<LayerChromium> >& sublayers)
{
    if (sublayers == m_sublayers)
        return;

    removeAllSublayers();
    size_t listSize = sublayers.size();
    for (size_t i = 0; i < listSize; i++)
        addSublayer(sublayers[i]);
}

LayerChromium* LayerChromium::superlayer() const
{
    return m_superlayer;
}

void LayerChromium::setName(const String& name)
{
    m_name = name;
}

void LayerChromium::setNeedsDisplay(const FloatRect& dirtyRect)
{
    // Simply mark the contents as dirty. For non-root layers, the call to
    // setNeedsCommit will schedule a fresh compositing pass.
    // For the root layer, setNeedsCommit has no effect.
    m_contentsDirty = true;

    m_dirtyRect.unite(dirtyRect);
    setNeedsCommit();
}

void LayerChromium::setNeedsDisplay()
{
    m_dirtyRect.setLocation(FloatPoint());
    m_dirtyRect.setSize(bounds());
    m_contentsDirty = true;
    setNeedsCommit();
}

void LayerChromium::resetNeedsDisplay()
{
    m_dirtyRect = FloatRect();
    m_contentsDirty = false;
}

void LayerChromium::toGLMatrix(float* flattened, const TransformationMatrix& m)
{
    flattened[0] = m.m11();
    flattened[1] = m.m12();
    flattened[2] = m.m13();
    flattened[3] = m.m14();
    flattened[4] = m.m21();
    flattened[5] = m.m22();
    flattened[6] = m.m23();
    flattened[7] = m.m24();
    flattened[8] = m.m31();
    flattened[9] = m.m32();
    flattened[10] = m.m33();
    flattened[11] = m.m34();
    flattened[12] = m.m41();
    flattened[13] = m.m42();
    flattened[14] = m.m43();
    flattened[15] = m.m44();
}

void LayerChromium::pushPropertiesTo(CCLayerImpl* layer)
{
    layer->setAnchorPoint(m_anchorPoint);
    layer->setAnchorPointZ(m_anchorPointZ);
    layer->setBounds(m_bounds);
    layer->setDebugBorderColor(m_debugBorderColor);
    layer->setDebugBorderWidth(m_debugBorderWidth);
    layer->setDoubleSided(m_doubleSided);
    layer->setLayerRenderer(m_layerRenderer.get());
    layer->setMasksToBounds(m_masksToBounds);
    layer->setName(m_name);
    layer->setOpacity(m_opacity);
    layer->setPosition(m_position);
    layer->setPreserves3D(preserves3D());
    layer->setSublayerTransform(m_sublayerTransform);
    layer->setTransform(m_transform);

    if (maskLayer())
        maskLayer()->pushPropertiesTo(layer->maskLayer());
    if (replicaLayer())
        replicaLayer()->pushPropertiesTo(layer->replicaLayer());
}

GraphicsContext3D* LayerChromium::layerRendererContext() const
{
    ASSERT(layerRenderer());
    return layerRenderer()->context();
}

void LayerChromium::drawTexturedQuad(GraphicsContext3D* context, const TransformationMatrix& projectionMatrix, const TransformationMatrix& drawMatrix,
                                     float width, float height, float opacity,
                                     int matrixLocation, int alphaLocation)
{
    static float glMatrix[16];

    TransformationMatrix renderMatrix = drawMatrix;

    // Apply a scaling factor to size the quad from 1x1 to its intended size.
    renderMatrix.scale3d(width, height, 1);

    // Apply the projection matrix before sending the transform over to the shader.
    toGLMatrix(&glMatrix[0], projectionMatrix * renderMatrix);

    GLC(context, context->uniformMatrix4fv(matrixLocation, false, &glMatrix[0], 1));

    if (alphaLocation != -1)
        GLC(context, context->uniform1f(alphaLocation, opacity));

    GLC(context, context->drawElements(GraphicsContext3D::TRIANGLES, 6, GraphicsContext3D::UNSIGNED_SHORT, 0));
}

String LayerChromium::layerTreeAsText() const
{
    TextStream ts;
    dumpLayer(ts, 0);
    return ts.release();
}

static void writeIndent(TextStream& ts, int indent)
{
    for (int i = 0; i != indent; ++i)
        ts << "  ";
}

void LayerChromium::dumpLayer(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << layerTypeAsString() << "(" << m_name << ")\n";
    dumpLayerProperties(ts, indent+2);
    if (m_ccLayerImpl)
        m_ccLayerImpl->dumpLayerProperties(ts, indent+2);
    if (m_replicaLayer) {
        writeIndent(ts, indent+2);
        ts << "Replica:\n";
        m_replicaLayer->dumpLayer(ts, indent+3);
    }
    if (m_maskLayer) {
        writeIndent(ts, indent+2);
        ts << "Mask:\n";
        m_maskLayer->dumpLayer(ts, indent+3);
    }
    for (size_t i = 0; i < m_sublayers.size(); ++i)
        m_sublayers[i]->dumpLayer(ts, indent+1);
}

void LayerChromium::dumpLayerProperties(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
#ifndef NDEBUG
    ts << "debugID: " << debugID() << ", ";
#else
#endif
    ts << "drawsContent: " << drawsContent() << "\n";

}

PassRefPtr<CCLayerImpl> LayerChromium::createCCLayerImpl()
{
    return CCLayerImpl::create(this);
}

void LayerChromium::createCCLayerImplIfNeeded()
{
    if (!m_ccLayerImpl)
        m_ccLayerImpl = createCCLayerImpl();
}

CCLayerImpl* LayerChromium::ccLayerImpl()
{
    return m_ccLayerImpl.get();
}

void LayerChromium::setBorderColor(const Color& color)
{
    m_debugBorderColor = color;
    setNeedsCommit();
}

void LayerChromium::setBorderWidth(float width)
{
    m_debugBorderWidth = width;
    setNeedsCommit();
}

LayerRendererChromium* LayerChromium::layerRenderer() const
{
    return m_layerRenderer.get();
}

}
#endif // USE(ACCELERATED_COMPOSITING)
