/*
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
#include "Image.h"

#include "FloatRect.h"
#include "Image.h"
#include "ImageAnimationObserver.h"
#include "IntRect.h"
#include "PlatformString.h"
#include "Timer.h"
#include <wtf/Vector.h>

#if __APPLE__
// FIXME: Will go away when we make PDF a subclass.
#include "PDFDocumentImage.h"
#endif

namespace WebCore {

// ================================================
// Image Class
// ================================================

Image::Image()
: m_currentFrame(0), m_frames(0), m_animationObserver(0),
  m_frameTimer(0), m_repetitionCount(0), m_repetitionsComplete(0),
  m_isSolidColor(false), m_animatingImageType(true), m_animationFinished(false),
  m_haveSize(false), m_sizeAvailable(false)
{
    initNativeData();
}

Image::Image(ImageAnimationObserver* observer, bool isPDF)
 : m_currentFrame(0), m_frames(0), m_animationObserver(0),
  m_frameTimer(0), m_repetitionCount(0), m_repetitionsComplete(0),
  m_isSolidColor(false), m_animatingImageType(true), m_animationFinished(false),
  m_haveSize(false), m_sizeAvailable(false)
{
    initNativeData();
#if __APPLE__
    if (isPDF)
        setIsPDF(); // FIXME: Will go away when we make PDF a subclass.
#endif
    m_animationObserver = observer;
}

Image::~Image()
{
    invalidateData();
    stopAnimation();
    destroyNativeData();
}

void Image::invalidateData()
{
    // Destroy the cached images and release them.
    if (m_frames.size()) {
        m_frames.last().clear();
        m_isSolidColor = false;
        invalidateNativeData();
    }
}

void Image::cacheFrame(size_t index)
{
    size_t numFrames = frameCount();
    if (!m_frames.size() && shouldAnimate()) {            
        // Snag the repetition count.
        m_repetitionCount = m_source.repetitionCount();
        if (m_repetitionCount == cAnimationNone)
            m_animatingImageType = false;
    }
    
    if (m_frames.size() < numFrames)
        m_frames.resize(numFrames);

    m_frames[index].m_frame = m_source.createFrameAtIndex(index);
    checkForSolidColor();

    if (shouldAnimate())
        m_frames[index].m_duration = m_source.frameDurationAtIndex(index);
    m_frames[index].m_hasAlpha = m_source.frameHasAlphaAtIndex(index);
}

bool Image::isNull() const
{
    return size().isEmpty();
}

IntSize Image::size() const
{
#if __APPLE__
    // FIXME: Will go away when we make PDF a subclass.
    if (m_isPDF) {
        if (m_PDFDoc) {
            FloatSize size = m_PDFDoc->size();
            return IntSize((int)size.width(), (int)size.height());
        }
    } else
#endif
    
    if (m_sizeAvailable && !m_haveSize) {
        m_size = m_source.size();
        m_haveSize = true;
    }
    return m_size;
}

bool Image::setData(bool allDataReceived)
{
    int length = m_data.size();
    if (!length)
        return true;

#ifdef kImageBytesCutoff
    // This is a hack to help with testing display of partially-loaded images.
    // To enable it, define kImageBytesCutoff to be a size smaller than that of the image files
    // being loaded. They'll never finish loading.
    if (length > kImageBytesCutoff) {
        length = kImageBytesCutoff;
        allDataReceived = false;
    }
#endif
    
#if __APPLE__
    // Avoid the extra copy of bytes by just handing the byte array directly to a CFDataRef.
    CFDataRef data = CFDataCreateWithBytesNoCopy(0, reinterpret_cast<const UInt8*>(m_data.data()), length, kCFAllocatorNull);
    bool result = setNativeData(data, allDataReceived);
    CFRelease(data);
#else
    bool result = setNativeData(&m_data, allDataReceived);
#endif

    return result;
}

bool Image::setNativeData(NativeBytePtr data, bool allDataReceived)
{
#if __APPLE__
    // FIXME: Will go away when we make PDF a subclass.
    if (m_isPDF) {
        if (allDataReceived && !m_PDFDoc)
            m_PDFDoc = new PDFDocumentImage(data);
        return m_PDFDoc;
    }
#endif

    invalidateData();
    
    // Feed all the data we've seen so far to the image decoder.
    m_source.setData(data, allDataReceived);
    
    // Image properties will not be available until the first frame of the file
    // reaches kCGImageStatusIncomplete.
    return isSizeAvailable();
}

size_t Image::frameCount()
{
    return m_source.frameCount();
}

bool Image::isSizeAvailable()
{
    if (m_sizeAvailable)
        return true;

    m_sizeAvailable = m_source.isSizeAvailable();

    return m_sizeAvailable;

}

NativeImagePtr Image::frameAtIndex(size_t index)
{
    if (index >= frameCount())
        return 0;

    if (index >= m_frames.size() || !m_frames[index].m_frame)
        cacheFrame(index);

    return m_frames[index].m_frame;
}

float Image::frameDurationAtIndex(size_t index)
{
    if (index >= frameCount())
        return 0;

    if (index >= m_frames.size() || !m_frames[index].m_frame)
        cacheFrame(index);

    return m_frames[index].m_duration;
}

bool Image::frameHasAlphaAtIndex(size_t index)
{
    if (index >= frameCount())
        return 0;

    if (index >= m_frames.size() || !m_frames[index].m_frame)
        cacheFrame(index);

    return m_frames[index].m_hasAlpha;
}

bool Image::shouldAnimate()
{
    return (m_animatingImageType && frameCount() > 1 && !m_animationFinished && m_animationObserver);
}

void Image::startAnimation()
{
    if (m_frameTimer || !shouldAnimate())
        return;

    m_frameTimer = new Timer<Image>(this, &Image::advanceAnimation);
    m_frameTimer->startOneShot(frameDurationAtIndex(m_currentFrame));
}

void Image::stopAnimation()
{
    // This timer is used to animate all occurrences of this image.  Don't invalidate
    // the timer unless all renderers have stopped drawing.
    delete m_frameTimer;
    m_frameTimer = 0;
}

void Image::resetAnimation()
{
    stopAnimation();
    m_currentFrame = 0;
    m_repetitionsComplete = 0;
    m_animationFinished = false;
}


void Image::advanceAnimation(Timer<Image>* timer)
{
    // Stop the animation.
    stopAnimation();
    
    // See if anyone is still paying attention to this animation.  If not, we don't
    // advance and will simply pause the animation.
    if (animationObserver()->shouldStopAnimation(this))
        return;

    m_currentFrame++;
    if (m_currentFrame >= frameCount()) {
        m_repetitionsComplete += 1;
        if (m_repetitionCount && m_repetitionsComplete >= m_repetitionCount) {
            m_animationFinished = false;
            m_currentFrame--;
            return;
        }
        m_currentFrame = 0;
    }

    // Notify our observer that the animation has advanced.
    animationObserver()->animationAdvanced(this);
        
    // Kick off a timer to move to the next frame.
    m_frameTimer = new Timer<Image>(this, &Image::advanceAnimation);
    m_frameTimer->startOneShot(frameDurationAtIndex(m_currentFrame));
}

IntRect Image::rect() const
{
    return IntRect(IntPoint(), size());
}

int Image::width() const
{
    return size().width();
}

int Image::height() const
{
    return size().height();
}

}
