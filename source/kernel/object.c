/*
 * Copyright (C) 2010-2013 Alex Smith
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
 * @brief		Kernel object manager.
 *
 * The kernel object manager manages all userspace-accessible objects. It
 * allows processes (as well as the kernel) to create handles to objects, and
 * implements a discretionary access control system that limits which processes
 * can access objects.
 *
 * It does not, however, manage how objects are referred to (i.e. there isn't
 * a single namespace for all objects - for example FS entries are referred to
 * by path strings, but ports, memory areas, etc. are referred to by global
 * IDs), or the lifetime of objects - it is up to each object type to manage
 * these.
 *
 * @todo		Make handle tables resizable, based on process limits
 *			or something (e.g. rlimit).
 */

#include <lib/atomic.h>
#include <lib/bitmap.h>

#include <mm/malloc.h>
#include <mm/safe.h>
#include <mm/slab.h>

#include <proc/process.h>
#include <proc/thread.h>

#include <assert.h>
#include <kdb.h>
#include <kernel.h>
#include <object.h>
#include <status.h>

#if CONFIG_OBJECT_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Maximum number of handles. */
#define HANDLE_TABLE_SIZE	512

/** Object wait synchronization information. */
typedef struct object_wait_sync {
	spinlock_t lock;		/**< Lock for the structure. */
	thread_t *thread;		/**< Thread which is waiting. */
	size_t count;			/**< Number of remaining events to be signalled. */
} object_wait_sync_t;

/** Object waiting internal data structure. */
typedef struct object_wait {
	object_event_t info;		/**< Userspace-supplied event information. */
	object_handle_t *handle;	/**< Handle being waited on. */
	object_wait_sync_t *sync;	/**< Synchronization object for the wait. */
} object_wait_t;

/** Cache for handle structures. */
static slab_cache_t *object_handle_cache;

/** Cache for handle table structures. */
static slab_cache_t *handle_table_cache;

/** Constructor for handle table objects.
 * @param obj		Object to construct.
 * @param data		Cache data pointer. */
static void handle_table_ctor(void *obj, void *data) {
	handle_table_t *table = obj;

	rwlock_init(&table->lock, "handle_table_lock");
}

/** Initialize an object structure.
 * @param obj		Object to initialize.
 * @param type		Pointer to type structure for object type. Can be NULL,
 *			in which case the object will be a 'NULL object', and
 *			handles will never be created to it. */
void object_init(object_t *object, object_type_t *type) {
	assert(object);

	object->type = type;
}

/** Destroy an object structure.
 * @param obj		Object to destroy. */
void object_destroy(object_t *object) {
	/* Nothing happens (yet). */
}

/** Calculate the set of rights that a process has for an object.
 * @param object	Object to check.
 * @param process	Process to check for.
 * @return		Set of rights that the process has for the object. */
object_rights_t object_rights(object_t *object, struct process *process) {
	// TODO
	return 0xFFFFFFFF;
}

/** Notifier function to use for object waiting.
 * @param arg1		Unused.
 * @param arg2		Unused.
 * @param arg3		Wait structure pointer. */
void object_wait_notifier(void *arg1, void *arg2, void *arg3) {
	// FIXME: How do we pass data here? Use one of the other args?
	object_wait_signal(arg3, 0);
}

/** Signal that an event being waited for has occurred.
 * @param wait		Internal data pointer.
 * @param data		Event data to return to waiter. */
void object_wait_signal(void *_wait, unsigned long data) {
	object_wait_t *wait = _wait;

	wait->info.signalled = true;
	wait->info.data = data;

	spinlock_lock(&wait->sync->lock);

	/* Don't decrement the count if its already 0, only wake if thread is
	 * actually sleeping. */
	if(wait->sync->count && --wait->sync->count == 0 && wait->sync->thread) {
		thread_wake(wait->sync->thread);
		wait->sync->thread = NULL;
	}

	spinlock_unlock(&wait->sync->lock);
}

/**
 * Create a handle to an object.
 *
 * Creates a new handle to a kernel object. This function does _NOT_ perform a
 * rights check on the object, if this should be done use object_handle_open()
 * instead. The created handle will have a single reference on it. The handle
 * must be closed with object_handle_release() when it is no longer required.
 *
 * @param object	Object to create a handle to.
 * @param data		Per-handle data pointer.
 * @param rights	Access rights for the handle.
 *
 * @return		Handle to the object.
 */
