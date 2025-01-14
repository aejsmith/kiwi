/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               String formatting functions.
 */

#include <stdlib.h>

#include "stdio/stdio.h"

/** Data used by vsnprintf_helper(). */
struct vsnprintf_data {
    char *buf;                      /**< Buffer to write to. */
    size_t size;                    /**< Total size of buffer. */
    size_t off;                     /**< Current number of bytes written. */
};

/** Helper for vsnprintf(). */
static void vsnprintf_helper(char ch, void *_data) {
    struct vsnprintf_data *data = _data;

    if (data->off < data->size)
        data->buf[data->off++] = ch;
}

/** Write a formatted string into a buffer.
 * @param buf           The buffer to place the result into.
 * @param size          The size of the buffer, including the trailing NULL.
 * @param fmt           The format string to use.
 * @param args          Arguments for the format string.
 * @return              The number of characters generated, excluding the
 *                      trailing NULL, as per ISO C99. */
int vsnprintf(char *restrict buf, size_t size, const char *restrict fmt, va_list args) {
    struct vsnprintf_data data;
    data.buf  = buf;
    data.size = (size > 0) ? size - 1 : 0;
    data.off  = 0;

    int ret = do_vprintf(vsnprintf_helper, &data, fmt, args);

    data.buf[(data.off < data.size) ? data.off : data.size] = 0;

    return ret;
}

/** Write a formatted string into a buffer.
 * @param buf           The buffer to place the result into.
 * @param fmt           The format string to use.
 * @param args          Arguments for the format string.
 * @return              The number of characters generated, excluding the
 *                      trailing NULL, as per ISO C99. */
int vsprintf(char *restrict buf, const char *restrict fmt, va_list args) {
    return vsnprintf(buf, (size_t)-1, fmt, args);
}

/** Write a formatted string into a buffer.
 * @param buf           The buffer to place the result into.
 * @param size          The size of the buffer, including the trailing NULL.
 * @param fmt           The format string to use.
 * @return              The number of characters generated, excluding the
 *                      trailing NULL, as per ISO C99. */
int snprintf(char *restrict buf, size_t size, const char *restrict fmt, ...) {
    va_list args;

    va_start(args, fmt);
    int ret = vsnprintf(buf, size, fmt, args);
    va_end(args);

    return ret;
}

/** Write a formatted string into a buffer.
 * @param buf           The buffer to place the result into.
 * @param fmt           The format string to use.
 * @return              The number of characters generated, excluding the
 *                      trailing NULL, as per ISO C99. */
int sprintf(char *restrict buf, const char *restrict fmt, ...) {
    va_list args;

    va_start(args, fmt);
    int ret = vsprintf(buf, fmt, args);
    va_end(args);

    return ret;
}

/** Write a formatted string into an allocated buffer.
 * @param buf           Where to store a malloc()-allocated result string.
 * @param fmt           The format string to use.
 * @return              The number of characters generated, excluding the
 *                      trailing NULL, as per ISO C99, or -1 if allocation
 *                      fails. */
int vasprintf(char **strp, const char *restrict fmt, va_list args) {
    va_list tmp_args;
    char dummy;
    va_copy(tmp_args, args);
    int len = vsnprintf(&dummy, 0, fmt, tmp_args);
    va_end(tmp_args);

    if (len < 0) {
        *strp = NULL;
        return len;
    }

    len++;

    *strp = malloc(len);
    if (!*strp)
        return -1;

    int ret = vsnprintf(*strp, len, fmt, args);
    if (ret < 0) {
        free(*strp);
        *strp = NULL;
    }

    return ret;
}

/** Write a formatted string into an allocated buffer.
 * @param buf           Where to store a malloc()-allocated result string.
 * @param fmt           The format string to use.
 * @return              The number of characters generated, excluding the
 *                      trailing NULL, as per ISO C99, or -1 if allocation
 *                      fails. */
int asprintf(char **strp, const char *restrict fmt, ...) {
    va_list args;

    va_start(args, fmt);
    int ret = vasprintf(strp, fmt, args);
    va_end(args);

    return ret;
}
