/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
extern const char *net_device_path;
extern handle_t ipv4_control_device;

extern void usage(void);

extern bool open_net_device(const char *path);
extern bool open_ipv4_control_device(void);

extern bool parse_ipv4_address(const char *str, net_addr_ipv4_t *addr);

extern bool command_dhcp(int argc, char **argv);
