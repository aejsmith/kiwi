/*
 * Copyright (C) 2009-2023 Alex Smith
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
 * @brief               Port address space helpers.
 */

#pragma once

#include <lib/list.h>

#include <net/ip.h>

#include <sync/rwlock.h>

/** UDP port address space (IPv4 and IPv6 have a different address space) */
typedef struct net_port_space {
    rwlock_t lock;

    // TODO: Replace this with a hash table.
    list_t ports;                   /**< List of all bound ports. */

    uint16_t next_ephemeral_port;   /**< Next ephemeral port number. */
} net_port_space_t;

#define NET_PORT_SPACE_INITIALIZER(_var) \
    { \
        .lock                = RWLOCK_INITIALIZER(_var.lock, "net_port_space_lock"), \
        .ports               = LIST_INITIALIZER(_var.ports), \
        .next_ephemeral_port = IP_EPHEMERAL_PORT_FIRST, \
    }

/** Network port structure (embedded inside protocol socket structure). */
typedef struct net_port {
    list_t link;                    /**< Link to port space. */
    uint16_t num;                   /**< Port number. */
} net_port_t;

/** Initialise a net_port_t structure. */
static inline void net_port_init(net_port_t *port) {
    list_init(&port->link);
    port->num = 0;
}

/** Locks a port space for reading. */
static inline void net_port_space_read_lock(net_port_space_t *space) {
    rwlock_read_lock(&space->lock);
}

/** Unlocks a port space. */
static inline void net_port_space_unlock(net_port_space_t *space) {
    rwlock_unlock(&space->lock);
}

extern net_port_t *net_port_lookup_unsafe(net_port_space_t *space, uint16_t num);

extern status_t net_port_alloc(net_port_space_t *space, net_port_t *port, uint16_t num);
extern status_t net_port_alloc_ephemeral(net_port_space_t *space, net_port_t *port);
extern void net_port_free(net_port_space_t *space, net_port_t *port);
