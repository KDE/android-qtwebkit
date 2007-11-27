/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "config.h"
#include "WebDatabaseManager.h"
#include "WebKitDLL.h"

#include "COMEnumVariant.h"
#include "WebSecurityOrigin.h"

#include <WebCore/BString.h>
#include <WebCore/COMPtr.h>
#include <WebCore/DatabaseTracker.h>
#include <WebCore/FileSystem.h>
#include <WebCore/SecurityOriginData.h>

using namespace WebCore;

static inline bool isEqual(LPCWSTR s1, LPCWSTR s2)
{
    return !wcscmp(s1, s2);
}

class DatabaseDetailsPropertyBag : public IPropertyBag, Noncopyable {
public:
    static DatabaseDetailsPropertyBag* createInstance(const DatabaseDetails&);

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IPropertyBag
    virtual HRESULT STDMETHODCALLTYPE Read(LPCOLESTR pszPropName, VARIANT* pVar, IErrorLog* pErrorLog);
    virtual HRESULT STDMETHODCALLTYPE Write(LPCOLESTR pszPropName, VARIANT* pVar);
private:
    DatabaseDetailsPropertyBag(const DatabaseDetails& details) 
        : m_refCount(0)
        , m_details(details) { }
    ~DatabaseDetailsPropertyBag() { }

    ULONG m_refCount;
    DatabaseDetails m_details;
};

// DatabaseDetailsPropertyBag ------------------------------------------------------
DatabaseDetailsPropertyBag* DatabaseDetailsPropertyBag::createInstance(const DatabaseDetails& details)
{
    DatabaseDetailsPropertyBag* instance = new DatabaseDetailsPropertyBag(details);
    instance->AddRef();
    return instance;
}

// IUnknown ------------------------------------------------------------------------
ULONG STDMETHODCALLTYPE DatabaseDetailsPropertyBag::AddRef()
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE DatabaseDetailsPropertyBag::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete this;

    return newRef;
}

HRESULT STDMETHODCALLTYPE DatabaseDetailsPropertyBag::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<DatabaseDetailsPropertyBag*>(this);
    else if (IsEqualGUID(riid, IID_IPropertyBag))
        *ppvObject = static_cast<DatabaseDetailsPropertyBag*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

// IPropertyBag --------------------------------------------------------------------
HRESULT STDMETHODCALLTYPE DatabaseDetailsPropertyBag::Read(LPCOLESTR pszPropName, VARIANT* pVar, IErrorLog*)
{
    if (!pszPropName || !pVar)
        return E_POINTER;

    VariantInit(pVar);

    if (isEqual(pszPropName, WebDatabaseDisplayNameKey)) {
        COMVariantSetter<String>::setVariant(pVar, m_details.displayName());
        return S_OK;
    } else if (isEqual(pszPropName, WebDatabaseExpectedSizeKey)) {
        COMVariantSetter<unsigned long long>::setVariant(pVar, m_details.expectedUsage());
        return S_OK;
    } else if (isEqual(pszPropName, WebDatabaseUsageKey)) {
        COMVariantSetter<unsigned long long>::setVariant(pVar, m_details.currentUsage());
        return S_OK;
    }

    return E_INVALIDARG;
}

HRESULT STDMETHODCALLTYPE DatabaseDetailsPropertyBag::Write(LPCOLESTR pszPropName, VARIANT* pVar)
{
    if (!pszPropName || !pVar)
        return E_POINTER;

    return E_FAIL;
}

static COMPtr<WebDatabaseManager> s_sharedWebDatabaseManager;

// WebDatabaseManager --------------------------------------------------------------
WebDatabaseManager* WebDatabaseManager::createInstance()
{
    WebDatabaseManager* manager = new WebDatabaseManager();
    manager->AddRef();
    return manager;    
}

WebDatabaseManager::WebDatabaseManager()
    : m_refCount(0)
{
    gClassCount++;
}

WebDatabaseManager::~WebDatabaseManager()
{
    gClassCount--;
}

// IUnknown ------------------------------------------------------------------------
HRESULT STDMETHODCALLTYPE WebDatabaseManager::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<WebDatabaseManager*>(this);
    else if (IsEqualGUID(riid, IID_IWebDatabaseManager))
        *ppvObject = static_cast<WebDatabaseManager*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebDatabaseManager::AddRef()
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebDatabaseManager::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete this;

    return newRef;
}

template<> struct COMVariantSetter<SecurityOriginData> : COMIUnknownVariantSetter<WebSecurityOrigin, SecurityOriginData> {};

