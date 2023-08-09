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

#include <termios.h>

#include <thread>
#include <vector>

class Terminal {
public:
    Terminal(size_t id, Kiwi::Core::Connection connection);
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

    void handleClientHangup();
    void handleClientMessages();
    Kiwi::Core::Message handleClientOpenHandle(Kiwi::Core::Message &request);
    Kiwi::Core::Message handleClientInput(Kiwi::Core::Message &request);

    void handleFileHangup();
    void handleFileMessages();
    status_t handleFileRead(const ipc_message_t &message);
    status_t handleFileWrite(const ipc_message_t &message, const void *data);
    status_t handleFileInfo(const ipc_message_t &message);
    status_t handleFileRequest(const ipc_message_t &message, const void *data);
    status_t handleFileWait(const ipc_message_t &message);
    status_t handleFileUnwait(const ipc_message_t &message);
    void signalReadEvents();

    status_t sendOutput(const void *data, size_t size);

    void addInput(unsigned char value);
    bool isControlChar(uint16_t ch, int control) const;
    void echoInput(uint16_t ch, bool raw);

    bool isReadable() const;
    bool readBuffer(ReadOperation &op);
    bool eraseChar();
    size_t eraseLine();
    void clearBuffer();

    status_t getProcessGroup(pid_t caller, pid_t &pgid);
    status_t setProcessGroup(pid_t caller, pid_t pgid);
    void handleSessionLeaderDeath();

private:
    size_t m_id;
    Kiwi::Core::Connection m_connection;
    std::thread m_thread;
    Kiwi::Core::Handle m_userFile;
    Kiwi::Core::Handle m_userFileConnection;
    Kiwi::Core::EventLoop m_eventLoop;
    bool m_exit;

    /** Pending reads that are waiting for input. */
    std::vector<ReadOperation> m_pendingReads;

    /** Readable event requests. */
    std::vector<uint64_t> m_readEvents;

    /** Terminal state. */
    struct termios m_termios;           /**< Terminal I/O settings. */
    struct winsize m_winsize;           /**< Window size. */
    bool m_escaped : 1;                 /**< Whether the next input character is escaped. */
    bool m_inhibited : 1;               /**< Whether output has been stopped. */

    /** Controlling process info. */
    Kiwi::Core::Handle m_sessionLeader; /**< Session leader process. */
    pid_t m_sessionId;                  /**< Session that the terminal is controlling. */
    pid_t m_processGroupId;             /**< Foreground process group ID. */

    /** Circular input buffer. */
    uint16_t m_inputBuffer[kInputBufferMax];
    size_t m_inputBufferStart;
    size_t m_inputBufferSize;
    size_t m_inputBufferLines;

    Kiwi::Core::EventRef m_sessionLeaderDeathEvent;
};
