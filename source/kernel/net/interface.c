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

#include <device/net/net.h>

#include <lib/string.h>

#include <mm/malloc.h>

#include <net/interface.h>
#include <net/ipv4.h>
#include <net/packet.h>

#include <status.h>

/** Supported network device operations, indexed by family. */
static const net_addr_ops_t *supported_net_addr_ops[] = {
    [AF_INET] = &ipv4_net_addr_ops,
    //[AF_INET6] TODO
};

/** Get operations for handling a network interface address.
 * @param addr          Address to get for.
 * @return              Operations for the address, or NULL if family is not
 *                      supported. */
const net_addr_ops_t *net_addr_ops(const net_addr_t *addr) {
    return (addr->family < array_size(supported_net_addr_ops))
        ? supported_net_addr_ops[addr->family]
        : NULL;
}

/** Called when a packet is received on an interface.
 * @param interface     Interface the packet was received on.
 * @param packet        Packet that was received. */
__export void net_interface_receive(net_interface_t *interface, net_packet_t *packet) {
    MUTEX_SCOPED_LOCK(lock, &interface->lock);

    /* Ignore the packet if the interface is down. */
    if (!(interface->flags & NET_INTERFACE_UP))
        return;

    kprintf(LOG_DEBUG, "net: packet received\n");
}

/** Adds an address to a network interface.
 * @param interface     Interface to add to.
 * @param addr          Address to add.
 * @return              Status code describing the result of the operation. */
status_t net_interface_add_addr(net_interface_t *interface, const net_addr_t *addr) {
    MUTEX_SCOPED_LOCK(lock, &interface->lock);

    /* Can't add addresses to an interface that's down. */
    if (!(interface->flags & NET_INTERFACE_UP))
        return STATUS_NET_DOWN;

    const net_addr_ops_t *ops = net_addr_ops(addr);
    if (!ops) {
        return STATUS_NOT_SUPPORTED;
    } else if (!ops->valid(addr)) {
        return STATUS_INVALID_ARG;
    }

    /* Check if this already exists. */
    for (size_t i = 0; i < interface->addrs.count; i++) {
        const net_addr_t *entry = array_entry(&interface->addrs, net_addr_t, i);
        if (ops->equal(entry, addr))
            return STATUS_ALREADY_EXISTS;
    }

    net_addr_t *entry = array_append(&interface->addrs, net_addr_t);
    memcpy(entry, addr, sizeof(*addr));

    return STATUS_SUCCESS;
}

/** Removes an address from a network interface.
 * @param interface     Interface to remove from.
 * @param addr          Address to remove.
 * @return              Status code describing the result of the operation. */
status_t net_interface_remove_addr(net_interface_t *interface, const net_addr_t *addr) {
    MUTEX_SCOPED_LOCK(lock, &interface->lock);

    /* Can't add addresses to an interface that's down. */
    if (!(interface->flags & NET_INTERFACE_UP))
        return STATUS_NET_DOWN;

    const net_addr_ops_t *ops = net_addr_ops(addr);
    if (!ops) {
        return STATUS_NOT_SUPPORTED;
    } else if (!ops->valid(addr)) {
        return STATUS_INVALID_ARG;
    }

    for (size_t i = 0; i < interface->addrs.count; i++) {
        const net_addr_t *entry = array_entry(&interface->addrs, net_addr_t, i);

        if (ops->equal(entry, addr)) {
            array_remove(&interface->addrs, net_addr_t, i);
            return STATUS_SUCCESS;
        }
    }

    return STATUS_NOT_FOUND;
}

/** Bring up a network interface.
 * @param interface     Interface to bring up.
 * @return              Status code describing the result of the operation. */
status_t net_interface_up(net_interface_t *interface) {
    net_device_t *device = net_device_from_interface(interface);

    MUTEX_SCOPED_LOCK(lock, &interface->lock);

    /* Do nothing if it's already up. */
    if (interface->flags & NET_INTERFACE_UP)
        return STATUS_SUCCESS;

    if (device->ops->up) {
        status_t ret = device->ops->up(device);
        if (ret != STATUS_SUCCESS)
            return ret;
    }

    interface->flags |= NET_INTERFACE_UP;

    kprintf(LOG_NOTICE, "net: %pD: interface is up\n", device->node);
    return STATUS_SUCCESS;
}

/** Shut down a network interface.
 * @param interface     Interface to shut down.
 * @return              Status code describing the result of the operation. */
status_t net_interface_down(net_interface_t *interface) {
    net_device_t *device = net_device_from_interface(interface);

    MUTEX_SCOPED_LOCK(lock, &interface->lock);

    /* Do nothing if it's already down. */
    if (!(interface->flags & NET_INTERFACE_UP))
        return STATUS_SUCCESS;

    if (device->ops->down) {
        status_t ret = device->ops->down(device);
        if (ret != STATUS_SUCCESS)
            return ret;
    }

    interface->flags &= ~NET_INTERFACE_UP;

    array_clear(&interface->addrs);

    kprintf(LOG_NOTICE, "net: %pD: interface is down\n", device->node);
    return STATUS_SUCCESS;
}

/** Initialize a network interface. */
void net_interface_init(net_interface_t *interface) {
    mutex_init(&interface->lock, "net_interface_lock", 0);
    array_init(&interface->addrs);

    interface->flags = 0;
}
