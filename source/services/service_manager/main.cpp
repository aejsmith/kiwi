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
 * @brief               Service manager main function.
 */

#include <core/ipc.h>
#include <core/log.h>
#include <core/utility.h>

#include <kernel/ipc.h>
#include <kernel/object.h>
#include <kernel/process.h>
#include <kernel/status.h>
#include <kernel/thread.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include <condition_variable>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

extern const char *const *environ;

class Service;

/**
 * Handles an object event. TODO: Probably something that should be wrapped up
 * into a proper C++ API in future.
 */
class EventHandler {
public:
    /** Handle an event on the object. */
    virtual void handleEvent(const object_event_t *event) = 0;
};

/** Represents a client connection. */
class Client final : public EventHandler {
public:
    Client(core_connection_t *connection, process_id_t processId);
    ~Client();

    void setService(Service *service) { m_service = service; }

    void handleEvent(const object_event_t *event) override;

protected:
    core_connection_t *m_connection;
    process_id_t m_processId;
    Service *m_service;
};

/** Represents a service. */
class Service final : public EventHandler {
public:
    enum Flags : uint32_t {
        /** Service is an IPC service and so can be connected to by clients. */
        kIpc = (1<<0),

        /** Start service on-demand (in combination with kIpc). */
        kOnDemand = (1<<1),
    };

public:
    Service(std::string name, std::string path, uint32_t flags);
    ~Service();

    const std::string &getName() const { return m_name; }

    bool start();

    void handleEvent(const object_event_t *event) override;

private:
    void handleDeath();

private:
    const std::string m_name;
    const std::string m_path;
    const uint32_t m_flags;

    handle_t m_process;
    handle_t m_port;

    Client *m_client;

    std::condition_variable m_portWait;
};

/** Main class of the service manager. */
class ServiceManager final : public EventHandler {
public:
    ServiceManager();
    ~ServiceManager();

    int run();

    status_t spawnProcess(const char *path, handle_t *_handle = nullptr) const;

    void handleEvent(const object_event_t *event) override;

private:
    void addService(std::string name, std::string path, uint32_t flags);

    void addEvent(handle_t handle, unsigned id, EventHandler *handler);
    void removeEvents(EventHandler *handler);

private:
    using ServiceMap = std::unordered_map<std::string, std::unique_ptr<Service>>;

private:
    handle_t m_servicePort;

    ServiceMap m_services;
    std::vector<object_event_t> m_events;
};

static ServiceManager g_serviceManager;

Client::Client(core_connection_t *connection, process_id_t processId) :
    m_connection (connection),
    m_processId  (processId)
{}

Client::~Client() {
    core_connection_close(m_connection);
}

void Client::handleEvent(const object_event_t *event) {
// clean up events, detach from service if not null.
}

Service::Service(std::string name, std::string path, uint32_t flags) :
    m_name    (std::move(name)),
    m_path    (std::move(path)),
    m_flags   (flags),
    m_process (INVALID_HANDLE),
    m_port    (INVALID_HANDLE),
    m_client  (nullptr)
{}

Service::~Service() {
    if (m_process != INVALID_HANDLE)
        kern_handle_close(m_process);

    if (m_port != INVALID_HANDLE)
        kern_handle_close(m_port);
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
    }

    return true;
}

void Service::handleEvent(const object_event_t *event) {

}

void Service::handleDeath() {
    core_log(CORE_LOG_WARN, "service '%s' terminated unexpectedly", m_name.c_str());

    kern_handle_close(m_process);
    m_process = INVALID_HANDLE;

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

ServiceManager::ServiceManager() :
    m_servicePort (INVALID_HANDLE)
{}

ServiceManager::~ServiceManager() {
    if (m_servicePort != INVALID_HANDLE)
        kern_handle_close(m_servicePort);
}

int ServiceManager::run() {
    status_t ret;

    core_log(CORE_LOG_NOTICE, "service manager started");

    ret = kern_port_create(&m_servicePort);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to create port: %d", ret);
        return EXIT_FAILURE;
    }

    addEvent(m_servicePort, PORT_EVENT_CONNECTION, this);

    /* TODO: Service configuration. */
    addService("org.kiwi.test", "/system/services/test", Service::kIpc);

    spawnProcess("/system/bin/shell");

    while (true) {
        ret = kern_object_wait(m_events.data(), m_events.size(), 0, -1);
        if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_WARN, "failed to wait for events: %d", ret);
            continue;
        }
    }
}

void ServiceManager::addService(std::string name, std::string path, uint32_t flags) {
    std::unique_ptr<Service> service(new Service(std::move(name), std::move(path), flags));

    if (!(flags & Service::kOnDemand))
        service->start();

    m_services.emplace(service->getName(), std::move(service));
}

status_t ServiceManager::spawnProcess(const char *path, handle_t *_handle) const {
    process_attrib_t attrib;
    handle_t map[][2] = { { 0, 0 }, { 1, 1 }, { 2, 2 } };
    attrib.token     = INVALID_HANDLE;
    attrib.root_port = m_servicePort;
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

    assert(event->handle == m_servicePort);
    assert(event->event == PORT_EVENT_CONNECTION);

    handle_t handle;
    ipc_client_t ipcClient;
    ret = kern_port_listen(m_servicePort, &ipcClient, 0, &handle);
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

    addEvent(handle, CONNECTION_EVENT_HANGUP, client);
    addEvent(handle, CONNECTION_EVENT_MESSAGE, client);

    core_log(CORE_LOG_DEBUG, "got client %" PRId32, ipcClient.pid);
}

void ServiceManager::addEvent(handle_t handle, unsigned id, EventHandler *handler) {
    m_events.emplace_back();
    object_event_t &event = m_events.back();

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
