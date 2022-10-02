/*
 * Copyright (C) 2009-2022 Alex Smith
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
 * @brief               Network device control utility.
 */

#pragma once

#include <core/log.h>

#include <device/net.h>

#include <kernel/device/ipv4_control.h>
#include <kernel/net/ipv4.h>
#include <kernel/status.h>

#include <stdio.h>

extern net_device_t *net_device;
extern handle_t ipv4_control_device;

extern void usage(void);

extern bool open_net_device(const char *path);
extern bool open_ipv4_control_device(void);

extern bool parse_ipv4_address(const char *str, net_addr_ipv4_t *addr);

extern bool command_dhcp(int argc, char **argv);
