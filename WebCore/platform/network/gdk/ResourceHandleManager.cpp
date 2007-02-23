/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
 * All rights reserved.
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

#include "config.h"
#include "ResourceHandleManager.h"

#include "CString.h"
#include "NotImplementedGdk.h"
#include "ResourceHandle.h"
#include "ResourceHandleInternal.h"

namespace WebCore {

const int selectTimeoutMS = 5;
const double pollTimeSeconds = 0.05;

ResourceHandleManager::ResourceHandleManager()
    : m_downloadTimer(this, &ResourceHandleManager::downloadTimerCallback)
    , m_cookieJarFileName(0)
{
    curl_global_init(CURL_GLOBAL_ALL);
    m_curlMultiHandle = curl_multi_init();
    m_curlShareHandle = curl_share_init();
    curl_share_setopt(m_curlShareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
    curl_share_setopt(m_curlShareHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
}

void ResourceHandleManager::setCookieJarFileName(const char* cookieJarFileName)
{ 
    m_cookieJarFileName = strdup(cookieJarFileName);
}

ResourceHandleManager* ResourceHandleManager::sharedInstance()
{
    static ResourceHandleManager* sharedInstance;
    if (!sharedInstance)
        sharedInstance = new ResourceHandleManager();
    return sharedInstance;
}

// called with data after all headers have been processed via headerCallback
static size_t writeCallback(void* ptr, size_t size, size_t nmemb, void* obj)
{
    ResourceHandle* job = static_cast<ResourceHandle*>(obj);
    ResourceHandleInternal* d = job->getInternal();
    int totalSize = size * nmemb;
    if (d->client())
        d->client()->didReceiveData(job, static_cast<char*>(ptr), totalSize, 0);
    return totalSize;
}

// This is being called for each HTTP header in the response so it'll be called
// multiple times for a given response.
static size_t headerCallback(char* ptr, size_t size, size_t nmemb, void* data)
{
    int totalSize = size * nmemb;
    return totalSize;
}

void ResourceHandleManager::downloadTimerCallback(Timer<ResourceHandleManager>* timer)
{
    fd_set fdread;
    fd_set fdwrite;
    fd_set fdexcep;
    int maxfd = 0;

    FD_ZERO(&fdread);
    FD_ZERO(&fdwrite);
    FD_ZERO(&fdexcep);
    curl_multi_fdset(m_curlMultiHandle, &fdread, &fdwrite, &fdexcep, &maxfd);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = selectTimeoutMS * 1000;       // select waits microseconds

    int rc = ::select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);

    if (-1 == rc) {
#ifndef NDEBUG
        printf("bad: select() returned -1\n");
#endif
        return;
    }

    int runningHandles = 0;
    CURLMcode curlCode = CURLM_CALL_MULTI_PERFORM;
    while (CURLM_CALL_MULTI_PERFORM == curlCode) {
        curlCode = curl_multi_perform(m_curlMultiHandle, &runningHandles);
    }

    // check the curl messages indicating completed transfers
    // and free their resources
    while (true) {
        int messagesInQueue;
        CURLMsg* msg = curl_multi_info_read(m_curlMultiHandle, &messagesInQueue);
        if (!msg)
            break;

        if (CURLMSG_DONE != msg->msg)
            continue;

        // find the node which has same d->m_handle as completed transfer
        CURL* handle = msg->easy_handle;
        ASSERT(handle);
        ResourceHandle* job;
        curl_easy_getinfo(handle, CURLINFO_PRIVATE, &job);
        ASSERT(job);
        if (!job)
            continue;

        ResourceHandleInternal* d = job->getInternal();
        if (CURLE_OK == msg->data.result) {
            if (d->client())
                d->client()->didFinishLoading(job);
        } else {
#ifndef NDEBUG
            char* url = 0;
            curl_easy_getinfo(d->m_handle, CURLINFO_EFFECTIVE_URL, &url);
            printf("Curl ERROR for url='%s', error: '%s'\n", url, curl_easy_strerror(msg->data.result));
            free(url);
#endif
            if (d->client())
                d->client()->didFail(job, ResourceError());
        }

        removeFromCurl(job);
    }

    if (!m_downloadTimer.isActive() && (runningHandles > 0))
        m_downloadTimer.startOneShot(pollTimeSeconds);
}

void ResourceHandleManager::removeFromCurl(ResourceHandle* job)
{
    ResourceHandleInternal* d = job->getInternal();
    ASSERT(d->m_handle);
    if (!d->m_handle)
        return;
    curl_multi_remove_handle(m_curlMultiHandle, d->m_handle);
    curl_easy_cleanup(d->m_handle);
    d->m_handle = 0;
}

void ResourceHandleManager::setupPUT(ResourceHandle*)
{
    notImplementedGdk();
}

void ResourceHandleManager::setupPOST(ResourceHandle*)
{
    notImplementedGdk();
}

void ResourceHandleManager::add(ResourceHandle* job)
{
    ResourceHandleInternal* d = job->getInternal();
    DeprecatedString url = job->url().url();
    d->m_handle = curl_easy_init();
    curl_easy_setopt(d->m_handle, CURLOPT_PRIVATE, job);
    curl_easy_setopt(d->m_handle, CURLOPT_ERRORBUFFER, m_curlErrorBuffer);
    curl_easy_setopt(d->m_handle, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(d->m_handle, CURLOPT_WRITEDATA, job);
    curl_easy_setopt(d->m_handle, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(d->m_handle, CURLOPT_WRITEHEADER, job);
    curl_easy_setopt(d->m_handle, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(d->m_handle, CURLOPT_MAXREDIRS, 10);
    curl_easy_setopt(d->m_handle, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
    curl_easy_setopt(d->m_handle, CURLOPT_SHARE, m_curlShareHandle);
    // enable gzip and deflate through Accept-Encoding:
    curl_easy_setopt(d->m_handle, CURLOPT_ENCODING, "");

    // url must remain valid through the request
    ASSERT(!d->m_url);
    d->m_url = strdup(url.ascii());
    curl_easy_setopt(d->m_handle, CURLOPT_URL, d->m_url);

    if (m_cookieJarFileName) {
        curl_easy_setopt(d->m_handle, CURLOPT_COOKIEFILE, m_cookieJarFileName);
        curl_easy_setopt(d->m_handle, CURLOPT_COOKIEJAR, m_cookieJarFileName);
    }

    if (job->requestHeaders().size() > 0) {
        struct curl_slist* headers = 0;
        HTTPHeaderMap customHeaders = job->requestHeaders();
        HTTPHeaderMap::const_iterator end = customHeaders.end();
        for (HTTPHeaderMap::const_iterator it = customHeaders.begin(); it != end; ++it) {
            String key = it->first;
            String value = it->second;
            String headerString = key + ": " + value;
            const char* header = headerString.latin1().data();
            headers = curl_slist_append(headers, header);
        }
        curl_easy_setopt(d->m_handle, CURLOPT_HTTPHEADER, headers);
        d->m_customHeaders = headers;
    }

    // default to GET
    curl_easy_setopt(d->m_handle, CURLOPT_HTTPGET, TRUE);

    if ("POST" == job->method())
        setupPOST(job);
    else if ("PUT" == job->method())
        setupPUT(job);
    else if ("HEAD" == job->method())
        curl_easy_setopt(d->m_handle, CURLOPT_NOBODY, TRUE);

    CURLMcode ret = curl_multi_add_handle(m_curlMultiHandle, d->m_handle);
    // don't call perform, because events must be async
    // timeout will occur and do curl_multi_perform
    if (ret && ret != CURLM_CALL_MULTI_PERFORM) {
#ifndef NDEBUG
        printf("Error %d starting job %s\n", ret, job->url().url().ascii());
#endif
        job->cancel();
        return;
    }

    if (!m_downloadTimer.isActive())
        m_downloadTimer.startOneShot(pollTimeSeconds);
}

void ResourceHandleManager::cancel(ResourceHandle* job)
{
    removeFromCurl(job);
    // FIXME: report an error?
}

} // namespace WebCore
