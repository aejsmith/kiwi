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
 * @brief               Network socket implementation.
 */

#include <net/interface.h>
#include <net/socket.h>

static status_t setsockopt_bindtointerface(net_socket_t *socket, const void *opt_value, socklen_t opt_len) {
    // TODO: Should this be a privileged option?

    if (opt_len != sizeof(uint32_t))
        return STATUS_INVALID_ARG;

    /* No need to validate. It will be looked up when sending and will fail if
     * it is invalid, and we just won't receive anything. */
    uint32_t interface_id = *(const uint32_t *)opt_value;
    socket->bound_interface_id = interface_id;

    return STATUS_SUCCESS;
}

static status_t getsockopt_error(
    net_socket_t *socket, socklen_t max_len, void *_opt_value,
    socklen_t *_opt_len)
{
    // TODO: Implement this, we should report conditions like TCP socket reset.
    // This should clear the pending error value. We'll want this to return a
    // status_t and translate to errno in libsystem.

    if (max_len != sizeof(int))
        return STATUS_INVALID_ARG;

    *(int *)_opt_value = 0;
    *_opt_len          = sizeof(int);

    return STATUS_SUCCESS;
}

/** Base implementation of getsockopt() for network sockets. */
status_t net_socket_getsockopt(
    socket_t *_socket, int level, int opt_name, socklen_t max_len,
    void *_opt_value, socklen_t *_opt_len)
{
    net_socket_t *socket = cast_net_socket(_socket);

    status_t ret = STATUS_UNKNOWN_SOCKET_OPT;

    if (level == SOL_SOCKET) {
        switch (opt_name) {
            case SO_ERROR:
                ret = getsockopt_error(socket, max_len, _opt_value, _opt_len);
                break;
        }
    }

    return ret;
}

/** Base implementation of setsockopt() for network sockets. */
status_t net_socket_setsockopt(
    socket_t *_socket, int level, int opt_name, const void *opt_value,
    socklen_t opt_len)
{
    net_socket_t *socket = cast_net_socket(_socket);

    status_t ret = STATUS_UNKNOWN_SOCKET_OPT;

    if (level == SOL_SOCKET) {
        switch (opt_name) {
            case SO_BINDTOINTERFACE:
                ret = setsockopt_bindtointerface(socket, opt_value, opt_len);
                break;
        }
    }

    return ret;
}
