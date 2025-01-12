/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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

#include <optional>

class ProcessGroup;

struct SignalState {
    /** Signal action. */
    uint32_t disposition    = POSIX_SIGNAL_DISPOSITION_DEFAULT;
    uint32_t flags          = 0;

    /** Pending signal information. */
    siginfo_t info          = {};
};

class Process {
public:
    Process(Kiwi::Core::Connection connection, Kiwi::Core::Handle handle, pid_t id);
    ~Process();

    pid_t id() const        { return m_id; }
    handle_t handle() const { return m_handle; }

    void reconnect(Kiwi::Core::Connection connection);

private:
    void initConnection();

    void handleDeathEvent();
    void handleHangupEvent();
    void handleMessageEvent();
    void handleAlarmEvent();

    Kiwi::Core::Message handleGetSignalCondition(Kiwi::Core::Message &request);
    Kiwi::Core::Message handleGetPendingSignal(Kiwi::Core::Message &request);
    Kiwi::Core::Message handleSetSignalAction(Kiwi::Core::Message &request);
    Kiwi::Core::Message handleSetSignalMask(Kiwi::Core::Message &request);
    Kiwi::Core::Message handleKill(Kiwi::Core::Message &request);
    Kiwi::Core::Message handleAlarm(Kiwi::Core::Message &request);
    Kiwi::Core::Message handleGetpgid(Kiwi::Core::Message &request);
    Kiwi::Core::Message handleSetpgid(Kiwi::Core::Message &request);
    Kiwi::Core::Message handleGetsid(Kiwi::Core::Message &request);
    Kiwi::Core::Message handleSetsid(Kiwi::Core::Message &request);
    Kiwi::Core::Message handleGetPgrpSession(Kiwi::Core::Message &request);
    Kiwi::Core::Message handleSetSessionTerminal(Kiwi::Core::Message &request);
    Kiwi::Core::Message handleGetTerminal(Kiwi::Core::Message &request);

    uint32_t signalsDeliverable() const;
    void updateSignals();
    void sendSignal(int32_t num, const Process *sender, const security_context_t *senderSecurity);

    bool isTerminalService();

private:
    Kiwi::Core::Connection m_connection;
    Kiwi::Core::Handle m_handle;
    pid_t m_id;
    std::optional<bool> m_isTerminalService;

    SignalState m_signals[NSIG];
    uint32_t m_signalsPending;
    uint32_t m_signalMask;
    Kiwi::Core::Handle m_signalCondition;
    Kiwi::Core::Handle m_alarmTimer;

    Kiwi::Core::EventRef m_deathEvent;
    Kiwi::Core::EventRef m_hangupEvent;
    Kiwi::Core::EventRef m_messageEvent;
    Kiwi::Core::EventRef m_alarmEvent;
};
