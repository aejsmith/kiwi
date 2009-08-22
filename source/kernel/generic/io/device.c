/* Kiwi device manager
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Device manager.
 */

#include <console/kprintf.h>

#include <io/device.h>

#include <lib/string.h>

#include <mm/malloc.h>
#include <mm/safe.h>

#include <proc/handle.h>
#include <proc/process.h>

#include <assert.h>
#include <errors.h>
#include <fatal.h>
#include <init.h>
#include <kdbg.h>

#if CONFIG_DEVICE_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Root of the device tree. */
static device_dir_t *device_tree_root;

/** Create a child in a device directory.
 * @param dir		Directory to create in (should be locked).
 * @param name		Name for child (will be duplicated).
 * @return		Pointer to child structure (locked). */
static device_dir_t *device_dir_child_create(device_dir_t *dir, const char *name) {
	device_dir_t *child = kmalloc(sizeof(device_dir_t), MM_SLEEP);

	mutex_init(&child->lock, "device_dir_lock", 0);
	mutex_lock(&child->lock, 0);
	radix_tree_init(&child->children);
	child->header = DEVICE_TREE_DIR;
	child->parent = dir;
	child->name = kstrdup(name, MM_SLEEP);

	radix_tree_insert(&dir->children, child->name, child);
	dprintf("device: created directory %p(%s) under %p(%s)\n", child, name, dir, dir->name);
	return child;
}

/** Create a directory in the device tree.
 *
 * Creates a directory under an existing directory in the device tree.
 *
 * @param name		Name to give directory.
 * @param parent	Directory to create under.
 * @param dirp		Where to store pointer to directory structure.
 *
 * @return		0 on success, negative error code on failure.
 */
int device_dir_create_in(const char *name, device_dir_t *parent, device_dir_t **dirp) {
	device_dir_t *dir;

	if(!name || !parent || !dirp) {
		return -ERR_PARAM_INVAL;
	}

	mutex_lock(&parent->lock, 0);

	if(radix_tree_lookup(&parent->children, name)) {
		mutex_unlock(&parent->lock);
		return -ERR_ALREADY_EXISTS;
	}

	dir = device_dir_child_create(parent, name);
	mutex_unlock(&dir->lock);
	mutex_unlock(&parent->lock);
	*dirp = dir;
	return 0;
}

/** Create a directory in the device tree.
 *
 * Creates a directory in the device tree, and all directories leading to the
 * directory if they do not already exist. If the directory itself already
 * exists, an error is returned.
 *
 * @param path		Path to directory to create.
 * @param dirp		Where to store pointer to directory structure.
 *
 * @return		0 on success, negative error code on failure.
 */
int device_dir_create(const char *path, device_dir_t **dirp) {
	device_dir_t *curr = device_tree_root, *child;
	char *dir, *orig, *name, *tok;
	int ret;

	if(!path || !path[0] || path[0] != '/' || !dirp) {
		return -ERR_PARAM_INVAL;
	}

	/* Split into directory/name. */
	dir = orig = kdirname(path, MM_SLEEP);
	name = kbasename(path, MM_SLEEP);
	if(strchr(name, '/')) {
		ret = -ERR_ALREADY_EXISTS;
		goto out;
	}

	/* Create non-existant parent directories. */
	mutex_lock(&curr->lock, 0);
	while((tok = strsep(&dir, "/"))) {
		if(!tok[0]) {
			continue;
		} else if((child = radix_tree_lookup(&curr->children, tok))) {
			if(child->header != DEVICE_TREE_DIR) {
				mutex_unlock(&curr->lock);
				ret = -ERR_TYPE_INVAL;
				goto out;
			}

			mutex_lock(&child->lock, 0);
			mutex_unlock(&curr->lock);
			curr = child;
			continue;
		}

		/* Does not exist, create. */
		child = device_dir_child_create(curr, tok);
		mutex_unlock(&curr->lock);
		curr = child;
	}

	child = device_dir_child_create(curr, name);
	mutex_unlock(&child->lock);
	mutex_unlock(&curr->lock);
	*dirp = child;
	ret = 0;
out:
	kfree(orig);
	kfree(name);
	return ret;
}

