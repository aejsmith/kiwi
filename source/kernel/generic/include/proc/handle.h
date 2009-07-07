/* Kiwi per-process object manager
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
 * @brief		Per-process object manager.
 */

#ifndef __PROC_HANDLE_H
#define __PROC_HANDLE_H

#include <sync/mutex.h>

#include <types/avltree.h>
#include <types/bitmap.h>
#include <types/refcount.h>

struct handle_info;
struct process;

/** Structure for storing information about a process' handles. */
typedef struct handle_table {
	avltree_t tree;			/**< Tree of ID to handle structure mappings. */
	bitmap_t bitmap;		/**< Bitmap for tracking free handles. */
	mutex_t lock;			/**< Lock to protect table. */
} handle_table_t;

/** Structure containing certain operations for a handle. */
typedef struct handle_ops {
	/** Close a handle.
	 * @param info		Pointer to handle structure being closed.
	 * @return		0 if handle can be closed, negative error code
	 *			if not. */
	int (*close)(struct handle_info *info);
} handle_ops_t;

/** Structure containing information of a handle. */
typedef struct handle_info {
	handle_ops_t *ops;		/**< Operations structure for the handle. */
	void *data;			/**< Data for the handle. */
	refcount_t count;		/**< Reference count for the handle. */
	mutex_t lock;			/**< Lock to protect the handle. */
} handle_info_t;

extern handle_t handle_create(struct process *process, handle_ops_t *ops, void *data);
extern int handle_close(struct process *process, handle_t handle);

extern int handle_get(struct process *process, handle_t handle, handle_ops_t *ops, handle_info_t **infop);
extern void handle_release(handle_info_t *info);

extern int handle_table_init(handle_table_t *table, handle_table_t *parent);
extern void handle_table_destroy(handle_table_t *table);

extern void handle_init(void);

extern int sys_handle_close(handle_t handle);

#endif /* __PROC_HANDLE_H */
