/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               AMD64 stack size definitions.
 */

#pragma once

/** Stack size definitions. */
#define KSTACK_SIZE         0x2000      /**< Kernel stack size (8KB). */
#define USTACK_SIZE         0x200000    /**< User stack size (2MB). */

/** Stack direction definition. */
#define STACK_GROWS_DOWN    1
