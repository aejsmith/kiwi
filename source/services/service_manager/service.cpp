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
 * @brief               Service class.
 */

#include "client.h"
#include "service_manager.h"
#include "service.h"

#include <core/log.h>

#include <kernel/process.h>
#include <kernel/status.h>

#include <assert.h>
#include <inttypes.h>

Service::Service(std::string name, std::string path, uint32_t flags) :
    m_name      (std::move(name)),
    m_path      (std::move(path)),
    m_flags     (flags),
    m_process   (INVALID_HANDLE),
    m_processId (-1),
    m_client    (nullptr),
    m_port      (INVALID_HANDLE)
{}

Service::~Service() {
    if (m_process != INVALID_HANDLE)
        kern_handle_close(m_process);

    if (m_port != INVALID_HANDLE)
        kern_handle_close(m_port);
}

/** Set the port for the service, failing if already set.
 * @return              Whether successful. */
bool Service::setPort(handle_t port) {
    if (m_port == INVALID_HANDLE) {
        m_port = port;

        /* Reply to pending connections. */
        while (!m_pendingConnects.empty()) {
            PendingConnect &connect = m_pendingConnects.front();
            connect.client->finishConnect(this, connect.reply);
            m_pendingConnects.pop_front();
        }

        return true;
    } else {
        return false;
    }
}

void Service::addPendingConnect(Client *client, core_message_t *reply) {
    PendingConnect &connect = m_pendingConnects.emplace_back();

    connect.client = client;
    connect.reply  = reply;
}

void Service::removePendingConnects(Client *client) {
    for (auto it = m_pendingConnects.begin(); it != m_pendingConnects.end(); ) {
        if (it->client == client) {
            /* We have to destroy this reply since the client won't do it. */
            core_message_destroy(it->reply);
            it = m_pendingConnects.erase(it);
        } else {
            ++it;
        }
    }
}

/** Start the service if it is not already running. */
bool Service::start() {
    status_t ret;

    if (m_process != INVALID_HANDLE) {
        ret = kern_process_status(m_process, nullptr, nullptr);
        if (ret != STATUS_STILL_RUNNING)
            handleDeath();
    }

    if (m_process == INVALID_HANDLE) {
        ret = g_serviceManager.spawnProcess(m_path.c_str(), &m_process);
        if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_WARN, "failed to start service '%s': %d", m_name.c_str(), ret);
            return false;
        }

        ret = kern_process_id(m_process, &m_processId);
        assert(ret == STATUS_SUCCESS);

        g_serviceManager.addEvent(m_process, PROCESS_EVENT_DEATH, this);
    }

    return true;
}

void Service::handleEvent(const object_event_t &event) {
    assert(event.handle == m_process);
    assert(event.event == PROCESS_EVENT_DEATH);

    handleDeath();
}

void Service::handleDeath() {
    core_log(CORE_LOG_WARN, "service '%s' terminated unexpectedly", m_name.c_str());

    g_serviceManager.removeEvents(this);

    kern_handle_close(m_process);
    m_process   = INVALID_HANDLE;
    m_processId = -1;

    if (m_port != INVALID_HANDLE) {
        kern_handle_close(m_port);
        m_port = INVALID_HANDLE;
    }

    if (m_client) {
        /* If the client is still set, then we haven't handled the connection
         * hangup event yet, we'll leave it to that to destroy the client. */
        m_client->setService(nullptr);
        m_client = nullptr;
    }
}
