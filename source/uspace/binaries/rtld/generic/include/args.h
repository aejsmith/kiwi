/* Kiwi RTLD argument functions
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
 * @brief		RTLD argument functions.
 */

#ifndef __RTLD_ARGS_H
#define __RTLD_ARGS_H

#include <stdbool.h>

struct process_args;

/** Variables containing parsed arguments. */
extern bool rtld_debug;			/**< Whether debug output is enabled. */
extern bool rtld_dryrun;		/**< Whether in dry-run mode. */

extern void rtld_args_init(struct process_args *args);

#endif /* __RTLD_ARGS_H */
