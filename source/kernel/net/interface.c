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

#include <sync/rwlock.h>

#include <net/ethernet.h>
#include <net/interface.h>
#include <net/ipv4.h>
#include <net/packet.h>

#include <assert.h>
#include <status.h>

/** Network interface lock (see net_interface_read_lock()). */
static RWLOCK_DEFINE(net_interface_lock);

/**
 * List of active network interfaces. Access to this is protected by
 * net_interface_lock.
 */
LIST_DEFINE(net_interface_list);

/** Next active interface ID. */
static uint32_t next_interface_id = 0;

/** Supported network device operations, indexed by family. */
static const net_addr_ops_t *supported_net_addr_ops[] = {
    [AF_INET] = &ipv4_net_addr_ops,
    //[AF_INET6] TODO
};

/** Supported network link operations, indexed by device type. */
static const net_link_ops_t *supported_net_link_ops[] = {
    [NET_DEVICE_ETHERNET] = &ethernet_net_link_ops,
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

/**
 * Takes the global network interface lock for reading. This is a global
 * read/write lock used to synchronise changes to the available network
 * interfaces and their address configuration.
 *
 * We need synchronisation at certain points to ensure that an interface cannot
 * be shut down/destroyed or have its address configuration changed while there
 * is an ongoing operation using that interface.
 *
 * Changes to the interface list and address configuration are infrequent, so
 * using a global rwlock is a simple and low overhead solution to this. It is
 * only locked for writing on address/interface changes, all other users just
 * lock for reading and can therefore operate concurrently. We have only a
 * global lock for this purpose rather than per-interface ones, since we'd need
 * the global one anyway to synchronise access to the interface list, and adding
 * per-interface ones on top of that would just add more overhead to getting
 * access to an interface from the interface list. Being an rwlock means there
 * is no concurrency issue for most usage.
 *
 * Use this lock where you need to ensure that a net_interface_t is kept alive
 * and that its address configuration will not change. It is only intended to
 * be held for short periods. Where interfaces need to be referred to
 * persistently (e.g. routing tables, ARP cache), or you need to keep a
 * reference to an interface for a long period (e.g. while waiting for an ARP
 * response), use the interface ID and re-lookup the interface as needed.
 */
void net_interface_read_lock(void) {
    rwlock_read_lock(&net_interface_lock);
}

/** Takes the global network interface lock for writing (internal only). */
static void net_interface_write_lock(void) {
    rwlock_write_lock(&net_interface_lock);
}

/** Release the global network interface lock. */
void net_interface_unlock(void) {
    rwlock_unlock(&net_interface_lock);
}

/**
 * Gets the network interface corresponding to the given ID. net_interface_lock
 * must be held. If this returns NULL, the interface has been taken down and
 * therefore the ID is no longer valid.
 *
 * @param id            Interface ID to get.
 *
 * @return              Pointer to interface or NULL if no longer valid.
 */
net_interface_t *net_interface_get(uint32_t id) {
    // TODO: This is a very frequent operation so maybe we can do something
    // better. Probably doesn't matter unless we have a lot of interfaces
    // though.

    list_foreach(&net_interface_list, iter) {
        net_interface_t *interface = list_entry(iter, net_interface_t, interfaces_link);

        if (interface->id == id)
            return interface;
    }

    return NULL;
}

/** Adds an address to a network interface.
 * @param interface     Interface to add to.
 * @param addr          Address to add.
 * @return              Status code describing the result of the operation. */
status_t net_interface_add_addr(net_interface_t *interface, const net_addr_t *addr) {
    status_t ret;

    net_interface_write_lock();

    /* Can't add addresses to an interface that's down. */
    if (!(interface->flags & NET_INTERFACE_UP)) {
        ret = STATUS_NET_DOWN;
        goto out;
    }

    const net_addr_ops_t *ops = net_addr_ops(addr);
    if (!ops) {
        ret = STATUS_ADDR_NOT_SUPPORTED;
        goto out;
    } else if (!ops->valid(addr)) {
        ret = STATUS_INVALID_ARG;
        goto out;
    }

    /* Check if this already exists. */
    for (size_t i = 0; i < interface->addrs.count; i++) {
        const net_addr_t *entry = array_entry(&interface->addrs, net_addr_t, i);
        if (ops->equal(entry, addr)) {
            ret = STATUS_ALREADY_EXISTS;
            goto out;
        }
    }

    net_addr_t *entry = array_append(&interface->addrs, net_addr_t);
    memcpy(entry, addr, sizeof(*addr));

    ret = STATUS_SUCCESS;

out:
    net_interface_unlock();
    return ret;
}

/** Removes an address from a network interface.
 * @param interface     Interface to remove from.
 * @param addr          Address to remove.
 * @return              Status code describing the result of the operation. */
status_t net_interface_remove_addr(net_interface_t *interface, const net_addr_t *addr) {
    status_t ret;

    net_interface_write_lock();

    /* Can't add addresses to an interface that's down. */
    if (!(interface->flags & NET_INTERFACE_UP)) {
        ret = STATUS_NET_DOWN;
        goto out;
    }

    const net_addr_ops_t *ops = net_addr_ops(addr);
    if (!ops) {
        ret = STATUS_ADDR_NOT_SUPPORTED;
        goto out;
    } else if (!ops->valid(addr)) {
        ret = STATUS_INVALID_ARG;
        goto out;
    }

    ret = STATUS_NOT_FOUND;

    for (size_t i = 0; i < interface->addrs.count; i++) {
        const net_addr_t *entry = array_entry(&interface->addrs, net_addr_t, i);

        if (ops->equal(entry, addr)) {
            array_remove(&interface->addrs, net_addr_t, i);
            ret = STATUS_SUCCESS;
            break;
        }
    }

out:
    net_interface_unlock();
    return ret;
}

/** Bring up a network interface.
 * @param interface     Interface to bring up.
 * @return              Status code describing the result of the operation. */
status_t net_interface_up(net_interface_t *interface) {
    net_device_t *device = net_device_from_interface(interface);
    status_t ret;

    assert(device->type < array_size(supported_net_link_ops) && supported_net_link_ops[device->type]);

    net_interface_write_lock();

    /* Do nothing if it's already up. */
    if (interface->flags & NET_INTERFACE_UP) {
        ret = STATUS_SUCCESS;
        goto out;
    }

    if (next_interface_id == NET_INTERFACE_INVALID_ID) {
        /* This won't be hit unless interfaces are brought up/down UINT32_MAX
         * times in the system lifetime... */
        kprintf(LOG_ERROR, "net: exhausted network interface IDs\n");
        ret = STATUS_IN_USE;
        goto out;
    }

    if (device->ops->up) {
        ret = device->ops->up(device);
        if (ret != STATUS_SUCCESS)
            goto out;
    }

    interface->id = next_interface_id++;
    interface->flags |= NET_INTERFACE_UP;

    list_append(&net_interface_list, &interface->interfaces_link);

    kprintf(LOG_NOTICE, "net: %pD: interface is up\n", device->node);
    ret = STATUS_SUCCESS;

out:
    net_interface_unlock();
    return ret;
}

/** Shut down a network interface.
 * @param interface     Interface to shut down.
 * @return              Status code describing the result of the operation. */
status_t net_interface_down(net_interface_t *interface) {
    net_device_t *device = net_device_from_interface(interface);
    status_t ret;

    net_interface_write_lock();

    /* Do nothing if it's already down. */
    if (!(interface->flags & NET_INTERFACE_UP)) {
        ret = STATUS_SUCCESS;
        goto out;
    }

    if (device->ops->down) {
        ret = device->ops->down(device);
        if (ret != STATUS_SUCCESS)
            goto out;
    }

    // TODO: Cancel anything that might be waiting on things coming in from
    // this interface, e.g. ARP requests? Or just let them time out...
    // TODO: Remove interface from routing tables, ARP cache, etc.

    list_remove(&interface->interfaces_link);
    interface->flags &= ~NET_INTERFACE_UP;
    interface->id = NET_INTERFACE_INVALID_ID;

    array_clear(&interface->addrs);

    kprintf(LOG_NOTICE, "net: %pD: interface is down\n", device->node);
    ret = STATUS_SUCCESS;

out:
    net_interface_unlock();
    return ret;
}

/** Called when a packet is received on an interface.
 * @param interface     Interface the packet was received on.
 * @param packet        Packet that was received. */
__export void net_interface_receive(net_interface_t *interface, net_packet_t *packet) {
    /* Ignore the packet if the interface is down. */
    if (!(interface->flags & NET_INTERFACE_UP))
        return;

    // TODO: What locking will be needed here? interface_lock should be taken
    // for reading to protect interface state above. do we need any per-interface
    // locking on top of that?
    kprintf(LOG_DEBUG, "net: packet received\n");
}

/**
 * Transmits a packet on a network interface. This will add the link-layer
 * protocol header and transmit it on the underlying device. The packet should
 * be no larger than the device's MTU. net_interface_lock must be held for
 * reading.
 *
 * @param interface     Interface to transmit on.
 * @param packet        Packet to transmit. Must have a valid type set.
 * @param dest_addr     Destination hardware address (length is the device's
 *                      hardware address length).
 *
 * @return              Status code describing the result of the operation.
 */
status_t net_interface_transmit(net_interface_t *interface, net_packet_t *packet, const uint8_t *dest_addr) {
    net_device_t *device = net_device_from_interface(interface);
    status_t ret;

    assert(packet->type != NET_PACKET_TYPE_UNKNOWN);

    // TODO: Any per-interface locking needed? net_interface_lock is held at least...
    // Will need something for transmit buffering.

    if (packet->size > device->mtu)
        return STATUS_MSG_TOO_LONG;

    ret = supported_net_link_ops[device->type]->add_header(interface, packet, dest_addr);
    if (ret != STATUS_SUCCESS)
        return ret;

    // TODO: Buffering when the device transmit queue is full.
    return device->ops->transmit(device, packet);
}

/** Initialize a network interface. */
void net_interface_init(net_interface_t *interface) {
    list_init(&interface->interfaces_link);
    array_init(&interface->addrs);

    interface->id    = NET_INTERFACE_INVALID_ID;
    interface->flags = 0;
}
