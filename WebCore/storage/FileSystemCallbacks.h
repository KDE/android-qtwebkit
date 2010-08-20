/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef FileSystemCallbacks_h
#define FileSystemCallbacks_h

#if ENABLE(FILE_SYSTEM)

#include "PlatformString.h"
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class DOMFileSystem;
class ErrorCallback;
class EntriesCallback;
class EntryArray;
class EntryCallback;
class FileSystemCallback;
class MetadataCallback;
class ScriptExecutionContext;
class VoidCallback;

typedef int ExceptionCode;

// A base class for FileSystem callbacks that bundles successCallback, errorCallback and some closure data for the callbacks.
class FileSystemCallbacksBase : public Noncopyable {
public:
    virtual ~FileSystemCallbacksBase();

    // For EntryCallbacks and VoidCallbacks.
    virtual void didSucceed();

    // For FileSystemCallbacks.
    virtual void didOpenFileSystem(const String& name, const String& rootPath);

    // For MetadataCallbacks.
    virtual void didReadMetadata(double modificationTime);

    // For EntriesCallbacks. didReadDirectoryEntry is called each time the API reads an entry, and didReadDirectoryDone is called when a chunk of entries have been read (i.e. good time to call back to the application).  If hasMore is true there can be more chunks.
    virtual void didReadDirectoryEntry(const String& name, bool isDirectory);
    virtual void didReadDirectoryChunkDone(bool hasMore);

    // For ErrorCallback.
    virtual void didFail(ExceptionCode code);

protected:
    FileSystemCallbacksBase(PassRefPtr<ErrorCallback> errorCallback);
    RefPtr<ErrorCallback> m_errorCallback;
};

// Subclasses ----------------------------------------------------------------

class EntryCallbacks : public FileSystemCallbacksBase {
public:
    EntryCallbacks(PassRefPtr<EntryCallback>, PassRefPtr<ErrorCallback>, DOMFileSystem*, const String& expectedPath, bool isDirectory);
    virtual void didSucceed();

private:
    RefPtr<EntryCallback> m_successCallback;
    DOMFileSystem* m_fileSystem;
    String m_expectedPath;
    bool m_isDirectory;
};

class EntriesCallbacks : public FileSystemCallbacksBase {
public:
    EntriesCallbacks(PassRefPtr<EntriesCallback>, PassRefPtr<ErrorCallback>, DOMFileSystem*, const String& basePath);
    virtual void didReadDirectoryEntry(const String& name, bool isDirectory);
    virtual void didReadDirectoryChunkDone(bool hasMore);

private:
    RefPtr<EntriesCallback> m_successCallback;
    DOMFileSystem* m_fileSystem;
    String m_basePath;
    RefPtr<EntryArray> m_entries;
};

class FileSystemCallbacks : public FileSystemCallbacksBase {
public:
    FileSystemCallbacks(PassRefPtr<FileSystemCallback>, PassRefPtr<ErrorCallback>);
    virtual void didOpenFileSystem(const String& name, const String& rootPath);

private:
    RefPtr<FileSystemCallback> m_successCallback;
};

class MetadataCallbacks : public FileSystemCallbacksBase {
public:
    MetadataCallbacks(PassRefPtr<MetadataCallback>, PassRefPtr<ErrorCallback>);
    virtual void didReadMetadata(double modificationTime);

private:
    RefPtr<MetadataCallback> m_successCallback;
};

class VoidCallbacks : public FileSystemCallbacksBase {
public:
    VoidCallbacks(PassRefPtr<VoidCallback>, PassRefPtr<ErrorCallback>);
    virtual void didSucceed();

private:
    RefPtr<VoidCallback> m_successCallback;
};

} // namespace

#endif // ENABLE(FILE_SYSTEM)

#endif // FileSystemCallbacks_h
