/*
 * Copyright (C) 2010-2013 Alex Smith
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
 * @brief               POSIX get working directory function.
 */

#include <kernel/fs.h>
#include <kernel/status.h>

#include <errno.h>
#include <limits.h>
#include <unistd.h>

#include "libsystem.h"

/** Get the path to the current working directory.
 * @param buf           Buffer to place path string in.
 * @param size          Size of the buffer. If it is too small, errno will be
 *                      set to ERANGE.
 * @return              Pointer to buffer or NULL on failure. */
char *getcwd(char *buf, size_t size) {
    status_t ret;

    if (!size || !buf) {
        errno = EINVAL;
        return NULL;
    }

    ret = kern_fs_curr_dir(buf, size);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return NULL;
    }

    return buf;
}

/** Get the path to the current working directory.
 * @param buf           Buffer to place path string in. Must be at least
 *                      PATH_MAX bytes long.
 * @return              Pointer to buffer or NULL on failure. */
char *getwd(char *buf) {
    if (!getcwd(buf, PATH_MAX)) {
        if (errno == ERANGE)
            errno = ENAMETOOLONG;

        return NULL;
    }

    return buf;
}
