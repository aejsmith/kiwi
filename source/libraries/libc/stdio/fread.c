/*
 * Copyright (C) 2008-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
