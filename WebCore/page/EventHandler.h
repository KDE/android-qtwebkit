/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef EventHandler_h
#define EventHandler_h

#include "PlatformMouseEvent.h"
#include "ScrollTypes.h"
#include "Timer.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/Platform.h>
#include <wtf/RefPtr.h>

#if PLATFORM(MAC)
#include "WebCoreKeyboardAccess.h"
#ifndef __OBJC__
typedef unsigned NSDragOperation;
class NSView;
#endif
#endif

namespace WebCore {

class AtomicString;
class Clipboard;
class EventTargetNode;
class Event;
class FloatPoint;
class FloatRect;
class Frame;
class HitTestResult;
class HTMLFrameSetElement;
class KeyboardEvent;
class MouseEventWithHitTestResults;
class Node;
class PlatformScrollbar;
class PlatformWheelEvent;
class RenderLayer;
class RenderObject;
class RenderWidget;
class VisiblePosition;
class Widget;

enum SelectionDirection { SelectingNext, SelectingPrevious };

class EventHandler : Noncopyable {
public:
    EventHandler(Frame*);
    ~EventHandler();

    void clear();

    void updateSelectionForMouseDragOverPosition(const VisiblePosition&);

    Node* mousePressNode() const;
    void setMousePressNode(PassRefPtr<Node>);

    void stopAutoscrollTimer(bool rendererIsBeingDestroyed = false);
    RenderObject* autoscrollRenderer() const;

    HitTestResult hitTestResultAtPoint(const IntPoint&, bool allowShadowContent);

    bool mousePressed() const { return m_mousePressed; }
    void setMousePressed(bool pressed) { m_mousePressed = pressed; }

    bool advanceFocus(KeyboardEvent*);

    bool updateDragAndDrop(const PlatformMouseEvent&, Clipboard*);
    void cancelDragAndDrop(const PlatformMouseEvent&, Clipboard*);
    bool performDragAndDrop(const PlatformMouseEvent&, Clipboard*);

    void scheduleHoverStateUpdate();

    void setResizingFrameSet(HTMLFrameSetElement*);

    IntPoint currentMousePosition() const;

    void setIgnoreWheelEvents(bool);

    bool scrollOverflow(ScrollDirection, ScrollGranularity);

    bool shouldDragAutoNode(Node*, const IntPoint&) const; // -webkit-user-drag == auto

    bool tabsToLinks(KeyboardEvent*) const;
    bool tabsToAllControls(KeyboardEvent*) const;

    bool mouseDownMayStartSelect() const { return m_mouseDownMayStartSelect; }
    bool inputManagerHasMarkedText() const;

    void handleMousePressEvent(const PlatformMouseEvent&);
    void handleMouseMoveEvent(const PlatformMouseEvent&);
    void handleMouseReleaseEvent(const PlatformMouseEvent&);
    void handleWheelEvent(PlatformWheelEvent&);

#if PLATFORM(MAC)

    NSView *nextKeyView(Node*, SelectionDirection);
    NSView *nextKeyViewInFrameHierarchy(Node*, SelectionDirection);
    static NSView *nextKeyView(Widget*, SelectionDirection);

    PassRefPtr<KeyboardEvent> currentKeyboardEvent() const;

    static bool currentEventIsMouseDownInWidget(Widget*);

    void mouseDown(NSEvent*);
    void mouseDragged(NSEvent*);
    void mouseUp(NSEvent*);
    void mouseMoved(NSEvent*);
    bool keyEvent(NSEvent*);
    bool wheelEvent(NSEvent*);
    bool sendContextMenuEvent(NSEvent*);

    bool eventMayStartDrag(NSEvent*) const;

    void sendFakeEventsAfterWidgetTracking(NSEvent* initiatingEvent);

    void setActivationEventNumber(int num) { m_activationEventNumber = num; }

    void dragSourceMovedTo(const PlatformMouseEvent&);
    void dragSourceEndedAt(const PlatformMouseEvent&, NSDragOperation);

    NSEvent *currentNSEvent();

#endif

private:
    void selectClosestWordFromMouseEvent(const PlatformMouseEvent&, Node* innerNode);

    void handleMouseDoubleClickEvent(const PlatformMouseEvent&);

