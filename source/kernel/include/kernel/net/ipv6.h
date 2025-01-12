/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               IPv6 definitions.
 */

#pragma once

#include <kernel/net/ipv4.h>

__KERNEL_EXTERN_C_BEGIN

#define IPV6_ADDR_LEN       16
#define IPV6_ADDR_STR_LEN   46

/** Type used to store an IPv6 address. */
typedef struct in6_addr {
    union {
        uint8_t bytes[IPV6_ADDR_LEN];
        uint8_t s6_addr[IPV6_ADDR_LEN];
        struct {
            uint64_t high;
            uint64_t low;
        } val;
    };
} net_addr_ipv6_t;

/** IPv6 socket address specification. */
typedef struct sockaddr_in6 {
    sa_family_t sin6_family;            /**< AF_INET6. */
    in_port_t sin6_port;                /**< Port number (network byte order). */
    net_addr_ipv6_t sin6_addr;          /**< IPv6 address. */
    uint32_t sin6_flowinfo;             /**< IPv6 traffic class and flow information. */
    uint32_t sin6_scope_id;             /**< Set of interfaces for a scope. */ 
} sockaddr_in6_t;

/** IPv6 network interface address specification. */
typedef struct net_interface_addr_ipv6 {
    sa_family_t family;                 /**< AF_INET6. */
    // TODO
} net_interface_addr_ipv6_t;

/** IPv6 socket options. */
// TODO
#define IPV6_V6ONLY         1

__KERNEL_EXTERN_C_END
