/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               x86 SMP definitions.
 */

#pragma once

struct cpu;

/** x86-specific SMP boot status values. */
#define SMP_BOOT_TSC_SYNC1      4   /**< Stage 1 of TSC synchronization. */
#define SMP_BOOT_TSC_SYNC2      5   /**< Stage 2 of TSC synchronization. */
