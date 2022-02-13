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
 * @brief               POSIX process class.
 */

#include "posix_service.h"
#include "process.h"

#include <core/log.h>

#include <kernel/process.h>
#include <kernel/status.h>

#include <services/posix_service.h>

#include <assert.h>

Process::Process(core_connection_t *connection, Kiwi::Core::Handle handle, process_id_t pid) :
    m_connection    (connection),
    m_handle        (std::move(handle)),
    m_pid           (pid)
{
    core_log(CORE_LOG_DEBUG, "connection received from %" PRId32, m_pid);

    handle_t connHandle = core_connection_handle(m_connection);

    m_hangupEvent = g_posixService.eventLoop().addEvent(
        connHandle, CONNECTION_EVENT_HANGUP, 0,
        [this] (const object_event_t &event) { handleHangupEvent(); });
    m_messageEvent = g_posixService.eventLoop().addEvent(
        connHandle, CONNECTION_EVENT_MESSAGE, 0,
        [this] (const object_event_t &event) { handleMessageEvent(); });
}

Process::~Process() {
    core_connection_close(m_connection);
}

void Process::handleHangupEvent() {
    g_posixService.removeProcess(this);
}

void Process::handleMessageEvent() {
    core_message_t *message;
    status_t ret = core_connection_receive(m_connection, 0, &message);
    if (ret != STATUS_SUCCESS)
        return;

    assert(core_message_type(message) == CORE_MESSAGE_REQUEST);

    uint32_t id = core_message_id(message);
    switch (id) {
        case POSIX_REQUEST_KILL:
            handleKill(message);
            break;

        default:
            core_log(
                CORE_LOG_NOTICE, "received unrecognised message type %" PRId32 " from client %" PRId32,
                id, m_pid);
            break;
    }

    core_message_destroy(message);
}

void Process::handleKill(core_message_t *request) {
    core_log(CORE_LOG_NOTICE, "kill request");
}
