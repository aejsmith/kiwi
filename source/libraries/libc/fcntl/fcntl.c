/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		POSIX file control functions.
 */

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

	ret = handle_get_flags(fd, &kflags);
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

	ret = handle_set_flags(fd, kflags);
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

	ret = handle_duplicate(fd, dest, false, &new);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	return new;
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
	case F_SETFL:
		libc_stub("fcntl(F_{GETFL,SETFL})", true);
		return -1;
	default:
		errno = EINVAL;
	}

	va_end(args);
	return ret;
}
