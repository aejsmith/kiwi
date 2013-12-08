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
 */

#ifndef __OBJECT_H
#define __OBJECT_H

#include <kernel/object.h>

#include <lib/list.h>
#include <lib/refcount.h>

#include <sync/rwlock.h>

struct object_handle;
struct vm_region;

/** Kernel object type structure. */
typedef struct object_type {
	unsigned id;			/**< ID number for the type. */
	unsigned flags;			/**< Flags for objects of this type. */

	/** Close a handle to an object.
	 * @param handle	Handle to the object. */
	void (*close)(struct object_handle *handle);

	/** Get the name of an object.
	 * @param handle	Handle to the object.
	 * @return		Pointer to allocated name string. */
	char *(*name)(struct object_handle *handle);

	/** Signal that an object event is being waited for.
	 * @note		If the event being waited for has occurred
	 *			already, this function should call the callback
	 *			function and return success.
	 * @param handle	Handle to object.
	 * @param event		Event that is being waited for.
	 * @param wait		Internal data pointer to be passed to
	 *			object_wait_signal() or object_wait_notifier().
	 * @return		Status code describing result of the operation. */
	status_t (*wait)(struct object_handle *handle, unsigned event, void *wait);

	/** Stop waiting for an object.
	 * @param handle	Handle to object.
	 * @param event		Event that is being waited for.
	 * @param wait		Internal data pointer. */
	void (*unwait)(struct object_handle *handle, unsigned event, void *wait);

	/**
	 * Map an object into memory.
	 *
	 * This function is called when an object is to be mapped into memory.
	 * It should check whether the current thread has permission to perform
	 * the mapping with the protection flags set in the region. It should
	 * then either map the entire region up front, or set the region's
	 * operations structure pointer to allow the region content to be
	 * demand paged.
	 *
	 * @param handle	Handle to object.
	 * @param region	Region being mapped.
	 *
	 * @return		Status code describing result of the operation.
	 */
	status_t (*map)(struct object_handle *handle, struct vm_region *region);
} object_type_t;

/** Properties of an object type. */
#define OBJECT_TRANSFERRABLE	(1<<0)	/**< Objects can be inherited or transferred over IPC. */

/** Structure containing a kernel object handle. */
typedef struct object_handle {
	object_type_t *type;		/**< Type of the object. */
	void *private;			/**< Per-handle data pointer. */
	refcount_t count;		/**< References to the handle. */
} object_handle_t;

/** Table that maps IDs to handles (handle_t -> object_handle_t). */
typedef struct handle_table {
	rwlock_t lock;			/**< Lock to protect table. */
	object_handle_t **handles;	/**< Array of allocated handles. */
	uint32_t *flags;		/**< Array of entry flags. */
	unsigned long *bitmap;		/**< Bitmap for tracking free handle IDs. */
} handle_table_t;

extern void object_wait_notifier(void *arg1, void *arg2, void *arg3);
extern void object_wait_signal(void *wait, unsigned long data);

extern object_handle_t *object_handle_create(object_type_t *type, void *private);
extern void object_handle_retain(object_handle_t *handle);
extern void object_handle_release(object_handle_t *handle);

extern status_t object_handle_lookup(handle_t id, int type,
	object_handle_t **handlep);
extern status_t object_handle_attach(object_handle_t *handle, handle_t *idp,
	handle_t *uidp);
extern status_t object_handle_detach(handle_t id);

extern status_t handle_table_create(handle_table_t *parent, handle_t map[][2],
	ssize_t count, handle_table_t **tablep);
extern handle_table_t *handle_table_clone(handle_table_t *parent);
extern void handle_table_destroy(handle_table_t *table);

extern void object_init(void);

#endif /* __OBJECT_H */
