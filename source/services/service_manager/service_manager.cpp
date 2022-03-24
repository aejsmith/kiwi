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
#include <stdlib.h>

extern const char *const *environ;

ServiceManager g_serviceManager;

ServiceManager::ServiceManager() {}

ServiceManager::~ServiceManager() {}

int ServiceManager::run() {
    status_t ret;

    /* Set default environment variables. TODO: Not appropriate for a per-
     * session service manager instance. */
    setenv("PATH", "/system/bin", 1);
    setenv("HOME", "/users/admin", 1);

    core_log(CORE_LOG_NOTICE, "service manager started");

    ret = kern_port_create(m_port.attach());
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to create port: %d", ret);
        return EXIT_FAILURE;
    }

    m_connectionEvent = m_eventLoop.addEvent(
        m_port, PORT_EVENT_CONNECTION, 0,
        [this] (const object_event_t &event) { handleConnectionEvent(); });

    /* TODO: Service configuration. */
    addService("org.kiwi.posix", "/system/services/posix_service", Service::kIpc | Service::kOnDemand);
    addService("org.kiwi.test", "/system/services/test", Service::kIpc | Service::kOnDemand);
    addService("org.kiwi.terminal", "/system/services/terminal_service", Service::kIpc | Service::kOnDemand);

    spawnProcess("/system/bin/terminal");

    while (true) {
        m_eventLoop.wait();
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

status_t ServiceManager::spawnProcess(const char *path, Kiwi::Core::Handle *_handle) const {
    process_attrib_t attrib;
    handle_t map[][2] = { { 0, 0 }, { 1, 1 }, { 2, 2 } };
    attrib.token     = INVALID_HANDLE;
    attrib.root_port = m_port;
    attrib.map       = map;
    attrib.map_count = core_array_size(map);

    const char *args[] = { path, nullptr };
    status_t ret = kern_process_create(
        path, args, environ, 0, &attrib,
        (_handle) ? _handle->attach() : nullptr);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to create process '%s': %d", path, ret);
        return ret;
    }

    return ret;
}

void ServiceManager::handleConnectionEvent() {
    status_t ret;

    Kiwi::Core::Handle handle;
    ret = kern_port_listen(m_port, 0, handle.attach());
    if (ret != STATUS_SUCCESS) {
        /* This may be harmless - client's connection attempt could be cancelled
         * between us receiving the event and calling listen, for instance. */
        if (ret != STATUS_WOULD_BLOCK)
            core_log(CORE_LOG_WARN, "failed to listen on port after connection event: %" PRId32, ret);
        return;
    }

    process_id_t pid;
    {
        Kiwi::Core::Handle process;
        ret = kern_connection_open_remote(handle, process.attach());
        if (ret == STATUS_SUCCESS) {
            ret = kern_process_id(process, &pid);
            if (ret != STATUS_SUCCESS)
                core_log(CORE_LOG_WARN, "failed to get client process ID: %" PRId32, ret);
        } else {
            core_log(CORE_LOG_WARN, "failed to open client process handle: %" PRId32, ret);
        }
    }

    if (ret != STATUS_SUCCESS)
        return;

    Kiwi::Core::Connection connection;
    if (!connection.create(std::move(handle), Kiwi::Core::Connection::kReceiveRequests)) {
        core_log(CORE_LOG_WARN, "failed to create connection");
        return;
    }

    handle.detach();

    Client* client = new Client(std::move(connection), pid);

    /* See if this client matches one of our services. Note that Service holds
     * a handle to its process, so we can guarantee here that we're talking to
     * the right process if the IDs match. */
    for (const auto &it : m_services) {
        Service *service = it.second.get();

        if (service->processId() == pid) {
            service->setClient(client);
            client->setService(service);
        }
    }
}

int main(int argc, char **argv) {
    return g_serviceManager.run();
}
