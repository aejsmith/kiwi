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
 * @brief		Filesystem functions.
 */

#include <boot/fs.h>
#include <boot/memory.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <assert.h>

/** Array of filesystem implementations. */
static fs_type_t *filesystem_types[] = {
	&ext2_fs_type,
	&iso9660_fs_type,
};

/** Create a filesystem handle.
 * @param path		Path to filesystem entry.
 * @param mount		Mount the entry resides on.
 * @param directory	Whether the entry is a directory.
 * @param data		Implementation-specific data pointer.
 * @return		Pointer to handle structure. */
fs_handle_t *fs_handle_create(fs_mount_t *mount, bool directory, void *data) {
	fs_handle_t *handle = kmalloc(sizeof(fs_handle_t));
	handle->mount = mount;
	handle->directory = directory;
	handle->data = data;
	handle->count = 1;
	return handle;
}

/** Probe a disk for filesystems.
 * @param disk		Disk to probe.
 * @return		Pointer to mount if detected, NULL if not. */
fs_mount_t *fs_probe(disk_t *disk) {
	fs_mount_t *mount;
	size_t i;

	mount = kmalloc(sizeof(fs_mount_t));
	for(i = 0; i < ARRAYSZ(filesystem_types); i++) {
		memset(mount, 0, sizeof(fs_mount_t));
		mount->disk = disk;
		mount->type = filesystem_types[i];
		if(mount->type->mount(mount)) {
			return mount;
		}
	}

	kfree(mount);
	return NULL;
}

/** Structure containing data for fs_open(). */
typedef struct fs_open_data {
	const char *name;		/**< Name of entry being searched for. */
	fs_handle_t *handle;		/**< Handle to found entry. */
} fs_open_data_t;

/** Directory iteration callback for fs_open().
 * @param name		Name of entry.
 * @param handle	Handle to entry.
 * @param _data		Pointer to data structure.
 * @return		Whether to continue iteration. */
static bool fs_open_cb(const char *name, fs_handle_t *handle, void *_data) {
	fs_open_data_t *data = _data;

	if(strcmp(name, data->name) == 0) {
		handle->count++;
		data->handle = handle;
		return false;
	} else {
		return true;
	}
}

/** Open a handle to a file/directory.
 * @param mount		Mount to open from. If NULL, current FS will be used.
 * @param path		Path to entry to open.
 * @return		Pointer to handle on success, NULL on failure. */
fs_handle_t *fs_open(fs_mount_t *mount, const char *path) {
	char *dup, *orig, *tok;
	fs_open_data_t data;
	fs_handle_t *handle;

	if(!mount) {
		if(!(mount = current_disk->fs)) {
			return NULL;
		}
	}

	/* Use the provided open() implementation if any. */
	if(mount->type->open) {
		return mount->type->open(mount, path);
	}

	/* Strip leading / characters from the path. */
	while(*path == '/') {
		path++;
	}

	assert(mount->type->read_dir);
	assert(mount->root);
	handle = mount->root;
	handle->count++;

	/* Loop through each element of the path string. The string must be
	 * duplicated so that it can be modified. */
	dup = orig = kstrdup(path);
	while(true) {
		tok = strsep(&dup, "/");
		if(tok == NULL) {
			/* The last token was the last element of the path
			 * string, return the node we're currently on. */
			kfree(orig);
			return handle;
		} else if(!handle->directory) {
			/* The previous node was not a directory: this means
			 * the path string is trying to treat a non-directory
			 * as a directory. Reject this. */
			fs_close(handle);
			kfree(orig);
			return NULL;
		} else if(!tok[0]) {
			/* Zero-length path component, do nothing. */
			continue;
		}

		/* Search the directory for the entry. */
		data.name = tok;
		data.handle = NULL;
		if(!mount->type->read_dir(handle, fs_open_cb, &data) || !data.handle) {
			fs_close(handle);
			kfree(orig);
			return NULL;
		}

		fs_close(handle);
		handle = data.handle;
	}
}

/** Close a handle.
 * @param handle	Handle to close. */
void fs_close(fs_handle_t *handle) {
	if(--handle->count == 0) {
		handle->mount->type->close(handle);
		kfree(handle);
	}
}

/** Read from a file.
 * @param handle	Handle to file to read from.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param offset	Offset in the file to read from.
 * @return		Whether the read was successful. */
bool fs_file_read(fs_handle_t *handle, void *buf, size_t count, offset_t offset) {
	assert(!handle->directory);
	return handle->mount->type->read(handle, buf, count, offset);
}

/** Get the size of a file.
 * @param handle	Handle to the file.
 * @return		Size of the file. */
offset_t fs_file_size(fs_handle_t *handle) {
	assert(!handle->directory);
	return handle->mount->type->size(handle);
}

/** Read directory entries.
 * @param handle	Handle to directory.
 * @param cb		Callback to call on each entry.
 * @param arg		Data to pass to callback.
 * @return		Whether read successfully. */
bool fs_dir_read(fs_handle_t *handle, fs_dir_read_cb_t cb, void *arg) {
	assert(handle->directory);
	return handle->mount->type->read_dir(handle, cb, arg);
}
