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

#include <kernel/object.h>
#include <kernel/security.h>

#include <lib/avl_tree.h>
#include <lib/bitmap.h>
#include <lib/list.h>
#include <lib/refcount.h>

#include <sync/rwlock.h>

struct object;
struct object_handle;
struct object_wait;
struct process;
struct vm_page;
struct vm_region;

/** Kernel object type structure. */
typedef struct object_type {
	int id;				/**< ID number for the type. */

	/** Set new security attributes.
	 * @note		This function can be used to validate security
	 *			attributes, and to write changes to permanent
	 *			storage (i.e. filesystem). If it is not provided,
	 *			all changes will be allowed as long as they are
	 *			permitted by the current ACL.
	 * @note		It is checked whether the process has the
	 *			necessary rights for the changes before calling
	 *			this function.
	 * @param object	Object to set for.
	 * @param security	Security attributes to set. The ACL (if any)
	 *			will be in canonical form.
	 * @return		Status code describing result of the operation. */
	status_t (*set_security)(struct object *object, object_security_t *security);

	/** Close a handle to an object.
	 * @param handle	Handle to the object. */
	void (*close)(struct object_handle *handle);

	/** Signal that an object event is being waited for.
	 * @note		If the event being waited for has occurred
	 *			already, this function should call the callback
	 *			function and return success.
	 * @param handle	Handle to object.
	 * @param event		Event that is being waited for.
	 * @param sync		Internal data pointer to be passed to
	 *			object_wait_signal() or object_wait_notifier().
	 * @return		Status code describing result of the operation. */
	status_t (*wait)(struct object_handle *handle, int event, void *sync);

	/** Stop waiting for an object.
	 * @param handle	Handle to object.
	 * @param event		Event that is being waited for.
	 * @param sync		Internal data pointer. */
	void (*unwait)(struct object_handle *handle, int event, void *sync);

	/** Check if an object can be memory-mapped.
	 * @note		If this function is implemented, the get_page
	 *			operation MUST be implemented. If it is not,
	 *			then the object will be classed as mappable if
	 *			get_page is implemented.
	 * @param handle	Handle to object.
	 * @param flags		Mapping flags (VM_MAP_*).
	 * @return		STATUS_SUCCESS if can be mapped, status code
	 *			explaining why if not. */
	status_t (*mappable)(struct object_handle *handle, int flags);

	/** Get a page from the object.
	 * @param handle	Handle to object to get page from.
	 * @param offset	Offset into object to get page from.
	 * @param physp		Where to store physical address of page.
	 * @return		Status code describing result of the operation. */
	status_t (*get_page)(struct object_handle *handle, offset_t offset, phys_ptr_t *physp);

	/** Release a page from the object.
	 * @param handle	Handle to object to release page in.
	 * @param offset	Offset of page in object.
	 * @param phys		Physical address of page that was unmapped. */
	void (*release_page)(struct object_handle *handle, offset_t offset, phys_ptr_t phys);
} object_type_t;

/** Structure defining a kernel object.
 * @note		This structure is intended to be embedded inside
 *			another structure for the object. */
typedef struct object {
	object_type_t *type;		/**< Type of the object. */
	rwlock_t lock;			/**< Lock protecting the security attributes. */
	user_id_t uid;			/**< Owning user ID. */
	group_id_t gid;			/**< Owning group ID. */
	object_acl_t uacl;		/**< User-configurable ACL. */
	object_acl_t sacl;		/**< System ACL. */
} object_t;

/** Structure containing a handle to a kernel object. */
typedef struct object_handle {
	object_t *object;		/**< Object that the handle refers to. */
	void *data;			/**< Per-handle data pointer. */
	object_rights_t rights;		/**< Rights for the handle. */
	refcount_t count;		/**< References to the handle. */
} object_handle_t;

/** Table that maps IDs to handles (handle_t -> object_handle_t). */
typedef struct handle_table {
	rwlock_t lock;			/**< Lock to protect table. */
	avl_tree_t tree;		/**< Tree of ID to handle structure mappings. */
	bitmap_t bitmap;		/**< Bitmap for tracking free handle IDs. */
} handle_table_t;

extern void object_acl_init(object_acl_t *acl);
extern void object_acl_destroy(object_acl_t *acl);
extern void object_acl_add_entry(object_acl_t *acl, uint8_t type, int32_t value, object_rights_t rights);
extern void object_acl_canonicalise(object_acl_t *acl);

extern status_t object_security_validate(object_security_t *security, struct process *process);
extern status_t object_security_from_user(object_security_t *dest, const object_security_t *src,
                                          bool validate);
extern void object_security_destroy(object_security_t *security);

extern void object_init(object_t *object, object_type_t *type, object_security_t *security,
                        object_acl_t *sacl);
extern void object_destroy(object_t *object);
extern object_rights_t object_rights(object_t *object, struct process *process);
extern status_t object_set_security(object_t *object, object_handle_t *handle,
                                    object_security_t *security);

extern void object_wait_notifier(void *arg1, void *arg2, void *arg3);
extern void object_wait_signal(void *sync);

/** Check if a handle has a set of rights.
 * @param handle	Handle to check.
 * @param rights	Rights to check for.
 * @return		Whether the handle has the rights. */
static inline bool object_handle_rights(object_handle_t *handle, object_rights_t rights) {
	return ((handle->rights & rights) == rights);
}

extern status_t object_handle_create(object_t *object, void *data, object_rights_t rights,
                                     struct process *process, int flags,
                                     object_handle_t **handlep, handle_t *idp,
                                     handle_t *uidp);
extern void object_handle_get(object_handle_t *handle);
extern void object_handle_release(object_handle_t *handle);
extern status_t object_handle_attach(object_handle_t *handle, struct process *process,
                                     int flags, handle_t *idp, handle_t *uidp);
extern status_t object_handle_detach(struct process *process, handle_t id);
extern status_t object_handle_lookup(struct process *process, handle_t id, int type,
                                     object_rights_t rights, object_handle_t **handlep);

extern status_t handle_table_create(handle_table_t *parent, handle_t map[][2], int count,
                                    handle_table_t **tablep);
extern handle_table_t *handle_table_clone(handle_table_t *src);
extern void handle_table_destroy(handle_table_t *table);

extern int kdbg_cmd_handles(int argc, char **argv);
extern int kdbg_cmd_object(int argc, char **argv);

extern void handle_init(void);

#endif /* __OBJECT_H */
