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
 * @brief               Terminal class.
 */

#include "terminal_app.h"
#include "terminal_window.h"
#include "terminal.h"

#include <core/log.h>
#include <core/service.h>
#include <core/utility.h>

#include <kernel/file.h>
#include <kernel/process.h>
#include <kernel/status.h>

#include <services/terminal_service.h>

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <termios.h>
#include <unistd.h>

#include <array>

extern const char *const *environ;

Terminal::Terminal(TerminalWindow &window) :
    m_window         (window),
    m_inputBatchSize (0)
{}

Terminal::~Terminal() {}

bool Terminal::init() {
    status_t ret;

    ret = m_connection.openService(TERMINAL_SERVICE_NAME, 0, Kiwi::Core::Connection::kReceiveSignals);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to open connection to terminal service: %" PRId32, ret);
        return false;
    }

    /* Request handles to the terminal. */
    const uint32_t handleAccess[2] = { FILE_ACCESS_READ, FILE_ACCESS_WRITE };
    for (int i = 0; i < 2; i++) {
        Kiwi::Core::Message request;
        if (!request.createRequest(TERMINAL_REQUEST_OPEN_HANDLE, sizeof(terminal_request_open_handle_t))) {
            core_log(CORE_LOG_ERROR, "failed to create request message");
            return false;
        }

        auto requestData = request.data<terminal_request_open_handle_t>();
        requestData->access = handleAccess[i];

        Kiwi::Core::Message reply;
        ret = m_connection.request(request, reply);
        if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_ERROR, "failed to make terminal handle request: %" PRId32, ret);
            return false;
        }

        auto replyData = reply.data<terminal_reply_open_handle_t>();

        if (replyData->result != STATUS_SUCCESS) {
            core_log(CORE_LOG_ERROR, "failed to open terminal handle: %" PRId32, replyData->result);
            return false;
        }

        m_terminal[i].attach(reply.detachHandle());
        assert(m_terminal[i] != INVALID_HANDLE);

        kern_handle_set_flags(m_terminal[i], HANDLE_INHERITABLE);
    }

    /* Configure window size. */
    struct winsize ws;
    ws.ws_col = m_window.cols();
    ws.ws_row = m_window.rows();

    ret = kern_file_request(m_terminal[1], TIOCSWINSZ, &ws, sizeof(ws), nullptr, 0, nullptr);
    if (ret != STATUS_SUCCESS)
        core_log(CORE_LOG_WARN, "failed to set window size: %" PRId32, ret);

    /* Spawn a process attached to the terminal. */
    if (!spawnProcess("/system/bin/bash", m_childProcess))
        return false;

    handle_t connHandle = m_connection.handle();

    m_hangupEvent = g_terminalApp.eventLoop().addEvent(
        connHandle, CONNECTION_EVENT_HANGUP, 0,
        [this] (const object_event_t &event) { handleHangupEvent(); });
    m_messageEvent = g_terminalApp.eventLoop().addEvent(
        connHandle, CONNECTION_EVENT_MESSAGE, 0,
        [this] (const object_event_t &event) { handleMessages(); });
    m_deathEvent = g_terminalApp.eventLoop().addEvent(
        m_childProcess, PROCESS_EVENT_DEATH, 0,
        [this] (const object_event_t &event) { handleDeathEvent(); });

    return true;
}

void Terminal::handleHangupEvent() {
    core_log(CORE_LOG_ERROR, "lost connection to terminal service, exiting");

    /* This will delete us. */
    m_window.close();
}

void Terminal::handleMessages() {
    while (true) {
        Kiwi::Core::Message message;
        status_t ret = m_connection.receive(0, message);
        if (ret == STATUS_WOULD_BLOCK) {
            break;
        } else if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_WARN, "failed to receive messages: %" PRId32, ret);
            break;
        }

        switch (message.id()) {
            case TERMINAL_SIGNAL_OUTPUT:
                handleOutput(message);
                break;
            default:
                core_log(CORE_LOG_ERROR, "unhandled signal %" PRIu32, message.id());
                break;
        }
    }
}

void Terminal::handleDeathEvent() {
    core_log(CORE_LOG_NOTICE, "child process exited, exiting");

    /* This will delete us. */
    m_window.close();
}

void Terminal::handleOutput(const Kiwi::Core::Message &message) {
    const uint8_t *data = message.data<uint8_t>();
    size_t size         = message.size();

    for (size_t i = 0; i < size; i++)
        output(data[i]);
}

void Terminal::sendInput(char ch) {
    /* Input is batched if possible to reduce number of messages. */
    m_inputBatch[m_inputBatchSize] = ch;

    m_inputBatchSize++;
    if (m_inputBatchSize == kInputBatchMax)
        flushInput();
}

void Terminal::sendInput(const char *str) {
    /* Input is batched if possible to reduce number of messages. */
    while (str[0]) {
        sendInput(str[0]);
        str++;
    }
}

void Terminal::sendInput(const uint8_t *buf, size_t len) {
    while (len) {
        size_t size = std::min(kInputBatchMax - m_inputBatchSize, len);

        memcpy(&m_inputBatch[m_inputBatchSize], buf, size);

        m_inputBatchSize += size;
        if (m_inputBatchSize == kInputBatchMax)
            flushInput();

        buf += size;
        len -= size;
    }
}

void Terminal::flushInput() {
    if (m_inputBatchSize == 0)
        return;

    Kiwi::Core::Message request;
    if (!request.createRequest(TERMINAL_REQUEST_INPUT, m_inputBatchSize)) {
        core_log(CORE_LOG_ERROR, "failed to create terminal input request");
        return;
    }

    memcpy(request.data(), m_inputBatch, m_inputBatchSize);

    Kiwi::Core::Message reply;
    status_t ret = m_connection.request(request, reply);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to make terminal input request: %" PRId32, ret);
        return;
    }

    auto replyData = reply.data<terminal_reply_input_t>();
    ret = replyData->result;

    if (ret != STATUS_SUCCESS)
        core_log(CORE_LOG_ERROR, "failed to send terminal input: %" PRId32, ret);

    m_inputBatchSize = 0;
}

/** Spawn a process attached to the terminal. */
bool Terminal::spawnProcess(const char *path, Kiwi::Core::Handle &handle) {
    status_t ret;

    ret = kern_process_clone(handle.attach());
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to create child process: %" PRId32, ret);
        return false;
    }

    if (!handle.isValid()) {
        int sid = setsid();
        if (sid < 0) {
            core_log(CORE_LOG_ERROR, "failed to create session: %d", errno);
            exit(EXIT_FAILURE);
        }

        /* Initial tcsetpgrp() sets the terminal session as well. SID == PGID. */
        int err = tcsetpgrp(m_terminal[0], sid);
        if (err < 0) {
            core_log(CORE_LOG_ERROR, "failed to set foreground process group: %d", errno);
            exit(EXIT_FAILURE);
        }

        /* Child process. */
        process_attrib_t attrib;
        handle_t map[][2] = { { m_terminal[0], 0 }, { m_terminal[1], 1 }, { m_terminal[1], 2 } };
        attrib.token     = INVALID_HANDLE;
        attrib.root_port = INVALID_HANDLE;
        attrib.map       = map;
        attrib.map_count = core_array_size(map);

        const char *args[] = { path, nullptr };
        ret = kern_process_exec(path, args, environ, 0, &attrib);
        core_log(CORE_LOG_ERROR, "failed to execute process '%s': %d", path, ret);
        exit(EXIT_FAILURE);
    }

    return true;
}
