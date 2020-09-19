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
 *
 * TODO:
 *  - Shouldn't block when sending messages if the remote message queue is full
 *    as this could be used for denial of service by blocking the service
 *    manager. We need a core_connection function that can do an asynchronous
 *    send driven by an event for space becoming available, and drop messages
 *    if we can't send them in a set amount of time to prevent us from piling
 *    up unsent messages.
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

#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "protocol.h"

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

    Service *service() const { return m_service; }

    void setService(Service *service) { m_service = service; }

    void handleEvent(const object_event_t *event) override;

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

    const std::string &name() const { return m_name; }
    uint32_t flags() const          { return m_flags; }
    process_id_t processId() const  { return m_processId; }
    Client *client() const          { return m_client; }
    handle_t port() const           { return m_port; }

    void setClient(Client *client) { m_client = client; }
    bool setPort(handle_t port);

    void addPendingConnect(Client *client, core_message_t *reply);
    void removePendingConnects(Client *client);

    bool start();

    void handleEvent(const object_event_t *event) override;

private:
    struct PendingConnect {
        Client *client;
        core_message_t *reply;
    };

private:
    void handleDeath();

private:
    const std::string m_name;
    const std::string m_path;
    const uint32_t m_flags;

    handle_t m_process;
    process_id_t m_processId;

    Client *m_client;

    handle_t m_port;

    std::list<PendingConnect> m_pendingConnects;
};

/** Main class of the service manager. */
class ServiceManager final : public EventHandler {
public:
    ServiceManager();
    ~ServiceManager();

    int run();

    Service* findService(const std::string &name);

    status_t spawnProcess(const char *path, handle_t *_handle = nullptr) const;

    void addEvent(handle_t handle, unsigned id, EventHandler *handler);
    void removeEvents(EventHandler *handler);

    void handleEvent(const object_event_t *event) override;

private:
    void addService(std::string name, std::string path, uint32_t flags);

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
    m_pendingConnects.emplace_back();
    PendingConnect &connect = m_pendingConnects.back();

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

        m_processId = kern_process_id(m_process);

        g_serviceManager.addEvent(m_process, PROCESS_EVENT_DEATH, this);
    }

    return true;
}

void Service::handleEvent(const object_event_t *event) {
    assert(event->handle == m_process);
    assert(event->event == PROCESS_EVENT_DEATH);

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
    addService("org.kiwi.test", "/system/services/test", Service::kIpc | Service::kOnDemand);

    spawnProcess("/system/bin/shell");

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
    std::unique_ptr<Service> service(new Service(std::move(name), std::move(path), flags));

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
