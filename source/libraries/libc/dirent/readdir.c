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
 * @brief		Read directory function.
 */

#include <stdlib.h>
#include <string.h>

#include "dirent_priv.h"

/** Read a directory entry.
 * @param dir		Directory stream to read from.
 * @return		Pointer to directory info structure, or NULL on failure.
 *			Data returned may be overwritten by a subsequent call
 *			to readdir(). */
struct dirent *readdir(DIR *dir) {
	struct dirent *dent;
	dir_entry_t *entry;
	status_t ret;

	entry = malloc(DIRSTREAM_BUF_SIZE);
	if(!entry) {
		return NULL;
	}

	ret = kern_dir_read(dir->handle, entry, DIRSTREAM_BUF_SIZE);
	if(ret != STATUS_SUCCESS) {
		if(ret != STATUS_NOT_FOUND) {
			libc_status_to_errno(ret);
		}
		return NULL;
	}

	/* Convert the kernel entry structure to a dirent structure. */
	dent = (struct dirent *)dir->buf;
	dent->d_ino = entry->id;
	dent->d_reclen = sizeof(*dent) + strlen(entry->name) + 1;
	strcpy(dent->d_name, entry->name);
	free(entry);
	return dent;
}
