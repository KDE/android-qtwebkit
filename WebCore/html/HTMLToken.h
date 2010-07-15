/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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

#ifndef HTMLToken_h
#define HTMLToken_h

#include "NamedNodeMap.h"
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class HTMLToken : public Noncopyable {
public:
    enum Type {
        Uninitialized,
        DOCTYPE,
        StartTag,
        EndTag,
        Comment,
        Character,
        EndOfFile,
    };

    class Attribute {
    public:
        WTF::Vector<UChar, 32> m_name;
        WTF::Vector<UChar, 32> m_value;
    };

    typedef WTF::Vector<Attribute, 10> AttributeList;
    typedef WTF::Vector<UChar, 1024> DataVector;

    HTMLToken() { clear(); }

    void clear()
    {
        m_type = Uninitialized;
    }

    void makeEndOfFile()
    {
        ASSERT(m_type == Uninitialized);
        m_type = EndOfFile;
    }

    void beginStartTag(UChar character)
    {
        ASSERT(character);
        ASSERT(m_type == Uninitialized);
        m_type = StartTag;
        m_data.clear();
        m_selfClosing = false;
        m_currentAttribute = 0;
        m_attributes.clear();

        m_data.append(character);
    }

    template<typename T>
    void beginEndTag(T characters)
    {
        ASSERT(m_type == Uninitialized);
        m_type = EndTag;
        m_data.clear();
        m_selfClosing = false;
        m_currentAttribute = 0;
        m_attributes.clear();

        m_data.append(characters);
    }

    void beginCharacter(UChar character)
    {
        ASSERT(character);
        ASSERT(m_type == Uninitialized);
        m_type = Character;
        m_data.clear();
        m_data.append(character);
    }

    void beginComment()
    {
        ASSERT(m_type == Uninitialized);
        m_type = Comment;
        m_data.clear();
    }

    void beginDOCTYPE()
    {
        ASSERT(m_type == Uninitialized);
        m_type = DOCTYPE;
        m_data.clear();
        m_doctypeData.set(new DoctypeData());
    }

    void beginDOCTYPE(UChar character)
    {
        ASSERT(character);
        beginDOCTYPE();
        m_data.append(character);
    }

    void appendToName(UChar character)
    {
        ASSERT(character);
        ASSERT(m_type == StartTag || m_type == EndTag || m_type == DOCTYPE);
        m_data.append(character);
    }

    template<typename T>
    void appendToCharacter(T characters)
    {
        ASSERT(m_type == Character);
        m_data.append(characters);
    }

    void appendToComment(UChar character)
    {
        ASSERT(character);
        ASSERT(m_type == Comment);
        m_data.append(character);
    }

    void addNewAttribute()
    {
        ASSERT(m_type == StartTag || m_type == EndTag);
        m_attributes.grow(m_attributes.size() + 1);
        m_currentAttribute = &m_attributes.last();
    }

    void appendToAttributeName(UChar character)
    {
        ASSERT(character);
        ASSERT(m_type == StartTag || m_type == EndTag);
        m_currentAttribute->m_name.append(character);
    }

    void appendToAttributeValue(UChar character)
    {
        ASSERT(character);
        ASSERT(m_type == StartTag || m_type == EndTag);
        m_currentAttribute->m_value.append(character);
    }

    Type type() const { return m_type; }

    bool selfClosing() const
    {
        ASSERT(m_type == StartTag || m_type == EndTag);
        return m_selfClosing;
    }

    void setSelfClosing()
    {
        ASSERT(m_type == HTMLToken::StartTag || m_type == HTMLToken::EndTag);
        m_selfClosing = true;
    }

    const AttributeList& attributes() const
    {
        ASSERT(m_type == StartTag || m_type == EndTag);
        return m_attributes;
    }

    const DataVector& name() const
    {
        ASSERT(m_type == StartTag || m_type == EndTag || m_type == DOCTYPE);
        return m_data;
    }

    const DataVector& characters() const
    {
        ASSERT(m_type == Character);
        return m_data;
    }

    const DataVector& comment() const
    {
        ASSERT(m_type == Comment);
        return m_data;
    }

    // FIXME: Distinguish between a missing public identifer and an empty one.
    const WTF::Vector<UChar>& publicIdentifier() const
    {
        ASSERT(m_type == DOCTYPE);
        return m_doctypeData->m_publicIdentifier;
    }

    // FIXME: Distinguish between a missing system identifer and an empty one.
    const WTF::Vector<UChar>& systemIdentifier() const
    {
        ASSERT(m_type == DOCTYPE);
        return m_doctypeData->m_systemIdentifier;
    }

    void setPublicIdentifierToEmptyString()
    {
        ASSERT(m_type == DOCTYPE);
        m_doctypeData->m_hasPublicIdentifier = true;
        m_doctypeData->m_publicIdentifier.clear();
    }

    void setSystemIdentifierToEmptyString()
    {
        ASSERT(m_type == DOCTYPE);
        m_doctypeData->m_hasSystemIdentifier = true;
        m_doctypeData->m_systemIdentifier.clear();
    }

    bool forceQuirks() const
    {
        ASSERT(m_type == DOCTYPE);
        return m_doctypeData->m_forceQuirks;
    }

    void setForceQuirks()
    {
        ASSERT(m_type == DOCTYPE);
        m_doctypeData->m_forceQuirks = true;
    }

    void appendToPublicIdentifier(UChar character)
    {
        ASSERT(character);
        ASSERT(m_type == DOCTYPE);
        ASSERT(m_doctypeData->m_hasPublicIdentifier);
        m_doctypeData->m_publicIdentifier.append(character);
    }

    void appendToSystemIdentifier(UChar character)
    {
        ASSERT(character);
        ASSERT(m_type == DOCTYPE);
        ASSERT(m_doctypeData->m_hasSystemIdentifier);
        m_doctypeData->m_systemIdentifier.append(character);
    }

private:
    // FIXME: I'm not sure what the final relationship between HTMLToken and
    // AtomicHTMLToken will be.  I'm marking this a friend for now, but we'll
    // want to end up with a cleaner interface between the two classes.
    friend class AtomicHTMLToken;

    class DoctypeData {
    public:
        DoctypeData()
            : m_hasPublicIdentifier(false)
            , m_hasSystemIdentifier(false)
            , m_forceQuirks(false)
        {
        }

        bool m_hasPublicIdentifier;
        bool m_hasSystemIdentifier;
        bool m_forceQuirks;
        WTF::Vector<UChar> m_publicIdentifier;
        WTF::Vector<UChar> m_systemIdentifier;
    };

    Type m_type;

    // "name" for DOCTYPE, StartTag, and EndTag
    // "characters" for Character
    // "data" for Comment
    DataVector m_data;

    // For DOCTYPE
    OwnPtr<DoctypeData> m_doctypeData;

    // For StartTag and EndTag
    bool m_selfClosing;
    AttributeList m_attributes; // Old tokenizer reserves 10.

    // A pointer into m_attributes used during lexing.
    Attribute* m_currentAttribute;
};

