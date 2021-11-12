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

#include <net/net.h>

#include <sync/mutex.h>

struct net_packet;

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
} net_interface_t;

extern void net_interface_receive(net_interface_t *interface, struct net_packet *packet);

extern status_t net_interface_up(net_interface_t *interface);
extern status_t net_interface_down(net_interface_t *interface);

extern void net_interface_init(net_interface_t *interface);
