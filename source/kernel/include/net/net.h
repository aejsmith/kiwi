/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Network stack core definitions.
 */

#pragma once

#include <types.h>

struct device;

/** Name of the network stack module. */
#define NET_MODULE_NAME     "net"

/** Endianness conversion functions. */
#define net16_to_cpu(v)     be16_to_cpu(v)
#define net32_to_cpu(v)     be32_to_cpu(v)
#define net64_to_cpu(v)     be64_to_cpu(v)
#define cpu_to_net16(v)     cpu_to_be16(v)
#define cpu_to_net32(v)     cpu_to_be32(v)
#define cpu_to_net64(v)     cpu_to_be64(v)

extern struct device *net_virtual_device;
extern struct device *net_control_device;
