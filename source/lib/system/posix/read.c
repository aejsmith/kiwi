/*
 * Copyright (C) 2009-2020 Alex Smith
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
 * @brief               POSIX read functions.
 *
 * @fixme               When a failure occurs after partially reading the data
 *                      the kernel updates the handle's offset by the number of
 *                      bytes that were successfully read. This is possibly
 *                      incorrect for POSIX.
 */

#include <kernel/device.h>
#include <kernel/fs.h>
#include <kernel/object.h>
#include <kernel/status.h>

#include <errno.h>
#include <unistd.h>

#include "libsystem.h"

/**
 * Read from a particular position in a file.
 *
 * Reads from the specified position in a file. The file descriptor's current
 * offset will be ignored, and will not be updated after the read.
 *
 * @param fd            File descriptor to read from.
 * @param buf           Buffer to read into.
 * @param count         Number of bytes to read.
 * @param offset        Offset into the file to read from.
 *
 * @return              Number of bytes read on success, -1 on failure (errno
 *                      will be set appropriately).
 */
ssize_t pread(int fd, void *buf, size_t count, off_t offset) {
    size_t bytes;
    status_t ret;

    if (offset < 0) {
        errno = EINVAL;
        return -1;
    }

    ret = kern_file_read(fd, buf, count, offset, &bytes);
    if (ret != STATUS_SUCCESS && (ret != STATUS_INTERRUPTED || bytes == 0)) {
        if (ret == STATUS_ACCESS_DENIED) {
            errno = EBADF;
        } else {
            libsystem_status_to_errno(ret);
        }

        return -1;
    }

    return (ssize_t)bytes;
}

/**
 * Read from a file.
 *
 * Reads from a file. After the read, the file descriptor's offset will be
 * updated by the number of bytes written.
 *
 * @param fd            File descriptor to read from.
 * @param buf           Buffer to read into.
 * @param count         Number of bytes to read.
 *
 * @return              Number of bytes read on success, -1 on failure (errno
 *                      will be set appropriately).
 */
ssize_t read(int fd, void *buf, size_t count) {
    size_t bytes;
    status_t ret;

    ret = kern_file_read(fd, buf, count, -1, &bytes);
    if (ret != STATUS_SUCCESS && (ret != STATUS_INTERRUPTED || bytes == 0)) {
        if (ret == STATUS_ACCESS_DENIED) {
            errno = EBADF;
        } else {
            libsystem_status_to_errno(ret);
        }

        return -1;
    }

    return (ssize_t)bytes;
}
