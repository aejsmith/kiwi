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
 * @brief		Filesystem node creation function.
 */

#include <sys/stat.h>
#include "../libc.h"

/** Create a filesystem node.
 * @param path		Path to node to create.
 * @param mode		Mode to give the node.
 * @param dev		Device number.
 * @return		0 on success, -1 on failure. */
int mknod(const char *path, mode_t mode, dev_t dev) {
	libc_stub("mknod", false);
	return -1;
}
