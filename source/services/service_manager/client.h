/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Client class.
 */

#pragma once

#include <kiwi/core/connection.h>
#include <kiwi/core/event_loop.h>

#include <list>

class Service;

/** Represents a client connection. */
class Client {
public:
    Client(Kiwi::Core::Connection connection, process_id_t processId);
    ~Client();

    Service *service() const { return m_service; }

    void setService(Service *service) { m_service = service; }

    void finishConnect(Service *service, Kiwi::Core::Message &reply);

private:
    void handleHangupEvent();
    void handleMessageEvent();

    void sendReply(Kiwi::Core::Message &reply);

    void handleConnect(Kiwi::Core::Message &request);
    void handleRegisterPort(Kiwi::Core::Message &request);
    void handleGetProcess(Kiwi::Core::Message &request);

private:
    Kiwi::Core::Connection m_connection;
    process_id_t m_processId;
    Service *m_service;
    std::list<Service *> m_pendingConnects;

    Kiwi::Core::EventRef m_hangupEvent;
    Kiwi::Core::EventRef m_messageEvent;
};
