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
 * @brief		POSIX change owner function.
 */

#include <unistd.h>
#include "../libc.h"

/** Change the owner of a filesystem entry.
 * @param path		Path to entry.
 * @param uid		New user ID.
 * @param gid		New group ID.
 * @return		0 on success, -1 on failure. */
int chown(const char *path, uid_t uid, gid_t gid) {
	libc_stub("chown", false);
	return 0;
}
