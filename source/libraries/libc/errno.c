/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		POSIX error number handling.
 *
 * @todo		Make this thread-local.
 */

#include <kernel/status.h>
#include <errno.h>
#include "libc.h"

/** Mappings of kernel status codes to POSIX error numbers.
 * @note		If a value maps to -1, a fatal error will be raised if
 *			libc_status_to_errno() is called with that status, as
 *			that status is either caused by an internal library
 *			error or should be handled by the caller. */
static int status_to_errno_table[] = {
	[STATUS_SUCCESS] =		-1,
	[STATUS_NOT_IMPLEMENTED] =	ENOSYS,
	[STATUS_NOT_SUPPORTED] =	ENOTSUP,
	[STATUS_WOULD_BLOCK] =		EWOULDBLOCK,
	[STATUS_INTERRUPTED] =		EINTR,
	[STATUS_TIMED_OUT] =		ETIMEDOUT,
	[STATUS_INVALID_SYSCALL] =	-1,
	[STATUS_INVALID_ARG] =		EINVAL,
	[STATUS_INVALID_HANDLE] =	EBADF,
	[STATUS_INVALID_ADDR] =		EFAULT,
	[STATUS_INVALID_REQUEST] =	-1,
	[STATUS_INVALID_EVENT] =	ENOSYS,
	[STATUS_OVERFLOW] =		EOVERFLOW,
	[STATUS_NO_MEMORY] =		ENOMEM,
	[STATUS_NO_HANDLES] =		EMFILE,
	[STATUS_NO_PORTS] =		EAGAIN,
	[STATUS_NO_SEMAPHORES] =	EAGAIN,
	[STATUS_NO_AREAS] =		EAGAIN,
	[STATUS_PROCESS_LIMIT] =	EAGAIN,
	[STATUS_THREAD_LIMIT] =		EAGAIN,
	[STATUS_READ_ONLY] =		EROFS,
	[STATUS_PERM_DENIED] =		EACCES,
	[STATUS_NOT_DIR] =		ENOTDIR,
	[STATUS_NOT_FILE] =		EISDIR,
	[STATUS_NOT_SYMLINK] =		EINVAL,
	[STATUS_NOT_MOUNT] =		-1,
	[STATUS_NOT_FOUND] =		ENOENT,
	[STATUS_ALREADY_EXISTS] =	EEXIST,
	[STATUS_TOO_SMALL] =		ERANGE,
	[STATUS_TOO_LONG] =		ENAMETOOLONG,
	[STATUS_DIR_NOT_EMPTY] =	ENOTEMPTY,
	[STATUS_DIR_FULL] =		ENOSPC,
	[STATUS_UNKNOWN_FS] =		-1,
	[STATUS_CORRUPT_FS] =		EIO,
	[STATUS_FS_FULL] =		ENOSPC,
	[STATUS_SYMLINK_LIMIT] =	ELOOP,
	[STATUS_IN_USE] =		EBUSY,
	[STATUS_DEVICE_ERROR] =		EIO,
	[STATUS_PROCESS_RUNNING] =	-1,
	[STATUS_UNKNOWN_IMAGE] =	ENOEXEC,
	[STATUS_MALFORMED_IMAGE] =	ENOEXEC,
	[STATUS_MISSING_LIBRARY] =	ENOEXEC,
	[STATUS_MISSING_SYMBOL] =	ENOEXEC,
	[STATUS_DEST_UNREACHABLE] =	EHOSTUNREACH,
	[STATUS_TRY_AGAIN] =		EAGAIN,
};

/** Real location of errno. */
static int real_errno;

/** Get the location of errno.
 * @return		Pointer to errno. */
int *__libc_errno_location(void) {
	return &real_errno;
}

/** Set errno from a kernel status code.
 * @note		This function may not do the correct thing, POSIX is
 *			annoyingly inconsistent about error codes. Callers
 *			should be careful.
 * @param status	Status to set. */
void libc_status_to_errno(status_t status) {
	if(status < 0 || (size_t)status >= ARRAYSZ(status_to_errno_table)) {
		libc_fatal("unknown status code passed to status_to_errno()");
	}
	errno = status_to_errno_table[status];
	if(errno == -1) {
		libc_fatal("trying to map disallowed status to errno");
	}
}
