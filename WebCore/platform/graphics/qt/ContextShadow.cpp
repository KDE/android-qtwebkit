/*
 * Copyright (C) 2010 Sencha, Inc.
 *
 * All rights reserved.
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
#include "ContextShadow.h"

#include <QTimerEvent>
#include <wtf/Noncopyable.h>

namespace WebCore {

// ContextShadow needs a scratch image as the buffer for the blur filter.
// Instead of creating and destroying the buffer for every operation,
// we create a buffer which will be automatically purged via a timer.

class ShadowBuffer: public QObject {
public:
    ShadowBuffer(QObject* parent = 0);

    QImage* scratchImage(const QSize& size);

    void schedulePurge();

protected:
    void timerEvent(QTimerEvent* event);

private:
    QImage image;
    int timerId;
};

ShadowBuffer::ShadowBuffer(QObject* parent)
    : QObject(parent)
    , timerId(0)
{
}

QImage* ShadowBuffer::scratchImage(const QSize& size)
{
    int width = size.width();
    int height = size.height();

    // We do not need to recreate the buffer if the buffer is reasonably
    // larger than the requested size. However, if the requested size is
    // much smaller than our buffer, reduce our buffer so that we will not
    // keep too many allocated pixels for too long.
    if (!image.isNull() && (image.width() > width) && (image.height() > height))
        if (((2 * width) > image.width()) && ((2 * height) > image.height())) {
            image.fill(Qt::transparent);
            return &image;
        }

    // Round to the nearest 32 pixels so we do not grow the buffer everytime
    // there is larger request by 1 pixel.
    width = (1 + (width >> 5)) << 5;
    height = (1 + (height >> 5)) << 5;

    image = QImage(width, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    return &image;
}

void ShadowBuffer::schedulePurge()
{
    static const double BufferPurgeDelay = 2; // seconds
    killTimer(timerId);
    timerId = startTimer(BufferPurgeDelay * 1000);
}

void ShadowBuffer::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == timerId) {
        killTimer(timerId);
        image = QImage();
    }
    QObject::timerEvent(event);
}

Q_GLOBAL_STATIC(ShadowBuffer, scratchShadowBuffer)

ContextShadow::ContextShadow()
    : type(NoShadow)
    , blurRadius(0)
{
}

ContextShadow::ContextShadow(const QColor& c, float r, qreal dx, qreal dy)
    : color(c)
    , blurRadius(qRound(r))
    , offset(dx, dy)
{
    // The type of shadow is decided by the blur radius, shadow offset, and shadow color.
    if (!color.isValid() || !color.alpha()) {
        // Can't paint the shadow with invalid or invisible color.
        type = NoShadow;
    } else if (r > 0) {
        // Shadow is always blurred, even the offset is zero.
        type = BlurShadow;
    } else if (offset.isNull()) {
        // Without blur and zero offset means the shadow is fully hidden.
        type = NoShadow;
    } else {
        if (color.alpha() > 0)
            type = AlphaSolidShadow;
        else
            type = OpaqueSolidShadow;
    }
}

void ContextShadow::clear()
{
    type = NoShadow;
    color = QColor();
    blurRadius = 0;
    offset = QPointF(0, 0);
}

// Instead of integer division, we use 18.14 for fixed-point division.
static const int BlurSumShift = 14;

// Note: image must be RGB32 format
static void blurHorizontal(QImage& image, int radius, bool swap = false)
{
    Q_ASSERT(image.format() == QImage::Format_ARGB32_Premultiplied);

    // See comments in http://webkit.org/b/40793, it seems sensible
    // to follow Skia's limit of 128 pixels of blur radius
    radius = qMin(128, radius);

    int imgWidth = image.width();
    int imgHeight = image.height();

    // Check http://www.w3.org/TR/SVG/filters.html#feGaussianBlur
    // for the approaches when the box-blur radius is even vs odd.
    int dmax = radius >> 1;
    int dmin = qMax(0, dmax - 1 + (radius & 1));

    for (int y = 0; y < imgHeight; ++y) {

        unsigned char* pixels = image.scanLine(y);

        int left;
        int right;
        int pixelCount;
        int prev;
        int next;
        int firstAlpha;
        int lastAlpha;
        int totalAlpha;
        unsigned char* target;
        unsigned char* prevPtr;
        unsigned char* nextPtr;

        int invCount;

        static const int alphaChannel = 3;
        static const int blueChannel = 0;
        static const int greenChannel = 1;

        // For each step, we use sliding window algorithm. This is much more
        // efficient than computing the sum of each pixels covered by the box
        // kernel size for each x.

        // As noted in the SVG filter specification, running box blur 3x
        // approximates a real gaussian blur nicely.

        // Step 1: blur alpha channel and store the result in the blue channel.
        left = swap ? dmax : dmin;
        right = swap ? dmin : dmax;
        pixelCount = left + 1 + right;
        invCount = (1 << BlurSumShift) / pixelCount;
        prev = -left;
        next = 1 + right;
        firstAlpha = pixels[alphaChannel];
        lastAlpha = pixels[(imgWidth - 1) * 4 + alphaChannel];
        totalAlpha = 0;
        for (int i = 0; i < pixelCount; ++i)
            totalAlpha += pixels[qBound(0, i - left, imgWidth - 1) * 4 + alphaChannel];
        target = pixels + blueChannel;
        prevPtr = pixels + prev * 4 + alphaChannel;
        nextPtr = pixels + next * 4 + alphaChannel;
        for (int x = 0; x < imgWidth; ++x, ++prev, ++next, target += 4, prevPtr += 4, nextPtr += 4) {
            *target = (totalAlpha * invCount) >> BlurSumShift;
            int delta = ((next < imgWidth) ? *nextPtr : lastAlpha) -
                        ((prev > 0) ? *prevPtr : firstAlpha);
            totalAlpha += delta;
        }

        // Step 2: blur blue channel and store the result in the green channel.
        left = swap ? dmin : dmax;
        right = swap ? dmax : dmin;
        pixelCount = left + 1 + right;
        invCount = (1 << BlurSumShift) / pixelCount;
        prev = -left;
        next = 1 + right;
        firstAlpha = pixels[blueChannel];
        lastAlpha = pixels[(imgWidth - 1) * 4 + blueChannel];
        totalAlpha = 0;
        for (int i = 0; i < pixelCount; ++i)
            totalAlpha += pixels[qBound(0, i - left, imgWidth - 1) * 4 + blueChannel];
        target = pixels + greenChannel;
        prevPtr = pixels + prev * 4 + blueChannel;
        nextPtr = pixels + next * 4 + blueChannel;
        for (int x = 0; x < imgWidth; ++x, ++prev, ++next, target += 4, prevPtr += 4, nextPtr += 4) {
            *target = (totalAlpha * invCount) >> BlurSumShift;
            int delta = ((next < imgWidth) ? *nextPtr : lastAlpha) -
                        ((prev > 0) ? *prevPtr : firstAlpha);
            totalAlpha += delta;
        }

        // Step 3: blur green channel and store the result in the alpha channel.
        left = dmax;
        right = dmax;
        pixelCount = left + 1 + right;
        invCount = (1 << BlurSumShift) / pixelCount;
        prev = -left;
        next = 1 + right;
        firstAlpha = pixels[greenChannel];
        lastAlpha = pixels[(imgWidth - 1) * 4 + greenChannel];
        totalAlpha = 0;
        for (int i = 0; i < pixelCount; ++i)
            totalAlpha += pixels[qBound(0, i - left, imgWidth - 1) * 4 + greenChannel];
        target = pixels + alphaChannel;
        prevPtr = pixels + prev * 4 + greenChannel;
        nextPtr = pixels + next * 4 + greenChannel;
        for (int x = 0; x < imgWidth; ++x, ++prev, ++next, target += 4, prevPtr += 4, nextPtr += 4) {
            *target = (totalAlpha * invCount) >> BlurSumShift;
            int delta = ((next < imgWidth) ? *nextPtr : lastAlpha) -
                        ((prev > 0) ? *prevPtr : firstAlpha);
            totalAlpha += delta;
        }
    }
}

static void shadowBlur(QImage& image, int radius, const QColor& shadowColor)
{
    blurHorizontal(image, radius);

    QTransform transform;
    transform.rotate(90);
    image = image.transformed(transform);
    blurHorizontal(image, radius, true);
    transform.reset();
    transform.rotate(270);
    image = image.transformed(transform);

    // "Colorize" with the right shadow color.
    QPainter p(&image);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(image.rect(), shadowColor.rgb());
    p.end();
}

QPainter* ContextShadow::beginShadowLayer(QPainter* p, const QRectF &rect)
{
    // We expand the area by the blur radius * 2 to give extra space
    // for the blur transition.
    int extra = (type == BlurShadow) ? blurRadius * 2 : 0;

    QRectF shadowRect = rect.translated(offset);
    QRectF bufferRect = shadowRect.adjusted(-extra, -extra, extra, extra);
    m_layerRect = bufferRect.toAlignedRect();

    QRect clipRect;
    if (p->hasClipping())
#if QT_VERSION >= QT_VERSION_CHECK(4, 8, 0)
        clipRect = p->clipBoundingRect();
#else
        clipRect = p->clipRegion().boundingRect();
#endif
    else
        clipRect = p->transform().inverted().mapRect(p->window());

    if (!clipRect.contains(m_layerRect)) {

        // No need to have the buffer larger than the clip.
        m_layerRect = m_layerRect.intersected(clipRect);
        if (m_layerRect.isEmpty())
            return 0;

        // We adjust again because the pixels at the borders are still
        // potentially affected by the pixels outside the buffer.
        if (type == BlurShadow)
            m_layerRect.adjust(-extra, -extra, extra, extra);
    }

    ShadowBuffer* shadowBuffer = scratchShadowBuffer();
    QImage* shadowImage = shadowBuffer->scratchImage(m_layerRect.size());
    m_layerImage = QImage(*shadowImage);

    m_layerPainter = new QPainter;
    m_layerPainter->begin(&m_layerImage);
    m_layerPainter->setFont(p->font());
    m_layerPainter->translate(offset);

    // The origin is now the top left corner of the scratch image.
    m_layerPainter->translate(-m_layerRect.topLeft());

    return m_layerPainter;
}

void ContextShadow::endShadowLayer(QPainter* p)
{
    m_layerPainter->end();
    delete m_layerPainter;
    m_layerPainter = 0;

    if (type == BlurShadow)
        shadowBlur(m_layerImage, blurRadius, color);

    p->drawImage(m_layerRect.topLeft(), m_layerImage);

    scratchShadowBuffer()->schedulePurge();
}

}
