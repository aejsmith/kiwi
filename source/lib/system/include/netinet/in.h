/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Internet address family.
 */

#pragma once

#include <core/endian.h>

#include <kernel/net/ipv4.h>
#include <kernel/net/ipv6.h>

#include <sys/socket.h>

__SYS_EXTERN_C_BEGIN

#define htonl(val) core_cpu_to_be32(val)
#define htons(val) core_cpu_to_be16(val)
#define ntohl(val) core_be32_to_cpu(val)
#define ntohs(val) core_be16_to_cpu(val)

#define INET_ADDRSTRLEN     IPV4_ADDR_STR_LEN
#define INET6_ADDRSTRLEN    IPV6_ADDR_STR_LEN

#define IN6ADDR_ANY_INIT \
    {{{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }}}
#define IN6ADDR_LOOPBACK_INIT \
    {{{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }}}

#define IN6_IS_ADDR_UNSPECIFIED(a) \
    (((uint32_t *)(a))[0] == 0 && \
     ((uint32_t *)(a))[1] == 0 && \
     ((uint32_t *)(a))[2] == 0 && \
     ((uint32_t *)(a))[3] == 0)

#define IN6_IS_ADDR_LOOPBACK(a) \
    (((uint32_t *)(a))[0]  == 0 && \
     ((uint32_t *)(a))[1]  == 0 && \
     ((uint32_t *)(a))[2]  == 0 && \
     ((uint8_t  *)(a))[12] == 0 && \
     ((uint8_t  *)(a))[13] == 0 && \
     ((uint8_t  *)(a))[14] == 0 && \
     ((uint8_t  *)(a))[15] == 1)

#define IN6_IS_ADDR_MULTICAST(a) \
    (((uint8_t *)(a))[0] == 0xff)

#define IN6_IS_ADDR_LINKLOCAL(a) \
    ((((uint8_t *)(a))[0]) == 0xfe && \
     (((uint8_t *)(a))[1] & 0xc0) == 0x80)

#define IN6_IS_ADDR_SITELOCAL(a) \
    ((((uint8_t *)(a))[0]) == 0xfe && \
     (((uint8_t *)(a))[1] & 0xc0) == 0xc0)

#define IN6_IS_ADDR_V4MAPPED(a) \
    (((uint32_t *)(a))[0]  == 0 && \
     ((uint32_t *)(a))[1]  == 0 && \
     ((uint8_t  *)(a))[8]  == 0 && \
     ((uint8_t  *)(a))[9]  == 0 && \
     ((uint8_t  *)(a))[10] == 0xff && \
     ((uint8_t  *)(a))[11] == 0xff)

#define IN6_IS_ADDR_V4COMPAT(a) \
    (((uint32_t *)(a))[0] == 0 && \
     ((uint32_t *)(a))[1] == 0 && \
     ((uint32_t *)(a))[2] == 0 && \
     ((uint8_t  *)(a))[15] > 1)

#define IN6_IS_ADDR_MC_NODELOCAL(a) \
    (IN6_IS_ADDR_MULTICAST(a) && ((((uint8_t *)(a))[1] & 0xf) == 0x1))

#define IN6_IS_ADDR_MC_LINKLOCAL(a) \
    (IN6_IS_ADDR_MULTICAST(a) && ((((uint8_t *)(a))[1] & 0xf) == 0x2))

#define IN6_IS_ADDR_MC_SITELOCAL(a) \
    (IN6_IS_ADDR_MULTICAST(a) && ((((uint8_t *)(a))[1] & 0xf) == 0x5))

#define IN6_IS_ADDR_MC_ORGLOCAL(a) \
    (IN6_IS_ADDR_MULTICAST(a) && ((((uint8_t *)(a))[1] & 0xf) == 0x8))

#define IN6_IS_ADDR_MC_GLOBAL(a) \
    (IN6_IS_ADDR_MULTICAST(a) && ((((uint8_t *)(a))[1] & 0xf) == 0xe))

__SYS_EXTERN_C_END
