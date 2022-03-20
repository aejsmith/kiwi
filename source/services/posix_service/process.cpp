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

#include <kernel/condition.h>
#include <kernel/process.h>
#include <kernel/status.h>

#include <kiwi/core/token_setter.h>

#include <services/posix_service.h>

#include <assert.h>
#include <errno.h>
#include <signal.h>

/** Internal signal disposition values. */
enum {
    kSignalDisposition_Default      = POSIX_SIGNAL_DISPOSITION_DEFAULT,
    kSignalDisposition_Ignore       = POSIX_SIGNAL_DISPOSITION_IGNORE,
    kSignalDisposition_Handler      = POSIX_SIGNAL_DISPOSITION_HANDLER,

    kSignalDisposition_Terminate    = 3,
    kSignalDisposition_CoreDump,
    kSignalDisposition_Stop,
    kSignalDisposition_Continue,
};

Process::Process(Kiwi::Core::Connection connection, Kiwi::Core::Handle handle, process_id_t pid) :
    m_connection     (std::move(connection)),
    m_handle         (std::move(handle)),
    m_pid            (pid),
    m_signalsPending (0),
    m_signalMask     (0)
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
    debug_log("PID %" PRId32 " hung up connection", m_pid);

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
        case POSIX_REQUEST_GET_SIGNAL_CONDITION:    reply = handleGetSignalCondition(message); break;
        case POSIX_REQUEST_GET_PENDING_SIGNAL:      reply = handleGetPendingSignal(message); break;
        case POSIX_REQUEST_SET_SIGNAL_ACTION:       reply = handleSetSignalAction(message); break;
        case POSIX_REQUEST_KILL:                    reply = handleKill(message); break;

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

Kiwi::Core::Message Process::handleGetSignalCondition(const Kiwi::Core::Message &request) {
    status_t ret;

    Kiwi::Core::Message reply;
    if (!reply.createReply(request, sizeof(posix_reply_get_signal_condition_t))) {
        core_log(CORE_LOG_WARN, "failed to allocate reply message");
        return Kiwi::Core::Message();
    }

    auto replyData = reply.data<posix_reply_get_signal_condition_t>();
    replyData->err = 0;

    if (!m_signalCondition.isValid()) {
        ret = kern_condition_create(m_signalCondition.attach());
        if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_WARN, "failed to create signal condition: %" PRId32);
            replyData->err = ENOMEM;
            return reply;
        }
    }

    reply.attachHandle(m_signalCondition);
    return reply;
}

Kiwi::Core::Message Process::handleGetPendingSignal(const Kiwi::Core::Message &request) {
    Kiwi::Core::Message reply;
    if (!reply.createReply(request, sizeof(posix_reply_get_pending_signal_t))) {
        core_log(CORE_LOG_WARN, "failed to allocate reply message");
        return Kiwi::Core::Message();
    }

    auto replyData = reply.data<posix_reply_get_pending_signal_t>();

    if (m_signalsPending) {
        int32_t num = __builtin_ffs(m_signalsPending) - 1;
        m_signalsPending &= ~(1 << num);

        memcpy(&replyData->info, &m_signals[num].info, sizeof(replyData->info));
    } else {
        replyData->info.si_signo = 0;
    }

    updateSignalCondition();

    reply.attachHandle(m_signalCondition);
    return reply;
}

Kiwi::Core::Message Process::handleSetSignalAction(const Kiwi::Core::Message &request) {
    Kiwi::Core::Message reply;
    if (!reply.createReply(request, sizeof(posix_reply_set_signal_action_t))) {
        core_log(CORE_LOG_WARN, "failed to allocate reply message");
        return Kiwi::Core::Message();
    }

    auto replyData = reply.data<posix_reply_set_signal_action_t>();
    replyData->err = 0;

    if (request.size() != sizeof(posix_request_set_signal_action_t)) {
        replyData->err = EINVAL;
        return reply;
    }

    auto requestData = request.data<posix_request_set_signal_action_t>();

    if (requestData->num < 1 || requestData->num >= NSIG) {
        replyData->err = EINVAL;
        return reply;
    }

    switch (requestData->disposition) {
        case kSignalDisposition_Default:
            break;

        case kSignalDisposition_Ignore:
        case kSignalDisposition_Handler:
            /* It is not allowed to set these to non-default action. */
            if (requestData->num == SIGKILL || requestData->num == SIGSTOP) {
                replyData->err = EINVAL;
                return reply;
            }

            break;

        default:
            replyData->err = EINVAL;
            return reply;
    }

    SignalState &signal = m_signals[requestData->num];

    signal.disposition = requestData->disposition;
    signal.flags       = requestData->flags;

    replyData->err = 0;
    return reply;
}

/** Get the set of deliverable signals (pending and unmasked). */
uint32_t Process::signalsDeliverable() const {
    return m_signalsPending & ~m_signalMask;
}

