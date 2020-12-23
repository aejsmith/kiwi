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

#include "event_handler.h"

#include <core/ipc.h>

class TerminalBuffer;

class Terminal : public EventHandler {
public:
    Terminal(TerminalWindow &window);
    virtual ~Terminal();

    bool init();

    void handleEvent(const object_event_t &event) final override;
    void handleMessages();

    void sendInput(const uint8_t *buf, size_t len);

    /** Get the active buffer. */
    virtual TerminalBuffer &activeBuffer() = 0;

    /** Output to the terminal. */
    virtual void output(uint8_t ch) = 0;

private:
    void handleOutput(core_message_t *message);

    status_t spawnProcess(const char *path, handle_t &handle);

protected:
    TerminalWindow& m_window;

private:
    core_connection_t *m_connection;        /**< Connection to terminal service. */
    handle_t m_childProcess;                /**< Main child process. */
    handle_t m_terminal[2];                 /**< Terminal handles (read/write). */
};
