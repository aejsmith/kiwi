/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 CPU cache definitions.
 */

#pragma once

/** CPU cache line shift/size. */
#define CPU_CACHE_SHIFT     6
#define CPU_CACHE_SIZE      (1 << CPU_CACHE_SHIFT)
