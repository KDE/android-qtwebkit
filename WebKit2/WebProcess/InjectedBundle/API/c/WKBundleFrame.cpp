/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "WKBundleFrame.h"

#include "WKAPICast.h"
#include "WKBundleAPICast.h"
#include "WebFrame.h"
#include <JavaScriptCore/APICast.h>
#include <WebCore/Frame.h>

using namespace WebCore;
using namespace WebKit;

bool WKBundleFrameIsMainFrame(WKBundleFrameRef frameRef)
{
    return toWK(frameRef)->isMainFrame();
}

WKURLRef WKBundleFrameGetURL(WKBundleFrameRef frameRef)
{
    return toURLRef(toWK(frameRef)->url().impl());
}

WKStringRef WKBundleFrameCopyInnerText(WKBundleFrameRef frameRef)
{
    WebCore::String string = toWK(frameRef)->innerText();
    string.impl()->ref();
    return toRef(string.impl());
}

WKArrayRef WKBundleFrameCopyChildFrames(WKBundleFrameRef frameRef)
{
    return toRef(toWK(frameRef)->childFrames().releaseRef());    
}

unsigned WKBundleFrameGetNumberOfActiveAnimations(WKBundleFrameRef frameRef)
{
    return toWK(frameRef)->numberOfActiveAnimations();
}

bool WKBundleFramePauseAnimationOnElementWithId(WKBundleFrameRef frameRef, WKStringRef name, WKStringRef elementID, double time)
{
    return toWK(frameRef)->pauseAnimationOnElementWithId(toWK(name), toWK(elementID), time);
}

JSGlobalContextRef WKBundleFrameGetJavaScriptContext(WKBundleFrameRef frameRef)
{
    // FIXME: Is there a way to get this and know that it's a JSGlobalContextRef?
    // The const_cast here is a bit ugly.
    return const_cast<JSGlobalContextRef>(toRef(toWK(frameRef)->coreFrame()->script()->globalObject(mainThreadNormalWorld())->globalExec()));
}

WKStringRef WKBundleFrameCopyName(WKBundleFrameRef frameRef)
{
    WebCore::String string = toWK(frameRef)->name();
    string.impl()->ref();
    return toRef(string.impl());
}
