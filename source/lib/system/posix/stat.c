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
 * @brief		POSIX file information functions.
 */

#include <kernel/fs.h>
#include <kernel/object.h>
#include <kernel/status.h>

#include <sys/stat.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "../libc.h"

/** Convert a set of rights to a mode.
 * @param rights	Rights to convert.
 * @return		Converted mode (only lowest 3 bits). */
static inline uint16_t rights_to_mode(object_rights_t rights) {
	uint16_t mode = 0;

	if(rights & FILE_RIGHT_READ) {
		mode |= S_IROTH;
	}
	if(rights & FILE_RIGHT_WRITE) {
		mode |= S_IWOTH;
	}
	if(rights & FILE_RIGHT_EXECUTE) {
		mode |= S_IXOTH;
	}
	return mode;
}

/** Convert a kernel information structure to a stat structure.
 * @param info		Kernel information structure.
 * @param security	Security attributes for the entry.
 * @param statp		Stat structure. */
static void file_info_to_stat(file_info_t *info, object_security_t *security, struct stat *restrict statp) {
	uint16_t umode = 0, gmode = 0, omode = 0;
	bool user = false, group = false;
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
	case FILE_TYPE_REGULAR:
		statp->st_mode = S_IFREG;
		break;
	case FILE_TYPE_DIR:
		statp->st_mode = S_IFDIR;
		break;
	case FILE_TYPE_SYMLINK:
		statp->st_mode = S_IFLNK;
		break;
	case FILE_TYPE_BLKDEV:
		statp->st_mode = S_IFBLK;
		break;
	case FILE_TYPE_CHRDEV:
		statp->st_mode = S_IFCHR;
		break;
	case FILE_TYPE_FIFO:
		statp->st_mode = S_IFIFO;
		break;
	case FILE_TYPE_SOCK:
		statp->st_mode = S_IFSOCK;
		break;
	}

	/* Convert the ACL to a set of file permission bits. */
	acl = object_security_acl(security);
	for(i = 0; i < acl->count; i++) {
		switch(acl->entries[i].type) {
		case ACL_ENTRY_USER:
			if(acl->entries[i].value == -1) {
				umode |= rights_to_mode(acl->entries[i].rights);
				user = true;
			}
			break;
		case ACL_ENTRY_GROUP:
			if(acl->entries[i].value == -1) {
				gmode |= rights_to_mode(acl->entries[i].rights);
				group = true;
			}
			break;
		case ACL_ENTRY_OTHERS:
			omode |= rights_to_mode(acl->entries[i].rights);
			break;
		}
	}

	/* If we've not seen a user/group entry for the owning user/group, put
	 * their permissions as the value for others. */
	statp->st_mode |= (((user) ? umode : omode) << 6);
	statp->st_mode |= (((group) ? gmode : omode) << 3);
	statp->st_mode |= omode;
}

/** Get information about a filesystem entry.
 * @param fd		File descriptor to entry.
 * @param statp		Structure to fill in.
 * @return		0 on success, -1 on failure. */
int fstat(int fd, struct stat *statp) {
	object_security_t security;
	file_info_t info;
	status_t ret;

	switch(kern_object_type(fd)) {
	case OBJECT_TYPE_FILE:
		ret = kern_file_info(fd, &info);
		if(ret != STATUS_SUCCESS) {
			libc_status_to_errno(ret);
			return -1;
		}
		break;
	case OBJECT_TYPE_DEVICE:
		memset(&info, 0, sizeof(info));
		info.type = (isatty(fd)) ? FILE_TYPE_CHRDEV : FILE_TYPE_BLKDEV;
		/* FIXME: Get correct values for block devices here. */
		info.block_size = 1;
		info.size = 1;
		info.links = 1;
		break;
	case -1:
		errno = EBADF;
		return -1;
	default:
		errno = ENOTSUP;
		return -1;
	}

	ret = kern_object_security(fd, &security);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	file_info_to_stat(&info, &security, statp);
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
	file_info_t info;
	status_t ret;

	ret = kern_fs_info(path, false, &info);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	ret = kern_fs_security(path, false, &security);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	file_info_to_stat(&info, &security, statp);
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
	file_info_t info;
	status_t ret;

	ret = kern_fs_info(path, true, &info);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	ret = kern_fs_security(path, true, &security);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	file_info_to_stat(&info, &security, statp);
	object_security_destroy(&security);
	return 0;
}
