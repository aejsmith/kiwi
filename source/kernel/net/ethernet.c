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
 * @brief               Ethernet link layer support.
 */

#include <device/net/net.h>

#include <net/ethernet.h>
#include <net/packet.h>

#include <assert.h>
#include <status.h>

static const uint8_t ethernet_broadcast_addr[ETHERNET_ADDR_LEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

static status_t ethernet_add_header(net_interface_t *interface, net_packet_t *packet, const uint8_t *dest_addr) {
    net_device_t *device = net_device_from_interface(interface);

    assert(device->hw_addr_len == ETHERNET_ADDR_LEN);

    ethernet_header_t *header;
    net_buffer_t *buffer = net_buffer_kmalloc(sizeof(*header), MM_KERNEL, (void **)&header);
    net_packet_prepend(packet, buffer);

    memcpy(header->dest_addr, dest_addr, ETHERNET_ADDR_LEN);
    memcpy(header->source_addr, device->hw_addr, ETHERNET_ADDR_LEN);
    header->type = cpu_to_net16(packet->type);

    return STATUS_SUCCESS;
}

static void ethernet_parse_header(net_interface_t *interface, net_packet_t *packet) {
    ethernet_header_t *header = net_packet_data(packet, 0, sizeof(*header));
    if (header) {
        packet->type = net16_to_cpu(header->type);

        // TODO: Do upper layers need to know if this is a broadcast packet or not?

        net_packet_offset(packet, sizeof(*header));
    } else {
        packet->type = NET_PACKET_TYPE_UNKNOWN;
    }
}

const net_link_ops_t ethernet_net_link_ops = {
    .broadcast_addr = ethernet_broadcast_addr,

    .add_header   = ethernet_add_header,
    .parse_header = ethernet_parse_header,
};
