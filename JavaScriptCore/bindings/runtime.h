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
#ifndef _RUNTIME_H_
#define _RUNTIME_H_

#include "list.h"
#include "value.h"

namespace Bindings
{

class Instance;

// For now just use Java style type descriptors.
typedef const char * RuntimeType;

class Parameter
{
public:
    virtual RuntimeType type() const = 0;
    virtual ~Parameter() {};
};

class Constructor
{
public:
    virtual Parameter *parameterAt(long i) const = 0;
    virtual long numParameters() const = 0;

    virtual KJS::Value value() const = 0;

    virtual ~Constructor() {};
};

class Field
{
public:
    virtual const char *name() const = 0;
    virtual RuntimeType type() const = 0;

    virtual KJS::Value valueFromInstance(const Instance *instance) const = 0;

    virtual ~Field() {};
};

class Method
{
public:
    virtual const char *name() const = 0;
    virtual RuntimeType returnType() const = 0;
    virtual Parameter *parameterAt(long i) const = 0;
    virtual long numParameters() const = 0;
    
    virtual KJS::Value value() const = 0;
    
    virtual ~Method() {};
};

class Class
{
public:
    virtual const char *name() const = 0;
    
    virtual Method *methodNamed(const char *name) const = 0;
    
    virtual Constructor *constructorAt(long i) const = 0;
    virtual long numConstructors() const = 0;
    
    virtual Field *fieldNamed(const char *name) const = 0;

    virtual ~Class() {};
};

class Instance
{
public:
    typedef enum {
        JavaLanguage,
        ObjectiveCLanguage
    } BindingLanguage;

    static Instance *createBindingForLanguageInstance (BindingLanguage language, void *instance);

    virtual Class *getClass() const = 0;
    
    virtual KJS::Value getValueOfField (const Field *aField) const;
    
    virtual KJS::Value invokeMethod (KJS::ExecState *exec, const Method *method, const KJS::List &args) = 0;
        
    virtual ~Instance() {};
};

const char *signatureForParameters(const KJS::List &aList);

};

#endif