object_handle_t *object_handle_create(object_t *object, void *data, object_rights_t rights) {
	object_handle_t *handle;

	handle = slab_cache_alloc(object_handle_cache, MM_WAIT);
	refcount_set(&handle->count, 1);
	handle->object = object;
	handle->data = data;
	handle->rights = rights;
	return handle;
}

/**
 * Open a handle to an object.
 *
 * Opens a new handle to a kernel object. This function checks whether the
 * current process has the necessary rights to access the object, and fail if
 * it does not. The created handle will have a single reference on it. The
 * handle must be closed with object_handle_release() when it is no longer
 * required.
 *
 * @param object	Object to create a handle to.
 * @param data		Per-handle data pointer.
 * @param rights	Access rights for the handle.
 * @param handlep	Where to store handle to object.
 *
 * @return		STATUS_SUCCESS if successful.
 *			STATUS_ACCESS_DENIED if the current process does not
 *			have the requested access rights for the object.
 */
status_t object_handle_open(object_t *object, void *data, object_rights_t rights,
	object_handle_t **handlep)
{
	if(rights && (object_rights(object, curr_proc) & rights) != rights)
		return STATUS_ACCESS_DENIED;

	*handlep = object_handle_create(object, data, rights);
	return STATUS_SUCCESS;
}

/**
 * Increase the reference count of a handle.
 *
 * Increases the reference count of a handle, ensuring that it will not be
 * freed. When the handle is no longer required, you must call
 * object_handle_release() on it.
 * 
 * @param handle	Handle to increase reference count of.
 */
void object_handle_retain(object_handle_t *handle) {
	assert(handle);
	refcount_inc(&handle->count);
}

/**
 * Release a handle.
 *
 * Decreases the reference count of a handle. If no more references remain to
 * the handle, it will be closed.
 *
 * @param handle	Handle to release.
 */
void object_handle_release(object_handle_t *handle) {
	assert(handle);

	/* If there are no more references we can close it. */
	if(refcount_dec(&handle->count) == 0) {
		if(handle->object->type->close)
			handle->object->type->close(handle);

		slab_cache_free(object_handle_cache, handle);
	}
}

/**
 * Look up a handle in a the current process' handle table.
 *
 * Looks up the handle with the given ID in the current process' handle table,
 * optionally checking that the object it refers to is a certain type and that
 * the handle has certain rights. The returned handle will have an extra
 * reference on it: when it is no longer needed, it should be released with
 * object_handle_release().
 *
 * @param id		Handle ID to look up.
 * @param type		Required object type ID (if negative, no type checking
 *			will be performed).
 * @param rights	If not 0, the handle will be checked for these rights
 *			and an error will be returned if it does not have them.
 * @param handlep	Where to store pointer to handle structure.
 *
 * @return		Status code describing result of the operation.
 */
status_t object_handle_lookup(handle_t id, int type, object_rights_t rights,
	object_handle_t **handlep)
{
	object_handle_t *handle;

	assert(handlep);

	if(id < 0 || id >= HANDLE_TABLE_SIZE)
		return STATUS_INVALID_HANDLE;

	rwlock_read_lock(&curr_proc->handles->lock);

	handle = curr_proc->handles->handles[id];
	if(!handle) {
		rwlock_unlock(&curr_proc->handles->lock);
		return STATUS_INVALID_HANDLE;
	}

	/* Check if the type is the type the caller wants. */
	if(type >= 0 && handle->object->type->id != type) {
		rwlock_unlock(&curr_proc->handles->lock);
		return STATUS_INVALID_HANDLE;
	}

	/* Check if the handle has the requested rights. */
	if(rights && !object_handle_rights(handle, rights)) {
		rwlock_unlock(&curr_proc->handles->lock);
		return STATUS_ACCESS_DENIED;
	}

	object_handle_retain(handle);
	rwlock_unlock(&curr_proc->handles->lock);

	*handlep = handle;
	return STATUS_SUCCESS;
}

/**
 * Insert a handle into the current process' handle table.
 *
 * Allocates a handle ID for the current process and adds a handle to its
 * handle table. On success, the handle will have an extra reference on it.
 *
 * @param handle	Handle to attach.
 * @param idp		If not NULL, a kernel location to store handle ID in.
 * @param uidp		If not NULL, a user location to store handle ID in.
 *
 * @return		Status code describing result of the operation.
 */
