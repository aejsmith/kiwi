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
 * @brief		TAR file extractor.
 */

#include <io/fs.h>

#include <mm/malloc.h>

#include <lib/string.h>
#include <lib/tar.h>
#include <lib/utility.h>

#include <console.h>
#include <object.h>
#include <status.h>

/** Convert a TAR mode to a set of rights.
 * @param mode		Mode to convert (the part of interest should be in the
 *			lowest 3 bits).
 * @return		Converted rights. */
static inline object_rights_t mode_to_rights(uint16_t mode) {
	object_rights_t rights = 0;

	if(mode & TOREAD) {
		rights |= FILE_READ;
	}
	if(mode & TOWRITE) {
		rights |= FILE_WRITE;
	}
	if(mode & TOEXEC) {
		rights |= FILE_EXECUTE;
	}
	return rights;
}

/** Handle an entry in a TAR file.
 * @param header	Header for the entry.
 * @param data		Data for the entry.
 * @param size		Size of data.
 * @param prefix	Prefix for path string.
 * @return		Status code describing result of the operation. */
static status_t handle_tar_entry(tar_header_t *header, void *data, size_t size, const char *prefix) {
	object_acl_t acl;
	object_security_t security = { 0, 0, &acl };
	object_handle_t *handle;
	object_rights_t rights;
	uint16_t mode;
	status_t ret;
	size_t bytes;
	char *path;

	/* Work out the path to the entry. */
	if(prefix) {
		path = kmalloc(strlen(prefix) + strlen(header->name) + 2, MM_SLEEP);
		strcpy(path, prefix);
		strcat(path, "/");
		strcat(path, header->name);
	} else {
		path = header->name;
	}

	/* Convert the mode to an ACL. */
	mode = strtoul(header->mode, NULL, 8);
	object_acl_init(&acl);
	rights = mode_to_rights(mode >> 6);
	object_acl_add_entry(&acl, ACL_ENTRY_USER, -1, rights);
	rights = mode_to_rights(mode >> 3);
	object_acl_add_entry(&acl, ACL_ENTRY_GROUP, -1, rights);
	rights = mode_to_rights(mode);
	object_acl_add_entry(&acl, ACL_ENTRY_OTHERS, 0, rights);

	/* Handle the entry based on its type flag. */
	switch(header->typeflag) {
	case REGTYPE:
	case AREGTYPE:
		ret = file_open(path, FILE_WRITE, 0, FILE_CREATE_ALWAYS, &security, &handle);
		if(ret != STATUS_SUCCESS) {
			goto out;
		}

		ret = file_write(handle, data, size, &bytes);
		if(ret != STATUS_SUCCESS) {
			object_handle_release(handle);
			goto out;
		} else if(bytes != size) {
			ret = STATUS_DEVICE_ERROR;
			object_handle_release(handle);
			goto out;
		}

		object_handle_release(handle);
		break;
	case DIRTYPE:
		ret = dir_create(path, &security);
		if(ret != STATUS_SUCCESS) {
			goto out;
		}
		break;
	case SYMTYPE:
		ret = symlink_create(path, header->linkname);
		if(ret != STATUS_SUCCESS) {
			goto out;
		}
		break;
	default:
		kprintf(LOG_DEBUG, "tar: unhandled type flag '%c'\n", header->typeflag);
		break;
	}

	ret = STATUS_SUCCESS;
out:
	object_acl_destroy(&acl);
	if(prefix) {
		kfree(path);
	}
	return ret;
}

/** Extract a TAR file.
 * @param handle	Handle to file.
 * @param dest		If not NULL, will be prepended to path strings in the
 *			TAR file. If NULL and any path strings are relative,
 *			they will be extracted to the current directory.
 * @return		Status code describing result of the operation. */
status_t tar_extract(object_handle_t *handle, const char *dest) {
	tar_header_t *header;
	offset_t offset = 0;
	size_t bytes, size;
	void *data = NULL;
	status_t ret;

	header = kmalloc(sizeof(tar_header_t), MM_SLEEP);

	while(true) {
		/* Read in the next header. */
		ret = file_pread(handle, header, sizeof(*header), offset, &bytes);
		if(ret != STATUS_SUCCESS) {
			goto fail;
		} else if(bytes < 2) {
			ret = (offset) ? STATUS_MALFORMED_IMAGE : STATUS_UNKNOWN_IMAGE;
			goto fail;
		}

		/* Two NULL bytes in the name field indicates EOF. */
		if(!header->name[0] && !header->name[1]) {
			break;
		}

		/* Check validity of the header. */
		if(bytes != sizeof(*header) || strncmp(header->magic, "ustar", 5) != 0) {
			ret = (offset) ? STATUS_MALFORMED_IMAGE : STATUS_UNKNOWN_IMAGE;
			goto fail;
		}

		/* All fields in the header are stored as ASCII - convert the
		 * size to an integer (base 8). */
		size = strtoul(header->size, NULL, 8);

		/* Read in the entry data. */
		if(size) {
			data = kmalloc(size, 0);
			if(!data) {
				ret = STATUS_NO_MEMORY;
				goto fail;
			}

			ret = file_pread(handle, data, size, offset + 512, &bytes);
			if(ret != STATUS_SUCCESS) {
				goto fail;
			} else if(bytes != size) {
				ret = STATUS_MALFORMED_IMAGE;
				goto fail;
			}
		}

		/* Process the entry. */
		ret = handle_tar_entry(header, data, size, dest);
		if(ret != STATUS_SUCCESS) {
			goto fail;
		}

		if(data) {
			kfree(data);
			data = NULL;
		}

		/* 512 for the header, plus the file size if necessary. */
		offset += 512;
		if(size) {
			offset += ROUND_UP(size, 512);
		}
	}

	kfree(header);
	return STATUS_SUCCESS;
fail:
	if(data) {
		kfree(data);
	}
	kfree(header);
	return ret;
}
