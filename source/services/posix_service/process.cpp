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
#include "process_group.h"
#include "process.h"
#include "session.h"

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

static int openProcess(pid_t pid, Kiwi::Core::Handle &handle) {
    status_t ret = kern_process_open(pid, handle.attach());
    if (ret != STATUS_SUCCESS) {
        if (ret == STATUS_NOT_FOUND) {
            return ESRCH;
        } else {
            core_log(CORE_LOG_WARN, "failed to open process %" PRId32 ": %" PRId32, pid, ret);
            return EAGAIN;
        }
    }

    return 0;
}

static int getProcessHandle(Process *caller, pid_t pid, Kiwi::Core::Handle &openedHandle, handle_t &handle) {
    if (pid == caller->id()) {
        handle = caller->handle();
    } else {
        Process *process = g_posixService.findProcess(pid);
        if (process) {
            handle = process->handle();
        } else {
            int err = openProcess(pid, openedHandle);
            if (err != 0)
                return err;

            handle = openedHandle;
        }
    }

    return 0;
}

Process::Process(Kiwi::Core::Connection connection, Kiwi::Core::Handle handle, pid_t pid) :
    m_connection     (std::move(connection)),
    m_handle         (std::move(handle)),
    m_id             (pid),
    m_signalsPending (0),
    m_signalMask     (0)
{
    debug_log("connection received from PID %" PRId32, m_id);

    m_deathEvent = g_posixService.eventLoop().addEvent(
        m_handle, PROCESS_EVENT_DEATH, 0,
        [this] (const object_event_t &) { handleDeathEvent(); });

    initConnection();
}

Process::~Process() {}

void Process::initConnection() {
    handle_t connHandle = m_connection.handle();

    m_hangupEvent = g_posixService.eventLoop().addEvent(
        connHandle, CONNECTION_EVENT_HANGUP, 0,
        [this] (const object_event_t &) { handleHangupEvent(); });
    m_messageEvent = g_posixService.eventLoop().addEvent(
        connHandle, CONNECTION_EVENT_MESSAGE, 0,
        [this] (const object_event_t &) { handleMessageEvent(); });
}

void Process::reconnect(Kiwi::Core::Connection connection) {
    if (m_connection.isValid()) {
        if (m_connection.isActive()) {
            core_log(CORE_LOG_NOTICE, "ignoring connection from already connected process %" PRId32, m_id);
            return;
        }

        m_connection.close();
    }

    debug_log("PID %" PRId32 " reconnected", m_id);

    m_connection = std::move(connection);
    initConnection();
}

void Process::handleDeathEvent() {
    debug_log("PID %" PRId32 " died", m_id);

    /* This destroys the Process, don't access this after. */
    g_posixService.removeProcess(this);
}

void Process::handleHangupEvent() {
    debug_log("PID %" PRId32 " hung up connection", m_id);

    m_connection.close();
    m_hangupEvent.remove();
    m_messageEvent.remove();

    /* We treat a hangup without the process dying as an exec(). */
    status_t ret = kern_process_status(m_handle, nullptr, nullptr);
    if (ret == STATUS_STILL_RUNNING) {
        /* Across exec, we retain the signal mask, ignored signals, and any
         * pending signals. Signals with handlers are reset to their default
         * action. */
        for (int num = 0; num < NSIG; num++) {
            SignalState &signal = m_signals[num];

            signal.flags = 0;

            if (signal.disposition == kSignalDisposition_Handler)
                signal.disposition = kSignalDisposition_Default;
        }

        updateSignals();
    } else {
        /* We should handle the death event after and destroy the process. */
    }
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
        case POSIX_REQUEST_SET_SIGNAL_MASK:         reply = handleSetSignalMask(message); break;
        case POSIX_REQUEST_KILL:                    reply = handleKill(message); break;
        case POSIX_REQUEST_GETPGID:                 reply = handleGetpgid(message); break;
        case POSIX_REQUEST_SETPGID:                 reply = handleSetpgid(message); break;
        case POSIX_REQUEST_GETSID:                  reply = handleGetsid(message); break;
        case POSIX_REQUEST_SETSID:                  reply = handleSetsid(message); break;

        default:
            core_log(
                CORE_LOG_NOTICE, "received unrecognised message type %" PRId32 " from client %" PRId32,
                id, m_id);
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

    uint32_t deliverable = signalsDeliverable();
    if (deliverable) {
        int32_t num = __builtin_ffs(deliverable) - 1;
        SignalState &signal = m_signals[num];

        /* If it's still deliverable here, it should be using a handler.
         * Ignored signals should not ever be set in pending, and default
         * signals should be handled as soon as they made deliverable. */
        assert(signal.disposition == kSignalDisposition_Handler);

        memcpy(&replyData->info, &m_signals[num].info, sizeof(replyData->info));

        m_signalsPending &= ~(1 << num);
    } else {
        replyData->info.si_signo = 0;
    }

    /* This will update the signal condition state . */
    updateSignals();

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

    /* If it was pending but now ignored, remove it. */
    if (signal.disposition == kSignalDisposition_Ignore) {
        m_signalsPending &= ~(1 << requestData->num);
        updateSignals();
    }

    replyData->err = 0;
    return reply;
}

