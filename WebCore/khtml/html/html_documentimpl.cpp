/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Portions are Copyright (C) 2002 Netscape Communications Corporation.
 * Other contributors: David Baron <dbaron@fas.harvard.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Alternatively, the document type parsing portions of this file may be used
 * under the terms of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "html/html_documentimpl.h"
#include "html/html_imageimpl.h"
#include "html/html_headimpl.h"
#include "html/html_baseimpl.h"
#include "html/htmltokenizer.h"
#include "html/html_miscimpl.h"
#include "html/html_imageimpl.h"
#include "html/html_formimpl.h"

#include "khtmlview.h"
#include "khtml_part.h"
#include "khtmlpart_p.h"
#include "khtml_settings.h"
#include "misc/htmlattrs.h"
#include "misc/htmlhashes.h"

#include "xml/xml_tokenizer.h"
#include "xml/dom2_eventsimpl.h"

#include "khtml_factory.h"
#include "rendering/render_object.h"

#include <dcopclient.h>
#include <kapplication.h>
#include <kdebug.h>
#include <kurl.h>
#include <kglobal.h>
#include <kcharsets.h>
#include <kglobalsettings.h>

#include "css/cssproperties.h"
#include "css/cssstyleselector.h"
#include "css/css_stylesheetimpl.h"
#include <stdlib.h>
#include <qptrstack.h>

#if APPLE_CHANGES
#include "KWQKCookieJar.h"
#endif


// Turn off inlining to avoid warning with newer gcc.
#undef __inline
#define __inline
#include "doctypes.cpp"
#undef __inline


template class QPtrStack<DOM::NodeImpl>;

using namespace DOM;
using namespace khtml;


HTMLDocumentImpl::HTMLDocumentImpl(DOMImplementationImpl *_implementation, KHTMLView *v)
  : DocumentImpl(_implementation, v)
{
//    kdDebug( 6090 ) << "HTMLDocumentImpl constructor this = " << this << endl;
    bodyElement = 0;
    htmlElement = 0;

/* dynamic history stuff to be fixed later (pfeiffer)
    connect( KHTMLFactory::vLinks(), SIGNAL( inserted( const QString& )),
             SLOT( slotHistoryChanged() ));
    connect( KHTMLFactory::vLinks(), SIGNAL( removed( const QString& )),
             SLOT( slotHistoryChanged() ));
*/
    connect( KHTMLFactory::vLinks(), SIGNAL( cleared()),
             SLOT( slotHistoryChanged() ));
    m_startTime.restart();

    processingLoadEvent = false;
}

HTMLDocumentImpl::~HTMLDocumentImpl()
{
}

DOMString HTMLDocumentImpl::referrer() const
{
    if ( view() )
#if APPLE_CHANGES
        return KWQ(view()->part())->incomingReferrer();
#else
        // This is broken; returns the referrer used for links within this page (basically
        // the same as the URL), not the referrer used for loading this page itself.
        return view()->part()->referrer();
#endif
    return DOMString();
}

DOMString HTMLDocumentImpl::domain() const
{
    if ( m_domain.isEmpty() ) // not set yet (we set it on demand to save time and space)
        m_domain = KURL(URL()).host(); // Initially set to the host
    return m_domain;
}

void HTMLDocumentImpl::setDomain(const DOMString &newDomain, bool force /*=false*/)
{
    if ( force ) {
        m_domain = newDomain;
        return;
    }
    if ( m_domain.isEmpty() ) // not set yet (we set it on demand to save time and space)
        m_domain = KURL(URL()).host(); // Initially set to the host

    // Both NS and IE specify that changing the domain is only allowed when
    // the new domain is a suffix of the old domain.
    int oldLength = m_domain.length();
    int newLength = newDomain.length();
    if ( newLength < oldLength ) // e.g. newDomain=kde.org (7) and m_domain=www.kde.org (11)
    {
        DOMString test = m_domain.copy();
        if ( test[oldLength - newLength - 1] == '.' ) // Check that it's a subdomain, not e.g. "de.org"
        {
            test.remove( 0, oldLength - newLength ); // now test is "kde.org" from m_domain
            if ( test == newDomain )                 // and we check that it's the same thing as newDomain
                m_domain = newDomain;
        }
    }
}

