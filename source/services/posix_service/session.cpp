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
