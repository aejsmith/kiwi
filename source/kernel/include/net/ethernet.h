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
 * @brief               Ethernet link layer support.
 */

#pragma once

#include <net/interface.h>

/** Ethernet MAC address length. */
#define ETHERNET_ADDR_LEN           6

/** Ethernet frame size definitions. */
#define ETHERNET_HEADER_SIZE        14      /**< Ethernet header size. */
#define ETHERNET_MIN_PAYLOAD_SIZE   46      /**< Minimum payload size. */
#define ETHERNET_MAX_PAYLOAD_SIZE   1500    /**< Minimum payload size. */
#define ETHERNET_MTU                ETHERNET_MAX_PAYLOAD_SIZE
#define ETHERNET_MIN_FRAME_SIZE     60      /**< Minimum frame (header + payload) size, minus FCS. */
#define ETHERNET_MAX_FRAME_SIZE     1514    /**< Maximum frame size, minus FCS. */

/** Ethernet frame header. */
typedef struct ethernet_header {
    uint8_t dest_addr[ETHERNET_ADDR_LEN];
    uint8_t source_addr[ETHERNET_ADDR_LEN];
    uint16_t type;
} __packed ethernet_header_t;

extern const net_link_ops_t ethernet_net_link_ops;
