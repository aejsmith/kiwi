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

#include "terminal.h"

#include <core/log.h>
#include <core/service.h>
#include <core/utility.h>

#include <kernel/device.h>
#include <kernel/file.h>
#include <kernel/process.h>
#include <kernel/status.h>

#include <assert.h>
#include <inttypes.h>

#include <array>

#include "../../services/terminal_service/protocol.h"

extern const char *const *environ;

Terminal::Terminal() {}

Terminal::~Terminal() {}

void Terminal::run() {
    status_t ret;

    ret = core_service_connect(TERMINAL_SERVICE_NAME, 0, CORE_CONNECTION_RECEIVE_SIGNALS, &m_connection);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to open connection to terminal service: %" PRId32, ret);
        return;
    }

    /* Open a non-blocking kernel console device handle (temporary, once we
     * have drivers implemented this will go away and we'll go directly to the
     * input/framebuffer devices, and then to a GUI once that's implemented). */
    ret = kern_device_open("/kconsole", FILE_ACCESS_READ | FILE_ACCESS_WRITE, FILE_NONBLOCK, &m_device);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to open input device: %" PRId32, ret);
        return;
    }

    /* Spawn a process attached to the terminal. */
    if (spawnProcess("/system/bin/shell", m_childProcess) != STATUS_SUCCESS)
        return;

    std::array<object_event_t, 4> events;
    events[0].handle = core_connection_get_handle(m_connection);
    events[0].event  = CONNECTION_EVENT_HANGUP;
    events[1].handle = events[0].handle;
    events[1].event  = CONNECTION_EVENT_MESSAGE;
    events[2].handle = m_childProcess;
    events[2].event  = PROCESS_EVENT_DEATH;
    events[3].handle = m_device;
    events[3].event  = FILE_EVENT_READABLE;

    bool exit = false;

    while (!exit) {
        /* Process any internally queued messages on the connection (if any
         * messages were queued internally while waiting for a request response,
         * these won't be picked up by kern_object_wait()). TODO: Better
         * solution for this, e.g. core_connection provides an event object to
         * signal. */
        handleMessages();

        ret = kern_object_wait(events.data(), events.size(), 0, -1);
        if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_WARN, "failed to wait for events: %" PRId32, ret);
            continue;
        }

        for (object_event_t &event : events) {
            if (event.flags & OBJECT_EVENT_SIGNALLED) {
                exit = handleEvent(event);
            } else if (event.flags & OBJECT_EVENT_ERROR) {
                core_log(CORE_LOG_WARN, "error signalled on event %" PRId32 "/%" PRId32, event.handle, event.event);
            }

            event.flags &= ~(OBJECT_EVENT_SIGNALLED | OBJECT_EVENT_ERROR);
        }
    }
}

bool Terminal::handleEvent(object_event_t &event) {
    if (event.handle == core_connection_get_handle(m_connection)) {
        switch (event.event) {
            case CONNECTION_EVENT_HANGUP:
                core_log(CORE_LOG_ERROR, "lost connection to terminal service, exiting");
                return true;

            case CONNECTION_EVENT_MESSAGE:
                handleMessages();
                break;

            default:
                core_unreachable();

        }
    } else if (event.handle == m_childProcess) {
        assert(event.event == PROCESS_EVENT_DEATH);

        core_log(CORE_LOG_NOTICE, "child process exited, exiting");
        return true;
    } else if (event.handle == m_device) {
        assert(event.event == FILE_EVENT_READABLE);

        handleInput();
    } else {
        core_unreachable();
    }

    return false;
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

        uint32_t id = core_message_get_id(message);
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
    const void *data = core_message_get_data(message);
    size_t size      = core_message_get_size(message) ;

    kern_file_write(m_device, data, size, -1, nullptr);
}

void Terminal::handleInput() {
    /* Read as much as we can in 128 byte batches, so that we're not repeatedly
     * doing syscalls and sending messages for 1 byte at a time. TODO: Could
     * provide a resize API on core_message to shrink the message and read
     * directly into it. */
    static constexpr size_t kBatchSize = 128;
    char buf[kBatchSize];

    while (true) {
        size_t bytesRead = 0;
        kern_file_read(m_device, buf, kBatchSize, -1, &bytesRead);
        if (bytesRead == 0)
            break;

        core_message_t *request = core_message_create_request(TERMINAL_REQUEST_INPUT, bytesRead);

        memcpy(core_message_get_data(request), buf, bytesRead);

        core_message_t *reply;
        status_t ret = core_connection_request(m_connection, request, &reply);

        core_message_destroy(request);

        if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_ERROR, "failed to make terminal input request: %" PRId32, ret);
            break;
        }

        auto replyData = reinterpret_cast<terminal_reply_input_t *>(core_message_get_data(reply));
        ret = replyData->result;

        core_message_destroy(reply);

        if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_ERROR, "failed to send terminal input: %" PRId32, ret);
            break;
        }
    }
}

/** Spawn a process attached to the terminal. */
status_t Terminal::spawnProcess(const char *path, handle_t &handle) {
    status_t ret;

    /* Request handles to the terminal. */
    const uint32_t handleAccess[2] = { FILE_ACCESS_READ, FILE_ACCESS_WRITE };
    handle_t handles[2];
    for (int i = 0; i < 2; i++) {
        core_message_t *request =
            core_message_create_request(TERMINAL_REQUEST_OPEN_HANDLE, sizeof(terminal_request_open_handle_t));

        auto requestData = reinterpret_cast<terminal_request_open_handle_t *>(core_message_get_data(request));
        requestData->access = handleAccess[i];

        core_message_t *reply;
        ret = core_connection_request(m_connection, request, &reply);

        core_message_destroy(request);

        if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_ERROR, "failed to make terminal handle request: %" PRId32, ret);
            return ret;
        }

        auto replyData = reinterpret_cast<terminal_reply_open_handle_t *>(core_message_get_data(reply));
        ret        = replyData->result;
        handles[i] = core_message_detach_handle(reply);

        core_message_destroy(reply);

        if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_ERROR, "failed to open terminal handle: %" PRId32, ret);
            return ret;
        }

        assert(handles[i] != INVALID_HANDLE);
    }

    process_attrib_t attrib;
    handle_t map[][2] = { { handles[0], 0 }, { handles[1], 1 }, { handles[1], 2 } };
    attrib.token     = INVALID_HANDLE;
    attrib.root_port = INVALID_HANDLE;
    attrib.map       = map;
    attrib.map_count = core_array_size(map);

    const char *args[] = { path, nullptr };
    ret = kern_process_create(path, args, environ, 0, &attrib, &handle);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to create process '%s': %d", path, ret);
        return ret;
    }

    return STATUS_SUCCESS;
}
