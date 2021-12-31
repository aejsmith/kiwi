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
 * @brief               Address Resolution Protocol.
 */

#include <lib/list.h>

#include <device/net/net.h>

#include <net/arp.h>
#include <net/packet.h>

#include <proc/thread.h>

#include <sync/condvar.h>
#include <sync/mutex.h>

#include <kdb.h>
#include <status.h>
#include <time.h>

/** Define to enable debug output. */
#define DEBUG_ARP

#ifdef DEBUG_ARP
#   define dprintf(fmt...)  kprintf(LOG_DEBUG, fmt)
#else
#   define dprintf(fmt...)
#endif

/** ARP cache entry structure. */
typedef struct arp_entry {
    list_t link;

    nstime_t timeout;               /**< Absolute timeout of current request attempt. */
    bool complete;                  /**< Whether the entry is complete. */
    uint8_t retries;                /**< Remaining retries. */
    uint32_t interface_id;          /**< Interface ID that this entry is for. */

    /** Destination IP address and resolved HW address. */
    net_addr_ipv4_t addr;
    uint8_t hw_addr[NET_DEVICE_ADDR_MAX];

    condvar_t cvar;                 /**< Condition variable to wait for completion. */
} arp_entry_t;

/** Number of retries for an ARP request before giving up. */
#define ARP_MAX_RETRIES 3

/** Timeout before retrying an ARP request. */
#define ARP_TIMEOUT     secs_to_nsecs(1)

/** ARP cache. */
// TODO: Better data structure.
static LIST_DEFINE(arp_cache);
static MUTEX_DEFINE(arp_cache_lock, 0);

static uint16_t arp_hw_type(net_device_type_t type) {
    switch (type) {
        case NET_DEVICE_ETHERNET:
            return cpu_to_net16(ARP_HW_TYPE_ETHERNET);
        default:
            kprintf(LOG_ERROR, "arp: unsupported device type %d\n", type);
            return 0;
    }
}

static status_t send_arp_request(
    uint32_t interface_id, const net_addr_ipv4_t *source_addr,
    const net_addr_ipv4_t *dest_addr)
{
    net_interface_read_lock();

    net_interface_t *interface = net_interface_get(interface_id);
    if (!interface) {
        net_interface_unlock();
        return STATUS_NET_DOWN;
    }

    net_device_t *device = net_device_from_interface(interface);

    size_t packet_size = sizeof(arp_packet_t) + (2 * device->hw_addr_len) + (2 * IPV4_ADDR_LEN);

    arp_packet_t *request;
    net_packet_t *packet = net_packet_kmalloc(packet_size, MM_KERNEL, (void **)&request);

    request->hw_type    = arp_hw_type(device->type);
    request->proto_type = cpu_to_net16(NET_PACKET_TYPE_IPV4);
    request->hw_len     = device->hw_addr_len;
    request->proto_len  = IPV4_ADDR_LEN;
    request->opcode     = cpu_to_net16(ARP_OPCODE_REQUEST);

    uint8_t *addrs = request->addrs;

    memcpy(addrs, device->hw_addr, device->hw_addr_len);
    addrs += device->hw_addr_len;
    memcpy(addrs, source_addr, IPV4_ADDR_LEN);
    addrs += IPV4_ADDR_LEN;
    memset(addrs, 0, device->hw_addr_len);
    addrs += device->hw_addr_len;
    memcpy(addrs, dest_addr, IPV4_ADDR_LEN);

    packet->type = NET_PACKET_TYPE_ARP;

    status_t ret = net_interface_transmit(interface, packet, interface->link_ops->broadcast_addr);
    net_packet_release(packet);
    net_interface_unlock();
    return ret;
}

/**
 * Removes ARP cache entries corresponding to an interface that is being
 * removed.
 */
