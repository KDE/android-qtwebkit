/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "IDBTransactionBackendImpl.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBDatabaseBackendImpl.h"
#include "IDBTransactionCoordinator.h"
#include "SQLiteDatabase.h"

namespace WebCore {

PassRefPtr<IDBTransactionBackendImpl> IDBTransactionBackendImpl::create(DOMStringList* objectStores, unsigned short mode, unsigned long timeout, int id, IDBDatabaseBackendImpl* database)
{
    return adoptRef(new IDBTransactionBackendImpl(objectStores, mode, timeout, id, database));
}

IDBTransactionBackendImpl::IDBTransactionBackendImpl(DOMStringList* objectStores, unsigned short mode, unsigned long timeout, int id, IDBDatabaseBackendImpl* database)
    : m_objectStoreNames(objectStores)
    , m_mode(mode)
    , m_timeout(timeout)
    , m_id(id)
    , m_state(NotStarted)
    , m_database(database)
    , m_transaction(new SQLiteTransaction(database->sqliteDatabase()))
    , m_timer(this, &IDBTransactionBackendImpl::timerFired)
    , m_pendingEvents(0)
{
}

PassRefPtr<IDBObjectStoreBackendInterface> IDBTransactionBackendImpl::objectStore(const String& name)
{
    if (isFinished())
        return 0;
    return m_database->objectStore(name, 0); // FIXME: remove mode param.
}

bool IDBTransactionBackendImpl::scheduleTask(PassOwnPtr<ScriptExecutionContext::Task> task)
{
    if (isFinished())
        return false;

    m_taskQueue.append(task);
    if (m_state == NotStarted)
        start();

    return true;
}

void IDBTransactionBackendImpl::abort()
{
    if (isFinished())
        return;

    m_state = Finished;
    m_transaction->rollback();
    m_callbacks->onAbort();
    m_database->transactionCoordinator()->didFinishTransaction(this);
}

void IDBTransactionBackendImpl::didCompleteTaskEvents()
{
    ASSERT(m_state == Started);
    ASSERT(m_pendingEvents);

    m_pendingEvents--;

    if (!m_pendingEvents && m_taskQueue.isEmpty()) {
        // The last task event has completed and the task
        // queue is empty. Commit the transaction.
        commit();
        return;
    }

    // We are still waiting for other events to complete. However,
    // the task queue is non-empty and the timer is inactive.
    // We can therfore schedule the timer again.
    if (!m_taskQueue.isEmpty() && !m_timer.isActive())
        m_timer.startOneShot(0);
}

void IDBTransactionBackendImpl::run()
{
    ASSERT(m_state == Started);
    ASSERT(!m_timer.isActive());

    m_timer.startOneShot(0);
}

void IDBTransactionBackendImpl::start()
{
    ASSERT(m_state == NotStarted);

    m_state = Started;
    m_transaction->begin();
    m_database->transactionCoordinator()->didStartTransaction(this);
}

void IDBTransactionBackendImpl::commit()
{
    ASSERT(m_state == Started);

    m_state = Finished;
    m_transaction->commit();
    m_database->transactionCoordinator()->didFinishTransaction(this);
}

void IDBTransactionBackendImpl::timerFired(Timer<IDBTransactionBackendImpl>*)
{
    ASSERT(!m_taskQueue.isEmpty());
    ASSERT(m_state == Started);

    TaskQueue queue;
    queue.swap(m_taskQueue);
    while (!queue.isEmpty()) {
        OwnPtr<ScriptExecutionContext::Task> task(queue.first().release());
        queue.removeFirst();
        m_pendingEvents++;
        task->performTask(0);
    }
}

};

#endif // ENABLE(INDEXED_DATABASE)
