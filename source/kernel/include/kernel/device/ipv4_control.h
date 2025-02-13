/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               IPv4 control device interface.
 */

#pragma once

#include <kernel/device.h>
#include <kernel/net/ipv4.h>

__KERNEL_EXTERN_C_BEGIN

/** Network device class name. */
#define IPV4_CONTROL_DEVICE_CLASS_NAME  "ipv4_control"

/** IPv4 control device requests. */
enum {
    /**
     * Adds a route to the IPv4 routing table.
     *
     * Input:               An ipv4_route_t structure to add.
     *
     * Errors:              STATUS_ALREADY_EXISTS if an identical route already
     *                      exists.
     *                      STATUS_NET_DOWN if the specified interface does not
     *                      exist.
     */
    IPV4_CONTROL_DEVICE_REQUEST_ADD_ROUTE       = DEVICE_CLASS_REQUEST_START + 0,

    /**
     * Removes a route from the IPv4 routing table.
     *
     * Input:               An ipv4_route_t structure to remove. Note that flags
     *                      are not considered matching against table entries.
     *
     * Errors:              STATUS_NOT_FOUND if the route does not exist.
     */
    IPV4_CONTROL_DEVICE_REQUEST_REMOVE_ROUTE    = DEVICE_CLASS_REQUEST_START + 1,
};

/** IPv4 routing table entry. */
typedef struct ipv4_route {
    net_addr_ipv4_t addr;           /**< Route address, or INADDR_ANY for default route. */
    net_addr_ipv4_t netmask;        /**< Network mask for address. */
    net_addr_ipv4_t gateway;        /**< Gateway address to use, or INADDR_ANY for direct route. */
    net_addr_ipv4_t source;         /**< Source address to use for this route. */
    uint32_t interface_id;          /**< Interface ID (NET_DEVICE_REQUEST_INTERFACE_ID). */
    uint32_t flags;                 /**< Route flags (IPV4_ROUTE_*).. */
} ipv4_route_t;

/** IPv4 route flags. */
enum {
    /**
     * Route is automatically added from an interface address and will be
     * automatically removed when the corresponding address is removed. Routes
     * with this flag set can be manually removed, but cannot be removed
     * manually.
     */
    IPV4_ROUTE_AUTO = (1<<0),
};

__KERNEL_EXTERN_C_END
