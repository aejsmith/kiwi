/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               POSIX service.
 */

#pragma once

#include <core/log.h>

#include <kiwi/core/event_loop.h>
#include <kiwi/core/handle.h>

#include <memory>
#include <unordered_map>

class Process;
class ProcessGroup;
class Session;

/** Define to enable debug output from the POSIX service. */
//#define DEBUG_POSIX_SERVICE     

#ifdef DEBUG_POSIX_SERVICE
#   define debug_log(fmt...)    core_log(CORE_LOG_DEBUG, fmt)
#else
#   define debug_log(fmt...)
#endif

class PosixService {
public:
    PosixService();
    ~PosixService();

    Kiwi::Core::EventLoop &eventLoop() { return m_eventLoop; }

    int run();

    Process *findProcess(pid_t pid) const;
    void removeProcess(Process *process);

    ProcessGroup *createProcessGroup(pid_t pgid, Session *session, handle_t leader);
    ProcessGroup *findProcessGroup(pid_t pgid) const;
    ProcessGroup *findProcessGroupForProcess(handle_t handle) const;
    void removeProcessGroup(ProcessGroup *group);

    Session *createSession(pid_t sid);
    Session *findSession(pid_t sid);
    void removeSession(Session *session);

    int openProcess(pid_t pid, Kiwi::Core::Handle &handle) const;
    int getProcessHandle(pid_t pid, Kiwi::Core::Handle &openedHandle, handle_t &handle) const;

private:
    void handleConnectionEvent();

private:
    Kiwi::Core::Handle m_port;
    Kiwi::Core::EventLoop m_eventLoop;

    std::unordered_map<pid_t, std::unique_ptr<Process>> m_processes;
    std::unordered_map<pid_t, std::unique_ptr<ProcessGroup>> m_processGroups;
    std::unordered_map<pid_t, std::unique_ptr<Session>> m_sessions;

    Kiwi::Core::EventRef m_connectionEvent;
};

extern PosixService g_posixService;
