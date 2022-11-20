/*
 * Copyright (C) 2009-2022 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
