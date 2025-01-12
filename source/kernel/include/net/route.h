/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Network route structure.
 */

#pragma once

#include <net/family.h>

/** Structure describing the route for a packet. */
typedef struct net_route {
    /** Interface to send the packet on. */
    uint32_t interface_id;

    /** Source address for the packet (one of the interface's addresses). */
    net_addr_t source_addr;

    /** Final destination address for the packet. */
    net_addr_t dest_addr;

    /**
     * Gateway address to send the packet to. If the destination is directly
     * reachable, then this will be the same as the destination address,
     * otherwise it is the gateway to send the packet to which is responsible
     * for forwarding the packet to its destination.
     */
    net_addr_t gateway_addr;
} net_route_t;
