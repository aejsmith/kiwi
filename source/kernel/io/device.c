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
 * @brief		Device manager.
 */

#include <io/device.h>

#include <lib/string.h>

#include <mm/malloc.h>
#include <mm/safe.h>
#include <mm/vm.h>

#include <proc/process.h>

#include <assert.h>
#include <console.h>
#include <kdbg.h>
#include <kernel.h>
#include <status.h>

#if CONFIG_DEVICE_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Access to grant others in the default ACL. */
#define DEVICE_DEFAULT_RIGHTS	(DEVICE_RIGHT_QUERY | DEVICE_RIGHT_READ | DEVICE_RIGHT_WRITE)

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

	refcount_dec(&device->count);
}

/** Signal that a device is being waited for.
 * @param handle	Handle to device.
 * @param event		Event to wait for.
 * @param sync		Internal data pointer.
 * @return		Status code describing result of the operation. */
static status_t device_object_wait(object_handle_t *handle, int event, void *sync) {
	device_t *device = (device_t *)handle->object;

	if(device->ops && device->ops->wait && device->ops->unwait) {
		return device->ops->wait(device, handle->data, event, sync);
	} else {
		return STATUS_INVALID_EVENT;
	}
}

/** Stop waiting for a device.
 * @param handle	Handle to device.
 * @param event		Event that was being waited for.
 * @param sync		Internal data pointer. */
static void device_object_unwait(object_handle_t *handle, int event, void *sync) {
	device_t *device = (device_t *)handle->object;
	assert(device->ops);
	return device->ops->unwait(device, handle->data, event, sync);
}

/** Check if a device can be memory-mapped.
 * @param handle	Handle to device.
 * @param flags		Mapping flags (VM_MAP_*).
 * @return		STATUS_SUCCESS if can be mapped, status code explaining
 *			why if not. */
static status_t device_object_mappable(object_handle_t *handle, int flags) {
	device_t *device = (device_t *)handle->object;

	/* Cannot create private mappings to devices. */
	if(flags & VM_MAP_PRIVATE) {
		return STATUS_NOT_SUPPORTED;
	}

	if(device->ops->mappable) {
		assert(device->ops->get_page);
		return device->ops->mappable(device, handle->data, flags);
	} else {
		return (device->ops->get_page) ? STATUS_SUCCESS : STATUS_NOT_SUPPORTED;
	}
}

/** Get a page from a device.
 * @param handle	Handle to device to get page from.
 * @param offset	Offset into device to get page from.
 * @param physp		Where to store physical address of page.
 * @return		Status code describing result of the operation. */
static status_t device_object_get_page(object_handle_t *handle, offset_t offset, phys_ptr_t *physp) {
	device_t *device = (device_t *)handle->object;
	status_t ret;

	assert(device->ops && device->ops->get_page);

	/* Ask the device for a page. */
	ret = device->ops->get_page(device, handle->data, offset, physp);
	if(ret != STATUS_SUCCESS) {
		dprintf("device: failed to get page from offset %" PRIu64 " in %p(%s) (%d)\n",
		        offset, device, device->name, ret);
	}

	return ret;
}

/** Device object type structure. */
static object_type_t device_object_type = {
	.id = OBJECT_TYPE_DEVICE,
	.close = device_object_close,
	.wait = device_object_wait,
	.unwait = device_object_unwait,
	.mappable = device_object_mappable,
	.get_page = device_object_get_page,
};

/** Create a new device tree node.
 *
 * Creates a new node in the device tree. The device created will not have a
 * reference on it. The device can have no operations, in which case it will
 * simply act as a container for other devices.
 *
 * @param name		Name of device to create (will be duplicated).
 * @param parent	Parent device. Must not be an alias.
 * @param ops		Pointer to operations for the device (can be NULL).
 * @param data		Implementation-specific data pointer.
 * @param attrs		Optional array of attributes for the device (will be
 *			duplicated).
 * @param count		Number of attributes.
 * @param devicep	Where to store pointer to device structure (can be NULL).
 *
 * @return		Status code describing result of the operation.
 */
