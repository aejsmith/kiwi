/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARP protocol definitions.
 */

#pragma once

#include <net/ipv4.h>

struct net_interface;
struct net_packet;

/** ARP packet structure. */
typedef struct arp_packet {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t hw_len;
    uint8_t proto_len;
    uint16_t opcode;

    /**
     * Variable length address fields.
     *   uint8_t hw_sender[hw_len];
     *   uint8_t proto_sender[proto_len];
     *   uint8_t hw_target[hw_len];
     *   uint8_t proto_target[proto_len];
     */
    uint8_t addrs[];
} __packed arp_packet_t;

#define ARP_HW_TYPE_ETHERNET    1

#define ARP_OPCODE_REQUEST      1
#define ARP_OPCODE_REPLY        2

extern void arp_interface_remove(struct net_interface *interface);

extern status_t arp_lookup(
    uint32_t interface_id, const net_addr_ipv4_t *source_addr,
    const net_addr_ipv4_t *dest_addr, uint8_t *_dest_hw_addr);

extern void arp_receive(net_interface_t *interface, struct net_packet *packet);

extern void arp_init(void);
