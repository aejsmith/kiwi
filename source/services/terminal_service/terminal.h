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

#include <termios.h>

#include <thread>
#include <vector>

class Terminal {
public:
    Terminal(core_connection_t *connection);
    ~Terminal();

    void run();

private:
    static constexpr size_t kInputBufferMax = 8192;

    /** Special character flags. */
    enum CharFlags : uint16_t {
        kChar_Escaped = (1<<8),         /**< Character is escaped. */
        kChar_NewLine = (1<<9),         /**< Character is classed as a new line. */
        kChar_Eof     = (1<<10),        /**< Character is an end-of-file. */
    };

    /** Details of a pending read operation. */
    struct ReadOperation {
        uint64_t serial;                /**< Serial number of the operation. */
        size_t size;                    /**< Size of the read request. */
        bool canon : 1;                 /**< Whether this request is in canonical mode. */
        bool nonblock : 1;              /**< Whether this is a non-blocking request. */
    };

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
    status_t handleFileRequest(const ipc_message_t &message, const void *data);

    status_t sendOutput(const void *data, size_t size);

    void addInput(unsigned char value);
    bool isControlChar(uint16_t ch, int control) const;
    void echoInput(uint16_t ch, bool raw);

    bool readBuffer(ReadOperation &op);
    bool eraseChar();
    size_t eraseLine();

private:
    core_connection_t *const m_connection;
    std::thread m_thread;
    handle_t m_userFile;
    handle_t m_userFileConnection;

    /** Pending reads that are waiting for input. */
    std::vector<ReadOperation> m_pendingReads;

    /** Terminal state. */
    struct termios m_termios;           /**< Terminal I/O settings. */
    struct winsize m_winsize;           /**< Window size. */
    bool m_escaped : 1;                 /**< Whether the next input character is escaped. */
    bool m_inhibited : 1;               /**< Whether output has been stopped. */

    /** Circular input buffer. */
    uint16_t m_inputBuffer[kInputBufferMax];
    size_t m_inputBufferStart;
    size_t m_inputBufferSize;
    size_t m_inputBufferLines;
};
