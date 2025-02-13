/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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

    /** Socket options. */
    uint32_t bound_interface_id;        /**< SO_BINDTOINTERFACE. */
} net_socket_t;

DEFINE_CLASS_CAST(net_socket, socket, socket);

extern status_t net_socket_getsockopt(
    socket_t *socket, int level, int opt_name, socklen_t max_len,
    void *_opt_value, socklen_t *_opt_len);
extern status_t net_socket_setsockopt(
    socket_t *socket, int level, int opt_name, const void *opt_value,
    socklen_t opt_len);

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
static inline status_t net_socket_route(net_socket_t *socket, const sockaddr_t *dest_addr, struct net_route *route) {
    return socket->family->socket_route(socket, dest_addr, route);
}

/** Transmits a packet on the socket using the address family.
 * @see                 net_family_t::transmit */
static inline status_t net_socket_transmit(
    net_socket_t *socket, struct net_packet *packet,
    const struct net_route *route)
{
    return socket->family->socket_transmit(socket, packet, route);
}
