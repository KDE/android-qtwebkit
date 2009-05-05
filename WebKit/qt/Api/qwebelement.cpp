/*
    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "qwebelement.h"

#include "CSSComputedStyleDeclaration.h"
#include "CSSParser.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "FrameView.h"
#include "HTMLElement.h"
#include "JSGlobalObject.h"
#include "JSHTMLElement.h"
#include "JSObject.h"
#include "NodeList.h"
#include "PropertyNameArray.h"
#include "ScriptFunctionCall.h"
#include "StaticNodeList.h"
#include "qt_runtime.h"
#include "qwebframe.h"
#include "qwebframe_p.h"
#include "runtime_root.h"
#include <wtf/Vector.h>

using namespace WebCore;

class QWebElementPrivate
{
public:
};

/*!
    \class QWebElement
    \since 4.6
    \brief The QWebElement class provides convenience access to DOM elements in a QWebFrame.
    \preliminary

    QWebElement is the main class to provide easy access to the document model.
    The document model is represented by a tree-like structure of DOM elements.
    The root of the tree is called the document element and can be accessed using QWebFrame::documentElement().

    You can reach specific elements by using the findAll() and findFirst() functions, which
    allow the use of CSS selectors to identify elements.

    \snippet webkitsnippets/webelement/main.cpp FindAll

    The first list contains all span elements in the document. The second list contains
    only the span elements that are children of the paragraph that is classified
    as "intro" paragraph.

    Alternatively you can manually traverse the document using firstChild() and nextSibling():

    \snippet webkitsnippets/webelement/main.cpp Traversing with QWebElement

    The underlying content of QWebElement is explicitly shared. Creating a copy of a QWebElement
    does not create a copy of the content, both instances point to the same underlying element.

    The element's attributes can be read using attribute() and changed using setAttribute().

    The content of the child elements can be converted to plain text using toPlainText() and to 
    x(html) using toXml(), and it is possible to replace the content using setPlainText() and setXml().

    Depending on the type of the underlying element there may be extra functionality available, not
    covered through QWebElement's API. For example a HTML form element can be triggered to submit the
    entire form. These list of these functions is available through functions() and they can be called
    directly using callFunction().
*/

/*!
    Constructs a null web element.
*/
QWebElement::QWebElement()
    : d(0), m_element(0)
{
}

/*!
    \internal
*/
QWebElement::QWebElement(WebCore::Element *domElement)
    : d(0), m_element(domElement)
{
    if (m_element)
        m_element->ref();
}

/*!
    Constructs a copy of \a other.
*/
QWebElement::QWebElement(const QWebElement &other)
    : d(0), m_element(other.m_element)
{
    if (m_element)
        m_element->ref();
}

/*!
    Assigns \a other to this element and returns a reference to this element.
*/
QWebElement &QWebElement::operator=(const QWebElement &other)
{
    // ### handle "d" assignment
    if (this != &other) {
        Element *otherElement = other.m_element;
        if (otherElement)
            otherElement->ref();
        if (m_element)
            m_element->deref();
        m_element = otherElement;
    }
    return *this;
}

/*!
    Destroys the element. The underlying DOM element is not destroyed.
*/
QWebElement::~QWebElement()
{
    delete d;
    if (m_element)
        m_element->deref();
}

bool QWebElement::operator==(const QWebElement& o) const
{
    return m_element == o.m_element;
}

bool QWebElement::operator!=(const QWebElement& o) const
{
    return m_element != o.m_element;
}

/*!
    Returns true if the element is a null element; false otherwise.
*/
bool QWebElement::isNull() const
{
    return !m_element;
}

/*!
    Returns a new collection of elements that are children of this element
    and that match the given CSS selector \a selectorQuery.

    The query is specified using \l{http://www.w3.org/TR/REC-CSS2/selector.html#q1}{standard CSS2 selectors}.
*/
QList<QWebElement> QWebElement::findAll(const QString &selectorQuery) const
{
    QList<QWebElement> elements;
    if (!m_element)
        return elements;

    ExceptionCode exception = 0; // ###
    RefPtr<NodeList> nodes = m_element->querySelectorAll(selectorQuery, exception);
    if (!nodes)
        return elements;

    for (int i = 0; i < nodes->length(); ++i) {
        WebCore::Node* n = nodes->item(i);
        elements.append(QWebElement(static_cast<Element*>(n)));
    }

    return elements;
}

