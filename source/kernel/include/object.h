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
 * @brief		Kernel object manager.
 */

#ifndef __OBJECT_H
#define __OBJECT_H

#include <lib/avl_tree.h>
#include <lib/bitmap.h>
#include <lib/list.h>
#include <lib/refcount.h>

#include <public/object.h>

#include <sync/rwlock.h>

struct khandle;
struct object_wait;
struct process;
struct vm_page;
struct vm_region;

/** Kernel object type structure. */
typedef struct object_type {
	int id;				/**< ID number for the type. */

	/** Close a handle to an object.
	 * @param handle	Handle to the object. */
	void (*close)(struct khandle *handle);

	/** Signal that an object is being waited for.
	 * @note		If the event being waited for has occurred
	 *			already, this function should call the callback
	 *			function and return success.
	 * @param wait		Wait information structure.
	 * @return		Status code describing result of the operation. */
	status_t (*wait)(struct object_wait *wait);

	/** Stop waiting for an object.
	 * @param wait		Wait information structure. */
	void (*unwait)(struct object_wait *wait);

	/** Check if an object can be memory-mapped.
	 * @note		If this function is implemented, the get_page
	 *			operation MUST be implemented. If it is not,
	 *			then the object will be classed as mappable if
	 *			get_page is implemented.
	 * @param handle	Handle to object.
	 * @param flags		Mapping flags (VM_MAP_*).
	 * @return		STATUS_SUCCESS if can be mapped, or a status
	 *			code explaining why it cannot be mapped. */
	status_t (*mappable)(struct khandle *handle, int flags);

	/** Get a page from the object.
	 * @param handle	Handle to object to get page from.
	 * @param offset	Offset into object to get page from.
	 * @param physp		Where to store physical address of page.
	 * @return		Status code describing result of the operation. */
	status_t (*get_page)(struct khandle *handle, offset_t offset, phys_ptr_t *physp);

	/** Release a page from the object.
	 * @param handle	Handle to object to release page in.
	 * @param offset	Offset of page in object.
	 * @param phys		Physical address of page that was unmapped. */
	void (*release_page)(struct khandle *handle, offset_t offset, phys_ptr_t phys);
} object_type_t;

/** Structure defining a kernel object.
 * @note		This structure is intended to be embedded inside
 *			another structure for the object. */
typedef struct object {
	object_type_t *type;		/**< Type of the object. */
	//rwlock_t lock;		/**< Lock protecting the ACL. */
	//list_t acl;			/**< List of ACL entries. */
} object_t;

/** Structure containing a handle to a kernel object. */
typedef struct khandle {
	object_t *object;		/**< Object that the handle refers to. */
	void *data;			/**< Per-handle data pointer. */
	refcount_t count;		/**< References to the handle. */
} khandle_t;

/** Handle waiting information structure. */
typedef struct object_wait {
	khandle_t *handle;		/**< Handle to object being waited for. */
	int event;			/**< Event ID being waited for. */
	void *priv;			/**< Internal implementation data pointer. */
	int idx;			/**< Index into array for object_wait_multiple(). */
} object_wait_t;

/** Table that maps IDs to handles. */
typedef struct handle_table {
	rwlock_t lock;			/**< Lock to protect table. */
	avl_tree_t tree;		/**< Tree of ID to handle structure mappings. */
	bitmap_t bitmap;		/**< Bitmap for tracking free handle IDs. */
} handle_table_t;

extern void object_init(object_t *obj, object_type_t *type);
extern void object_destroy(object_t *obj);
//extern bool object_acl_check(object_t *obj, process_t *proc, uint32_t rights);
//extern void object_acl_insert(object_t *obj,
//extern void object_acl_remove(object_t *obj,

extern void object_wait_notifier(void *arg1, void *arg2, void *arg3);
extern void object_wait_callback(object_wait_t *wait);
extern status_t object_wait(khandle_t *handle, int event, useconds_t timeout);
extern status_t object_wait_multiple(khandle_t **handles, int *events, size_t count,
                                     useconds_t timeout, int *indexp);

extern khandle_t *handle_create(object_t *obj, void *data);
extern status_t handle_create_and_attach(object_t *obj, void *data, struct process *process,
                                         int flags, handle_t *handlep);
extern void handle_get(khandle_t *handle);
extern void handle_release(khandle_t *handle);
extern status_t handle_attach(struct process *process, khandle_t *handle, int flags, handle_t *handlep);
extern status_t handle_detach(struct process *process, handle_t id);
extern status_t handle_lookup(struct process *process, handle_t id, int type, khandle_t **handlep);

extern status_t handle_table_create(handle_table_t *parent, handle_t map[][2], int count,
                                    handle_table_t **tablep);
extern void handle_table_destroy(handle_table_t *table);

extern int kdbg_cmd_handles(int argc, char **argv);

extern void handle_init(void);

#endif /* __OBJECT_H */