// IWebDatabaseManager -------------------------------------------------------------
HRESULT STDMETHODCALLTYPE WebDatabaseManager::sharedWebDatabaseManager( 
    /* [retval][out] */ IWebDatabaseManager** result)
{
    if (!s_sharedWebDatabaseManager)
        s_sharedWebDatabaseManager.adoptRef(WebDatabaseManager::createInstance());

    return s_sharedWebDatabaseManager.copyRefTo(result);
}

HRESULT STDMETHODCALLTYPE WebDatabaseManager::origins( 
    /* [retval][out] */ IEnumVARIANT** result)
{
    if (!result)
        return E_POINTER;

    *result = 0;

    if (this != s_sharedWebDatabaseManager)
        return E_FAIL;

    Vector<SecurityOriginData> origins;
    DatabaseTracker::tracker().origins(origins);
    COMPtr<COMEnumVariant<Vector<SecurityOriginData> > > enumVariant(AdoptCOM, COMEnumVariant<Vector<SecurityOriginData> >::adopt(origins));

    *result = enumVariant.releaseRef();
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebDatabaseManager::databasesWithOrigin( 
    /* [in] */ IWebSecurityOrigin* origin,
    /* [retval][out] */ IEnumVARIANT** result)
{
    if (!origin || !result)
        return E_POINTER;

    *result = 0;

    if (this != s_sharedWebDatabaseManager)
        return E_FAIL;

    COMPtr<WebSecurityOrigin> webSecurityOrigin(Query, origin);
    if (!webSecurityOrigin)
        return E_FAIL;

    Vector<String> databaseNames;
    DatabaseTracker::tracker().databaseNamesForOrigin(webSecurityOrigin->securityOriginData(), databaseNames);

    COMPtr<COMEnumVariant<Vector<String> > > enumVariant(AdoptCOM, COMEnumVariant<Vector<String> >::adopt(databaseNames));

    *result = enumVariant.releaseRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebDatabaseManager::detailsForDatabaseWithOrigin( 
    /* [in] */ BSTR databaseName,
    /* [in] */ IWebSecurityOrigin* origin,
    /* [retval][out] */ IPropertyBag** result)
{
    if (!databaseName || !origin || !result)
        return E_POINTER;

    *result = 0;

    if (this != s_sharedWebDatabaseManager)
        return E_FAIL;

    COMPtr<WebSecurityOrigin> webSecurityOrigin(Query, origin);
    if (!webSecurityOrigin)
        return E_FAIL;

    DatabaseDetails details = DatabaseTracker::tracker().detailsForNameAndOrigin(String(databaseName, SysStringLen(databaseName)),
        webSecurityOrigin->securityOriginData());

    if (!details.isValid())
        return E_INVALIDARG;

    *result = DatabaseDetailsPropertyBag::createInstance(details);
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebDatabaseManager::deleteAllDatabases()
{
    if (this != s_sharedWebDatabaseManager)
        return E_FAIL;

    DatabaseTracker::tracker().deleteAllDatabases();

    return S_OK;
}
   
HRESULT STDMETHODCALLTYPE WebDatabaseManager::deleteDatabasesWithOrigin( 
    /* [in] */ IWebSecurityOrigin* origin)
{
    if (!origin)
        return E_POINTER;

    if (this != s_sharedWebDatabaseManager)
        return E_FAIL;

    COMPtr<WebSecurityOrigin> webSecurityOrigin(Query, origin);
    if (!webSecurityOrigin)
        return E_FAIL;

    DatabaseTracker::tracker().deleteDatabasesWithOrigin(webSecurityOrigin->securityOriginData());

    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE WebDatabaseManager::deleteDatabaseWithOrigin( 
    /* [in] */ BSTR databaseName,
    /* [in] */ IWebSecurityOrigin* origin)
{
    if (!databaseName || !origin)
        return E_POINTER;

    if (this != s_sharedWebDatabaseManager)
        return E_FAIL;

    COMPtr<WebSecurityOrigin> webSecurityOrigin(Query, origin);
    if (!webSecurityOrigin)
        return E_FAIL;

    DatabaseTracker::tracker().deleteDatabase(webSecurityOrigin->securityOriginData(), String(databaseName, SysStringLen(databaseName)));

    return S_OK;
}

void WebKitSetWebDatabasesPathIfNecessary()
{
    static bool pathSet = false;
    if (pathSet)
        return;

    WebCore::String databasesDirectory = WebCore::pathByAppendingComponent(WebCore::localUserSpecificStorageDirectory(), "Databases");
    WebCore::DatabaseTracker::tracker().setDatabasePath(databasesDirectory);

    pathSet = true;
}