/*!
    Returns the first child element that matches the given CSS selector \a selectorQuery.

    This function is equivalent to calling findAll() and taking only the
    first element in the returned collection of elements. However calling
    this function is more efficient.
*/
QWebElement QWebElement::findFirst(const QString &selectorQuery) const
{
    if (!m_element)
        return QWebElement();
    ExceptionCode exception = 0; // ###
    return QWebElement(m_element->querySelector(selectorQuery, exception).get());
}

/*!
    Replaces the existing content of this element with \a text.

    This is equivalent to setting the HTML innerText property.
*/
void QWebElement::setPlainText(const QString &text)
{
    if (!m_element || !m_element->isHTMLElement())
        return;
    ExceptionCode exception = 0;
    static_cast<HTMLElement*>(m_element)->setInnerText(text, exception);
}

/*!
    Returns the text between the start and the end tag of this
    element.

    This is equivalent to reading the HTML innerText property.
*/
QString QWebElement::toPlainText() const
{
    if (!m_element || !m_element->isHTMLElement())
        return QString();
    return static_cast<HTMLElement*>(m_element)->innerText();
}

/*!
    Replaces the contents of this element as well as its own tag with \a markup.
    The string may contain HTML or XML tags, which is parsed and formatted
    before insertion into the document.

    \note This is currently only implemented for (X)HTML elements.
*/
void QWebElement::setOuterXml(const QString &markup)
{
    if (!m_element || !m_element->isHTMLElement())
        return;

    ExceptionCode exception = 0;

    static_cast<HTMLElement*>(m_element)->setOuterHTML(markup, exception);
}

/*!
    Returns this element converted to XML, including the start and the end
    tag of this element and its attributes.

    \note This is currently only implemented for (X)HTML elements.
*/
QString QWebElement::toOuterXml() const
{
    if (!m_element || !m_element->isHTMLElement())
        return QString();

    return static_cast<HTMLElement*>(m_element)->outerHTML();
}

/*!
    Replaces the content of this element with \a markup.
    The string may contain HTML or XML tags, which is parsed and formatted
    before insertion into the document.

    \note This is currently only implemented for (X)HTML elements.
*/
void QWebElement::setInnerXml(const QString &markup)
{
    if (!m_element || !m_element->isHTMLElement())
        return;

    ExceptionCode exception = 0;

    static_cast<HTMLElement*>(m_element)->setInnerHTML(markup, exception);
}

/*!
    Returns the XML between the start and the end tag of this
    element.

    \note This is currently only implemented for (X)HTML elements.
*/
QString QWebElement::toInnerXml() const
{
    if (!m_element || !m_element->isHTMLElement())
        return QString();

    return static_cast<HTMLElement*>(m_element)->innerHTML();
}

/*!
    Adds an attribute called \a name with the value \a value. If an attribute
    with the same name exists, its value is replaced by \a value.
*/
void QWebElement::setAttribute(const QString &name, const QString &value)
{
    if (!m_element)
        return;
    ExceptionCode exception = 0;
    m_element->setAttribute(name, value, exception);
}

/*!
    Adds an attribute called \a name in the namespace described with \a namespaceUri
    with the value \a value. If an attribute with the same name exists, its value is
    replaced by \a value.
*/
void QWebElement::setAttributeNS(const QString &namespaceUri, const QString &name, const QString &value)
{
    if (!m_element)
        return;
    WebCore::ExceptionCode exception = 0;
    m_element->setAttributeNS(namespaceUri, name, value, exception);
}

/*!
    Returns the attributed called \a name. If the attribute does not exist \a defaultValue is
    returned.
*/
QString QWebElement::attribute(const QString &name, const QString &defaultValue) const
{
    if (!m_element)
        return QString();
    if (m_element->hasAttribute(name))
        return m_element->getAttribute(name);
    else
        return defaultValue;
}

/*!
    Returns the attributed called \a name in the namespace described with \a namespaceUri.
    If the attribute does not exist \a defaultValue is returned.
*/
QString QWebElement::attributeNS(const QString &namespaceUri, const QString &name, const QString &defaultValue) const
{
    if (!m_element)
        return QString();
    if (m_element->hasAttributeNS(namespaceUri, name))
        return m_element->getAttributeNS(namespaceUri, name);
    else
        return defaultValue;
}

