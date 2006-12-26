/*
 * Copyright (C) 2005, 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifdef SVG_SUPPORT
#import "SVGResourceFilter.h"
#import "SVGResourceMasker.h"
#import "SVGRenderStyle.h"

#import "GraphicsContext.h"
#import "CgSupport.h"

#import <QuartzCore/CoreImage.h>
#import <QuartzCore/CIFilter.h>

namespace WebCore {

static CIImage* applyLuminanceToAlphaFilter(CIImage* inputImage)
{
    CIFilter* luminanceToAlpha = [CIFilter filterWithName:@"CIColorMatrix"];
    [luminanceToAlpha setDefaults];
    CGFloat alpha[4] = {0.2125, 0.7154, 0.0721, 0};
    CGFloat zero[4] = {0, 0, 0, 0};
    [luminanceToAlpha setValue:inputImage forKey:@"inputImage"];  
    [luminanceToAlpha setValue:[CIVector vectorWithValues:zero count:4] forKey:@"inputRVector"];
    [luminanceToAlpha setValue:[CIVector vectorWithValues:zero count:4] forKey:@"inputGVector"];
    [luminanceToAlpha setValue:[CIVector vectorWithValues:zero count:4] forKey:@"inputBVector"];
    [luminanceToAlpha setValue:[CIVector vectorWithValues:alpha count:4] forKey:@"inputAVector"];
    [luminanceToAlpha setValue:[CIVector vectorWithValues:zero count:4] forKey:@"inputBiasVector"];
    return [luminanceToAlpha valueForKey:@"outputImage"];
}

static CIImage* applyExpandAlphatoGrayscaleFilter(CIImage* inputImage)
{
    CIFilter* alphaToGrayscale = [CIFilter filterWithName:@"CIColorMatrix"];
    CGFloat zero[4] = {0, 0, 0, 0};
    [alphaToGrayscale setDefaults];
    [alphaToGrayscale setValue:inputImage forKey:@"inputImage"];
    [alphaToGrayscale setValue:[CIVector vectorWithValues:zero count:4] forKey:@"inputRVector"];
    [alphaToGrayscale setValue:[CIVector vectorWithValues:zero count:4] forKey:@"inputGVector"];
    [alphaToGrayscale setValue:[CIVector vectorWithValues:zero count:4] forKey:@"inputBVector"];
    [alphaToGrayscale setValue:[CIVector vectorWithX:0.0 Y:0.0 Z:0.0 W:1.0] forKey:@"inputAVector"];
    [alphaToGrayscale setValue:[CIVector vectorWithX:1.0 Y:1.0 Z:1.0 W:0.0] forKey:@"inputBiasVector"];
    return [alphaToGrayscale valueForKey:@"outputImage"];
}

static CIImage* transformImageIntoGrayscaleMask(CIImage* inputImage)
{
    CIFilter* blackBackground = [CIFilter filterWithName:@"CIConstantColorGenerator"];
    [blackBackground setValue:[CIColor colorWithRed:0.0 green:0.0 blue:0.0 alpha:1.0] forKey:@"inputColor"];

    CIFilter* layerOverBlack = [CIFilter filterWithName:@"CISourceOverCompositing"];
    [layerOverBlack setValue:[blackBackground valueForKey:@"outputImage"] forKey:@"inputBackgroundImage"];  
    [layerOverBlack setValue:inputImage forKey:@"inputImage"];  

    CIImage* luminanceAlpha = applyLuminanceToAlphaFilter([layerOverBlack valueForKey:@"outputImage"]);
    CIImage* luminanceAsGrayscale = applyExpandAlphatoGrayscaleFilter(luminanceAlpha);
    CIImage* alphaAsGrayscale = applyExpandAlphatoGrayscaleFilter(inputImage);

    CIFilter* multipliedGrayscale = [CIFilter filterWithName:@"CIMultiplyCompositing"];
    [multipliedGrayscale setValue:luminanceAsGrayscale forKey:@"inputBackgroundImage"];  
    [multipliedGrayscale setValue:alphaAsGrayscale forKey:@"inputImage"];  
    return [multipliedGrayscale valueForKey:@"outputImage"];
}

void SVGResourceMasker::applyMask(GraphicsContext* context, const FloatRect& boundingBox) const
{
    if (!m_mask)
        return;

    IntSize maskSize = m_mask->size();

    // The mask we operate on is has it's top left corner at (0, 0) on the CGImage.
    // We have to translate to the current relative bbox, to get the clipping right.
    CGRect maskDestinationRect = CGRectMake(lroundf(boundingBox.x()), lroundf(boundingBox.y()),
                                            maskSize.width(), maskSize.height());

    // Create new graphics context in gray scale mode for image rendering
    OwnPtr<ImageBuffer> grayScaleImage(GraphicsContext::createImageBuffer(maskSize, true));
    if (!grayScaleImage)
        return;
    CGContextRef grayScaleContext = grayScaleImage->context()->platformContext();

    // Wrap CG context in CI context
    CIContext* ciGrayscaleContext = [CIContext contextWithCGContext:grayScaleContext options:nil];

    // Transform colorized mask to gray scale
    CIImage* grayScaleMask = transformImageIntoGrayscaleMask([CIImage imageWithCGImage:m_mask->cgImage()]);
    [ciGrayscaleContext drawImage:grayScaleMask atPoint:CGPointZero fromRect:CGRectMake(0, 0, maskSize.width(), maskSize.height())];

    // Do the actual masking!
    CGContextClipToMask(context->platformContext(), maskDestinationRect, grayScaleImage->cgImage());
}

} // namespace WebCore

#endif // SVG_SUPPORT
