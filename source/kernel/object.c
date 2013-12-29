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

/** Define to enable debug output on handle creation/close. */
//#define DEBUG_HANDLE

#ifdef DEBUG_HANDLE
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

/** Object type names. */
static const char *object_type_names[] = {
	[OBJECT_TYPE_PROCESS] = "OBJECT_TYPE_PROCESS",
	[OBJECT_TYPE_THREAD] = "OBJECT_TYPE_THREAD",
	[OBJECT_TYPE_TOKEN] = "OBJECT_TYPE_TOKEN",
	[OBJECT_TYPE_TIMER] = "OBJECT_TYPE_TIMER",
	[OBJECT_TYPE_WATCHER] = "OBJECT_TYPE_WATCHER",
	[OBJECT_TYPE_AREA] = "OBJECT_TYPE_AREA",
	[OBJECT_TYPE_FILE] = "OBJECT_TYPE_FILE",
	[OBJECT_TYPE_PORT] = "OBJECT_TYPE_PORT",
	[OBJECT_TYPE_CONNECTION] = "OBJECT_TYPE_CONNECTION",
	[OBJECT_TYPE_SEMAPHORE] = "OBJECT_TYPE_SEMAPHORE",
};

/** Notifier function to use for object waiting.
 * @param arg1		Unused.
 * @param arg2		Wait structure pointer.
 * @param arg3		Data for the event (unsigned long cast to void *). */
void object_wait_notifier(void *arg1, void *arg2, void *arg3) {
	object_wait_signal(arg2, (unsigned long)arg3);
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
 * Creates a new handle to a kernel object. The handle will have a single
 * reference on it. The handle must be closed with object_handle_release() when
 * it is no longer required.
 *
 * @param type		Type of the object.
 * @param private	Per-handle data pointer. This can be a pointer to the
 *			object, or for object types that need per-handle state,
 *			a pointer to a structure containing the object pointer
 *			plus the required state.
 *
 * @return		Handle to the object.
 */
object_handle_t *object_handle_create(object_type_t *type, void *private) {
	object_handle_t *handle;

	handle = slab_cache_alloc(object_handle_cache, MM_WAIT);
	refcount_set(&handle->count, 1);
	handle->type = type;
	handle->private = private;
	return handle;
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
		if(handle->type->close)
			handle->type->close(handle);

		slab_cache_free(object_handle_cache, handle);
	}
}

/**
 * Look up a handle in a the current process' handle table.
 *
 * Looks up the handle with the given ID in the current process' handle table,
 * optionally checking that the object it refers to is a certain type. The
 * returned handle will be referenced: when it is no longer needed, it should
 * be released with object_handle_release().
 *
 * @param id		Handle ID to look up.
 * @param type		Required object type ID (if negative, no type checking
 *			will be performed).
 * @param handlep	Where to store pointer to handle structure.
 *
 * @return		Status code describing result of the operation.
 */
status_t object_handle_lookup(handle_t id, int type, object_handle_t **handlep) {
	handle_table_t *table = &curr_proc->handles;
	object_handle_t *handle;

	assert(handlep);

	if(id < 0 || id >= HANDLE_TABLE_SIZE)
		return STATUS_INVALID_HANDLE;

	rwlock_read_lock(&table->lock);

	handle = table->handles[id];
	if(!handle) {
		rwlock_unlock(&table->lock);
		return STATUS_INVALID_HANDLE;
	}

	/* Check if the type is the type the caller wants. */
	if(type >= 0 && handle->type->id != (unsigned)type) {
		rwlock_unlock(&table->lock);
		return STATUS_INVALID_HANDLE;
	}

	object_handle_retain(handle);
	rwlock_unlock(&table->lock);

	*handlep = handle;
	return STATUS_SUCCESS;
}

/** Internal part of object_handle_attach().
 * @param handle	Handle to attach.
 * @param idp		If not NULL, a kernel location to store handle ID in.
 * @param uidp		If not NULL, a user location to store handle ID in.
 * @param retain	Whether to retain the handle.
 * @return		Status code describing result of the operation. */
