/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
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

#ifdef SVG_SUPPORT

#import "DOMSVGPoint.h"

#import "DOMInternal.h"
#import "FloatPoint.h"

#define IMPL reinterpret_cast<WebCore::FloatPoint*>(_internal)

@implementation DOMSVGPoint

- (void)dealloc
{
    delete IMPL;
    [super dealloc];
}

- (void)finalize
{
    delete IMPL;
    [super finalize];
}

- (float)x
{
    return IMPL->x();
}

- (void)setX:(float)newX
{
    IMPL->setX(newX);
}

- (float)y
{
    return IMPL->y();
}

- (void)setY:(float)newY
{
    IMPL->setY(newY);
}

- (DOMSVGPoint *)matrixTransform:(DOMSVGMatrix *)matrix
{
    // FIXME: IMPLEMENT ME
    return [DOMSVGPoint _SVGPointWith:WebCore::FloatPoint()];
}

@end

@implementation DOMSVGPoint (WebCoreInternal)

- (WebCore::FloatPoint)_SVGPoint
{
    return *(IMPL);
}

- (id)_initWithFloatPoint:(WebCore::FloatPoint)value
{
    // FIXME: Implement Caching
    [super _init];
    WebCore::FloatPoint *impl = new WebCore::FloatPoint(value);
    _internal = reinterpret_cast<DOMObjectInternal*>(impl);
    return self;
}

+ (DOMSVGPoint *)_SVGPointWith:(WebCore::FloatPoint)impl
{
    // FIXME: Implement Caching
    return [[[self alloc] _initWithFloatPoint:impl] autorelease];
}

@end

#endif // SVG_SUPPORT

