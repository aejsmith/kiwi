/*
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

#include <io/device.h>

#include <lib/string.h>

#include <mm/malloc.h>
#include <mm/safe.h>

#include <proc/process.h>

#include <assert.h>
#include <console.h>
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
device_t *device_tree_root;

/** Standard device directories. */
device_t *device_bus_dir;

/** Closes a handle to a device.
 * @param handle	Handle to the device. */
static void device_object_close(object_handle_t *handle) {
	device_t *device = (device_t *)handle->object;

	if(device->ops && device->ops->close) {
		device->ops->close(device, handle->data);
	}

	device_release(device);
}

/** Signal that a device is being waited for.
 * @param wait		Wait information structure.
 * @return		0 on success, negative error code on failure. */
static int device_object_wait(object_wait_t *wait) {
	device_t *device = (device_t *)wait->handle->object;

	if(device->ops && (!device->ops->wait || !device->ops->unwait)) {
		return -ERR_NOT_IMPLEMENTED;
	}

	return device->ops->wait(device, wait->handle->data, wait);
}

/** Stop waiting for a device.
 * @param wait		Wait information structure. */
static void device_object_unwait(object_wait_t *wait) {
	device_t *device = (device_t *)wait->handle->object;
	assert(device->ops);
	return device->ops->unwait(device, wait->handle->data, wait);
}

/** Handle a page fault on a device-backed region.
 * @param region	Region fault occurred in.
 * @param addr		Virtual address of fault (rounded down to base of page).
 * @param reason	Reason for the fault.
 * @param access	Type of access that caused the fault.
 * @return		Whether fault was handled. */
static bool device_object_fault(vm_region_t *region, ptr_t addr, int reason, int access) {
	offset_t offset = region->obj_offset + (addr - region->start);
	device_t *device = (device_t *)region->handle->object;
	phys_ptr_t page;
	int ret;

	assert(device->ops && device->ops->fault);

	/* Ask the device for a page. */
	if((ret = device->ops->fault(device, region->handle->data, offset, &page)) != 0) {
		dprintf("device: failed to get page from offset %" PRIu64 " in %p(%s) (%d)\n",
		        offset, device, device->name, ret);
		return false;
	}

	/* Insert into page map. */
	page_map_insert(&region->as->pmap, addr, page, region->flags & VM_REGION_WRITE,
	                region->flags & VM_REGION_EXEC, MM_SLEEP);
	return true;
}

/** Device object type structure. */
static object_type_t device_object_type = {
	.id = OBJECT_TYPE_DEVICE,
	.close = device_object_close,
	.wait = device_object_wait,
	.unwait = device_object_unwait,
	.fault = device_object_fault,
};

/** Create a new device tree node.
 *
 * Creates a new node in the device tree. The device created will not have a
 * reference on it. The device can have no operations, in which case it will
 * simply act as a container for other devices.
 *
 * @param name		Name of device to create (will be duplicated).
 * @param parent	Parent device. Must not be an alias.
 * @param ops		Operations for the device (can be NULL).
 * @param data		Data used by the device operations.
 * @param attrs		Optional array of attributes for the device (will be
 *			duplicated).
 * @param count		Number of attributes.
 * @param devicep	Where to store pointer to device structure.
 *
 * @return		0 on success, negative error code on failure.
 */
