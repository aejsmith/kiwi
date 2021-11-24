/*
 * Copyright (C) 2009-2021 Alex Smith
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

#pragma once

#include <io/socket.h>

#include <lib/utility.h>

#include <status.h>

struct net_socket;

/** Network socket address family operations. */
typedef struct net_family_ops {
    /**
     * MTU (maximum payload size) of packets for the address family. This is
     * currently static since we don't use IPv4 options, but may need to change
     * to a dynamic method in future.
     */
    size_t mtu;

    /** Socket address length for this family. */
    socklen_t addr_len;

    /** Gets the port from an address.
     * @param socket        Socket to get for.
     * @param addr          Address to get from (must be valid).
     * @return              Port number (in network byte order). */
    uint16_t (*addr_port)(const struct net_socket *socket, const sockaddr_t *addr);
} net_family_ops_t;

/** Network socket structure. */
typedef struct net_socket {
    socket_t socket;
    const net_family_ops_t *family_ops;
} net_socket_t;

DEFINE_CLASS_CAST(net_socket, socket, socket);

/** Checks if an address is valid for the given socket.
 * @param socket        Socket to check for.
 * @param addr          Address to check.
 * @param addr_len      Specified address length.
 * @return              Status code describing result of the check. */
static inline status_t net_socket_addr_valid(const net_socket_t *socket, const sockaddr_t *addr, socklen_t addr_len) {
    if (addr_len != socket->family_ops->addr_len) {
        return STATUS_INVALID_ARG;
    } else if (addr->sa_family != socket->socket.family) {
        return STATUS_ADDR_NOT_SUPPORTED;
    }

    return STATUS_SUCCESS;
}