status_t object_handle_attach(object_handle_t *handle, handle_t *idp, handle_t *uidp) {
	status_t ret;
	handle_t id;

	assert(handle);
	assert(idp || uidp);

	rwlock_write_lock(&curr_proc->handles->lock);

	/* Find a handle ID in the table. */
	id = bitmap_ffz(curr_proc->handles->bitmap, HANDLE_TABLE_SIZE);
	if(id < 0) {
		rwlock_unlock(&curr_proc->handles->lock);
		return STATUS_NO_HANDLES;
	}

	if(idp)
		*idp = id;

	if(uidp) {
		ret = memcpy_to_user(uidp, &id, sizeof(*uidp));
		if(ret != STATUS_SUCCESS) {
			rwlock_unlock(&curr_proc->handles->lock);
			return ret;
		}
	}

	object_handle_retain(handle);
	curr_proc->handles->handles[id] = handle;
	curr_proc->handles->flags[id] = 0;
	bitmap_set(curr_proc->handles->bitmap, id);

	rwlock_unlock(&curr_proc->handles->lock);

	dprintf("object: allocated handle %" PRId32 " in process %" PRId32 " ("
		"object: %p, data: %p)\n", id, curr_proc->id, handle->object,
		handle->data);
	return STATUS_SUCCESS;
}

/** Detach a handle from the current process' handle table.
 * @param id		ID of handle to detach.
 * @return		Status code describing result of the operation. */
static status_t object_handle_detach_unsafe(handle_t id) {
	if(id < 0 || id >= HANDLE_TABLE_SIZE || !curr_proc->handles->handles[id])
		return STATUS_INVALID_HANDLE;

	object_handle_release(curr_proc->handles->handles[id]);
	curr_proc->handles->handles[id] = NULL;
	curr_proc->handles->flags[id] = 0;

	dprintf("object: detached handle %" PRId32 " from process %" PRId32 "\n",
		id, curr_proc->id);
	return STATUS_SUCCESS;
}

/**
 * Detach a handle from the current process.
 *
 * Removes the specified handle ID from the current process' handle table and
 * releases the handle.
 *
 * @param id		ID of handle to detach.
 *
 * @return		Status code describing result of the operation.
 */
status_t object_handle_detach(handle_t id) {
	status_t ret;

	rwlock_write_lock(&curr_proc->handles->lock);
	ret = object_handle_detach_unsafe(id);
	rwlock_unlock(&curr_proc->handles->lock);
	return ret;
}

/**
 * Create a new handle table.
 *
 * Creates a new handle table and duplicates handles from another handle table
 * into it, either using provided mapping information, or by looking at the
 * inheritable flag of handles.
 *
 * @param parent	Parent process' handle table (can be NULL, in which
 *			case no handles will be duplicated).
 * @param map		An array specifying handles to add to the new table.
 *			The first ID of each entry specifies the handle in the
 *			parent, and the second specifies the ID to give it in
 *			the new table. Can be NULL if count <= 0.
 * @param count		The number of handles in the array. If negative, the
 *			map will be ignored and all handles with the inheritable
 *			flag set will be duplicated. If 0, no handles will be
 *			duplicated.
 * @param tablep	Where to store pointer to table structure.
 *
 * @return		Status code describing result of the operation. Failure
 *			can only occur when handle mappings are specified.
 */