static status_t object_handle_attach_internal(object_handle_t *handle,
	handle_t *idp, handle_t *uidp, bool retain)
{
	handle_table_t *table = &curr_proc->handles;
	handle_t id;
	status_t ret;

	assert(handle);
	assert(idp || uidp);

	rwlock_write_lock(&table->lock);

	/* Find a handle ID in the table. */
	id = bitmap_ffz(table->bitmap, HANDLE_TABLE_SIZE);
	if(id < 0) {
		rwlock_unlock(&table->lock);
		return STATUS_NO_HANDLES;
	}

	if(idp)
		*idp = id;

	if(uidp) {
		ret = write_user(uidp, id);
		if(ret != STATUS_SUCCESS) {
			rwlock_unlock(&table->lock);
			return ret;
		}
	}

	if(retain)
		object_handle_retain(handle);

	if(handle->type->attach)
		handle->type->attach(handle, curr_proc);

	table->handles[id] = handle;
	table->flags[id] = 0;
	bitmap_set(table->bitmap, id);

	rwlock_unlock(&table->lock);

	dprintf("object: allocated handle %" PRId32 " in process %" PRId32 " ("
		"type: %u, private: %p)\n", id, curr_proc->id, handle->type->id,
		handle->private);
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
status_t object_handle_attach(object_handle_t *handle, handle_t *idp,
	handle_t *uidp)
{
	return object_handle_attach_internal(handle, idp, uidp, true);
}

/** Detach a handle from the current process' handle table.
 * @param id		ID of handle to detach.
 * @return		Status code describing result of the operation. */
static status_t object_handle_detach_unsafe(handle_t id) {
	handle_table_t *table = &curr_proc->handles;
	object_handle_t *handle;

	if(id < 0 || id >= HANDLE_TABLE_SIZE || !table->handles[id])
		return STATUS_INVALID_HANDLE;

	handle = table->handles[id];

	if(handle->type->detach)
		handle->type->detach(handle, curr_proc);

	dprintf("object: detached handle %" PRId32 " from process %" PRId32 " "
		"(count: %d)\n", id, curr_proc->id,
		refcount_get(&handle->count));

	object_handle_release(handle);
	table->handles[id] = NULL;
	table->flags[id] = 0;
	bitmap_clear(table->bitmap, id);

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

	rwlock_write_lock(&curr_proc->handles.lock);
	ret = object_handle_detach_unsafe(id);
	rwlock_unlock(&curr_proc->handles.lock);
	return ret;
}

/**
 * Create a handle in the current process' handle table.
 *
 * Allocates a handle ID in the current process and creates a new handle in its
 * handle table. This is a shortcut for creating a new handle with
 * object_handle_create() and then attaching it with object_handle_attach().
 * The behaviour of this function also differs slightly from doing that: if
 * attaching the handle fails, the object type's release method will not be
 * called. Note that as soon as this function succeeds, it is possible for the
 * process to close the handle and cause it to be released.
 *
 * @param type		Type of the object.
 * @param private	Per-handle data pointer. This can be a pointer to the
 *			object, or for object types that need per-handle state,
 *			a pointer to a structure containing the object pointer
 *			plus the required state.
 * @param idp		If not NULL, a kernel location to store handle ID in.
 * @param uidp		If not NULL, a user location to store handle ID in.
 *
 * @return		Status code describing result of the operation.
 */
status_t object_handle_open(object_type_t *type, void *private, handle_t *idp,
	handle_t *uidp)
{
	object_handle_t *handle;
	status_t ret;

	handle = object_handle_create(type, private);
	ret = object_handle_attach_internal(handle, idp, uidp, false);
	if(ret != STATUS_SUCCESS) {
		slab_cache_free(object_handle_cache, handle);
		return ret;
	}

	return ret;
}

/** Initialize a process' handle table.
 * @param process	Process to initialize. */
void object_process_init(process_t *process) {
	handle_table_t *table = &process->handles;

	rwlock_init(&table->lock, "handle_table_lock");

	table->handles = kcalloc(HANDLE_TABLE_SIZE, sizeof(table->handles[0]),
		MM_KERNEL);
	table->flags = kcalloc(HANDLE_TABLE_SIZE, sizeof(table->flags[0]),
		MM_KERNEL);
	table->bitmap = bitmap_alloc(HANDLE_TABLE_SIZE, MM_KERNEL);
}

/** Destroy a process' handle table.
 * @param process	Process to clean up. */
void object_process_cleanup(process_t *process) {
	handle_table_t *table = &process->handles;
	size_t i;
	object_handle_t *handle;

	for(i = 0; i < HANDLE_TABLE_SIZE; i++) {
		handle = table->handles[i];
		if(handle) {
			if(handle->type->detach)
				handle->type->detach(handle, process);

			dprintf("object: detached handle %" PRId32 " from "
				"process %" PRId32 " (count: %d)\n", i,
				process->id, refcount_get(&handle->count));

			object_handle_release(handle);
		}
	}

	kfree(table->bitmap);
	kfree(table->flags);
	kfree(table->handles);
}

/** Inherit a handle from one table to another.
 * @param table		Table to duplicate to.
 * @param dest		Destination handle ID.
 * @param parent	Parent table.
 * @param source	Source handle ID.
 * @param process	Process to attach to, if any.
 * @return		Status code describing result of the operation. */
static status_t inherit_handle(handle_table_t *table, handle_t dest,
	handle_table_t *parent, handle_t source, process_t *process)
{
	object_handle_t *handle;

	if(source < 0 || source >= HANDLE_TABLE_SIZE) {
		return STATUS_INVALID_HANDLE;
	} else if(dest < 0 || dest >= HANDLE_TABLE_SIZE) {
		return STATUS_INVALID_HANDLE;
	} else if(!parent->handles[source]) {
		return STATUS_INVALID_HANDLE;
	} else if(table->handles[dest]) {
		return STATUS_ALREADY_EXISTS;
	}

	handle = parent->handles[source];

	/* When using a map, the inheritable flag is ignored so we must check
	 * whether transferring handles is allowed. */
	if(!(handle->type->flags & OBJECT_TRANSFERRABLE))
		return STATUS_NOT_SUPPORTED;

	object_handle_retain(handle);

	if(process && handle->type->attach)
		handle->type->attach(handle, process);

	table->handles[dest] = handle;
	table->flags[dest] = parent->flags[source];
	bitmap_set(table->bitmap, dest);
	return STATUS_SUCCESS;
}

/**
 * Duplicate handles to a new process.
 *
 * Duplicates handles from a process into a newly created process, either using
 * provided mapping information, or based on the inheritable flag of handles.
 *
 * @param process	Newly created process.
 * @param parent	Parent process.
 * @param map		An array specifying handles to add to the new table.
 *			The first ID of each entry specifies the handle in the
 *			parent, and the second specifies the ID to give it in
 *			the new table. Can be NULL if count <= 0.
 * @param count		The number of handles in the array. If negative, the
 *			map will be ignored and all handles with the inheritable
 *			flag set will be duplicated. If 0, no handles will be
 *			duplicated.
 *
 * @return		STATUS_SUCCESS on success (always the case with no map).
 *			STATUS_INVALID_HANDLE if the given map specifies a
 *			non-existant handle in the parent.
 *			STATUS_ALREADY_EXISTS if the given map maps multiple
 *			handles from the parent to the same ID in the new table.
 */
status_t object_process_create(process_t *process, process_t *parent,
	handle_t map[][2], ssize_t count)
{
	ssize_t i;
	status_t ret;

	if(!count)
		return STATUS_SUCCESS;

	rwlock_read_lock(&parent->handles.lock);

	if(count > 0) {
		assert(map);

		for(i = 0; i < count; i++) {
			ret = inherit_handle(&process->handles, map[i][1],
				&parent->handles, map[i][0], process);
			if(ret != STATUS_SUCCESS)
				goto out;
		}
	} else {
		/* Inherit all inheritable handles in the parent table. */
		for(i = 0; i < HANDLE_TABLE_SIZE; i++) {
			/* Flag can only be set if handle is not NULL and the
			 * type allows transferring. */
			if(parent->handles.flags[i] & HANDLE_INHERITABLE) {
				inherit_handle(&process->handles, i,
					&parent->handles, i, process);
			}
		}
	}

	ret = STATUS_SUCCESS;
out:
	/* We don't need to clean up on failure - this will be done when the
	 * process gets destroyed. */
	rwlock_unlock(&parent->handles.lock);
	return ret;
}

/**
 * Close handles when executing a new program.
 *
 * If no map is specified, closes all non-inheritable handles in the given
 * process. If a map is specified, replaces the process' handle table with a
 * new table containing all handles from the old table specified by the map.
 *
 * @param process	Process that is executing a new program.
 * @param map		An array specifying handles to add to the new table.
 *			The first ID of each entry specifies the handle in the
 *			parent, and the second specifies the ID to give it in
 *			the new table. Can be NULL if count <= 0.
 * @param count		The number of handles in the array. If negative, the
 *			map will be ignored and all handles with the inheritable
 *			flag set will be duplicated. If 0, no handles will be
 *			duplicated.
 *
 * @return		STATUS_SUCCESS on success (always the case with no map).
 *			STATUS_INVALID_HANDLE if the given map specifies a
 *			non-existant handle in the parent.
 *			STATUS_ALREADY_EXISTS if the given map maps multiple
 *			handles from the parent to the same ID in the new table.
 */
status_t object_process_exec(process_t *process, handle_t map[][2],
	ssize_t count)
{
	handle_table_t new;
	ssize_t i;
	object_handle_t *handle;
	status_t ret;

	/* The attach and detach callbacks are used by IPC code to track when
	 * ports are no longer referenced by their owning process. When we exec
	 * a process, that should count as giving up ownership of ports.
	 * Therefore, this function has to do a somewhat complicated dance to
	 * ensure that this happens: we first populate a new table without
	 * calling attach on any handles. If that succeeds, we then call
	 * detach on all handles in the old table, and *then* call attach on
	 * all in the new table. This ensures that the IPC code correctly sees
	 * that all references from the process to any ports it owns are
	 * dropped and disowns the ports, before we re-add any references. */

	new.handles = kcalloc(HANDLE_TABLE_SIZE, sizeof(new.handles[0]),
		MM_KERNEL);
	new.flags = kcalloc(HANDLE_TABLE_SIZE, sizeof(new.flags[0]),
		MM_KERNEL);
	new.bitmap = bitmap_alloc(HANDLE_TABLE_SIZE, MM_KERNEL);

	if(count > 0) {
		assert(map);

		for(i = 0; i < count; i++) {
			ret = inherit_handle(&new, map[i][1], &process->handles,
				map[i][0], NULL);
			if(ret != STATUS_SUCCESS) {
				while(i--) {
					handle = new.handles[map[i][1]];
					object_handle_release(handle);
				}

				goto out;
			}
		}
	} else if(count < 0) {
		for(i = 0; i < HANDLE_TABLE_SIZE; i++) {
			if(process->handles.flags[i] & HANDLE_INHERITABLE) {
				inherit_handle(&new, i, &process->handles,
					i, NULL);
			}
		}
	}

	/* Now we can detach and release all handles in the old table. */
	for(i = 0; i < HANDLE_TABLE_SIZE; i++) {
		handle = process->handles.handles[i];
		if(handle) {
			if(handle->type->detach)
				handle->type->detach(handle, process);

			object_handle_release(handle);
		}
	}

	SWAP(process->handles.handles, new.handles);
	SWAP(process->handles.flags, new.flags);
	SWAP(process->handles.bitmap, new.bitmap);

	/* Finally, attach all handles in the new table. */
	for(i = 0; i < HANDLE_TABLE_SIZE; i++) {
		handle = process->handles.handles[i];
		if(handle && handle->type->attach)
			handle->type->attach(handle, process);
	}

	ret = STATUS_SUCCESS;
out:
	kfree(new.bitmap);
	kfree(new.flags);
	kfree(new.handles);
	return ret;
}

/**
 * Clone an existing process' handle table to a new process.
 *
 * Clones all handles, including non-inheritable ones (unless they are of a
 * non-transferrable type), from an existing process' handle table to a new
 * process. The table entries will all refer to the same underlying handles as
 * the old table.
 *
 * @param process	New process.
 * @param parent	Original process.
 */
void object_process_clone(process_t *process, process_t *parent) {
	size_t i;

	rwlock_read_lock(&parent->handles.lock);

	for(i = 0; i < HANDLE_TABLE_SIZE; i++) {
		/* inherit_handle() ignores non-transferrable handles. */
		if(parent->handles.handles[i]) {
			inherit_handle(&process->handles, i, &parent->handles,
				i, process);
		}
	}

	rwlock_unlock(&parent->handles.lock);
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
		kdb_printf("Incorrect number of arguments. See 'help %s' for help.\n",
			argv[0]);
		return KDB_FAILURE;
	}

	if(kdb_parse_expression(argv[1], &id, NULL) != KDB_SUCCESS)
		return KDB_FAILURE;

	process = process_lookup_unsafe(id);
	if(!process) {
		kdb_printf("Invalid process ID.\n");
		return KDB_FAILURE;
	}

	kdb_printf("ID   Flags  Type                        Count Private\n");
	kdb_printf("==   =====  ====                        ===== =======\n");

	for(i = 0; i < HANDLE_TABLE_SIZE; i++) {
		handle = process->handles.handles[i];
		if(!handle)
			continue;

		kdb_printf("%-4zu 0x%-4" PRIx32 " %-2u - %-22s %-5" PRId32
			" %p\n", i, process->handles.flags[i], handle->type->id,
			(handle->type->id < ARRAY_SIZE(object_type_names))
				? object_type_names[handle->type->id]
				: "Unknown",
			refcount_get(&handle->count), handle->private);
	}

	return KDB_SUCCESS;
}