int device_create(const char *name, device_t *parent, device_ops_t *ops, void *data,
                  device_attr_t *attrs, size_t count, device_t **devicep) {
	device_t *device = NULL;
	size_t i;
	int ret;

	if(!name || strlen(name) >= DEVICE_NAME_MAX || !parent || parent->dest || !devicep) {
		return -ERR_PARAM_INVAL;
	}

	/* Check if a child already exists with this name. */
	mutex_lock(&parent->lock);
	if(radix_tree_lookup(&parent->children, name)) {
		ret = -ERR_ALREADY_EXISTS;
		goto fail;
	}

	device = kmalloc(sizeof(device_t), MM_SLEEP);
	object_init(&device->obj, &device_object_type, (ops->fault) ? OBJECT_MAPPABLE : 0);
	mutex_init(&device->lock, "device_lock", 0);
	refcount_set(&device->count, 0);
	radix_tree_init(&device->children);
	device->name = kstrdup(name, MM_SLEEP);
	device->parent = parent;
	device->dest = NULL;
	device->ops = ops;
	device->data = data;

	if(attrs) {
		/* Ensure the attribute structures are valid. Do validity
		 * checking before allocating anything to make it easier to
		 * clean up if an invalid structure is found. */
		for(i = 0; i < count; i++) {
			if(!attrs[i].name || strlen(attrs[i].name) >= DEVICE_NAME_MAX) {
				ret = -ERR_PARAM_INVAL;
				goto fail;
			} else if(attrs[i].type == DEVICE_ATTR_STRING) {
				if(!attrs[i].value.string || strlen(attrs[i].value.string) >= DEVICE_ATTR_MAX) {
					ret = -ERR_PARAM_INVAL;
					goto fail;
				}
			}
		}

		/* Duplicate the structures, then fix up the data. */
		device->attrs = kmemdup(attrs, sizeof(device_attr_t) * count, MM_SLEEP);
		device->attr_count = count;
		for(i = 0; i < device->attr_count; i++) {
			device->attrs[i].name = kstrdup(device->attrs[i].name, MM_SLEEP);
			if(device->attrs[i].type == DEVICE_ATTR_STRING) {
				device->attrs[i].value.string = kstrdup(device->attrs[i].value.string, MM_SLEEP);
			}
		}
	} else {
		device->attrs = NULL;
		device->attr_count = 0;
	}

	/* Attach to the parent. */
	refcount_inc(&parent->count);
	radix_tree_insert(&parent->children, device->name, device);
	mutex_unlock(&parent->lock);

	dprintf("device: created device %p(%s) under %p(%s) (ops: %p, data: %p)\n",
	        device, device->name, parent, parent->name, ops, data);
	*devicep = device;
	return 0;
fail:
	if(device) {
		kfree(device->name);
		kfree(device);
	}
	mutex_unlock(&parent->lock);
	return ret;
}

/** Create an alias for a device.
 *
 * Creates an alias for another device in the device tree. Any attempts to get
 * the alias will return the device it is an alias for. Any aliases created
 * for a device should be destroyed before destroying the device itself.
 *
 * @param name		Name to give alias.
 * @param parent	Device to create alias under.
 * @param dest		Destination device.
 * @param devicep	Where to store pointer to alias structure.
 *
 * @return		0 on success, negative error code on failure. Can only
 *			fail if device name already exists.
 */
int device_alias(const char *name, device_t *parent, device_t *dest, device_t **devicep) {
	device_t *device;

	if(!name || strlen(name) >= DEVICE_NAME_MAX || !parent || !dest || !devicep) {
		return -ERR_PARAM_INVAL;
	}

	/* Check if a child already exists with this name. */
	mutex_lock(&parent->lock);
	if(radix_tree_lookup(&parent->children, name)) {
		mutex_unlock(&parent->lock);
		return -ERR_ALREADY_EXISTS;
	}

	refcount_inc(&dest->count);

	device = kmalloc(sizeof(device_t), MM_SLEEP);
	object_init(&device->obj, &device_object_type, 0);
	mutex_init(&device->lock, "device_alias_lock", 0);
	refcount_set(&device->count, 0);
	radix_tree_init(&device->children);
	device->name = kstrdup(name, MM_SLEEP);
	device->parent = parent;
	device->dest = dest;
	device->ops = NULL;
	device->data = NULL;
	device->attrs = NULL;
	device->attr_count = 0;

	refcount_inc(&parent->count);
	radix_tree_insert(&parent->children, device->name, device);
	mutex_unlock(&parent->lock);

	dprintf("device: created alias %p(%s) under %p(%s) (dest: %p)\n",
	        device, device->name, parent, parent->name, dest);
	*devicep = device;
	return 0;
}

