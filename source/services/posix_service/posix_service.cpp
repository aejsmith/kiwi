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

#include <core/log.h>
#include <core/service.h>

#include <kernel/object.h>
#include <kernel/process.h>
#include <kernel/status.h>

#include <assert.h>
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

    while (true) {
        m_eventLoop.wait();
    }

    return EXIT_SUCCESS;
}

void PosixService::removeProcess(Process *process) {
    auto ret = m_processes.find(process->pid());

    assert(ret != m_processes.end());
    assert(ret->second.get() == process);

    m_processes.erase(ret);
}

void PosixService::handleConnectionEvent() {
    status_t ret;

    Kiwi::Core::Handle handle;
    ret = kern_port_listen(m_port, 0, handle.attach());
    if (ret != STATUS_SUCCESS) {
        /* This may be harmless - client's connection attempt could be cancelled
         * between us receiving the event and calling listen, for instance. */
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

    /* Look for an existing process. */
    auto it = m_processes.find(pid);
    if (it != m_processes.end()) {
        core_log(CORE_LOG_NOTICE, "ignoring connection from already connected process %" PRId32);
        return;
    }

    Kiwi::Core::Connection connection;
    if (!connection.create(std::move(handle), CORE_CONNECTION_RECEIVE_REQUESTS)) {
        core_log(CORE_LOG_WARN, "failed to create connection");
        return;
    }

    auto process = std::make_unique<Process>(std::move(connection), std::move(processHandle), pid);
    m_processes.emplace(pid, std::move(process));
}

int main(int argc, char **argv) {
    return g_posixService.run();
}
