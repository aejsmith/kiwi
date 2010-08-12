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
 * @brief		File read function.
 */

#include <unistd.h>
#include "stdio_priv.h"

/** Read from a file stream.
 *
 * Reads nmemb elements of data, each size bytes long, from a file stream
 * into a buffer.
 *
 * @param ptr		Buffer to read into.
 * @param size		Size of each element.
 * @param nmemb		Number of elements to read.
 * @param stream	Stream to read from.
 *
 * @return		Number of elements read successfully.
 */
size_t fread(void *restrict ptr, size_t size, size_t nmemb, FILE *restrict stream) {
	char *buf = (char *)ptr;
	size_t total, count = 0;
	ssize_t ret;

	total = size * nmemb;
	if(!total) {
		return 0;
	}

	/* Read the pushed back character if there is one. */
	if(stream->have_pushback) {
		buf[count++] = stream->pushback_ch;
		stream->have_pushback = false;
	}

	/* Read remaining data. */
	if(count < total) {
		ret = read(stream->fd, &buf[count], total - count);
		if(ret > 0) {
			count += ret;
		}
	}

	return count / size;
}
