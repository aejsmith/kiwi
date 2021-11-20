/*
 * Copyright (C) 2009-2021 Alex Smith
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
 * @brief               POSIX error number handling.
 */

#include <core/utility.h>

#include <kernel/status.h>

#include <errno.h>

#include "libsystem.h"

/**
 * Mappings of kernel status codes to POSIX error numbers.
 *
 * If a value maps to -1, a fatal error will be raised if
 * libsystem_status_to_errno() is called with that status, as that status is
 * either caused by an internal library error or should be handled by the
 * caller.
 */
static int status_to_errno_table[] = {
    [STATUS_SUCCESS]         = -1,
    [STATUS_NOT_IMPLEMENTED] = ENOSYS,
    [STATUS_NOT_SUPPORTED]   = ENOTSUP,
    [STATUS_WOULD_BLOCK]     = EWOULDBLOCK,
    [STATUS_INTERRUPTED]     = EINTR,
    [STATUS_TIMED_OUT]       = ETIMEDOUT,
    [STATUS_INVALID_SYSCALL] = -1,
    [STATUS_INVALID_ARG]     = EINVAL,
    [STATUS_INVALID_HANDLE]  = EBADF,
    [STATUS_INVALID_ADDR]    = EFAULT,
    [STATUS_INVALID_REQUEST] = -1,
    [STATUS_INVALID_EVENT]   = -1,
    [STATUS_OVERFLOW]        = EOVERFLOW,
    [STATUS_NO_MEMORY]       = ENOMEM,
    [STATUS_NO_HANDLES]      = EMFILE,
    [STATUS_PROCESS_LIMIT]   = EAGAIN,
    [STATUS_THREAD_LIMIT]    = EAGAIN,
    [STATUS_READ_ONLY]       = EROFS,
    [STATUS_PERM_DENIED]     = EPERM,
    [STATUS_ACCESS_DENIED]   = EACCES,
    [STATUS_NOT_DIR]         = ENOTDIR,
    [STATUS_NOT_REGULAR]     = EISDIR,         /* FIXME */
    [STATUS_NOT_SYMLINK]     = EINVAL,
    [STATUS_NOT_MOUNT]       = -1,
    [STATUS_NOT_FOUND]       = ENOENT,
    [STATUS_ALREADY_EXISTS]  = EEXIST,
    [STATUS_TOO_SMALL]       = ERANGE,
    [STATUS_TOO_LARGE]       = EMSGSIZE,       /* Is this right? */
    [STATUS_TOO_LONG]        = ENAMETOOLONG,
    [STATUS_NOT_EMPTY]       = ENOTEMPTY,
    [STATUS_DIR_FULL]        = ENOSPC,
    [STATUS_UNKNOWN_FS]      = -1,
    [STATUS_CORRUPT_FS]      = EIO,
    [STATUS_FS_FULL]         = ENOSPC,
    [STATUS_SYMLINK_LIMIT]   = ELOOP,
    [STATUS_IN_USE]          = EBUSY,
    [STATUS_DEVICE_ERROR]    = EIO,
    [STATUS_STILL_RUNNING]   = -1,
    [STATUS_UNKNOWN_IMAGE]   = ENOEXEC,
    [STATUS_MALFORMED_IMAGE] = ENOEXEC,
    [STATUS_MISSING_LIBRARY] = ENOEXEC,
    [STATUS_MISSING_SYMBOL]  = ENOEXEC,
    [STATUS_TRY_AGAIN]       = EAGAIN,
    [STATUS_DIFFERENT_FS]    = EXDEV,
    [STATUS_IS_DIR]          = EISDIR,
    [STATUS_PIPE_CLOSED]     = EPIPE,
    [STATUS_NET_DOWN]        = ENETDOWN,
};

/** Real location of errno. */
static __thread int __errno = 0;

/** Get the location of errno.
 * @return              Pointer to errno. */
int *__errno_location(void) {
    return &__errno;
}

/**
 * Set errno from a kernel status code.
 *
 * This function may not do the correct thing, POSIX is annoyingly inconsistent
 * about error codes. Callers should be careful.
 *
 * @param status        Status to set.
 */
void libsystem_status_to_errno(status_t status) {
    if (status < 0 || (size_t)status >= core_array_size(status_to_errno_table))
        libsystem_fatal("unknown status code passed to status_to_errno()");

    __errno = status_to_errno_table[status];
    if (__errno == -1)
        libsystem_fatal("trying to map disallowed status to errno");
}
