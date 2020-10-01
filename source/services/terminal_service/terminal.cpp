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
#include <kernel/user_file.h>

#include <assert.h>
#include <inttypes.h>

#include <array>

#include "protocol.h"

Terminal::Terminal(core_connection_t *connection) :
    m_connection         (connection),
    m_userFile           (INVALID_HANDLE),
    m_userFileConnection (INVALID_HANDLE)
{}

Terminal::~Terminal() {
    core_connection_close(m_connection);

    if (m_userFile != INVALID_HANDLE)
        kern_handle_close(m_userFile);

    if (m_userFileConnection != INVALID_HANDLE)
        kern_handle_close(m_userFileConnection);
}

void Terminal::run() {
    status_t ret;

    ret = kern_user_file_create(
        FILE_TYPE_CHAR, FILE_ACCESS_READ | FILE_ACCESS_WRITE, 0,
        &m_userFileConnection, &m_userFile);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to create user file: %" PRId32, ret);
        return;
    }

    m_thread = std::thread([this] () { thread(); });
}

void Terminal::thread() {
    status_t ret;

    core_log(CORE_LOG_DEBUG, "thread started");

    std::array<object_event_t, 4> events;
    events[0].handle = core_connection_get_handle(m_connection);
    events[0].event  = CONNECTION_EVENT_HANGUP;
    events[1].handle = events[0].handle;
    events[1].event  = CONNECTION_EVENT_MESSAGE;
    events[2].handle = m_userFileConnection;
    events[2].event  = CONNECTION_EVENT_HANGUP;
    events[3].handle = m_userFileConnection;
    events[3].event  = CONNECTION_EVENT_MESSAGE;

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
                return handleClientMessages();

            default:
                core_unreachable();

        }
    } else if (event.handle == m_userFileConnection) {
        switch (event.event) {
            case CONNECTION_EVENT_HANGUP:
                /* This shouldn't happen since we have the file open ourself. */
                core_log(CORE_LOG_ERROR, "user file connection hung up unexpectedly");
                return true;

            case CONNECTION_EVENT_MESSAGE:
                return handleFileMessages();

            default:
                core_unreachable();

        }
    } else {
        core_unreachable();
    }

    return false;
}

