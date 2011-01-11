/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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
#include "ImageBuffer.h"

#include "Base64.h"
#include "BitmapImage.h"
#include "BitmapImageSingleFrameSkia.h"
#include "DrawingBuffer.h"
#include "GLES2Canvas.h"
#include "GraphicsContext.h"
#include "ImageData.h"
#include "JPEGImageEncoder.h"
#include "MIMETypeRegistry.h"
#include "PNGImageEncoder.h"
#include "PlatformContextSkia.h"
#include "SkColorPriv.h"
#include "SkiaUtils.h"

#include <wtf/text/StringConcatenate.h>

using namespace std;

namespace WebCore {

// We pass a technically-uninitialized canvas to the platform context here since
// the canvas initialization completes in ImageBuffer::ImageBuffer. But
// PlatformContext doesn't actually need to use the object, and this makes all
// the ownership easier to manage.
ImageBufferData::ImageBufferData(const IntSize& size)
    : m_platformContext(0)  // Canvas is set in ImageBuffer constructor.
{
}

ImageBuffer::ImageBuffer(const IntSize& size, ColorSpace, RenderingMode, bool& success)
    : m_data(size)
    , m_size(size)
{
    if (!m_data.m_canvas.initialize(size.width(), size.height(), false)) {
        success = false;
        return;
    }

    m_data.m_platformContext.setCanvas(&m_data.m_canvas);
    m_context.set(new GraphicsContext(&m_data.m_platformContext));
    m_context->platformContext()->setDrawingToImageBuffer(true);

    // Make the background transparent. It would be nice if this wasn't
    // required, but the canvas is currently filled with the magic transparency
    // color. Can we have another way to manage this?
    m_data.m_canvas.drawARGB(0, 0, 0, 0, SkXfermode::kClear_Mode);
    success = true;
}

ImageBuffer::~ImageBuffer()
{
}

GraphicsContext* ImageBuffer::context() const
{
    return m_context.get();
}

bool ImageBuffer::drawsUsingCopy() const
{
    return false;
}

PassRefPtr<Image> ImageBuffer::copyImage() const
{
    m_context->platformContext()->syncSoftwareCanvas();
    return BitmapImageSingleFrameSkia::create(*m_data.m_platformContext.bitmap(), true);
}

void ImageBuffer::clip(GraphicsContext* context, const FloatRect& rect) const
{
    context->platformContext()->beginLayerClippedToImage(rect, this);
}

void ImageBuffer::draw(GraphicsContext* context, ColorSpace styleColorSpace, const FloatRect& destRect, const FloatRect& srcRect,
                       CompositeOperator op, bool useLowQualityScale)
{
    if (m_data.m_platformContext.useGPU() && context->platformContext()->useGPU()) {
        if (context->platformContext()->canAccelerate()) {
            DrawingBuffer* sourceDrawingBuffer = m_data.m_platformContext.gpuCanvas()->drawingBuffer();
            unsigned sourceTexture = static_cast<unsigned>(sourceDrawingBuffer->platformColorBuffer());
            FloatRect destRectNormalized(normalizeRect(destRect));
            FloatRect srcRectFlipped(normalizeRect(srcRect));
            srcRectFlipped.setY(m_size.height() - srcRect.y());
            srcRectFlipped.setHeight(-srcRect.height());
            context->platformContext()->prepareForHardwareDraw();
            context->platformContext()->gpuCanvas()->drawTexturedRect(sourceTexture, m_size, srcRectFlipped, destRectNormalized, styleColorSpace, op);
            return;
        }
        m_data.m_platformContext.syncSoftwareCanvas();
    }

    RefPtr<Image> image = BitmapImageSingleFrameSkia::create(*m_data.m_platformContext.bitmap(), context == m_context);
    context->drawImage(image.get(), styleColorSpace, destRect, srcRect, op, useLowQualityScale);
}

void ImageBuffer::drawPattern(GraphicsContext* context, const FloatRect& srcRect, const AffineTransform& patternTransform,
                              const FloatPoint& phase, ColorSpace styleColorSpace, CompositeOperator op, const FloatRect& destRect)
{
    RefPtr<Image> image = BitmapImageSingleFrameSkia::create(*m_data.m_platformContext.bitmap(), context == m_context);
    image->drawPattern(context, srcRect, patternTransform, phase, styleColorSpace, op, destRect);
}

void ImageBuffer::platformTransformColorSpace(const Vector<int>& lookUpTable)
{
    const SkBitmap& bitmap = *context()->platformContext()->bitmap();
    if (bitmap.isNull())
        return;

    ASSERT(bitmap.config() == SkBitmap::kARGB_8888_Config);
    SkAutoLockPixels bitmapLock(bitmap);
    for (int y = 0; y < m_size.height(); ++y) {
        uint32_t* srcRow = bitmap.getAddr32(0, y);
        for (int x = 0; x < m_size.width(); ++x) {
            SkColor color = SkPMColorToColor(srcRow[x]);
            srcRow[x] = SkPreMultiplyARGB(SkColorGetA(color),
                                          lookUpTable[SkColorGetR(color)],
                                          lookUpTable[SkColorGetG(color)],
                                          lookUpTable[SkColorGetB(color)]);
        }
    }
}

template <Multiply multiplied>
PassRefPtr<ByteArray> getImageData(const IntRect& rect, const SkBitmap& bitmap, 
                                   const IntSize& size)
{
    RefPtr<ByteArray> result = ByteArray::create(rect.width() * rect.height() * 4);

    if (bitmap.config() == SkBitmap::kNo_Config) {
        // This is an empty SkBitmap that could not be configured.
        ASSERT(!size.width() || !size.height());
        return result.release();
    }

    unsigned char* data = result->data();

    if (rect.x() < 0
        || rect.y() < 0
        || rect.right() > size.width()
        || rect.bottom() > size.height())
        memset(data, 0, result->length());

    int originX = rect.x();
    int destX = 0;
    if (originX < 0) {
        destX = -originX;
        originX = 0;
    }
    int endX = rect.right();
    if (endX > size.width())
        endX = size.width();
    int numColumns = endX - originX;

    if (numColumns <= 0) 
        return result.release();

    int originY = rect.y();
    int destY = 0;
    if (originY < 0) {
        destY = -originY;
        originY = 0;
    }
    int endY = rect.bottom();
    if (endY > size.height())
        endY = size.height();
    int numRows = endY - originY;

    if (numRows <= 0) 
        return result.release();

    ASSERT(bitmap.config() == SkBitmap::kARGB_8888_Config);
    SkAutoLockPixels bitmapLock(bitmap);

    unsigned destBytesPerRow = 4 * rect.width();
    unsigned char* destRow = data + destY * destBytesPerRow + destX * 4;

    for (int y = 0; y < numRows; ++y) {
        uint32_t* srcRow = bitmap.getAddr32(originX, originY + y);
        for (int x = 0; x < numColumns; ++x) {
            unsigned char* destPixel = &destRow[x * 4];
            if (multiplied == Unmultiplied) {
                SkColor color = srcRow[x];
                unsigned a = SkColorGetA(color);
                destPixel[0] = a ? SkColorGetR(color) * 255 / a : 0;
                destPixel[1] = a ? SkColorGetG(color) * 255 / a : 0;
                destPixel[2] = a ? SkColorGetB(color) * 255 / a : 0;
                destPixel[3] = a;
            } else {
                // Input and output are both pre-multiplied, we just need to re-arrange the
                // bytes from the bitmap format to RGBA.
                destPixel[0] = SkGetPackedR32(srcRow[x]);
                destPixel[1] = SkGetPackedG32(srcRow[x]);
                destPixel[2] = SkGetPackedB32(srcRow[x]);
                destPixel[3] = SkGetPackedA32(srcRow[x]);
            }
        }
        destRow += destBytesPerRow;
    }

    return result.release();
}

PassRefPtr<ByteArray> ImageBuffer::getUnmultipliedImageData(const IntRect& rect) const
{
    context()->platformContext()->syncSoftwareCanvas();
    return getImageData<Unmultiplied>(rect, *context()->platformContext()->bitmap(), m_size);
}

PassRefPtr<ByteArray> ImageBuffer::getPremultipliedImageData(const IntRect& rect) const
{
    context()->platformContext()->syncSoftwareCanvas();
    return getImageData<Premultiplied>(rect, *context()->platformContext()->bitmap(), m_size);
}

template <Multiply multiplied>
void putImageData(ByteArray*& source, const IntSize& sourceSize, const IntRect& sourceRect, const IntPoint& destPoint, 
                  const SkBitmap& bitmap, const IntSize& size)
{
    ASSERT(sourceRect.width() > 0);
    ASSERT(sourceRect.height() > 0);

    int originX = sourceRect.x();
    int destX = destPoint.x() + sourceRect.x();
    ASSERT(destX >= 0);
    ASSERT(destX < size.width());
    ASSERT(originX >= 0);
    ASSERT(originX < sourceRect.right());

    int endX = destPoint.x() + sourceRect.right();
    ASSERT(endX <= size.width());

    int numColumns = endX - destX;

    int originY = sourceRect.y();
    int destY = destPoint.y() + sourceRect.y();
    ASSERT(destY >= 0);
    ASSERT(destY < size.height());
    ASSERT(originY >= 0);
    ASSERT(originY < sourceRect.bottom());

    int endY = destPoint.y() + sourceRect.bottom();
    ASSERT(endY <= size.height());
    int numRows = endY - destY;

    ASSERT(bitmap.config() == SkBitmap::kARGB_8888_Config);
    SkAutoLockPixels bitmapLock(bitmap);

    unsigned srcBytesPerRow = 4 * sourceSize.width();

    const unsigned char* srcRow = source->data() + originY * srcBytesPerRow + originX * 4;

    for (int y = 0; y < numRows; ++y) {
        uint32_t* destRow = bitmap.getAddr32(destX, destY + y);
        for (int x = 0; x < numColumns; ++x) {
            const unsigned char* srcPixel = &srcRow[x * 4];
            if (multiplied == Unmultiplied) {
                unsigned char alpha = srcPixel[3];
                unsigned char r = SkMulDiv255Ceiling(srcPixel[0], alpha);
                unsigned char g = SkMulDiv255Ceiling(srcPixel[1], alpha);
                unsigned char b = SkMulDiv255Ceiling(srcPixel[2], alpha);
                destRow[x] = SkPackARGB32(alpha, r, g, b);
            } else
                destRow[x] = SkPackARGB32(srcPixel[3], srcPixel[0],
                                          srcPixel[1], srcPixel[2]);
        }
        srcRow += srcBytesPerRow;
    }
}

void ImageBuffer::putUnmultipliedImageData(ByteArray* source, const IntSize& sourceSize, const IntRect& sourceRect, const IntPoint& destPoint)
{
    context()->platformContext()->prepareForSoftwareDraw();
    putImageData<Unmultiplied>(source, sourceSize, sourceRect, destPoint, *context()->platformContext()->bitmap(), m_size);
}

void ImageBuffer::putPremultipliedImageData(ByteArray* source, const IntSize& sourceSize, const IntRect& sourceRect, const IntPoint& destPoint)
{
    putImageData<Premultiplied>(source, sourceSize, sourceRect, destPoint, *context()->platformContext()->bitmap(), m_size);
}

String ImageBuffer::toDataURL(const String& mimeType, const double* quality) const
{
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));

    Vector<unsigned char> encodedImage;
    if (mimeType == "image/jpeg") {
        int compressionQuality = JPEGImageEncoder::DefaultCompressionQuality;
        if (quality && *quality >= 0.0 && *quality <= 1.0)
            compressionQuality = static_cast<int>(*quality * 100 + 0.5);
        if (!JPEGImageEncoder::encode(*context()->platformContext()->bitmap(), compressionQuality, &encodedImage))
            return "data:,";
    } else {
        if (!PNGImageEncoder::encode(*context()->platformContext()->bitmap(), &encodedImage))
            return "data:,";
        ASSERT(mimeType == "image/png");
    }

    Vector<char> base64Data;
    base64Encode(*reinterpret_cast<Vector<char>*>(&encodedImage), base64Data);

    return makeString("data:", mimeType, ";base64,", base64Data);
}

} // namespace WebCore
