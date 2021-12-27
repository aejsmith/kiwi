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
 * @brief               Port address space helpers.
 */

#include <net/port.h>

#include <assert.h>
#include <status.h>

/** Looks up a port in a space, with the lock held.
 * @param space         Space to look up in (must be locked).
 * @param num           Port number to look up.
 * @return              Port found, or NULL if no port bound. */
net_port_t *net_port_lookup_unsafe(net_port_space_t *space, uint16_t num) {
    // TODO: Really needs a hash table...
    list_foreach(&space->ports, iter) {
        net_port_t *port = list_entry(iter, net_port_t, link);
        if (port->num == num)
            return port;
    }

    return NULL;
}

/** Allocates an ephemeral port from a port space.
 * @param space         Space to allocate from.
 * @param port          Port to allocate for.
 * @return              STATUS_SUCCESS if successful.
 *                      STATUS_TRY_AGAIN if no ports are currently available. */
status_t net_port_alloc_ephemeral(net_port_space_t *space, net_port_t *port) {
    assert(list_empty(&port->link));
    assert(port->num == 0);

    rwlock_write_lock(&space->lock);

    /* Round-robin allocation of port numbers. */
    uint16_t start = space->next_ephemeral_port;
    do {
        if (!net_port_lookup_unsafe(space, space->next_ephemeral_port)) {
            port->num = space->next_ephemeral_port;
            list_append(&space->ports, &port->link);
        }

        if (space->next_ephemeral_port == IP_EPHEMERAL_PORT_LAST) {
            space->next_ephemeral_port = IP_EPHEMERAL_PORT_FIRST;
        } else {
            space->next_ephemeral_port++;
        }
    } while (port->num == 0 && space->next_ephemeral_port != start);

    rwlock_unlock(&space->lock);
    return (port->num != 0) ? STATUS_SUCCESS : STATUS_TRY_AGAIN;
}

/** Frees an allocated port.
 * @param space         Space the port is allocated from.
 * @param port          Port to free. */
void net_port_free(net_port_space_t *space, net_port_t *port) {
    if (port->num != 0) {
        assert(!list_empty(&port->link));

        rwlock_write_lock(&space->lock);
        list_remove(&port->link);
        rwlock_unlock(&space->lock);

        port->num = 0;
    }
}