void arp_interface_remove(net_interface_t *interface) {
    MUTEX_SCOPED_LOCK(lock, &arp_cache_lock);

    list_foreach_safe(&arp_cache, iter) {
        arp_entry_t *entry = list_entry(iter, arp_entry_t, link);

        if (entry->interface_id == interface->id) {
            /* arp_lookup() will search for the entry again after waking and
             * see that it has been removed. */
            condvar_broadcast(&entry->cvar);
            list_remove(&entry->link);
            kfree(entry);
        }
    }
}

/**
 * Looks up a destination hardware address for the given destination IP address,
 * either by retrieiving an existing entry from the ARP cache or by performing
 * an ARP request.
 *
 * @param interface_id  Interface ID to perform the request on.
 * @param source_addr   Source IP address to include in the request.
 * @param dest_addr     Destination IP address.
 * @param _dest_hw_addr Where to store destination hardware address (must be a
 *                      NET_DEVICE_ADDR_MAX-sized buffer).
 *
 * @return              Status code describing the result of the operation.
 */
status_t arp_lookup(
    uint32_t interface_id, const net_addr_ipv4_t *source_addr,
    const net_addr_ipv4_t *dest_addr, uint8_t *_dest_hw_addr)
{
    MUTEX_SCOPED_LOCK(lock, &arp_cache_lock);

    arp_entry_t *entry = NULL;

    /* See if there's an existing entry. */
    list_foreach(&arp_cache, iter) {
        arp_entry_t *exist = list_entry(iter, arp_entry_t, link);

        if (exist->interface_id == interface_id && exist->addr.val == dest_addr->val) {
            entry = exist;
            break;
        }
    }

    if (!entry) {
        /* Need to make a new entry. */
        entry = kmalloc(sizeof(arp_entry_t), MM_KERNEL | MM_ZERO);

        list_init(&entry->link);
        condvar_init(&entry->cvar, "arp_entry_cvar");

        entry->retries      = ARP_MAX_RETRIES;
        entry->interface_id = interface_id;
        entry->addr.val     = dest_addr->val;

        list_append(&arp_cache, &entry->link);
    }

    status_t ret = STATUS_SUCCESS;

    while (entry && !entry->complete) {
        nstime_t curr_time = system_time();

        /* If the current timeout has passed, we should retry sending a new
         * request out or give up. */
        if (curr_time >= entry->timeout) {
            if (entry->retries == 0)
                break;

            dprintf(
                "arp: sending request for %pI4 from %pI4 on interface %u (retries: %u)\n",
                dest_addr, source_addr, entry->interface_id, entry->retries);

            entry->retries--;

            ret = send_arp_request(entry->interface_id, source_addr, dest_addr);
            if (ret != STATUS_SUCCESS)
                break;

            entry->timeout = curr_time + ARP_TIMEOUT;
        }

        /* Wait for the request and check again. */
        ret = condvar_wait_etc(
            &entry->cvar, &arp_cache_lock, entry->timeout,
            SLEEP_ABSOLUTE | SLEEP_INTERRUPTIBLE);

        /* Check to see if the entry still exists. It may have been removed if
         * the interface was removed or the entry was manually removed. */
        entry = NULL;
        list_foreach(&arp_cache, iter) {
            arp_entry_t *exist = list_entry(iter, arp_entry_t, link);

            if (exist->interface_id == interface_id && exist->addr.val == dest_addr->val) {
                entry = exist;
                break;
            }
        }

        if (ret == STATUS_TIMED_OUT)
            ret = STATUS_SUCCESS;
        if (ret != STATUS_SUCCESS)
            break;
    }

    if (ret == STATUS_SUCCESS) {
        if (entry && entry->complete) {
            memcpy(_dest_hw_addr, entry->hw_addr, NET_DEVICE_ADDR_MAX);
        } else {
            ret = STATUS_HOST_UNREACHABLE;
        }
    }

    return ret;
}