/*!
    Returns true if this element has an attribute called \a name; otherwise returns false.
*/
bool QWebElement::hasAttribute(const QString &name) const
{
    if (!m_element)
        return false;
    return m_element->hasAttribute(name);
}

/*!
    Returns true if this element has an attribute called \a name in the namespace described
    with \a namespaceUri; otherwise returns false.
*/
bool QWebElement::hasAttributeNS(const QString &namespaceUri, const QString &name) const
{
    if (!m_element)
        return false;
    return m_element->hasAttributeNS(namespaceUri, name);
}

/*!
    Removes the attribute called \a name from this element.
*/
void QWebElement::removeAttribute(const QString &name)
{
    if (!m_element)
        return;
    ExceptionCode exception = 0;
    m_element->removeAttribute(name, exception);
}

/*!
    Removes the attribute called \a name in the namespace described with \a namespaceUri
    from this element.
*/
void QWebElement::removeAttributeNS(const QString &namespaceUri, const QString &name)
{
    if (!m_element)
        return;
    WebCore::ExceptionCode exception = 0;
    m_element->removeAttributeNS(namespaceUri, name, exception);
}

/*!
    Returns true if the element has any attributes defined; otherwise returns false;
*/
bool QWebElement::hasAttributes() const
{
    if (!m_element)
        return false;
    return m_element->hasAttributes();
}

/*!
    Returns the geometry of this element, relative to its containing frame.
*/
QRect QWebElement::geometry() const
{
    if (!m_element)
        return QRect();
    return m_element->getRect();
}

/*!
    Returns the tag name of this element.
*/
QString QWebElement::tagName() const
{
    if (!m_element)
        return QString();
    return m_element->tagName();
}

/*!
    Returns the namespace prefix of the element or an empty string if the element has no namespace prefix.
*/
QString QWebElement::prefix() const
{
    if (!m_element)
        return QString();
    return m_element->prefix();
}

/*!
    If the element uses namespaces, this function returns the local name of the element;
    otherwise it returns an empty string.
*/
QString QWebElement::localName() const
{
    if (!m_element)
        return QString();
    return m_element->localName();
}

/*!
    Returns the namespace URI of this element or an empty string if the element has no namespace URI.
*/
QString QWebElement::namespaceUri() const
{
    if (!m_element)
        return QString();
    return m_element->namespaceURI();
}

/*!
    Returns the parent element of this element or a null element if this element
    is the root document element.
*/
QWebElement QWebElement::parent() const
{
    if (m_element)
        return QWebElement(m_element->parentElement());
    return QWebElement();
}

/*!
    Returns the first child element of this element.

    \sa lastChild() previousSibling() nextSibling()
*/
QWebElement QWebElement::firstChild() const
{
    if (!m_element)
        return QWebElement();
    for (Node* child = m_element->firstChild(); child; child = child->nextSibling()) {
        if (!child->isElementNode())
            continue;
        Element* e = static_cast<Element*>(child);
        return QWebElement(e);
    }
    return QWebElement();
}

/*!
    Returns the last child element of this element.

    \sa firstChild() previousSibling() nextSibling()
*/
QWebElement QWebElement::lastChild() const
{
    if (!m_element)
        return QWebElement();
    for (Node* child = m_element->lastChild(); child; child = child->previousSibling()) {
        if (!child->isElementNode())
            continue;
        Element* e = static_cast<Element*>(child);
        return QWebElement(e);
    }
    return QWebElement();
}

/*!
    Returns the next sibling element of this element.

    \sa firstChild() previousSibling() lastChild()
*/
QWebElement QWebElement::nextSibling() const
{
    if (!m_element)
        return QWebElement();
    for (Node* sib = m_element->nextSibling(); sib; sib = sib->nextSibling()) {
        if (!sib->isElementNode())
            continue;
        Element* e = static_cast<Element*>(sib);
        return QWebElement(e);
    }
    return QWebElement();
}

/*!
    Returns the previous sibling element of this element.

    \sa firstChild() nextSibling() lastChild()
*/
QWebElement QWebElement::previousSibling() const
{
    if (!m_element)
        return QWebElement();
    for (Node* sib = m_element->previousSibling(); sib; sib = sib->previousSibling()) {
        if (!sib->isElementNode())
            continue;
        Element* e = static_cast<Element*>(sib);
        return QWebElement(e);
    }
    return QWebElement();
}

