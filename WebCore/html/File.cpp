/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#include "config.h"
#include "File.h"

#include "FileSystem.h"
#include "MIMETypeRegistry.h"

namespace WebCore {

File::File(const String& path)
    : Blob(path)
{
    Init();
}

#if ENABLE(DIRECTORY_UPLOAD)
File::File(const String& relativePath, const String& filePath)
    : Blob(FileBlobItem::create(filePath, relativePath))
{
    Init();
}
#endif

void File::Init()
{
    // We don't use MIMETypeRegistry::getMIMETypeForPath() because it returns "application/octet-stream" upon failure.
    const String& fileName = name();
    int index = fileName.reverseFind('.');
    if (index != -1)
        m_type = MIMETypeRegistry::getMIMETypeForExtension(fileName.substring(index + 1));
}

const String& File::name() const
{
    return items().at(0)->toFileBlobItem()->name();
}

#if ENABLE(DIRECTORY_UPLOAD)
const String& File::webkitRelativePath() const
{
    return items().at(0)->toFileBlobItem()->relativePath();
}
#endif

} // namespace WebCore
