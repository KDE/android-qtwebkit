/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#ifndef KURL_H_
#define KURL_H_

#include "KWQMemArray.h"
#include "KWQString.h"
#include "KWQValueList.h"

class QTextCodec;

#ifdef __OBJC__
@class NSData;
@class NSURL;
#else
class NSData;
class NSURL;
#endif

struct KWQIntegerPair {
    KWQIntegerPair(int s, int e) : start(s), end(e) { }
    int start;
    int end;
};

class KURL {
public:
    KURL();
    KURL(const char *);
    KURL(const KURL &, const QString &, const QTextCodec * = 0);
    KURL(const QString &);
    KURL(NSURL *);
    
    bool isEmpty() const { return urlString.isEmpty(); } 
    bool isMalformed() const { return !m_isValid; }
    bool isValid() const { return m_isValid; }
    bool hasPath() const;

    QString canonicalURL() const;
    QString url() const { return urlString; }

    QString protocol() const;
    QString host() const;
    unsigned short int port() const;
    QString user() const;
    QString pass() const;
    QString path() const;
    QString query() const;
    QString ref() const;
    bool hasRef() const;

    QString htmlRef() const;
    QString encodedHtmlRef() const;

    void setProtocol(const QString &);
    void setHost(const QString &);
    void setPort(unsigned short int);
    void setUser(const QString &);
    void setPass(const QString &);
    void setPath(const QString &);
    void setQuery(const QString &, int encoding_hint=0);
    void setRef(const QString &);

    QString prettyURL() const;
    
    NSURL *getNSURL() const;
    NSData *getNSData() const;
    
    static QString decode_string(const QString &);
    static QString encode_string(const QString &);
    
    friend bool operator==(const KURL &, const KURL &);

private:
    void parse(const char *url, const QString *originalString);

    static QString encodeHostname(const QString &);
    static QString encodeHostnames(const QString &);
    static bool findHostnameInHierarchicalURL(const QString &, int &startOffset, int &endOffset);
    static QMemArray<KWQIntegerPair> findHostnamesInMailToURL(const QString &);

#ifdef CONSTRUCT_CANONICAL_STRING
    QString _path() const;
    QString _user() const;
    QString _pass() const;
    QString _host() const;
#endif
    
    QString urlString;
    bool m_isValid;
    int schemeEndPos;
    int userStartPos;
    int userEndPos;
    int passwordEndPos;
    int hostEndPos;
    int portEndPos;
    int pathEndPos;
    int queryEndPos;
    int fragmentEndPos;
    
    // True if both URLs are the same.
    friend bool urlcmp(const QString &URLA, const QString &URLB, bool ignoreTrailingSlash, bool ignoreRef);
};

#endif
