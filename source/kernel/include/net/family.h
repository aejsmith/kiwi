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
 * @brief               Network address families.
 */

#pragma once

#include <kernel/net/family.h>
#include <kernel/socket.h>

struct net_packet;
struct net_socket;
union net_interface_addr;

/** Network address family properties/operations. */
typedef struct net_family {
    /**
     * MTU (maximum payload size) of packets for the address family. This is
     * currently static since we don't use IPv4 options, but may need to change
     * to a dynamic method in future.
     */
    size_t mtu;

    /** Interface address length for this family. */
    size_t interface_addr_len;

    /** Socket address length for this family. */
    socklen_t socket_addr_len;

    /** Check if an interface address is valid. */
    bool (*interface_addr_valid)(const union net_interface_addr *addr);

    /** Check if two network interface addresses are equal. */
    bool (*interface_addr_equal)(const union net_interface_addr *a, const union net_interface_addr *b);

    /** Determines a route (interface and source address) for a packet.
     * @param socket        Socket to route for.
     * @param dest_addr     Destination address (must be valid)
     * @param _interface_id Where to  return ID of interface to transmit on.
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
} net_family_t;

extern const net_family_t *net_family_get(sa_family_t id);
