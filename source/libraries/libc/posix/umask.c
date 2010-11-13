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
 * @brief		POSIX umask() function.
 *
 * @todo		Preserve umask across an exec*() call.
 */

#include <sys/stat.h>
#include "posix_priv.h"

/** Current file mode creation mask. */
mode_t current_umask = 022;

/** Set the file mode creation mask.
 * @param mask		New mask.
 * @return		Previous mask. */
mode_t umask(mode_t mask) {
	mode_t prev = current_umask;
	current_umask = mask & 0777;
	return prev;
}
