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
 * @brief		POSIX file information functions.
 */

#include <kernel/fs.h>
#include <kernel/object.h>
#include <kernel/status.h>

#include <sys/stat.h>

#include <errno.h>
#include <string.h>

#include "../libc.h"

/** Convert a set of rights to a mode.
 * @param rights	Rights to convert.
 * @return		Converted mode (only lowest 3 bits). */
static inline uint16_t rights_to_mode(object_rights_t rights) {
	uint16_t mode = 0;

	if(rights & FS_READ) {
		mode |= S_IROTH;
	}
	if(rights & FS_WRITE) {
		mode |= S_IWOTH;
	}
	if(rights & FS_EXECUTE) {
		mode |= S_IXOTH;
	}
	return mode;
}

/** Convert a kernel information structure to a stat structure.
 * @param info		Kernel information structure.
 * @param security	Security attributes for the entry.
 * @param statp		Stat structure. */
static void fs_info_to_stat(fs_info_t *info, object_security_t *security, struct stat *restrict statp) {
	object_acl_t *acl;
	size_t i;

	memset(statp, 0, sizeof(*statp));
	statp->st_dev = info->mount;
	statp->st_ino = info->id;
	statp->st_nlink = info->links;
	statp->st_size = info->size;
	statp->st_blksize = info->block_size;
	statp->st_atime = (info->accessed / 1000000);
	statp->st_mtime = (info->modified / 1000000);
	statp->st_ctime = (info->created / 1000000);
	statp->st_uid = security->uid;
	statp->st_gid = security->gid;

	/* TODO. */
	statp->st_blocks = 0;

	/* Determine the file type mode. */
	switch(info->type) {
	case FS_NODE_FILE:
		statp->st_mode = S_IFREG;
		break;
	case FS_NODE_DIR:
		statp->st_mode = S_IFDIR;
		break;
	case FS_NODE_SYMLINK:
		statp->st_mode = S_IFLNK;
		break;
	case FS_NODE_BLKDEV:
		statp->st_mode = S_IFBLK;
		break;
	case FS_NODE_CHRDEV:
		statp->st_mode = S_IFCHR;
		break;
	case FS_NODE_FIFO:
		statp->st_mode = S_IFIFO;
		break;
	case FS_NODE_SOCK:
		statp->st_mode = S_IFSOCK;
		break;
	}

	/* Convert the ACL to a set of file permission bits. */
	acl = object_security_acl(security);
	for(i = 0; i < acl->count; i++) {
		switch(acl->entries[i].type) {
		case ACL_ENTRY_USER:
			if(acl->entries[i].value == -1) {
				statp->st_mode |= (rights_to_mode(acl->entries[i].rights) << 6);
			}
			break;
		case ACL_ENTRY_GROUP:
			if(acl->entries[i].value == -1) {
				statp->st_mode |= (rights_to_mode(acl->entries[i].rights) << 3);
			}
			break;
		case ACL_ENTRY_OTHERS:
			statp->st_mode |= rights_to_mode(acl->entries[i].rights);
			break;
		}
	}
}

/** Get information about a filesystem entry.
 * @param fd		File descriptor to entry.
 * @param statp		Structure to fill in.
 * @return		0 on success, -1 on failure. */
int fstat(int fd, struct stat *statp) {
	object_security_t security;
	fs_info_t info;
	status_t ret;

	ret = fs_handle_info(fd, &info);
	if(ret != STATUS_SUCCESS) {
		if(ret == STATUS_INVALID_HANDLE && object_type(fd) != -1) {
			errno = ENOTSUP;
			return -1;
		}
		libc_status_to_errno(ret);
		return -1;
	}

	ret = object_security(fd, &security);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	fs_info_to_stat(&info, &security, statp);
	object_security_destroy(&security);
	return 0;
}

/** Get information about a filesystem entry.
 * @param path		Path to entry. If it refers to a symbolic link, it will
 *			not be followed.
 * @param statp		Structure to fill in.
 * @param		0 on success, -1 on failure. */
int lstat(const char *restrict path, struct stat *restrict statp) {
	object_security_t security;
	fs_info_t info;
	status_t ret;

	ret = fs_info(path, false, &info);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	ret = fs_security(path, false, &security);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	fs_info_to_stat(&info, &security, statp);
	object_security_destroy(&security);
	return 0;
}

/** Get information about a filesystem entry.
 * @param path		Path to entry. If it refers to a symbolic link, it will
 *			be followed.
 * @param statp		Structure to fill in.
 * @param		0 on success, -1 on failure. */
int stat(const char *restrict path, struct stat *restrict statp) {
	object_security_t security;
	fs_info_t info;
	status_t ret;

	ret = fs_info(path, true, &info);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	ret = fs_security(path, true, &security);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	fs_info_to_stat(&info, &security, statp);
	object_security_destroy(&security);
	return 0;
}
