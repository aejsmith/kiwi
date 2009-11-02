/* File write function
 * Copyright (C) 2009 Alex Smith
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
 * @brief		File write function.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stdio_priv.h"

/** Write to a file stream.
 *
 * Writes nmemb elements of data, each size bytes long, from a buffer
 * into a file stream.
 *
 * @param ptr		Buffer to write from.
 * @param size		Size of each element.
 * @param nmemb		Number of elements to write.
 * @param stream	Stream to write to.
 *
 * @return		Number of elements written successfully.
 */
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
	const char *buf = (char *)ptr;
	size_t i;

	for(i = 0; i < (size * nmemb); i++) {
		if(fputc((int)buf[i], stream) == EOF) {
			break;
		}
	}

	return (size) ? (i / size) : 0;
}