static void handle_arp_reply(net_device_t *device, const net_addr_ipv4_t *addr, const uint8_t *hw_addr) {
    MUTEX_SCOPED_LOCK(lock, &arp_cache_lock);

    list_foreach(&arp_cache, iter) {
        arp_entry_t *entry = list_entry(iter, arp_entry_t, link);

        if (entry->interface_id == device->interface.id && entry->addr.val == addr->val) {
            dprintf("arp: resolved address %pI4 to %pM\n", addr, hw_addr);

            memcpy(entry->hw_addr, hw_addr, device->hw_addr_len);
            entry->complete = true;
            entry->retries  = 0;

            condvar_broadcast(&entry->cvar);
            break;
        }
    }

    // TODO: If we don't have it we could cache this anyway, if it looks valid
    // for the interface's address configuration.
}

/** Handles a received ARP packet.
 * @param interface     Source interface.
 * @param packet        Packet that was received. */
void arp_receive(net_interface_t *interface, net_packet_t *packet) {
    net_device_t *device = net_device_from_interface(interface);

    const arp_packet_t *header = net_packet_data(packet, 0, sizeof(*header));
    if (!header) {
        dprintf("arp: dropping packet: too short\n");
        return;
    } else if (header->hw_type != arp_hw_type(device->type)) {
        dprintf("arp: dropping packet: invalid HW type\n");
        return;
    } else if (header->hw_len != device->hw_addr_len) {
        dprintf("arp: dropping packet: invalid HW address length\n");
        return;
    } else if (header->proto_type != cpu_to_net16(NET_PACKET_TYPE_IPV4)) {
        dprintf("arp: dropping packet: unknown protocol type\n");
        return;
    } else if (header->proto_len != IPV4_ADDR_LEN) {
        dprintf("arp: dropping packet: invalid HW address length\n");
        return;
    }

    const uint8_t *addrs = header->addrs;

    const uint8_t *hw_sender = addrs;
    addrs += device->hw_addr_len;
    const net_addr_ipv4_t *proto_sender = (const net_addr_ipv4_t *)addrs;
    addrs += IPV4_ADDR_LEN;
    const uint8_t *hw_target = addrs;
    addrs += device->hw_addr_len;
    const net_addr_ipv4_t *proto_target = (const net_addr_ipv4_t *)addrs;

    dprintf(
        "arp: received packet 0x%" PRIx16 " (hw_sender: %pM, proto_sender, %pI4, hw_target: %pM, proto_target: %pI4)\n",
        net16_to_cpu(header->opcode), hw_sender, proto_sender, hw_target, proto_target);

    switch (net16_to_cpu(header->opcode)) {
        case ARP_OPCODE_REPLY:
            handle_arp_reply(device, proto_sender, hw_sender);
            break;
        case ARP_OPCODE_REQUEST:
            // TODO: Can add the sender to our cache.
            dprintf("arp: TODO: handle requests\n");
            break;
        default:
            dprintf("arp: dropping packet: unknown opcode\n");
            break;
    }
}

static kdb_status_t kdb_cmd_arp(int argc, char **argv, kdb_filter_t *filter) {
    if (kdb_help(argc, argv)) {
        kdb_printf("Usage: %s\n\n", argv[0]);

        kdb_printf("Shows the ARP cache.\n");
        return KDB_SUCCESS;
    }

    kdb_printf("IP address      MAC address       Interface Complete Retries\n");
    kdb_printf("==========      ===========       ========= ======== =======\n");

    list_foreach(&arp_cache, iter) {
        arp_entry_t *entry = list_entry(iter, arp_entry_t, link);

        kdb_printf(
            "%-15pI4 %-17pM %-9" PRIu32 " %-8s %" PRIu8 "\n",
            &entry->addr, entry->hw_addr, entry->interface_id,
            (entry->complete) ? "Y" : "N", entry->retries);
    }

    return KDB_SUCCESS;
}

void arp_init(void) {
    kdb_register_command("arp", "Show the ARP cache.", kdb_cmd_arp);
}