status_t device_create(const char *name, device_t *parent, device_ops_t *ops, void *data,
                       device_attr_t *attrs, size_t count, device_t **devicep) {
	object_security_t security;
	device_t *device = NULL;
	object_acl_t acl;
	status_t ret;
	size_t i;

	if(!name || strlen(name) >= DEVICE_NAME_MAX || !parent || parent->dest) {
		return STATUS_INVALID_ARG;
	}

	/* Check if a child already exists with this name. */
	mutex_lock(&parent->lock);
	if(radix_tree_lookup(&parent->children, name)) {
		ret = STATUS_ALREADY_EXISTS;
		goto fail;
	}

	/* Create an ACL and security structure. TODO: Restrict device access. */
	object_acl_init(&acl);
	object_acl_add_entry(&acl, ACL_ENTRY_OTHERS, 0, DEVICE_DEFAULT_RIGHTS);
	security.uid = 0;
	security.gid = 0;
	security.acl = &acl;

	device = kmalloc(sizeof(device_t), MM_SLEEP);
	object_init(&device->obj, &device_object_type, &security, NULL);
	mutex_init(&device->lock, "device_lock", 0);
	refcount_set(&device->count, 0);
	radix_tree_init(&device->children);
	list_init(&device->aliases);
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
				ret = STATUS_INVALID_ARG;
				goto fail;
			} else if(attrs[i].type == DEVICE_ATTR_STRING) {
				if(!attrs[i].value.string || strlen(attrs[i].value.string) >= DEVICE_ATTR_MAX) {
					ret = STATUS_INVALID_ARG;
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
	if(devicep) {
		*devicep = device;
	}
	return STATUS_SUCCESS;
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
 * Creates an alias for another device in the device tree. Any attempts to open
 * the alias will open the device it is an alias for.
 *
 * @param name		Name to give alias.
 * @param parent	Device to create alias under.
 * @param dest		Destination device. If this is an alias, the new alias
 *			will refer to the destination, not the alias itself.
 * @param devicep	Where to store pointer to alias structure (can be NULL).
 *
 * @return		Status code describing result of the operation.
 */
status_t device_alias(const char *name, device_t *parent, device_t *dest, device_t **devicep) {
	device_t *device;

	if(!name || strlen(name) >= DEVICE_NAME_MAX || !parent || !dest) {
		return STATUS_INVALID_ARG;
	}

	/* If the destination is an alias, use it's destination. */
	if(dest->dest) {
		dest = dest->dest;
	}

	/* Check if a child already exists with this name. */
	mutex_lock(&parent->lock);
	if(radix_tree_lookup(&parent->children, name)) {
		mutex_unlock(&parent->lock);
		return STATUS_ALREADY_EXISTS;
	}

	device = kmalloc(sizeof(device_t), MM_SLEEP);
	object_init(&device->obj, NULL, NULL, NULL);
	mutex_init(&device->lock, "device_alias_lock", 0);
	refcount_set(&device->count, 0);
	radix_tree_init(&device->children);
	list_init(&device->dest_link);
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

	/* Add the device to the destination's alias list. */
	mutex_lock(&dest->lock);
	list_append(&dest->aliases, &device->dest_link);
	mutex_unlock(&dest->lock);

	dprintf("device: created alias %p(%s) under %p(%s) (dest: %p)\n",
	        device, device->name, parent, parent->name, dest);
	if(devicep) {
		*devicep = device;
	}
	return STATUS_SUCCESS;
}

/** Remove a device from the device tree.
 *
 * Removes a device from the device tree. The device must have no users. All
 * aliases of the device should be destroyed before the device itself.
 *
 * @todo		Sometime we'll need to allow devices to be removed when
 *			they have users, for example for hotplugging.
 * @fixme		I don't think alias removal is entirely thread-safe.
 *
 * @param device	Device to remove.
 *
 * @return		Status code describing result of the operation. Cannot
 *			fail if the device being removed is an alias.
 */
status_t device_destroy(device_t *device) {
	device_t *alias;
	size_t i;

	assert(device->parent);

	mutex_lock(&device->parent->lock);
	mutex_lock(&device->lock);

	if(refcount_get(&device->count) != 0) {
		mutex_unlock(&device->lock);
		mutex_unlock(&device->parent->lock);
		return STATUS_IN_USE;
	}

	/* Call the device's destroy operation, if any. */
	if(device->ops && device->ops->destroy) {
		device->ops->destroy(device);
	}

	/* Remove all aliases to the device. */
	if(!device->dest) {
		LIST_FOREACH(&device->aliases, iter) {
			alias = list_entry(iter, device_t, dest_link);
			device_destroy(alias);
		}
	}

	radix_tree_remove(&device->parent->children, device->name, NULL);
	refcount_dec(&device->parent->count);
	mutex_unlock(&device->parent->lock);

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
	return STATUS_SUCCESS;
}

/** Internal iteration function.
 * @param device	Device we're currently on.
 * @param func		Function to call on devices.
 * @param data		Data argument to pass to function.
 * @return		Whether to continue lookup. */
static bool device_iterate_internal(device_t *device, device_iterate_t func, void *data) {
	device_t *child;

	switch(func(device, data)) {
	case 0: return false;
	case 2: return true;
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

/** Look up a device and increase its reference count.
 * @param path		Path to device.
 * @return		Pointer to device if found, NULL if not. */
static device_t *device_lookup(const char *path) {
	device_t *device = device_tree_root, *child;
	char *dup, *orig, *tok;

	assert(path);

	if(!path[0] || path[0] != '/') {
		return NULL;
	}

	dup = orig = kstrdup(path, MM_SLEEP);

	mutex_lock(&device->lock);
	while((tok = strsep(&dup, "/"))) {
		if(!tok[0]) {
			continue;
		}

		child = radix_tree_lookup(&device->children, tok);
		if(!child) {
			mutex_unlock(&device->lock);
			kfree(orig);
			return NULL;
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
	return device;
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

/** Get the path to a device.
 * @param device	Device to get path to.
 * @return		Pointer to kmalloc()'d string containing device path. */
char *device_path(device_t *device) {
	char *path = NULL, *tmp;
	device_t *parent;
	size_t len = 0;

	while(device != device_tree_root) {
		mutex_lock(&device->lock);
		len += strlen(device->name) + 1;
		tmp = kmalloc(len + 1, MM_SLEEP);
		strcpy(tmp, "/");
		strcat(tmp, device->name);
		if(path) {
			strcat(tmp, path);
			kfree(path);
		}
		path = tmp;
		parent = device->parent;
		mutex_unlock(&device->lock);
		device = parent;
	}

	if(!len) {
		path = kstrdup("/", MM_SLEEP);
	}
	return path;
}


/** Get a handle to a device.
 * @param device	Device to get handle to.
 * @param rights	Requested rights for the handle.
 * @param handlep	Where to store handle to device.
 * @return		Status code describing result of the operation. */
status_t device_get(device_t *device, object_rights_t rights, object_handle_t **handlep) {
	void *data = NULL;
	status_t ret;

	assert(device);
	assert(handlep);

	mutex_lock(&device->lock);
	refcount_inc(&device->count);

	if(device->ops && device->ops->open) {
		ret = device->ops->open(device, &data);
		if(ret != STATUS_SUCCESS) {
			refcount_dec(&device->count);
			mutex_unlock(&device->lock);
			return ret;
		}
	}

	ret = object_handle_open(&device->obj, data, rights, NULL, 0, handlep, NULL, NULL);
	if(ret != STATUS_SUCCESS) {
		refcount_dec(&device->count);
	}

	mutex_unlock(&device->lock);
	return ret;
}

/** Create a handle to a device.
 * @param path		Path to device to open.
 * @param rights	Requested rights for the handle.
 * @param handlep	Where to store pointer to handle structure.
 * @return		Status code describing result of the operation. */
status_t device_open(const char *path, object_rights_t rights, object_handle_t **handlep) {
	void *data = NULL;
	device_t *device;
	status_t ret;

	assert(path);
	assert(handlep);

	device = device_lookup(path);
	if(device == NULL) {
		return STATUS_NOT_FOUND;
	}

	mutex_lock(&device->lock);

	if(device->ops && device->ops->open) {
		ret = device->ops->open(device, &data);
		if(ret != STATUS_SUCCESS) {
			refcount_dec(&device->count);
			mutex_unlock(&device->lock);
			return ret;
		}
	}

	ret = object_handle_open(&device->obj, data, rights, NULL, 0, handlep, NULL, NULL);
	if(ret != STATUS_SUCCESS) {
		refcount_dec(&device->count);
	}

	mutex_unlock(&device->lock);
	return ret;
}

/** Read from a device.
 *
 * Reads data from a device into a buffer. The device may not support the
 * operation - it is provided as a function rather than a request type because
 * it is supported by multiple device types.
 *
 * @param handle	Handle to device to read from. Must have the
 *			DEVICE_RIGHT_READ access right.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param offset	Offset in the device to read from (only valid for
 *			certain device types).
 * @param bytesp	Where to store number of bytes read.
 *
 * @return		Status code describing result of the operation.
 */
status_t device_read(object_handle_t *handle, void *buf, size_t count, offset_t offset, size_t *bytesp) {
	device_t *device;
	status_t ret;
	size_t bytes;

	if(!handle || !buf) {
		return STATUS_INVALID_ARG;
	} else if(handle->object->type->id != OBJECT_TYPE_DEVICE) {
		return STATUS_INVALID_HANDLE;
	} else if(!object_handle_rights(handle, DEVICE_RIGHT_READ)) {
		return STATUS_ACCESS_DENIED;
	}

	device = (device_t *)handle->object;
	if(!device->ops || !device->ops->read) {
		return STATUS_NOT_SUPPORTED;
	} else if(!count) {
		if(bytesp) {
			*bytesp = 0;
		}
		return STATUS_SUCCESS;
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
 * @param handle	Handle to device to write to. Must have the
 *			DEVICE_RIGHT_WRITE right.
 * @param buf		Buffer containing data to write.
 * @param count		Number of bytes to write.
 * @param offset	Offset in the device to write to (only valid for
 *			certain device types).
 * @param bytesp	Where to store number of bytes read.
 *
 * @return		Status code describing result of the operation.
 */
status_t device_write(object_handle_t *handle, const void *buf, size_t count, offset_t offset,
                      size_t *bytesp) {
	device_t *device;
	status_t ret;
	size_t bytes;

	if(!handle || !buf) {
		return STATUS_INVALID_ARG;
	} else if(handle->object->type->id != OBJECT_TYPE_DEVICE) {
		return STATUS_INVALID_HANDLE;
	} else if(!object_handle_rights(handle, DEVICE_RIGHT_WRITE)) {
		return STATUS_ACCESS_DENIED;
	}

	device = (device_t *)handle->object;
	if(!device->ops || !device->ops->write) {
		return STATUS_NOT_SUPPORTED;
	} else if(!count) {
		if(bytesp) {
			*bytesp = 0;
		}
		return STATUS_SUCCESS;
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
 * @return		Status code describing result of the operation. */
status_t device_request(object_handle_t *handle, int request, const void *in, size_t insz,
                        void **outp, size_t *outszp) {
	device_t *device;

	if(!handle) {
		return STATUS_INVALID_ARG;
	} else if(handle->object->type->id != OBJECT_TYPE_DEVICE) {
		return STATUS_INVALID_HANDLE;
	}

	device = (device_t *)handle->object;
	if(!device->ops || !device->ops->request) {
		return STATUS_INVALID_REQUEST;
	}

	// FIXME: Right checks?
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
void __init_text device_init(void) {
	status_t ret;

	/* Create the root node of the device tree. */
	device_tree_root = kcalloc(1, sizeof(device_t), MM_FATAL);
	mutex_init(&device_tree_root->lock, "device_root_lock", 0);
	refcount_set(&device_tree_root->count, 0);
	radix_tree_init(&device_tree_root->children);
	device_tree_root->name = (char *)"<root>";

	/* Create standard device directories. */
	ret = device_create("bus", device_tree_root, NULL, NULL, NULL, 0, &device_bus_dir);
	if(ret != STATUS_SUCCESS) {
		fatal("Could not create bus directory in device tree (%d)", ret);
	}
}

/** Open a handle to a device.
 *
 * Opens a handle to a device that can be used to perform other operations on
 * it. Once the device is no longer required, the handle should be closed with
 * kern_handle_close().
 *
 * @param path		Device tree path for device to open.
 * @param rights	Requested rights for the handle.
 * @param handlep	Where to store handle to the device.
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_device_open(const char *path, object_rights_t rights, handle_t *handlep) {
	object_handle_t *handle;
	status_t ret;
	char *kpath;

	if(!handlep) {
		return STATUS_INVALID_ARG;
	}

	ret = strndup_from_user(path, DEVICE_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	ret = device_open(kpath, rights, &handle);
	if(ret != STATUS_SUCCESS) {
		kfree(kpath);
		return ret;
	}

	ret = object_handle_attach(handle, NULL, 0, NULL, handlep);
	object_handle_release(handle);
	kfree(kpath);
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
 * @param bytesp	Where to store number of bytes read (can be NULL).
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_device_read(handle_t handle, void *buf, size_t count, offset_t offset,
                          size_t *bytesp) {
	object_handle_t *khandle = NULL;
	status_t ret, err;
	size_t bytes = 0;
	void *kbuf;

	ret = object_handle_lookup(NULL, handle, OBJECT_TYPE_DEVICE, 0, &khandle);
	if(ret != STATUS_SUCCESS) {
		goto out;
	}

	/* Don't do anything if there are no bytes to read. */
	if(!count) {
		goto out;
	}

	/* Allocate a temporary buffer to read into. Don't use MM_SLEEP for
	 * this allocation because the process may provide a count larger than
	 * we can allocate in kernel space, in which case it would block
	 * forever. */
	kbuf = kmalloc(count, 0);
	if(!kbuf) {
		ret = STATUS_NO_MEMORY;
		goto out;
	}

	ret = device_read(khandle, kbuf, count, offset, &bytes);
	if(bytes) {
		err = memcpy_to_user(buf, kbuf, bytes);	
		if(err != STATUS_SUCCESS) {
			ret = err;
			bytes = 0;
		}
	}
	kfree(kbuf);
out:
	if(khandle) {
		object_handle_release(khandle);
	}
	if(bytesp) {
		err = memcpy_to_user(bytesp, &bytes, sizeof(size_t));
		if(err != STATUS_SUCCESS) {
			ret = err;
		}
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
 * @param bytesp	Where to store number of bytes read (can be NULL).
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_device_write(handle_t handle, const void *buf, size_t count, offset_t offset,
                           size_t *bytesp) {
	object_handle_t *khandle = NULL;
	status_t ret, err;
	void *kbuf = NULL;
	size_t bytes = 0;

	ret = object_handle_lookup(NULL, handle, OBJECT_TYPE_DEVICE, 0, &khandle);
	if(ret != STATUS_SUCCESS) {
		goto out;
	}

	/* Don't do anything if there are no bytes to write. */
	if(!count) {
		goto out;
	}

	/* Copy the data to write across from userspace. Don't use MM_SLEEP for
	 * this allocation because the process may provide a count larger than
	 * we can allocate in kernel space, in which case it would block
	 * forever. */
	kbuf = kmalloc(count, 0);
	if(!kbuf) {
		ret = STATUS_NO_MEMORY;
		goto out;
	}
	ret = memcpy_from_user(kbuf, buf, count);
	if(ret != STATUS_SUCCESS) {
		goto out;
	}

	ret = device_write(khandle, kbuf, count, offset, &bytes);
out:
	if(kbuf) {
		kfree(kbuf);
	}
	if(khandle) {
		object_handle_release(khandle);
	}
	if(bytesp) {
		err = memcpy_to_user(bytesp, &bytes, sizeof(size_t));
		if(err != STATUS_SUCCESS) {
			ret = err;
		}
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
 * @return		Status code describing result of the operation. */
status_t kern_device_request(handle_t handle, int request, const void *in, size_t insz, void *out,
                             size_t outsz, size_t *bytesp) {
	void *kin = NULL, *kout = NULL;
	object_handle_t *khandle;
	status_t ret, err;
	size_t koutsz;

	ret = object_handle_lookup(NULL, handle, OBJECT_TYPE_DEVICE, 0, &khandle);
	if(ret != STATUS_SUCCESS) {
		goto out;
	}

	if(in && insz) {
		kin = kmalloc(insz, 0);
		if(!kin) {
			ret = STATUS_NO_MEMORY;
			goto out;
		}
		ret = memcpy_from_user(kin, in, insz);
		if(ret != STATUS_SUCCESS) {
			goto out;
		}
	}

	ret = device_request(khandle, request, kin, insz, (out) ? &kout : NULL, (out) ? &koutsz : NULL);
	if(kout) {
		assert(koutsz);
		if(koutsz > outsz) {
			ret = STATUS_TOO_SMALL;
			goto out;
		}

		err = memcpy_to_user(out, kout, koutsz);
		if(err != STATUS_SUCCESS) {
			ret = err;
			goto out;
		}

		if(bytesp) {
			err = memcpy_to_user(bytesp, &koutsz, sizeof(size_t));
			if(err != STATUS_SUCCESS) {
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
	object_handle_release(khandle);
	return ret;
}
