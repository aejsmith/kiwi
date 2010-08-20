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
 * @brief		Scan directory function.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "dirent_priv.h"

/** Get array of directory entries.
 *
 * Gets an array of directory entries from a directory, filters them and
 * sorts them using the given functions.
 *
 * @param path		Path to directory.
 * @param namelist	Where to store array pointer.
 * @param filter	Function to filter entries (should return zero if
 *			an entry should be ignored).
 * @param compar	Comparison function.
 *
 * @return		Number of entries.
 */
int scandir(const char *path, struct dirent ***namelist, int (*filter)(const struct dirent *),
            int(*compar)(const void *, const void *)) {
	struct dirent **list = NULL, **nlist, *dent;
	int count = 0, i;
	DIR *dir;

	dir = opendir(path);
	if(dir == NULL) {
		return -1;
	}

	/* Clear errno so we can detect failures. */
	errno = 0;

	/** Loop through all directory entries. */
	while((dent = readdir(dir)) != NULL) {
		if(filter != NULL && filter(dent) == 0) {
			continue;
		}

		if((nlist = realloc(list, sizeof(void *) * (count + 1))) == NULL) {
			break;
		} else {
			list = nlist;
		}

		/* Insert it into the list. */
		if((list[count] = malloc(dent->d_reclen)) == NULL) {
			break;
		}

		memcpy(list[count], dent, dent->d_reclen);
		count++;
	}

	if(errno != 0) {
		for(i = 0; i < count; i++) {
			free(list[i]);
		}
		free(list);
		closedir(dir);
		return -1;
	}

	closedir(dir);

	if(compar != NULL) {
		qsort(list, count, sizeof(void *), compar);
	}

	*namelist = list;
        return count;
}
