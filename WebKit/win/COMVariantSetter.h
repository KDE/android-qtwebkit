/*
 * Copyright (C) 2007, 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef COMVariantSetter_h
#define COMVariantSetter_h

#include <WebCore/BString.h>
#include <WebCore/COMPtr.h>
#include <wtf/Assertions.h>

namespace WebCore {
    class String;
}

template<typename T> struct COMVariantSetter {};

template<> struct COMVariantSetter<WebCore::String>
{
    static const VARENUM VariantType = VT_BSTR;

    static void setVariant(VARIANT* variant, const WebCore::String& value)
    {
        ASSERT(V_VT(variant) == VT_EMPTY);

        V_VT(variant) = VariantType;
        V_BSTR(variant) = WebCore::BString(value).release();
    }
};

template<> struct COMVariantSetter<unsigned long long>
{
    static const VARENUM VariantType = VT_UI8;

    static void setVariant(VARIANT* variant, unsigned long long value)
    {
        ASSERT(V_VT(variant) == VT_EMPTY);

        V_VT(variant) = VariantType;
        V_UI8(variant) = value;
    }
};

template<> struct COMVariantSetter<int>
{
    static const VARENUM VariantType = VT_I4;

    static void setVariant(VARIANT* variant, int value)
    {
        ASSERT(V_VT(variant) == VT_EMPTY);

        V_VT(variant) = VariantType;
        V_I4(variant) = value;
    }
};

template<typename T> struct COMVariantSetter<COMPtr<T> >
{
    static const VARENUM VariantType = VT_UNKNOWN;

    static void setVariant(VARIANT* variant, const COMPtr<T>& value)
    {
        ASSERT(V_VT(variant) == VT_EMPTY);

        V_VT(variant) = VariantType;
        V_UNKNOWN(variant) = value.get();
        value->AddRef();
    }
};

template<typename COMType, typename UnderlyingType>
struct COMIUnknownVariantSetter
{
    static const VARENUM VariantType = VT_UNKNOWN;

    static void setVariant(VARIANT* variant, const UnderlyingType& value)
    {
        ASSERT(V_VT(variant) == VT_EMPTY);

        V_VT(variant) = VariantType;
        V_UNKNOWN(variant) = COMType::createInstance(value);
    }
};

#endif // COMVariantSetter
