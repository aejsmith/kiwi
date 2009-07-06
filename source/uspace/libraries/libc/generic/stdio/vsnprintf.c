/* Kiwi C library - String formatting functions
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
 * @brief		String formatting functions.
 */

#include "stdio_priv.h"

/** Data used by vsnprintf_helper(). */
struct vsnprintf_data {
	char *buf;			/**< Buffer to write to. */
	size_t size;			/**< Total size of buffer. */
	size_t off;			/**< Current number of bytes written. */
};

/** Helper for vsnprintf().
 * @param ch		Character to place in buffer.
 * @param _data		Data.
 * @param total		Pointer to total character count. */
static void vsnprintf_helper(char ch, void *_data, int *total) {
	struct vsnprintf_data *data = _data;

	if(data->off < data->size) {
		data->buf[data->off++] = ch;
		*total = *total + 1;
	}
}

/** Format a string and place it in a buffer.
 *
 * Places a formatted string in a buffer according to the format and
 * arguments given.
 *
 * @param buf		The buffer to place the result into.
 * @param size		The size of the buffer, including the trailing NULL.
 * @param fmt		The format string to use.
 * @param args		Arguments for the format string.
 *
 * @return		The number of characters generated, excluding the
 *			trailing NULL, as per ISO C99.
 */
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args) {
	struct vsnprintf_data data;
	int ret;

	data.buf = buf;
	data.size = size - 1;
	data.off = 0;

	ret = do_printf(vsnprintf_helper, &data, fmt, args);

	if(data.off < data.size) {
		data.buf[data.off] = 0;
	} else {
		data.buf[data.size-1] = 0;
	}
	return ret;
}

/** Format a string and place it in a buffer.
 *
 * Places a formatted string in a buffer according to the format and
 * arguments given.
 *
 * @param buf		The buffer to place the result into.
 * @param fmt		The format string to use.
 * @param args		Arguments for the format string.
 *
 * @return		The number of characters generated, excluding the
 *			trailing NULL, as per ISO C99.
 */
int vsprintf(char *buf, const char *fmt, va_list args) {
	return vsnprintf(buf, (size_t)-1, fmt, args);
}

/** Format a string and place it in a buffer.
 *
 * Places a formatted string in a buffer according to the format and
 * arguments given.
 *
 * @param buf		The buffer to place the result into.
 * @param size		The size of the buffer, including the trailing NULL.
 * @param fmt		The format string to use.
 *
 * @return		The number of characters generated, excluding the
 *			trailing NULL, as per ISO C99.
 */
int snprintf(char *buf, size_t size, const char *fmt, ...) {
	int ret;
	va_list args;

	va_start(args, fmt);
	ret = vsnprintf(buf, size, fmt, args);
	va_end(args);

	return ret;
}

/** Format a string and place it in a buffer.
 *
 * Places a formatted string in a buffer according to the format and
 * arguments given.
 *
 * @param buf		The buffer to place the result into.
 * @param fmt		The format string to use.
 *
 * @return		The number of characters generated, excluding the
 *			trailing NULL, as per ISO C99.
 */
int sprintf(char *buf, const char *fmt, ...) {
	int ret;
	va_list args;

	va_start(args, fmt);
	ret = vsprintf(buf, fmt, args);
	va_end(args);

	return ret;
}