DOMString HTMLDocumentImpl::lastModified() const
{
    if ( view() )
        return view()->part()->lastModified();
    return DOMString();
}

DOMString HTMLDocumentImpl::cookie() const
{
#if APPLE_CHANGES
    return KWQKCookieJar::cookie(URL());
#else
    long windowId = 0;
    KHTMLView *v = view ();
    
    if ( v && v->topLevelWidget() )
      windowId = v->topLevelWidget()->winId();

    QCString replyType;
    QByteArray params, reply;
    QDataStream stream(params, IO_WriteOnly);
    stream << URL() << windowId;
    if (!kapp->dcopClient()->call("kcookiejar", "kcookiejar",
                                  "findDOMCookies(QString, int)", params, 
                                  replyType, reply)) {
         // Maybe it wasn't running (e.g. we're opening local html files)
         KApplication::startServiceByDesktopName( "kcookiejar");
         if (!kapp->dcopClient()->call("kcookiejar", "kcookiejar",
                                       "findDOMCookies(QString)", params, replyType, reply)) {
           kdWarning(6010) << "Can't communicate with cookiejar!" << endl;
           return DOMString();
         }
    }

    QDataStream stream2(reply, IO_ReadOnly);
    if(replyType != "QString") {
         kdError(6010) << "DCOP function findDOMCookies(...) returns "
                       << replyType << ", expected QString" << endl;
         return DOMString();
    }

    QString result;
    stream2 >> result;
    return DOMString(result);
#endif // APPLE_CHANGES
}

void HTMLDocumentImpl::setCookie( const DOMString & value )
{
#if APPLE_CHANGES
    return KWQKCookieJar::setCookie(URL(), m_policyBaseURL.string(), value.string());
#else
    long windowId = 0;
    KHTMLView *v = view ();
    
    if ( v && v->topLevelWidget() )
      windowId = v->topLevelWidget()->winId();
     
    QByteArray params;
    QDataStream stream(params, IO_WriteOnly);
    QString fake_header("Set-Cookie: ");
    fake_header.append(value.string());
    fake_header.append("\n");
    stream << URL() << fake_header.utf8() << windowId;
    if (!kapp->dcopClient()->send("kcookiejar", "kcookiejar",
                                  "addCookies(QString,QCString,long int)", params))
    {
         // Maybe it wasn't running (e.g. we're opening local html files)
         KApplication::startServiceByDesktopName( "kcookiejar");
         if (!kapp->dcopClient()->send("kcookiejar", "kcookiejar",
                                       "addCookies(QString,QCString,long int)", params))
             kdWarning(6010) << "Can't communicate with cookiejar!" << endl;
    }
#endif
}



HTMLElementImpl *HTMLDocumentImpl::body()
{
    NodeImpl *de = documentElement();
    if (!de)
        return 0;

    // try to prefer a FRAMESET element over BODY
    NodeImpl* body = 0;
    for (NodeImpl* i = de->firstChild(); i; i = i->nextSibling()) {
        if (i->id() == ID_FRAMESET)
            return static_cast<HTMLElementImpl*>(i);

        if (i->id() == ID_BODY)
            body = i;
    }
    return static_cast<HTMLElementImpl *>(body);
}

void HTMLDocumentImpl::setBody(HTMLElementImpl *_body)
{
    int exceptioncode = 0;
    HTMLElementImpl *b = body();
    if ( !_body && !b ) return;
    if ( !_body )
        documentElement()->removeChild( b, exceptioncode );
    else if ( !b )
        documentElement()->appendChild( _body, exceptioncode );
    else
        documentElement()->replaceChild( _body, b, exceptioncode );
}

Tokenizer *HTMLDocumentImpl::createTokenizer()
{
    return new HTMLTokenizer(docPtr(),m_view);
}

