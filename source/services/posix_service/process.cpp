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

#include <kiwi/core/token_setter.h>

#include <services/posix_service.h>

#include <assert.h>
#include <errno.h>

Process::Process(Kiwi::Core::Connection connection, Kiwi::Core::Handle handle, process_id_t pid) :
    m_connection    (std::move(connection)),
    m_handle        (std::move(handle)),
    m_pid           (pid)
{
    debug_log("connection received from PID %" PRId32, m_pid);

    handle_t connHandle = m_connection.handle();

    m_hangupEvent = g_posixService.eventLoop().addEvent(
        connHandle, CONNECTION_EVENT_HANGUP, 0,
        [this] (const object_event_t &event) { handleHangupEvent(); });
    m_messageEvent = g_posixService.eventLoop().addEvent(
        connHandle, CONNECTION_EVENT_MESSAGE, 0,
        [this] (const object_event_t &event) { handleMessageEvent(); });
}

Process::~Process() {}

void Process::handleHangupEvent() {
    /* This destroys the Process, don't access this after. */
    g_posixService.removeProcess(this);
}

void Process::handleMessageEvent() {
    Kiwi::Core::Message message;
    status_t ret = m_connection.receive(0, message);
    if (ret != STATUS_SUCCESS)
        return;

    assert(message.type() == Kiwi::Core::Message::kRequest);

    Kiwi::Core::Message reply;

    uint32_t id = message.id();
    switch (id) {
        case POSIX_REQUEST_KILL:
            reply = handleKill(message);
            break;

        default:
            core_log(
                CORE_LOG_NOTICE, "received unrecognised message type %" PRId32 " from client %" PRId32,
                id, m_pid);
            break;
    }

    if (reply.isValid()) {
        ret = m_connection.reply(reply);
        if (ret != STATUS_SUCCESS)
            core_log(CORE_LOG_WARN, "failed to send reply: %" PRId32, ret);
    }
}

Kiwi::Core::Message Process::handleKill(const Kiwi::Core::Message &request) {
    status_t ret;

    Kiwi::Core::Message reply;
    if (!reply.createReply(request, sizeof(posix_reply_kill_t))) {
        core_log(CORE_LOG_WARN, "failed to allocate reply message");
        return Kiwi::Core::Message();
    }

    auto replyData = reply.data<posix_reply_kill_t>();
    replyData->err = 0;

    const security_context_t *security = request.security();

    if (request.size() != sizeof(posix_request_kill_t) || !security) {
        replyData->err = EINVAL;
        return reply;
    }

    auto requestData = request.data<posix_request_kill_t>();

    debug_log("kill(%" PRId32 ", %" PRId32 ") from PID %" PRId32, requestData->pid, requestData->num, m_pid);

    {
        Kiwi::Core::TokenSetter token;
        ret = token.set(security);
        if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_WARN, "failed to set security context: %" PRId32);
            replyData->err = EPERM;
            return reply;
        }


    }

    replyData->err = ENOSYS;
    return reply;
}
