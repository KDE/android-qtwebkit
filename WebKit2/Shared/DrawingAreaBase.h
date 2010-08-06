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

#ifndef DrawingAreaBase_h
#define DrawingAreaBase_h

#include "ArgumentCoders.h"
#include "Connection.h"

namespace WebCore {
    class IntRect;
    class IntSize;
}

namespace WebKit {

class DrawingAreaBase {
public:
    enum Type {
        None,
        ChunkedUpdateDrawingAreaType,
#if USE(ACCELERATED_COMPOSITING)
        LayerBackedDrawingAreaType,
#endif
    };
    
    typedef uint64_t DrawingAreaID;
    
    virtual ~DrawingAreaBase() { }
    
    Type type() const { return m_type; }
    DrawingAreaID id() const { return m_id; }

    struct DrawingAreaInfo {
        Type type;
        DrawingAreaID id;

        DrawingAreaInfo(Type type = None, DrawingAreaID identifier = 0)
            : type(type)
            , id(identifier)
        {
        }
    };
    
    // The DrawingAreaProxy should never be decoded itself. Instead, the DrawingArea should be decoded.
    void encode(CoreIPC::ArgumentEncoder* encoder) const;
    static bool decode(CoreIPC::ArgumentDecoder*, DrawingAreaInfo&);

protected:
    DrawingAreaBase(Type type, DrawingAreaID identifier)
        : m_type(type)
        , m_id(identifier)
    {
    }

    Type m_type;
    DrawingAreaID m_id;
};

} // namespace WebKit

namespace CoreIPC {
template<> struct ArgumentCoder<WebKit::DrawingAreaBase::DrawingAreaInfo> : SimpleArgumentCoder<WebKit::DrawingAreaBase::DrawingAreaInfo> { };
}

#endif // DrawingAreaBase_h
