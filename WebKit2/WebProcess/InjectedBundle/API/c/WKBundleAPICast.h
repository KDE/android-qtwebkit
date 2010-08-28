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

#ifndef WKBundleAPICast_h
#define WKBundleAPICast_h

#include "WKBundleBase.h"
#include "WKBundlePage.h"
#include "WKBundlePagePrivate.h"
#include "WKBundlePrivate.h"
#include <WebCore/EditorInsertAction.h>
#include <WebCore/TextAffinity.h>
#include <WebCore/UserContentTypes.h>
#include <WebCore/UserScriptTypes.h>
#include <WebCore/ZoomMode.h>

namespace WebCore {
    class CSSStyleDeclaration;
    class Node;
    class Range;
}

namespace WebKit {

class InjectedBundle;
class InjectedBundleNodeHandle;
class InjectedBundleScriptWorld;
class WebFrame;
class WebPage;

template<typename APIType> struct BundleAPITypeInfo { };
template<> struct BundleAPITypeInfo<WKBundleFrameRef>                   { typedef WebFrame* ImplType; };
template<> struct BundleAPITypeInfo<WKBundlePageRef>                    { typedef WebPage* ImplType; };
template<> struct BundleAPITypeInfo<WKBundleRef>                        { typedef InjectedBundle* ImplType; };
template<> struct BundleAPITypeInfo<WKBundleNodeHandleRef>              { typedef InjectedBundleNodeHandle* ImplType; };
template<> struct BundleAPITypeInfo<WKBundleScriptWorldRef>             { typedef InjectedBundleScriptWorld* ImplType; };
template<> struct BundleAPITypeInfo<WKBundleNodeRef>                    { typedef WebCore::Node* ImplType; };
template<> struct BundleAPITypeInfo<WKBundleRangeRef>                   { typedef WebCore::Range* ImplType; };
template<> struct BundleAPITypeInfo<WKBundleCSSStyleDeclarationRef>     { typedef WebCore::CSSStyleDeclaration* ImplType; };

template<typename ImplType> struct BundleImplTypeInfo { };
template<> struct BundleImplTypeInfo<InjectedBundle*>                   { typedef WKBundleRef APIType; };
template<> struct BundleImplTypeInfo<InjectedBundleNodeHandle*>         { typedef WKBundleNodeHandleRef APIType; };
template<> struct BundleImplTypeInfo<InjectedBundleScriptWorld*>        { typedef WKBundleScriptWorldRef APIType; };
template<> struct BundleImplTypeInfo<WebFrame*>                         { typedef WKBundleFrameRef APIType; };
template<> struct BundleImplTypeInfo<WebPage*>                          { typedef WKBundlePageRef APIType; };
template<> struct BundleImplTypeInfo<WebCore::CSSStyleDeclaration*>     { typedef WKBundleCSSStyleDeclarationRef APIType; };
template<> struct BundleImplTypeInfo<WebCore::Node*>                    { typedef WKBundleNodeRef APIType; };
template<> struct BundleImplTypeInfo<WebCore::Range*>                   { typedef WKBundleRangeRef APIType; };

} // namespace WebKit

/* Opaque typing convenience methods */

template<typename T>
inline typename WebKit::BundleAPITypeInfo<T>::ImplType toWK(T t)
{
    return reinterpret_cast<typename WebKit::BundleAPITypeInfo<T>::ImplType>(t);
}

template<typename T>
inline typename WebKit::BundleImplTypeInfo<T>::APIType toRef(T t)
{
    return reinterpret_cast<typename WebKit::BundleImplTypeInfo<T>::APIType>(t);
}

inline WKInsertActionType toWK(WebCore::EditorInsertAction action)
{
    switch (action) {
    case WebCore::EditorInsertActionTyped:
        return kWKInsertActionTyped;
        break;
    case WebCore::EditorInsertActionPasted:
        return kWKInsertActionPasted;
        break;
    case WebCore::EditorInsertActionDropped:
        return kWKInsertActionDropped;
        break;
    }
    ASSERT_NOT_REACHED();
    return kWKInsertActionTyped;
}

inline WKAffinityType toWK(WebCore::EAffinity affinity)
{
    switch (affinity) {
    case WebCore::UPSTREAM:
        return kWKAffinityUpstream;
        break;
    case WebCore::DOWNSTREAM:
        return kWKAffinityDownstream;
        break;
    }
    ASSERT_NOT_REACHED();
    return kWKAffinityUpstream;
}

inline WebCore::ZoomMode toZoomMode(WKBundlePageZoomMode wkZoomMode)
{
    switch (wkZoomMode) {
    case kWKBundlePageZoomModeTextOnly:
        return WebCore::ZoomTextOnly;
    case kWKBundlePageZoomModePage:
        return WebCore::ZoomPage;
    }

    ASSERT_NOT_REACHED();
    return WebCore::ZoomTextOnly;
}

inline WebCore::UserScriptInjectionTime toUserScriptInjectionTime(WKUserScriptInjectionTime wkInjectedTime)
{
    switch (wkInjectedTime) {
    case kWKInjectAtDocumentStart:
        return WebCore::InjectAtDocumentStart;
    case kWKInjectAtDocumentEnd:
        return WebCore::InjectAtDocumentEnd;
    }

    ASSERT_NOT_REACHED();
    return WebCore::InjectAtDocumentStart;
}

inline WebCore::UserContentInjectedFrames toUserContentInjectedFrames(WKUserContentInjectedFrames wkInjectedFrames)
{
    switch (wkInjectedFrames) {
    case kWKInjectInAllFrames:
        return WebCore::InjectInAllFrames;
    case kWKInjectInTopFrameOnly:
        return WebCore::InjectInTopFrameOnly;
    }

    ASSERT_NOT_REACHED();
    return WebCore::InjectInAllFrames;
}

#endif // WKBundleAPICast_h
