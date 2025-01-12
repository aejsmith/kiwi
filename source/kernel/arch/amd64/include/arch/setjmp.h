/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               AMD64 setjmp() definitions.
 */

#pragma once

#include <types.h>

/** Type of the buffer for setjmp(). */
typedef unsigned long jmp_buf[8];