/** Delete a device tree directory.
 *
 * Deletes a directory in the device tree referred to by the provided
 * structure. The directory must be empty.
 *
 * @param dir		Directory to delete.
 *
 * @return		0 on success, negative error code on failure.
 */
int device_dir_destroy(device_dir_t *dir) {
	assert(dir->parent);

	mutex_lock(&dir->parent->lock, 0);
	mutex_lock(&dir->lock, 0);

	if(!radix_tree_empty(&dir->children)) {
		return -ERR_IN_USE;
	}

	radix_tree_remove(&dir->parent->children, dir->name, NULL);
	mutex_unlock(&dir->parent->lock);
	mutex_unlock(&dir->lock);

	kfree(dir->name);
	kfree(dir);
	return 0;
}

/** Create a new device.
 *
 * Creates a new device and inserts it into the device tree. The device created
 * will not have a reference on it.
 *
 * @param name		Name of device to create.
 * @param parent	Parent directory.
 * @param type		Device type ID.
 * @param ops		Operations for the device.
 * @param data		Data used by the device's creator.
 * @param devicep	Where to store pointer to device structure.
 *
 * @return		0 on success, negative error code on failure.
 */
int device_create(const char *name, device_dir_t *parent, int type, device_ops_t *ops, void *data, device_t **devicep) {
	device_t *device;

	if(!name || !parent || !ops || !devicep) {
		return -ERR_PARAM_INVAL;
	}

	mutex_lock(&parent->lock, 0);

	device = kmalloc(sizeof(device_t), MM_SLEEP);
	refcount_set(&device->count, 0);
	device->header = DEVICE_TREE_DEVICE;
	device->parent = parent;
	device->name = kstrdup(name, MM_SLEEP);
	device->type = type;
	device->ops = ops;
	device->data = data;

	radix_tree_insert(&parent->children, device->name, device);
	dprintf("device: created device %p(%s) under %p(%s) (type: %d, ops: %p)\n",
	        device, device->name, parent, parent->name, type, ops);
	mutex_unlock(&parent->lock);
	*devicep = device;
	return 0;
}

/** Remove a device from the device tree.
 *
 * Removes a device from the device tree. The device must have no users.
 *
 * @todo		Sometime we'll need to allow devices to be removed when
 *			they have users, for example for hotplugging.
 *
 * @param device	Device to remove.
 *
 * @return		0 on success, negative error code on failure.
 */
int device_destroy(device_t *device) {
	/* Obtain the parent's lock. By doing so before checking the reference
	 * count of the device, we guarantee that the reference count will not
	 * change after it has been checked, because the parent must be locked
	 * to increase a device's reference count. */
	mutex_lock(&device->parent->lock, 0);

	if(refcount_get(&device->count) != 0) {
		mutex_unlock(&device->parent->lock);
		return -ERR_IN_USE;
	}

	radix_tree_remove(&device->parent->children, device->name, NULL);
	mutex_unlock(&device->parent->lock);

	dprintf("device: destroyed device %p(%s)\n", device, device->name);
	kfree(device->name);
	kfree(device);
	return 0;
}

/** Look up a device.
 *
 * Looks up a device in the device tree and increases its reference count.
 * Once the device is no longer required it should be released with
 * device_release().
 *
 * @param path		Path to device.
 * @param devicep	Where to store pointer to device structure.
 */
int device_get(const char *path, device_t **devicep) {
	device_dir_t *curr = device_tree_root, *child;
	char *dup, *orig, *tok;
	device_t *device;
	int ret;

	if(!path || !path[0] || path[0] != '/' || !devicep) {
		return -ERR_PARAM_INVAL;
	}

	dup = orig = kstrdup(path, MM_SLEEP);

	mutex_lock(&curr->lock, 0);
	while(true) {
		tok = strsep(&dup, "/");

		if(!tok[0]) {
			continue;
		} else if(!(child = radix_tree_lookup(&curr->children, tok))) {
			mutex_unlock(&curr->lock);
			kfree(orig);
			return -ERR_NOT_FOUND;
		} else if(child->header == DEVICE_TREE_DEVICE) {
			if((tok = strsep(&dup, "/"))) {
				mutex_unlock(&curr->lock);
				kfree(orig);
				return -ERR_TYPE_INVAL;
			}

			device = (device_t *)child;
			refcount_inc(&device->count);
			mutex_unlock(&curr->lock);

			if(device->ops->get && (ret = device->ops->get(device)) != 0) {
				refcount_dec(&device->count);
				return ret;
			}

			*devicep = device;
			return 0;
		} else {
			mutex_lock(&child->lock, 0);
			mutex_unlock(&curr->lock);
			curr = child;
		}
	}
}

