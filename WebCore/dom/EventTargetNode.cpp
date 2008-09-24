/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *           (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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
#include "EventTargetNode.h"

#include "Document.h"
#include "EventException.h"
#include "EventHandler.h"
#include "EventListener.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameView.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "MutationEvent.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "ProgressEvent.h"
#include "RegisteredEventListener.h"
#include "ScriptController.h"
#include "TextEvent.h"
#include "WebKitAnimationEvent.h"
#include "WebKitTransitionEvent.h"
#include "WheelEvent.h"
#include <wtf/HashSet.h>

#if ENABLE(DOM_STORAGE)
#include "StorageEvent.h"
#endif

namespace WebCore {

using namespace EventNames;
    
static HashSet<EventTargetNode*>* gNodesDispatchingSimulatedClicks = 0; 

EventTargetNode::EventTargetNode(Document* doc, bool isElement)
    : Node(doc, isElement)
    , m_regdListeners(0)
{
}

EventTargetNode::~EventTargetNode()
{
    if (m_regdListeners && !m_regdListeners->isEmpty() && !inDocument())
        document()->unregisterDisconnectedNodeWithEventListeners(this);

    delete m_regdListeners;
    m_regdListeners = 0;
}

void EventTargetNode::insertedIntoDocument()
{
    if (m_regdListeners && !m_regdListeners->isEmpty())
        document()->unregisterDisconnectedNodeWithEventListeners(this);

    Node::insertedIntoDocument();
}

void EventTargetNode::removedFromDocument()
{
    if (m_regdListeners && !m_regdListeners->isEmpty())
        document()->registerDisconnectedNodeWithEventListeners(this);

    Node::removedFromDocument();
}

void EventTargetNode::willMoveToNewOwnerDocument()
{
    if (m_regdListeners && !m_regdListeners->isEmpty())
        document()->unregisterDisconnectedNodeWithEventListeners(this);

    Node::willMoveToNewOwnerDocument();
}

void EventTargetNode::didMoveToNewOwnerDocument()
{
    if (m_regdListeners && !m_regdListeners->isEmpty())
        document()->registerDisconnectedNodeWithEventListeners(this);

    Node::didMoveToNewOwnerDocument();
}

void EventTargetNode::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> listener, bool useCapture)
{
    Document* doc = document();
    if (!doc->attached())
        return;

    doc->addListenerTypeIfNeeded(eventType);

    if (!m_regdListeners)
        m_regdListeners = new RegisteredEventListenerList;

    // Remove existing identical listener set with identical arguments.
    // The DOM2 spec says that "duplicate instances are discarded" in this case.
    removeEventListener(eventType, listener.get(), useCapture);

    // adding the first one
    if (m_regdListeners->isEmpty() && !inDocument())
        doc->registerDisconnectedNodeWithEventListeners(this);

    m_regdListeners->append(RegisteredEventListener::create(eventType, listener, useCapture));
}

void EventTargetNode::removeEventListener(const AtomicString& eventType, EventListener* listener, bool useCapture)
{
    if (!m_regdListeners)
        return;

    RegisteredEventListenerList::Iterator end = m_regdListeners->end();
    for (RegisteredEventListenerList::Iterator it = m_regdListeners->begin(); it != end; ++it) {
        RegisteredEventListener& r = **it;
        if (r.eventType() == eventType && r.listener() == listener && r.useCapture() == useCapture) {
            (*it)->setRemoved(true);
            it = m_regdListeners->remove(it);

            // removed last
            if (m_regdListeners->isEmpty() && !inDocument())
                document()->unregisterDisconnectedNodeWithEventListeners(this);

            return;
        }
    }
}

void EventTargetNode::removeAllEventListeners()
{
    delete m_regdListeners;
    m_regdListeners = 0;
}

void EventTargetNode::handleLocalEvents(Event *evt, bool useCapture)
{
    if (disabled() && evt->isMouseEvent())
        return;

    if (!m_regdListeners || m_regdListeners->isEmpty())
        return;

    RegisteredEventListenerList listenersCopy = *m_regdListeners;
    RegisteredEventListenerList::Iterator end = listenersCopy.end();

    for (RegisteredEventListenerList::Iterator it = listenersCopy.begin(); it != end; ++it) {
        if ((*it)->eventType() == evt->type() && (*it)->useCapture() == useCapture && !(*it)->removed())
            (*it)->listener()->handleEvent(evt, false);
    }
}

static inline EventTargetNode* setCurrentEventTargetRespectingSVGTargetRules(EventTargetNode* referenceNode, Event* event)
{
    ASSERT(referenceNode);

    // TODO: SVG will add logic here soon.
    event->setCurrentTarget(referenceNode);
    return referenceNode;
}

bool EventTargetNode::dispatchEvent(PassRefPtr<Event> e, ExceptionCode& ec, bool tempEvent)
{
    RefPtr<Event> evt(e);
    ASSERT(!eventDispatchForbidden());
    if (!evt || evt->type().isEmpty()) { 
        ec = EventException::UNSPECIFIED_EVENT_TYPE_ERR;
        return false;
    }

    EventTarget* target = this;

    // TODO: SVG will add logic here soon.
    evt->setTarget(target);

    RefPtr<FrameView> view = document()->view();
    return dispatchGenericEvent(evt.release(), ec, tempEvent);
}

bool EventTargetNode::dispatchGenericEvent(PassRefPtr<Event> e, ExceptionCode& ec, bool tempEvent)
{
    RefPtr<Event> evt(e);

    ASSERT(!eventDispatchForbidden());
    ASSERT(evt->target());
    ASSERT(!evt->type().isNull()); // JavaScript code could create an event with an empty name

    // work out what nodes to send event to
    DeprecatedPtrList<Node> nodeChain;

    if (inDocument()) {
        for (Node* n = this; n; n = n->eventParentNode()) {
            n->ref();
            nodeChain.prepend(n);
        }
    } else {
        // if node is not in the document just send event to itself 
        ref();
        nodeChain.prepend(this);
    }

    DeprecatedPtrListIterator<Node> it(nodeChain);

    // Before we begin dispatching events, give the target node a chance to do some work prior
    // to the DOM event handlers getting a crack.
    void* data = preDispatchEventHandler(evt.get());

    // trigger any capturing event handlers on our way down
    evt->setEventPhase(Event::CAPTURING_PHASE);
    it.toFirst();

    // Handle window events for capture phase, except load events, this quirk is needed
    // because Mozilla used to never propagate load events to the window object
    if (evt->type() != loadEvent && it.current()->isDocumentNode() && !evt->propagationStopped())
        static_cast<Document*>(it.current())->handleWindowEvent(evt.get(), true);

    EventTargetNode* eventTargetNode = 0;
    for (; it.current() && it.current() != this && !evt->propagationStopped(); ++it) {
        eventTargetNode = setCurrentEventTargetRespectingSVGTargetRules(EventTargetNodeCast(it.current()), evt.get());
        eventTargetNode->handleLocalEvents(evt.get(), true);
    }

    // dispatch to the actual target node
    it.toLast();

    if (!evt->propagationStopped()) {
        evt->setEventPhase(Event::AT_TARGET);
        eventTargetNode = setCurrentEventTargetRespectingSVGTargetRules(EventTargetNodeCast(it.current()), evt.get());

        // We do want capturing event listeners to be invoked here, even though
        // that violates the specification since Mozilla does it.
        eventTargetNode->handleLocalEvents(evt.get(), true);

        eventTargetNode->handleLocalEvents(evt.get(), false);
    }

    --it;

    // ok, now bubble up again (only non-capturing event handlers will be called)
    // ### recalculate the node chain here? (e.g. if target node moved in document by previous event handlers)
    // no. the DOM specs says:
    // The chain of EventTargets from the event target to the top of the tree
    // is determined before the initial dispatch of the event.
    // If modifications occur to the tree during event processing,
    // event flow will proceed based on the initial state of the tree.
    //
    // since the initial dispatch is before the capturing phase,
    // there's no need to recalculate the node chain.
    // (tobias)

    if (evt->bubbles()) {
        evt->setEventPhase(Event::BUBBLING_PHASE);

        for (; it.current() && !evt->propagationStopped() && !evt->cancelBubble(); --it) {
            eventTargetNode = setCurrentEventTargetRespectingSVGTargetRules(EventTargetNodeCast(it.current()), evt.get());
            eventTargetNode->handleLocalEvents(evt.get(), false);
        }

        it.toFirst();

        // Handle window events for bubbling phase, except load events, this quirk is needed
        // because Mozilla used to never propagate load events at all
        if (evt->type() != loadEvent && it.current()->isDocumentNode() && !evt->propagationStopped() && !evt->cancelBubble()) {
            evt->setCurrentTarget(EventTargetNodeCast(it.current()));
            static_cast<Document*>(it.current())->handleWindowEvent(evt.get(), false);
        } 
    }

    evt->setCurrentTarget(0);
    evt->setEventPhase(0); // I guess this is correct, the spec does not seem to say
                           // anything about the default event handler phase.


    // Now call the post dispatch.
    postDispatchEventHandler(evt.get(), data);

    // now we call all default event handlers (this is not part of DOM - it is internal to WebCore)
    it.toLast();

    if (evt->bubbles())
        for (; it.current() && !evt->defaultPrevented() && !evt->defaultHandled(); --it)
            EventTargetNodeCast(it.current())->defaultEventHandler(evt.get());
    else if (!evt->defaultPrevented() && !evt->defaultHandled())
        EventTargetNodeCast(it.current())->defaultEventHandler(evt.get());

    // deref all nodes in chain
    it.toFirst();
    for (; it.current(); ++it)
        it.current()->deref(); // this may delete us

    Document::updateDocumentsRendering();

    // If tempEvent is true, this means that the DOM implementation
    // will not be storing a reference to the event, i.e.  there is no
    // way to retrieve it from javascript if a script does not already
    // have a reference to it in a variable.  So there is no need for
    // the interpreter to keep the event in it's cache
    Frame* frame = document()->frame();
    if (tempEvent && frame && frame->script()->isEnabled())
        frame->script()->finishedWithEvent(evt.get());

    return !evt->defaultPrevented(); // ### what if defaultPrevented was called before dispatchEvent?
}

bool EventTargetNode::dispatchSubtreeModifiedEvent()
{
    ASSERT(!eventDispatchForbidden());
    
    document()->incDOMTreeVersion();

    notifyNodeListsAttributeChanged(); // FIXME: Can do better some day. Really only care about the name attribute changing.
    
    if (!document()->hasListenerType(Document::DOMSUBTREEMODIFIED_LISTENER))
        return false;
    ExceptionCode ec = 0;
    return dispatchEvent(MutationEvent::create(DOMSubtreeModifiedEvent, true, false, 0, String(), String(), String(), 0), ec, true);
}

void EventTargetNode::dispatchWindowEvent(PassRefPtr<Event> e)
{
    ASSERT(!eventDispatchForbidden());
    RefPtr<Event> evt(e);
    RefPtr<Document> doc = document();
    evt->setTarget(doc);
    doc->handleWindowEvent(evt.get(), true);
    doc->handleWindowEvent(evt.get(), false);
}

void EventTargetNode::dispatchWindowEvent(const AtomicString& eventType, bool canBubbleArg, bool cancelableArg)
{
    ASSERT(!eventDispatchForbidden());
    RefPtr<Document> doc = document();
    dispatchWindowEvent(Event::create(eventType, canBubbleArg, cancelableArg));
    
    if (eventType == loadEvent) {
        // For onload events, send a separate load event to the enclosing frame only.
        // This is a DOM extension and is independent of bubbling/capturing rules of
        // the DOM.
        Element* ownerElement = doc->ownerElement();
        if (ownerElement) {
            RefPtr<Event> ownerEvent = Event::create(eventType, false, cancelableArg);
            ownerEvent->setTarget(ownerElement);
            ExceptionCode ec = 0;
            ownerElement->dispatchGenericEvent(ownerEvent.release(), ec, true);
        }
    }
}

bool EventTargetNode::dispatchUIEvent(const AtomicString& eventType, int detail, PassRefPtr<Event> underlyingEvent)
{
    ASSERT(!eventDispatchForbidden());
    ASSERT(eventType == DOMFocusInEvent || eventType == DOMFocusOutEvent || eventType == DOMActivateEvent);
    
    bool cancelable = eventType == DOMActivateEvent;
    
    ExceptionCode ec = 0;
    RefPtr<UIEvent> evt = UIEvent::create(eventType, true, cancelable, document()->defaultView(), detail);
    evt->setUnderlyingEvent(underlyingEvent);
    return dispatchEvent(evt.release(), ec, true);
}

bool EventTargetNode::dispatchKeyEvent(const PlatformKeyboardEvent& key)
{
    ASSERT(!eventDispatchForbidden());
    ExceptionCode ec = 0;
    RefPtr<KeyboardEvent> keyboardEvent = KeyboardEvent::create(key, document()->defaultView());
    bool r = dispatchEvent(keyboardEvent,ec,true);
    
    // we want to return false if default is prevented (already taken care of)
    // or if the element is default-handled by the DOM. Otherwise we let it just
    // let it get handled by AppKit 
    if (keyboardEvent->defaultHandled())
        r = false;
    
    return r;
}

bool EventTargetNode::dispatchMouseEvent(const PlatformMouseEvent& event, const AtomicString& eventType,
    int detail, Node* relatedTarget)
{
    ASSERT(!eventDispatchForbidden());
    
    IntPoint contentsPos;
    if (FrameView* view = document()->view())
        contentsPos = view->windowToContents(event.pos());

    short button = event.button();

    ASSERT(event.eventType() == MouseEventMoved || button != NoButton);
    
    return dispatchMouseEvent(eventType, button, detail,
        contentsPos.x(), contentsPos.y(), event.globalX(), event.globalY(),
        event.ctrlKey(), event.altKey(), event.shiftKey(), event.metaKey(),
        false, relatedTarget);
}

void EventTargetNode::dispatchSimulatedMouseEvent(const AtomicString& eventType,
    PassRefPtr<Event> underlyingEvent)
{
    ASSERT(!eventDispatchForbidden());

    bool ctrlKey = false;
    bool altKey = false;
    bool shiftKey = false;
    bool metaKey = false;
    if (UIEventWithKeyState* keyStateEvent = findEventWithKeyState(underlyingEvent.get())) {
        ctrlKey = keyStateEvent->ctrlKey();
        altKey = keyStateEvent->altKey();
        shiftKey = keyStateEvent->shiftKey();
        metaKey = keyStateEvent->metaKey();
    }

    // Like Gecko, we just pass 0 for everything when we make a fake mouse event.
    // Internet Explorer instead gives the current mouse position and state.
    dispatchMouseEvent(eventType, 0, 0, 0, 0, 0, 0,
        ctrlKey, altKey, shiftKey, metaKey, true, 0, underlyingEvent);
}

void EventTargetNode::dispatchSimulatedClick(PassRefPtr<Event> event, bool sendMouseEvents, bool showPressedLook)
{
    if (!gNodesDispatchingSimulatedClicks)
        gNodesDispatchingSimulatedClicks = new HashSet<EventTargetNode*>;
    else if (gNodesDispatchingSimulatedClicks->contains(this))
        return;
    
    gNodesDispatchingSimulatedClicks->add(this);
    
    // send mousedown and mouseup before the click, if requested
    if (sendMouseEvents)
        dispatchSimulatedMouseEvent(mousedownEvent, event.get());
    setActive(true, showPressedLook);
    if (sendMouseEvents)
        dispatchSimulatedMouseEvent(mouseupEvent, event.get());
    setActive(false);

    // always send click
    dispatchSimulatedMouseEvent(clickEvent, event);
    
    gNodesDispatchingSimulatedClicks->remove(this);
}

bool EventTargetNode::dispatchMouseEvent(const AtomicString& eventType, int button, int detail,
    int pageX, int pageY, int screenX, int screenY,
    bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, 
    bool isSimulated, Node* relatedTargetArg, PassRefPtr<Event> underlyingEvent)
{
    ASSERT(!eventDispatchForbidden());
    if (disabled()) // Don't even send DOM events for disabled controls..
        return true;
    
    if (eventType.isEmpty())
        return false; // Shouldn't happen.
    
    // Dispatching the first event can easily result in this node being destroyed.
    // Since we dispatch up to three events here, we need to make sure we're referenced
    // so the pointer will be good for the two subsequent ones.
    RefPtr<Node> protect(this);
    
    bool cancelable = eventType != mousemoveEvent;
    
    ExceptionCode ec = 0;
    
    bool swallowEvent = false;
    
    // Attempting to dispatch with a non-EventTarget relatedTarget causes the relatedTarget to be silently ignored.
    RefPtr<EventTargetNode> relatedTarget = (relatedTargetArg && relatedTargetArg->isEventTargetNode())
        ? static_cast<EventTargetNode*>(relatedTargetArg) : 0;

    if (Frame* frame = document()->frame()) {
        float pageZoom = frame->pageZoomFactor();
        if (pageZoom != 1.0f) {
            // Adjust our pageX and pageY to account for the page zoom.
            pageX = lroundf(pageX / pageZoom);
            pageY = lroundf(pageY / pageZoom);
        }
    }

    RefPtr<Event> mouseEvent = MouseEvent::create(eventType,
        true, cancelable, document()->defaultView(),
        detail, screenX, screenY, pageX, pageY,
        ctrlKey, altKey, shiftKey, metaKey, button,
        relatedTarget, 0, isSimulated);
    mouseEvent->setUnderlyingEvent(underlyingEvent.get());
    
    dispatchEvent(mouseEvent, ec, true);
    bool defaultHandled = mouseEvent->defaultHandled();
    bool defaultPrevented = mouseEvent->defaultPrevented();
    if (defaultHandled || defaultPrevented)
        swallowEvent = true;
    
    // Special case: If it's a double click event, we also send the dblclick event. This is not part
    // of the DOM specs, but is used for compatibility with the ondblclick="" attribute.  This is treated
    // as a separate event in other DOM-compliant browsers like Firefox, and so we do the same.
    if (eventType == clickEvent && detail == 2) {
        RefPtr<Event> doubleClickEvent = MouseEvent::create(dblclickEvent,
            true, cancelable, document()->defaultView(),
            detail, screenX, screenY, pageX, pageY,
            ctrlKey, altKey, shiftKey, metaKey, button,
            relatedTarget, 0, isSimulated);
        doubleClickEvent->setUnderlyingEvent(underlyingEvent.get());
        if (defaultHandled)
            doubleClickEvent->setDefaultHandled();
        dispatchEvent(doubleClickEvent, ec, true);
        if (doubleClickEvent->defaultHandled() || doubleClickEvent->defaultPrevented())
            swallowEvent = true;
    }

    return swallowEvent;
}

void EventTargetNode::dispatchWheelEvent(PlatformWheelEvent& e)
{
    ASSERT(!eventDispatchForbidden());
    if (e.deltaX() == 0 && e.deltaY() == 0)
        return;
    
    FrameView* view = document()->view();
    if (!view)
        return;
    
    IntPoint pos = view->windowToContents(e.pos());
    
    RefPtr<WheelEvent> we = WheelEvent::create(e.deltaX(), e.deltaY(),
        document()->defaultView(), e.globalX(), e.globalY(), pos.x(), pos.y(),
        e.ctrlKey(), e.altKey(), e.shiftKey(), e.metaKey());
    ExceptionCode ec = 0;
    if (!dispatchEvent(we.release(), ec, true))
        e.accept();
}

bool EventTargetNode::dispatchWebKitAnimationEvent(const AtomicString& eventType, const String& animationName, double elapsedTime)
{
    ASSERT(!eventDispatchForbidden());
    
    ExceptionCode ec = 0;
    return dispatchEvent(WebKitAnimationEvent::create(eventType, animationName, elapsedTime), ec, true);
}

bool EventTargetNode::dispatchWebKitTransitionEvent(const AtomicString& eventType, const String& propertyName, double elapsedTime)
{
    ASSERT(!eventDispatchForbidden());
    
    ExceptionCode ec = 0;
    return dispatchEvent(WebKitTransitionEvent::create(eventType, propertyName, elapsedTime), ec, true);
}

void EventTargetNode::dispatchFocusEvent()
{
    dispatchEventForType(focusEvent, false, false);
}

void EventTargetNode::dispatchBlurEvent()
{
    dispatchEventForType(blurEvent, false, false);
}

bool EventTargetNode::dispatchEventForType(const AtomicString& eventType, bool canBubbleArg, bool cancelableArg)
{
    ASSERT(!eventDispatchForbidden());
    ExceptionCode ec = 0;
    return dispatchEvent(Event::create(eventType, canBubbleArg, cancelableArg), ec, true);
}

bool EventTargetNode::dispatchProgressEvent(const AtomicString &eventType, bool lengthComputableArg, unsigned loadedArg, unsigned totalArg)
{
    ASSERT(!eventDispatchForbidden());
    ExceptionCode ec = 0;
    return dispatchEvent(ProgressEvent::create(eventType, lengthComputableArg, loadedArg, totalArg), ec, true);
}

void EventTargetNode::dispatchStorageEvent(const AtomicString &eventType, const String& key, const String& oldValue, const String& newValue, Frame* source)
{
#if ENABLE(DOM_STORAGE)
    ASSERT(!eventDispatchForbidden());
    ExceptionCode ec = 0;
    dispatchEvent(StorageEvent::create(eventType, key, oldValue, newValue, source->document()->documentURI(), source->domWindow()), ec, true); 
#endif
}

void EventTargetNode::removeEventListenerForType(const AtomicString& eventType)
{
    if (!m_regdListeners) // nothing to remove
        return;
    
    RegisteredEventListenerList::Iterator end = m_regdListeners->end();
    for (RegisteredEventListenerList::Iterator it = m_regdListeners->begin(); it != end; ++it) {
        if ((*it)->eventType() != eventType || !(*it)->listener()->isAttachedToEventTargetNode())
            continue;
        
        it = m_regdListeners->remove(it);

        // removed last
        if (m_regdListeners->isEmpty() && !inDocument())
            document()->unregisterDisconnectedNodeWithEventListeners(this);

        return;
    }
}

void EventTargetNode::setEventListenerForType(const AtomicString& eventType, PassRefPtr<EventListener> listener)
{
    // In case we are the only one holding a reference to it, we don't want removeEventListenerForType to destroy it.
    removeEventListenerForType(eventType);
    if (listener)
        addEventListener(eventType, listener, false);
}

void EventTargetNode::setEventListenerForTypeAndAttribute(const AtomicString& eventType, Attribute* attr)
{
    setEventListenerForType(eventType, document()->createEventListener(attr->localName().string(), attr->value(), this));
}

EventListener* EventTargetNode::eventListenerForType(const AtomicString& eventType)
{
    if (!m_regdListeners)
        return 0;
    
    RegisteredEventListenerList::Iterator end = m_regdListeners->end();
    for (RegisteredEventListenerList::Iterator it = m_regdListeners->begin(); it != end; ++it)
        if ((*it)->eventType() == eventType && (*it)->listener()->isAttachedToEventTargetNode())
            return (*it)->listener();
    return 0;
}

bool EventTargetNode::disabled() const
{
    return false;
}

void EventTargetNode::defaultEventHandler(Event* event)
{
    if (event->target() != this)
        return;
    const AtomicString& eventType = event->type();
    if (eventType == keydownEvent || eventType == keypressEvent) {
        if (event->isKeyboardEvent())
            if (Frame* frame = document()->frame())
                frame->eventHandler()->defaultKeyboardEventHandler(static_cast<KeyboardEvent*>(event));
    } else if (eventType == clickEvent) {
        int detail = event->isUIEvent() ? static_cast<UIEvent*>(event)->detail() : 0;
        dispatchUIEvent(DOMActivateEvent, detail, event);
    } else if (eventType == contextmenuEvent) {
        if (Frame* frame = document()->frame())
            if (Page* page = frame->page())
                page->contextMenuController()->handleContextMenuEvent(event);
    } else if (eventType == textInputEvent) {
        if (event->isTextEvent())
            if (Frame* frame = document()->frame())
                frame->eventHandler()->defaultTextInputEventHandler(static_cast<TextEvent*>(event));
    }
}

} // namespace WebCore