Kiwi::Core::Message Process::handleSetSignalMask(const Kiwi::Core::Message &request) {
    Kiwi::Core::Message reply;
    if (!reply.createReply(request, sizeof(posix_reply_set_signal_mask_t))) {
        core_log(CORE_LOG_WARN, "failed to allocate reply message");
        return Kiwi::Core::Message();
    }

    auto replyData = reply.data<posix_reply_set_signal_mask_t>();
    replyData->err = 0;

    if (request.size() != sizeof(posix_request_set_signal_mask_t)) {
        replyData->err = EINVAL;
        return reply;
    }

    auto requestData = request.data<posix_request_set_signal_mask_t>();

    /* Attempts to mask SIGKILL or SIGSTOP are silently ignored. */
    uint32_t mask = requestData->mask;
    mask &=  (1 << NSIG) - 1;
    mask &= ~(1 << SIGKILL);
    mask &= ~(1 << SIGSTOP);

    if (mask != m_signalMask) {
        m_signalMask = mask;
        updateSignals();
    }

    replyData->err = 0;
    return reply;
}

/**
 * Perform the default action for a signal. Note that this requires privileged
 * access to the process, and the service should be running with sufficient
 * privilege (PRIV_PROCESS_ADMIN) for this, so it is not necessary to call with
 * the sending thread's security context.
 */
static void defaultSignal(handle_t process, int32_t num) {
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
            if (ret != STATUS_SUCCESS)
                core_log(CORE_LOG_ERROR, "failed to kill process: %" PRId32, ret);

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
}

/** Get the set of deliverable signals (pending and unmasked). */
uint32_t Process::signalsDeliverable() const {
    return m_signalsPending & ~m_signalMask;
}

/**
 * Called when signal state is changed such that we should re-test if we can
 * deliver any signals.
 */
void Process::updateSignals() {
    bool needHandler = false;

    uint32_t deliverable = signalsDeliverable();
    while (deliverable) {
        int32_t num = __builtin_ffs(deliverable) - 1;
        deliverable &= ~(1 << num);

        SignalState &signal = m_signals[num];

        /* These should not be in the pending set. */
        assert(signal.disposition != kSignalDisposition_Ignore);

        if (signal.disposition == kSignalDisposition_Default) {
            defaultSignal(m_handle, num);
            m_signalsPending &= ~(1 << num);
        } else if (signal.disposition == kSignalDisposition_Handler) {
            /* Removed from pending set by GET_PENDING_SIGNAL. */
            needHandler = true;
        }
    }

    if (m_signalCondition.isValid()) {
        status_t ret = kern_condition_set(m_signalCondition, needHandler);
        if (ret != STATUS_SUCCESS)
            core_log(CORE_LOG_ERROR, "failed to set signal condition for PID %" PRId32 ": %" PRId32, m_id, ret);
    }
}