// --------------------------------------------------------------------------
// not part of the DOM
// --------------------------------------------------------------------------

bool HTMLDocumentImpl::childAllowed( NodeImpl *newChild )
{
    // ### support comments. etc as a child
    return (newChild->id() == ID_HTML || newChild->id() == ID_COMMENT);
}

ElementImpl *HTMLDocumentImpl::createElement( const DOMString &name )
{
    return createHTMLElement(name);
}

void HTMLDocumentImpl::slotHistoryChanged()
{
    if ( true || !m_render ) // disabled for now
        return;

    recalcStyle( Force );
    m_render->repaint();
}

HTMLMapElementImpl* HTMLDocumentImpl::getMap(const DOMString& _url)
{
    QString url = _url.string();
    QString s;
    int pos = url.find('#');
    //kdDebug(0) << "map pos of #:" << pos << endl;
    s = QString(_url.unicode() + pos + 1, _url.length() - pos - 1);

    QMapConstIterator<QString,HTMLMapElementImpl*> it = mapMap.find(s);

    if (it != mapMap.end())
        return *it;
    else
        return 0;
}

void HTMLDocumentImpl::close()
{
    // First fire the onload.
    bool doload = !parsing() && m_tokenizer && !processingLoadEvent;
    
    bool wasNotRedirecting = !view() || view()->part()->d->m_scheduledRedirection == noRedirectionScheduled;

    processingLoadEvent = true;
    if (body() && doload) {
	// We have to clear the tokenizer, in case someone document.write()s from the
	// onLoad event handler, as in Radar 3206524
	delete m_tokenizer;
	m_tokenizer = 0;
        dispatchImageLoadEventsNow();
        body()->dispatchWindowEvent(EventImpl::LOAD_EVENT, false, false);
    }
    processingLoadEvent = false;
        
    // Make sure both the initial layout and reflow happen after the onload
    // fires. This will improve onload scores, and other browsers do it.
    // If they wanna cheat, we can too. -dwh
    if (doload && wasNotRedirecting && view()
            && view()->part()->d->m_scheduledRedirection != noRedirectionScheduled
            && view()->part()->d->m_delayRedirect == 0
            && m_startTime.elapsed() < 1000) {
        static int redirectCount = 0;
        if (redirectCount++ % 4) {
            // When redirecting over and over (e.g., i-bench), to avoid the appearance of complete inactivity,
            // paint every fourth page.
            // Just bail out. During the onload we were shifted to another page.
            // i-Bench does this. When this happens don't bother painting or laying out.        
            delete m_tokenizer;
            m_tokenizer = 0;
            view()->unscheduleRelayout();
            return;
        }
    }
                
    // The initial layout happens here.
    DocumentImpl::closeInternal(!doload);

    // Now do our painting
    if (body() && doload) {
        updateRendering();
        
        // Always do a layout/repaint after loading.
        if (renderer()) {
            renderer()->layoutIfNeeded();
            renderer()->repaint();
        }
    }
}

void HTMLDocumentImpl::addNamedImageOrForm(const QString &name)
{
    if (name.length() == 0) {
	return;
    }
 
    int oldCount = (int)namedImageAndFormCounts.find(name);
    namedImageAndFormCounts.insert(name, (char *)(oldCount + 1));
}

void HTMLDocumentImpl::removeNamedImageOrForm(const QString &name)
{ 
    if (name.length() == 0) {
	return;
    }
 
    int oldVal = (int)(namedImageAndFormCounts.find(name));
    if (oldVal != 0) {
	int newVal = oldVal - 1;
	if (newVal == 0) {
	    namedImageAndFormCounts.remove(name);
	} else {
	    namedImageAndFormCounts.insert(name, (char *)newVal);
	}
    }
}

bool HTMLDocumentImpl::haveNamedImageOrForm(const QString &name)
{
    return namedImageAndFormCounts.find(name) != NULL;
}



