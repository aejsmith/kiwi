/*
 * Copyright (C) 2009-2023 Alex Smith
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

#include <kiwi/core/connection.h>
#include <kiwi/core/event_loop.h>
#include <kiwi/core/handle.h>

class TerminalBuffer;

class Terminal {
public:
    Terminal(TerminalWindow &window);
    virtual ~Terminal();

    bool init();

    void handleMessages();

    void sendInput(char ch);
    void sendInput(const char *str);
    void sendInput(const uint8_t *buf, size_t len);
    void flushInput();

    /** Get the active buffer. */
    virtual TerminalBuffer &activeBuffer() = 0;

    /** Output to the terminal. */
    virtual void output(uint8_t ch) = 0;

private:
    void handleHangupEvent();
    void handleDeathEvent();

    void handleOutput(const Kiwi::Core::Message &message);

    bool spawnProcess(const char *path, Kiwi::Core::Handle &handle);

protected:
    TerminalWindow& m_window;

private:
    Kiwi::Core::Connection m_connection;    /**< Connection to terminal service. */
    Kiwi::Core::Handle m_childProcess;      /**< Main child process. */
    Kiwi::Core::Handle m_terminal[2];       /**< Terminal handles (read/write). */

    static constexpr uint32_t kInputBatchMax = 128;
    uint8_t m_inputBatch[kInputBatchMax];
    size_t m_inputBatchSize;

    Kiwi::Core::EventRef m_hangupEvent;
    Kiwi::Core::EventRef m_messageEvent;
    Kiwi::Core::EventRef m_deathEvent;
};
