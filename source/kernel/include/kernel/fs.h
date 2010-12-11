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

#include <kernel/object.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Limitations for the filesystem. */
#define FS_PATH_MAX		4096	/**< Maximum length of a path string. */
#define FS_NESTED_LINK_MAX	16	/**< Maximum number of nested symbolic links. */

/** Directory entry information structure. */
typedef struct dir_entry {
	size_t length;			/**< Length of this structure including name. */
	node_id_t id;			/**< ID of the node for the entry. */
	mount_id_t mount;		/**< ID of the mount the node is on. */
	char name[];			/**< Name of entry (NULL-terminated). */
} dir_entry_t;

/** Possible filesystem node types. */
typedef enum file_type {
	FILE_TYPE_REGULAR,		/**< Regular file. */
	FILE_TYPE_DIR,			/**< Directory. */
	FILE_TYPE_SYMLINK,		/**< Symbolic link. */
	FILE_TYPE_BLKDEV,		/**< Block device. */
	FILE_TYPE_CHRDEV,		/**< Character device. */
	FILE_TYPE_FIFO,			/**< FIFO (named pipe). */
	FILE_TYPE_SOCK,			/**< Socket. */
} file_type_t;

/** Filesystem entry information structure. */
typedef struct file_info {
	node_id_t id;			/**< Node ID. */
	mount_id_t mount;		/**< Mount ID. */
	file_type_t type;		/**< Type of the file. */
	size_t block_size;		/**< I/O block size. */
	offset_t size;			/**< Total size of file on filesystem. */
	size_t links;			/**< Number of links to the node. */

	/** Node times, all in microseconds since the UNIX epoch. */
	useconds_t created;		/**< Time of creation. */
	useconds_t accessed;		/**< Time of last access. */
	useconds_t modified;		/**< Time last modified. */
} file_info_t;

/** Mount information structure. */
typedef struct mount_info {
	mount_id_t id;			/**< Mount ID. */
	char type[32];			/**< Name of the filesystem type. */
	char path[FS_PATH_MAX];		/**< Path that mount is at. */
	char device[256];		/**< Device tree path of device. */
} mount_info_t;

/** Access rights for files. */
#define FILE_RIGHT_READ		(1<<0)	/**< Open for reading. */
#define FILE_RIGHT_WRITE	(1<<1)	/**< Open for writing. */
#define FILE_RIGHT_EXECUTE	(1<<2)	/**< Open for execution. */

/** Behaviour flags for file handles. */
#define FILE_NONBLOCK		(1<<0)	/**< I/O operations on the handle should not block. */
#define FILE_APPEND		(1<<1)	/**< Before each write, offset is set to the end of the file. */

/** Values for the create option of kern_file_open(). */
#define FILE_CREATE		1	/**< Create the file if it doesn't exist. */
#define FILE_CREATE_ALWAYS	2	/**< If the file already exists, fail, else create it. */

/** Operations for kern_file_seek(). */
#define FILE_SEEK_SET		1	/**< Set the offset to the exact position specified. */
#define FILE_SEEK_ADD		2	/**< Add the supplied value to the current offset. */
#define FILE_SEEK_END		3	/**< Set the offset to the end of the file plus the supplied value. */

extern status_t kern_file_open(const char *path, object_rights_t rights, int flags,
                               int create, const object_security_t *security,
                               handle_t *handlep);
extern status_t kern_file_read(handle_t handle, void *buf, size_t count, size_t *bytesp);
extern status_t kern_file_pread(handle_t handle, void *buf, size_t count, offset_t offset,
                                size_t *bytesp);
extern status_t kern_file_write(handle_t handle, const void *buf, size_t count, size_t *bytesp);
extern status_t kern_file_pwrite(handle_t handle, const void *buf, size_t count, offset_t offset,
                                 size_t *bytesp);
extern status_t kern_file_resize(handle_t handle, offset_t size);
extern status_t kern_file_seek(handle_t handle, int action, rel_offset_t offset, offset_t *newp);
extern status_t kern_file_info(handle_t handle, file_info_t *infop);
extern status_t kern_file_sync(handle_t handle);

extern status_t kern_dir_create(const char *path, const object_security_t *security);
extern status_t kern_dir_read(handle_t handle, dir_entry_t *buf, size_t size);

extern status_t kern_symlink_create(const char *path, const char *target);
extern status_t kern_symlink_read(const char *path, char *buf, size_t size);

extern status_t kern_fs_mount(const char *device, const char *path, const char *type,
                              const char *opts);
extern status_t kern_fs_mount_info(mount_info_t *infop, size_t *countp);
extern status_t kern_fs_unmount(const char *path);

extern status_t kern_fs_getcwd(char *buf, size_t size);
extern status_t kern_fs_setcwd(const char *path);
extern status_t kern_fs_setroot(const char *path);
extern status_t kern_fs_info(const char *path, bool follow, file_info_t *infop);
#ifndef KERNEL
extern status_t kern_fs_security(const char *path, bool follow, object_security_t *securityp);
#endif
extern status_t kern_fs_set_security(const char *path, bool follow, const object_security_t *security);
extern status_t kern_fs_link(const char *source, const char *dest);
extern status_t kern_fs_unlink(const char *path);
extern status_t kern_fs_rename(const char *source, const char *dest);
extern status_t kern_fs_sync(void);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_FS_H */
