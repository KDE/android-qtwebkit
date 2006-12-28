/*
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

#import "config.h"
#import "BitmapImage.h"

#import "FloatRect.h"
#import "FoundationExtras.h"
#import "GraphicsContext.h"
#import "PlatformString.h"
#import "WebCoreFrameBridge.h"
#import "WebCoreSystemInterface.h"

namespace WebCore {

void BitmapImage::initPlatformData()
{
    m_nsImage = 0;
    m_tiffRep = 0;
}

void BitmapImage::invalidatePlatformData()
{
    if (m_frames.size() != 1)
        return;

    if (m_nsImage) {
        CFRelease(m_nsImage);
        m_nsImage = 0;
    }

    if (m_tiffRep) {
        CFRelease(m_tiffRep);
        m_tiffRep = 0;
    }
}

Image* Image::loadPlatformResource(const char *name)
{
    NSBundle *bundle = [NSBundle bundleForClass:[WebCoreFrameBridge class]];
    NSString *imagePath = [bundle pathForResource:[NSString stringWithUTF8String:name] ofType:@"tiff"];
    NSData *namedImageData = [NSData dataWithContentsOfFile:imagePath];
    if (namedImageData) {
        Image* image = new BitmapImage;
        image->setNativeData((CFDataRef)namedImageData, true);
        return image;
    }
    return 0;
}

CFDataRef BitmapImage::getTIFFRepresentation()
{
    if (m_tiffRep)
        return m_tiffRep;
    
    unsigned numFrames = frameCount();
    
    // If numFrames is zero, we know for certain this image doesn't have valid data
    // Even though the call to CGImageDestinationCreateWithData will fail and we'll handle it gracefully,
    // in certain circumstances that call will spam the console with an error message
    if (!numFrames)
        return 0;
    CFMutableDataRef data = CFDataCreateMutable(0, 0);
    // FIXME:  Use type kCGImageTypeIdentifierTIFF constant once is becomes available in the API
    CGImageDestinationRef destination = CGImageDestinationCreateWithData(data, CFSTR("public.tiff"), numFrames, 0);
    if (!destination)
        return 0;

    for (unsigned i = 0; i < numFrames; ++i ) {
        CGImageRef cgImage = frameAtIndex(i);
        if (!cgImage) {
            CFRelease(destination);
            return 0;    
        }
        CGImageDestinationAddImage(destination, cgImage, 0);
    }
    CGImageDestinationFinalize(destination);
    CFRelease(destination);

    m_tiffRep = data;
    return m_tiffRep;
}

NSImage* BitmapImage::getNSImage()
{
    if (m_nsImage)
        return m_nsImage;

    CFDataRef data = getTIFFRepresentation();
    if (!data)
        return 0;
    
    m_nsImage = HardRetainWithNSRelease([[NSImage alloc] initWithData:(NSData*)data]);
    return m_nsImage;
}

}
