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

	/** Signal that an object event is being waited for.
	 * @note		If the event being waited for has occurred
	 *			already, this function should call the callback
	 *			function and return success.
	 * @param handle	Handle to object.
	 * @param event		Event that is being waited for.
	 * @param sync		Internal data pointer to be passed to
	 *			object_wait_signal() or object_wait_notifier().
	 * @return		Status code describing result of the operation. */
	status_t (*wait)(struct khandle *handle, int event, void *sync);

	/** Stop waiting for an object.
	 * @param handle	Handle to object.
	 * @param event		Event that is being waited for.
	 * @param sync		Internal data pointer. */
	void (*unwait)(struct khandle *handle, int event, void *sync);

	/** Check if an object can be memory-mapped.
	 * @note		If this function is implemented, the get_page
	 *			operation MUST be implemented. If it is not,
	 *			then the object will be classed as mappable if
	 *			get_page is implemented.
	 * @param handle	Handle to object.
	 * @param flags		Mapping flags (VM_MAP_*).
	 * @return		STATUS_SUCCESS if can be mapped, status code
	 *			explaining why if not. */
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
	rwlock_t lock;			/**< Lock protecting the security attributes. */
	user_id_t owner;		/**< Owning user ID. */
	object_acl_t uacl;		/**< User-configurable ACL. */
	object_acl_t sacl;		/**< System ACL. */
} object_t;

/** Structure containing a handle to a kernel object. */
typedef struct khandle {
	object_t *object;		/**< Object that the handle refers to. */
	void *data;			/**< Per-handle data pointer. */
	uint32_t rights;		/**< Rights for the handle. */
	refcount_t count;		/**< References to the handle. */
} khandle_t;

/** Table that maps IDs to handles. */
typedef struct handle_table {
	rwlock_t lock;			/**< Lock to protect table. */
	avl_tree_t tree;		/**< Tree of ID to handle structure mappings. */
	bitmap_t bitmap;		/**< Bitmap for tracking free handle IDs. */
} handle_table_t;

/** Check if a handle has a set of rights.
 * @param handle	Handle to check.
 * @param rights	Rights to check for.
 * @return		Whether the handle has the rights. */
static inline bool handle_rights(khandle_t *handle, uint32_t rights) {
	return ((handle->rights & rights) == rights);
}

extern void object_acl_init(object_acl_t *acl);
extern void object_acl_destroy(object_acl_t *acl);

extern void object_init(object_t *obj, object_type_t *type);
extern void object_destroy(object_t *obj);

extern void object_wait_notifier(void *arg1, void *arg2, void *arg3);
extern void object_wait_signal(void *sync);

extern khandle_t *handle_create(object_t *obj, void *data);
extern status_t handle_create_and_attach(struct process *process, object_t *obj, void *data,
                                         int flags, handle_t *idp, handle_t *uidp);
extern void handle_get(khandle_t *handle);
extern void handle_release(khandle_t *handle);
extern status_t handle_attach(struct process *process, khandle_t *handle, int flags,
                              handle_t *idp, handle_t *uidp);
extern status_t handle_detach(struct process *process, handle_t id);
extern status_t handle_lookup(struct process *process, handle_t id, int type, khandle_t **handlep);

extern status_t handle_table_create(handle_table_t *parent, handle_t map[][2], int count,
                                    handle_table_t **tablep);
extern handle_table_t *handle_table_clone(handle_table_t *src);
extern void handle_table_destroy(handle_table_t *table);

extern int kdbg_cmd_handles(int argc, char **argv);

extern void handle_init(void);

#endif /* __OBJECT_H */
