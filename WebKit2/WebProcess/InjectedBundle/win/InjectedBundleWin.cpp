/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "InjectedBundle.h"

#include "WKBundleAPICast.h"
#include "WKBundleInitialize.h"

#include <windows.h>
#include <winbase.h>
#include <shlobj.h>
#include <shlwapi.h>

using namespace WebCore;

namespace WebKit {

// FIXME: This should try and use <WebCore/FileSystem.h>.

static String pathGetFileName(const String& path)
{
    return String(::PathFindFileName(String(path).charactersWithNullTermination()));
}

static String directoryName(const String& path)
{
    String fileName = pathGetFileName(path);
    String dirName = String(path);
    dirName.truncate(dirName.length() - pathGetFileName(path).length());
    return dirName;
}

bool InjectedBundle::load()
{
    WCHAR currentPath[MAX_PATH];
    if (!::GetCurrentDirectoryW(MAX_PATH, currentPath))
        return false;

    String directorBundleResidesIn = directoryName(m_path);
    if (!::SetCurrentDirectoryW(directorBundleResidesIn.charactersWithNullTermination()))
        return false;

    m_platformBundle = ::LoadLibraryExW(m_path.charactersWithNullTermination(), 0, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!m_platformBundle)
        return false;

    // Reset the current directory.
    if (!::SetCurrentDirectoryW(currentPath)) {
        return false;
    }

    WKBundleInitializeFunctionPtr initializeFunction = reinterpret_cast<WKBundleInitializeFunctionPtr>(::GetProcAddress(m_platformBundle, "WKBundleInitialize"));
    if (!initializeFunction)
        return false;

    initializeFunction(toRef(this));
    return true;
}

} // namespace WebKit
