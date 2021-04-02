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

#pragma once

#include <net/net.h>

/** Ethernet MAC address length. */
#define ETHERNET_ADDR_LEN       6

/** Frame sizes. */
#define ETHERNET_MTU            1500
#define ETHERNET_MIN_FRAME_SIZE 64
#define ETHERNET_MAX_FRAME_SIZE 1514

/** Ethernet frame header. */
typedef struct ethernet_header {
    uint8_t dest[ETHERNET_ADDR_LEN];
    uint8_t source[ETHERNET_ADDR_LEN];
    uint16_t type;
} __packed ethernet_header_t;

/** EtherType values. */
#define ETHERNET_TYPE_IPV4      0x0800
#define ETHERNET_TYPE_ARP       0x0806
#define ETHERNET_TYPE_IPV6      0x86dd