/*!
    Returns the document this element belongs to.
*/
QWebElement QWebElement::document() const
{
    if (!m_element)
        return QWebElement();
    Document* document = m_element->document();
    if (!document)
        return QWebElement();
    return QWebElement(document->documentElement());
}

/*!
    Returns the web frame this elements is a part of. If the element is
    a null element null is returned.
*/
QWebFrame *QWebElement::webFrame() const
{
    if (!m_element)
        return 0;

    Document* document = m_element->document();
    if (!document)
        return 0;

    Frame* frame = document->frame();
    if (!frame)
        return 0;
    return QWebFramePrivate::kit(frame);
}

static bool setupScriptContext(WebCore::Element* element, JSC::JSValue& thisValue, ScriptState*& state, ScriptController*& scriptController)
{
    if (!element)
        return false;

    Document* document = element->document();
    if (!document)
        return false;

    Frame* frame = document->frame();
    if (!frame)
        return false;

    scriptController = frame->script();
    if (!scriptController)
        return false;

    state = scriptController->globalObject()->globalExec();
    if (!state)
        return false;

    thisValue = toJS(state, element);
    if (!thisValue)
        return false;

    return true;
}


static bool setupScriptObject(WebCore::Element* element, ScriptObject& object, ScriptState*& state, ScriptController*& scriptController)
{
    if (!element)
        return false;

    Document* document = element->document();
    if (!document)
        return false;

    Frame* frame = document->frame();
    if (!frame)
        return false;

    scriptController = frame->script();

    state = scriptController->globalObject()->globalExec();

    JSC::JSValue thisValue = toJS(state, element);
    if (!thisValue)
        return false;

    JSC::JSObject* thisObject = thisValue.toObject(state);
    if (!thisObject)
        return false;

    object = ScriptObject(thisObject);
    return true;
}

/*!
    Executes the \a scriptSource with this element as the `this' object.

    \sa callFunction()
*/
QVariant QWebElement::evaluateScript(const QString& scriptSource)
{
    if (scriptSource.isEmpty())
        return QVariant();

    ScriptState* state = 0;
    JSC::JSValue thisValue;
    ScriptController* scriptController = 0;

    if (!setupScriptContext(m_element, thisValue, state, scriptController))
        return QVariant();

    JSC::ScopeChain& scopeChain = state->dynamicGlobalObject()->globalScopeChain();
    JSC::UString script((const ushort*)scriptSource.data(), scriptSource.length());
    JSC::Completion completion = JSC::evaluate(state, scopeChain, JSC::makeSource(script), thisValue);
    if ((completion.complType() != JSC::ReturnValue) && (completion.complType() != JSC::Normal))
        return QVariant();

    JSC::JSValue result = completion.value();
    if (!result)
        return QVariant();

    int distance = 0;
    return JSC::Bindings::convertValueToQVariant(state, result, QMetaType::Void, &distance);
}

/*!
    Calls the function with the given \a name and \a arguments.

    The underlying DOM element that QWebElement wraps may have dedicated functions depending
    on its type. For example a form element can have the "submit" function, that would submit
    the form to the destination specified in the HTML.

    \sa functions()
*/
QVariant QWebElement::callFunction(const QString &name, const QVariantList &arguments)
{
    ScriptState* state = 0;
    ScriptObject thisObject;
    ScriptController* scriptController = 0;

    if (!setupScriptObject(m_element, thisObject, state, scriptController))
        return QVariant();

    ScriptFunctionCall functionCall(state, thisObject, name);

    for (QVariantList::ConstIterator it = arguments.constBegin(), end = arguments.constEnd();
         it != end; ++it)
        functionCall.appendArgument(JSC::Bindings::convertQVariantToValue(state, scriptController->bindingRootObject(), *it));

    bool hadException = false;
    ScriptValue result = functionCall.call(hadException);
    if (hadException)
        return QVariant();

    int distance = 0;
    return JSC::Bindings::convertValueToQVariant(state, result.jsValue(), QMetaType::Void, &distance);
}

