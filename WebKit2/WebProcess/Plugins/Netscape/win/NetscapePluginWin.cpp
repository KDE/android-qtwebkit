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

#include "NetscapePlugin.h"

#include "NotImplemented.h"
#include "WebEvent.h"
#include <WebCore/GraphicsContext.h>

using namespace WebCore;

namespace WebKit {

bool NetscapePlugin::platformPostInitialize()
{
    notImplemented();
    return true;
}

void NetscapePlugin::platformPaint(GraphicsContext* context, const IntRect& dirtyRect)
{
    // FIXME: Call SetWindow here if we haven't called it yet (see r59904).

    if (m_isWindowed) {
        // FIXME: Paint windowed plugins into context if context->shouldIncludeChildWindows() is true.
        return;
    }

    // FIXME: Support transparent plugins.
    HDC hdc = context->getWindowsContext(dirtyRect, false);

    m_npWindow.type = NPWindowTypeDrawable;
    m_npWindow.window = hdc;

    WINDOWPOS windowpos = { 0, 0, 0, 0, 0, 0, 0 };

    windowpos.x = m_frameRect.x();
    windowpos.y = m_frameRect.y();
    windowpos.cx = m_frameRect.width();
    windowpos.cy = m_frameRect.height();

    NPEvent npEvent;
    npEvent.event = WM_WINDOWPOSCHANGED;
    npEvent.wParam = 0;
    npEvent.lParam = reinterpret_cast<uintptr_t>(&windowpos);

    NPP_HandleEvent(&npEvent);

    callSetWindow();

    RECT dirtyWinRect = dirtyRect;

    npEvent.event = WM_PAINT;
    npEvent.wParam = reinterpret_cast<uintptr_t>(hdc);
    npEvent.lParam = reinterpret_cast<uintptr_t>(&dirtyWinRect);

    NPP_HandleEvent(&npEvent);

    // FIXME: Support transparent plugins.
    context->releaseWindowsContext(hdc, dirtyRect, false);
}

NPEvent toNP(const WebMouseEvent& event)
{
    NPEvent npEvent;

    npEvent.wParam = 0;
    if (event.controlKey())
        npEvent.wParam |= MK_CONTROL;
    if (event.shiftKey())
        npEvent.wParam |= MK_SHIFT;

    npEvent.lParam = MAKELPARAM(event.positionX(), event.positionY());

    switch (event.type()) {
    case WebEvent::MouseMove:
        npEvent.event = WM_MOUSEMOVE;
        switch (event.button()) {
        case WebMouseEvent::LeftButton:
            npEvent.wParam |= MK_LBUTTON;
            break;
        case WebMouseEvent::MiddleButton:
            npEvent.wParam |= MK_MBUTTON;
            break;
        case WebMouseEvent::RightButton:
            npEvent.wParam |= MK_RBUTTON;
            break;
        case WebMouseEvent::NoButton:
            break;
        }
        break;
    case WebEvent::MouseDown:
        switch (event.button()) {
        case WebMouseEvent::LeftButton:
            npEvent.event = WM_LBUTTONDOWN;
            break;
        case WebMouseEvent::MiddleButton:
            npEvent.event = WM_MBUTTONDOWN;
            break;
        case WebMouseEvent::RightButton:
            npEvent.event = WM_RBUTTONDOWN;
            break;
        case WebMouseEvent::NoButton:
            ASSERT_NOT_REACHED();
            break;
        }
        break;
    case WebEvent::MouseUp:
        switch (event.button()) {
        case WebMouseEvent::LeftButton:
            npEvent.event = WM_LBUTTONUP;
            break;
        case WebMouseEvent::MiddleButton:
            npEvent.event = WM_MBUTTONUP;
            break;
        case WebMouseEvent::RightButton:
            npEvent.event = WM_RBUTTONUP;
            break;
        case WebMouseEvent::NoButton:
            ASSERT_NOT_REACHED();
            break;
        }
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    return npEvent;
}

bool NetscapePlugin::platformHandleMouseEvent(const WebMouseEvent& event)
{
    if (m_isWindowed)
        return false;

    NPEvent npEvent = toNP(event);
    NPP_HandleEvent(&npEvent);
    return true;
}

bool NetscapePlugin::platformHandleWheelEvent(const WebWheelEvent&)
{
    notImplemented();
    return false;
}

void NetscapePlugin::platformSetFocus(bool)
{
    notImplemented();
}

bool NetscapePlugin::platformHandleMouseEnterEvent(const WebMouseEvent& event)
{
    if (m_isWindowed)
        return false;

    NPEvent npEvent = toNP(event);
    NPP_HandleEvent(&npEvent);
    return true;
}

bool NetscapePlugin::platformHandleMouseLeaveEvent(const WebMouseEvent& event)
{
    if (m_isWindowed)
        return false;

    NPEvent npEvent = toNP(event);
    NPP_HandleEvent(&npEvent);
    return true;
}

} // namespace WebKit
