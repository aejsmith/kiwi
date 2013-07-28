/*
 * Copyright (C) 2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
	dir_entry_t *entry;
	struct dirent *dent;
	status_t ret;

	entry = malloc(DIRSTREAM_BUF_SIZE);
	if(!entry)
		return NULL;

	ret = kern_file_read_dir(dir->handle, entry, DIRSTREAM_BUF_SIZE);
	if(ret != STATUS_SUCCESS) {
		if(ret != STATUS_NOT_FOUND)
			libsystem_status_to_errno(ret);

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