/*!
    Returns a list of function names this element supports.

    The function names returned are the same functions that are callable from the DOM
    element's JavaScript binding.

    \sa callFunction()
*/
QStringList QWebElement::functions() const
{
    ScriptState* state = 0;
    ScriptObject thisObject;
    ScriptController* scriptController = 0;

    if (!setupScriptObject(m_element, thisObject, state, scriptController))
        return QStringList();

    JSC::JSObject* object = thisObject.jsObject();
    if (!object)
        return QStringList();

    QStringList names;

    // Enumerate the contents of the object
    JSC::PropertyNameArray properties(state);
    object->getPropertyNames(state, properties);
    for (JSC::PropertyNameArray::const_iterator it = properties.begin();
         it != properties.end(); ++it) {

        JSC::JSValue property = object->get(state, *it);
        if (!property)
            continue;

        JSC::JSObject* function = property.toObject(state);
        if (!function)
            continue;

        JSC::CallData callData;
        JSC::CallType callType = function->getCallData(callData);
        if (callType == JSC::CallTypeNone)
            continue;

        JSC::UString ustring = (*it).ustring();
        names << QString::fromUtf16((const ushort*)ustring.rep()->data(),ustring.size());
    }

    if (state->hadException())
        state->clearException();

    return names;
}

/*!
    Returns the value of the element's \a name property.

    If no such property exists, the returned variant is invalid.

    The return property has the same value as the corresponding property
    in the element's JavaScript binding with the same name.

    Information about all available properties is provided through scriptProperties().

    \sa setScriptableProperty(), scriptableProperties()
*/
QVariant QWebElement::scriptableProperty(const QString &name) const
{
    ScriptState* state = 0;
    ScriptObject thisObject;
    ScriptController *scriptController = 0;

    if (!setupScriptObject(m_element, thisObject, state, scriptController))
        return QVariant();

    String wcName(name);
    JSC::JSValue property = thisObject.jsObject()->get(state, JSC::Identifier(state, wcName));

    // ###
    if (state->hadException())
        state->clearException();

    int distance = 0;
    return JSC::Bindings::convertValueToQVariant(state, property, QMetaType::Void, &distance);
}

/*!
    Sets the value of the element's \a name property to \a value.

    Information about all available properties is provided through scriptProperties().

    Setting the property will affect the corresponding property
    in the element's JavaScript binding with the same name.

    \sa scriptableProperty(), scriptableProperties()
*/
void QWebElement::setScriptableProperty(const QString &name, const QVariant &value)
{
    ScriptState* state = 0;
    ScriptObject thisObject;
    ScriptController* scriptController = 0;

    if (!setupScriptObject(m_element, thisObject, state, scriptController))
        return;

    JSC::JSValue jsValue = JSC::Bindings::convertQVariantToValue(state, scriptController->bindingRootObject(), value);
    if (!jsValue)
        return;

    String wcName(name);
    JSC::PutPropertySlot slot;
    thisObject.jsObject()->put(state, JSC::Identifier(state, wcName), jsValue, slot);
    if (state->hadException())
        state->clearException();
}

/*!
    Returns a list of property names this element supports.

    The function names returned are the same properties that are accessible from the DOM
    element's JavaScript binding.

    \sa setScriptableProperty(), scriptableProperty()
*/
QStringList QWebElement::scriptableProperties() const
{
    if (!m_element)
        return QStringList();

    Document* document = m_element->document();
    if (!document)
        return QStringList();

    Frame* frame = document->frame();
    if (!frame)
        return QStringList();

    ScriptController* script = frame->script();
    JSC::ExecState* exec = script->globalObject()->globalExec();

    JSC::JSValue thisValue = toJS(exec, m_element);
    if (!thisValue)
        return QStringList();

    JSC::JSObject* object = thisValue.toObject(exec);
    if (!object)
        return QStringList();

    QStringList names;

    // Enumerate the contents of the object
    JSC::PropertyNameArray properties(exec);
    object->getPropertyNames(exec, properties);
    for (JSC::PropertyNameArray::const_iterator it = properties.begin();
         it != properties.end(); ++it) {

        JSC::JSValue property = object->get(exec, *it);
        if (!property)
            continue;

        JSC::JSObject* function = property.toObject(exec);
        if (!function)
            continue;

        JSC::CallData callData;
        JSC::CallType callType = function->getCallData(callData);
        if (callType != JSC::CallTypeNone)
            continue;

        JSC::UString ustring = (*it).ustring();
        names << QString::fromUtf16((const ushort*)ustring.rep()->data(),ustring.size());
    }

    if (exec->hadException())
        exec->clearException();

    return names;
}

