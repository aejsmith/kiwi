/*
 * Copyright (C) 2009-2013 Alex Smith
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
 * @brief		File object functions/definitions.
 *
 * The interface in this file is common to all file types (filesystem entries,
 * devices, sockets, pipes). Each of these types in addition has its own
 * interface for operations specific to that type, defined in a separate header.
 */

#ifndef __KERNEL_FILE_H
#define __KERNEL_FILE_H

#include <kernel/object.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Possible file types. */
typedef enum file_type {
	FILE_TYPE_REGULAR,		/**< Regular file. */
	FILE_TYPE_DIR,			/**< Directory. */
	FILE_TYPE_SYMLINK,		/**< Symbolic link. */
	FILE_TYPE_BLOCK,		/**< Block device. */
	FILE_TYPE_CHAR,			/**< Character device. */
	FILE_TYPE_FIFO,			/**< FIFO (pipe). */
	FILE_TYPE_SOCKET,		/**< Socket. */
} file_type_t;

/** File information structure. */
typedef struct file_info {
	node_id_t id;			/**< Node ID. */
	mount_id_t mount;		/**< Mount ID. */
	file_type_t type;		/**< Type of the file. */
	size_t block_size;		/**< I/O block size. */
	offset_t size;			/**< Total size of file on filesystem. */
	size_t links;			/**< Number of links to the node. */

	/** Node times, all in nanoseconds since the UNIX epoch. */
	nstime_t created;		/**< Time of creation. */
	nstime_t accessed;		/**< Time of last access. */
	nstime_t modified;		/**< Time last modified. */
} file_info_t;

/** Directory entry information structure. */
typedef struct dir_entry {
	size_t length;			/**< Length of this structure including name. */
	node_id_t id;			/**< ID of the node for the entry. */
	mount_id_t mount;		/**< ID of the mount the node is on. */
	char name[];			/**< Name of entry (null-terminated). */
} dir_entry_t;

/** I/O vector structure. */
typedef struct io_vec {
	void *buffer;			/**< Buffer to read from/write to. */
	size_t size;			/**< Size of the buffer. */
} io_vec_t;

/** Access rights for files. */
#define FILE_RIGHT_READ		(1<<0)	/**< Open for reading. */
#define FILE_RIGHT_WRITE	(1<<1)	/**< Open for writing. */
#define FILE_RIGHT_EXECUTE	(1<<2)	/**< Open for execution. */

/** Behaviour flags for file handles. */
#define FILE_NONBLOCK		(1<<0)	/**< I/O operations on the handle should not block. */
#define FILE_APPEND		(1<<1)	/**< Before each write, offset is set to the end of the file. */

/** Operations for kern_file_seek(). */
#define FILE_SEEK_SET		1	/**< Set to the exact position specified. */
#define FILE_SEEK_ADD		2	/**< Add the supplied value to the current offset. */
#define FILE_SEEK_END		3	/**< Set to the end of the file plus the supplied value. */

/** Events that can occur on file objects. */
#define FILE_EVENT_READABLE	1	/**< Wait for the device to be readable. */
#define FILE_EVENT_WRITABLE	2	/**< Wait for the device to be writable. */

extern status_t kern_file_read(handle_t handle, void *buf, size_t size,
	offset_t offset, size_t *bytesp);
extern status_t kern_file_write(handle_t handle, const void *buf, size_t size,
	offset_t offset, size_t *bytesp);

extern status_t kern_file_read_vecs(handle_t handle, const io_vec_t *vecs,
	size_t count, offset_t offset, size_t *bytesp);
extern status_t kern_file_write_vecs(handle_t handle, const io_vec_t *vecs,
	size_t count, offset_t offset, size_t *bytesp);

extern status_t kern_file_read_dir(handle_t handle, dir_entry_t *buf, size_t size);
extern status_t kern_file_rewind_dir(handle_t handle);

extern status_t kern_file_resize(handle_t handle, offset_t size);
extern status_t kern_file_seek(handle_t handle, unsigned action, offset_t offset,
	offset_t *resultp);
extern status_t kern_file_info(handle_t handle, file_info_t *info);
extern status_t kern_file_sync(handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_FILE_H */