/** Initialize the handle caches. */
__init_text void object_init(void) {
	object_handle_cache = object_cache_create("object_handle_cache",
		object_handle_t, NULL, NULL, NULL, 0, MM_BOOT);

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

	if(object_handle_lookup(handle, -1, &khandle) != STATUS_SUCCESS)
		return -1;

	ret = khandle->type->id;
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
 * If the OBJECT_WAIT_ALL flag is specified, then the function will wait until
 * all of the given events occur, rather than just one of them. If a wait with
 * OBJECT_WAIT_ALL times out or is interrupted, some of the events may have
 * fired, so the events array will be updated with the status of each event.
 *
 * @param events	Array of structures describing events to wait for. If
 *			the function returns STATUS_SUCCESS, STATUS_TIMED_OUT,
 *			STATUS_INTERRUPTED or STATUS_WOULD_BLOCK, the signalled
 *			field of each structure will be updated to reflect
 *			whether or not the event was signalled, and if set to
 *			true then the data field will be updated with the data
 *			value associated with the event.
 * @param count		Number of array entries.
 * @param flags		Behaviour flags for waiting. See above
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
 *			STATUS_WOULD_BLOCK if the timeout is 0 and no events
 *			have already occurred.
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
		ret = memcpy_from_user(&waits[i].info, &events[i],
			sizeof(*events));
		if(ret != STATUS_SUCCESS)
			goto out;

		ret = object_handle_lookup(waits[i].info.handle, -1, &handle);
		if(ret != STATUS_SUCCESS) {
			goto out;
		} else if(!handle->type->wait || !handle->type->unwait) {
			ret = STATUS_INVALID_EVENT;
			object_handle_release(handle);
			goto out;
		}

		waits[i].sync = &sync;
		waits[i].handle = handle;
		waits[i].info.signalled = false;

		ret = handle->type->wait(handle, waits[i].info.event,
			&waits[i]);
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
		sync.thread = curr_thread;
		ret = thread_sleep(&sync.lock, timeout, "object_wait",
			SLEEP_INTERRUPTIBLE);
	}
