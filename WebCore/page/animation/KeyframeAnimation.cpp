/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "KeyframeAnimation.h"
#include "CompositeAnimation.h"
#include "SystemTime.h"
#include "RenderObject.h"
#include "EventNames.h"

namespace WebCore {

void KeyframeAnimation::animate(CompositeAnimation* animation, RenderObject* renderer, const RenderStyle* currentStyle, 
                                    const RenderStyle* targetStyle, RenderStyle*& animatedStyle)
{
    // if we have not yet started, we will not have a valid start time, so just start the animation if needed
    if (isNew() && m_animation->playState() == AnimPlayStatePlaying)
        updateStateMachine(STATE_INPUT_START_ANIMATION, -1);
    
    // If we get this far and the animation is done, it means we are cleaning up a just finished animation.
    // If so, we need to send back the targetStyle
    if (postActive()) {
        if (!animatedStyle)
            animatedStyle = const_cast<RenderStyle*>(targetStyle);
        return;
    }

    // if we are waiting for the start timer, we don't want to change the style yet
    // Special case - if the delay time is 0, then we do want to set the first frame of the
    // animation right away. This avoids a flash when the animation starts
    if (waitingToStart() && m_animation->delay() > 0)
        return;
    
    // FIXME: we need to be more efficient about determining which keyframes we are animating between.
    // We should cache the last pair or something
    
    // find the first key
    double elapsedTime = (m_startTime > 0) ? ((!paused() ? currentTime() : m_pauseTime) - m_startTime) : 0.0;
    if (elapsedTime < 0.0)
        elapsedTime = 0.0;
        
    double t = m_animation->duration() ? (elapsedTime / m_animation->duration()) : 1.0;
    int i = (int) t;
    t -= i;
    if (m_animation->direction() && (i & 1))
        t = 1 - t;

    RenderStyle* fromStyle = 0;
    RenderStyle* toStyle = 0;
    double scale = 1.0;
    double offset = 0.0;
    if (m_keyframes.get()) {
        Vector<KeyframeValue>::const_iterator end = m_keyframes->endKeyframes();
        for (Vector<KeyframeValue>::const_iterator it = m_keyframes->beginKeyframes(); it != end; ++it) {
            if (t < it->key) {
                // The first key should always be 0, so we should never succeed on the first key
                if (!fromStyle)
                    break;
                scale = 1.0 / (it->key - offset);
                toStyle = const_cast<RenderStyle*>(&(it->style));
                break;
            }
            
            offset = it->key;
            fromStyle = const_cast<RenderStyle*>(&(it->style));
        }
    }
    
    // if either style is 0 we have an invalid case, just stop the animation
    if (!fromStyle || !toStyle) {
        updateStateMachine(STATE_INPUT_END_ANIMATION, -1);
        return;
    }
    
    // run a cycle of animation.
    // We know we will need a new render style, so make one if needed
    if (!animatedStyle)
        animatedStyle = new (renderer->renderArena()) RenderStyle(*targetStyle);
    
    double prog = progress(scale, offset);

    HashSet<int>::const_iterator end = m_keyframes->endProperties();
    for (HashSet<int>::const_iterator it = m_keyframes->beginProperties(); it != end; ++it) {
        if (blendProperties(*it, animatedStyle, fromStyle, toStyle, prog))
            setAnimating();
    }
}

void KeyframeAnimation::endAnimation(bool reset)
{
    // Restore the original (unanimated) style
    if (m_object)
        setChanged(m_object->element());
}

bool KeyframeAnimation::shouldSendEventForListener(Document::ListenerType inListenerType)
{
    return m_object->document()->hasListenerType(inListenerType);
}
    
void KeyframeAnimation::onAnimationStart(double inElapsedTime)
{
    sendAnimationEvent(EventNames::webkitAnimationStartEvent, inElapsedTime);
}

void KeyframeAnimation::onAnimationIteration(double inElapsedTime)
{
    sendAnimationEvent(EventNames::webkitAnimationIterationEvent, inElapsedTime);
}

void KeyframeAnimation::onAnimationEnd(double inElapsedTime)
{
    if (!sendAnimationEvent(EventNames::webkitAnimationEndEvent, inElapsedTime)) {
        // We didn't dispatch an event, which would call endAnimation(), so we'll just call it here.
        endAnimation(true);
    }
}

bool KeyframeAnimation::sendAnimationEvent(const AtomicString& inEventType, double inElapsedTime)
{
    Document::ListenerType listenerType;
    if (inEventType == EventNames::webkitAnimationIterationEvent)
        listenerType = Document::ANIMATIONITERATION_LISTENER;
    else if (inEventType == EventNames::webkitAnimationEndEvent)
        listenerType = Document::ANIMATIONEND_LISTENER;
    else
        listenerType = Document::ANIMATIONSTART_LISTENER;
    
    if (shouldSendEventForListener(listenerType)) {
        Element* element = elementForEventDispatch();
        if (element) {
            m_waitingForEndEvent = true;
            m_animationEventDispatcher.startTimer(element, m_name, -1, true, inEventType, inElapsedTime);
            return true; // Did dispatch an event
        }
    }

    return false; // Did not dispatch an event
}

void KeyframeAnimation::overrideAnimations()
{
    // This will override implicit animations that match the properties in the keyframe animation
    HashSet<int>::const_iterator end = m_keyframes->endProperties();
    for (HashSet<int>::const_iterator it = m_keyframes->beginProperties(); it != end; ++it)
        compositeAnimation()->overrideImplicitAnimations(*it);
}

void KeyframeAnimation::resumeOverriddenAnimations()
{
    // This will resume overridden implicit animations
    HashSet<int>::const_iterator end = m_keyframes->endProperties();
    for (HashSet<int>::const_iterator it = m_keyframes->beginProperties(); it != end; ++it)
        compositeAnimation()->resumeOverriddenImplicitAnimations(*it);
}

bool KeyframeAnimation::affectsProperty(int property) const
{
    HashSet<int>::const_iterator end = m_keyframes->endProperties();
    for (HashSet<int>::const_iterator it = m_keyframes->beginProperties(); it != end; ++it) {
        if ((*it) == property)
            return true;
    }
    return false;
}

}