/*!
    Returns the value of the style named \a name or an empty string if the style has no such name.
*/
QString QWebElement::styleProperty(const QString &name) const
{
    if (!m_element || !m_element->isStyledElement())
        return QString();

    int propID = cssPropertyID(name);
    CSSStyleDeclaration* style = static_cast<StyledElement*>(m_element)->style();
    if (!propID || !style)
        return QString();

    return style->getPropertyValue(propID);
}

/*!
    Sets the value of the style named \a name to \a value.
*/
void QWebElement::setStyleProperty(const QString &name, const QString &value)
{
    if (!m_element || !m_element->isStyledElement())
        return;

    int propID = cssPropertyID(name);
    CSSStyleDeclaration* style = static_cast<StyledElement*>(m_element)->style();
    if (!propID || !style)
        return;

    ExceptionCode exception = 0;
    style->setProperty(name, value, exception);
}

/*!
    Returns the computed value for style named \a name or an empty string if the style has no such name.
*/
QString QWebElement::computedStyleProperty(const QString &name) const
{
    if (!m_element || !m_element->isStyledElement())
        return QString();

    int propID = cssPropertyID(name);

    RefPtr<CSSComputedStyleDeclaration> style = computedStyle(m_element);
    if (!propID || !style)
        return QString();

    return style->getPropertyValue(propID);
}

/*!
    Returns the list of classes of this element.
*/
QStringList QWebElement::classes() const
{
    if (!hasAttribute("class"))
        return QStringList();

    QStringList classes =  attribute("class").simplified().split(' ', QString::SkipEmptyParts);
#if QT_VERSION >= 0x040500
    classes.removeDuplicates();
#else
    int n = classes.size();
    int j = 0;
    QSet<QString> seen;
    seen.reserve(n);
    for (int i = 0; i < n; ++i) {
        const QString& s = classes.at(i);
        if (seen.contains(s))
            continue;
        seen.insert(s);
        if (j != i)
            classes[j] = s;
        ++j;
    }
    if (n != j)
        classes.erase(classes.begin() + j, classes.end());
#endif
    return classes;
}

/*!
    Returns true if this element has a class called \a name; otherwise returns false.
*/
bool QWebElement::hasClass(const QString &name) const
{
    QStringList list = classes();
    return list.contains(name);
}

/*!
    Adds the specified class \a name to the element.
*/
void QWebElement::addClass(const QString &name)
{
    QStringList list = classes();
    if (!list.contains(name)) {
        list.append(name);
        QString value = list.join(" ");
        setAttribute("class", value);
    }
}

/*!
    Removes the specified class \a name from the element.
*/
void QWebElement::removeClass(const QString &name)
{
    QStringList list = classes();
    if (list.contains(name)) {
        list.removeAll(name);
        QString value = list.join(" ");
        setAttribute("class", value);
    }
}

/*!
    Adds the specified class \a name if it is not present,
    removes it if it is already present.
*/
void QWebElement::toggleClass(const QString &name)
{
    QStringList list = classes();
    if (list.contains(name))
        list.removeAll(name);
    else
        list.append(name);

    QString value = list.join(" ");
    setAttribute("class", value);
}

/*!
    Appends \a element as the element's last child.

    If \a element is the child of another element, it is re-parented
    to this element. If \a element is a child of this element, then
    its position in the list of children is changed.

    Calling this function on a null element does nothing.

    \sa prependInside(), prependOutside(), appendOutside()
*/
void QWebElement::appendInside(const QWebElement &element)
{
    if (!m_element || element.isNull())
        return;

    ExceptionCode exception = 0;
    m_element->appendChild(element.m_element, exception);
}

/*!
    Appends the result of parsing \a markup as the element's last child.

    Calling this function on a null element does nothing.

    \sa prependInside(), prependOutside(), appendOutside()
*/
void QWebElement::appendInside(const QString &markup)
{
    if (!m_element)
        return;

    if (!m_element->isHTMLElement())
        return;

    HTMLElement* htmlElement = static_cast<HTMLElement*>(m_element);
    RefPtr<DocumentFragment> fragment = htmlElement->createContextualFragment(markup);

    ExceptionCode exception = 0;
    m_element->appendChild(fragment, exception);
}

