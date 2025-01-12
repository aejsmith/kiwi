/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               AMD64 Programmable Interval Timer code.
 */

#pragma once

/** Base frequency of the PIT. */
#define PIT_BASE_FREQUENCY      1193182

/** Frequency that we run the PIT periodic timer at. */
#define PIT_TIMER_FREQUENCY     1000

/** PIT port definitions. */
#define PIT_MODE                0x43
#define PIT_CHAN0               0x40
#define PIT_CHAN1               0x41
#define PIT_CHAN2               0x42