const int PARSEMODE_HAVE_DOCTYPE	=	(1<<0);
const int PARSEMODE_HAVE_PUBLIC_ID	=	(1<<1);
const int PARSEMODE_HAVE_SYSTEM_ID	=	(1<<2);
const int PARSEMODE_HAVE_INTERNAL	=	(1<<3);

static int parseDocTypePart(const QString& buffer, int index)
{
    while (true) {
        QChar ch = buffer[index];
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')
            ++index;
        else if (ch == '-') {
            int tmpIndex=index;
            if (buffer[index+1] == '-' &&
                ((tmpIndex=buffer.find("--", index+2)) != -1))
                index = tmpIndex+2;
            else
                return index;
        }
        else
            return index;
    }
}

static bool containsString(const char* str, const QString& buffer, int offset)
{
    QString startString(str);
    if (offset + startString.length() > buffer.length())
        return false;
    
    QString bufferString = buffer.mid(offset, startString.length()).lower();
    QString lowerStart = startString.lower();

    return bufferString.startsWith(lowerStart);
}

static bool parseDocTypeDeclaration(const QString& buffer,
                                    int* resultFlags,
                                    QString& publicID,
                                    QString& systemID)
{
    bool haveDocType = false;
    *resultFlags = 0;

    // Skip through any comments and processing instructions.
    int index = 0;
    do {
        index = buffer.find('<', index);
        if (index == -1) break;
        QChar nextChar = buffer[index+1];
        if (nextChar == '!') {
            if (containsString("doctype", buffer, index+2)) {
                haveDocType = true;
                index += 9; // Skip "<!DOCTYPE"
                break;
            }
            index = parseDocTypePart(buffer,index);
            index = buffer.find('>', index);
        }
        else if (nextChar == '?')
            index = buffer.find('>', index);
        else
            break;
    } while (index != -1);

    if (!haveDocType)
        return true;
    *resultFlags |= PARSEMODE_HAVE_DOCTYPE;

    index = parseDocTypePart(buffer, index);
    if (!containsString("html", buffer, index))
        return false;
    
    index = parseDocTypePart(buffer, index+4);
    bool hasPublic = containsString("public", buffer, index);
    if (hasPublic) {
        index = parseDocTypePart(buffer, index+6);

        // We've read <!DOCTYPE HTML PUBLIC (not case sensitive).
        // Now we find the beginning and end of the public identifers
        // and system identifiers (assuming they're even present).
        QChar theChar = buffer[index];
        if (theChar != '\"' && theChar != '\'')
            return false;
        
        // |start| is the first character (after the quote) and |end|
        // is the final quote, so there are |end|-|start| characters.
        int publicIDStart = index+1;
        int publicIDEnd = buffer.find(theChar, publicIDStart);
        if (publicIDEnd == -1)
            return false;
        index = parseDocTypePart(buffer, publicIDEnd+1);
        QChar next = buffer[index];
        if (next == '>') {
            // Public identifier present, but no system identifier.
            // Do nothing.  Note that this is the most common
            // case.
        }
        else if (next == '\"' || next == '\'') {
            // We have a system identifier.
            *resultFlags |= PARSEMODE_HAVE_SYSTEM_ID;
            int systemIDStart = index+1;
            int systemIDEnd = buffer.find(next, systemIDStart);
            if (systemIDEnd == -1)
                return false;
            systemID = buffer.mid(systemIDStart, systemIDEnd - systemIDStart);
        }
        else if (next == '[') {
            // We found an internal subset.
            *resultFlags |= PARSEMODE_HAVE_INTERNAL;
        }
        else
            return false; // Something's wrong.

        // We need to trim whitespace off the public identifier.
        publicID = buffer.mid(publicIDStart, publicIDEnd - publicIDStart);
        publicID = publicID.stripWhiteSpace();
        *resultFlags |= PARSEMODE_HAVE_PUBLIC_ID;
    } else {
        if (containsString("system", buffer, index)) {
            // Doctype has a system ID but no public ID
            *resultFlags |= PARSEMODE_HAVE_SYSTEM_ID;
            index = parseDocTypePart(buffer, index+6);
            QChar next = buffer[index];
            if (next != '\"' && next != '\'')
                return false;
            int systemIDStart = index+1;
            int systemIDEnd = buffer.find(next, systemIDStart);
            if (systemIDEnd == -1)
                return false;
            systemID = buffer.mid(systemIDStart, systemIDEnd - systemIDStart);
            index = parseDocTypePart(buffer, systemIDEnd+1);
        }

        QChar nextChar = buffer[index];
        if (nextChar == '[')
            *resultFlags |= PARSEMODE_HAVE_INTERNAL;
        else if (nextChar != '>')
            return false;
    }

    return true;
}