// FIXME: This class should eventually be named HTMLToken once we move the
// exiting HTMLToken to be internal to the HTMLTokenizer.
class AtomicHTMLToken : public Noncopyable {
public:
    AtomicHTMLToken(HTMLToken& token)
        : m_type(token.type())
    {
        switch (m_type) {
        case HTMLToken::Uninitialized:
            ASSERT_NOT_REACHED();
            break;
        case HTMLToken::DOCTYPE:
            m_name = AtomicString(token.name().data(), token.name().size());
            m_doctypeData = token.m_doctypeData.release();
            break;
        case HTMLToken::EndOfFile:
            break;
        case HTMLToken::StartTag:
        case HTMLToken::EndTag: {
            m_selfClosing = token.selfClosing();
            m_name = AtomicString(token.name().data(), token.name().size());
            const HTMLToken::AttributeList& attributes = token.attributes();
            for (HTMLToken::AttributeList::const_iterator iter = attributes.begin();
                 iter != attributes.end(); ++iter) {
                if (!iter->m_name.isEmpty()) {
                    String name(iter->m_name.data(), iter->m_name.size());
                    String value(iter->m_value.data(), iter->m_value.size());
                    RefPtr<Attribute> mappedAttribute = Attribute::createMapped(name, value);
                    if (!m_attributes)
                        m_attributes = NamedNodeMap::create();
                    m_attributes->insertAttribute(mappedAttribute.release(), false);
                }
            }
            break;
        }
        case HTMLToken::Comment:
            m_data = String(token.comment().data(), token.comment().size());
            break;
        case HTMLToken::Character:
            m_externalCharacters = &token.characters();
            break;
        }
    }

