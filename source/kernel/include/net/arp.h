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
 * @brief               ARP protocol definitions.
 */

#pragma once

#include <net/ipv4.h>

/** ARP packet structure. */
typedef struct arp_packet {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t hw_len;
    uint8_t proto_len;
    uint16_t opcode;

    /**
     * Address fields. These are technically variable length but we only use
     * this for Ethernet/IPv4 right now.
     */
    uint8_t hw_sender[6];
    ipv4_addr_t proto_sender;
    uint8_t hw_target[6];
    ipv4_addr_t proto_target;
} __packed arp_packet_t;

#define ARP_HW_TYPE_ETHERNET    1

#define ARP_OPCODE_REQUEST      1
#define ARP_OPCODE_REPLY        2
