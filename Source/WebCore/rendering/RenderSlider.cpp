/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "RenderSlider.h"

#include "CSSPropertyNames.h"
#include "CSSStyleSelector.h"
#include "Document.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "Frame.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "MediaControlElements.h"
#include "MouseEvent.h"
#include "RenderLayer.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "ShadowElement.h"
#include "SliderThumbElement.h"
#include "StepRange.h"
#include <wtf/MathExtras.h>

using std::min;

namespace WebCore {

static const int defaultTrackLength = 129;

// Returns a value between 0 and 1.
static double sliderPosition(HTMLInputElement* element)
{
    StepRange range(element);
    return range.proportionFromValue(range.valueFromElement(element));
}

RenderSlider::RenderSlider(HTMLInputElement* element)
    : RenderBlock(element)
{
}

RenderSlider::~RenderSlider()
{
    if (m_thumb)
        m_thumb->detach();
}

int RenderSlider::baselinePosition(FontBaseline, bool /*firstLine*/, LineDirectionMode, LinePositionMode) const
{
    // FIXME: Patch this function for writing-mode.
    return height() + marginTop();
}

void RenderSlider::computePreferredLogicalWidths()
{
    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;

    if (style()->width().isFixed() && style()->width().value() > 0)
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = computeContentBoxLogicalWidth(style()->width().value());
    else
        m_maxPreferredLogicalWidth = defaultTrackLength * style()->effectiveZoom();

    if (style()->minWidth().isFixed() && style()->minWidth().value() > 0) {
        m_maxPreferredLogicalWidth = max(m_maxPreferredLogicalWidth, computeContentBoxLogicalWidth(style()->minWidth().value()));
        m_minPreferredLogicalWidth = max(m_minPreferredLogicalWidth, computeContentBoxLogicalWidth(style()->minWidth().value()));
    } else if (style()->width().isPercent() || (style()->width().isAuto() && style()->height().isPercent()))
        m_minPreferredLogicalWidth = 0;
    else
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth;
    
    if (style()->maxWidth().isFixed() && style()->maxWidth().value() != undefinedLength) {
        m_maxPreferredLogicalWidth = min(m_maxPreferredLogicalWidth, computeContentBoxLogicalWidth(style()->maxWidth().value()));
        m_minPreferredLogicalWidth = min(m_minPreferredLogicalWidth, computeContentBoxLogicalWidth(style()->maxWidth().value()));
    }

    int toAdd = borderAndPaddingWidth();
    m_minPreferredLogicalWidth += toAdd;
    m_maxPreferredLogicalWidth += toAdd;

    setPreferredLogicalWidthsDirty(false); 
}

void RenderSlider::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);

    if (m_thumb)
        m_thumb->renderer()->setStyle(createThumbStyle(style()));
}

PassRefPtr<RenderStyle> RenderSlider::createThumbStyle(const RenderStyle* parentStyle)
{
    RefPtr<RenderStyle> thumbStyle = document()->styleSelector()->styleForElement(m_thumb.get(), style(), false);

    if (parentStyle->appearance() == SliderVerticalPart)
        thumbStyle->setAppearance(SliderThumbVerticalPart);
    else if (parentStyle->appearance() == SliderHorizontalPart)
        thumbStyle->setAppearance(SliderThumbHorizontalPart);
    else if (parentStyle->appearance() == MediaSliderPart)
        thumbStyle->setAppearance(MediaSliderThumbPart);
    else if (parentStyle->appearance() == MediaVolumeSliderPart)
        thumbStyle->setAppearance(MediaVolumeSliderThumbPart);

    return thumbStyle.release();
}

IntRect RenderSlider::thumbRect()
{
    if (!m_thumb)
        return IntRect();

    IntRect thumbRect;
    RenderBox* thumb = toRenderBox(m_thumb->renderer());

    thumbRect.setWidth(thumb->style()->width().calcMinValue(contentWidth()));
    thumbRect.setHeight(thumb->style()->height().calcMinValue(contentHeight()));

    double fraction = sliderPosition(static_cast<HTMLInputElement*>(node()));
    IntRect contentRect = contentBoxRect();
    if (style()->appearance() == SliderVerticalPart || style()->appearance() == MediaVolumeSliderPart) {
        thumbRect.setX(contentRect.x() + (contentRect.width() - thumbRect.width()) / 2);
        thumbRect.setY(contentRect.y() + static_cast<int>(nextafter((contentRect.height() - thumbRect.height()) + 1, 0) * (1 - fraction)));
    } else {
        thumbRect.setX(contentRect.x() + static_cast<int>(nextafter((contentRect.width() - thumbRect.width()) + 1, 0) * fraction));
        thumbRect.setY(contentRect.y() + (contentRect.height() - thumbRect.height()) / 2);
    }

    return thumbRect;
}

void RenderSlider::layout()
{
    ASSERT(needsLayout());

    RenderBox* thumb = m_thumb ? toRenderBox(m_thumb->renderer()) : 0;

    IntSize baseSize(borderAndPaddingWidth(), borderAndPaddingHeight());

    if (thumb) {
        // Allow the theme to set the size of the thumb.
        if (thumb->style()->hasAppearance()) {
            // FIXME: This should pass the style, not the renderer, to the theme.
            theme()->adjustSliderThumbSize(thumb);
        }

        baseSize.expand(thumb->style()->width().calcMinValue(0), thumb->style()->height().calcMinValue(0));
    }

    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());

    IntSize oldSize = size();

    setSize(baseSize);
    computeLogicalWidth();
    computeLogicalHeight();
    updateLayerTransform();

    if (thumb) {
        if (oldSize != size())
            thumb->setChildNeedsLayout(true, false);

        LayoutStateMaintainer statePusher(view(), this, size(), style()->isFlippedBlocksWritingMode());

        IntRect oldThumbRect = thumb->frameRect();

        thumb->layoutIfNeeded();

        IntRect rect = thumbRect();
        thumb->setFrameRect(rect);
        if (thumb->checkForRepaintDuringLayout())
            thumb->repaintDuringLayoutIfMoved(oldThumbRect);

        statePusher.pop();
        addOverflowFromChild(thumb);
    }

    repainter.repaintAfterLayout();    

    setNeedsLayout(false);
}

