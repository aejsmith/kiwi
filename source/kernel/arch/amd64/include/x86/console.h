/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               AMD64 console code.
 */

#pragma once

/** Keyboard code definitions */
#define LEFT_CTRL           0x1d
#define RIGHT_CTRL          0x1d
#define LEFT_ALT            0x38
#define RIGHT_ALT           0x38
#define LEFT_SHIFT          0x2a
#define RIGHT_SHIFT         0x36

extern void i8042_init(void);
