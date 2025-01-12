/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Service manager.
 */

#pragma once

#include <kiwi/core/event_loop.h>
#include <kiwi/core/handle.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Client;
class Service;

/** Main class of the service manager. */
class ServiceManager {
public:
    ServiceManager();
    ~ServiceManager();

    Kiwi::Core::EventLoop &eventLoop() { return m_eventLoop; }

    int run();

    Service *findService(const std::string &name);

    status_t spawnProcess(const std::vector<std::string> &args, Kiwi::Core::Handle *_handle = nullptr) const;

private:
    void addService(std::string name, std::string path, uint32_t flags);

    void handleConnectionEvent();

private:
    using ServiceMap = std::unordered_map<std::string, std::unique_ptr<Service>>;

private:
    Kiwi::Core::Handle m_port;

    ServiceMap m_services;

    Kiwi::Core::EventLoop m_eventLoop;
    Kiwi::Core::EventRef m_connectionEvent;
};

extern ServiceManager g_serviceManager;
