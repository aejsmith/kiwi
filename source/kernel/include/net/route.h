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
