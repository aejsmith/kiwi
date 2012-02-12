/*
 * Copyright (C) 2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
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
 * @note		The ACL-related functionality is not implemented in
 *			this file.
 * @see			security/acl.c
 */

#include <mm/malloc.h>
#include <mm/safe.h>
#include <mm/slab.h>

#include <proc/process.h>

#include <security/context.h>

#include <sync/semaphore.h>

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

/** Object waiting internal data structure. */
typedef struct object_wait_sync {
	semaphore_t *sem;		/**< Pointer to semaphore to up. */
	object_event_t info;		/**< Userspace-supplied event information. */
	object_handle_t *handle;	/**< Handle being waited on structure. */
} object_wait_sync_t;

/** Structure linking a handle to a handle table. */
typedef struct handle_link {
	object_handle_t *handle;	/**< Handle the link refers to. */
	int flags;			/**< Behaviour flags for the handle. */
	avl_tree_node_t link;		/**< AVL tree link. */
} handle_link_t;

/** Cache for handle structures. */
static slab_cache_t *object_handle_cache;

/** Cache for handle table structures. */
static slab_cache_t *handle_table_cache;

/** Constructor for handle table objects.
 * @param obj		Object to construct.
 * @param data		Cache data pointer. */
static void handle_table_ctor(void *obj, void *data) {
	handle_table_t *table = obj;

	avl_tree_init(&table->tree);
	rwlock_init(&table->lock, "handle_table_lock");
}

/** Initialize an object structure.
 * @param obj		Object to initialize.
 * @param type		Pointer to type structure for object type. Can be NULL,
 *			in which case the object will be a 'NULL object', and
 *			handles will never be created to it.
 * @param security	Details of the owning user/group and ACL to assign to
 *			the object. The ACL pointer must not be NULL: default
 *			ACLs must be built by the caller. Note that this
 *			function does not perform any checks as to whether the
 *			security attributes should be allowed, nor does it
 *			canonicalise the ACL: to copy in a security attributes
 *			structure from userspace, object_security_from_user()
 *			must be used, which copies the structure, canonicalises
 *			the ACL and validates it. This function does not copy
 *			the data for the ACL, so it invalidates the structure
 *			after taking the data pointer from it, which means it
 *			will be safe to call object_acl_destroy() on it.
 * @param sacl		If not NULL, the system ACL to give to the object. The
 *			system ACL is interpreted differently to the user ACL:
 *			in the user ACL user, group and others entries are
 *			exclusive. In the system ACL they are all used together.
 *			This ACL should be in canonical form (see
 *			object_acl_canonicalise()). This function does not copy
 *			the data for the ACL, so it invalidates the structure
 *			after taking the data pointer from it, which means it
 *			will be safe to call object_acl_destroy() on it. */
void object_init(object_t *object, object_type_t *type, object_security_t *security, object_acl_t *sacl) {
	assert(object);
	if(type) {
		assert(security);
		assert(security->acl);
	}

	rwlock_init(&object->lock, "object_lock");
	object->type = type;

	/* If a user/group ID are provided, use them, else use the current. */
	object->uid = (security->uid >= 0) ? security->uid : security_current_uid();
	object->gid = (security->gid >= 0) ? security->gid : security_current_gid();

	/* Set the user ACL and invalidate the provided structure. */
	object->uacl.entries = security->acl->entries;
	object->uacl.count = security->acl->count;
	object_acl_init(security->acl);

	/* If a system ACL is provided, use it, otherwise just make an empty one. */
	if(sacl) {
		object->sacl.entries = sacl->entries;
		object->sacl.count = sacl->count;
		object_acl_init(sacl);
	} else {
		object_acl_init(&object->sacl);
	}

	/* Add default system ACL entries. We always allow an object's owning
	 * user to change its ACL and owner. */
	object_acl_add_entry(&object->sacl, ACL_ENTRY_USER, -1, OBJECT_RIGHT_OWNER);
}

