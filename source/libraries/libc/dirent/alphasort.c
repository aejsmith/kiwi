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
 * @brief		Alphabetical sort function.
 */

#include <dirent.h>
#include <stdlib.h>
#include <string.h>

/** Sort directory entries in alphabetical order.
 *
 * Sort function to be used with scandir() to sort entries in alphabetical
 * order.
 *
 * @param a		Pointer to pointer to first entry.
 * @param b		Pointer to pointer to second entry.
 *
 * @return		An integer less than, equal to or greater than 0 if
 *			a is found, respectively, to be less than, to match,
 *			or to be greater than b.
 */
int alphasort(const void *a, const void *b) {
	const struct dirent *d1 = *(const struct dirent **)a;
        const struct dirent *d2 = *(const struct dirent **)b;

        return strcmp(d1->d_name, d2->d_name);
}