status_t handle_table_create(handle_table_t *parent, handle_t map[][2], int count,
	handle_table_t **tablep)
{
	handle_table_t *table;
	object_handle_t *handle;
	status_t ret;
	int i;

	table = slab_cache_alloc(handle_table_cache, MM_KERNEL);
	table->handles = kcalloc(HANDLE_TABLE_SIZE, sizeof(table->handles[0]), MM_KERNEL);
	table->flags = kcalloc(HANDLE_TABLE_SIZE, sizeof(table->flags[0]), MM_KERNEL);
	table->bitmap = bitmap_alloc(HANDLE_TABLE_SIZE, MM_KERNEL);

	*tablep = table;

	if(!parent || count == 0)
		return STATUS_SUCCESS;

	rwlock_read_lock(&parent->lock);

	/* Inherit all inheritable handles in the parent table. */
	if(count > 0) {
		assert(map);

		for(i = 0; i < count; i++) {
			if(map[i][0] < 0 || map[i][0] >= HANDLE_TABLE_SIZE
				|| map[i][1] < 0 || map[i][1] >= HANDLE_TABLE_SIZE
				|| !parent->handles[map[i][0]])
			{
				ret = STATUS_INVALID_HANDLE;
				goto fail;
			}

			if(table->handles[map[i][1]]) {
				ret = STATUS_ALREADY_EXISTS;
				goto fail;
			}

			handle = parent->handles[map[i][0]];

			/* When using a map, the inheritable flag is ignored so
			 * we must check whether transferring handles is
			 * allowed. */
			if(!(handle->object->type->flags & OBJECT_TRANSFERRABLE)) {
				ret = STATUS_NOT_SUPPORTED;
				goto fail;
			}

			object_handle_retain(handle);
			table->handles[map[i][1]] = handle;
			table->flags[map[i][1]] = parent->flags[map[i][0]];
			bitmap_set(table->bitmap, map[i][1]);
		}
	} else {
		for(i = 0; i < HANDLE_TABLE_SIZE; i++) {
			/* Flag can only be set if handle is not NULL and the
			 * type allows transferring. */
			if(parent->flags[i] & HANDLE_INHERITABLE) {
				object_handle_retain(parent->handles[i]);
				table->handles[i] = parent->handles[i];
				table->flags[i] = parent->flags[i];
				bitmap_set(table->bitmap, i);
			}
		}
	}

	rwlock_unlock(&parent->lock);
	return STATUS_SUCCESS;
fail:
	rwlock_unlock(&parent->lock);
	handle_table_destroy(table);
	return ret;
}

/**
 * Clone a handle table.
 *
 * Creates a clone of a handle table. All handles, even non-inheritable ones,
 * will be copied into the new table. The table entries will all refer to the
 * same underlying handle as the old table.
 *
 * @param src		Source table.
 *
 * @return		Pointer to cloned table.
 */
handle_table_t *handle_table_clone(handle_table_t *src) {
	handle_table_t *table;
	size_t i;

	table = slab_cache_alloc(handle_table_cache, MM_KERNEL);
	table->handles = kcalloc(HANDLE_TABLE_SIZE, sizeof(table->handles[0]), MM_KERNEL);
	table->flags = kcalloc(HANDLE_TABLE_SIZE, sizeof(table->flags[0]), MM_KERNEL);
	table->bitmap = bitmap_alloc(HANDLE_TABLE_SIZE, MM_KERNEL);

	rwlock_read_lock(&src->lock);

	for(i = 0; i < HANDLE_TABLE_SIZE; i++) {
		/* Flag can only be set if handle is not NULL and the type
		 * allows transferring. */
		if(src->flags[i] & HANDLE_INHERITABLE) {
			object_handle_retain(src->handles[i]);
			table->handles[i] = src->handles[i];
			table->flags[i] = src->flags[i];
			bitmap_set(table->bitmap, i);
		}
	}

	rwlock_unlock(&src->lock);
	return table;
}

/** Destroy a handle table.
 * @param table		Table being destroyed. */
void handle_table_destroy(handle_table_t *table) {
	size_t i;

	for(i = 0; i < HANDLE_TABLE_SIZE; i++) {
		if(table->handles[i])
			object_handle_release(table->handles[i]);
	}

	kfree(table->bitmap);
	kfree(table->flags);
	kfree(table->handles);
	slab_cache_free(handle_table_cache, table);
}