/** Read from a device.
 *
 * Reads data from a device into a buffer. The device may not support the
 * operation - it is provided as a function rather than a request type because
 * it is supported by multiple device types.
 *
 * @param device	Device to read from.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param offset	Offset in the device to read from (only valid for
 *			certain device types).
 * @param bytesp	Where to store number of bytes read.
 *
 * @return		0 on success, negative error code on failure.
 */
int device_read(device_t *device, void *buf, size_t count, offset_t offset, size_t *bytesp) {
	if(!device || !buf || offset < 0) {
		return -ERR_PARAM_INVAL;
	} else if(!count) {
		if(bytesp) {
			*bytesp = 0;
		}
		return 0;
	} else if(!device->ops->read) {
		return -ERR_NOT_SUPPORTED;
	}

	assert(refcount_get(&device->count));

	return device->ops->read(device, buf, count, offset, bytesp);
}

/** Write to a device.
 *
 * Writes data to a device from buffer. The device may not support the
 * operation - it is provided as a function rather than a request type because
 * it is supported by multiple device types.
 *
 * @param device	Device to write to.
 * @param buf		Buffer containing data to write.
 * @param count		Number of bytes to write.
 * @param offset	Offset in the device to write to (only valid for
 *			certain device types).
 * @param bytesp	Where to store number of bytes read.
 *
 * @return		0 on success, negative error code on failure.
 */
int device_write(device_t *device, const void *buf, size_t count, offset_t offset, size_t *bytesp) {
	if(!device || !buf || offset < 0) {
		return -ERR_PARAM_INVAL;
	} else if(!count) {
		if(bytesp) {
			*bytesp = 0;
		}
		return 0;
	} else if(!device->ops->write) {
		return -ERR_NOT_SUPPORTED;
	}

	assert(refcount_get(&device->count));

	return device->ops->write(device, buf, count, offset, bytesp);
}

/** Perform a device-specific operation.
 *
 * Performs an operation that is specific to a device/device type.
 *
 * @param device	Device to perform operation on.
 * @param request	Operation number to perform.
 * @param in		Optional input buffer containing data to pass to the
 *			operation handler.
 * @param insz		Size of input buffer.
 * @param outp		Where to store pointer to data returned by the
 *			operation handler (optional).
 * @param outszp	Where to store size of data returned.
 *
 * @return		Positive value on success, negative error code on
 *			failure.
 */
int device_request(device_t *device, int request, void *in, size_t insz, void **outp, size_t *outszp) {
	if(!device) {
		return -ERR_PARAM_INVAL;
	} else if(!device->ops->request) {
		return -ERR_NOT_SUPPORTED;
	}

	assert(refcount_get(&device->count));

	return device->ops->request(device, request, in, insz, outp, outszp);
}

/** Release a device.
 *
 * Signal that a device is no longer required. This should be called once a
 * device obtained via device_get() is not needed any more.
 *
 * @param device	Device to release.
 */
void device_release(device_t *device) {
	if(device->ops->release) {
		device->ops->release(device);
	}
	refcount_dec(&device->count);
}

/** Print out a device directory's children.
 * @param tree		Radix tree to print.
 * @param indent	Indentation level. */
static void device_dir_dump(radix_tree_t *tree, int indent) {
	device_dir_t *dir;
	uint32_t *header;
	device_t *device;

	RADIX_TREE_FOREACH(tree, iter) {
		header = radix_tree_entry(iter, uint32_t);

		if(*header == DEVICE_TREE_DIR) {
			dir = radix_tree_entry(iter, device_dir_t);

			kprintf(LOG_NONE, "%*s%-*s %-18p\n", indent, "",
			        24 - indent, dir->name, dir->parent);
			device_dir_dump(&dir->children, indent + 2);
		} else {
			device = radix_tree_entry(iter, device_t);

			kprintf(LOG_NONE, "%*s%-*s %-18p %-4d %-5d %p\n", indent, "",
			        24 - indent, device->name, device->parent,
			        device->type, refcount_get(&device->count),
			        device->data);
		}
	}
}