    void handleMousePressEvent(const MouseEventWithHitTestResults&);
    void handleMousePressEventSingleClick(const MouseEventWithHitTestResults&);
    void handleMousePressEventDoubleClick(const MouseEventWithHitTestResults&);
    void handleMousePressEventTripleClick(const MouseEventWithHitTestResults&);
    void handleMouseMoveEvent(const MouseEventWithHitTestResults&);
    void handleMouseReleaseEvent(const MouseEventWithHitTestResults&);

    void hoverTimerFired(Timer<EventHandler>*);

    bool lastEventIsMouseUp() const;

    static bool canMouseDownStartSelect(Node*);

    void handleAutoscroll(RenderObject*);
    void startAutoscrollTimer();
    void setAutoscrollRenderer(RenderObject*);

    void autoscrollTimerFired(Timer<EventHandler>*);

    void invalidateClick();

    Node* nodeUnderMouse() const;

    MouseEventWithHitTestResults prepareMouseEvent(bool readonly, bool active, bool mouseMove, const PlatformMouseEvent&);

    bool dispatchMouseEvent(const AtomicString& eventType, Node* target,
        bool cancelable, int clickCount, const PlatformMouseEvent&, bool setUnder);
    bool dispatchDragEvent(const AtomicString& eventType, Node* target,
        const PlatformMouseEvent&, Clipboard*);

    void freeClipboard();

    void focusDocumentView();

    bool handleDrag(const MouseEventWithHitTestResults&);
    bool handleMouseUp(const MouseEventWithHitTestResults&);

    bool dispatchDragSrcEvent(const AtomicString& eventType, const PlatformMouseEvent&);

    bool dragHysteresisExceeded(const FloatPoint&) const;

    bool passMousePressEventToSubframe(MouseEventWithHitTestResults&, Frame* subframe);
    bool passMouseMoveEventToSubframe(MouseEventWithHitTestResults&, Frame* subframe);
    bool passMouseReleaseEventToSubframe(MouseEventWithHitTestResults&, Frame* subframe);
    bool passWheelEventToSubframe(PlatformWheelEvent&, Frame* subframe);

    bool passSubframeEventToSubframe(MouseEventWithHitTestResults&, Frame* subframe);

    bool passMousePressEventToScrollbar(MouseEventWithHitTestResults&, PlatformScrollbar*);

    bool passWidgetMouseDownEventToWidget(const MouseEventWithHitTestResults&);
    bool passWidgetMouseDownEventToWidget(RenderWidget*);

    bool passMouseDownEventToWidget(Widget*);
    bool passWheelEventToWidget(Widget*);
    
#if PLATFORM(MAC)
    WebCoreKeyboardUIMode keyboardUIMode() const;

    NSView *mouseDownViewIfStillGood();
    NSView *nextKeyViewInFrame(Node*, SelectionDirection, bool* focusCallResultedInViewBeingCreated = 0);
#endif

    Frame* m_frame;

    bool m_bMousePressed;
    bool m_mousePressed;
    RefPtr<Node> m_mousePressNode;

    bool m_beganSelectingText;

    IntPoint m_dragStartPos;

    Timer<EventHandler> m_hoverTimer;
    
    Timer<EventHandler> m_autoscrollTimer;
    RenderObject* m_autoscrollRenderer;
    bool m_mouseDownMayStartAutoscroll;
    bool m_mouseDownMayStartDrag;

    RenderLayer* m_resizeLayer;

    RefPtr<Node> m_nodeUnderMouse;
    RefPtr<Node> m_lastNodeUnderMouse;
    RefPtr<Frame> m_lastMouseMoveEventSubframe;
    RefPtr<PlatformScrollbar> m_lastScrollbarUnderMouse;

    bool m_ignoreWheelEvents;

    int m_clickCount;
    RefPtr<Node> m_clickNode;

    RefPtr<Node> m_dragTarget;
    
    RefPtr<HTMLFrameSetElement> m_frameSetBeingResized;

    IntSize m_offsetFromResizeCorner;    
    
    IntPoint m_currentMousePosition;

    bool m_mouseDownWasInSubframe;
    bool m_mouseDownMayStartSelect;

#if PLATFORM(MAC)
    NSView *m_mouseDownView;
    bool m_sendingEventToSubview;
    PlatformMouseEvent m_mouseDown;
    IntPoint m_mouseDownPos; // in our view's coords
    double m_mouseDownTimestamp;
    int m_activationEventNumber;
#endif

};

} // namespace WebCore

#endif // EventHandler_h
