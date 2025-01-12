/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               POSIX session class.
 */

#pragma once

#include <vector>

class ProcessGroup;

class Session {
public:
    explicit Session(pid_t id);
    ~Session();

    pid_t id() const                           { return m_id; }
    const Kiwi::Core::Handle &terminal() const { return m_terminal; }

    void addProcessGroup(ProcessGroup *group);
    void removeProcessGroup(ProcessGroup *group);

    void setTerminal(Kiwi::Core::Handle handle);

private:
    pid_t m_id;                             /**< ID of the session. */

    std::vector<ProcessGroup *> m_groups;   /**< Groups in this session. */
    Kiwi::Core::Handle m_terminal;          /**< Controlling terminal. */
};