/** Dump the device tree.
 *
 * Prints out the contents of the device tree.
 *
 * @param argc		Argument count.
 * @param argv		Argument array.
 *
 * @return		Always returns KDBG_OK.
 */
int kdbg_cmd_devices(int argc, char **argv) {
	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s\n", argv[0]);

		kprintf(LOG_NONE, "Prints out the contents of the device tree.\n");
		return KDBG_OK;
	}

	kprintf(LOG_NONE, "Name                     Parent             Type Count Data\n");
	kprintf(LOG_NONE, "====                     ======             ==== ===== ====\n");

	device_dir_dump(&device_tree_root->children, 0);
	return KDBG_OK;
}

/** Initialize the device manager. */
static void __init_text device_init(void) {
	device_tree_root = kmalloc(sizeof(device_dir_t), MM_FATAL);
	mutex_init(&device_tree_root->lock, "device_tree_root_lock", 0);
	radix_tree_init(&device_tree_root->children);
	device_tree_root->header = DEVICE_TREE_DIR;
	device_tree_root->parent = NULL;
	device_tree_root->name = (char *)"<root>";
}
INITCALL(device_init);

#if 0
# pragma mark System calls.
#endif

/** Closes a handle to a device.
 * @param info		Handle information structure.
 * @return		0 on success, negative error code on failure. */
static int device_handle_close(handle_info_t *info) {
	device_t *device = info->data;

	device_release(device);
	return 0;
}

/** Device handle operations. */
static handle_type_t device_handle_type = {
	.id = HANDLE_TYPE_DEVICE,
	.close = device_handle_close,
};

/** Open a handle to a device.
 *
 * Opens a handle to a device that can be used to perform other operations on
 * it. Once the device is no longer required, the handle should be closed with
 * handle_close().
 *
 * @param path		Device tree path for device to open.
 *
 * @return		Handle ID on success, negative error code on failure.
 */
handle_t sys_device_open(const char *path) {
	device_t *device;
	handle_t ret;

	if((ret = device_get(path, &device)) != 0) {
		return ret;
	} else if((ret = handle_create(&curr_proc->handles, &device_handle_type, device)) < 0) {
		device_release(device);
	}

	return ret;
}

/** Get the type of a device.
 *
 * Returns the type ID of the device referred to by a handle.
 *
 * @param handle	Handle to device to query.
 *
 * @return		Type ID on success, negative error code on failure.
 */
int sys_device_type(handle_t handle) {
	handle_info_t *info;
	device_t *device;
	int ret;

	if(!(ret = handle_get(&curr_proc->handles, handle, HANDLE_TYPE_DEVICE, &info))) {
		device = info->data;
		ret = device->type;
		handle_release(info);
	}

	return ret;
}

/** Read from a device.
 *
 * Reads data from a device into a buffer. The device may not support the
 * operation - it is provided as a function rather than a request type because
 * it is supported by multiple device types.
 *
 * @param handle	Handle to device to read from.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param offset	Offset in the device to read from (only valid for
 *			certain device types).
 * @param bytesp	Where to store number of bytes read.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_device_read(handle_t handle, void *buf, size_t count, offset_t offset, size_t *bytesp) {
	handle_info_t *info = NULL;
	device_t *device;
	size_t bytes = 0;
	int ret = 0, err;
	void *kbuf;

	/* Look up the device handle. */
	if((ret = handle_get(&curr_proc->handles, handle, HANDLE_TYPE_DEVICE, &info)) != 0) {
		goto out;
	}
	device = info->data;

	if(!count) {
		goto out;
	}

	/* Allocate a temporary buffer to read into. Don't use MM_SLEEP for
	 * this allocation because the process may provide a count larger than
	 * we can allocate in kernel space, in which case it would block
	 * forever. */
	if(!(kbuf = kmalloc(count, 0))) {
		ret = -ERR_NO_MEMORY;
		goto out;
	}

	ret = device_read(device, kbuf, count, offset, &bytes);
	if(bytes) {
		if((err = memcpy_to_user(buf, kbuf, bytes)) != 0) {
			ret = err;
		}
	}
	kfree(kbuf);