/** Destroy an object structure.
 * @param obj		Object to destroy. */
void object_destroy(object_t *object) {
	object_acl_destroy(&object->uacl);
	object_acl_destroy(&object->sacl);
}

/** Notifier function to use for object waiting.
 * @param arg1		Unused.
 * @param arg2		Unused.
 * @param arg3		Wait structure pointer. */
void object_wait_notifier(void *arg1, void *arg2, void *arg3) {
	object_wait_signal(arg3);
}

/** Signal that an event being waited for has occurred.
 * @param sync		Internal data pointer. */
void object_wait_signal(void *_sync) {
	object_wait_sync_t *sync = _sync;
	sync->info.signalled = true;
	semaphore_up(sync->sem, 1);
}

/**
 * Create a handle to an object.
 *
 * Creates a new handle to an object, and optionally attaches it to a process.
 * If either the idp or uidp parameter is not NULL, the handle will be attached.
 *
 * Note that this function does NOT check whether the process is allowed the
 * rights specified. This is because it must be possible to create a handle
 * without doing rights checks when creating objects (when creating objects you
 * can open them with rights the ACL does not grant). If it is necessary to
 * perform a rights check, either perform it before calling this, or use the
 * object_handle_open() wrapper around this function.
 *
 * @param object	Object to create handle to.
 * @param data		Per-handle data pointer.
 * @param rights	Rights for the handle.
 * @param process	Process to attach handle to. If NULL, the current
 *			process will be used.
 * @param flags		Flags for the handle table entry if attaching.
 * @param handlep	If not NULL, Where to store pointer to kernel handle
 *			structure.
 * @param idp		If not NULL, specifies a kernel location to store the
 *			handle ID in (see above).
 * @param uidp		If not NULL, specifies a user location to store the
 *			handle ID in (see above).
 *
 * @return		Status code describing result of the operation.
 */
status_t object_handle_create(object_t *object, void *data, object_rights_t rights,
                              process_t *process, int flags, object_handle_t **handlep,
                              handle_t *idp, handle_t *uidp) {
	object_handle_t *handle;
	status_t ret;

	assert(object);
	assert(object->type);
	assert(handlep || idp || uidp);

	/* Create the kernel handle structure. */
	handle = slab_cache_alloc(object_handle_cache, MM_WAIT);
	refcount_set(&handle->count, 1);
	handle->object = object;
	handle->data = data;
	handle->rights = rights;

	/* If required, attach it to the process. */
	if(idp || uidp) {
		ret = object_handle_attach(handle, process, flags, idp, uidp);
		if(ret != STATUS_SUCCESS) {
			/* We do not want the object's close operation to be
			 * called upon failure, so free the handle directly. */
			slab_cache_free(object_handle_cache, handle);
			return ret;
		}
	}

	/* Store a pointer if requested. */
	if(handlep) {
		*handlep = handle;
	} else {
		object_handle_release(handle);
	}

	return STATUS_SUCCESS;
}

/** Open a handle to an object.
 * @note		This is a wrapper around object_handle_create() which
 *			checks whether the process is allowed the specified
 *			rights on the object.
 * @see			object_handle_create(). */
status_t object_handle_open(object_t *object, void *data, object_rights_t rights,
                            process_t *process, int flags, object_handle_t **handlep,
                            handle_t *idp, handle_t *uidp) {
	if(rights && (object_rights(object, process) & rights) != rights) {
		return STATUS_ACCESS_DENIED;
	}

	return object_handle_create(object, data, rights, process, flags, handlep, idp, uidp);
}

/**
 * Increase the reference count of a handle.
 *
 * Increases the reference count of a handle to signal that it is being used.
 * When the handle is no longer needed it should be released with
 * object_handle_release().
 * 
 * @param handle	Handle to increase reference count of.
 */
void object_handle_get(object_handle_t *handle) {
	assert(handle);
	refcount_inc(&handle->count);
}