bool Terminal::handleClientMessages() {
    while (true) {
        status_t ret;

        core_message_t *message;
        ret = core_connection_receive(m_connection, 0, &message);
        if (ret == STATUS_WOULD_BLOCK) {
            return false;
        } else if (ret == STATUS_CONN_HUNGUP) {
            return true;
        } else if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_WARN, "failed to receive client message: %" PRId32, ret);
            return false;
        }

        assert(core_message_get_type(message) == CORE_MESSAGE_REQUEST);

        core_message_t *reply = nullptr;

        uint32_t id = core_message_get_id(message);
        switch (id) {
            case TERMINAL_REQUEST_OPEN_HANDLE:
                reply = handleClientOpenHandle(message);
                break;
            case TERMINAL_REQUEST_INPUT:
                reply = handleClientInput(message);
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

core_message_t *Terminal::handleClientOpenHandle(core_message_t *request) {
    auto requestData = reinterpret_cast<const terminal_request_open_handle_t *>(core_message_get_data(request));

    core_message_t *reply = core_message_create_reply(request, sizeof(terminal_reply_open_handle_t));
    if (!reply) {
        core_log(CORE_LOG_ERROR, "failed to create message");
        return nullptr;
    }

    auto replyData = reinterpret_cast<terminal_reply_open_handle_t *>(core_message_get_data(reply));
    replyData->result = STATUS_SUCCESS;

    handle_t handle;
    status_t ret = kern_file_reopen(m_userFile, requestData->access, 0, &handle);
    if (ret != STATUS_SUCCESS) {
        replyData->result = STATUS_TRY_AGAIN;
    } else {
        core_message_attach_handle(reply, handle, true);
    }

    return reply;
}

core_message_t *Terminal::handleClientInput(core_message_t *request) {
    core_message_t *reply = core_message_create_reply(request, sizeof(terminal_reply_input_t));
    if (!reply) {
        core_log(CORE_LOG_ERROR, "failed to create message");
        return nullptr;
    }

    auto replyData = reinterpret_cast<terminal_reply_input_t *>(core_message_get_data(reply));
    replyData->result = STATUS_NOT_IMPLEMENTED;

    return reply;
}

bool Terminal::handleFileMessages() {
    status_t ret;

    while (true) {
        ipc_message_t message;
        ret = kern_connection_receive(m_userFileConnection, &message, nullptr, 0);
        if (ret == STATUS_WOULD_BLOCK) {
            return false;
        } else if (ret == STATUS_CONN_HUNGUP) {
            core_log(CORE_LOG_ERROR, "user file connection hung up unexpectedly");
            return true;
        } else if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_WARN, "failed to receive file message: %" PRId32, ret);
            return false;
        }

        std::unique_ptr<uint8_t[]> data;
        if (message.size > 0) {
            data.reset(new uint8_t[message.size]);

            ret = kern_connection_receive_data(m_userFileConnection, data.get());
            if (ret != STATUS_SUCCESS) {
                core_log(CORE_LOG_WARN, "failed to receive file message data: %" PRId32, ret);
                return false;
            }
        }

        switch (message.id) {
            case USER_FILE_OP_READ:
                ret = handleFileRead(message);
                break;
            case USER_FILE_OP_WRITE:
                ret = handleFileWrite(message, data.get());
                break;
            case USER_FILE_OP_INFO:
                ret = handleFileInfo(message);
                break;
            default:
                core_unreachable();
        }

        if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_WARN, "failed to send file message: %" PRId32, ret);
            return false;
        }
    }
}

static ipc_message_t initializeFileReply(const ipc_message_t &message) {
    ipc_message_t reply = {};
    reply.id = message.id;
    reply.args[USER_FILE_MESSAGE_ARG_SERIAL] = message.args[USER_FILE_MESSAGE_ARG_SERIAL];
    return reply;
}

status_t Terminal::handleFileRead(const ipc_message_t &message) {
    //ipc_message_t reply = initializeFileReply(message);
    // This needs to block until input is available (except for nonblocking read).
    return STATUS_SUCCESS;
}

status_t Terminal::handleFileWrite(const ipc_message_t &message, const void *data) {
    status_t ret;

    /* Pass this on to the client. */
    core_message_t *signal = core_message_create_signal(TERMINAL_SIGNAL_OUTPUT, message.size);
    if (signal) {
        memcpy(core_message_get_data(signal), data, message.size);

        ret = core_connection_signal(m_connection, signal);
        if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_WARN, "failed to send signal: %" PRId32, ret);
            ret = STATUS_DEVICE_ERROR;
        }

        core_message_destroy(signal);
    } else {
        core_log(CORE_LOG_ERROR, "failed to create message");
        ret = STATUS_NO_MEMORY;
    }

    ipc_message_t reply = initializeFileReply(message);
    reply.args[USER_FILE_MESSAGE_ARG_WRITE_STATUS] = ret;
    reply.args[USER_FILE_MESSAGE_ARG_WRITE_SIZE]   = (ret == STATUS_SUCCESS) ? message.size : 0;

    return kern_connection_send(m_userFileConnection, &reply, nullptr, INVALID_HANDLE, -1);
}

status_t Terminal::handleFileInfo(const ipc_message_t &message) {
    file_info_t info;
    memset(&info, 0, sizeof(info));
    info.block_size = 4096;
    info.links      = 1;

    ipc_message_t reply = initializeFileReply(message);
    reply.size = sizeof(file_info_t);

    return kern_connection_send(m_userFileConnection, &reply, &info, INVALID_HANDLE, -1);
}
