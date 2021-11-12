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
 * @brief               Network stack core definitions.
 */

#pragma once

#include <kernel/device/net.h>

/** Name of the network stack module. */
#define NET_MODULE_NAME     "net"

/** Endianness conversion functions. */
#define net16_to_cpu(v)     be16_to_cpu(v)
#define net32_to_cpu(v)     be32_to_cpu(v)
#define net64_to_cpu(v)     be64_to_cpu(v)
#define cpu_to_net16(v)     cpu_to_be16(v)
#define cpu_to_net32(v)     cpu_to_be32(v)
#define cpu_to_net64(v)     cpu_to_be64(v)
