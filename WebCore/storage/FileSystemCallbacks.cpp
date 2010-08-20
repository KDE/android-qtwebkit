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

#include "config.h"
#include "FileSystemCallbacks.h"

#if ENABLE(FILE_SYSTEM)

#include "DOMFileSystem.h"
#include "DirectoryEntry.h"
#include "EntriesCallback.h"
#include "EntryArray.h"
#include "EntryCallback.h"
#include "ErrorCallback.h"
#include "ExceptionCode.h"
#include "FileEntry.h"
#include "FileError.h"
#include "FileSystemCallback.h"
#include "Metadata.h"
#include "MetadataCallback.h"
#include "ScriptExecutionContext.h"
#include "VoidCallback.h"

namespace WebCore {

FileSystemCallbacksBase::FileSystemCallbacksBase(PassRefPtr<ErrorCallback> errorCallback)
    : m_errorCallback(errorCallback)
{
}

FileSystemCallbacksBase::~FileSystemCallbacksBase()
{
}

void FileSystemCallbacksBase::didSucceed()
{
    // Each subclass must implement an appropriate one.
    ASSERT_NOT_REACHED();
}

void FileSystemCallbacksBase::didOpenFileSystem(const String&, const String&)
{
    // Each subclass must implement an appropriate one.
    ASSERT_NOT_REACHED();
}

void FileSystemCallbacksBase::didReadMetadata(double)
{
    // Each subclass must implement an appropriate one.
    ASSERT_NOT_REACHED();
}

void FileSystemCallbacksBase::didReadDirectoryChunkDone(bool)
{
    // Each subclass must implement an appropriate one.
    ASSERT_NOT_REACHED();
}

void FileSystemCallbacksBase::didReadDirectoryEntry(const String&, bool)
{
    // Each subclass must implement an appropriate one.
    ASSERT_NOT_REACHED();
}

void FileSystemCallbacksBase::didFail(ExceptionCode code)
{
    if (m_errorCallback) {
        m_errorCallback->handleEvent(FileError::create(code).get());
        m_errorCallback.clear();
    }
}

// EntryCallbacks -------------------------------------------------------------

EntryCallbacks::EntryCallbacks(PassRefPtr<EntryCallback> successCallback, PassRefPtr<ErrorCallback> errorCallback, DOMFileSystem* fileSystem, const String& expectedPath, bool isDirectory)
    : FileSystemCallbacksBase(errorCallback)
    , m_successCallback(successCallback)
    , m_fileSystem(fileSystem)
    , m_expectedPath(expectedPath)
    , m_isDirectory(isDirectory)
{
}

void EntryCallbacks::didSucceed()
{
    if (m_successCallback) {
        if (m_isDirectory)
            m_successCallback->handleEvent(DirectoryEntry::create(m_fileSystem, m_expectedPath).get());
        else
            m_successCallback->handleEvent(FileEntry::create(m_fileSystem, m_expectedPath).get());
    }
    m_successCallback.clear();
}

// EntriesCallbacks -----------------------------------------------------------

EntriesCallbacks::EntriesCallbacks(PassRefPtr<EntriesCallback> successCallback, PassRefPtr<ErrorCallback> errorCallback, DOMFileSystem* fileSystem, const String& basePath)
    : FileSystemCallbacksBase(errorCallback)
    , m_successCallback(successCallback)
    , m_fileSystem(fileSystem)
    , m_basePath(basePath)
{
}

void EntriesCallbacks::didReadDirectoryEntry(const String& name, bool isDirectory)
{
    if (isDirectory)
        m_entries->append(DirectoryEntry::create(m_fileSystem, m_basePath + "/" + name));
    else
        m_entries->append(FileEntry::create(m_fileSystem, m_basePath + "/" + name));
}

void EntriesCallbacks::didReadDirectoryChunkDone(bool hasMore)
{
    if (m_successCallback) {
        m_successCallback->handleEvent(m_entries.get());
        m_entries->clear();
        if (!hasMore) {
            // If there're no more entries, call back once more with an empty array.
            m_successCallback->handleEvent(m_entries.get());
            m_successCallback.clear();
        }
    }
}

// FileSystemCallbacks --------------------------------------------------------

FileSystemCallbacks::FileSystemCallbacks(PassRefPtr<FileSystemCallback> successCallback, PassRefPtr<ErrorCallback> errorCallback)
    : FileSystemCallbacksBase(errorCallback)
    , m_successCallback(successCallback)
{
}

void FileSystemCallbacks::didOpenFileSystem(const String& name, const String& rootPath)
{
    if (m_successCallback)
        m_successCallback->handleEvent(DOMFileSystem::create(name, rootPath).get());
    m_successCallback.clear();
}

// MetadataCallbacks ----------------------------------------------------------

MetadataCallbacks::MetadataCallbacks(PassRefPtr<MetadataCallback> successCallback, PassRefPtr<ErrorCallback> errorCallback)
    : FileSystemCallbacksBase(errorCallback)
    , m_successCallback(successCallback)
{
}

void MetadataCallbacks::didReadMetadata(double modificationTime)
{
    if (m_successCallback)
        m_successCallback->handleEvent(Metadata::create(modificationTime).get());
    m_successCallback.clear();
}

// VoidCallbacks --------------------------------------------------------------

VoidCallbacks::VoidCallbacks(PassRefPtr<VoidCallback> successCallback, PassRefPtr<ErrorCallback> errorCallback)
    : FileSystemCallbacksBase(errorCallback)
    , m_successCallback(successCallback)
{
}

void VoidCallbacks::didSucceed()
{
    if (m_successCallback)
        m_successCallback->handleEvent();
    m_successCallback.clear();
}

} // namespace

#endif // ENABLE(FILE_SYSTEM)
