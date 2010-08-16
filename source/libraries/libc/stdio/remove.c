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
 * @brief		Path remove function.
 */

#include <stdio.h>
#include <unistd.h>

/** Remove a path from the filesystem.
 * @param path		Path to remove.
 * @return		0 on success, -1 on failure. */
int remove(const char *path) {
	/* Our unlink implementation supports directory removal. */
	return unlink(path);
}