/** Print a list of a process' handles.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_handles(int argc, char **argv, kdb_filter_t *filter) {
	object_handle_t *handle;
	uint64_t id;
	process_t *process;
	size_t i;

	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s <process ID>\n\n", argv[0]);

		kdb_printf("Prints out a list of all currently open handles in a process.\n");
		return KDB_SUCCESS;
	} else if(argc != 2) {
		kdb_printf("Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
		return KDB_FAILURE;
	}

	if(kdb_parse_expression(argv[1], &id, NULL) != KDB_SUCCESS)
		return KDB_FAILURE;

	process = process_lookup_unsafe(id);
	if(!process) {
		kdb_printf("Invalid process ID.\n");
		return KDB_FAILURE;
	}

	kdb_printf("ID   Flags  Object             Type                    Count Data\n");
	kdb_printf("==   =====  ======             ====                    ===== ====\n");

	for(i = 0; i < HANDLE_TABLE_SIZE; i++) {
		handle = process->handles->handles[i];

		kdb_printf("%-4zu 0x%-4" PRIx32 " %-18p %-2u (%-18p) %-5" PRId32 " %p\n",
			i, process->handles->flags[i], handle->object,
			handle->object->type->id, handle->object->type,
			refcount_get(&handle->count), handle->data);
	}

	return KDB_SUCCESS;
}

/** Initialize the handle caches. */
__init_text void handle_init(void) {
	object_handle_cache = slab_cache_create("object_handle_cache",
		sizeof(object_handle_t), 0, NULL, NULL, NULL, 0,
		MM_BOOT);
	handle_table_cache = slab_cache_create("handle_table_cache",
		sizeof(handle_table_t), 0, handle_table_ctor, NULL, NULL, 0,
		MM_BOOT);

	/* Register the KDB commands. */
	kdb_register_command("handles", "Inspect a process' handle table.",
		kdb_cmd_handles);
}

/** Get the type of an object referred to by a handle.
 * @param handle	Handle to object.
 * @return		Type ID of object on success, -1 if the handle was not
 *			found. */
int kern_object_type(handle_t handle) {
	object_handle_t *khandle;
	int ret;

	if(object_handle_lookup(handle, -1, 0, &khandle) != STATUS_SUCCESS)
		return -1;

	ret = khandle->object->type->id;
	object_handle_release(khandle);
	return ret;
}

/**
 * Wait for events to occur on one or more objects.
 *
 * Waits until one or all of the specified events occurs on one or more kernel
 * objects, or until the timeout period expires. Note that this function is
 * better suited for waiting on small numbers of objects. For frequent waits
 * on a large number of objects, using the watcher API will yield better
 * performance.
 *
 * @param events	Array of structures describing events to wait for. Upon
 *			successful return, the signalled field of each
 *			structure will be updated to reflect whether or not the
 *			event was signalled, and if set to true then the data
 *			field will be updated with the data value associated
 *			with the event.
 * @param count		Number of array entries.
 * @param flags		Behaviour flags for waiting. If the OBJECT_WAIT_ALL
 *			flag is specified, then the function will wait until
 *			all of the given events occur, rather than just one of
 *			them.
 * @param timeout	Maximum time to wait in nanoseconds. A value of 0 will
 *			cause the function to return immediately if the none of
 *			the events have already occurred, and a value of -1 will
 *			block indefinitely until one of events happens.
 *
 * @return		STATUS_SUCCESS if successful.
 *			STATUS_INVALID_ARG if count is 0 or too big, or if
 *			events is NULL.
 *			STATUS_INVALID_HANDLE if a handle does not exist.
 *			STATUS_INVALID_EVENT if an incorrect event ID is used.
 *			STATUS_TIMED_OUT if the timeout expires.
 *			STATUS_INTERRUPTED if the sleep was interrupted.
 */
