/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Network address families.
 */

#pragma once

#include <kernel/net/ipv4.h>
#include <kernel/net/ipv6.h>
#include <kernel/socket.h>

struct net_interface;
struct net_packet;
struct net_route;
struct net_socket;
union net_interface_addr;

/**
 * Single network address structure. This is used where we need a generic space
 * to store an address of any supported family.
 */
typedef struct net_addr {
    sa_family_t family;
    union {
        net_addr_ipv4_t ipv4;
        net_addr_ipv6_t ipv6;
    };
} net_addr_t;

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

    /**
     * Interface address operations.
     */

    /** Checks if an interface address is valid. */
    bool (*interface_addr_valid)(const union net_interface_addr *addr);

    /** Checks if two network interface addresses are equal. */
    bool (*interface_addr_equal)(const union net_interface_addr *a, const union net_interface_addr *b);

    /**
     * Interface operations.
     */

    /** Called when an interface is being removed.
     * @param interface     Interface being removed. */
    void (*interface_remove)(struct net_interface *interface);

    /** Called when an address is added to an interface.
     * @param interface     Interface being added to.
     * @param addr          Address being added. */
    void (*interface_add_addr)(struct net_interface *interface, const union net_interface_addr *addr);

    /** Called when an address is removed from an interface.
     * @param interface     Interface being removed from.
     * @param addr          Address being removed. */
    void (*interface_remove_addr)(struct net_interface *interface, const union net_interface_addr *addr);

    /**
     * Socket operations.
     */

    /** Determines a route for a packet.
     * @param socket        Socket to route for.
     * @param dest_addr     Destination address (must be valid).
     * @param route         Where to store route information.
     * @return              Status code describing result of the operation. */
    status_t (*socket_route)(
        struct net_socket *socket, const sockaddr_t *dest_addr,
        struct net_route *route);

    /**
     * Transmits a packet on the socket using the address family. This function
     * will add a reference to the packet if necessary so the caller should
     * release its reference.
     *
     * @param socket        Socket to transmit on.
     * @param packet        Packet to transmit.
     * @param route         Route for the packet.
     *
     * @return              Status code describing result of the operation.
     */
    status_t (*socket_transmit)(
        struct net_socket *socket, struct net_packet *packet,
        const struct net_route *route);
} net_family_t;

extern const net_family_t *net_family_get(sa_family_t id);
