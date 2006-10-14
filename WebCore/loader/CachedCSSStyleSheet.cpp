/*
    This file is part of the KDE libraries

    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#include "config.h"
#include "CachedCSSStyleSheet.h"

#include "Cache.h"
#include "CachedResourceClient.h"
#include "CachedResourceClientWalker.h"
#include "Decoder.h"
#include "DeprecatedString.h"
#include "LoaderFunctions.h"
#include "loader.h"
#include <wtf/Vector.h>

namespace WebCore {

CachedCSSStyleSheet::CachedCSSStyleSheet(DocLoader* dl, const String& url, CachePolicy cachePolicy, time_t _expireDate, const String& charset)
    : CachedResource(url, CSSStyleSheet, cachePolicy, _expireDate)
    , m_decoder(new Decoder("text/css", charset))
{
    // It's css we want.
    setAccept("text/css");
    // load the file
    Cache::loader()->load(dl, this, false);
    m_loading = true;
}

CachedCSSStyleSheet::~CachedCSSStyleSheet()
{
}

void CachedCSSStyleSheet::ref(CachedResourceClient *c)
{
    CachedResource::ref(c);

    if (!m_loading)
        c->setCSSStyleSheet(m_url, m_decoder->encoding().name(), m_sheet);
}

void CachedCSSStyleSheet::deref(CachedResourceClient *c)
{
    Cache::flush();
    CachedResource::deref(c);
    if (canDelete() && m_free)
        delete this;
}

void CachedCSSStyleSheet::setCharset(const String& chs)
{
    m_decoder->setEncoding(chs, Decoder::EncodingFromHTTPHeader);
}

void CachedCSSStyleSheet::data(Vector<char>& data, bool allDataReceived)
{
    if (!allDataReceived)
        return;

    setSize(data.size());
    m_sheet = m_decoder->decode(data.data(), size());
    m_sheet += m_decoder->flush();
    m_loading = false;
    checkNotify();
}

void CachedCSSStyleSheet::checkNotify()
{
    if (m_loading)
        return;

    CachedResourceClientWalker w(m_clients);
    while (CachedResourceClient *c = w.next()) {
#if !PLATFORM(QT)
        if (m_response && !IsResponseURLEqualToURL(m_response, m_url))
#else 
        if (!m_response.isEmpty() && !IsResponseURLEqualToURL(m_response, m_url))
#endif
            c->setCSSStyleSheet(ResponseURL(m_response), m_decoder->encoding().name(), m_sheet);
        else
            c->setCSSStyleSheet(m_url, m_decoder->encoding().name(), m_sheet);
    }
}

void CachedCSSStyleSheet::error()
{
    m_loading = false;
    checkNotify();
}

}