status_t kern_object_wait(object_event_t *events, size_t count, uint32_t flags,
	nstime_t timeout)
{
	object_wait_sync_t sync;
	object_wait_t *waits;
	object_handle_t *handle;
	status_t ret, err;
	size_t i;

	/* TODO: Is this a sensible limit to impose? Do we even need one? We
	 * allocate via MM_USER so it won't cause problems if it's too large
	 * to allocate. */
	if(!count || count > 1024 || !events)
		return STATUS_INVALID_ARG;

	/* Initialize the synchronization information. Thread is set to NULL
	 * initially so that object_wait_signal() does not try to wake us if
	 * an event is signalled while setting up the waits. */
	spinlock_init(&sync.lock, "object_wait_lock");
	sync.thread = NULL;
	sync.count = (flags & OBJECT_WAIT_ALL) ? count : 1;

	waits = kmalloc(count * sizeof(*waits), MM_USER);
	if(!waits)
		return STATUS_NO_MEMORY;

	for(i = 0; i < count; i++) {
		ret = memcpy_from_user(&waits[i].info, &events[i], sizeof(*events));
		if(ret != STATUS_SUCCESS)
			goto out;

		ret = object_handle_lookup(waits[i].info.handle, -1, 0, &handle);
		if(ret != STATUS_SUCCESS) {
			goto out;
		} else if(!handle->object->type->wait || !handle->object->type->unwait) {
			ret = STATUS_INVALID_EVENT;
			object_handle_release(handle);
			goto out;
		}

		waits[i].sync = &sync;
		waits[i].handle = handle;
		waits[i].info.signalled = false;

		ret = handle->object->type->wait(handle, waits[i].info.event, &waits[i]);
		if(ret != STATUS_SUCCESS) {
			object_handle_release(handle);
			goto out;
		}
	}

	spinlock_lock(&sync.lock);

	/* Now we wait for the events. If all the events required have already
	 * been signalled, don't sleep. */
	if(sync.count == 0) {
		ret = STATUS_SUCCESS;
		spinlock_unlock(&sync.lock);
	} else {
		ret = thread_sleep(&sync.lock, timeout, "object_wait",
			SLEEP_INTERRUPTIBLE);
	}
out:
	/* Cancel all waits which have been set up. */
	while(i--) {
		handle = waits[i].handle;
		handle->object->type->unwait(handle, waits[i].info.event, &waits[i]);
		object_handle_release(handle);

		if(ret == STATUS_SUCCESS) {
			err = memcpy_to_user(&events[i], &waits[i].info, sizeof(*events));
			if(err != STATUS_SUCCESS)
				ret = err;
		}
	}

	kfree(waits);
	return ret;
}

/** Get the flags set on a handle table entry.
 * @see			kern_handle_set_flags().
 * @param handle	Handle to get flags for.
 * @param flagsp	Where to store handle table entry flags.
 * @return		Status code describing result of the operation. */
status_t kern_handle_flags(handle_t handle, uint32_t *flagsp) {
	status_t ret;

	if(handle < 0 || handle >= HANDLE_TABLE_SIZE)
		return STATUS_INVALID_HANDLE;

	rwlock_read_lock(&curr_proc->handles->lock);

	if(!curr_proc->handles->handles[handle]) {
		rwlock_unlock(&curr_proc->handles->lock);
		return STATUS_INVALID_HANDLE;
	}

	ret = memcpy_to_user(flagsp, &curr_proc->handles->flags[handle], sizeof(*flagsp));
	rwlock_unlock(&curr_proc->handles->lock);
	return ret;
}

/**
 * Set the flags set on a handle table entry.
 *
 * Sets the flags set on a handle table entry. Note that these flags affect the
 * handle table entry, not the actual open handle. Multiple handle table entries
 * across multiple processes can refer to the same handle, for example handles
 * inherited by new processes refer to the same underlying handle. Any flags
 * that can be set on the underlying handle are manipulated using an object
 * type-specific API.
 *
 * Only one flag is currently defined: HANDLE_INHERITABLE. This determines
 * whether the handle will be duplicated when creating a new process.
 *
 * @param handle	Handle to get flags for.
 * @param flags		New flags to set.
 *
 * @return		STATUS_SUCCESS if successful.
 *			STATUS_INVALID_HANDLE if the handle does not exist.
 *			STATUS_NOT_SUPPORTED if attempting to set
 *			HANDLE_INHERITABLE on a handle to a non-transferrable
 *			object.
 */
status_t kern_handle_set_flags(handle_t handle, uint32_t flags) {
	object_handle_t *khandle;

	if(handle < 0 || handle >= HANDLE_TABLE_SIZE)
		return STATUS_INVALID_HANDLE;

	/* Don't need to write lock just to set flags. */
	rwlock_read_lock(&curr_proc->handles->lock);

	khandle = curr_proc->handles->handles[handle];
	if(!khandle) {
		rwlock_unlock(&curr_proc->handles->lock);
		return STATUS_INVALID_HANDLE;
	}

	/* To set the inheritable flag, the object type must be transferrable. */
	if(flags & HANDLE_INHERITABLE) {
		if(!(khandle->object->type->flags & OBJECT_TRANSFERRABLE)) {
			rwlock_unlock(&curr_proc->handles->lock);
			return STATUS_NOT_SUPPORTED;
		}
	}

	curr_proc->handles->flags[handle] = flags;
	rwlock_unlock(&curr_proc->handles->lock);
	return STATUS_SUCCESS;
}

