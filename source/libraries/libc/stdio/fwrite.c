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
 * @brief		File write function.
 */

#include <unistd.h>
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
size_t fwrite(const void *restrict ptr, size_t size, size_t nmemb, FILE *restrict stream) {
	ssize_t ret;

	if(!size || !nmemb) {
		return 0;
	}

	ret = write(stream->fd, ptr, size * nmemb);
	if(ret <= 0) {
		return 0;
	}

	return ret / size;
}
