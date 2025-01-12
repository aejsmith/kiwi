/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               POSIX session class.
 */

#include "posix_service.h"
#include "process_group.h"
#include "session.h"

#include <assert.h>

Session::Session(pid_t id) :
    m_id (id)
{
    debug_log("created session %" PRId32, id);
}

Session::~Session() {}

void Session::addProcessGroup(ProcessGroup *group) {
    m_groups.emplace_back(group);
}

void Session::removeProcessGroup(ProcessGroup *group) {
    for (auto it = m_groups.begin(), end = m_groups.end(); it != end; ++it) {
        if (*it == group) {
            m_groups.erase(it);

            if (m_groups.empty()) {
                debug_log("session %" PRId32 " died", m_id);

                /* This destroys the Session. */
                g_posixService.removeSession(this);
            }

            return;
        }
    }
}

void Session::setTerminal(Kiwi::Core::Handle handle) {
    m_terminal = std::move(handle);
}