    AtomicHTMLToken(HTMLToken::Type type, AtomicString name, PassRefPtr<NamedNodeMap> attributes = 0)
        : m_type(type)
        , m_name(name)
        , m_attributes(attributes)
    {
        ASSERT(usesName());
    }

    HTMLToken::Type type() const { return m_type; }

    const AtomicString& name() const
    {
        ASSERT(usesName());
        return m_name;
    }

    void setName(const AtomicString& name)
    {
        ASSERT(usesName());
        m_name = name;
    }

    bool selfClosing() const
    {
        ASSERT(m_type == HTMLToken::StartTag || m_type == HTMLToken::EndTag);
        return m_selfClosing;
    }

    Attribute* getAttributeItem(const QualifiedName& attributeName)
    {
        ASSERT(usesAttributes());
        if (!m_attributes)
            return 0;
        return m_attributes->getAttributeItem(attributeName);
    }

    NamedNodeMap* attributes() const
    {
        ASSERT(usesAttributes());
        return m_attributes.get();
    }

    PassRefPtr<NamedNodeMap> takeAtributes()
    {
        ASSERT(usesAttributes());
        return m_attributes.release();
    }

    const HTMLToken::DataVector& characters() const
    {
        ASSERT(m_type == HTMLToken::Character);
        return *m_externalCharacters;
    }

    const String& comment() const
    {
        ASSERT(m_type == HTMLToken::Comment);
        return m_data;
    }

    // FIXME: Distinguish between a missing public identifer and an empty one.
    WTF::Vector<UChar>& publicIdentifier() const
    {
        ASSERT(m_type == HTMLToken::DOCTYPE);
        return m_doctypeData->m_publicIdentifier;
    }

    // FIXME: Distinguish between a missing system identifer and an empty one.
    WTF::Vector<UChar>& systemIdentifier() const
    {
        ASSERT(m_type == HTMLToken::DOCTYPE);
        return m_doctypeData->m_systemIdentifier;
    }

    bool forceQuirks() const
    {
        ASSERT(m_type == HTMLToken::DOCTYPE);
        return m_doctypeData->m_forceQuirks;
    }

private:
    HTMLToken::Type m_type;

    bool usesName() const
    {
        return m_type == HTMLToken::StartTag || m_type == HTMLToken::EndTag || m_type == HTMLToken::DOCTYPE;
    }

    bool usesAttributes() const
    {
        return m_type == HTMLToken::StartTag || m_type == HTMLToken::EndTag;
    }

    // "name" for DOCTYPE, StartTag, and EndTag
    AtomicString m_name;

    // "data" for Comment
    String m_data;

    // "characters" for Character
    //
    // We don't want to copy the the characters out of the HTMLToken, so we
    // keep a pointer to its buffer instead.  This buffer is owned by the
    // HTMLToken and causes a lifetime dependence between these objects.
    //
    // FIXME: Add a mechanism for "internalizing" the characters when the
    //        HTMLToken is destructed.
    const HTMLToken::DataVector* m_externalCharacters;

    // For DOCTYPE
    OwnPtr<HTMLToken::DoctypeData> m_doctypeData;

    // For StartTag and EndTag
    bool m_selfClosing;

    RefPtr<NamedNodeMap> m_attributes;
};

}

#endif
