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

#import "DumpRenderTree.h"
#import "AccessibilityUIElement.h"

#import <Foundation/Foundation.h>
#import <JavaScriptCore/JSStringRef.h>
#import <JavaScriptCore/JSStringRefCF.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebHTMLView.h>
#import <WebKit/WebTypesInternal.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

AccessibilityUIElement::AccessibilityUIElement(PlatformUIElement element)
    : m_element(element)
{
    [m_element retain];
}

AccessibilityUIElement::AccessibilityUIElement()
{
    [m_element release];
}

@interface NSString (JSStringRefAdditions)
- (JSStringRef)createJSStringRef;
@end

@implementation NSString (JSStringRefAdditions)

- (JSStringRef)createJSStringRef
{
    return JSStringCreateWithCFString((CFStringRef)self);
}

@end

static NSString* descriptionOfValue(id valueObject, id focusedAccessibilityObject)
{
    if (!valueObject)
        return NULL;

    if ([valueObject isKindOfClass:[NSArray class]])
        return [NSString stringWithFormat:@"<array of size %d>", [(NSArray*)valueObject count]];

    if ([valueObject isKindOfClass:[NSNumber class]])
        return [(NSNumber*)valueObject stringValue];

    if ([valueObject isKindOfClass:[NSValue class]]) {
        NSString* type = [NSString stringWithCString:[valueObject objCType] encoding:NSASCIIStringEncoding];
        NSValue* value = (NSValue*)valueObject;
        if ([type rangeOfString:@"NSRect"].length > 0)
            return [NSString stringWithFormat:@"NSRect: %@", NSStringFromRect([value rectValue])];
        if ([type rangeOfString:@"NSPoint"].length > 0)
            return [NSString stringWithFormat:@"NSPoint: %@", NSStringFromPoint([value pointValue])];
        if ([type rangeOfString:@"NSSize"].length > 0)
            return [NSString stringWithFormat:@"NSSize: %@", NSStringFromSize([value sizeValue])];
        if ([type rangeOfString:@"NSRange"].length > 0)
            return [NSString stringWithFormat:@"NSRange: %@", NSStringFromRange([value rangeValue])];
    }

    // Strip absolute URL paths
    NSString* description = [valueObject description];
    NSRange range = [description rangeOfString:@"LayoutTests"];
    if (range.length)
        return [description substringFromIndex:range.location];

    // Strip pointer locations
    if ([description rangeOfString:@"0x"].length) {
        NSString* role = [focusedAccessibilityObject accessibilityAttributeValue:@"AXRole"];
        NSString* title = [focusedAccessibilityObject accessibilityAttributeValue:@"AXTitle"];
        if ([title length])
            return [NSString stringWithFormat:@"<%@: '%@'>", role, title];
        return [NSString stringWithFormat:@"<%@>", role];
    }
    
    return [valueObject description];
}

static NSString* attributesOfElement(id accessibilityObject)
{
    NSArray* supportedAttributes = [accessibilityObject accessibilityAttributeNames];

    NSMutableString* attributesString = [NSMutableString string];
    for (NSUInteger i = 0; i < [supportedAttributes count]; ++i) {
        NSString* attribute = [supportedAttributes objectAtIndex:i];
        
        // Right now, position provides useless and screen-specific information, so we do not
        // want to include it for the sake of universally passing tests.
        if ([attribute isEqualToString:@"AXPosition"])
            continue;
        
        id valueObject = [accessibilityObject accessibilityAttributeValue:attribute];
        NSString* value = descriptionOfValue(valueObject, accessibilityObject);
        [attributesString appendFormat:@"%@: %@\n", attribute, value];
    }
    
    return attributesString;
}

static JSStringRef concatenateAttributeAndValue(NSString* attribute, NSString* value)
{
    Vector<UniChar> buffer([attribute length]);
    [attribute getCharacters:buffer.data()];
    buffer.append(':');
    buffer.append(' ');

    Vector<UniChar> valueBuffer([value length]);
    [value getCharacters:valueBuffer.data()];
    buffer.append(valueBuffer);

    return JSStringCreateWithCharacters(buffer.data(), buffer.size());
}

