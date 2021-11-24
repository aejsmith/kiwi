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
 * @brief               Network interface management.
 */

#pragma once

#include <kernel/net/interface.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/ipv6.h>

#include <lib/array.h>

#include <sync/mutex.h>

struct net_packet;

/**
 * Address assigned to a network interface. This is a kernel-internal union of
 * all the supported address structures, which all start with a family member.
 * The overall union is not exposed to userspace, which allows flexibility of
 * adding new families with different (possibly larger) address structures,
 * without breaking ABI compatibility.
 */
typedef union net_addr {
    sa_family_t family;                 /**< Address family this is for. */
    net_addr_ipv4_t ipv4;               /**< AF_INET. */
    net_addr_ipv6_t ipv6;               /**< AF_INET6. */
} net_addr_t;

/** Operations for handling network interface addresses. */
typedef struct net_addr_ops {
    size_t len;                         /**< Length of the address structure. */

    /** Check if an interface address is valid. */
    bool (*valid)(const net_addr_t *addr);

    /** Check if two network interface addresses are equal. */
    bool (*equal)(const net_addr_t *a, const net_addr_t *b);
} net_addr_ops_t;

extern const net_addr_ops_t *net_addr_ops(const net_addr_t *addr);

/**
 * Network interface state (addresses etc.). This is embedded within
 * net_device_t, but it is a separate structure/file so that we have some
 * separation between the underlying device implementation and higher level
 * interface state.
 */
typedef struct net_interface {
    list_t interfaces_link;             /**< Link to active interfaces list. */

    /**
     * Lock for operations on the interface. TODO: Might be too coarse-grained,
     * investigate whether this can be broken up later. Good enough for initial
     * implementation though.
     */
    mutex_t lock;

    uint32_t flags;                     /**< Flags for the interface (NET_INTERFACE_*). */
    array_t addrs;                      /**< Array of addresses. */
} net_interface_t;

extern void net_interface_receive(net_interface_t *interface, struct net_packet *packet);

extern status_t net_interface_add_addr(net_interface_t *interface, const net_addr_t *addr);
extern status_t net_interface_remove_addr(net_interface_t *interface, const net_addr_t *addr);

extern status_t net_interface_up(net_interface_t *interface);
extern status_t net_interface_down(net_interface_t *interface);

extern void net_interface_init(net_interface_t *interface);
