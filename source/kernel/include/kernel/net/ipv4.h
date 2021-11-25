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
 * @brief               IPv4 definitions.
 */

#pragma once

#include <kernel/net/family.h>

__KERNEL_EXTERN_C_BEGIN

#define IPV4_ADDR_LEN   4

typedef uint32_t in_addr_t;
typedef uint16_t in_port_t;

/** Type used to store an IPv4 address. */
typedef struct in_addr {
    union {
        in_addr_t val;                  /**< 32-bit address in network byte order. */
        uint8_t bytes[IPV4_ADDR_LEN];
        in_addr_t s_addr;               /**< 32-bit address in network byte order (POSIX-compatible name). */
    };
} ipv4_addr_t;

/** IPv4 socket address specification. */
typedef struct sockaddr_in {
    sa_family_t sin_family;             /**< AF_INET. */
    in_port_t sin_port;                 /**< Port number (network byte order). */
    ipv4_addr_t sin_addr;               /**< Address. */
} sockaddr_in_t;

/** IPv4 network interface address specification. */
typedef struct net_addr_ipv4 {
    sa_family_t family;                 /**< AF_INET. */
    ipv4_addr_t addr;                   /**< Address of interface. */
    ipv4_addr_t netmask;                /**< Mask for address. */
    ipv4_addr_t broadcast;              /**< Broadcast address. */
} net_addr_ipv4_t;

/**
 * Standard IP protocol numbers from
 * https://www.iana.org/assignments/protocol-numbers/protocol-numbers.xhtml
 */
enum {
    IPPROTO_IP      = 0,
    IPPROTO_ICMP    = 1,
    IPPROTO_TCP     = 6,
    IPPROTO_UDP     = 17,
};

__KERNEL_EXTERN_C_END
