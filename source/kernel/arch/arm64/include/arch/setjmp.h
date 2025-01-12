/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 setjmp() definitions.
 */

#pragma once

#include <types.h>

/**
 * Type of the buffer for setjmp(). In the kernel we don't use floating point
 * so no need to store those here.
 */
typedef unsigned long jmp_buf[13];
