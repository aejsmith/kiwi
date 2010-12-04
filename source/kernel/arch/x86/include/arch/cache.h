/*
 * Copyright (C) 2010 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		x86 CPU cache definitions.
 */

#ifndef __ARCH_CACHE_H
#define __ARCH_CACHE_H

/** CPU cache line shift/size. */
#define CPU_CACHE_SHIFT		6
#define CPU_CACHE_SIZE		(1 << CPU_CACHE_SHIFT)

#endif /* __ARCH_CACHE_H */
