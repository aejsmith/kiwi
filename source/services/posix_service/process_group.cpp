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
 * @brief               POSIX process group class.
 */

#include "posix_service.h"
#include "process_group.h"
#include "session.h"

#include <core/log.h>

#include <kernel/process_group.h>
#include <kernel/status.h>

#include <assert.h>
#include <errno.h>

ProcessGroup::ProcessGroup(pid_t id, Session *session) :
    m_id        (id),
    m_session   (session)
{
    debug_log("created process group %" PRId32 " in session %" PRId32, m_id, session->id());

    m_session->addProcessGroup(this);
}

ProcessGroup::~ProcessGroup() {
    m_session->removeProcessGroup(this);
}

bool ProcessGroup::init(handle_t leader) {
    status_t ret;

    /* Create a duplicate of the leader handle that we own, the one we're given
     * won't necessarily live as long as the group. */
    ret = kern_handle_duplicate(HANDLE_DUPLICATE_ALLOCATE, leader, INVALID_HANDLE, m_leader.attach());
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_WARN, "failed to duplicate leader handle: %" PRId32, ret);
        return false;
    }

    ret = kern_process_group_create(PROCESS_GROUP_INHERIT_MEMBERSHIP, m_handle.attach());
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_WARN, "failed to create process group: %" PRId32, ret);
        return false;
    }

    m_deathEvent = g_posixService.eventLoop().addEvent(
        m_handle, PROCESS_GROUP_EVENT_DEATH, 0,
        [this] (const object_event_t &) { handleDeathEvent(); });

    addProcess(leader);

    return true;
}

void ProcessGroup::handleDeathEvent() {
    debug_log("process group %" PRId32 " died", m_id);

    /* This fires when there are no more running processes in the group, which
     * means we can remove the group. This will free the ProcessGroup. */
    g_posixService.removeProcessGroup(this);
}

bool ProcessGroup::containsProcess(handle_t process) const {
    /* This returns false for the default group, which has no group object. */
    return (m_handle.isValid())
        ? kern_process_group_query(m_handle, process) == STATUS_SUCCESS
        : false;
}

void ProcessGroup::addProcess(handle_t handle) {
    if (m_handle.isValid()) {
        status_t ret __sys_unused = kern_process_group_add(m_handle, handle);
        assert(ret == STATUS_SUCCESS || ret == STATUS_NOT_RUNNING);
    }
}

void ProcessGroup::removeProcess(handle_t handle) {
    if (m_handle.isValid()) {
        /* May be NOT_FOUND if we failed to add because the process is dead. */
        status_t ret __sys_unused = kern_process_group_remove(m_handle, handle);
        assert(ret == STATUS_SUCCESS || ret == STATUS_NOT_FOUND);
    }
}

bool ProcessGroup::forEachProcess(const std::function<void (handle_t, pid_t)> &func) {
    status_t ret;

    /* Shouldn't call on default group. */
    assert(m_handle.isValid());

    size_t procCount = 0;
    ret = kern_process_group_enumerate(m_handle, nullptr, &procCount);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_WARN, "failed to enumerate process group: %" PRId32, ret);
        return false;
    }

    if (procCount > 0) {
        std::vector<process_id_t> ids;
        ids.resize(procCount);

        ret = kern_process_group_enumerate(m_handle, ids.data(), &procCount);
        if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_WARN, "failed to enumerate process group: %" PRId32, ret);
            return false;
        }

        ids.resize(std::min(procCount, ids.size()));

        for (process_id_t id : ids) {
            Kiwi::Core::Handle openedHandle;
            handle_t handle;
            int err = g_posixService.getProcessHandle(id, openedHandle, handle);

            /* Ignore ESRCH in case the process died between enumerate and open. */
            if (err != 0 && err != ESRCH)
                return false;

            /* Recheck membership in case this is a new process that recycled
             * the PID in between enumerate and open. */
            if (!containsProcess(handle))
                continue;

            func(handle, id);
        }
    }

    return true;
}