out:
	/* Cancel all waits which have been set up. */
	while(i--) {
		handle = waits[i].handle;
		handle->type->unwait(handle, waits[i].info.event, &waits[i]);
		object_handle_release(handle);

		/* If we're waiting with OBJECT_WAIT_ALL and we've timed out or
		 * been interrupted, some of the events could have fired so
		 * return them. */
		switch(ret) {
		case STATUS_SUCCESS:
		case STATUS_TIMED_OUT:
		case STATUS_INTERRUPTED:
		case STATUS_WOULD_BLOCK:
			err = memcpy_to_user(&events[i], &waits[i].info,
				sizeof(*events));
			if(err != STATUS_SUCCESS)
				ret = err;

			break;
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
	handle_table_t *table = &curr_proc->handles;
	status_t ret;

	if(handle < 0 || handle >= HANDLE_TABLE_SIZE)
		return STATUS_INVALID_HANDLE;

	rwlock_read_lock(&table->lock);

	if(!table->handles[handle]) {
		rwlock_unlock(&table->lock);
		return STATUS_INVALID_HANDLE;
	}

	ret = write_user(flagsp, table->flags[handle]);
	rwlock_unlock(&table->lock);
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
	handle_table_t *table = &curr_proc->handles;
	object_handle_t *khandle;

	if(handle < 0 || handle >= HANDLE_TABLE_SIZE)
		return STATUS_INVALID_HANDLE;

	/* Don't need to write lock just to set flags, it's atomic. */
	rwlock_read_lock(&table->lock);

	khandle = table->handles[handle];
	if(!khandle) {
		rwlock_unlock(&table->lock);
		return STATUS_INVALID_HANDLE;
	}

	/* To set the inheritable flag, the object type must be transferrable. */
	if(flags & HANDLE_INHERITABLE) {
		if(!(khandle->type->flags & OBJECT_TRANSFERRABLE)) {
			rwlock_unlock(&table->lock);
			return STATUS_NOT_SUPPORTED;
		}
	}

	table->flags[handle] = flags;
	rwlock_unlock(&table->lock);
	return STATUS_SUCCESS;
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
	handle_table_t *table = &curr_proc->handles;
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

	rwlock_write_lock(&table->lock);

	khandle = table->handles[handle];
	if(!khandle) {
		rwlock_unlock(&table->lock);
		return STATUS_INVALID_HANDLE;
	}

	if(dest != INVALID_HANDLE) {
		/* Close any existing handle in the slot. */
		object_handle_detach_unsafe(dest);
	} else {
		/* Try to allocate a new ID. */
		dest = bitmap_ffz(table->bitmap, HANDLE_TABLE_SIZE);
		if(dest < 0) {
			rwlock_unlock(&table->lock);
			return STATUS_NO_HANDLES;
		}
	}

	ret = write_user(newp, dest);
	if(ret != STATUS_SUCCESS) {
		rwlock_unlock(&table->lock);
		return ret;
	}

	if(khandle->type->attach)
		khandle->type->attach(khandle, curr_proc);

	/* Insert the new handle. */
	object_handle_retain(khandle);
	table->handles[dest] = khandle;
	table->flags[dest] = 0;
	bitmap_set(table->bitmap, dest);

	dprintf("object: duplicated handle %" PRId32 " to %" PRId32 " in "
		"process %" PRId32 " (type: %u, private: %p)\n", handle, dest,
		curr_proc->id, khandle->type->id, khandle->private);
	rwlock_unlock(&table->lock);
	return STATUS_SUCCESS;
}

/** Close a handle.
 * @param handle	Handle ID to close.
 * @return		STATUS_SUCCESS if successful.
 *			STATUS_INVALID_HANDLE if handle does not exist. */
status_t kern_handle_close(handle_t handle) {
	return object_handle_detach(handle);
}
