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
 * @brief               POSIX process class.
 */

#pragma once

#include <kiwi/core/connection.h>
#include <kiwi/core/event_loop.h>
#include <kiwi/core/handle.h>

class Process {
public:
    Process(Kiwi::Core::Connection connection, Kiwi::Core::Handle handle, process_id_t pid);
    ~Process();

    process_id_t pid() const { return m_pid; }

private:
    void handleHangupEvent();
    void handleMessageEvent();

    Kiwi::Core::Message handleKill(const Kiwi::Core::Message &request);

private:
    Kiwi::Core::Connection m_connection;
    Kiwi::Core::Handle m_handle;
    process_id_t m_pid;

    Kiwi::Core::EventRef m_hangupEvent;
    Kiwi::Core::EventRef m_messageEvent;
};
