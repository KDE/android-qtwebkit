/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#include "config.h"
#include "ScrollbarThemeComposite.h"

#include "ChromeClient.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "Scrollbar.h"
#include "ScrollbarClient.h"
#include "Settings.h"

namespace WebCore {

#if PLATFORM(WIN)
static Page* pageForScrollView(ScrollView* view)
{
    if (!view)
        return 0;
    if (!view->isFrameView())
        return 0;
    FrameView* frameView = static_cast<FrameView*>(view);
    if (!frameView->frame())
        return 0;
    return frameView->frame()->page();
}
#endif

bool ScrollbarThemeComposite::paint(Scrollbar* scrollbar, GraphicsContext* graphicsContext, const IntRect& damageRect)
{
    // Create the ScrollbarControlPartMask based on the damageRect
    ScrollbarControlPartMask scrollMask = NoPart;

    IntRect backButtonStartPaintRect;
    IntRect backButtonEndPaintRect;
    IntRect forwardButtonStartPaintRect;
    IntRect forwardButtonEndPaintRect;
    if (hasButtons(scrollbar)) {
        backButtonStartPaintRect = backButtonRect(scrollbar, BackButtonStartPart, true);
        if (damageRect.intersects(backButtonStartPaintRect))
            scrollMask |= BackButtonStartPart;
        backButtonEndPaintRect = backButtonRect(scrollbar, BackButtonEndPart, true);
        if (damageRect.intersects(backButtonEndPaintRect))
            scrollMask |= BackButtonEndPart;
        forwardButtonStartPaintRect = forwardButtonRect(scrollbar, ForwardButtonStartPart, true);
        if (damageRect.intersects(forwardButtonStartPaintRect))
            scrollMask |= ForwardButtonStartPart;
        forwardButtonEndPaintRect = forwardButtonRect(scrollbar, ForwardButtonEndPart, true);
        if (damageRect.intersects(forwardButtonEndPaintRect))
            scrollMask |= ForwardButtonEndPart;
    }

    IntRect startTrackRect;
    IntRect thumbRect;
    IntRect endTrackRect;
    IntRect trackPaintRect = trackRect(scrollbar, true);
    bool thumbPresent = hasThumb(scrollbar);
    if (thumbPresent) {
        IntRect track = trackRect(scrollbar);
        splitTrack(scrollbar, track, startTrackRect, thumbRect, endTrackRect);
        if (damageRect.intersects(thumbRect)) {
            scrollMask |= ThumbPart;
            if (trackIsSinglePiece()) {
                // The track is a single strip that paints under the thumb.
                // Add both components of the track to the mask.
                scrollMask |= BackTrackPart;
                scrollMask |= ForwardTrackPart;
            }
        }
        if (damageRect.intersects(startTrackRect))
            scrollMask |= BackTrackPart;
        if (damageRect.intersects(endTrackRect))
            scrollMask |= ForwardTrackPart;
    } else if (damageRect.intersects(trackPaintRect)) {
        scrollMask |= BackTrackPart;
        scrollMask |= ForwardTrackPart;
    }

#if PLATFORM(WIN)
    // FIXME: This API makes the assumption that the custom scrollbar's metrics will match
    // the theme's metrics.  This is not a valid assumption.  The ability for a client to paint
    // custom scrollbars should be removed once scrollbars can be styled via CSS.
    if (Page* page = pageForScrollView(scrollbar->parent())) {
        if (page->settings()->shouldPaintCustomScrollbars()) {
            float proportion = static_cast<float>(scrollbar->visibleSize()) / scrollbar->totalSize();
            float value = scrollbar->currentPos() / static_cast<float>(scrollbar->maximum());
            ScrollbarControlState s = 0;
            if (scrollbar->client()->isActive())
                s |= ActiveScrollbarState;
            if (scrollbar->enabled())
                s |= EnabledScrollbarState;
            if (scrollbar->pressedPart() != NoPart)
                s |= PressedScrollbarState;
            if (page->chrome()->client()->paintCustomScrollbar(graphicsContext,
                                                               scrollbar->frameRect(), 
                                                               scrollbar->controlSize(),
                                                               s, 
                                                               scrollbar->pressedPart(), 
                                                               scrollbar->orientation() == VerticalScrollbar,
                                                               value,
                                                               proportion,
                                                               scrollMask))
                return true;
        }
    }
#endif

    // Paint the track.
    if ((scrollMask & ForwardTrackPart) || (scrollMask & BackTrackPart)) {
        if (!thumbPresent || trackIsSinglePiece())
            paintTrack(graphicsContext, scrollbar, trackPaintRect, ForwardTrackPart | BackTrackPart);
        else {
            if (scrollMask & BackTrackPart)
                paintTrack(graphicsContext, scrollbar, startTrackRect, BackTrackPart);
            if (scrollMask & ForwardTrackPart)
                paintTrack(graphicsContext, scrollbar, endTrackRect, ForwardTrackPart);
        }
    }

    // Paint the back and forward buttons.
    if (scrollMask & BackButtonStartPart)
        paintButton(graphicsContext, scrollbar, backButtonStartPaintRect, BackButtonStartPart);
    if (scrollMask & BackButtonEndPart)
        paintButton(graphicsContext, scrollbar, backButtonEndPaintRect, BackButtonEndPart);
    if (scrollMask & ForwardButtonStartPart)
        paintButton(graphicsContext, scrollbar, forwardButtonStartPaintRect, ForwardButtonStartPart);
    if (scrollMask & ForwardButtonEndPart)
        paintButton(graphicsContext, scrollbar, forwardButtonEndPaintRect, ForwardButtonEndPart);
    
    // Paint the thumb.
    if (scrollMask & ThumbPart)
        paintThumb(graphicsContext, scrollbar, thumbRect);

    return true;
}

ScrollbarPart ScrollbarThemeComposite::hitTest(Scrollbar* scrollbar, const PlatformMouseEvent& evt)
{
    ScrollbarPart result = NoPart;
    if (!scrollbar->enabled())
        return result;

    IntPoint mousePosition = scrollbar->convertFromContainingWindow(evt.pos());
    mousePosition.move(scrollbar->x(), scrollbar->y());
    if (backButtonRect(scrollbar, BackButtonStartPart).contains(mousePosition))
        result = BackButtonStartPart;
    else if (backButtonRect(scrollbar, BackButtonEndPart).contains(mousePosition))
        result = BackButtonEndPart;
    else if (forwardButtonRect(scrollbar, ForwardButtonStartPart).contains(mousePosition))
        result = ForwardButtonStartPart;
    else if (forwardButtonRect(scrollbar, ForwardButtonEndPart).contains(mousePosition))
        result = ForwardButtonEndPart;
    else {
        IntRect track = trackRect(scrollbar);
        if (track.contains(mousePosition)) {
            IntRect beforeThumbRect;
            IntRect thumbRect;
            IntRect afterThumbRect;
            splitTrack(scrollbar, track, beforeThumbRect, thumbRect, afterThumbRect);
            if (beforeThumbRect.contains(mousePosition))
                result = BackTrackPart;
            else if (thumbRect.contains(mousePosition))
                result = ThumbPart;
            else
                result = ForwardTrackPart;
        }
    }
    return result;
}

void ScrollbarThemeComposite::invalidatePart(Scrollbar* scrollbar, ScrollbarPart part)
{
    if (part == NoPart)
        return;

    IntRect result;    
    switch (part) {
        case BackButtonStartPart:
            result = backButtonRect(scrollbar, BackButtonStartPart, true);
            break;
        case BackButtonEndPart:
            result = backButtonRect(scrollbar, BackButtonEndPart, true);
            break;
        case ForwardButtonStartPart:
            result = forwardButtonRect(scrollbar, ForwardButtonStartPart, true);
            break;
        case ForwardButtonEndPart:
            result = forwardButtonRect(scrollbar, ForwardButtonEndPart, true);
            break;
        default: {
            IntRect beforeThumbRect, thumbRect, afterThumbRect;
            splitTrack(scrollbar, trackRect(scrollbar), beforeThumbRect, thumbRect, afterThumbRect);
            if (part == BackTrackPart)
                result = beforeThumbRect;
            else if (part == ForwardTrackPart)
                result = afterThumbRect;
            else
                result = thumbRect;
        }
    }
    result.move(-scrollbar->x(), -scrollbar->y());
    scrollbar->invalidateRect(result);
}

void ScrollbarThemeComposite::splitTrack(Scrollbar* scrollbar, const IntRect& trackRect, IntRect& beforeThumbRect, IntRect& thumbRect, IntRect& afterThumbRect)
{
    // This function won't even get called unless we're big enough to have some combination of these three rects where at least
    // one of them is non-empty.
    int thickness = scrollbarThickness();
    int thumbPos = thumbPosition(scrollbar);
    if (scrollbar->orientation() == HorizontalScrollbar) {
        thumbRect = IntRect(trackRect.x() + thumbPos, trackRect.y() + (trackRect.height() - thickness) / 2, thumbLength(scrollbar), thickness);
        beforeThumbRect = IntRect(trackRect.x(), trackRect.y(), thumbPos, trackRect.height());
        afterThumbRect = IntRect(thumbRect.x() + thumbRect.width(), trackRect.y(), trackRect.right() - thumbRect.right(), trackRect.height());
    } else {
        thumbRect = IntRect(trackRect.x() + (trackRect.width() - thickness) / 2, trackRect.y() + thumbPos, thickness, thumbLength(scrollbar));
        beforeThumbRect = IntRect(trackRect.x(), trackRect.y(), trackRect.width(), thumbPos);
        afterThumbRect = IntRect(trackRect.x(), thumbRect.y() + thumbRect.height(), trackRect.width(), trackRect.bottom() - thumbRect.bottom());
    }
}

int ScrollbarThemeComposite::thumbPosition(Scrollbar* scrollbar)
{
    if (scrollbar->enabled())
        return scrollbar->currentPos() * (trackLength(scrollbar) - thumbLength(scrollbar)) / scrollbar->maximum();
    return 0;
}

int ScrollbarThemeComposite::thumbLength(Scrollbar* scrollbar)
{
    if (!scrollbar->enabled())
        return 0;

    float proportion = (float)scrollbar->visibleSize() / scrollbar->totalSize();
    int trackLen = trackLength(scrollbar);
    int length = proportion * trackLen;
    length = max(length, minimumThumbLength(scrollbar));
    if (length > trackLen)
        length = 0; // Once the thumb is below the track length, it just goes away (to make more room for the track).
    return length;
}

int ScrollbarThemeComposite::minimumThumbLength(Scrollbar* scrollbar)
{
    return scrollbarThickness(scrollbar->controlSize());
}

int ScrollbarThemeComposite::trackPosition(Scrollbar* scrollbar)
{
    return (scrollbar->orientation() == HorizontalScrollbar) ? trackRect(scrollbar).x() - scrollbar->x() : trackRect(scrollbar).y() - scrollbar->y();
}

int ScrollbarThemeComposite::trackLength(Scrollbar* scrollbar)
{
    return (scrollbar->orientation() == HorizontalScrollbar) ? trackRect(scrollbar).width() : trackRect(scrollbar).height();
}

}
