/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#include "IconLoader.h"

#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "IconDatabase.h"
#include "Logging.h"
#include "ResourceHandle.h"
#include "ResourceResponse.h"
#include "ResourceRequest.h"

using namespace std;

namespace WebCore {

const size_t defaultBufferSize = 4096; // bigger than most icons

IconLoader::IconLoader(Frame* frame)
    : m_frame(frame)
    , m_loadIsInProgress(false)
{
}

auto_ptr<IconLoader> IconLoader::create(Frame* frame)
{
    return auto_ptr<IconLoader>(new IconLoader(frame));
}

IconLoader::~IconLoader()
{
    if (m_handle)
        m_handle->kill();
}

void IconLoader::startLoading()
{    
    if (m_handle)
        return;

    // FIXME: http://bugs.webkit.org/show_bug.cgi?id=10902
    // Once ResourceHandle will load without a DocLoader, we can remove this check.
    // A frame may be documentless - one example is a frame containing only a PDF.
    if (!m_frame->document()) {
        LOG(IconDatabase, "Documentless-frame - icon won't be loaded");
        return;
    }

    // Set flag so we can detect the case where the load completes before
    // ResourceHandle::create returns.
    m_loadIsInProgress = true;
    m_buffer.reserveCapacity(defaultBufferSize);

    RefPtr<ResourceHandle> loader = ResourceHandle::create(m_frame->loader()->iconURL(),
        this, m_frame->document()->docLoader());
    if (!loader)
        LOG_ERROR("Failed to start load for icon at url %s", m_frame->loader()->iconURL().url().ascii());

    // Store the handle so we can cancel the load if stopLoading is called later.
    // But only do it if the load hasn't already completed.
    if (m_loadIsInProgress)
        m_handle = loader.release();
}

void IconLoader::stopLoading()
{
    if (m_handle)
        m_handle->kill();
    clearLoadingState();
}

void IconLoader::didReceiveResponse(ResourceHandle* handle, const ResourceResponse& response)
{
    // If we got a status code indicating an invalid response, then lets
    // ignore the data and not try to decode the error page as an icon.
    int status = response.httpStatusCode();
    if (status && (status < 200 || status > 299)) {
        KURL iconURL = handle->url();
        handle->kill();
        finishLoading(iconURL);
    }
}

void IconLoader::didReceiveData(ResourceHandle*, const char* data, int size)
{
    ASSERT(data || size == 0);
    ASSERT(size >= 0);
    m_buffer.append(data, size);
}

void IconLoader::didFailWithError(ResourceHandle* handle, const ResourceError&)
{
    ASSERT(m_loadIsInProgress);
    m_buffer.clear();
    finishLoading(handle->url());
}

void IconLoader::didFinishLoading(ResourceHandle* handle)
{
    // If the icon load resulted in an error-response earlier, the ResourceHandle was killed and icon data commited via finishLoading().
    // In that case this didFinishLoading callback is pointless and we bail.  Otherwise, finishLoading() as expected
    if (m_loadIsInProgress) {
        ASSERT(handle == m_handle);
        finishLoading(handle->url());
    }
}

void IconLoader::finishLoading(const KURL& iconURL)
{
    IconDatabase::sharedIconDatabase()->setIconDataForIconURL(m_buffer.data(), m_buffer.size(), iconURL.url());

    // Tell the frame to map its URL(s) to its iconURL in the database.
    m_frame->loader()->commitIconURLToIconDatabase(iconURL);

    // Send the notification to the app that this icon is finished loading.
#if PLATFORM(MAC) // turn this on for other platforms when FrameLoader is deployed more fully
    m_frame->loader()->notifyIconChanged();
#endif

    clearLoadingState();
}

void IconLoader::clearLoadingState()
{
    m_handle = 0;
    m_buffer.clear();
    m_loadIsInProgress = false;
}

}
