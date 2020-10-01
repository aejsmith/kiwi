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
 * @brief               Terminal class.
 */

#pragma once

#include <core/ipc.h>

#include <thread>

class Terminal {
public:
    Terminal(core_connection_t *connection);
    ~Terminal();

    void run();

private:
    void thread();

    bool handleEvent(object_event_t &event);

    bool handleClientMessages();
    core_message_t *handleClientOpenHandle(core_message_t *request);
    core_message_t *handleClientInput(core_message_t *request);

    bool handleFileMessages();
    status_t handleFileRead(const ipc_message_t &message);
    status_t handleFileWrite(const ipc_message_t &message, const void *data);
    status_t handleFileInfo(const ipc_message_t &message);

private:
    core_connection_t *const m_connection;
    std::thread m_thread;

    handle_t m_userFile;
    handle_t m_userFileConnection;
};