/*!
    Prepends \a element as the element's first child.

    If \a element is the child of another element, it is re-parented
    to this element. If \a element is a child of this element, then
    its position in the list of children is changed.

    Calling this function on a null element does nothing.

    \sa appendInside(), prependOutside(), appendOutside()
*/
void QWebElement::prependInside(const QWebElement &element)
{
    if (!m_element || element.isNull())
        return;

    ExceptionCode exception = 0;
    m_element->insertBefore(element.m_element, m_element->firstChild(), exception);
}

/*!
    Prepends the result of parsing \a markup as the element's first child.

    Calling this function on a null element does nothing.

    \sa appendInside(), prependOutside(), appendOutside()
*/
void QWebElement::prependInside(const QString &markup)
{
    if (!m_element)
        return;

    if (!m_element->isHTMLElement())
        return;

    HTMLElement* htmlElement = static_cast<HTMLElement*>(m_element);
    RefPtr<DocumentFragment> fragment = htmlElement->createContextualFragment(markup);

    ExceptionCode exception = 0;
    m_element->insertBefore(fragment, m_element->firstChild(), exception);
}


/*!
    Inserts \a element before this element.

    If \a element is the child of another element, it is re-parented
    to the parent of this element.

    Calling this function on a null element does nothing.

    \sa appendInside(), prependInside(), appendOutside()
*/
void QWebElement::prependOutside(const QWebElement &element)
{
    if (!m_element || element.isNull())
        return;

    if (!m_element->parent())
        return;

    ExceptionCode exception = 0;
    m_element->parent()->insertBefore(element.m_element, m_element, exception);
}

/*!
    Inserts the result of parsing \a markup before this element.

    Calling this function on a null element does nothing.

    \sa appendInside(), prependInside(), appendOutside()
*/
void QWebElement::prependOutside(const QString &markup)
{
    if (!m_element)
        return;

    if (!m_element->parent())
        return;

    if (!m_element->isHTMLElement())
        return;

    HTMLElement* htmlElement = static_cast<HTMLElement*>(m_element);
    RefPtr<DocumentFragment> fragment = htmlElement->createContextualFragment(markup);

    ExceptionCode exception = 0;
    m_element->parent()->insertBefore(fragment, m_element, exception);
}

/*!
    Inserts \a element after this element.

    If \a element is the child of another element, it is re-parented
    to the parent of this element.

    Calling this function on a null element does nothing.

    \sa appendInside(), prependInside(), prependOutside()
*/
void QWebElement::appendOutside(const QWebElement &element)
{
    if (!m_element || element.isNull())
        return;

    if (!m_element->parent())
        return;

    if (!m_element->nextSibling())
        return;

    ExceptionCode exception = 0;
    m_element->parent()->insertBefore(element.m_element, m_element->nextSibling(), exception);
}

/*!
    Inserts the result of parsing \a markup after this element.

    Calling this function on a null element does nothing.

    \sa appendInside(), prependInside(), prependOutside()
*/
void QWebElement::appendOutside(const QString &markup)
{
    if (!m_element)
        return;

    if (!m_element->parent())
        return;

    if (!m_element->isHTMLElement())
        return;

    HTMLElement* htmlElement = static_cast<HTMLElement*>(m_element);
    RefPtr<DocumentFragment> fragment = htmlElement->createContextualFragment(markup);

    ExceptionCode exception = 0;
    m_element->parent()->insertBefore(fragment, m_element->nextSibling(), exception);
}

/*!
    Returns a clone of this element.

    The clone may be inserted at any point in the document.

    \sa appendInside(), prependInside(), prependOutside(), appendOutside()
*/
QWebElement QWebElement::clone() const
{
    if (!m_element)
        return QWebElement();

    return QWebElement(m_element->cloneElementWithChildren().get());
}

/*!
    Removes this element from the document and returns a reference
    to this.

    The element is still valid after removal, and can be inserted into
    other parts of the document.

    \sa removeChildren(), removeFromDocument()
*/
QWebElement &QWebElement::takeFromDocument()
{
    if (!m_element)
        return *this;

    ExceptionCode exception = 0;
    m_element->remove(exception);

    return *this;
}

