/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		POSIX file control functions.
 */

#include <kernel/fs.h>
#include <kernel/object.h>
#include <kernel/status.h>

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>

#include "../libc.h"

/** Perform the F_GETFD command.
 * @param fd		File descriptor.
 * @return		FD flags on success, -1 on failure. */
static int fcntl_getfd(int fd) {
	int kflags, flags = 0;
	status_t ret;

	ret = kern_handle_control(fd, HANDLE_GET_LFLAGS, 0, &kflags);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	flags |= ((kflags & HANDLE_INHERITABLE) ? 0 : FD_CLOEXEC);
	return flags;
}

/** Perform the F_SETFD command.
 * @param fd		File descriptor.
 * @param flags		New flags.
 * @return		0 on success, -1 on failure. */
static int fcntl_setfd(int fd, int flags) {
	int kflags = 0;
	status_t ret;

	kflags |= ((flags & FD_CLOEXEC) ? 0 : HANDLE_INHERITABLE);

	ret = kern_handle_control(fd, HANDLE_SET_LFLAGS, kflags, NULL);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	return 0;
}

/** Perform the F_DUPFD command.
 * @param fd		File descriptor.
 * @param dest		Minimum ID for new descriptor.
 * @return		New FD on success, -1 on failure. */
static int fcntl_dupfd(int fd, int dest) {
	status_t ret;
	handle_t new;

	ret = kern_handle_duplicate(fd, dest, false, &new);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	return new;
}

/** Perform the F_GETFL command.
 * @param fd		File descriptor.
 * @return		File status flags on success, -1 on failure. */
static int fcntl_getfl(int fd) {
	int kflags, flags = 0;
	status_t ret;

	ret = kern_handle_control(fd, HANDLE_GET_FLAGS, 0, &kflags);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	flags |= ((kflags & FILE_NONBLOCK) ? 0 : O_NONBLOCK);
	flags |= ((kflags & FILE_APPEND) ? 0 : O_APPEND);
	return flags;
}

/** Perform the F_SETFL command.
 * @param fd		File descriptor.
 * @param flags		New flags.
 * @return		0 on success, -1 on failure. */
static int fcntl_setfl(int fd, int flags) {
	int kflags = 0;
	status_t ret;

	kflags |= ((flags & O_NONBLOCK) ? 0 : FILE_NONBLOCK);
	kflags |= ((flags & O_APPEND) ? 0 : FILE_APPEND);

	ret = kern_handle_control(fd, HANDLE_SET_FLAGS, kflags, NULL);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	return 0;
}

/** Control file descriptor behaviour.
 *
 * Controls the behaviour of a file descriptor according to the specified
 * command. The following commands are currently recognised:
 *  F_DUPFD  - Duplicates the given file descriptor. The new descriptor will be
 *             the lowest available that is greater than or equal to the third
 *             argument. It will refer to the same open file description as the
 *             old descriptor. The return value (on success) is the new file
 *             descriptor.
 *  F_GETFD  - Get file descriptor flags. These flags are associated with a
 *             single file descriptor, and do not affect other descriptors
 *             referring to the same open file. The return value (on success)
 *             is the set of flags currently set on the FD.
 *  F_SETFD  - Set file descriptor flags (see F_GETFD). The return value (on
 *             success) is 0.
 *  F_GETFL  - Get file status flags and access flags. These flags are
 *             stored for each open file description, and modifying them affects
 *             other file descriptors referring to the same description (FDs
 *             duplicated by dup()/dup2()/F_DUPFD and duplicated by fork() refer
 *             to the same file description). The return value (on success) is
 *             the set of flags currently set on the file description.
 *  F_SETFL  - Set file status flags and access flags (see F_GETFL). The return
 *             value (on success) is 0.
 * 
 * @param fd		File descriptor to control.
 * @param cmd		Command to perform.
 * @param ...		Optional argument specific to the command.
 *
 * @return		Dependent on the command performed on success, -1 on
 *			failure (errno will be set appropriately).
 */
int fcntl(int fd, int cmd, ...) {
	int arg, ret = -1;
	va_list args;

	va_start(args, cmd);

	switch(cmd) {
	case F_GETFD:
		ret = fcntl_getfd(fd);
		break;
	case F_SETFD:
		arg = va_arg(args, int);
		ret = fcntl_setfd(fd, arg);
		break;
	case F_DUPFD:
		arg = va_arg(args, int);
		ret = fcntl_dupfd(fd, arg);
		break;
	case F_GETFL:
		ret = fcntl_getfl(fd);
		break;
	case F_SETFL:
		arg = va_arg(args, int);
		ret = fcntl_setfl(fd, arg);
		break;
	default:
		errno = EINVAL;
		break;
	}

	va_end(args);
	return ret;
}
