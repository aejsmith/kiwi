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
 * @brief               Service manager.
 */

#include "client.h"
#include "service_manager.h"
#include "service.h"

#include <core/log.h>
#include <core/utility.h>

#include <kernel/ipc.h>
#include <kernel/process.h>
#include <kernel/status.h>

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>

extern const char *const *environ;

ServiceManager g_serviceManager;

ServiceManager::ServiceManager() :
    m_port (INVALID_HANDLE)
{}

ServiceManager::~ServiceManager() {
    if (m_port != INVALID_HANDLE)
        kern_handle_close(m_port);
}

int ServiceManager::run() {
    status_t ret;

    /* Set default environment variables. TODO: Not appropriate for a per-
     * session service manager instance. */
    setenv("PATH", "/system/bin", 1);
    setenv("HOME", "/users/admin", 1);

    core_log(CORE_LOG_NOTICE, "service manager started");

    ret = kern_port_create(&m_port);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to create port: %d", ret);
        return EXIT_FAILURE;
    }

    addEvent(m_port, PORT_EVENT_CONNECTION, this);

    /* TODO: Service configuration. */
    addService("org.kiwi.test", "/system/services/test", Service::kIpc | Service::kOnDemand);
    addService("org.kiwi.terminal", "/system/services/terminal_service", Service::kIpc | Service::kOnDemand);

    spawnProcess("/system/bin/terminal");

    while (true) {
        ret = kern_object_wait(m_events.data(), m_events.size(), 0, -1);
        if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_WARN, "failed to wait for events: %d", ret);
            continue;
        }

        size_t numEvents = m_events.size();

        for (size_t i = 0; i < numEvents; ) {
            object_event_t &event = m_events[i];

            uint32_t flags = event.flags;
            event.flags &= ~(OBJECT_EVENT_SIGNALLED | OBJECT_EVENT_ERROR);

            if (flags & OBJECT_EVENT_ERROR) {
                core_log(CORE_LOG_WARN, "error flagged on event %u for handle %u", event.event, event.handle);
            } else if (flags & OBJECT_EVENT_SIGNALLED) {
                auto handler = reinterpret_cast<EventHandler *>(event.udata);
                handler->handleEvent(&event);
            }

            /* Calling the handler may change the event array, so we have to
             * handle this - start from the beginning. */
            if (numEvents != m_events.size()) {
                numEvents = m_events.size();
                i = 0;
            } else {
                i++;
            }
        }
    }
}

void ServiceManager::addService(std::string name, std::string path, uint32_t flags) {
    auto service = std::make_unique<Service>(std::move(name), std::move(path), flags);

    if (!(flags & Service::kOnDemand))
        service->start();

    m_services.emplace(service->name(), std::move(service));
}

Service* ServiceManager::findService(const std::string &name) {
    auto ret = m_services.find(name);
    if (ret != m_services.end()) {
        return ret->second.get();
    } else {
        return nullptr;
    }
}

status_t ServiceManager::spawnProcess(const char *path, handle_t *_handle) const {
    process_attrib_t attrib;
    handle_t map[][2] = { { 0, 0 }, { 1, 1 }, { 2, 2 } };
    attrib.token     = INVALID_HANDLE;
    attrib.root_port = m_port;
    attrib.map       = map;
    attrib.map_count = core_array_size(map);

    const char *args[] = { path, nullptr };
    status_t ret = kern_process_create(path, args, environ, 0, &attrib, _handle);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to create process '%s': %d", path, ret);
        return ret;
    }

    return ret;
}

void ServiceManager::handleEvent(const object_event_t *event) {
    status_t ret;

    assert(event->handle == m_port);
    assert(event->event == PORT_EVENT_CONNECTION);

    handle_t handle;
    ipc_client_t ipcClient;
    ret = kern_port_listen(m_port, &ipcClient, 0, &handle);
    if (ret != STATUS_SUCCESS) {
        /* This may be harmless - client's connection attempt could be cancelled
         * between us receiving the event and calling listen, for instance. */
        core_log(CORE_LOG_WARN, "failed to listen on port after connection event: %d", ret);
        return;
    }

    core_connection_t *connection = core_connection_create(handle, CORE_CONNECTION_RECEIVE_REQUESTS);
    if (!connection) {
        core_log(CORE_LOG_WARN, "failed to create connection");
        kern_handle_close(handle);
        return;
    }

    Client* client = new Client(connection, ipcClient.pid);

    /* See if this client matches one of our services. */
    for (const auto &it : m_services) {
        Service *service = it.second.get();

        if (service->processId() == ipcClient.pid) {
            service->setClient(client);
            client->setService(service);
        }
    }
}

void ServiceManager::addEvent(handle_t handle, unsigned id, EventHandler *handler) {
    object_event_t &event = m_events.emplace_back();

    event.handle = handle;
    event.event  = id;
    event.flags  = 0;
    event.data   = 0;
    event.udata  = handler;
}

void ServiceManager::removeEvents(EventHandler *handler) {
    for (auto it = m_events.begin(); it != m_events.end(); ) {
        if (reinterpret_cast<EventHandler *>(it->udata) == handler) {
            it = m_events.erase(it);
        } else {
            ++it;
        }
    }
}

int main(int argc, char **argv) {
    return g_serviceManager.run();
}
