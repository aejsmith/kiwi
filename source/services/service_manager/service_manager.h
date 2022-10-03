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