void Process::sendSignal(int32_t num, const Process *sender, const security_context_t *senderSecurity) {
    SignalState &signal = m_signals[num];

    /* Only need to do something if it's not ignored, and not already pending. */
    if (signal.disposition != kSignalDisposition_Ignore &&
        !(m_signalsPending & (1 << num)))
    {
        memset(&signal.info, 0, sizeof(signal.info));

        signal.info.si_signo = num;
        signal.info.si_pid   = sender->m_id;
        signal.info.si_uid   = senderSecurity->uid;

        m_signalsPending |= (1 << num);

        updateSignals();
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

    debug_log("kill(%" PRId32 ", %" PRId32 ") from PID %" PRId32, requestData->pid, requestData->num, m_id);

    if (requestData->num < 1 || requestData->num >= NSIG) {
        replyData->err = EINVAL;
        return reply;
    }

    // TODO: Process groups etc. Don't allow for default process group
    if (requestData->pid <= 0) {
        replyData->err = ENOSYS;
        return reply;
    }

    Kiwi::Core::Handle openedHandle;
    handle_t handle;
    replyData->err = getProcessHandle(this, requestData->pid, openedHandle, handle);
    if (replyData->err != 0)
        return reply;

    /* Check if we have sufficient privilege to signal the process. The kernel's
     * privileged access definition matches the requirement of POSIX so use
     * that. */
    // TODO: What about saved-setuid?
    if (requestData->pid != m_id) {
        Kiwi::Core::TokenSetter token;
        ret = token.set(security);
        if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_WARN, "failed to set security context: %" PRId32);
            replyData->err = EPERM;
            return reply;
        }

        ret = kern_process_access(handle);
        if (ret != STATUS_SUCCESS) {
            replyData->err = EPERM;
            return reply;
        }
    }

    Process *process = g_posixService.findProcess(requestData->pid);
    if (process) {
        process->sendSignal(requestData->num, this, security);
    } else {
        /* If the process is not known, it has not connected to the service and
         * therefore should be treated as having default signal state. */
        defaultSignal(handle, requestData->num);
    }

    return reply;
}

Kiwi::Core::Message Process::handleGetpgid(const Kiwi::Core::Message &request) {
    Kiwi::Core::Message reply;
    if (!reply.createReply(request, sizeof(posix_reply_getpgid_t))) {
        core_log(CORE_LOG_WARN, "failed to allocate reply message");
        return Kiwi::Core::Message();
    }

    auto replyData = reply.data<posix_reply_getpgid_t>();
    replyData->err  = 0;
    replyData->pgid = 0;

    if (request.size() != sizeof(posix_request_getpgid_t)) {
        replyData->err = EINVAL;
        return reply;
    }

    auto requestData = request.data<posix_request_getpgid_t>();

    if (requestData->pid < 0) {
        replyData->err = EINVAL;
        return reply;
    }

    pid_t pid = (requestData->pid != 0) ? requestData->pid : m_id;

    Kiwi::Core::Handle openedHandle;
    handle_t handle;
    replyData->err = getProcessHandle(this, pid, openedHandle, handle);
    if (replyData->err != 0)
        return reply;

    ProcessGroup *group = g_posixService.findProcessGroupForProcess(handle);
    replyData->pgid = group->id();

    return reply;
}

