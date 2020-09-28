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

#include <vector>

class Terminal {
public:
    Terminal();
    ~Terminal();

    void run();

private:
    bool handleEvent(object_event_t &event);
    void handleMessages();
    void handleOutput(core_message_t *message);
    void handleInput();

    status_t spawnProcess(const char *path, handle_t &handle);

private:
    core_connection_t *m_connection;        /**< Connection to terminal service. */
    handle_t m_device;                      /**< Target device. */
    handle_t m_childProcess;                /**< Main child process. */
    std::vector<object_event_t> m_events;   /**< Event list. */
};