void Process::updateSignalCondition() {
    if (m_signalCondition.isValid()) {
        bool state = signalsDeliverable() != 0;

        status_t ret = kern_condition_set(m_signalCondition, state);
        if (ret != STATUS_SUCCESS)
            core_log(CORE_LOG_ERROR, "failed to set signal condition for PID %" PRId32 ": %" PRId32, m_pid, ret);
    }
}

/** Perform the default action for a signal. */
static int32_t defaultSignal(handle_t process, int32_t num) {
    status_t ret;

    uint32_t disposition;
    switch (num) {
        case SIGHUP:
        case SIGINT:
        case SIGKILL:
        case SIGPIPE:
        case SIGALRM:
        case SIGTERM:
        case SIGUSR1:
        case SIGUSR2:
            disposition = kSignalDisposition_Terminate;
            break;

        case SIGQUIT:
        case SIGILL:
        case SIGTRAP:
        case SIGABRT:
        case SIGBUS:
        case SIGFPE:
        case SIGSEGV:
            disposition = kSignalDisposition_CoreDump;
            break;

        case SIGSTOP:
        case SIGTSTP:
        case SIGTTIN:
        case SIGTTOU:
            disposition = kSignalDisposition_Stop;
            break;

        case SIGCONT:
            disposition = kSignalDisposition_Continue;
            break;

        case SIGCHLD:
        case SIGURG:
        case SIGWINCH:
            disposition = kSignalDisposition_Ignore;
            break;

        default:
            core_log(CORE_LOG_ERROR, "unhandled signal %" PRId32, num);
            disposition = kSignalDisposition_Ignore;
            break;
    }

    switch (disposition) {
        case kSignalDisposition_Terminate:
        case kSignalDisposition_CoreDump:
            // TODO: Core dump.
            ret = kern_process_kill(process, (__POSIX_KILLED_STATUS << 16) | num);
            break;

        case kSignalDisposition_Stop:
        case kSignalDisposition_Continue:
            // TODO: Stop/continue.
            core_log(CORE_LOG_ERROR, "TODO: signal stop/continue");
            break;

        default:
            /* Ignore. */
            break;
    }

    return 0;
}

int32_t Process::sendSignal(int32_t num, const Process *sender, const security_context_t *senderSecurity) {
    SignalState &signal = m_signals[num];

    if (signal.disposition == kSignalDisposition_Default) {
        return defaultSignal(m_handle, num);
    } else if (signal.disposition == kSignalDisposition_Handler) {
        /* If it's not already pending, save info for it. */
        if (!(m_signalsPending & (1 << num))) {
            memset(&signal.info, 0, sizeof(signal.info));

            signal.info.si_signo = num;
            signal.info.si_pid   = sender->m_pid;
            signal.info.si_uid   = senderSecurity->uid;

            m_signalsPending |= (1 << num);
        }

        updateSignalCondition();
        return 0;
    } else {
        /* Ignored. */
        return 0;
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

    if (requestData->num < 1 || requestData->num >= NSIG) {
        replyData->err = EINVAL;
        return reply;
    }

    // TODO: Process groups etc.
    if (requestData->pid <= 0) {
        replyData->err = ENOSYS;
        return reply;
    }

    /* Set our overridden token to the caller's security context so that access
     * checks on the target process are performed against that. */
    {
        Kiwi::Core::TokenSetter token;
        ret = token.set(security);
        if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_WARN, "failed to set security context: %" PRId32);
            replyData->err = EPERM;
            return reply;
        }

        Process *process = g_posixService.findProcess(requestData->pid);

        /* If the process is not known, it has not connected to the service and
         * therefore should be treated as having default signal state. We need
         * to open a handle to it. */
        Kiwi::Core::Handle openedHandle;
        handle_t handle;
        if (process) {
            handle = process->m_handle;
        } else {
            ret = kern_process_open(requestData->pid, openedHandle.attach());
            if (ret != STATUS_SUCCESS) {
                if (ret == STATUS_NOT_FOUND) {
                    replyData->err = ESRCH;
                } else {
                    core_log(
                        CORE_LOG_WARN, "failed to open process %" PRId32 ": %" PRId32,
                        requestData->pid, ret);

                    replyData->err = EAGAIN;
                }

                return reply;
            }

            handle = openedHandle;
        }

        /* Check if we have sufficient privilege to signal the process. The
         * kernel's privileged access definition matches the requirement of
         * POSIX so use that. */
        // TODO: What about saved-setuid?
        ret = kern_process_access(handle);
        if (ret != STATUS_SUCCESS) {
            replyData->err = EPERM;
            return reply;
        }

        if (process) {
            replyData->err = process->sendSignal(requestData->num, this, security);
        } else {
            replyData->err = defaultSignal(handle, requestData->num);
        }
    }

    return reply;
}