void HTMLDocumentImpl::determineParseMode( const QString &str )
{
    //kdDebug() << "DocumentImpl::determineParseMode str=" << str<< endl;

    // This code more or less mimics Mozilla's implementation (specifically the
    // doctype parsing implemented by David Baron in Mozilla's nsParser.cpp).
    //
    // There are three possible parse modes:
    // COMPAT - quirks mode emulates WinIE
    // and NS4.  CSS parsing is also relaxed in this mode, e.g., unit types can
    // be omitted from numbers.
    // ALMOST STRICT - This mode is identical to strict mode
    // except for its treatment of line-height in the inline box model.  For
    // now (until the inline box model is re-written), this mode is identical
    // to STANDARDS mode.
    // STRICT - no quirks apply.  Web pages will obey the specifications to
    // the letter.

    QString systemID, publicID;
    int resultFlags = 0;
    if (parseDocTypeDeclaration(str, &resultFlags, publicID, systemID)) {
        m_doctype->setName("HTML");
        if (!(resultFlags & PARSEMODE_HAVE_DOCTYPE)) {
            // No doctype found at all.  Default to quirks mode and Html4.
            pMode = Compat;
            hMode = Html4;
        }
        else if ((resultFlags & PARSEMODE_HAVE_INTERNAL) ||
                 !(resultFlags & PARSEMODE_HAVE_PUBLIC_ID)) {
            // Internal subsets always denote full standards, as does
            // a doctype without a public ID.
            pMode = Strict;
            hMode = Html4;
        }
        else {
            // We have to check a list of public IDs to see what we
            // should do.
            QString lowerPubID = publicID.lower();
            const char* pubIDStr = lowerPubID.latin1();
           
            // Look up the entry in our gperf-generated table.
            const PubIDInfo* doctypeEntry = findDoctypeEntry(pubIDStr, publicID.length());
            if (!doctypeEntry) {
                // The DOCTYPE is not in the list.  Assume strict mode.
                pMode = Strict;
                hMode = Html4;
                return;
            }

            switch ((resultFlags & PARSEMODE_HAVE_SYSTEM_ID) ?
                    doctypeEntry->mode_if_sysid :
                    doctypeEntry->mode_if_no_sysid)
            {
                case PubIDInfo::eQuirks3:
                    pMode = Compat;
                    hMode = Html3;
                    break;
                case PubIDInfo::eQuirks:
                    pMode = Compat;
                    hMode = Html4;
                    break;
                case PubIDInfo::eAlmostStandards:
                    pMode = AlmostStrict;
                    hMode = Html4;
                    break;
                 default:
                    assert(false);
            }
        }   
    }
    else {
        // Malformed doctype implies quirks mode.
        pMode = Compat;
        hMode = Html3;
    }
  
//     kdDebug() << "DocumentImpl::determineParseMode: publicId =" << publicId << " systemId = " << systemId << endl;
//     kdDebug() << "DocumentImpl::determineParseMode: htmlMode = " << hMode<< endl;
//     if( pMode == Strict )
//         kdDebug(6020) << " using strict parseMode" << endl;
//     else if (pMode == Compat )
//         kdDebug(6020) << " using compatibility parseMode" << endl;
//     else
//         kdDebug(6020) << " using almost strict parseMode" << endl;
 
}

#include "html_documentimpl.moc"
