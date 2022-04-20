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
 * @brief               POSIX service.
 *
 * The POSIX service is responsible for implementing POSIX functionality that
 * does not exist and does not belong in the kernel, and cannot be implemented
 * locally to a single process. For example, we implement POSIX process groups,
 * sessions and signals through this service, built on top of lower-level
 * kernel functionality. This avoids polluting the kernel with legacy POSIX
 * details it shouldn't need to care about like terminals (which are also
 * implemented via a userspace service).
 */

#include "posix_service.h"
#include "process.h"
#include "process_group.h"
#include "session.h"

#include <core/log.h>
#include <core/service.h>

#include <kernel/object.h>
#include <kernel/process.h>
#include <kernel/status.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

PosixService g_posixService;

PosixService::PosixService() {}

PosixService::~PosixService() {}

int PosixService::run() {
    status_t ret;

    ret = kern_port_create(m_port.attach());
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to create port: %" PRId32, ret);
        return EXIT_FAILURE;
    }

    ret = core_service_register_port(m_port);
    if (ret != STATUS_SUCCESS) {
        core_log(CORE_LOG_ERROR, "failed to register port: %" PRId32, ret);
        return EXIT_FAILURE;
    }

    m_connectionEvent = m_eventLoop.addEvent(
        m_port, PORT_EVENT_CONNECTION, 0,
        [this] (const object_event_t &event) { handleConnectionEvent(); });

    /* Create default process group and session. Don't call init() on the group,
     * this creates the kernel group and opens the leader, which we don't want
     * for the default group. */
    auto defaultSession = std::make_unique<Session>(kDefaultProcessGroupId);
    auto defaultGroup   = std::make_unique<ProcessGroup>(kDefaultProcessGroupId, defaultSession.get());

    m_sessions.emplace(defaultSession->id(), std::move(defaultSession));
    m_processGroups.emplace(defaultGroup->id(), std::move(defaultGroup));

    while (true) {
        m_eventLoop.wait();
    }

    return EXIT_SUCCESS;
}

Process *PosixService::findProcess(pid_t pid) const {
    auto ret = m_processes.find(pid);
    return (ret != m_processes.end()) ? ret->second.get() : nullptr;
}

void PosixService::removeProcess(Process *process) {
    auto ret = m_processes.find(process->id());

    assert(ret != m_processes.end());
    assert(ret->second.get() == process);

    m_processes.erase(ret);
}

/** Creates a new process group and adds the leader to it. */
ProcessGroup *PosixService::createProcessGroup(pid_t pgid, Session *session, handle_t leader) {
    auto group = std::make_unique<ProcessGroup>(pgid, session);
    if (!group->init(leader))
        return nullptr;

    auto ret = m_processGroups.emplace(pgid, std::move(group));
    assert(ret.second);
    return ret.first->second.get();
}

ProcessGroup *PosixService::findProcessGroup(pid_t pgid) const {
    auto ret = m_processGroups.find(pgid);
    return (ret != m_processGroups.end()) ? ret->second.get() : nullptr;
}

/**
 * Finds the process group that a process handle belongs to. Always returns a
 * group, will be the default group if the process is not a member of any other
 * group.
 */
ProcessGroup *PosixService::findProcessGroupForProcess(handle_t handle) const {
    ProcessGroup *defaultGroup = nullptr;

    for (const auto &it : m_processGroups) {
        if (it.second->id() == kDefaultProcessGroupId) {
            defaultGroup = it.second.get();
        } else if (it.second->containsProcess(handle)) {
            return it.second.get();
        }
    }

    /* We've iterated all groups so should have found this. */
    assert(defaultGroup);
    return defaultGroup;
}

void PosixService::removeProcessGroup(ProcessGroup *group) {
    auto ret = m_processGroups.find(group->id());

    assert(ret != m_processGroups.end());
    assert(ret->second.get() == group);

    m_processGroups.erase(ret);
}

Session *PosixService::createSession(pid_t sid) {
    auto session = std::make_unique<Session>(sid);

    auto ret = m_sessions.emplace(sid, std::move(session));
    assert(ret.second);
    return ret.first->second.get();
}

Session *PosixService::findSession(pid_t sid) {
    auto ret = m_sessions.find(sid);
    return (ret != m_sessions.end()) ? ret->second.get() : nullptr;
}

void PosixService::removeSession(Session *session) {
    auto ret = m_sessions.find(session->id());

    assert(ret != m_sessions.end());
    assert(ret->second.get() == session);

    m_sessions.erase(ret);
}

void PosixService::handleConnectionEvent() {
    status_t ret;

    Kiwi::Core::Handle handle;
    ret = kern_port_listen(m_port, 0, handle.attach());
    if (ret != STATUS_SUCCESS) {
        /* This may be harmless - client's connection attempt could be cancelled
         * between us receiving the event and calling listen, for instance. */
        if (ret != STATUS_WOULD_BLOCK)
            core_log(CORE_LOG_WARN, "failed to listen on port after connection event: %" PRId32, ret);

        return;
    }

    process_id_t pid;
    Kiwi::Core::Handle processHandle;
    ret = kern_connection_open_remote(handle, processHandle.attach());
    if (ret == STATUS_SUCCESS) {
        ret = kern_process_id(processHandle, &pid);
        if (ret != STATUS_SUCCESS)
            core_log(CORE_LOG_WARN, "failed to get client process ID: %" PRId32, ret);
    } else {
        core_log(CORE_LOG_WARN, "failed to open client process handle: %" PRId32, ret);
    }

    if (ret != STATUS_SUCCESS)
        return;

    Kiwi::Core::Connection connection;
    if (!connection.create(
            std::move(handle),
            Kiwi::Core::Connection::kReceiveRequests | Kiwi::Core::Connection::kReceiveSecurity))
    {
        core_log(CORE_LOG_WARN, "failed to create connection");
        return;
    }

    /* Look for an existing process. */
    auto it = m_processes.find(pid);
    if (it != m_processes.end()) {
        /* This may be a reconnection after an exec() which we want to handle. */
        it->second->reconnect(std::move(connection));
    } else {
        auto process = std::make_unique<Process>(std::move(connection), std::move(processHandle), pid);
        m_processes.emplace(pid, std::move(process));
    }
}

int PosixService::openProcess(pid_t pid, Kiwi::Core::Handle &handle) const {
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

int PosixService::getProcessHandle(pid_t pid, Kiwi::Core::Handle &openedHandle, handle_t &handle) const {
    Process *process = findProcess(pid);
    if (process) {
        handle = process->handle();
    } else {
        int err = openProcess(pid, openedHandle);
        if (err != 0)
            return err;

        handle = openedHandle;
    }

    return 0;
}

int main(int argc, char **argv) {
    return g_posixService.run();
}
