/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Process/thread exit code definitions.
 */

#pragma once

#include <kernel/types.h>

__KERNEL_EXTERN_C_BEGIN

/** Process/thread exit reasons. */
#define EXIT_REASON_NORMAL      0   /**< Normal exit. */
#define EXIT_REASON_KILLED      1   /**< Process/thread was killed. */
#define EXIT_REASON_EXCEPTION   2   /**< Unhandled exception. */

__KERNEL_EXTERN_C_END