/*!
    Removes this element from the document and makes this
    a null element.

    \sa removeChildren(), takeFromDocument()
*/
void QWebElement::removeFromDocument()
{
    if (!m_element)
        return;

    ExceptionCode exception = 0;
    m_element->remove(exception);
    m_element->deref();
    m_element = 0;
}

/*!
    Removes all children from this element.

    \sa removeFromDocument(), takeFromDocument()
*/
void QWebElement::removeChildren()
{
    if (!m_element)
        return;

    m_element->removeAllChildren();
}

/*!
    Makes the children of this element children of \a element,
    and then makes \a element the only child of this element.
*/
void QWebElement::encloseContentsWith(const QWebElement &element)
{
    if (!m_element || element.isNull())
        return;

    QWebElement other = element;
    for (QWebElement child = firstChild(); !child.isNull();) {
        QWebElement next = child.nextSibling();
        other.appendInside(child);
        child = next;
    }

    appendInside(other);
}

/*!
    Parses the \a markup and makes the children of this element
    children of the parsed markup. Afterwards the element parsed
    from the markup becomes the only child of this element.
*/
void QWebElement::encloseContentsWith(const QString &markup)
{
    if (!m_element)
        return;

    if (!m_element->parent())
        return;

    if (!m_element->isHTMLElement())
        return;

    HTMLElement* htmlElement = static_cast<HTMLElement*>(m_element);
    RefPtr<DocumentFragment> fragment = htmlElement->createContextualFragment(markup);

    if (!fragment || !fragment->firstChild())
        return;

    ExceptionCode exception = 0;

    // reparent children
    for (RefPtr<Node> child = m_element->firstChild(); child;) {
        RefPtr<Node> next = child->nextSibling();
        fragment->firstChild()->appendChild(child, exception);
        child = next;
    }

    if (m_element->hasChildNodes())
        m_element->insertBefore(fragment, m_element->firstChild(), exception);
    else
        m_element->appendChild(fragment, exception);
}

/*!
    Enclose this element in \a element as the last child.

    \sa replace()
*/
void QWebElement::encloseWith(const QWebElement &element)
{
    if (!m_element || element.isNull())
        return;

    appendOutside(element);
    QWebElement other = element;
    other.appendInside(*this);
}

/*!
    Enclose this element in the result of parsing \a html,
    as the last child.

    \sa replace()
*/
void QWebElement::encloseWith(const QString &html)
{
    if (!m_element)
        return;

    if (!m_element->parent())
        return;

    if (!m_element->isHTMLElement())
        return;

    HTMLElement* htmlElement = static_cast<HTMLElement*>(m_element);
    RefPtr<DocumentFragment> fragment = htmlElement->createContextualFragment(html);

    if (!fragment || !fragment->firstChild())
        return;

    // Keep reference to these two nodes before pulling out this element and
    // wrapping it in the fragment. The reason for doing it in this order is
    // that once the fragment has been added to the document it is empty, so
    // we no longer have access to the nodes it contained.
    Node* parentNode = m_element->parent();
    Node* siblingNode = m_element->nextSibling();

    // Elements with forbidden tag status can never have children
    HTMLElement* element = static_cast<HTMLElement*>(fragment->firstChild());
    if (element->endTagRequirement() == TagStatusForbidden)
        return;

    ExceptionCode exception = 0;
    fragment->firstChild()->appendChild(m_element, exception);
    parentNode->insertBefore(element, siblingNode, exception);
}

/*!
    Replaces this element with \a element.

    It is not possible to replace the <html>, <head>, or <body>
    elements using this method.

    \sa encloseWith()
*/
void QWebElement::replace(const QWebElement &element)
{
    if (!m_element || element.isNull())
        return;

    appendOutside(element);
    takeFromDocument();
}

/*!
    Replaces this element with the result of parsing \a html.

    It is not possible to replace the <html>, <head>, or <body>
    elements using this method.

    \sa encloseWith()
*/
void QWebElement::replace(const QString &html)
{
    if (!m_element)
        return;

    appendOutside(html);
    takeFromDocument();
}

/*!
    \fn inline bool QWebElement::operator==(const QWebElement& o) const;

    Returns true if this element points to the same underlying DOM object than \a o; otherwise returns false.
*/

/*!
    \fn inline bool QWebElement::operator!=(const QWebElement& o) const;

    Returns true if this element points to a different underlying DOM object than \a o; otherwise returns false.
*/
