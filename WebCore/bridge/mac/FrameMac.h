/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef FrameMac_h
#define FrameMac_h

#import "ClipboardAccessPolicy.h"
#import "Frame.h"
#import "PlatformMouseEvent.h"
#import "WebCoreKeyboardAccess.h"

class NPObject;

namespace KJS {
    namespace Bindings {
        class Instance;
        class RootObject;
    }
}

#ifdef __OBJC__

@class NSArray;
@class NSDictionary;
@class NSEvent;
@class NSFont;
@class NSImage;
@class NSMenu;
@class NSMutableDictionary;
@class NSString;
@class NSView;
@class WebCoreFrameBridge;
@class WebScriptObject;

#else

class NSArray;
class NSDictionary;
class NSEvent;
class NSFont;
class NSImage;
class NSMenu;
class NSMutableDictionary;
class NSString;
class NSView;
class WebCoreFrameBridge;
class WebScriptObject;

typedef unsigned int NSDragOperation;
typedef int NSWritingDirection;

#endif

namespace WebCore {

class ClipboardMac;
class HTMLTableCellElement;
class RenderWidget;

enum SelectionDirection {
    SelectingNext,
    SelectingPrevious
};

class FrameMac : public Frame {
    friend class Frame;

public:
    FrameMac(Page*, Element*, PassRefPtr<EditorClient>);
    ~FrameMac();

    void setBridge(WebCoreFrameBridge*);
    WebCoreFrameBridge* bridge() const { return _bridge; }

private:    
    WebCoreFrameBridge* _bridge;

// === undecided, may or may not belong here

public:
    virtual void setView(FrameView*);
    
    static WebCoreFrameBridge* bridgeForWidget(const Widget*);

    NSString* searchForLabelsAboveCell(RegularExpression*, HTMLTableCellElement*);
    NSString* searchForLabelsBeforeElement(NSArray* labels, Element*);
    NSString* matchLabelsAgainstElement(NSArray* labels, Element*);

    virtual KJS::Bindings::Instance* getEmbedInstanceForWidget(Widget*);
    virtual KJS::Bindings::Instance* getObjectInstanceForWidget(Widget*);
    virtual KJS::Bindings::Instance* getAppletInstanceForWidget(Widget*);
    virtual KJS::Bindings::RootObject* bindingRootObject();

    void addPluginRootObject(KJS::Bindings::RootObject*);

    KJS::Bindings::RootObject* executionContextForDOM();
    
    WebScriptObject* windowScriptObject();
    NPObject* windowScriptNPObject();
    
    NSMutableDictionary* dashboardRegionsDictionary();
    void dashboardRegionsChanged();

    void willPopupMenu(NSMenu *);

    void cleanupPluginObjects();
    bool shouldClose();

private:    
    KJS::Bindings::RootObject* _bindingRoot; // The root object used for objects bound outside the context of a plugin.
    Vector<KJS::Bindings::RootObject*> m_rootObjects;
    WebScriptObject* _windowScriptObject;
    NPObject* _windowScriptNPObject;

// === to be moved into Chrome

public:
    virtual void setStatusBarText(const String&);

    virtual void focusWindow();
    virtual void unfocusWindow();

    virtual void addMessageToConsole(const String& message, unsigned int lineNumber, const String& sourceID);
    
    virtual void runJavaScriptAlert(const String&);
    virtual bool runJavaScriptConfirm(const String&);
    virtual bool runJavaScriptPrompt(const String& message, const String& defaultValue, String& result);
    virtual bool shouldInterruptJavaScript();    

    FloatRect customHighlightLineRect(const AtomicString& type, const FloatRect& lineRect);
    void paintCustomHighlight(const AtomicString& type, const FloatRect& boxRect, const FloatRect& lineRect, bool text, bool line);

    virtual void print();

    virtual void scheduleClose();

// === to be moved into Editor

public:
    void advanceToNextMisspelling(bool startBeforeSelection = false);

    NSFont* fontForSelection(bool* hasMultipleFonts) const;
    NSDictionary* fontAttributesForSelectionStart() const;
    
    NSWritingDirection baseWritingDirectionForSelectionStart() const;

    virtual void markMisspellingsInAdjacentWords(const VisiblePosition&);
    virtual void markMisspellings(const Selection&);

    bool canDHTMLCut();
    bool canDHTMLCopy();
    bool canDHTMLPaste();
    bool tryDHTMLCut();
    bool tryDHTMLCopy();
    bool tryDHTMLPaste();
    
    virtual void issueCutCommand();
    virtual void issueCopyCommand();
    virtual void issuePasteCommand();
    virtual void issuePasteAndMatchStyleCommand();
    virtual void issueTransposeCommand();
    virtual void respondToChangedSelection(const Selection& oldSelection, bool closeTyping);
    virtual bool isContentEditable() const;
    virtual bool shouldChangeSelection(const Selection& oldSelection, const Selection& newSelection, EAffinity affinity, bool stillSelecting) const;
    virtual bool shouldDeleteSelection(const Selection&) const;
    
