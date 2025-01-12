/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Formatted output function.
 */

#pragma once

#include <types.h>

/** Type for a do_printf() helper function. */
typedef void (*printf_helper_t)(char, void *, int *);

extern int do_vprintf(printf_helper_t helper, void *data, const char *fmt, va_list args);
extern int do_printf(printf_helper_t helper, void *data, const char *fmt, ...);
