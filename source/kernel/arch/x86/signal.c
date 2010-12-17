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
 * @brief		x86 signal dispatcher.
 */

#include <cpu/intr.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/safe.h>

#include <proc/process.h>
#include <proc/signal.h>

#include <assert.h>
#include <status.h>

/** Signal frame structure. */
typedef struct signal_frame {
	void *retaddr;			/**< Return address. */
#ifdef __i386__
	/** Arguments to the handler. */
	unative_t num;			/**< Signal number. */
	siginfo_t *infop;		/**< Signal information pointer. */
	ucontext_t *contextp;		/**< Previous context pointer. */
#endif
	siginfo_t info;			/**< Signal information. */
	ucontext_t context;		/**< Previous context. */
} signal_frame_t;
