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
 * @brief               IPv6 definitions.
 */

#pragma once

#include <kernel/net/ipv4.h>

__KERNEL_EXTERN_C_BEGIN

#define IPV6_ADDR_LEN   16

/** Type used to store an IPv6 address. */
typedef struct in6_addr {
    union {
        struct {
            uint64_t high;
            uint64_t low;
        } val;
        uint8_t bytes[IPV6_ADDR_LEN];
        uint8_t s6_addr[IPV6_ADDR_LEN];
    };
} ipv6_addr_t;

/** IPv6 socket address specification. */
typedef struct sockaddr_in6 {
    sa_family_t sin6_family;            /**< AF_INET6. */
    in_port_t sin6_port;                /**< Port number (network byte order). */
    ipv6_addr_t sin6_addr;              /**< IPv6 address. */
    uint32_t sin6_flowinfo;             /**< IPv6 traffic class and flow information. */
    uint32_t sin6_scope_id;             /**< Set of interfaces for a scope. */ 
} sockaddr_in6_t;

/** IPv6 network interface address specification. */
typedef struct net_addr_ipv6 {
    sa_family_t family;                 /**< AF_INET6. */
    // TODO
} net_addr_ipv6_t;

__KERNEL_EXTERN_C_END
