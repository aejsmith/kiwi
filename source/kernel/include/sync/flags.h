/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		Synchronization flag definitions.
 */

#ifndef __SYNC_FLAGS_H
#define __SYNC_FLAGS_H

/** Behaviour flags for synchronization functions. */
#define SYNC_INTERRUPTIBLE	(1<<0)	/**< Sleep should be interruptible. */
//#define SYNC_ABSOLUTE		(1<<1)	/**< Specified timeout is absolute, not relative to current time. */

#endif /* __SYNC_FLAGS_H */
