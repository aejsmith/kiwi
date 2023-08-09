/*
 * Copyright (C) 2009-2023 Alex Smith
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
#include "service_manager.h"
#include "service.h"

#include <core/log.h>
#include <core/utility.h>

#include <kernel/status.h>

#include <services/service_manager.h>

#include <assert.h>

Client::Client(Kiwi::Core::Connection connection, process_id_t processId) :
    m_connection (std::move(connection)),
    m_processId  (processId),
    m_service    (nullptr)
{
    handle_t handle = m_connection.handle();

    m_hangupEvent = g_serviceManager.eventLoop().addEvent(
        handle, CONNECTION_EVENT_HANGUP, 0,
        [this] (const object_event_t &event) { handleHangupEvent(); });
    m_messageEvent = g_serviceManager.eventLoop().addEvent(
        handle, CONNECTION_EVENT_MESSAGE, 0,
        [this] (const object_event_t &event) { handleMessageEvent(); });
}

Client::~Client() {
    if (m_service)
        m_service->setClient(nullptr);

    for (Service *service : m_pendingConnects)
        service->removePendingConnects(this);
}

void Client::handleHangupEvent() {
    delete this;
}

void Client::handleMessageEvent() {
    Kiwi::Core::Message message;
    status_t ret = m_connection.receive(0, message);
    if (ret != STATUS_SUCCESS)
        return;

    assert(message.type() == Kiwi::Core::Message::kRequest);

    uint32_t id = message.id();
    switch (id) {
        case SERVICE_MANAGER_REQUEST_CONNECT:
            handleConnect(message);
            break;
        case SERVICE_MANAGER_REQUEST_REGISTER_PORT:
            handleRegisterPort(message);
            break;
        case SERVICE_MANAGER_REQUEST_GET_PROCESS:
            handleGetProcess(message);
            break;
        default:
            core_log(
                CORE_LOG_NOTICE, "received unrecognised message type %" PRId32 " from client %" PRId32,
                id, m_processId);
            break;
    }
}

static inline bool createReply(Kiwi::Core::Message &reply, const Kiwi::Core::Message &request, size_t size) {
    if (!reply.createReply(request, size)) {
        core_log(CORE_LOG_WARN, "failed to allocate reply message");
        return false;
    }

    return true;
}

void Client::sendReply(Kiwi::Core::Message &reply) {
    status_t ret = m_connection.reply(reply);
    if (ret != STATUS_SUCCESS)
        core_log(CORE_LOG_WARN, "failed to send reply message: %" PRId32, ret);
}

void Client::handleConnect(Kiwi::Core::Message &request) {
    Kiwi::Core::Message reply;
    if (!createReply(reply, request, sizeof(service_manager_reply_connect_t)))
        return;

    auto replyData = reply.data<service_manager_reply_connect_t>();

    size_t requestSize = request.size();

    Service *service = nullptr;
    bool canReply    = true;

    if (requestSize <= sizeof(service_manager_request_connect_t)) {
        replyData->result = STATUS_INVALID_ARG;
    } else {
        auto requestData = request.data<service_manager_request_connect_t>();

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
                service->addPendingConnect(this, std::move(reply));
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

void Client::finishConnect(Service *service, Kiwi::Core::Message &reply) {
    if (service) {
        assert(service->port() != INVALID_HANDLE);
        reply.attachHandle(service->port(), false);

        m_pendingConnects.remove(service);
    }

    sendReply(reply);
}

void Client::handleRegisterPort(Kiwi::Core::Message &request) {
    Kiwi::Core::Message reply;
    if (!createReply(reply, request, sizeof(service_manager_reply_register_port_t)))
        return;

    auto replyData = reply.data<service_manager_reply_register_port_t>();

    if (m_service) {
        Kiwi::Core::Handle port = request.detachHandle();

        if (port.isValid()) {
            if (m_service->setPort(std::move(port))) {
                replyData->result = STATUS_SUCCESS;
            } else {
                replyData->result = STATUS_ALREADY_EXISTS;
            }
        } else {
            replyData->result = STATUS_INVALID_ARG;
        }
    } else {
        replyData->result = STATUS_INVALID_REQUEST;
    }

    sendReply(reply);
}

void Client::handleGetProcess(Kiwi::Core::Message &request) {
    Kiwi::Core::Message reply;
    if (!createReply(reply, request, sizeof(service_manager_reply_get_process_t)))
        return;

    auto replyData     = reply.data<service_manager_reply_get_process_t>();
    size_t requestSize = request.size();
    Service *service   = nullptr;

    if (requestSize <= sizeof(service_manager_request_get_process_t)) {
        replyData->result = STATUS_INVALID_ARG;
    } else {
        auto requestData = request.data<service_manager_request_get_process_t>();

        size_t nameSize = requestSize - sizeof(service_manager_request_get_process_t);
        requestData->name[nameSize - 1] = 0;

        service = g_serviceManager.findService(requestData->name);
        if (service) {
            replyData->result = STATUS_NOT_RUNNING;

            Client *client = service->client();
            if (client && client->m_connection.isValid()) {
                Kiwi::Core::Handle process;
                status_t ret = kern_connection_open_remote(client->m_connection.handle(), process.attach());
                if (ret == STATUS_SUCCESS) {
                    reply.attachHandle(std::move(process));
                    replyData->result = STATUS_SUCCESS;
                }
            }
        } else {
            replyData->result = STATUS_NOT_FOUND;
        }
    }

    sendReply(reply);
}