out:
	if(bytesp) {
		/* TODO: Something better than memcpy_to_user(). */
		if((err = memcpy_to_user(bytesp, &bytes, sizeof(size_t))) != 0) {
			ret = err;
		}
	}
	if(info) {
		handle_release(info);
	}
	return ret;
}

/** Write to a device.
 *
 * Writes data to a device from buffer. The device may not support the
 * operation - it is provided as a function rather than a request type because
 * it is supported by multiple device types.
 *
 * @param handle	Handle to device to write to.
 * @param buf		Buffer containing data to write.
 * @param count		Number of bytes to write.
 * @param offset	Offset in the device to write to (only valid for
 *			certain device types).
 * @param bytesp	Where to store number of bytes read.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_device_write(handle_t handle, const void *buf, size_t count, offset_t offset, size_t *bytesp) {
	handle_info_t *info = NULL;
	void *kbuf = NULL;
	device_t *device;
	size_t bytes = 0;
	int ret = 0, err;

	/* Look up the device handle. */
	if((ret = handle_get(&curr_proc->handles, handle, HANDLE_TYPE_DEVICE, &info)) != 0) {
		goto out;
	}
	device = info->data;

	if(count == 0) {
		goto out;
	}

	/* Copy the data to write across from userspace. Don't use MM_SLEEP for
	 * this allocation because the process may provide a count larger than
	 * we can allocate in kernel space, in which case it would block
	 * forever. */
	if(!(kbuf = kmalloc(count, 0))) {
		ret = -ERR_NO_MEMORY;
		goto out;
	} else if((ret = memcpy_from_user(kbuf, buf, count)) != 0) {
		goto out;
	}

	ret = device_write(device, kbuf, count, offset, &bytes);
out:
	if(kbuf) {
		kfree(kbuf);
	}
	if(bytesp) {
		/* TODO: Something better than memcpy_to_user(). */
		if((err = memcpy_to_user(bytesp, &bytes, sizeof(size_t))) != 0) {
			ret = err;
		}
	}
	if(info) {
		handle_release(info);
	}
	return ret;
}

/** Perform a device-specific operation.
 *
 * Performs an operation that is specific to a device/device type.
 *
 * @param args		Pointer to arguments structure.
 *
 * @return		Positive value on success, negative error code on
 *			failure.
 */
int sys_device_request(device_request_args_t *args) {
	void *kin = NULL, *kout = NULL;
	device_request_args_t kargs;
	handle_info_t *info;
	device_t *device;
	size_t koutsz;
	int ret, err;

	if((ret = memcpy_from_user(&kargs, args, sizeof(device_request_args_t))) != 0) {
		return ret;
	}

	/* Look up the device handle. */
	if((ret = handle_get(&curr_proc->handles, kargs.handle, HANDLE_TYPE_DEVICE, &info)) != 0) {
		return ret;
	}
	device = info->data;

	if(kargs.in && kargs.insz) {
		if(!(kin = kmalloc(kargs.insz, 0))) {
			ret = -ERR_NO_MEMORY;
			goto out;
		} else if((ret = memcpy_from_user(kin, kargs.in, kargs.insz)) != 0) {
			goto out;
		}
	}

	ret = device_request(device, kargs.request, kin, kargs.insz, &kout, &koutsz);
	if(kout) {
		assert(koutsz);
		if(koutsz > kargs.outsz) {
			ret = -ERR_BUF_TOO_SMALL;
		} else if((err = memcpy_to_user(kargs.out, kout, koutsz)) != 0) {
			ret = err;
		} else if(kargs.bytesp) {
			if((err = memcpy_to_user(kargs.bytesp, &koutsz, sizeof(size_t))) != 0) {
				ret = err;
			}
		}
	}
out:
	if(kin) {
		kfree(kin);
	}
	if(kout) {
		kfree(kout);
	}
	handle_release(info);
	return ret;
}
