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

#pragma once

#include <kiwi/core/connection.h>
#include <kiwi/core/event_loop.h>
#include <kiwi/core/handle.h>

#include <services/posix_service.h>

#include <signal.h>

struct SignalState {
    /** Signal action. */
    uint32_t disposition    = POSIX_SIGNAL_DISPOSITION_DEFAULT;
    uint32_t flags          = 0;

    /** Pending signal information. */
    siginfo_t info          = {};
};

class Process {
public:
    Process(Kiwi::Core::Connection connection, Kiwi::Core::Handle handle, process_id_t pid);
    ~Process();

    process_id_t pid() const { return m_pid; }

private:
    void handleHangupEvent();
    void handleMessageEvent();

    Kiwi::Core::Message handleGetSignalCondition(const Kiwi::Core::Message &request);
    Kiwi::Core::Message handleGetPendingSignal(const Kiwi::Core::Message &request);
    Kiwi::Core::Message handleSetSignalAction(const Kiwi::Core::Message &request);
    Kiwi::Core::Message handleKill(const Kiwi::Core::Message &request);

    uint32_t signalsDeliverable() const;
    void updateSignalCondition();
    int32_t sendSignal(int32_t num, const Process *sender, const security_context_t *senderSecurity);

private:
    Kiwi::Core::Connection m_connection;
    Kiwi::Core::Handle m_handle;
    process_id_t m_pid;

    SignalState m_signals[NSIG];
    uint32_t m_signalsPending;
    uint32_t m_signalMask;
    Kiwi::Core::Handle m_signalCondition;

    Kiwi::Core::EventRef m_hangupEvent;
    Kiwi::Core::EventRef m_messageEvent;
};
