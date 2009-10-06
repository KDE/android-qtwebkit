/*
 * Copyright (C) 2006 Friedemann Kleint <fkleint@trolltech.com>
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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
#include "ImageDecoderQt.h"

#include <QtCore/QByteArray>
#include <QtCore/QBuffer>

#include <QtGui/QImageReader>
#include <qdebug.h>

namespace WebCore {

ImageDecoder* ImageDecoder::create(const SharedBuffer& data)
{
    // We need at least 4 bytes to figure out what kind of image we're dealing with.
    if (data.size() < 4)
        return 0;

    QByteArray bytes = QByteArray::fromRawData(data.data(), data.size());
    QBuffer buffer(&bytes);
    if (!buffer.open(QBuffer::ReadOnly))
        return 0;

    QString imageFormat = QString::fromLatin1(QImageReader::imageFormat(&buffer).toLower());
    if (imageFormat.isEmpty())
        return 0; // Image format not supported

    return new ImageDecoderQt(imageFormat);
}

ImageDecoderQt::ImageData::ImageData(const QImage& image, int duration)
    : m_image(image)
    , m_duration(duration)
{
}

// Context, maintains IODevice on a data buffer.
class ImageDecoderQt::ReadContext {
public:

    ReadContext(SharedBuffer* data, ImageList &target);
    bool read();

    QImageReader *reader() { return &m_reader; }

private:
    enum IncrementalReadResult { IncrementalReadFailed, IncrementalReadPartial, IncrementalReadComplete };
    // Incrementally read an image
    IncrementalReadResult readImageLines(ImageData &);

    QByteArray m_data;
    QBuffer m_buffer;
    QImageReader m_reader;

    ImageList &m_target;

    // Detected data format of the stream
    enum QImage::Format m_dataFormat;
    QSize m_size;

};

ImageDecoderQt::ReadContext::ReadContext(SharedBuffer* data, ImageList &target)
    : m_data(data->data(), data->size())
    , m_buffer(&m_data)
    , m_reader(&m_buffer)
    , m_target(target)
    , m_dataFormat(QImage::Format_Invalid)
{
    m_buffer.open(QIODevice::ReadOnly);
}


bool ImageDecoderQt::ReadContext::read()
{
    // Attempt to read out all images
    bool completed = false;
    while (true) {
        if (m_target.empty() || completed) {
            // Start a new image.
            if (!m_reader.canRead())
                return true;

            // Attempt to construct an empty image of the matching size and format
            // for efficient reading
            QImage newImage = m_dataFormat != QImage::Format_Invalid  ?
                          QImage(m_size, m_dataFormat) : QImage();
            m_target.push_back(ImageData(newImage));
            completed = false;
        }

        // read chunks
        switch (readImageLines(m_target.back())) {
        case IncrementalReadFailed:
            m_target.pop_back();
            return false;
        case IncrementalReadPartial:
            return true;
        case IncrementalReadComplete:
            completed = true;
            //store for next
            m_dataFormat = m_target.back().m_image.format();
            m_size = m_target.back().m_image.size();
            const bool supportsAnimation = m_reader.supportsAnimation();

            // No point in readinfg further
            if (!supportsAnimation)
                return true;

            break;
        }
    }
    return true;
}



ImageDecoderQt::ReadContext::IncrementalReadResult
        ImageDecoderQt::ReadContext::readImageLines(ImageData &imageData)
{
    // TODO: Implement incremental reading here,
    // set state to reflect complete header, etc.
    // For now, we read the whole image.

    const qint64 startPos = m_buffer.pos();
    // Oops, failed. Rewind.
    if (!m_reader.read(&imageData.m_image)) {
        m_buffer.seek(startPos);
        const bool gotHeader = imageData.m_image.size().width();

        // [Experimental] Did we manage to read the header?
        if (gotHeader)
            return IncrementalReadPartial;
        return IncrementalReadFailed;
    }
    imageData.m_duration = m_reader.nextImageDelay();
    return IncrementalReadComplete;
}

ImageDecoderQt::ImageDecoderQt(const QString& imageFormat)
    : m_hasAlphaChannel(false)
    , m_imageFormat(imageFormat)
{
}

ImageDecoderQt::~ImageDecoderQt()
{
}

bool ImageDecoderQt::hasFirstImageHeader() const
{
    return  !m_imageList.empty();
}

void ImageDecoderQt::setData(SharedBuffer* data, bool allDataReceived)
{
    if (m_failed)
        return;

    // Cache our own new data.
    ImageDecoder::setData(data, allDataReceived);

    // No progressive loading possible
    if (!allDataReceived)
        return;

    ReadContext readContext(data, m_imageList);

    const bool result =  readContext.read();

    if (hasFirstImageHeader())
        m_hasAlphaChannel = m_imageList[0].m_image.hasAlphaChannel();

    if (!result)
        setFailed();
    else if (hasFirstImageHeader()) {
        QSize imgSize = m_imageList[0].m_image.size();
        setSize(imgSize.width(), imgSize.height());

        if (readContext.reader()->supportsAnimation()) {
            if (readContext.reader()->loopCount() != -1)
                m_loopCount = readContext.reader()->loopCount();
            else
                m_loopCount = 0; //loop forever
        }
    }
}


bool ImageDecoderQt::isSizeAvailable()
{
    return ImageDecoder::isSizeAvailable();
}

size_t ImageDecoderQt::frameCount()
{
    return m_imageList.size();
}

int ImageDecoderQt::repetitionCount() const
{
    return m_loopCount;
}

bool ImageDecoderQt::supportsAlpha() const
{
    return m_hasAlphaChannel;
}

int ImageDecoderQt::duration(size_t index) const
{
    if (index >= static_cast<size_t>(m_imageList.size()))
        return 0;
    return  m_imageList[index].m_duration;
}

String ImageDecoderQt::filenameExtension() const
{
    return m_imageFormat;
};

RGBA32Buffer* ImageDecoderQt::frameBufferAtIndex(size_t)
{
    Q_ASSERT("use imageAtIndex instead");
    return 0;
}

QPixmap* ImageDecoderQt::imageAtIndex(size_t index) const
{
    if (index >= static_cast<size_t>(m_imageList.size()))
        return 0;

    if (!m_pixmapCache.contains(index)) {
        m_pixmapCache.insert(index,
                             QPixmap::fromImage(m_imageList[index].m_image));

        // store null image since the converted pixmap is already in pixmap cache
        Q_ASSERT(m_imageList[index].m_imageState == ImageComplete);
        m_imageList[index].m_image = QImage();
    }
    return  &m_pixmapCache[index];
}

void ImageDecoderQt::clearFrame(size_t index)
{
    if (m_imageList.size() < (int)index)
        m_imageList[index].m_image = QImage();
    m_pixmapCache.take(index);
}

}

// vim: ts=4 sw=4 et
