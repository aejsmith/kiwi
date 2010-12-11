/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		Close directory function.
 */

#include <stdlib.h>
#include "dirent_priv.h"

/** Close a directory stream.
 * @param dir		Directory stream to close.
 * @return		0 on success, -1 on failure. */
int closedir(DIR *dir) {
	status_t ret;

	ret = kern_handle_close(dir->handle);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	free(dir);
	return 0;
}
