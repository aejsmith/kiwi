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
 * @brief		Filesystem functions.
 */

#ifndef __KERNEL_FS_H
#define __KERNEL_FS_H

#ifdef KERNEL
# include <public/types.h>
#else
# include <kernel/types.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Directory entry information structure. */
typedef struct fs_dir_entry {
	size_t length;			/**< Length of this structure including name. */
	node_id_t id;			/**< ID of the node for the entry. */
	mount_id_t mount;		/**< ID of the mount the node is on. */
	char name[];			/**< Name of entry (NULL-terminated). */
} fs_dir_entry_t;

/** Possible filesystem node types. */
typedef enum fs_node_type {
	FS_NODE_FILE,			/**< Regular file. */
	FS_NODE_DIR,			/**< Directory. */
	FS_NODE_SYMLINK,		/**< Symbolic link. */
	FS_NODE_BLKDEV,			/**< Block device. */
	FS_NODE_CHRDEV,			/**< Character device. */
	FS_NODE_FIFO,			/**< FIFO (named pipe). */
	FS_NODE_SOCK,			/**< Socket. */
} fs_node_type_t;

/** Filesystem node information structure. */
typedef struct fs_info {
	node_id_t id;			/**< Node ID. */
	mount_id_t mount;		/**< Mount ID. */
	fs_node_type_t type;		/**< Type of the node. */
	size_t blksize;			/**< I/O block size. */
	offset_t size;			/**< Total size of file on filesystem. */
	size_t links;			/**< Number of links to the node. */
} fs_info_t;

/** Limitations for the filesystem. */
#define FS_PATH_MAX		4096	/**< Maximum length of a path string. */
#define FS_NESTED_LINK_MAX	16	/**< Maximum number of nested symbolic links. */

/** Behaviour flags for both FS handle types. */
#define FS_NONBLOCK		(1<<0)	/**< I/O operations on the handle should not block. */

/** Behaviour flags for fs_file_open(). */
#define FS_FILE_READ		(1<<1)	/**< Open for reading. */
#define FS_FILE_WRITE		(1<<2)	/**< Open for writing. */
#define FS_FILE_APPEND		(1<<3)	/**< Before each write, offset is set to the end of the file. */

/** Operations for fs_handle_seek(). */
#define FS_SEEK_SET		1	/**< Set the offset to the exact position specified. */
#define FS_SEEK_ADD		2	/**< Add the supplied value to the current offset. */
#define FS_SEEK_END		3	/**< Set the offset to the end of the file plus the supplied value. */

extern status_t SYSCALL(fs_file_create)(const char *path);
extern status_t SYSCALL(fs_file_open)(const char *path, int flags, handle_t *handlep);
extern status_t SYSCALL(fs_file_read)(handle_t handle, void *buf, size_t count, size_t *bytesp);
extern status_t SYSCALL(fs_file_pread)(handle_t handle, void *buf, size_t count, offset_t offset,
                                       size_t *bytesp);
extern status_t SYSCALL(fs_file_write)(handle_t handle, const void *buf, size_t count, size_t *bytesp);
extern status_t SYSCALL(fs_file_pwrite)(handle_t handle, const void *buf, size_t count,
                                        offset_t offset, size_t *bytesp);
extern status_t SYSCALL(fs_file_resize)(handle_t handle, offset_t size);

extern status_t SYSCALL(fs_dir_create)(const char *path);
extern status_t SYSCALL(fs_dir_open)(const char *path, int flags, handle_t *handlep);
extern status_t SYSCALL(fs_dir_read)(handle_t handle, fs_dir_entry_t *buf, size_t size);

extern status_t SYSCALL(fs_handle_seek)(handle_t handle, int action, rel_offset_t offset, offset_t *newp);
extern status_t SYSCALL(fs_handle_info)(handle_t handle, fs_info_t *info);
extern status_t SYSCALL(fs_handle_sync)(handle_t handle);

extern status_t SYSCALL(fs_symlink_create)(const char *path, const char *target);
extern status_t SYSCALL(fs_symlink_read)(const char *path, char *buf, size_t size);

extern status_t SYSCALL(fs_mount)(const char *device, const char *path, const char *type, const char *opts);
extern status_t SYSCALL(fs_unmount)(const char *path);
extern status_t SYSCALL(fs_sync)(void);
extern status_t SYSCALL(fs_getcwd)(char *buf, size_t size);
extern status_t SYSCALL(fs_setcwd)(const char *path);
extern status_t SYSCALL(fs_setroot)(const char *path);
extern status_t SYSCALL(fs_info)(const char *path, bool follow, fs_info_t *info);
extern status_t SYSCALL(fs_link)(const char *source, const char *dest);
extern status_t SYSCALL(fs_unlink)(const char *path);
extern status_t SYSCALL(fs_rename)(const char *source, const char *dest);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_FS_H */
