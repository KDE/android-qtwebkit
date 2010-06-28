/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#if ENABLE(PROGRESS_TAG)

#include "RenderProgress.h"

#include "HTMLNames.h"
#include "HTMLProgressElement.h"
#include "RenderTheme.h"
#include "ShadowElement.h"
#include <wtf/CurrentTime.h>
#include <wtf/RefPtr.h>

using namespace std;

namespace WebCore {

RenderProgress::RenderProgress(HTMLProgressElement* element)
    : RenderIndicator(element)
    , m_position(-1)
    , m_animationStartTime(0)
    , m_animationRepeatInterval(0)
    , m_animationDuration(0)
    , m_animating(false)
    , m_animationTimer(this, &RenderProgress::animationTimerFired)
{
}

RenderProgress::~RenderProgress()
{
    if (m_valuePart)
        m_valuePart->detach();
}

void RenderProgress::updateFromElement()
{
    HTMLProgressElement* element = progressElement();
    if (m_position == element->position())
        return;
    m_position = element->position();

    updateAnimationState();
    RenderIndicator::updateFromElement();
}

double RenderProgress::animationProgress() const
{
    return m_animating ? (fmod((currentTime() - m_animationStartTime), m_animationDuration) / m_animationDuration) : 0;
}

bool RenderProgress::isDeterminate() const
{
    return 0 <= position();
}

void RenderProgress::animationTimerFired(Timer<RenderProgress>*)
{
    repaint();
}

void RenderProgress::paint(PaintInfo& paintInfo, int tx, int ty)
{
    if (paintInfo.phase == PaintPhaseBlockBackground) {
        if (!m_animationTimer.isActive() && m_animating)
            m_animationTimer.startOneShot(m_animationRepeatInterval);
    }

    RenderIndicator::paint(paintInfo, tx, ty);
}

void RenderProgress::layoutParts()
{
    updatePartsState();
    if (m_valuePart)
        m_valuePart->layoutAsPart(valuePartRect());
    updateAnimationState();
}

bool RenderProgress::shouldHaveParts() const
{
    if (!style()->hasAppearance())
        return true;
    if (ShadowBlockElement::partShouldHaveStyle(this, PROGRESS_BAR_VALUE))
        return true;
    return false;
}

void RenderProgress::updatePartsState()
{
    if (shouldHaveParts() && !m_valuePart) {
        style()->setAppearance(NoControlPart);
        m_valuePart = ShadowBlockElement::createForPart(this->node(), PROGRESS_BAR_VALUE);
        addChild(m_valuePart->renderer());
    } else if (!shouldHaveParts() && m_valuePart) {
        m_valuePart->detach();
        m_valuePart = 0;
    }
}

void RenderProgress::updateAnimationState()
{
    m_animationDuration = theme()->animationDurationForProgressBar(this);
    m_animationRepeatInterval = theme()->animationRepeatIntervalForProgressBar(this);

    bool animating = style()->hasAppearance() && m_animationDuration > 0;
    if (animating == m_animating)
        return;

    m_animating = animating;
    if (m_animating) {
        m_animationStartTime = currentTime();
        m_animationTimer.startOneShot(m_animationRepeatInterval);
    } else
        m_animationTimer.stop();
}

IntRect RenderProgress::valuePartRect() const
{
    IntRect rect(borderLeft() + paddingLeft(), borderTop() + paddingTop(), lround((width() - borderLeft() - paddingLeft() - borderRight() - paddingRight()) * position()), height()  - borderTop() - paddingTop() - borderBottom() - paddingBottom());
    if (style()->direction() == RTL)
        rect.setX(width() - borderRight() - paddingRight() - rect.width());
    return rect;
}

HTMLProgressElement* RenderProgress::progressElement() const
{
    return static_cast<HTMLProgressElement*>(node());
}    

} // namespace WebCore

#endif
