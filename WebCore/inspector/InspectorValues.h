/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorValues_h
#define InspectorValues_h

#if ENABLE(INSPECTOR)

#include "PlatformString.h"
#include "StringHash.h"

#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class InspectorArray;
class InspectorObject;
class String;

class InspectorValue : public RefCounted<InspectorValue> {
public:
    InspectorValue() : m_type(TypeNull) { }
    virtual ~InspectorValue() { }

    static PassRefPtr<InspectorValue> null()
    {
        return adoptRef(new InspectorValue());
    }

    typedef enum {
        TypeNull = 0,
        TypeBoolean,
        TypeDouble,
        TypeString,
        TypeObject,
        TypeArray
    } Type;

    Type type() const { return m_type; }

    virtual bool asBool(bool* output) const;
    virtual bool asNumber(double* output) const;
    virtual bool asString(String* output) const;
    virtual PassRefPtr<InspectorObject> asObject();
    virtual PassRefPtr<InspectorArray> asArray();

    static PassRefPtr<InspectorValue> parseJSON(const String& json);

    String toJSONString() const;
    virtual void writeJSON(Vector<UChar>* output) const;

protected:
    explicit InspectorValue(Type type) : m_type(type) { }

private:
    Type m_type;
};

class InspectorBasicValue : public InspectorValue {
public:

    static PassRefPtr<InspectorBasicValue> create(bool value)
    {
        return adoptRef(new InspectorBasicValue(value));
    }

    static PassRefPtr<InspectorBasicValue> create(int value)
    {
        return adoptRef(new InspectorBasicValue(value));
    }

    static PassRefPtr<InspectorBasicValue> create(double value)
    {
        return adoptRef(new InspectorBasicValue(value));
    }

    virtual bool asBool(bool* output) const;
    virtual bool asNumber(double* output) const;

    virtual void writeJSON(Vector<UChar>* output) const;

private:
    explicit InspectorBasicValue(bool value) : InspectorValue(TypeBoolean), m_boolValue(value) { }
    explicit InspectorBasicValue(int value) : InspectorValue(TypeDouble), m_doubleValue((double)value) { }
    explicit InspectorBasicValue(double value) : InspectorValue(TypeDouble), m_doubleValue(value) { }

    union {
        bool m_boolValue;
        double m_doubleValue;
    };
};

class InspectorString : public InspectorValue {
public:
    static PassRefPtr<InspectorString> create(const String& value)
    {
        return adoptRef(new InspectorString(value));
    }

    static PassRefPtr<InspectorString> create(const char* value)
    {
        return adoptRef(new InspectorString(value));
    }

    virtual bool asString(String* output) const;    

    virtual void writeJSON(Vector<UChar>* output) const;

private:
    explicit InspectorString(const String& value) : InspectorValue(TypeString), m_stringValue(value) { }
    explicit InspectorString(const char* value) : InspectorValue(TypeString), m_stringValue(value) { }

    String m_stringValue;
};

class InspectorObject : public InspectorValue {
private:
    typedef HashMap<String, RefPtr<InspectorValue> > Dictionary;

public:
    typedef Dictionary::iterator iterator;
    typedef Dictionary::const_iterator const_iterator;

public:
    static PassRefPtr<InspectorObject> create()
    {
        return adoptRef(new InspectorObject());
    }
    ~InspectorObject() { }

    virtual PassRefPtr<InspectorObject> asObject();

    void setBool(const String& name, bool);
    void setNumber(const String& name, double);
    void setString(const String& name, const String&);
    void set(const String& name, PassRefPtr<InspectorValue>);

    bool getBool(const String& name, bool* output) const;
    bool getNumber(const String& name, double* output) const;
    bool getString(const String& name, String* output) const;
    PassRefPtr<InspectorObject> getObject(const String& name) const;
    PassRefPtr<InspectorArray> getArray(const String& name) const;
    PassRefPtr<InspectorValue> get(const String& name) const;

    virtual void writeJSON(Vector<UChar>* output) const;

    iterator begin() { return m_data.begin(); }
    iterator end() { return m_data.end(); }
    const_iterator begin() const { return m_data.begin(); }
    const_iterator end() const { return m_data.end(); }

private:
    InspectorObject() : InspectorValue(TypeObject) { }
    Dictionary m_data;
    Vector<String> m_order;
};

class InspectorArray : public InspectorValue {
public:
    static PassRefPtr<InspectorArray> create()
    {
        return adoptRef(new InspectorArray());
    }
    ~InspectorArray() { }

    virtual PassRefPtr<InspectorArray> asArray();

    void pushBool(bool);
    void pushNumber(double);
    void pushString(const String&);
    void push(PassRefPtr<InspectorValue>);
    unsigned length() const { return m_data.size(); }

    PassRefPtr<InspectorValue> get(size_t index);

    virtual void writeJSON(Vector<UChar>* output) const;

private:
    InspectorArray() : InspectorValue(TypeArray) { }
    Vector<RefPtr<InspectorValue> > m_data;
};

inline void InspectorObject::setBool(const String& name, bool value)
{
    set(name, InspectorBasicValue::create(value));
}

inline void InspectorObject::setNumber(const String& name, double value)
{
    set(name, InspectorBasicValue::create(value));
}

inline void InspectorObject::setString(const String& name, const String& value)
{
    set(name, InspectorString::create(value));
}

inline void InspectorObject::set(const String& name, PassRefPtr<InspectorValue> value)
{
    if (m_data.set(name, value).second)
        m_order.append(name);
}

inline void InspectorArray::pushBool(bool value)
{
    m_data.append(InspectorBasicValue::create(value));
}

inline void InspectorArray::pushNumber(double value)
{
    m_data.append(InspectorBasicValue::create(value));
}

inline void InspectorArray::pushString(const String& value)
{
    m_data.append(InspectorString::create(value));
}

inline void InspectorArray::push(PassRefPtr<InspectorValue> value)
{
    m_data.append(value);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
#endif // !defined(InspectorValues_h)