/**
 * Release a handle.
 *
 * Decreases the reference count of a handle. If no more references remain to
 * the handle, it will be destroyed.
 *
 * @param handle	Handle to release.
 */
void object_handle_release(object_handle_t *handle) {
	assert(handle);

	/* If there are no more references we can close it. */
	if(refcount_dec(&handle->count) == 0) {
		if(handle->object->type->close) {
			handle->object->type->close(handle);
		}
		slab_cache_free(object_handle_cache, handle);
	}
}

/** Insert a handle into a handle table.
 * @param table		Table to insert into.
 * @param id		ID to give the handle.
 * @param handle	Handle to insert.
 * @param flags		Flags for the handle. */
static void handle_table_insert(handle_table_t *table, handle_t id, object_handle_t *handle, int flags) {
	handle_link_t *link;

	object_handle_get(handle);

	link = kmalloc(sizeof(*link), MM_WAIT);
	link->handle = handle;
	link->flags = flags;

	bitmap_set(&table->bitmap, id);
	avl_tree_insert(&table->tree, &link->link, id, link);
}

/**
 * Insert a handle into a process' handle table.
 *
 * Allocates a handle ID for a process and adds a handle to it. On success,
 * the handle will have an extra reference on it.
 *
 * @param handle	Handle to attach.
 * @param process	Process to attach to. If NULL, current process will be
 *			used.
 * @param flags		Flags for the handle.
 * @param idp		If not NULL, a kernel location to store handle ID in.
 * @param uidp		If not NULL, a user location to store handle ID in.
 *
 * @return		Status code describing result of the operation.
 */
status_t object_handle_attach(object_handle_t *handle, process_t *process, int flags,
                              handle_t *idp, handle_t *uidp) {
	status_t ret;
	handle_t id;

	assert(handle);
	assert(idp || uidp);

	if(!process) {
		process = curr_proc;
	}

	rwlock_write_lock(&process->handles->lock);

	/* Find a handle ID in the table. */
	id = bitmap_ffz(&process->handles->bitmap);
	if(id < 0) {
		rwlock_unlock(&process->handles->lock);
		return STATUS_NO_HANDLES;
	}

	/* Copy the handle ID before actually attempting to insert so we don't
	 * have to detach if this fails. */
	if(uidp) {
		ret = memcpy_to_user(uidp, &id, sizeof(*uidp));
		if(ret != STATUS_SUCCESS) {
			rwlock_unlock(&process->handles->lock);
			return ret;
		}
	}

	handle_table_insert(process->handles, id, handle, flags);
	rwlock_unlock(&process->handles->lock);

	dprintf("object: allocated handle %d in process %" PRId32 " (object: %p, data: %p)\n",
	        id, process->id, handle->object, handle->data);
	if(idp) {
		*idp = id;
	}
	return STATUS_SUCCESS;
}

/** Detach a handle from a process' handle table.
 * @param process	Process to remove from.
 * @param id		ID of handle to detach.
 * @return		Status code describing result of the operation. */
static status_t object_handle_detach_unsafe(process_t *process, handle_t id) {
	handle_link_t *link;

	/* Look up the handle in the tree. */
	link = avl_tree_lookup(&process->handles->tree, id);
	if(!link) {
		return STATUS_INVALID_HANDLE;
	}

	/* Remove from the tree and mark the ID as free. */
	avl_tree_remove(&process->handles->tree, &link->link);
	bitmap_clear(&process->handles->bitmap, id);

	/* Release the handle and free the link. */
	object_handle_release(link->handle);
	kfree(link);

	dprintf("object: detached handle %" PRId32 " from process %" PRId32 "\n",
	        id, process->id);
	return STATUS_SUCCESS;
}

/**
 * Detach a handle from a process.
 *
 * Removes the specified handle ID from a process' handle table and releases
 * the handle.
 *
 * @param process	Process to remove from. If NULL, the current process
 *			will be used.
 * @param id		ID of handle to detach.
 *
 * @return		Status code describing result of the operation.
 */
