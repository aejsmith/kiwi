/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               NS16550 console implementation.
 */

#pragma once

#include <device/console/serial.h>

#if CONFIG_DEVICE_CONSOLE_NS16550
extern const serial_port_ops_t ns16550_serial_port_ops;
#endif

extern void ns16550_serial_configure(struct kboot_tag_serial *serial, uint32_t clock_rate);
