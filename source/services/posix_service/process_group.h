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

#pragma once

#include <kiwi/core/event_loop.h>
#include <kiwi/core/handle.h>

#include <sys/types.h>

#include <functional>
#include <memory>

class Session;

/**
 * Processes that have not explicitly had a process group set and are not a
 * child of such a process either will not belong to any of our kernel process
 * groups.
 *
 * POSIX requires that all processes belong to a process group, therefore all
 * processes that do not have a known process group will be attributed to
 * process group 1. This can be safely reserved since PID 1 is always the
 * service manager, which is a native process that will not use POSIX process
 * group APIs.
 *
 * Since this group effectively contains all processes that are untracked by
 * one of our kernel process groups, we cannot enumerate all the processes in
 * it. Therefore, operations that target all processes in the group (e.g.
 * signals) will fail.
 */
static constexpr pid_t kDefaultProcessGroupId = 1;

class ProcessGroup {
public:
    ProcessGroup(pid_t id, Session *session);
    ~ProcessGroup();

    pid_t id() const            { return m_id; }
    Session *session() const    { return m_session; }

    bool init(handle_t leader);

    bool containsProcess(handle_t handle) const;

    void addProcess(handle_t handle);
    void removeProcess(handle_t handle);

    bool forEachProcess(const std::function<void (handle_t, pid_t)> &func);

private:
    void handleDeathEvent();

private:
    pid_t m_id;                     /**< ID of the group. */
    Session *m_session;             /**< Session the group is in. */
    Kiwi::Core::Handle m_handle;    /**< Kernel process group. */

    /**
     * Process group leader handle. This is the process from which the group
     * takes its ID. We keep a handle to it while any process still exists in
     * the group, which prevents the ID from being recycled by the kernel and
     * therefore avoids the possibility of a new process being created with that
     * ID outside the process group.
     */
    Kiwi::Core::Handle m_leader;

    Kiwi::Core::EventRef m_deathEvent;
};
