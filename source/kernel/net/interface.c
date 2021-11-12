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

#include <net/interface.h>
#include <net/packet.h>

#include <status.h>

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

    kprintf(LOG_NOTICE, "net: interface %pD is up\n", device->node);
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

    kprintf(LOG_NOTICE, "net: interface %pD is down\n", device->node);
    return STATUS_SUCCESS;
}

/** Initialize a network interface. */
void net_interface_init(net_interface_t *interface) {
    mutex_init(&interface->lock, "net_interface_lock", 0);

    interface->flags = 0;
}
