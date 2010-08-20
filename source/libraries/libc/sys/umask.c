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
 */

#include <sys/stat.h>
#include "../libc.h"

/** Set the file mode creation mask.
 * @param mask		New mask.
 * @return		Previous mask. */
mode_t umask(mode_t mask) {
	libc_stub("umask", false);
	return 022;
}