void AccessibilityUIElement::getLinkedUIElements(Vector<AccessibilityUIElement*>& elementVector)
{
    NSArray* linkedElements = [m_element accessibilityAttributeValue:NSAccessibilityLinkedUIElementsAttribute];
    NSUInteger count = [linkedElements count];
    for (NSUInteger i = 0; i < count; ++i)
        elementVector.append(new AccessibilityUIElement([linkedElements objectAtIndex:i]));
}

void AccessibilityUIElement::getChildren(Vector<AccessibilityUIElement*>& elementVector)
{
    NSArray* children = [m_element accessibilityAttributeValue:NSAccessibilityChildrenAttribute];
    NSUInteger count = [children count];
    for (NSUInteger i = 0; i < count; ++i)
        elementVector.append(new AccessibilityUIElement([children objectAtIndex:i]));
}

JSStringRef AccessibilityUIElement::attributesOfLinkedUIElements()
{
    Vector<AccessibilityUIElement*> linkedElements;
    getLinkedUIElements(linkedElements);

    NSMutableString* allElementString = [NSMutableString string];
    size_t size = linkedElements.size();
    for (size_t i = 0; i < size; ++i) {
        NSString* attributes = attributesOfElement(linkedElements[i]->platformUIElement());
        [allElementString appendFormat:@"%@\n------------\n", attributes];
    }

    return [allElementString createJSStringRef];
}

JSStringRef AccessibilityUIElement::attributesOfChildren()
{
    Vector<AccessibilityUIElement*> children;
    getChildren(children);

    NSMutableString* allElementString = [NSMutableString string];
    size_t size = children.size();
    for (size_t i = 0; i < size; ++i) {
        NSString* attributes = attributesOfElement(children[i]->platformUIElement());
        [allElementString appendFormat:@"%@\n------------\n", attributes];
    }

    return [allElementString createJSStringRef];
}

JSStringRef AccessibilityUIElement::allAttributes()
{
    NSString* attributes = attributesOfElement(m_element);
    return [attributes createJSStringRef];
}

JSStringRef AccessibilityUIElement::role()
{
    NSString* role = descriptionOfValue([m_element accessibilityAttributeValue:@"AXRole"], m_element);
    return concatenateAttributeAndValue(@"AXRole", role);
}

JSStringRef AccessibilityUIElement::title()
{
    NSString* title = descriptionOfValue([m_element accessibilityAttributeValue:@"AXTitle"], m_element);
    return concatenateAttributeAndValue(@"AXTitle", title);
}

JSStringRef AccessibilityUIElement::description()
{
    id description = descriptionOfValue([m_element accessibilityAttributeValue:@"AXDescription"], m_element);
    return concatenateAttributeAndValue(@"AXDescription", description);
}

double AccessibilityUIElement::width()
{
    NSValue* sizeValue = [m_element accessibilityAttributeValue:@"AXSize"];
    return static_cast<double>([sizeValue sizeValue].width);
}

double AccessibilityUIElement::height()
{
    NSValue* sizeValue = [m_element accessibilityAttributeValue:@"AXSize"];
    return static_cast<double>([sizeValue sizeValue].height);
}

double AccessibilityUIElement::intValue()
{
    id value = [m_element accessibilityAttributeValue:@"AXValue"];
    if ([value isKindOfClass:[NSNumber class]])
        return [(NSNumber*)value doubleValue]; 
    return 0.0f;
}

double AccessibilityUIElement::minValue()
{
    id value = [m_element accessibilityAttributeValue:@"AXMinValue"];
    if ([value isKindOfClass:[NSNumber class]])
        return [(NSNumber*)value doubleValue]; 
    return 0.0f;
}

double AccessibilityUIElement::maxValue()
{
    id value = [m_element accessibilityAttributeValue:@"AXMaxValue"];
    if ([value isKindOfClass:[NSNumber class]])
        return [(NSNumber*)value doubleValue]; 
    return 0.0;
}
