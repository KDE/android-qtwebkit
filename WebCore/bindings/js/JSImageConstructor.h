/*
 *  Copyright (C) 1999 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef JSImageConstructor_h
#define JSImageConstructor_h

#include "kjs_binding.h"

namespace WebCore {

    class Document;

    class JSImageConstructor : public DOMObject {
    public:
        JSImageConstructor(KJS::ExecState*, Document*);

        virtual bool implementsConstruct() const;
        virtual KJS::JSObject* construct(KJS::ExecState*, const KJS::List&);

        virtual const KJS::ClassInfo* classInfo() const { return &info; }
        static const KJS::ClassInfo info;

    private:
        RefPtr<Document> m_document;
    };

} // namespace WebCore

#endif // JSImageConstructor_h
