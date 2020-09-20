/*
 * Copyright (C) 2009-2020 Alex Smith
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
 *
 * TODO:
 *  - Shouldn't block when sending messages if the remote message queue is full
 *    as this could be used for denial of service by blocking the service
 *    manager. We need a core_connection function that can do an asynchronous
 *    send driven by an event for space becoming available, and drop messages
 *    if we can't send them in a set amount of time to prevent us from piling
 *    up unsent messages.
 */

#include "client.h"
#include "protocol.h"
#include "service_manager.h"
#include "service.h"

#include <core/log.h>
#include <core/utility.h>

#include <kernel/status.h>

#include <assert.h>
#include <inttypes.h>

Client::Client(core_connection_t *connection, process_id_t processId) :
    m_connection (connection),
    m_processId  (processId),
    m_service    (nullptr)
{
    handle_t handle = core_connection_get_handle(m_connection);
    g_serviceManager.addEvent(handle, CONNECTION_EVENT_HANGUP, this);
    g_serviceManager.addEvent(handle, CONNECTION_EVENT_MESSAGE, this);
}

Client::~Client() {
    if (m_service)
        m_service->setClient(nullptr);

    for (Service *service : m_pendingConnects)
        service->removePendingConnects(this);

    g_serviceManager.removeEvents(this);
    core_connection_close(m_connection);
}

void Client::handleEvent(const object_event_t *event) {
    assert(event->handle == core_connection_get_handle(m_connection));

    switch (event->event) {
        case CONNECTION_EVENT_HANGUP:
            delete this;
            break;

        case CONNECTION_EVENT_MESSAGE:
            handleMessage();
            break;

        default:
            core_unreachable();
            break;
    }
}

void Client::handleMessage() {
    core_message_t *message;
    status_t ret = core_connection_receive(m_connection, 0, &message);
    if (ret != STATUS_SUCCESS)
        return;

    assert(core_message_get_type(message) == CORE_MESSAGE_REQUEST);

    uint32_t id = core_message_get_id(message);
    switch (id) {
        case SERVICE_MANAGER_REQUEST_CONNECT:
            handleConnect(message);
            break;
        case SERVICE_MANAGER_REQUEST_REGISTER_PORT:
            handleRegisterPort(message);
            break;
        default:
            core_log(
                CORE_LOG_NOTICE, "received unrecognised message type %" PRId32 " from client %" PRId32,
                id, m_processId);
            break;
    }

    core_message_destroy(message);
}

void Client::handleConnect(core_message_t *request) {
    core_message_t *reply = core_message_create_reply(request, sizeof(service_manager_reply_connect_t));
    if (!reply) {
        core_log(CORE_LOG_WARN, "failed to allocate reply message");
        return;
    }

    auto replyData = reinterpret_cast<service_manager_reply_connect_t *>(core_message_get_data(reply));

    size_t requestSize = core_message_get_size(request);

    Service *service = nullptr;
    bool canReply    = true;

    if (requestSize <= sizeof(service_manager_request_connect_t)) {
        replyData->result = STATUS_INVALID_ARG;
    } else {
        auto requestData = reinterpret_cast<service_manager_request_connect_t *>(core_message_get_data(request));

        size_t nameSize = requestSize - sizeof(service_manager_request_connect_t);
        requestData->name[nameSize - 1] = 0;

        service = g_serviceManager.findService(requestData->name);
        if (service) {
            replyData->result = STATUS_SUCCESS;

            /* Ensure the service is started. */
            service->start();

            /* The port may not have been registered yet, especially if we've
             * just started it. We wait until the port is registered before
             * replying. */
            canReply = service->port() != INVALID_HANDLE;
            if (!canReply) {
                service->addPendingConnect(this, reply);
                m_pendingConnects.emplace_back(service);
            }
        } else {
            replyData->result = STATUS_NOT_FOUND;
        }
    }

    /* Reply immediately if we failed or the service port is already available. */
    if (canReply)
        finishConnect(service, reply);
}

void Client::finishConnect(Service *service, core_message_t *reply) {
    if (service) {
        assert(service->port() != INVALID_HANDLE);
        core_message_attach_handle(reply, service->port(), false);

        m_pendingConnects.remove(service);
    }

    status_t ret = core_connection_reply(m_connection, reply);
    if (ret != STATUS_SUCCESS)
        core_log(CORE_LOG_WARN, "failed to send reply message: %" PRId32, ret);

    core_message_destroy(reply);
}

void Client::handleRegisterPort(core_message_t *request) {
    core_message_t *reply = core_message_create_reply(request, sizeof(service_manager_reply_register_port_t));
    if (!reply) {
        core_log(CORE_LOG_WARN, "failed to allocate reply message");
        return;
    }

    auto replyData = reinterpret_cast<service_manager_reply_register_port_t *>(core_message_get_data(reply));

    if (m_service) {
        handle_t handle = core_message_detach_handle(request);

        if (handle != INVALID_HANDLE) {
            if (m_service->setPort(handle)) {
                replyData->result = STATUS_SUCCESS;
            } else {
                kern_handle_close(handle);
            }
        } else {
            replyData->result = STATUS_INVALID_ARG;
        }
    } else {
        replyData->result = STATUS_INVALID_REQUEST;
    }

    status_t ret = core_connection_reply(m_connection, reply);
    if (ret != STATUS_SUCCESS)
        core_log(CORE_LOG_WARN, "failed to send reply message: %" PRId32, ret);

    core_message_destroy(reply);
}
