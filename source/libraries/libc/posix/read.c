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
 * @brief		POSIX read functions.
 *
 * @fixme		When a failure occurs after partially reading the data
 *			the kernel updates the handle's offset by the number of
 *			bytes that were successfully read. This is possibly
 *			incorrect for POSIX.
 */

#include <kernel/device.h>
#include <kernel/fs.h>
#include <kernel/object.h>
#include <kernel/status.h>

#include <errno.h>
#include <unistd.h>

#include "../libc.h"

/** Read from a particular position in a file.
 *
 * Reads from the specified position in a file. The file descriptor's current
 * offset will be ignored, and will not be updated after the read.
 *
 * @param fd		File descriptor to read from.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param offset	Offset into the file to read from.
 *
 * @return		Number of bytes read on success, -1 on failure (errno
 *			will be set appropriately).
 */
ssize_t pread(int fd, void *buf, size_t count, off_t offset) {
	status_t ret;
	size_t bytes;
	int type;

	if(offset < 0) {
		errno = EINVAL;
		return -1;
	}

	type = object_type(fd);
	switch(type) {
	case OBJECT_TYPE_FILE:
		ret = fs_file_pread(fd, buf, count, offset, &bytes);
		if(ret != STATUS_SUCCESS && (ret != STATUS_INTERRUPTED || bytes == 0)) {
			if(ret == STATUS_ACCESS_DENIED) {
				errno = EBADF;
			} else {
				libc_status_to_errno(ret);
			}
			return -1;
		}
		return (ssize_t)bytes;
	case OBJECT_TYPE_DEVICE:
		ret = device_read(fd, buf, count, offset, &bytes);
		if(ret != STATUS_SUCCESS && (ret != STATUS_INTERRUPTED || bytes == 0)) {
			libc_status_to_errno(ret);
			return -1;
		}
		return (ssize_t)bytes;
	case OBJECT_TYPE_DIR:
		errno = EISDIR;
		return -1;
	case -1:
		errno = EBADF;
		return -1;
	default:
		errno = ENOTSUP;
		return -1;
	}
}

/** Read from a file.
 *
 * Reads from a file. After the read, the file descriptor's offset will be
 * updated by the number of bytes written.
 *
 * @param fd		File descriptor to read from.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 *
 * @return		Number of bytes read on success, -1 on failure (errno
 *			will be set appropriately).
 */
ssize_t read(int fd, void *buf, size_t count) {
	status_t ret;
	size_t bytes;
	int type;

	type = object_type(fd);
	switch(type) {
	case OBJECT_TYPE_FILE:
		ret = fs_file_read(fd, buf, count, &bytes);
		if(ret != STATUS_SUCCESS && (ret != STATUS_INTERRUPTED || bytes == 0)) {
			if(ret == STATUS_ACCESS_DENIED) {
				errno = EBADF;
			} else {
				libc_status_to_errno(ret);
			}
			return -1;
		}
		return (ssize_t)bytes;
	case OBJECT_TYPE_DEVICE:
		ret = device_read(fd, buf, count, 0, &bytes);
		if(ret != STATUS_SUCCESS && (ret != STATUS_INTERRUPTED || bytes == 0)) {
			libc_status_to_errno(ret);
			return -1;
		}
		return (ssize_t)bytes;
	case OBJECT_TYPE_DIR:
		errno = EISDIR;
		return -1;
	case -1:
		errno = EBADF;
		return -1;
	default:
		errno = ENOTSUP;
		return -1;
	}
}
