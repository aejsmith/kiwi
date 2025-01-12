/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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

Service::Service(std::string name, std::string path, uint32_t flags) :
    m_name      (std::move(name)),
    m_path      (std::move(path)),
    m_flags     (flags),
    m_processId (-1),
    m_client    (nullptr)
{}

Service::~Service() {}

/** Set the port for the service, failing if already set.
 * @return              Whether successful. */
bool Service::setPort(Kiwi::Core::Handle port) {
    if (!m_port.isValid()) {
        m_port = std::move(port);

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

void Service::addPendingConnect(Client *client, Kiwi::Core::Message reply) {
    PendingConnect &connect = m_pendingConnects.emplace_back();

    connect.client = client;
    connect.reply  = std::move(reply);
}

void Service::removePendingConnects(Client *client) {
    for (auto it = m_pendingConnects.begin(); it != m_pendingConnects.end(); ) {
        if (it->client == client) {
            it = m_pendingConnects.erase(it);
        } else {
            ++it;
        }
    }
}

/** Start the service if it is not already running. */
bool Service::start() {
    status_t ret;

    if (m_process.isValid()) {
        ret = kern_process_status(m_process, nullptr, nullptr);
        if (ret != STATUS_STILL_RUNNING)
            handleDeath();
    }

    if (!m_process.isValid()) {
        ret = g_serviceManager.spawnProcess({ m_path }, &m_process);
        if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_WARN, "failed to start service '%s': %d", m_name.c_str(), ret);
            return false;
        }

        ret = kern_process_id(m_process, &m_processId);
        assert(ret == STATUS_SUCCESS);

        m_deathEvent = g_serviceManager.eventLoop().addEvent(
            m_process, PROCESS_EVENT_DEATH, 0,
            [this] (const object_event_t &event) { handleDeath(); });
    }

    return true;
}

void Service::handleDeath() {
    core_log(CORE_LOG_WARN, "service '%s' terminated unexpectedly", m_name.c_str());

    m_deathEvent.remove();

    m_process.close();
    m_processId = -1;

    m_port.close();

    if (m_client) {
        /* If the client is still set, then we haven't handled the connection
         * hangup event yet, we'll leave it to that to destroy the client. */
        m_client->setService(nullptr);
        m_client = nullptr;
    }
}
