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

#include <lib/avl.h>
#include <lib/bitmap.h>
#include <lib/list.h>
#include <lib/refcount.h>

#include <sync/rwlock.h>

struct object_handle;
struct object_wait;
struct process;
struct vm_page;
struct vm_region;

/** Handle type ID definitions. */
#define OBJECT_TYPE_FILE	1	/**< File. */
#define OBJECT_TYPE_DIR		2	/**< Directory. */
#define OBJECT_TYPE_DEVICE	3	/**< Device. */
#define OBJECT_TYPE_PROCESS	4	/**< Process. */
#define OBJECT_TYPE_THREAD	5	/**< Thread. */
#define OBJECT_TYPE_PORT	6	/**< IPC port. */
#define OBJECT_TYPE_CONNECTION	7	/**< IPC connection. */

/** Kernel object type structure. */
typedef struct object_type {
	int id;				/**< ID number for the type. */

	/** Close a handle to an object.
	 * @param handle	Handle to the object. */
	void (*close)(struct object_handle *handle);

	/** Signal that an object is being waited for.
	 * @note		If the event being waited for has occurred
	 *			already, this function should call the callback
	 *			function and return success.
	 * @param wait		Wait information structure.
	 * @return		0 on success, negative error code on failure. */
	int (*wait)(struct object_wait *wait);

	/** Stop waiting for an object.
	 * @param wait		Wait information structure. */
	void (*unwait)(struct object_wait *wait);

	/** Non-standard fault handling.
	 * @note		If this operation is specified, it is called
	 *			on all faults on memory regions using the
	 *			object rather than using the standard fault
	 *			handling mechanism. In this case, the page_get
	 *			operation is not needed.
	 * @note		When this is called, the main fault handler
	 *			will have verified that the fault is allowed by
	 *			the region's access flags.
	 * @param region	Region fault occurred in.
	 * @param addr		Virtual address of fault (rounded down to base
	 *			of page).
	 * @param reason	Reason for the fault.
	 * @param access	Type of access that caused the fault.
	 * @return		Whether succeeded in handling the fault. */
	bool (*fault)(struct vm_region *region, ptr_t addr, int reason, int access);

	/** Get a page from the object.
	 * @param handle	Handle to object to get page from.
	 * @param offset	Offset to get page from (the offset into the
	 *			region the fault occurred at, plus the offset
	 *			of the region into the object).
	 * @param pagep		Where to store pointer to page structure.
	 * @return		0 on success, negative error code on failure. */
	int (*page_get)(struct object_handle *handle, offset_t offset, struct vm_page **pagep);

	/** Release a page from the object.
	 * @param handle	Handle to object to release page in.
	 * @param offset	Offset of page in object.
	 * @param paddr		Physical address of page that was unmapped. */
	void (*page_release)(struct object_handle *handle, offset_t offset, phys_ptr_t paddr);
} object_type_t;

/** Structure defining a kernel object.
 * @note		This structure is intended to be embedded inside
 *			another structure for the object. */
typedef struct object {
	object_type_t *type;		/**< Type of the object. */
	int flags;			/**< Flags for the object. */
	//rwlock_t lock;		/**< Lock protecting the ACL. */
	//list_t acl;			/**< List of ACL entries. */
} object_t;

/** Structure containing a handle to a kernel object. */
typedef struct object_handle {
	object_t *object;		/**< Object that the handle refers to. */
	void *data;			/**< Per-handle data pointer. */
	refcount_t count;		/**< References to the handle. */
} object_handle_t;

/** Handle waiting information structure. */
typedef struct object_wait {
	object_handle_t *handle;	/**< Handle to object being waited for. */
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

/** Object flags. */
#define OBJECT_MAPPABLE		(1<<0)	/**< Object is mappable (fault or page_get should be implemented). */

extern void object_init(object_t *obj, object_type_t *type, int flags);
extern void object_destroy(object_t *obj);
//extern bool object_acl_check(object_t *obj, process_t *proc, uint32_t rights);
//extern void object_acl_insert(object_t *obj,
//extern void object_acl_remove(object_t *obj,

extern object_handle_t *object_handle_create(object_t *obj, void *data);
extern void object_handle_get(object_handle_t *handle);
extern void object_handle_release(object_handle_t *handle);
extern handle_t object_handle_attach(struct process *process, object_handle_t *handle);
extern int object_handle_detach(struct process *process, handle_t id);
extern int object_handle_lookup(struct process *process, handle_t id, int type, object_handle_t **handlep);

extern void object_wait_notifier(void *arg1, void *arg2, void *arg3);
extern void object_wait_callback(object_wait_t *wait);
extern int object_wait(object_handle_t *handle, int event, useconds_t timeout);
extern int object_wait_multiple(object_handle_t **handles, int *events, size_t count, useconds_t timeout);

extern int handle_table_init(handle_table_t *table, handle_table_t *parent);
extern void handle_table_destroy(handle_table_t *table);

extern void handle_cache_init(void);

extern int sys_object_type(handle_t handle);
extern int sys_object_wait(handle_t handle, int event, useconds_t timeout);
extern int sys_object_wait_multiple(handle_t *handle, int *events, size_t count, useconds_t timeout);
extern int sys_handle_close(handle_t handle);

#endif /* __OBJECT_H */
