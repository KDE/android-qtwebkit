/*
 * Copyright (C) 2011 Google, Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
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

#include "config.h"
#include "ContentSecurityPolicy.h"

#include "DOMWindow.h"
#include "Document.h"
#include "FormData.h"
#include "FormDataList.h"
#include "Frame.h"
#include "NotImplemented.h"
#include "PingLoader.h"
#include "SecurityOrigin.h"
#include "TextEncoding.h"
#include <wtf/text/StringConcatenate.h>

namespace WebCore {

// Normally WebKit uses "static" for internal linkage, but using "static" for
// these functions causes a compile error because these functions are used as
// template parameters.
namespace {

bool isDirectiveNameCharacter(UChar c)
{
    return isASCIIAlphanumeric(c) || c == '-';
}

bool isDirectiveValueCharacter(UChar c)
{
    return isASCIISpace(c) || (c >= 0x21 && c <= 0x7e); // Whitespace + VCHAR
}

bool isSourceCharacter(UChar c)
{
    return !isASCIISpace(c);
}

bool isHostCharacter(UChar c)
{
    return isASCIIAlphanumeric(c) || c == '-';
}

bool isOptionValueCharacter(UChar c)
{
    return isASCIIAlphanumeric(c) || c == '-';
}

bool isSchemeContinuationCharacter(UChar c)
{
    return isASCIIAlphanumeric(c) || c == '+' || c == '-' || c == '.';
}

bool isNotASCIISpace(UChar c)
{
    return !isASCIISpace(c);
}

} // namespace

static bool skipExactly(const UChar*& position, const UChar* end, UChar delimiter)
{
    if (position < end && *position == delimiter) {
        ++position;
        return true;
    }
    return false;
}

template<bool characterPredicate(UChar)>
static bool skipExactly(const UChar*& position, const UChar* end)
{
    if (position < end && characterPredicate(*position)) {
        ++position;
        return true;
    }
    return false;
}

static void skipUtil(const UChar*& position, const UChar* end, UChar delimiter)
{
    while (position < end && *position != delimiter)
        ++position;
}

template<bool characterPredicate(UChar)>
static void skipWhile(const UChar*& position, const UChar* end)
{
    while (position < end && characterPredicate(*position))
        ++position;
}

class CSPSource {
public:
    CSPSource(const String& scheme, const String& host, int port, bool hostHasWildcard, bool portHasWildcard)
        : m_scheme(scheme)
        , m_host(host)
        , m_port(port)
        , m_hostHasWildcard(hostHasWildcard)
        , m_portHasWildcard(portHasWildcard)
    {
    }

    bool matches(const KURL& url) const
    {
        if (!schemeMatches(url))
            return false;
        if (isSchemeOnly())
            return true;
        return hostMatches(url) && portMatches(url);
    }

private:
    bool schemeMatches(const KURL& url) const
    {
        return equalIgnoringCase(url.protocol(), m_scheme);
    }

    bool hostMatches(const KURL& url) const
    {
        if (m_hostHasWildcard)
            notImplemented();

        return equalIgnoringCase(url.host(), m_host);
    }

    bool portMatches(const KURL& url) const
    {
        if (m_portHasWildcard)
            return true;

        // FIXME: Handle explicit default ports correctly.
        return url.port() == m_port;
    }

    bool isSchemeOnly() const { return m_host.isEmpty(); }

    String m_scheme;
    String m_host;
    int m_port;

    bool m_hostHasWildcard;
    bool m_portHasWildcard;
};

class CSPSourceList {
public:
    explicit CSPSourceList(SecurityOrigin*);

    void parse(const String&);
    bool matches(const KURL&);

private:
    void parse(const UChar* begin, const UChar* end);

    bool parseSource(const UChar* begin, const UChar* end, String& scheme, String& host, int& port, bool& hostHasWildcard, bool& portHasWildcard);
    bool parseScheme(const UChar* begin, const UChar* end, String& scheme);
    bool parseHost(const UChar* begin, const UChar* end, String& host, bool& hostHasWildcard);
    bool parsePort(const UChar* begin, const UChar* end, int& port, bool& portHasWildcard);

    void addSourceSelf();

    SecurityOrigin* m_origin;
    Vector<CSPSource> m_list;
};

CSPSourceList::CSPSourceList(SecurityOrigin* origin)
    : m_origin(origin)
{
}

void CSPSourceList::parse(const String& value)
{
    parse(value.characters(), value.characters() + value.length());
}

bool CSPSourceList::matches(const KURL& url)
{
    for (size_t i = 0; i < m_list.size(); ++i) {
        if (m_list[i].matches(url))
            return true;
    }
    return false;
}

// source-list       = *WSP [ source *( 1*WSP source ) *WSP ]
//                   / *WSP "'none'" *WSP
//
void CSPSourceList::parse(const UChar* begin, const UChar* end)
{
    const UChar* position = begin;

    bool isFirstSourceInList = true;
    while (position < end) {
        skipWhile<isASCIISpace>(position, end);
        const UChar* beginSource = position;
        skipWhile<isSourceCharacter>(position, end);

        if (isFirstSourceInList && equalIgnoringCase("'none'", beginSource, position - beginSource))
            return; // We represent 'none' as an empty m_list.
        isFirstSourceInList = false;

        String scheme, host;
        int port = 0;
        bool hostHasWildcard = false;
        bool portHasWildcard = false;

        if (parseSource(beginSource, position, scheme, host, port, hostHasWildcard, portHasWildcard)) {
            if (scheme.isEmpty())
                scheme = m_origin->protocol();
            m_list.append(CSPSource(scheme, host, port, hostHasWildcard, portHasWildcard));
        }

        ASSERT(position == end || isASCIISpace(*position));
     }
}

// source            = scheme ":"
//                   / ( [ scheme "://" ] host [ port ] )
//                   / "'self'"
//
bool CSPSourceList::parseSource(const UChar* begin, const UChar* end,
                                String& scheme, String& host, int& port,
                                bool& hostHasWildcard, bool& portHasWildcard)
{
    if (begin == end)
        return false;

    if (equalIgnoringCase("'self'", begin, end - begin)) {
        addSourceSelf();
        return false;
    }

    const UChar* position = begin;

    const UChar* beginHost = begin;
    skipUtil(position, end, ':');

    if (position == end) {
        // This must be a host-only source.
        if (!parseHost(beginHost, position, host, hostHasWildcard))
            return false;
        return true;
    }

    if (end - position == 1) {
        ASSERT(*position == ':');
        // This must be a scheme-only source.
        if (!parseScheme(begin, position, scheme))
            return false;
        return true;
    }

    ASSERT(end - position >= 2);
    if (position[1] == '/') {
        if (!parseScheme(begin, position, scheme)
            || !skipExactly(position, end, ':')
            || !skipExactly(position, end, '/')
            || !skipExactly(position, end, '/'))
            return false;
        beginHost = position;
        skipUtil(position, end, ':');
    }

    if (position == beginHost)
        return false;

    if (!parseHost(beginHost, position, host, hostHasWildcard))
        return false;

    if (position == end) {
        port = 0;
        return true;
    }

    if (!skipExactly(position, end, ':'))
        ASSERT_NOT_REACHED();

    if (!parsePort(position, end, port, portHasWildcard))
        return false;

    return true;
}

//                     ; <scheme> production from RFC 3986
// scheme      = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
//
bool CSPSourceList::parseScheme(const UChar* begin, const UChar* end, String& scheme)
{
    ASSERT(begin <= end);
    ASSERT(scheme.isEmpty());

    if (begin == end)
        return false;

    const UChar* position = begin;

    if (!skipExactly<isASCIIAlpha>(position, end))
        return false;

    skipWhile<isSchemeContinuationCharacter>(position, end);

    if (position != end)
        return false;

    scheme = String(begin, end - begin);
    return true;
}

// host              = [ "*." ] 1*host-char *( "." 1*host-char )
//                   / "*"
// host-char         = ALPHA / DIGIT / "-"
//
bool CSPSourceList::parseHost(const UChar* begin, const UChar* end, String& host, bool& hostHasWildcard)
{
    ASSERT(begin <= end);
    ASSERT(host.isEmpty());
    ASSERT(!hostHasWildcard);

    if (begin == end)
        return false;

    const UChar* position = begin;

    if (skipExactly(position, end, '*')) {
        hostHasWildcard = true;

        if (position == end)
            return true;

        if (!skipExactly(position, end, '.'))
            return false;
    }

    const UChar* hostBegin = position;

    while (position < end) {
        if (!skipExactly<isHostCharacter>(position, end))
            return false;

        skipWhile<isHostCharacter>(position, end);

        if (position < end && !skipExactly(position, end, '.'))
            return false;
    }

    ASSERT(position == end);
    host = String(hostBegin, end - hostBegin);
    return true;
}

// port              = ":" ( 1*DIGIT / "*" )
//
bool CSPSourceList::parsePort(const UChar* begin, const UChar* end, int& port, bool& portHasWildcard)
{
    ASSERT(begin <= end);
    ASSERT(!port);
    ASSERT(!portHasWildcard);

    if (begin == end)
        return false;

    if (end - begin == 1 && *begin == '*') {
        port = 0;
        portHasWildcard = true;
        return true;
    }

    const UChar* position = begin;
    skipWhile<isASCIIDigit>(position, end);

    if (position != end)
        return false;

    bool ok;
    port = charactersToIntStrict(begin, end - begin, &ok);
    return ok;
}

void CSPSourceList::addSourceSelf()
{
    m_list.append(CSPSource(m_origin->protocol(), m_origin->host(), m_origin->port(), false, false));
}

class CSPDirective {
public:
    CSPDirective(const String& name, const String& value, SecurityOrigin* origin)
        : m_sourceList(origin)
        , m_text(makeString(name, " ", value))
    {
        m_sourceList.parse(value);
    }

    bool allows(const KURL& url)
    {
        return m_sourceList.matches(url);
    }

    const String& text() { return m_text; }

private:
    CSPSourceList m_sourceList;
    String m_text;
};

class CSPOptions {
public:
    explicit CSPOptions(const String& value)
        : m_disableXSSProtection(false)
        , m_evalScript(false)
    {
        parse(value);
    }

    bool disableXSSProtection() const { return m_disableXSSProtection; }
    bool evalScript() const { return m_evalScript; }

private:
    void parse(const String&);

    bool m_disableXSSProtection;
    bool m_evalScript;
};

// options           = "options" *( 1*WSP option-value ) *WSP
// option-value      = 1*( ALPHA / DIGIT / "-" )
//
void CSPOptions::parse(const String& value)
{
    DEFINE_STATIC_LOCAL(String, disableXSSProtection, ("disable-xss-protection"));
    DEFINE_STATIC_LOCAL(String, evalScript, ("eval-script"));

    const UChar* position = value.characters();
    const UChar* end = position + value.length();

    while (position < end) {
        skipWhile<isASCIISpace>(position, end);

        const UChar* optionsValueBegin = position;

        if (!skipExactly<isOptionValueCharacter>(position, end))
            return;

        skipWhile<isOptionValueCharacter>(position, end);

        String optionsValue(optionsValueBegin, position - optionsValueBegin);

        if (equalIgnoringCase(optionsValue, disableXSSProtection))
            m_disableXSSProtection = true;
        else if (equalIgnoringCase(optionsValue, evalScript))
            m_evalScript = true;
    }
}

ContentSecurityPolicy::ContentSecurityPolicy(Document* document)
    : m_havePolicy(false)
    , m_document(document)
{
}

ContentSecurityPolicy::~ContentSecurityPolicy()
{
}

void ContentSecurityPolicy::didReceiveHeader(const String& header)
{
    if (m_havePolicy)
        return; // The first policy wins.

    parse(header);
    m_havePolicy = true;
}

void ContentSecurityPolicy::reportViolation(const String& directiveText, const String& consoleMessage) const
{
    Frame* frame = m_document->frame();
    if (!frame)
        return;

    frame->domWindow()->console()->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, consoleMessage, 1, String());

    if (m_reportURLs.isEmpty())
        return;

    // We need to be careful here when deciding what information to send to the
    // report-uri. Currently, we send only the current document's URL and the
    // directive that was violated. The document's URL is safe to send because
    // it's the document itself that's requesting that it be sent. You could
    // make an argument that we shouldn't send HTTPS document URLs to HTTP
    // report-uris (for the same reasons that we supress the Referer in that
    // case), but the Referer is sent implicitly whereas this request is only
    // sent explicitly. As for which directive was violated, that's pretty
    // harmless information.

    FormDataList reportList(UTF8Encoding());
    reportList.appendData("document-url", m_document->url());
    if (!directiveText.isEmpty())
        reportList.appendData("violated-directive", directiveText);

    RefPtr<FormData> report = FormData::create(reportList, UTF8Encoding());

    for (size_t i = 0; i < m_reportURLs.size(); ++i)
        PingLoader::reportContentSecurityPolicyViolation(frame, m_reportURLs[i], report);
}

bool ContentSecurityPolicy::protectAgainstXSS() const
{
    return m_scriptSrc && (!m_options || !m_options->disableXSSProtection());
}

bool ContentSecurityPolicy::allowJavaScriptURLs() const
{
    if (!protectAgainstXSS())
        return true;

    DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to execute JavaScript URL because of Content-Security-Policy.\n"));
    reportViolation(m_scriptSrc->text(), consoleMessage);
    return false;
}

bool ContentSecurityPolicy::allowInlineEventHandlers() const
{
    if (!protectAgainstXSS())
        return true;

    DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to execute inline event handler because of Content-Security-Policy.\n"));
    reportViolation(m_scriptSrc->text(), consoleMessage);
    return false;
}

bool ContentSecurityPolicy::allowInlineScript() const
{
    if (!protectAgainstXSS())
        return true;

    DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to execute inline script because of Content-Security-Policy.\n"));
    reportViolation(m_scriptSrc->text(), consoleMessage);
    return false;
}

bool ContentSecurityPolicy::allowEval() const
{
    if (!m_scriptSrc || (m_options && m_options->evalScript()))
        return true;

    DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to evaluate script because of Content-Security-Policy.\n"));
    reportViolation(m_scriptSrc->text(), consoleMessage);
    return false;
}

bool ContentSecurityPolicy::allowScriptFromSource(const KURL& url) const
{
    if (!m_scriptSrc || m_scriptSrc->allows(url))
        return true;

    reportViolation(m_scriptSrc->text(), makeString("Refused to load script from '", url.string(), "' because of Content-Security-Policy.\n"));
    return false;
}

bool ContentSecurityPolicy::allowObjectFromSource(const KURL& url) const
{
    if (!m_objectSrc || m_objectSrc->allows(url))
        return true;

    reportViolation(m_objectSrc->text(), makeString("Refused to load object from '", url.string(), "' because of Content-Security-Policy.\n"));
    return false;
}

bool ContentSecurityPolicy::allowChildFrameFromSource(const KURL& url) const
{
    if (!m_frameSrc || m_frameSrc->allows(url))
        return true;

    reportViolation(m_frameSrc->text(), makeString("Refused to load frame from '", url.string(), "' because of Content-Security-Policy.\n"));
    return false;
}

bool ContentSecurityPolicy::allowImageFromSource(const KURL& url) const
{
    if (!m_imgSrc || m_imgSrc->allows(url))
        return true;

    reportViolation(m_imgSrc->text(), makeString("Refused to load image from '", url.string(), "' because of Content-Security-Policy.\n"));
    return false;
}

bool ContentSecurityPolicy::allowStyleFromSource(const KURL& url) const
{
    if (!m_styleSrc || m_styleSrc->allows(url))
        return true;

    reportViolation(m_styleSrc->text(), makeString("Refused to load style from '", url.string(), "' because of Content-Security-Policy.\n"));
    return false;
}

bool ContentSecurityPolicy::allowFontFromSource(const KURL& url) const
{
    if (!m_fontSrc || m_fontSrc->allows(url))
        return true;

    reportViolation(m_fontSrc->text(), makeString("Refused to load font from '", url.string(), "' because of Content-Security-Policy.\n"));
    return false;
}

bool ContentSecurityPolicy::allowMediaFromSource(const KURL& url) const
{
    if (!m_mediaSrc || m_mediaSrc->allows(url))
        return true;

    reportViolation(m_mediaSrc->text(), makeString("Refused to load media from '", url.string(), "' because of Content-Security-Policy.\n"));
    return false;
}

// policy            = directive-list
// directive-list    = [ directive *( ";" [ directive ] ) ]
//
void ContentSecurityPolicy::parse(const String& policy)
{
    ASSERT(!m_havePolicy);

    if (policy.isEmpty())
        return;

    const UChar* position = policy.characters();
    const UChar* end = position + policy.length();

    while (position < end) {
        const UChar* directiveBegin = position;
        skipUtil(position, end, ';');

        String name, value;
        if (parseDirective(directiveBegin, position, name, value)) {
            ASSERT(!name.isEmpty());
            addDirective(name, value);
        }

        ASSERT(position == end || *position == ';');
        skipExactly(position, end, ';');
    }
}

// directive         = *WSP [ directive-name [ WSP directive-value ] ]
// directive-name    = 1*( ALPHA / DIGIT / "-" )
// directive-value   = *( WSP / <VCHAR except ";"> )
//
bool ContentSecurityPolicy::parseDirective(const UChar* begin, const UChar* end, String& name, String& value)
{
    ASSERT(name.isEmpty());
    ASSERT(value.isEmpty());

    const UChar* position = begin;
    skipWhile<isASCIISpace>(position, end);

    const UChar* nameBegin = position;
    skipWhile<isDirectiveNameCharacter>(position, end);

    // The directive-name must be non-empty.
    if (nameBegin == position)
        return false;

    name = String(nameBegin, position - nameBegin);

    if (position == end)
        return true;

    if (!skipExactly<isASCIISpace>(position, end))
        return false;

    skipWhile<isASCIISpace>(position, end);

    const UChar* valueBegin = position;
    skipWhile<isDirectiveValueCharacter>(position, end);

    if (position != end)
        return false;

    // The directive-value may be empty.
    if (valueBegin == position)
        return true;

    value = String(valueBegin, position - valueBegin);
    return true;
}

void ContentSecurityPolicy::parseReportURI(const String& value)
{
    const UChar* position = value.characters();
    const UChar* end = position + value.length();

    while (position < end) {
        skipWhile<isASCIISpace>(position, end);

        const UChar* urlBegin = position;
        skipWhile<isNotASCIISpace>(position, end);

        if (urlBegin < position) {
            String url = String(urlBegin, position - urlBegin);
            m_reportURLs.append(m_document->completeURL(url));
        }
    }
}

PassOwnPtr<CSPDirective> ContentSecurityPolicy::createCSPDirective(const String& name, const String& value)
{
    return adoptPtr(new CSPDirective(name, value, m_document->securityOrigin()));
}

void ContentSecurityPolicy::addDirective(const String& name, const String& value)
{
    DEFINE_STATIC_LOCAL(String, scriptSrc, ("script-src"));
    DEFINE_STATIC_LOCAL(String, objectSrc, ("object-src"));
    DEFINE_STATIC_LOCAL(String, frameSrc, ("frame-src"));
    DEFINE_STATIC_LOCAL(String, imgSrc, ("img-src"));
    DEFINE_STATIC_LOCAL(String, styleSrc, ("style-src"));
    DEFINE_STATIC_LOCAL(String, fontSrc, ("font-src"));
    DEFINE_STATIC_LOCAL(String, mediaSrc, ("media-src"));
    DEFINE_STATIC_LOCAL(String, reportURI, ("report-uri"));
    DEFINE_STATIC_LOCAL(String, options, ("options"));

    ASSERT(!name.isEmpty());

    if (!m_scriptSrc && equalIgnoringCase(name, scriptSrc))
        m_scriptSrc = createCSPDirective(name, value);
    else if (!m_objectSrc && equalIgnoringCase(name, objectSrc))
        m_objectSrc = createCSPDirective(name, value);
    else if (!m_frameSrc && equalIgnoringCase(name, frameSrc))
        m_frameSrc = createCSPDirective(name, value);
    else if (!m_imgSrc && equalIgnoringCase(name, imgSrc))
        m_imgSrc = createCSPDirective(name, value);
    else if (!m_styleSrc && equalIgnoringCase(name, styleSrc))
        m_styleSrc = createCSPDirective(name, value);
    else if (!m_fontSrc && equalIgnoringCase(name, fontSrc))
        m_fontSrc = createCSPDirective(name, value);
    else if (!m_mediaSrc && equalIgnoringCase(name, mediaSrc))
        m_mediaSrc = createCSPDirective(name, value);
    else if (m_reportURLs.isEmpty() && equalIgnoringCase(name, reportURI))
        parseReportURI(value);
    else if (!m_options && equalIgnoringCase(name, options))
        m_options = adoptPtr(new CSPOptions(value));
}

}
