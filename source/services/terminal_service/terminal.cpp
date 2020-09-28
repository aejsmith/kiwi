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
#include <core/utility.h>

#include <kernel/status.h>
#include <kernel/thread.h>

#include <assert.h>
#include <inttypes.h>

#include <array>

#include "protocol.h"

Terminal::Terminal(core_connection_t *connection) :
    m_connection (connection)
{}

Terminal::~Terminal() {
    core_connection_close(m_connection);
}

void Terminal::run() {
    m_thread = std::thread([this] () { thread(); });
}

void Terminal::thread() {
    status_t ret;

    core_log(CORE_LOG_DEBUG, "thread started");

    std::array<object_event_t, 2> events;
    events[0].handle = core_connection_get_handle(m_connection);
    events[0].event  = CONNECTION_EVENT_HANGUP;
    events[1].handle = events[0].handle;
    events[1].event  = CONNECTION_EVENT_MESSAGE;

    bool exit = false;

    while (!exit) {
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

    core_log(CORE_LOG_DEBUG, "thread exiting");
    m_thread.detach();
    delete this;
}

bool Terminal::handleEvent(object_event_t &event) {
    if (event.handle == core_connection_get_handle(m_connection)) {
        switch (event.event) {
            case CONNECTION_EVENT_HANGUP:
                core_log(CORE_LOG_DEBUG, "client hung up, closing terminal");
                return true;

            case CONNECTION_EVENT_MESSAGE:
                return handleMessages();

            default:
                core_unreachable();

        }
    } else {
        core_unreachable();
    }

    return false;
}

bool Terminal::handleMessages() {
    while (true) {
        status_t ret;

        core_message_t *message;
        ret = core_connection_receive(m_connection, 0, &message);
        if (ret == STATUS_WOULD_BLOCK) {
            return false;
        } else if (ret == STATUS_CONN_HUNGUP) {
            return true;
        } else if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_WARN, "failed to receive messages: %" PRId32, ret);
            return false;
        }

        assert(core_message_get_type(message) == CORE_MESSAGE_REQUEST);

        core_message_t *reply = nullptr;

        uint32_t id = core_message_get_id(message);
        switch (id) {
            case TERMINAL_REQUEST_OPEN_HANDLE:
                reply = handleOpenHandle(message);
                break;
            case TERMINAL_REQUEST_INPUT:
                reply = handleInput(message);
                break;
            default:
                core_log(CORE_LOG_WARN, "unhandled request %" PRIu32, id);
                break;
        }

        if (reply) {
            ret = core_connection_reply(m_connection, reply);

            core_message_destroy(reply);

            if (ret != STATUS_SUCCESS)
                core_log(CORE_LOG_WARN, "failed to send reply: %" PRId32, ret);
        }

        core_message_destroy(message);
    }
}

core_message_t *Terminal::handleOpenHandle(core_message_t *request) {
    core_message_t *reply = core_message_create_reply(request, sizeof(terminal_reply_open_handle_t));
    if (!reply) {
        core_log(CORE_LOG_ERROR, "failed to create message");
        return nullptr;
    }

    auto replyData = reinterpret_cast<terminal_reply_open_handle_t *>(core_message_get_data(reply));
    replyData->result = STATUS_NOT_IMPLEMENTED;

    return reply;
}

core_message_t *Terminal::handleInput(core_message_t *request) {
    core_message_t *reply = core_message_create_reply(request, sizeof(terminal_reply_input_t));
    if (!reply) {
        core_log(CORE_LOG_ERROR, "failed to create message");
        return nullptr;
    }

    auto replyData = reinterpret_cast<terminal_reply_input_t *>(core_message_get_data(reply));
    replyData->result = STATUS_NOT_IMPLEMENTED;

    return reply;
}