/** Get the access rights for a handle.
 * @param handle	Handle to get rights for.
 * @param rightsp	Where to store handle access rights.
 * @return		STATUS_SUCCESS if successful.
 *			STATUS_INVALID_HANDLE if handle does not exist.
 *			STATUS_NOT_SUPPORTED if the handle refers to an object
 *			of a non-securable type. */
status_t kern_handle_rights(handle_t handle, object_rights_t *rightsp) {
	object_handle_t *khandle;
	status_t ret;

	if(handle < 0 || handle >= HANDLE_TABLE_SIZE)
		return STATUS_INVALID_HANDLE;

	rwlock_read_lock(&curr_proc->handles->lock);

	khandle = curr_proc->handles->handles[handle];
	if(!khandle) {
		ret = STATUS_INVALID_HANDLE;
	} else if(!(khandle->object->type->flags & OBJECT_SECURABLE)) {
		ret = STATUS_NOT_SUPPORTED;
	} else {
		ret = memcpy_to_user(rightsp, &khandle->rights, sizeof(*rightsp));
	}

	rwlock_unlock(&curr_proc->handles->lock);
	return ret;
}

/**
 * Duplicate a handle table entry.
 *
 * Duplicates an entry in the calling process' handle table. The new handle ID
 * will refer to the same underlying handle as the source ID, i.e. they will
 * share the same state, for example for file handles they will share the same
 * file offset, etc. The new table entry's flags will be set to 0.
 *
 * @param handle	Handle ID to duplicate.
 * @param dest		Destination handle ID. If INVALID_HANDLE is specified,
 *			then a new handle ID is allocated. Otherwise, this
 *			exact ID will be used and any existing handle referred
 *			to by that ID will be closed.
 * @param newp		Where to store new handle ID. Can be NULL if dest is
 *			not INVALID_HANDLE.
 *
 * @return		STATUS_SUCCESS if successful.
 *			STATUS_INVALID_HANDLE if handle does not exist.
 *			STATUS_INVALID_ARG if dest is invalid, or if dest is
 *			INVALID_HANDLE and newp is NULL.
 *			STATUS_NO_HANDLES if allocating a handle ID and the
 *			handle table is full.
 */
status_t kern_handle_duplicate(handle_t handle, handle_t dest, handle_t *newp) {
	object_handle_t *khandle;
	status_t ret;

	if(handle < 0 || handle >= HANDLE_TABLE_SIZE)
		return STATUS_INVALID_HANDLE;

	if(dest == INVALID_HANDLE) {
		if(!newp)
			return STATUS_INVALID_ARG;
	} else if(dest < 0 || dest >= HANDLE_TABLE_SIZE) {
		return STATUS_INVALID_ARG;
	}

	rwlock_write_lock(&curr_proc->handles->lock);

	khandle = curr_proc->handles->handles[handle];
	if(!khandle) {
		rwlock_unlock(&curr_proc->handles->lock);
		return STATUS_INVALID_HANDLE;
	}

	if(dest != INVALID_HANDLE) {
		/* Close any existing handle in the slot. */
		object_handle_detach_unsafe(dest);
	} else {
		/* Try to allocate a new ID. */
		dest = bitmap_ffz(curr_proc->handles->bitmap, HANDLE_TABLE_SIZE);
		if(dest < 0) {
			rwlock_unlock(&curr_proc->handles->lock);
			return STATUS_NO_HANDLES;
		}
	}

	ret = memcpy_to_user(newp, &dest, sizeof(*newp));
	if(ret != STATUS_SUCCESS) {
		rwlock_unlock(&curr_proc->handles->lock);
		return ret;
	}

	/* Insert the new handle. */
	object_handle_retain(khandle);
	curr_proc->handles->handles[dest] = khandle;
	curr_proc->handles->flags[dest] = 0;
	bitmap_set(curr_proc->handles->bitmap, dest);

	dprintf("object: duplicated handle %" PRId32 " to %" PRId32 " in process %"
		PRId32 " (object: %p, data: %p)\n", handle, dest, curr_proc->id,
		khandle->object, khandle->data);
	rwlock_unlock(&curr_proc->handles->lock);
	return STATUS_SUCCESS;
}

/** Close a handle.
 * @param handle	Handle ID to close.
 * @return		STATUS_SUCCESS if successful.
 *			STATUS_INVALID_HANDLE if handle does not exist. */
status_t kern_handle_close(handle_t handle) {
	return object_handle_detach(handle);
}