status_t object_handle_detach(process_t *process, handle_t id) {
	status_t ret;

	if(!process) {
		process = curr_proc;
	}

	rwlock_write_lock(&process->handles->lock);
	ret = object_handle_detach_unsafe(process, id);
	rwlock_unlock(&process->handles->lock);
	return ret;
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
status_t object_handle_lookup(handle_t id, int type, object_rights_t rights, object_handle_t **handlep) {
	handle_link_t *link;

	assert(handlep);

	rwlock_read_lock(&curr_proc->handles->lock);

	/* Look up the handle in the tree. */
	link = avl_tree_lookup(&curr_proc->handles->tree, id);
	if(!link) {
		rwlock_unlock(&curr_proc->handles->lock);
		return STATUS_INVALID_HANDLE;
	}

	/* Check if the type is the type the caller wants. */
	if(type >= 0 && link->handle->object->type->id != type) {
		rwlock_unlock(&curr_proc->handles->lock);
		return STATUS_INVALID_HANDLE;
	}

	/* Check if the handle has the requested rights. */
	if(rights && !object_handle_rights(link->handle, rights)) {
		rwlock_unlock(&curr_proc->handles->lock);
		return STATUS_ACCESS_DENIED;
	}

	object_handle_get(link->handle);
	*handlep = link->handle;
	rwlock_unlock(&curr_proc->handles->lock);
	return STATUS_SUCCESS;
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
                             handle_table_t **tablep) {
	handle_table_t *table;
	handle_link_t *link;
	int i;

	table = slab_cache_alloc(handle_table_cache, MM_WAIT);
	bitmap_init(&table->bitmap, CONFIG_HANDLE_MAX, NULL, MM_WAIT);

	/* Inherit all inheritable handles in the parent table. */
	if(parent && count != 0) {
		rwlock_read_lock(&parent->lock);

		if(count > 0) {
			assert(map);

			for(i = 0; i < count; i++) {
				link = avl_tree_lookup(&parent->tree, map[i][0]);
				if(!link) {
					rwlock_unlock(&parent->lock);
					handle_table_destroy(table);
					return STATUS_INVALID_HANDLE;
				} else if(map[i][1] >= CONFIG_HANDLE_MAX) {
					rwlock_unlock(&parent->lock);
					handle_table_destroy(table);
					return STATUS_INVALID_HANDLE;
				} else if(avl_tree_lookup(&table->tree, map[i][1])) {
					rwlock_unlock(&parent->lock);
					handle_table_destroy(table);
					return STATUS_ALREADY_EXISTS;
				}

				handle_table_insert(table, map[i][1], link->handle, link->flags);
			}
		} else {
			AVL_TREE_FOREACH(&parent->tree, iter) {
				link = avl_tree_entry(iter, handle_link_t);

				if(link->flags & HANDLE_INHERITABLE) {
					handle_table_insert(table, iter->key, link->handle,
					                    link->flags);
				}
			}
		}

		rwlock_unlock(&parent->lock);
	}

	*tablep = table;
	return STATUS_SUCCESS;
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
	handle_link_t *link;

	table = slab_cache_alloc(handle_table_cache, MM_WAIT);
	bitmap_init(&table->bitmap, CONFIG_HANDLE_MAX, NULL, MM_WAIT);

	rwlock_read_lock(&src->lock);

	AVL_TREE_FOREACH(&src->tree, iter) {
		link = avl_tree_entry(iter, handle_link_t);
		handle_table_insert(table, iter->key, link->handle, link->flags);
	}

	rwlock_unlock(&src->lock);
	return table;
}

/** Destroy a handle table.
 * @param table		Table being destroyed. */
void handle_table_destroy(handle_table_t *table) {
	handle_link_t *link;

	/* Close all handles. */
	AVL_TREE_FOREACH_SAFE(&table->tree, iter) {
		link = avl_tree_entry(iter, handle_link_t);
		object_handle_release(link->handle);
		avl_tree_remove(&table->tree, &link->link);
		kfree(link);
	}

	bitmap_destroy(&table->bitmap);
	slab_cache_free(handle_table_cache, table);
}

/** Print a list of a process' handles.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_handles(int argc, char **argv, kdb_filter_t *filter) {
	handle_link_t *link;
	process_t *process;
	uint64_t id;

	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s <process ID>\n\n", argv[0]);

		kdb_printf("Prints out a list of all currently open handles in a process.\n");
		return KDB_SUCCESS;
	} else if(argc != 2) {
		kdb_printf("Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
		return KDB_FAILURE;
	}

	if(kdb_parse_expression(argv[1], &id, NULL) != KDB_SUCCESS) {
		return KDB_FAILURE;
	} else if(!(process = process_lookup_unsafe(id))) {
		kdb_printf("Invalid process ID.\n");
		return KDB_FAILURE;
	}

	kdb_printf("ID    Object             Type                  Count  Data\n");
	kdb_printf("==    ======             ====                  =====  ====\n");

	AVL_TREE_FOREACH(&process->handles->tree, iter) {
		link = avl_tree_entry(iter, handle_link_t);
		kdb_printf("%-5" PRIu64 " %-18p %d(%-18p) %-6d %p\n",
		           iter->key, link->handle->object, link->handle->object->type->id,
		           link->handle->object->type, refcount_get(&link->handle->count),
		           link->handle->data);
	}

	return KDB_SUCCESS;
}

/** Dump an ACL.
 * @param acl		ACL to dump. */
static void dump_object_acl(object_acl_t *acl) {
	size_t i;

	for(i = 0; i < acl->count; i++) {
		switch(acl->entries[i].type) {
		case ACL_ENTRY_USER:
			kdb_printf(" User(%d): ", acl->entries[i].value);
			break;
		case ACL_ENTRY_GROUP:
			kdb_printf(" Group(%d): ", acl->entries[i].value);
			break;
		case ACL_ENTRY_OTHERS:
			kdb_printf(" Others: ");
			break;
		case ACL_ENTRY_SESSION:
			kdb_printf(" Session(%d): ", acl->entries[i].value);
			break;
		case ACL_ENTRY_CAPABILITY:
			kdb_printf(" Capability(%d): ", acl->entries[i].value);
			break;
		}

		kdb_printf("0x%x\n", acl->entries[i].rights);
	}
}

/** Print information about an object.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_object(int argc, char **argv, kdb_filter_t *filter) {
	object_t *object;
	uint64_t addr;

	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s <address>\n\n", argv[0]);

		kdb_printf("Prints out information about an object.\n");
		return KDB_SUCCESS;
	} else if(argc != 2) {
		kdb_printf("Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
		return KDB_FAILURE;
	}

	if(kdb_parse_expression(argv[1], &addr, NULL) != KDB_SUCCESS) {
		return KDB_FAILURE;
	}

	object = (object_t *)((ptr_t)addr);

	kdb_printf("Object %p\n", object);
	kdb_printf("=================================================\n");
	kdb_printf("Type:  %d(%p)\n", object->type->id, object->type);
	kdb_printf("User:  %d\n", object->uid);
	kdb_printf("Group: %d\n\n", object->gid);

	kdb_printf("User ACL:\n");
	dump_object_acl(&object->uacl);
	kdb_printf("System ACL:\n");
	dump_object_acl(&object->sacl);

	return KDB_SUCCESS;
}

/** Initialize the handle caches. */
__init_text void handle_init(void) {
	object_handle_cache = slab_cache_create("object_handle_cache", sizeof(object_handle_t),
	                                        0, NULL, NULL, NULL, 0, MM_BOOT);
	handle_table_cache = slab_cache_create("handle_table_cache", sizeof(handle_table_t),
	                                       0, handle_table_ctor, NULL, NULL, 0,
	                                       MM_BOOT);

	/* Register the KDB commands. */
	kdb_register_command("handles", "Inspect a process' handle table.", kdb_cmd_handles);
	kdb_register_command("object", "Show information about a kernel object.", kdb_cmd_object);
}

/** Get the type of an object referred to by a handle.
 * @param handle	Handle to object.
 * @return		Type ID of object on success, -1 if the handle was not
 *			found. */
int kern_object_type(handle_t handle) {
	object_handle_t *khandle;
	int ret;

	if(object_handle_lookup(handle, -1, 0, &khandle) != STATUS_SUCCESS) {
		return -1;
	}

	ret = khandle->object->type->id;
	object_handle_release(khandle);
	return ret;
}

/** Wait for events to occur on one or more objects..
 * @param events	Array of structures describing events to wait for. Upon
 *			successful return, the signalled field of each
 *			structure will be updated to reflect whether or not the
 *			event was signalled.
 * @param count		Number of array entries.
 * @param timeout	Maximum time to wait in microseconds. A value of 0 will
 *			cause the function to return immediately if the none of
 *			the events have happened, and a value of -1 will block
 *			indefinitely until one of events happens.
 * @return		Status code describing result of the operation. */
status_t kern_object_wait(object_event_t *events, size_t count, useconds_t timeout) {
	semaphore_t sem = SEMAPHORE_INITIALIZER(sem, "object_wait_sem", 0);
	object_wait_sync_t *syncs;
	status_t ret, err;
	size_t i = 0;

	if(!count || count > 1024 || !events) {
		return STATUS_INVALID_ARG;
	}

	/* Use of kcalloc() is important: the structures must be zeroed
	 * initially for the cleanup code to work correctly. */
	syncs = kcalloc(count, sizeof(*syncs), 0);
	if(!syncs) {
		return STATUS_NO_MEMORY;
	}

	for(i = 0; i < count; i++) {
		ret = memcpy_from_user(&syncs[i].info, &events[i], sizeof(*events));
		if(ret != STATUS_SUCCESS) {
			goto out;
		}

		syncs[i].sem = &sem;
		syncs[i].info.signalled = false;

		ret = object_handle_lookup(syncs[i].info.handle, -1, 0, &syncs[i].handle);
		if(ret != STATUS_SUCCESS) {
			goto out;
		} else if(!syncs[i].handle->object->type->wait || !syncs[i].handle->object->type->unwait) {
			ret = STATUS_INVALID_EVENT;
			object_handle_release(syncs[i].handle);
			syncs[i].handle = NULL;
			goto out;
		}

		ret = syncs[i].handle->object->type->wait(syncs[i].handle, syncs[i].info.event, &syncs[i]);
		if(ret != STATUS_SUCCESS) {
			object_handle_release(syncs[i].handle);
			syncs[i].handle = NULL;
			goto out;
		}
	}

	ret = semaphore_down_etc(&sem, timeout, SYNC_INTERRUPTIBLE);
out:
	while(i--) {
		syncs[i].handle->object->type->unwait(syncs[i].handle, syncs[i].info.event, &syncs[i]);
		object_handle_release(syncs[i].handle);

		if(ret == STATUS_SUCCESS) {
			err = memcpy_to_user(&events[i], &syncs[i].info, sizeof(*events));
			if(err != STATUS_SUCCESS) {
				ret = err;
			}
		}
	}

	kfree(syncs);
	return ret;
}

/** Change handle options.
 * @param handle	Handle to operate on.
 * @param action	Action to perform.
 * @param arg		Argument to function.
 * @param outp		Where to store return value from function.
 * @return		Status code describing result of the operation. */
status_t kern_handle_control(handle_t handle, int action, int arg, int *outp) {
	status_t ret = STATUS_SUCCESS;
	object_handle_t *khandle;
	object_rights_t rights;
	handle_link_t *link;
	int kout = 0;

	rwlock_write_lock(&curr_proc->handles->lock);

	/* Look up the handle in the tree. */
	link = avl_tree_lookup(&curr_proc->handles->tree, handle);
	if(!link) {
		rwlock_unlock(&curr_proc->handles->lock);
		return STATUS_INVALID_HANDLE;
	}
	khandle = link->handle;

	/* Perform the action. Implementation of HANDLE_{G,S}ET_FLAGS is left
	 * to the object type, since they operate on flags specific to the
	 * type. */
	switch(action) {
	case HANDLE_GET_LFLAGS:
		/* Get handle table link flags. */
		kout = link->flags;
		break;
	case HANDLE_SET_LFLAGS:
		/* Set handle table link flags. */
		link->flags = arg;
		break;
	case HANDLE_GET_RIGHTS:
		/* Get handle rights. */
		kout = khandle->rights;
		break;
	case HANDLE_SET_RIGHTS:
		/* Set handle rights. */
		rights = (object_rights_t)arg;
		if(rights) {
			if((object_rights(khandle->object, curr_proc) & rights) != rights) {
				ret = STATUS_ACCESS_DENIED;
				break;
			}
		}

		khandle->rights = rights;
		break;
	default:
		if(khandle->object->type->control) {
			ret = khandle->object->type->control(khandle, action, arg, &kout);
		} else {
			ret = STATUS_NOT_SUPPORTED;
		}
		break;
	}

	rwlock_unlock(&curr_proc->handles->lock);
	if(ret == STATUS_SUCCESS && outp) {
		ret = memcpy_to_user(outp, &kout, sizeof(int));
	}
	return ret;
}

/**
 * Duplicate a handle ID.
 *
 * Duplicates an entry in the calling process' handle table. The new handle ID
 * will refer to the same handle as the source ID.
 *
 * @param handle	Handle ID to duplicate.
 * @param dest		Destination handle ID.
 * @param force		If true, and the destination handle ID already refers
 *			to a handle, then that handle will be closed. Otherwise,
 *			the lowest handle available that is higher than the
 *			specified ID will be used.
 * @param newp		Where to store new handle ID.
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_handle_duplicate(handle_t handle, handle_t dest, bool force, handle_t *newp) {
	handle_link_t *link;

	if(handle >= CONFIG_HANDLE_MAX || dest >= CONFIG_HANDLE_MAX || !newp) {
		return STATUS_INVALID_ARG;
	}

	/* FIXME: Laziness! */
	if(!force && dest > 0) {
		return STATUS_NOT_IMPLEMENTED;
	}

	rwlock_write_lock(&curr_proc->handles->lock);

	/* Look up the handle in the tree. */
	link = avl_tree_lookup(&curr_proc->handles->tree, handle);
	if(!link) {
		rwlock_unlock(&curr_proc->handles->lock);
		return STATUS_INVALID_HANDLE;
	}

	/* If forcing, close any existing handle. Otherwise, find a new ID. */
	if(force) {
		object_handle_detach_unsafe(curr_proc, dest);
	} else {
		/* See previous FIXME. */
		dest = bitmap_ffz(&curr_proc->handles->bitmap);
		if(dest < 0) {
			rwlock_unlock(&curr_proc->handles->lock);
			return STATUS_NO_HANDLES;
		}
	}

	/* Insert the new handle. */
	handle_table_insert(curr_proc->handles, dest, link->handle, link->flags);

	dprintf("object: duplicated handle %d to %d in process %" PRId32 " (object: %p, data: %p)\n",
	        handle, dest, curr_proc->id, link->handle->object, link->handle->data);
	rwlock_unlock(&curr_proc->handles->lock);
	return memcpy_to_user(newp, &dest, sizeof(*newp));
}

/** Close a handle.
 * @param handle	Handle ID to close.
 * @return		Status code describing result of the operation.
 *			The only reason for failure is an invalid handle being
 *			specified. */
status_t kern_handle_close(handle_t handle) {
	return object_handle_detach(NULL, handle);
}
