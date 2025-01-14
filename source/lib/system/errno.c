/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
 * If a value maps to 0, a fatal error will be raised if
 * libsystem_status_to_errno() is called with that status, as that status is
 * either caused by an internal library error or should be handled by the
 * caller.
 */
static int status_to_errno_table[] = {
    [STATUS_SUCCESS]                = -1,
    [STATUS_NOT_IMPLEMENTED]        = ENOSYS,
    [STATUS_NOT_SUPPORTED]          = ENOTSUP,
    [STATUS_WOULD_BLOCK]            = EWOULDBLOCK,
    [STATUS_INTERRUPTED]            = EINTR,
    [STATUS_TIMED_OUT]              = ETIMEDOUT,
    [STATUS_INVALID_SYSCALL]        = 0,
    [STATUS_INVALID_ARG]            = EINVAL,
    [STATUS_INVALID_HANDLE]         = EBADF,
    [STATUS_INVALID_ADDR]           = EFAULT,
    [STATUS_INVALID_REQUEST]        = 0,
    [STATUS_INVALID_EVENT]          = 0,
    [STATUS_OVERFLOW]               = EOVERFLOW,
    [STATUS_NO_MEMORY]              = ENOMEM,
    [STATUS_NO_HANDLES]             = EMFILE,
    [STATUS_PROCESS_LIMIT]          = EAGAIN,
    [STATUS_THREAD_LIMIT]           = EAGAIN,
    [STATUS_READ_ONLY]              = EROFS,
    [STATUS_PERM_DENIED]            = EPERM,
    [STATUS_ACCESS_DENIED]          = EACCES,
    [STATUS_NOT_DIR]                = ENOTDIR,
    [STATUS_NOT_REGULAR]            = EISDIR,         /* FIXME */
    [STATUS_NOT_SYMLINK]            = EINVAL,
    [STATUS_NOT_MOUNT]              = 0,
    [STATUS_NOT_FOUND]              = ENOENT,
    [STATUS_NOT_EMPTY]              = ENOTEMPTY,
    [STATUS_ALREADY_EXISTS]         = EEXIST,
    [STATUS_TOO_SMALL]              = ERANGE,
    [STATUS_TOO_LARGE]              = EMSGSIZE,       /* Is this right? */
    [STATUS_TOO_LONG]               = ENAMETOOLONG,
    [STATUS_DIR_FULL]               = ENOSPC,
    [STATUS_UNKNOWN_FS]             = 0,
    [STATUS_CORRUPT_FS]             = EIO,
    [STATUS_FS_FULL]                = ENOSPC,
    [STATUS_SYMLINK_LIMIT]          = ELOOP,
    [STATUS_IN_USE]                 = EBUSY,
    [STATUS_DEVICE_ERROR]           = EIO,
    [STATUS_STILL_RUNNING]          = 0,
    [STATUS_NOT_RUNNING]            = 0,
    [STATUS_UNKNOWN_IMAGE]          = ENOEXEC,
    [STATUS_MALFORMED_IMAGE]        = ENOEXEC,
    [STATUS_MISSING_LIBRARY]        = ENOEXEC,
    [STATUS_MISSING_SYMBOL]         = ENOEXEC,
    [STATUS_TRY_AGAIN]              = EAGAIN,
    [STATUS_DIFFERENT_FS]           = EXDEV,
    [STATUS_IS_DIR]                 = EISDIR,
    [STATUS_CONN_HUNGUP]            = EAGAIN,
    [STATUS_CANCELLED]              = ECANCELED,
    [STATUS_INCORRECT_TYPE]         = EINVAL,
    [STATUS_PIPE_CLOSED]            = EPIPE,
    [STATUS_NET_DOWN]               = ENETDOWN,
    [STATUS_ADDR_NOT_SUPPORTED]     = EAFNOSUPPORT,
    [STATUS_PROTO_NOT_SUPPORTED]    = EPROTONOSUPPORT,
    [STATUS_MSG_TOO_LONG]           = EMSGSIZE,
    [STATUS_NET_UNREACHABLE]        = ENETUNREACH,
    [STATUS_HOST_UNREACHABLE]       = EHOSTUNREACH,
    [STATUS_IN_PROGRESS]            = EINPROGRESS,
    [STATUS_ALREADY_IN_PROGRESS]    = EALREADY,
    [STATUS_ALREADY_CONNECTED]      = EISCONN,
    [STATUS_CONNECTION_REFUSED]     = ECONNREFUSED,
    [STATUS_NOT_CONNECTED]          = ENOTCONN,
    [STATUS_ADDR_IN_USE]            = EADDRINUSE,
    [STATUS_UNKNOWN_SOCKET_OPT]     = ENOPROTOOPT,
};

/** Real location of errno. */
static __thread int __errno = 0;

/** Get the location of errno.
 * @return              Pointer to errno. */
int *__errno_location(void) {
    return &__errno;
}

/**
 * Return an errno value from a kernel status code.
 *
 * This function may not do the correct thing, POSIX is annoyingly inconsistent
 * about error codes. Callers should be careful.
 *
 * @param status        Status code.
 *
 * @return              Corresponding errno value.
 */
int libsystem_status_to_errno_val(status_t status) {
    if (status == STATUS_SUCCESS)
        return 0;

    if (status < 0 || (size_t)status >= core_array_size(status_to_errno_table))
        libsystem_fatal("unknown status code passed to status_to_errno()");

    int val = status_to_errno_table[status];
    if (val == 0)
        libsystem_fatal("trying to map disallowed status to errno");

    return val;
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
    __errno = libsystem_status_to_errno_val(status);
}
