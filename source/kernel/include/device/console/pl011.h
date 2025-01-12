/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               PL011 console implementation.
 */

#pragma once

#include <device/console/serial.h>

#if CONFIG_DEVICE_CONSOLE_PL011
extern const serial_port_ops_t pl011_serial_port_ops;
#endif
