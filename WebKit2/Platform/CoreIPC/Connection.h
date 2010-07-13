/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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

#ifndef Connection_h
#define Connection_h

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"
#include "Arguments.h"
#include "MessageID.h"
#include "WorkQueue.h"
#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>
#include <wtf/OwnPtr.h>
#include <wtf/Threading.h>

#if PLATFORM(MAC)
#include <mach/mach_port.h>
#elif PLATFORM(WIN)
#include <string>
#elif PLATFORM(QT)
#include <QString>
class QLocalServer;
class QLocalSocket;
#endif

class RunLoop;

namespace CoreIPC {

class MessageID;
    
class Connection : public ThreadSafeShared<Connection> {
public:
    class Client {
    public:
        virtual ~Client() { }
        
        virtual void didReceiveMessage(Connection*, MessageID, ArgumentDecoder*) = 0;
        virtual void didReceiveSyncMessage(Connection*, MessageID, ArgumentDecoder*, ArgumentEncoder*) = 0;
        virtual void didClose(Connection*) = 0;
    };

#if PLATFORM(MAC)
    typedef mach_port_t Identifier;
#elif PLATFORM(WIN)
    typedef HANDLE Identifier;
    static bool createServerAndClientIdentifiers(Identifier& serverIdentifier, Identifier& clientIdentifier);
#elif PLATFORM(QT)
    typedef const QString Identifier;
#endif

    static PassRefPtr<Connection> createServerConnection(Identifier, Client*, RunLoop* clientRunLoop);
    static PassRefPtr<Connection> createClientConnection(Identifier, Client*, RunLoop* clientRunLoop);
    ~Connection();

    bool open();
    void invalidate();

    template<typename E, typename T> bool send(E messageID, uint64_t destinationID, const T& arguments);
    
    static const unsigned long long NoTimeout = 10000000000ULL;
    template<typename E, typename T, typename U> bool sendSync(E messageID, uint64_t destinationID, const T& arguments, const U& reply, double timeout);

    template<typename E> PassOwnPtr<ArgumentDecoder> waitFor(E messageID, uint64_t destinationID, double timeout);

    bool sendMessage(MessageID, PassOwnPtr<ArgumentEncoder>);

private:
    template<typename T> class Message {
    public:
        Message(MessageID messageID, PassOwnPtr<T> arguments)
            : m_messageID(messageID)
            , m_arguments(arguments.leakPtr())
        {
        }
        
        MessageID messageID() const { return m_messageID; }
        T* arguments() const { return m_arguments; }
        
        void destroy() 
        {
            delete m_arguments;
        }
        
    private:
        MessageID m_messageID;
        T* m_arguments;
    };

public:
    typedef Message<ArgumentEncoder> OutgoingMessage;

private:
    Connection(Identifier, bool isServer, Client*, RunLoop* clientRunLoop);
    void platformInitialize(Identifier);
    void platformInvalidate();
    
    bool isValid() const { return m_client; }
    
    PassOwnPtr<ArgumentDecoder> sendSyncMessage(MessageID, uint64_t syncRequestID, PassOwnPtr<ArgumentEncoder>, double timeout);
    PassOwnPtr<ArgumentDecoder> waitForMessage(MessageID, uint64_t destinationID, double timeout);
    
    // Called on the connection work queue.
    void processIncomingMessage(MessageID, PassOwnPtr<ArgumentDecoder>);
    void sendOutgoingMessages();
    void sendOutgoingMessage(MessageID, PassOwnPtr<ArgumentEncoder>);
    void connectionDidClose();
    
    // Called on the listener thread.
    void dispatchConnectionDidClose();
    void dispatchMessages();
    
    Client* m_client;
    bool m_isServer;
    uint64_t m_syncRequestID;

    bool m_isConnected;
    WorkQueue m_connectionQueue;
    RunLoop* m_clientRunLoop;

    // Incoming messages.
    typedef Message<ArgumentDecoder> IncomingMessage;

    Mutex m_incomingMessagesLock;
    Vector<IncomingMessage> m_incomingMessages;

    // Outgoing messages.
    Mutex m_outgoingMessagesLock;
    Vector<OutgoingMessage> m_outgoingMessages;
    
    ThreadCondition m_waitForMessageCondition;
    Mutex m_waitForMessageMutex;
    HashMap<std::pair<unsigned, uint64_t>, ArgumentDecoder*> m_waitForMessageMap;
    
#if PLATFORM(MAC)
    // Called on the connection queue.
    void receiveSourceEventHandler();
    void initializeDeadNameSource();

    mach_port_t m_sendPort;
    mach_port_t m_receivePort;
#elif PLATFORM(WIN)
    // Called on the connection queue.
    void readEventHandler();

    Vector<uint8_t> m_readBuffer;
    OVERLAPPED m_readState;
    HANDLE m_connectionPipe;
#elif PLATFORM(QT)
    // Called on the connection queue.
    void readyReadHandler();

    Vector<uint8_t> m_readBuffer;
    size_t m_currentMessageSize;
    QLocalSocket* m_socket;
    QString m_serverName;
#endif
};

template<typename E, typename T>
bool Connection::send(E messageID, uint64_t destinationID, const T& arguments)
{
    OwnPtr<ArgumentEncoder> argumentEncoder(new ArgumentEncoder(destinationID));
    argumentEncoder->encode(arguments);

    return sendMessage(MessageID(messageID), argumentEncoder.release());
}

template<typename E, typename T, typename U>
inline bool Connection::sendSync(E messageID, uint64_t destinationID, const T& arguments, const U& reply, double timeout)
{
    OwnPtr<ArgumentEncoder> argumentEncoder(new ArgumentEncoder(destinationID));

    uint64_t syncRequestID = ++m_syncRequestID;
    
    // Encode the sync request ID.
    argumentEncoder->encode(syncRequestID);
    
    // Encode the rest of the input arguments.
    argumentEncoder->encode(arguments);
    
    // Now send the message and wait for a reply.
    OwnPtr<ArgumentDecoder> replyDecoder = sendSyncMessage(MessageID(messageID, MessageID::SyncMessage), syncRequestID, argumentEncoder.release(), timeout);
    if (!replyDecoder)
        return false;
    
    // Decode the reply.
    return replyDecoder->decode(const_cast<U&>(reply));
}

template<typename E> inline PassOwnPtr<ArgumentDecoder> Connection::waitFor(E messageID, uint64_t destinationID, double timeout)
{
    return waitForMessage(MessageID(messageID), destinationID, timeout);
}

} // namespace CoreIPC

#endif // Connection_h