    virtual void setSecureKeyboardEntry(bool);
    virtual bool isSecureKeyboardEntry();

    void setMarkedTextRange(const Range* , NSArray* attributes, NSArray* ranges);
    virtual Range* markedTextRange() const { return m_markedTextRange.get(); }

    virtual void textFieldDidBeginEditing(Element*);
    virtual void textFieldDidEndEditing(Element*);
    virtual void textDidChangeInTextField(Element*);
    virtual bool doTextFieldCommandFromEvent(Element*, KeyboardEvent*);
    virtual void textWillBeDeletedInTextField(Element*);
    virtual void textDidChangeInTextArea(Element*);
    
private:
    bool dispatchCPPEvent(const AtomicString& eventType, ClipboardAccessPolicy);

    void freeClipboard();

    RefPtr<Range> m_markedTextRange;
    
// === to be moved into EventHandler

public:
    NSView* nextKeyView(Node* startingPoint, SelectionDirection);
    NSView* nextKeyViewInFrameHierarchy(Node* startingPoint, SelectionDirection);
    static NSView* nextKeyViewForWidget(Widget* startingPoint, SelectionDirection);

    PassRefPtr<KeyboardEvent> currentKeyboardEvent() const;

    virtual bool tabsToLinks(KeyboardEvent*) const;
    virtual bool tabsToAllControls(KeyboardEvent*) const;
    
    static bool currentEventIsMouseDownInWidget(Widget* candidate);

    NSImage* selectionImage(bool forceWhiteText = false) const;
    NSImage* snapshotDragImage(Node* node, NSRect* imageRect, NSRect* elementRect) const;

    bool dispatchDragSrcEvent(const AtomicString &eventType, const PlatformMouseEvent&) const;

    void mouseDown(NSEvent*);
    void mouseDragged(NSEvent*);
    void mouseUp(NSEvent*);
    void mouseMoved(NSEvent*);
    bool keyEvent(NSEvent*);
    bool wheelEvent(NSEvent*);

    void sendFakeEventsAfterWidgetTracking(NSEvent* initiatingEvent);

    virtual bool lastEventIsMouseUp() const;
    void setActivationEventNumber(int num) { _activationEventNumber = num; }

    bool dragHysteresisExceeded(float dragLocationX, float dragLocationY) const;
    bool eventMayStartDrag(NSEvent*) const;
    void dragSourceMovedTo(const PlatformMouseEvent&);
    void dragSourceEndedAt(const PlatformMouseEvent&, NSDragOperation);

    bool sendContextMenuEvent(NSEvent*);

    bool passWidgetMouseDownEventToWidget(const MouseEventWithHitTestResults&);
    bool passWidgetMouseDownEventToWidget(RenderWidget*);
    bool passMouseDownEventToWidget(Widget*);
    bool passSubframeEventToSubframe(MouseEventWithHitTestResults&, Frame* subframePart);
    bool passWheelEventToWidget(Widget*);
    
    WebCoreKeyboardUIMode keyboardUIMode() const;

    virtual bool inputManagerHasMarkedText() const;
    
    virtual bool shouldDragAutoNode(Node*, const IntPoint&) const; // -webkit-user-drag == auto

    virtual bool mouseDownMayStartSelect() const { return _mouseDownMayStartSelect; }
    
    NSEvent* currentEvent() { return _currentEvent; }

private:
    virtual void handleMousePressEvent(const MouseEventWithHitTestResults&);
    virtual void handleMouseMoveEvent(const MouseEventWithHitTestResults&);
    virtual void handleMouseReleaseEvent(const MouseEventWithHitTestResults&);
      
    NSView* mouseDownViewIfStillGood();

    NSView* nextKeyViewInFrame(Node* startingPoint, SelectionDirection, bool* focusCallResultedInViewBeingCreated = 0);
    static NSView* documentViewForNode(Node*);

    NSImage* imageFromRect(NSRect) const;

    NSView* _mouseDownView;
    bool _mouseDownWasInSubframe;
    bool _sendingEventToSubview;
    bool _mouseDownMayStartSelect;
    PlatformMouseEvent m_mouseDown;
    // in our view's coords
    IntPoint m_mouseDownPos;
    float _mouseDownTimestamp;
    int _activationEventNumber;
    
    static NSEvent* _currentEvent;

// === to be moved into the Platform directory

public:
    virtual String mimeTypeForFileName(const String&) const;
    virtual bool isCharacterSmartReplaceExempt(UChar, bool);
    
};

inline FrameMac* Mac(Frame* frame) { return static_cast<FrameMac*>(frame); }
inline const FrameMac* Mac(const Frame* frame) { return static_cast<const FrameMac*>(frame); }

}

#endif
