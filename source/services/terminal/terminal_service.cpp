/*
 * Copyright (C) 2009-2020 Alex Smith
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

#include <kernel/object.h>
#include <kernel/status.h>

#include <inttypes.h>
#include <stdlib.h>

TerminalService::TerminalService() {}

TerminalService::~TerminalService() {
    if (m_port != INVALID_HANDLE)
        kern_handle_close(m_port);
}

int TerminalService::run() {
    status_t ret;

    ret = kern_port_create(&m_port);
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
        handle_t handle;
        ret = kern_port_listen(m_port, nullptr, -1, &handle);
        if (ret != STATUS_SUCCESS) {
            core_log(CORE_LOG_ERROR, "failed to listen on port: %" PRId32, ret);
            continue;
        }

        core_connection_t *connection = core_connection_create(handle, CORE_CONNECTION_RECEIVE_REQUESTS);
        if (!connection) {
            core_log(CORE_LOG_WARN, "failed to create connection");
            kern_handle_close(handle);
            continue;
        }

        /* Each connection (terminal) runs in its own thread. */
        Terminal *terminal = new Terminal(connection);
        terminal->run();
    }

    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    return g_terminalService.run();
}
