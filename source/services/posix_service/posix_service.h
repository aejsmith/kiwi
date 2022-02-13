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
 * @brief               POSIX service.
 */

#pragma once

#include <kiwi/core/event_loop.h>
#include <kiwi/core/handle.h>

#include <memory>
#include <unordered_map>

class Process;

class PosixService {
public:
    PosixService();
    ~PosixService();

    Kiwi::Core::EventLoop &eventLoop() { return m_eventLoop; }

    int run();

    void removeProcess(Process *process);

private:
    void handleConnectionEvent();

private:
    Kiwi::Core::Handle m_port;
    Kiwi::Core::EventLoop m_eventLoop;

    std::unordered_map<process_id_t, std::unique_ptr<Process>> m_processes;

    Kiwi::Core::EventRef m_connectionEvent;
};

extern PosixService g_posixService;
