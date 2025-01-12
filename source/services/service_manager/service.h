/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Service class.
 */

#pragma once

#include <kiwi/core/event_loop.h>
#include <kiwi/core/handle.h>
#include <kiwi/core/message.h>

#include <list>
#include <string>

class Client;

/** Represents a service. */
class Service {
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
    bool setPort(Kiwi::Core::Handle port);

    void addPendingConnect(Client *client, Kiwi::Core::Message reply);
    void removePendingConnects(Client *client);

    bool start();

private:
    struct PendingConnect {
        Client *client;
        Kiwi::Core::Message reply;
    };

private:
    void handleDeath();

private:
    const std::string m_name;
    const std::string m_path;
    const uint32_t m_flags;

    Kiwi::Core::Handle m_process;
    process_id_t m_processId;

    Client *m_client;

    Kiwi::Core::Handle m_port;

    std::list<PendingConnect> m_pendingConnects;

    Kiwi::Core::EventRef m_deathEvent;
};
