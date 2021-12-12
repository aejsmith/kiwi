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

struct net_interface;
struct net_packet;
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

    /** Determines a route (interface and source address) for a packet.
     * @param socket        Socket to route for.
     * @param dest_addr     Destination address (must be valid)
     * @param _interface_id Where to return ID of interface to transmit on.
     * @param _source_addr  Where to return source address (must be sized for
     *                      the family).
     * @return              Status code describing result of the operation. */
    status_t (*route)(
        struct net_socket *socket, const sockaddr_t *dest_addr,
        uint32_t *_interface_id, sockaddr_t *_source_addr);

    /**
     * Transmits a packet on the socket using the address family. This function
     * will add a reference to the packet if necessary so the caller should
     * release its reference.
     *
     * @param socket        Socket to transmit on.
     * @param packet        Packet to transmit.
     * @param interface_id  ID of interface to transmit on.
     * @param source_addr   Source address.
     * @param dest_addr     Destination address.
     *
     * @return              Status code describing result of the operation.
     */
    status_t (*transmit)(
        struct net_socket *socket, struct net_packet *packet,
        uint32_t interface_id, const sockaddr_t *source_addr,
        const sockaddr_t *dest_addr);
} net_family_ops_t;

/** Network socket structure. */
typedef struct net_socket {
    socket_t socket;                    /**< Socket header. */
    const net_family_ops_t *family_ops; /**< Family operations. */
    int protocol;                       /**< Family-specific protocol number. */
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

/** Helper to return socket addresses. */
static inline void net_socket_addr_copy(
    const net_socket_t *socket, const sockaddr_t *addr, socklen_t max_addr_len,
    sockaddr_t *_addr, socklen_t *_addr_len)
{
    if (_addr_len)
        *_addr_len = socket->family_ops->addr_len;
    if (_addr)
        memcpy(_addr, addr, min(max_addr_len, socket->family_ops->addr_len));
}

/** Determines a route (interface and source address) for a packet.
 * @see                 net_family_ops_t::route */
static inline status_t net_socket_route(
    net_socket_t *socket, const sockaddr_t *dest_addr, uint32_t *_interface_id,
    sockaddr_t *_source_addr)
{
    return socket->family_ops->route(socket, dest_addr, _interface_id, _source_addr);
}

/** Transmits a packet on the socket using the address family.
 * @see                 net_family_ops_t::transmit */
static inline status_t net_socket_transmit(
    net_socket_t *socket, struct net_packet *packet, uint32_t interface_id,
    const sockaddr_t *source_addr, const sockaddr_t *dest_addr)
{
    return socket->family_ops->transmit(socket, packet, interface_id, source_addr, dest_addr);
}
