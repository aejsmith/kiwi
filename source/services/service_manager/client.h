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

#include "event_handler.h"

#include <core/ipc.h>

#include <list>

class Service;

/** Represents a client connection. */
class Client final : public EventHandler {
public:
    Client(core_connection_t *connection, process_id_t processId);
    ~Client();

    Service *service() const { return m_service; }

    void setService(Service *service) { m_service = service; }

    void handleEvent(const object_event_t &event) override;

    void finishConnect(Service *service, core_message_t *reply);

private:
    void handleMessage();
    void handleConnect(core_message_t *request);
    void handleRegisterPort(core_message_t *request);

private:
    core_connection_t *m_connection;
    process_id_t m_processId;
    Service *m_service;
    std::list<Service *> m_pendingConnects;
};