void RenderSlider::updateFromElement()
{
    // Layout will take care of the thumb's size and position.
    if (!m_thumb) {
        m_thumb = SliderThumbElement::create(static_cast<HTMLElement*>(node()));
        RefPtr<RenderStyle> thumbStyle = createThumbStyle(style());
        m_thumb->setRenderer(m_thumb->createRenderer(renderArena(), thumbStyle.get()));
        m_thumb->renderer()->setStyle(thumbStyle.release());
        m_thumb->setAttached();
        m_thumb->setInDocument();
        addChild(m_thumb->renderer());
    }
    setNeedsLayout(true);
}

bool RenderSlider::mouseEventIsInThumb(MouseEvent* evt)
{
    if (!m_thumb || !m_thumb->renderer())
        return false;

#if ENABLE(VIDEO)
    if (style()->appearance() == MediaSliderPart || style()->appearance() == MediaVolumeSliderPart) {
        MediaControlInputElement *sliderThumb = static_cast<MediaControlInputElement*>(m_thumb->renderer()->node());
        return sliderThumb->hitTest(evt->absoluteLocation());
    }
#endif

    FloatPoint localPoint = m_thumb->renderBox()->absoluteToLocal(evt->absoluteLocation(), false, true);
    IntRect thumbBounds = m_thumb->renderBox()->borderBoxRect();
    return thumbBounds.contains(roundedIntPoint(localPoint));
}

FloatPoint RenderSlider::mouseEventOffsetToThumb(MouseEvent* evt)
{
    ASSERT(m_thumb && m_thumb->renderer());
    FloatPoint localPoint = m_thumb->renderBox()->absoluteToLocal(evt->absoluteLocation(), false, true);
    IntRect thumbBounds = m_thumb->renderBox()->borderBoxRect();
    FloatPoint offset;
    offset.setX(thumbBounds.x() + thumbBounds.width() / 2 - localPoint.x());
    offset.setY(thumbBounds.y() + thumbBounds.height() / 2 - localPoint.y());
    return offset;
}

void RenderSlider::setValueForPosition(int position)
{
    if (!m_thumb || !m_thumb->renderer())
        return;

    HTMLInputElement* element = static_cast<HTMLInputElement*>(node());

    // Calculate the new value based on the position, and send it to the element.
    StepRange range(element);
    double fraction = static_cast<double>(position) / trackSize();
    if (style()->appearance() == SliderVerticalPart || style()->appearance() == MediaVolumeSliderPart)
        fraction = 1 - fraction;
    double value = range.clampValue(range.valueFromProportion(fraction));
    element->setValueFromRenderer(serializeForNumberType(value));

    // Also update the position if appropriate.
    if (position != currentPosition()) {
        setNeedsLayout(true);

        // FIXME: It seems like this could send extra change events if the same value is set
        // multiple times with no layout in between.
        element->dispatchFormControlChangeEvent();
    }
}

int RenderSlider::positionForOffset(const IntPoint& p)
{
    if (!m_thumb || !m_thumb->renderer())
        return 0;

    int position;
    if (style()->appearance() == SliderVerticalPart || style()->appearance() == MediaVolumeSliderPart)
        position = p.y() - m_thumb->renderBox()->height() / 2;
    else
        position = p.x() - m_thumb->renderBox()->width() / 2;
    
    return max(0, min(position, trackSize()));
}

int RenderSlider::currentPosition()
{
    ASSERT(m_thumb);
    ASSERT(m_thumb->renderer());

    if (style()->appearance() == SliderVerticalPart || style()->appearance() == MediaVolumeSliderPart)
        return toRenderBox(m_thumb->renderer())->y() - contentBoxRect().y();
    return toRenderBox(m_thumb->renderer())->x() - contentBoxRect().x();
}

int RenderSlider::trackSize()
{
    ASSERT(m_thumb);
    ASSERT(m_thumb->renderer());

    if (style()->appearance() == SliderVerticalPart || style()->appearance() == MediaVolumeSliderPart)
        return contentHeight() - m_thumb->renderBox()->height();
    return contentWidth() - m_thumb->renderBox()->width();
}

void RenderSlider::forwardEvent(Event* event)
{
    if (event->isMouseEvent()) {
        MouseEvent* mouseEvent = static_cast<MouseEvent*>(event);
        if (event->type() == eventNames().mousedownEvent && mouseEvent->button() == LeftButton) {
            if (!mouseEventIsInThumb(mouseEvent)) {
                IntPoint eventOffset = roundedIntPoint(absoluteToLocal(mouseEvent->absoluteLocation(), false, true));
                setValueForPosition(positionForOffset(eventOffset));
            }
        }
    }

    m_thumb->defaultEventHandler(event);
}

bool RenderSlider::inDragMode() const
{
    return m_thumb && m_thumb->inDragMode();
}

} // namespace WebCore
