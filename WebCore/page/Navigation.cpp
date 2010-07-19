/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Navigation.h"

#if ENABLE(WEB_TIMING)

#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoaderTypes.h"

namespace WebCore {

Navigation::Navigation(Frame* frame)
    : m_frame(frame)
{
}

Frame* Navigation::frame() const
{
    return m_frame;
}

void Navigation::disconnectFrame()
{
    m_frame = 0;
}

unsigned short Navigation::type() const
{
    if (!m_frame)
        return NAVIGATE;

    DocumentLoader* documentLoader = m_frame->loader()->documentLoader();
    if (!documentLoader)
        return NAVIGATE;

    WebCore::NavigationType navigationType = documentLoader->triggeringAction().type();
    switch (navigationType) {
    case NavigationTypeReload:
        return RELOAD;
    case NavigationTypeBackForward:
        return BACK_FORWARD;
    default:
        return NAVIGATE;
    }
}

unsigned short Navigation::redirectCount() const
{
    if (!m_frame)
        return 0;

    DocumentLoader* loader = m_frame->loader()->documentLoader();
    if (!loader)
        return 0;

    return loader->timing()->redirectCount;
}

} // namespace WebCore

#endif // ENABLE(WEB_TIMING)
