/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Terminal service.
 *
 * The terminal service provides an implementation of POSIX-style pseudo
 * terminals. On the master side, usage is not the same as a PTY (everything is
 * done over an IPC interface), but the slave side looks like a POSIX terminal
 * (implemented via a user file).
 */

#include "terminal_service.h"
#include "terminal.h"

#include <core/log.h>
#include <core/service.h>

#include <kernel/status.h>

#include <stdlib.h>

TerminalService g_terminalService;

TerminalService::TerminalService() :
    m_nextTerminalId (0)
{}

TerminalService::~TerminalService() {}

int TerminalService::run() {
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

    while (true) {
        Kiwi::Core::Handle handle;
        ret = kern_port_listen(m_port, -1, handle.attach());
        if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_ERROR, "failed to listen on port: %" PRId32, ret);
            continue;
        }

        Kiwi::Core::Connection connection;
        if (!connection.create(std::move(handle), Kiwi::Core::Connection::kReceiveRequests)) {
            core_log(CORE_LOG_WARN, "failed to create connection");
            continue;
        }

        /* Each connection (terminal) runs in its own thread. */
        size_t id = m_nextTerminalId++;
        Terminal *terminal = new Terminal(id, std::move(connection));
        terminal->run();
    }

    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    return g_terminalService.run();
}