/** Remove a device from the device tree.
 *
 * Removes a device from the device tree. The device must have no users. All
 * aliases of the device should be destroyed before the device itself.
 *
 * @todo		Sometime we'll need to allow devices to be removed when
 *			they have users, for example for hotplugging.
 *
 * @param device	Device to remove.
 *
 * @return		0 on success, negative error code on failure. Cannot
 *			fail for aliases.
 */
int device_destroy(device_t *device) {
	size_t i;

	assert(device->parent);

	mutex_lock(&device->parent->lock);
	mutex_lock(&device->lock);

	if(refcount_get(&device->count) != 0) {
		mutex_unlock(&device->lock);
		mutex_unlock(&device->parent->lock);
		return -ERR_IN_USE;
	}

	radix_tree_remove(&device->parent->children, device->name, NULL);
	refcount_dec(&device->parent->count);
	mutex_unlock(&device->parent->lock);

	if(device->dest) {
		refcount_dec(&device->dest->count);
	}

	/* Free up attributes if any. */
	if(device->attrs) {
		for(i = 0; i < device->attr_count; i++) {
			kfree((char *)device->attrs[i].name);
			if(device->attrs[i].type == DEVICE_ATTR_STRING) {
				kfree((char *)device->attrs[i].value.string);
			}
		}

		kfree(device->attrs);
	}

	dprintf("device: destroyed device %p(%s) (parent: %p)\n", device, device->name, device->parent);
	object_destroy(&device->obj);
	kfree(device->name);
	kfree(device);
	return 0;
}

/** Internal iteration function.
 * @param device	Device we're currently on.
 * @param func		Function to call on devices.
 * @param data		Data argument to pass to function.
 * @return		Whether to continue lookup. */
static bool device_iterate_internal(device_t *device, device_iterate_t func, void *data) {
	device_t *child;
	int ret;

	if((ret = func(device, data)) != 1) {
		return (ret == 0) ? false : true;
	}

	RADIX_TREE_FOREACH(&device->children, iter) {
		child = radix_tree_entry(iter, device_t);

		if(!device_iterate_internal(child, func, data)) {
			return false;
		}
	}

	return true;
}

/** Iterate through the device tree.
 *
 * Calls the specified function on a device and all its children (and all their
 * children, etc).
 *
 * @todo		Meep, we have small kernel stacks. Recursive lookup
 *			probably isn't a very good idea. Then again, the device
 *			tree shouldn't go *too* deep.
 *
 * @param start		Starting device.
 * @param func		Function to call on devices.
 * @param data		Data argument to pass to function.
 */
void device_iterate(device_t *start, device_iterate_t func, void *data) {
	device_iterate_internal(start, func, data);
}

/** Look up a device.
 *
 * Looks up an entry in the device tree and increases its reference count. Once
 * the device is no longer needed it should be released with device_release().
 *
 * @param path		Path to device.
 * @param devicep	Where to store pointer to device structure.
 */
int device_lookup(const char *path, device_t **devicep) {
	device_t *device = device_tree_root, *child;
	char *dup, *orig, *tok;

	if(!path || !path[0] || path[0] != '/' || !devicep) {
		return -ERR_PARAM_INVAL;
	}

	dup = orig = kstrdup(path, MM_SLEEP);

	mutex_lock(&device->lock);
	while((tok = strsep(&dup, "/"))) {
		if(!tok[0]) {
			continue;
		} else if(!(child = radix_tree_lookup(&device->children, tok))) {
			mutex_unlock(&device->lock);
			kfree(orig);
			return -ERR_NOT_FOUND;
		}

		/* Move down to the device and then iterate through until we
		 * reach an entry that isn't an alias. */
		do {
			mutex_lock(&child->lock);
			mutex_unlock(&device->lock);
			device = child;
		} while((child = device->dest));
	}

	refcount_inc(&device->count);
	mutex_unlock(&device->lock);
	*devicep = device;
	return 0;
}

/** Get a device attribute.
 *
 * Gets an attribute from a device, and optionally checks that it is the
 * required type. Returned structure must NOT be modified.
 *
 * @param device	Device to get from.
 * @param name		Attribute name.
 * @param type		Required type (if -1 will not check).
 *
 * @return		Pointer to attribute structure if found, NULL if not.
 */