Kiwi::Core::Message Process::handleSetpgid(const Kiwi::Core::Message &request) {
    Kiwi::Core::Message reply;
    if (!reply.createReply(request, sizeof(posix_reply_setpgid_t))) {
        core_log(CORE_LOG_WARN, "failed to allocate reply message");
        return Kiwi::Core::Message();
    }

    auto replyData = reply.data<posix_reply_setpgid_t>();
    replyData->err = 0;

    if (request.size() != sizeof(posix_request_setpgid_t)) {
        replyData->err = EINVAL;
        return reply;
    }

    auto requestData = request.data<posix_request_setpgid_t>();

    if (requestData->pid < 0 || requestData->pgid < 0) {
        replyData->err = EINVAL;
        return reply;
    }

    pid_t pid = (requestData->pid != 0) ? requestData->pid : m_id;

    Kiwi::Core::Handle openedHandle;
    handle_t handle;
    replyData->err = getProcessHandle(this, pid, openedHandle, handle);
    if (replyData->err != 0)
        return reply;

    if (pid != m_id) {
        // TODO: Allow changing other processes. This is only allowed if the
        // target process is a child of the caller and has not execve()'d yet.
        // We don't currently have the capability to track this.
        // This must also reject children in a different session to the caller.
        replyData->err = ENOSYS;
        return reply;
    }

    /* New group must be in the same session as the *calling* process. A process
     * can only change the group of child processes in the same session as it,
     * so the calling and target process sessions are the same. */
    ProcessGroup *currentGroup = g_posixService.findProcessGroupForProcess(handle);

    if (currentGroup->session()->id() == pid) {
        replyData->err = EPERM;
        return reply;
    }

    pid_t pgid = (requestData->pgid != 0) ? requestData->pgid : pid;

    if (pgid != currentGroup->id()) {
        ProcessGroup *newGroup = g_posixService.findProcessGroup(pgid);
        if (newGroup) {
            if (newGroup->session() != currentGroup->session()) {
                replyData->err = EPERM;
                return reply;
            }

            newGroup->addProcess(handle);
        } else if (pgid == pid) {
            newGroup = g_posixService.createProcessGroup(pgid, currentGroup->session(), handle);
            if (!newGroup) {
                replyData->err = EAGAIN;
                return reply;
            }
        } else {
            replyData->err = EPERM;
            return reply;
        }

        currentGroup->removeProcess(handle);
    }

    return reply;
}

Kiwi::Core::Message Process::handleGetsid(const Kiwi::Core::Message &request) {
    Kiwi::Core::Message reply;
    if (!reply.createReply(request, sizeof(posix_reply_getsid_t))) {
        core_log(CORE_LOG_WARN, "failed to allocate reply message");
        return Kiwi::Core::Message();
    }

    auto replyData = reply.data<posix_reply_getsid_t>();
    replyData->err = 0;

    if (request.size() != sizeof(posix_request_getsid_t)) {
        replyData->err = EINVAL;
        return reply;
    }

    auto requestData = request.data<posix_request_getsid_t>();

    pid_t pid = (requestData->pid != 0) ? requestData->pid : m_id;

    Kiwi::Core::Handle openedHandle;
    handle_t handle;
    replyData->err = getProcessHandle(this, pid, openedHandle, handle);
    if (replyData->err != 0)
        return reply;

    ProcessGroup *group = g_posixService.findProcessGroupForProcess(handle);
    replyData->sid = group->session()->id();

    return reply;
}

Kiwi::Core::Message Process::handleSetsid(const Kiwi::Core::Message &request) {
    Kiwi::Core::Message reply;
    if (!reply.createReply(request, sizeof(posix_reply_setsid_t))) {
        core_log(CORE_LOG_WARN, "failed to allocate reply message");
        return Kiwi::Core::Message();
    }

    auto replyData = reply.data<posix_reply_setsid_t>();
    replyData->err = 0;

    if (request.size() != 0) {
        replyData->err = EINVAL;
        return reply;
    }

    /* Not allowed to create a new session if there's a group with our ID. */
    if (g_posixService.findProcessGroup(m_id)) {
        replyData->err = EPERM;
        return reply;
    }

    ProcessGroup *currentGroup = g_posixService.findProcessGroupForProcess(m_handle);

    Session *session = g_posixService.createSession(m_id);

    ProcessGroup *newGroup = g_posixService.createProcessGroup(m_id, session, m_handle);
    if (!newGroup) {
        /* Group destructor will have destroyed the session. */
        replyData->err = EAGAIN;
        return reply;
    }

    currentGroup->removeProcess(m_handle);

    replyData->sid = m_id;
    return reply;
}
