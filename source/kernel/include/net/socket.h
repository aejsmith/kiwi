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

#include <net/family.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <status.h>

/** Network socket structure. */
typedef struct net_socket {
    socket_t socket;                    /**< Socket header. */
    const net_family_t *family;         /**< Address family. */
    int protocol;                       /**< Family-specific protocol number. */
} net_socket_t;

DEFINE_CLASS_CAST(net_socket, socket, socket);

/** Checks if an address is valid for the given socket.
 * @param socket        Socket to check for.
 * @param addr          Address to check.
 * @param addr_len      Specified address length. This must be equal to the
 *                      family's address length to be valid.
 * @return              Status code describing result of the check. */
static inline status_t net_socket_addr_valid(const net_socket_t *socket, const sockaddr_t *addr, socklen_t addr_len) {
    if (addr_len != socket->family->socket_addr_len) {
        return STATUS_INVALID_ARG;
    } else if (addr->sa_family != socket->socket.family) {
        return STATUS_ADDR_NOT_SUPPORTED;
    }

    return STATUS_SUCCESS;
}

/** Helper to return socket addresses in receive() implementations. */
static inline void net_socket_addr_copy(
    const net_socket_t *socket, const sockaddr_t *addr, socklen_t max_addr_len,
    sockaddr_t *_addr, socklen_t *_addr_len)
{
    if (_addr_len)
        *_addr_len = socket->family->socket_addr_len;
    if (_addr)
        memcpy(_addr, addr, min(max_addr_len, socket->family->socket_addr_len));
}

/** Determines a route (interface and source address) for a packet.
 * @see                 net_family_t::route */
static inline status_t net_socket_route(
    net_socket_t *socket, const sockaddr_t *dest_addr, uint32_t *_interface_id,
    sockaddr_t *_source_addr)
{
    return socket->family->route(socket, dest_addr, _interface_id, _source_addr);
}

/** Transmits a packet on the socket using the address family.
 * @see                 net_family_t::transmit */
static inline status_t net_socket_transmit(
    net_socket_t *socket, struct net_packet *packet, uint32_t interface_id,
    const sockaddr_t *source_addr, const sockaddr_t *dest_addr)
{
    return socket->family->transmit(socket, packet, interface_id, source_addr, dest_addr);
}