device_attr_t *device_attr(device_t *device, const char *name, int type) {
	size_t i;

	assert(device);
	assert(name);

	for(i = 0; i < device->attr_count; i++) {
		if(strcmp(device->attrs[i].name, name) == 0) {
			if(type != -1 && (int)device->attrs[i].type != type) {
				return NULL;
			}
			return &device->attrs[i];
		}
	}

	return NULL;
}

/** Release a device.
 *
 * Signal that a device is no longer required. This should be called once a
 * device obtained via device_lookup() is not needed any more.
 *
 * @param device	Device to release.
 */
void device_release(device_t *device) {
	refcount_dec(&device->count);
}

/** Create a handle to a device.
 * @param device	Device to create handle to.
 * @param handlep	Where to store pointer to handle structure.
 * @return		0 on success, negative error code on failure. */
int device_open(device_t *device, object_handle_t **handlep) {
	void *data;
	int ret;

	assert(device);
	assert(handlep);

	mutex_lock(&device->lock);

	if(device->ops && device->ops->open && (ret = device->ops->open(device, &data)) != 0) {
		mutex_unlock(&device->lock);
		return ret;
	}

	*handlep = object_handle_create(&device->obj, data);
	mutex_unlock(&device->lock);
	return 0;
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
int device_read(object_handle_t *handle, void *buf, size_t count, offset_t offset, size_t *bytesp) {
	device_t *device;
	size_t bytes;
	int ret;

	assert(handle);
	assert(handle->object->type->id == OBJECT_TYPE_DEVICE);
	assert(buf);

	device = (device_t *)handle->object;
	if(offset < 0) {
		return -ERR_PARAM_INVAL;
	} else if(!device->ops || !device->ops->read) {
		return -ERR_NOT_SUPPORTED;
	} else if(!count) {
		if(bytesp) {
			*bytesp = 0;
		}
		return 0;
	}

	ret = device->ops->read(device, handle->data, buf, count, offset, &bytes);
	if(bytesp) {
		*bytesp = bytes;
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
int device_write(object_handle_t *handle, const void *buf, size_t count, offset_t offset, size_t *bytesp) {
	device_t *device;
	size_t bytes;
	int ret;

	assert(handle);
	assert(handle->object->type->id == OBJECT_TYPE_DEVICE);
	assert(buf);

	device = (device_t *)handle->object;
	if(offset < 0) {
		return -ERR_PARAM_INVAL;
	} else if(!device->ops || !device->ops->write) {
		return -ERR_NOT_SUPPORTED;
	} else if(!count) {
		if(bytesp) {
			*bytesp = 0;
		}
		return 0;
	}

	ret = device->ops->write(device, handle->data, buf, count, offset, &bytes);
	if(bytesp) {
		*bytesp = bytes;
	}
	return ret;
}

/** Perform a device-specific operation.
 * @param handle	Handle to device to perform operation on.
 * @param request	Operation number to perform.
 * @param in		Optional input buffer containing data to pass to the
 *			operation handler.
 * @param insz		Size of input buffer.
 * @param outp		Where to store pointer to data returned by the
 *			operation handler (optional).
 * @param outszp	Where to store size of data returned.
 * @return		Positive value on success, negative error code on
 *			failure. */
int device_request(object_handle_t *handle, int request, void *in, size_t insz, void **outp, size_t *outszp) {
	device_t *device;

	assert(handle);
	assert(handle->object->type->id == OBJECT_TYPE_DEVICE);

	device = (device_t *)handle->object;
	if(!device->ops || !device->ops->request) {
		return -ERR_NOT_SUPPORTED;
	}

	return device->ops->request(device, handle->data, request, in, insz, outp, outszp);
}

/** Print out a device's children.
 * @param tree		Radix tree to print.
 * @param indent	Indentation level. */
static void device_dump_children(radix_tree_t *tree, int indent) {
	device_t *device;

	RADIX_TREE_FOREACH(tree, iter) {
		device = radix_tree_entry(iter, device_t);

		kprintf(LOG_NONE, "%*s%-*s %-18p %-18p %d\n", indent, "",
		        24 - indent, device->name, device, device->parent,
		        refcount_get(&device->count));
		if(device->dest) {
			device_dump_children(&device->dest->children, indent + 2);
		} else {
			device_dump_children(&device->children, indent + 2);
		}
	}
}

/** Print out device information.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		Always returns KDBG_OK. */
int kdbg_cmd_device(int argc, char **argv) {
	device_t *device;
	unative_t val;
	size_t i;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s [<addr>]\n\n", argv[0]);

		kprintf(LOG_NONE, "If no arguments are given, shows the contents of the device tree. Otherwise\n");
		kprintf(LOG_NONE, "shows information about a single device.\n");
		return KDBG_OK;
	} else if(argc != 1 && argc != 2) {
		kprintf(LOG_NONE, "Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
		return KDBG_FAIL;
	}

	if(argc == 1) {
		kprintf(LOG_NONE, "Name                     Address            Parent             Count\n");
		kprintf(LOG_NONE, "====                     =======            ======             =====\n");

		device_dump_children(&device_tree_root->children, 0);
		return KDBG_OK;
	}

	if(kdbg_parse_expression(argv[1], &val, NULL) != KDBG_OK) {
		return KDBG_FAIL;
	}
	device = (device_t *)((ptr_t)val);

	kprintf(LOG_NONE, "Device %p(%s)\n", device, device->name);
	kprintf(LOG_NONE, "=================================================\n");
	kprintf(LOG_NONE, "Count:       %d\n", refcount_get(&device->count));
	kprintf(LOG_NONE, "Parent:      %p\n", device->parent);
	if(device->dest) {
		kprintf(LOG_NONE, "Destination: %p(%s)\n", device->dest, device->dest->name);
	}
	kprintf(LOG_NONE, "Ops:         %p\n", device->ops);
	kprintf(LOG_NONE, "Data:        %p\n", device->data);

	if(!device->attrs) {
		return KDBG_OK;
	}

	kprintf(LOG_NONE, "\nAttributes:\n");

	for(i = 0; i < device->attr_count; i++) {
		kprintf(LOG_NONE, "  %s - ", device->attrs[i].name);
		switch(device->attrs[i].type) {
		case DEVICE_ATTR_UINT8:
			kprintf(LOG_NONE, "uint8: %" PRIu8 " (0x%" PRIx8 ")\n",
			        device->attrs[i].value.uint8, device->attrs[i].value.uint8);
			break;
		case DEVICE_ATTR_UINT16:
			kprintf(LOG_NONE, "uint16: %" PRIu16 " (0x%" PRIx16 ")\n",
			        device->attrs[i].value.uint16, device->attrs[i].value.uint16);
			break;
		case DEVICE_ATTR_UINT32:
			kprintf(LOG_NONE, "uint32: %" PRIu32 " (0x%" PRIx32 ")\n",
			        device->attrs[i].value.uint32, device->attrs[i].value.uint32);
			break;
		case DEVICE_ATTR_UINT64:
			kprintf(LOG_NONE, "uint64: %" PRIu64 " (0x%" PRIx64 ")\n",
			        device->attrs[i].value.uint64, device->attrs[i].value.uint64);
			break;
		case DEVICE_ATTR_STRING:
			kprintf(LOG_NONE, "string: '%s'\n", device->attrs[i].value.string);
			break;
		default:
			kprintf(LOG_NONE, "Invalid!\n");
			break;
		}
	}
	return KDBG_OK;
}

/** Initialise the device manager. */
static void __init_text device_init(void) {
	device_tree_root = kcalloc(1, sizeof(device_t), MM_FATAL);
	mutex_init(&device_tree_root->lock, "device_root_lock", 0);
	refcount_set(&device_tree_root->count, 0);
	radix_tree_init(&device_tree_root->children);
	device_tree_root->name = (char *)"<root>";

	if(device_create("bus", device_tree_root, NULL, NULL, NULL, 0, &device_bus_dir) != 0) {
		fatal("Could not create bus directory in device tree");
	}
}
INITCALL(device_init);
#if 0
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
#endif
