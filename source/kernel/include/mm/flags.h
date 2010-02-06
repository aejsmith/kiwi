/*
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Memory allocation flags.
 *
 * This file includes definitions for flags supported by all allocation
 * functions. Allocators defining their own specific flags should start
 * from bit 10.
 */

#ifndef __MM_FLAGS_H
#define __MM_FLAGS_H

/** Allocation flags supported by all allocators. */
#define MM_SLEEP		(1<<0)		/**< Block until memory is available. */
#define MM_FATAL		(1<<1)		/**< Call fatal() if unable to satisfy an allocation. */

/** Internal flags used by Vmem, defined here to include in flag mask. */
#define VM_REFILLING		(1<<2)		/**< Tag refill in progress, do not attempt to refill again. */

/** Mask to select only generic allocation flags. */
#define MM_FLAG_MASK		(MM_SLEEP | MM_FATAL | VM_REFILLING)

#endif /* __MM_FLAGS_H */
