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

#ifndef CompositeAnimation_h
#define CompositeAnimation_h

#include "AtomicString.h"

#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

class AnimationController;
class ImplicitAnimation;
class KeyframeAnimation;
class RenderObject;
class RenderStyle;

// A CompositeAnimation represents a collection of animations that are running
// on a single RenderObject, such as a number of properties transitioning at once.
class CompositeAnimation : public Noncopyable {
public:
    CompositeAnimation(AnimationController* animationController)
        : m_suspended(false)
        , m_animationController(animationController)
        , m_numStyleAvailableWaiters(0)
    {
    }
    
    ~CompositeAnimation();

    RenderStyle* animate(RenderObject*, const RenderStyle* currentStyle, RenderStyle* targetStyle);

    void setAnimating(bool);
    bool animating();

    bool hasAnimationForProperty(int prop) const { return m_transitions.contains(prop); }

    void resetTransitions(RenderObject*);
    void resetAnimations(RenderObject*);

    void cleanupFinishedAnimations(RenderObject*);

    void setAnimationStartTime(double t);
    void setTransitionStartTime(int property, double t);

    void suspendAnimations();
    void resumeAnimations();
    bool suspended() const { return m_suspended; }

    void overrideImplicitAnimations(int property);
    void resumeOverriddenImplicitAnimations(int property);

    void styleAvailable();

    bool isAnimatingProperty(int property, bool isRunningNow) const;

    void setWaitingForStyleAvailable(bool);

protected:
    void updateTransitions(RenderObject*, const RenderStyle* currentStyle, RenderStyle* targetStyle);
    void updateKeyframeAnimations(RenderObject*, const RenderStyle* currentStyle, RenderStyle* targetStyle);

    KeyframeAnimation* getKeyframeAnimation(const AtomicString&);

private:
    typedef HashMap<int, ImplicitAnimation*> CSSPropertyTransitionsMap;
    typedef HashMap<AtomicStringImpl*, KeyframeAnimation*>  AnimationNameMap;

    CSSPropertyTransitionsMap m_transitions;
    AnimationNameMap m_keyframeAnimations;
    bool m_suspended;
    AnimationController* m_animationController;
    unsigned m_numStyleAvailableWaiters;
};

} // namespace WebCore

#endif // CompositeAnimation_h
