/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Network interface definitions.
 */

#pragma once

#include <kernel/net/ipv4.h>
#include <kernel/net/ipv6.h>

__KERNEL_EXTERN_C_BEGIN

/** Network interface flags. */
enum {
    /** Interface is up. */
    NET_INTERFACE_UP = (1<<0),
};

/** Specifies an invalid network interface. */
#define NET_INTERFACE_INVALID_ID    UINT32_MAX

__KERNEL_EXTERN_C_END
