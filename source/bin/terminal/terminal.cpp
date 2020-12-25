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
#include <inttypes.h>

#include <array>

extern const char *const *environ;

Terminal::Terminal(TerminalWindow &window) :
    m_window         (window),
    m_connection     (nullptr),
    m_childProcess   (INVALID_HANDLE),
    m_terminal       {INVALID_HANDLE, INVALID_HANDLE},
    m_inputBatchSize (0)
{}

Terminal::~Terminal() {
    g_terminalApp.removeEvents(this);

    if (m_connection)
        core_connection_close(m_connection);

    if (m_childProcess != INVALID_HANDLE)
        kern_handle_close(m_childProcess);

    for (handle_t handle : m_terminal) {
        if (handle != INVALID_HANDLE)
            kern_handle_close(handle);
    }
}

bool Terminal::init() {
    status_t ret;

    ret = core_service_connect(TERMINAL_SERVICE_NAME, 0, CORE_CONNECTION_RECEIVE_SIGNALS, &m_connection);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to open connection to terminal service: %" PRId32, ret);
        return false;
    }

    /* Request handles to the terminal. */
    const uint32_t handleAccess[2] = { FILE_ACCESS_READ, FILE_ACCESS_WRITE };
    for (int i = 0; i < 2; i++) {
        core_message_t *request =
            core_message_create_request(TERMINAL_REQUEST_OPEN_HANDLE, sizeof(terminal_request_open_handle_t));

        auto requestData = reinterpret_cast<terminal_request_open_handle_t *>(core_message_data(request));
        requestData->access = handleAccess[i];

        core_message_t *reply;
        ret = core_connection_request(m_connection, request, &reply);

        core_message_destroy(request);

        if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_ERROR, "failed to make terminal handle request: %" PRId32, ret);
            return false;
        }

        auto replyData = reinterpret_cast<terminal_reply_open_handle_t *>(core_message_data(reply));
        ret            = replyData->result;
        m_terminal[i]  = core_message_detach_handle(reply);

        core_message_destroy(reply);

        if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_ERROR, "failed to open terminal handle: %" PRId32, ret);
            return false;
        }

        assert(m_terminal[i] != INVALID_HANDLE);

        kern_handle_set_flags(m_terminal[i], HANDLE_INHERITABLE);
    }

    /* Spawn a process attached to the terminal. */
    if (spawnProcess("/system/bin/bash", m_childProcess) != STATUS_SUCCESS)
        return false;

    g_terminalApp.addEvent(core_connection_handle(m_connection), CONNECTION_EVENT_HANGUP, this);
    g_terminalApp.addEvent(core_connection_handle(m_connection), CONNECTION_EVENT_MESSAGE, this);
    g_terminalApp.addEvent(m_childProcess, PROCESS_EVENT_DEATH, this);

    return true;
}

void Terminal::handleEvent(const object_event_t &event) {
    bool close = false;

    if (event.handle == core_connection_handle(m_connection)) {
        switch (event.event) {
            case CONNECTION_EVENT_HANGUP:
                core_log(CORE_LOG_ERROR, "lost connection to terminal service, exiting");
                close = true;
                break;

            case CONNECTION_EVENT_MESSAGE:
                handleMessages();
                break;

            default:
                core_unreachable();

        }
    } else if (event.handle == m_childProcess) {
        assert(event.event == PROCESS_EVENT_DEATH);

        core_log(CORE_LOG_NOTICE, "child process exited, exiting");
        close = true;
    } else {
        core_unreachable();
    }

    if (close) {
        /* This will delete us - do this last. */
        m_window.close();
    }
}

void Terminal::handleMessages() {
    while (true) {
        core_message_t *message;
        status_t ret = core_connection_receive(m_connection, 0, &message);
        if (ret == STATUS_WOULD_BLOCK) {
            break;
        } else if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_WARN, "failed to receive messages: %" PRId32, ret);
            break;
        }

        uint32_t id = core_message_id(message);
        switch (id) {
            case TERMINAL_SIGNAL_OUTPUT:
                handleOutput(message);
                break;
            default:
                core_log(CORE_LOG_ERROR, "unhandled signal %" PRIu32, id);
                break;
        }

        core_message_destroy(message);
    }
}

void Terminal::handleOutput(core_message_t *message) {
    const uint8_t *data = reinterpret_cast<uint8_t *>(core_message_data(message));
    size_t size         = core_message_size(message);

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

    core_message_t *request = core_message_create_request(TERMINAL_REQUEST_INPUT, m_inputBatchSize);

    memcpy(core_message_data(request), m_inputBatch, m_inputBatchSize);

    core_message_t *reply;
    status_t ret = core_connection_request(m_connection, request, &reply);

    core_message_destroy(request);

    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to make terminal input request: %" PRId32, ret);
        return;
    }

    auto replyData = reinterpret_cast<terminal_reply_input_t *>(core_message_data(reply));
    ret = replyData->result;

    core_message_destroy(reply);

    if (ret != STATUS_SUCCESS)
        core_log(CORE_LOG_ERROR, "failed to send terminal input: %" PRId32, ret);

    m_inputBatchSize = 0;
}

/** Spawn a process attached to the terminal. */
status_t Terminal::spawnProcess(const char *path, handle_t &handle) {
    process_attrib_t attrib;
    handle_t map[][2] = { { m_terminal[0], 0 }, { m_terminal[1], 1 }, { m_terminal[1], 2 } };
    attrib.token     = INVALID_HANDLE;
    attrib.root_port = INVALID_HANDLE;
    attrib.map       = map;
    attrib.map_count = core_array_size(map);

    const char *args[] = { path, nullptr };
    status_t ret = kern_process_create(path, args, environ, 0, &attrib, &handle);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to create process '%s': %d", path, ret);
        return ret;
    }

    return STATUS_SUCCESS;
}
