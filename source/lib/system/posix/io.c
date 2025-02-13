/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               POSIX I/O functions.
 */

#include <kernel/device.h>
#include <kernel/fs.h>
#include <kernel/object.h>
#include <kernel/status.h>

#include <errno.h>
#include <unistd.h>

#include "libsystem.h"

/**
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
    if (offset < 0) {
        errno = EINVAL;
        return -1;
    }

    size_t bytes = 0;
    status_t ret = kern_file_read(fd, buf, count, offset, &bytes);
    if (ret != STATUS_SUCCESS && bytes == 0) {
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
 * Reads from a file. After the read, the file descriptor's offset will be
 * updated by the number of bytes read.
 *
 * @param fd            File descriptor to read from.
 * @param buf           Buffer to read into.
 * @param count         Number of bytes to read.
 *
 * @return              Number of bytes read on success, -1 on failure (errno
 *                      will be set appropriately).
 */
ssize_t read(int fd, void *buf, size_t count) {
    size_t bytes = 0;
    status_t ret = kern_file_read(fd, buf, count, -1, &bytes);
    if (ret != STATUS_SUCCESS && bytes == 0) {
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
 * Writes to the specified position in a file. The file descriptor's current
 * offset will be ignored, and will not be updated after the write.
 *
 * @param fd            File descriptor to write to.
 * @param buf           Buffer containing data to write.
 * @param count         Number of bytes to write.
 * @param offset        Offset into the file to write to.
 *
 * @return              Number of bytes written on success, -1 on failure (errno
 *                      will be set appropriately).
 */
ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset) {
    if (offset < 0) {
        errno = EINVAL;
        return -1;
    }

    size_t bytes = 0;
    status_t ret = kern_file_write(fd, buf, count, offset, &bytes);
    if (ret != STATUS_SUCCESS && bytes == 0) {
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
 * Writes to a file. After the write, the file descriptor's offset will be
 * updated by the number of bytes written.
 *
 * @param fd            File descriptor to write to.
 * @param buf           Buffer containing data to write.
 * @param count         Number of bytes to write.
 *
 * @return              Number of bytes written on success, -1 on failure (errno
 *                      will be set appropriately).
 */
ssize_t write(int fd, const void *buf, size_t count) {
    size_t bytes = 0;
    status_t ret = kern_file_write(fd, buf, count, -1, &bytes);
    if (ret != STATUS_SUCCESS && bytes == 0) {
        if (ret == STATUS_ACCESS_DENIED) {
            errno = EBADF;
        } else {
            libsystem_status_to_errno(ret);
        }

        return -1;
    }

    return (ssize_t)bytes;
}
