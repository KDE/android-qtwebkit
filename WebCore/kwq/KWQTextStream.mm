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

#import "KWQLogging.h"

#import "KWQTextStream.h"

QTextStream::QTextStream(const QByteArray &ba)
    : _hasByteArray(true), _byteArray(ba), _string(0)
{
}

QTextStream::QTextStream(QString *s, int mode)
    : _hasByteArray(false), _string(s)
{
    ASSERT(mode == IO_WriteOnly);
}

QTextStream &QTextStream::operator<<(char c)
{
    if (_hasByteArray) {
        uint oldSize = _byteArray.size();
        _byteArray.resize(oldSize + 1);
        _byteArray[oldSize] = c;
    }
    if (_string) {
        _string->append(QChar(c));
    }
    return *this;
}

QTextStream &QTextStream::operator<<(short i)
{
    char buffer[10];
    sprintf(buffer, "%d", i);
    return *this << buffer;
}

QTextStream &QTextStream::operator<<(unsigned short i)
{
    char buffer[10];
    sprintf(buffer, "%u", i);
    return *this << buffer;
}

QTextStream &QTextStream::operator<<(int i)
{
    char buffer[10];
    sprintf(buffer, "%d", i);
    return *this << buffer;
}

QTextStream &QTextStream::operator<<(unsigned i)
{
    char buffer[10];
    sprintf(buffer, "%u", i);
    return *this << buffer;
}

QTextStream &QTextStream::operator<<(long i)
{
    char buffer[10];
    sprintf(buffer, "%ld", i);
    return *this << buffer;
}

QTextStream &QTextStream::operator<<(unsigned long i)
{
    char buffer[10];
    sprintf(buffer, "%lu", i);
    return *this << buffer;
}

QTextStream &QTextStream::operator<<(const char *s)
{
    if (_hasByteArray) {
        uint length = strlen(s);
        uint oldSize = _byteArray.size();
        _byteArray.resize(oldSize + length);
        memcpy(_byteArray.data() + oldSize, s, length);
    }
    if (_string) {
        _string->append(s);
    }
    return *this;
}

QTextStream &QTextStream::operator<<(const QCString &qcs)
{
    const char *s = qcs;
    return *this << s;
}

QTextStream &QTextStream::operator<<(const QString &s)
{
    if (_hasByteArray) {
        uint length = s.length();
        uint oldSize = _byteArray.size();
        _byteArray.resize(oldSize + length);
        memcpy(_byteArray.data() + oldSize, s.latin1(), length);
    }
    if (_string) {
        _string->append(s);
    }
    return *this;
}

QTextStream &QTextStream::operator<<(void *p)
{
    char buffer[10];
    sprintf(buffer, "%p", p);
    return *this << buffer;
}

QTextStream &QTextStream::operator<<(const QTextStreamManipulator &m) 
{
    return m(*this);
}

QTextStream &endl(QTextStream& stream)
{
    return stream << '\n';
}

QString QTextIStream::readLine()
{
    ERROR("not yet implemented");
    return QString();
}
