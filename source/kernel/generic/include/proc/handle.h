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
#include <sync/rwlock.h>

#include <types/avl.h>
#include <types/bitmap.h>
#include <types/refcount.h>

struct handle_info;
struct handle_wait;

/** Structure for storing information about a process' handles. */
typedef struct handle_table {
	avl_tree_t tree;		/**< Tree of ID to handle structure mappings. */
	bitmap_t bitmap;		/**< Bitmap for tracking free handle IDs. */
	mutex_t lock;			/**< Lock to protect table. */
} handle_table_t;

/** Structure defining a handle type. */
typedef struct handle_type {
	int id;				/**< ID of the handle type. */

	/** Signal that a handle is being waited for.
	 * @note		If the event being waited for has occurred
	 *			already, this function should call the callback
	 *			function and return success.
	 * @param wait		Wait information structure.
	 * @return		0 on success, negative error code on failure. */
	int (*wait)(struct handle_wait *wait);

	/** Stop waiting for a handle.
	 * @param wait		Wait information structure. */
	void (*unwait)(struct handle_wait *wait);

	/** Close a handle when all references to it are gone.
	 * @param info		Pointer to handle structure being closed.
	 * @return		0 if handle can be closed, negative error code
	 *			if not. */
	int (*close)(struct handle_info *info);
} handle_type_t;

/** Structure containing information of a handle. */
typedef struct handle_info {
	handle_type_t *type;		/**< Type of the handle. */
	void *data;			/**< Data for the handle. */
	refcount_t count;		/**< Reference count for the handle. */
	rwlock_t lock;			/**< Lock to protect the handle. */
} handle_info_t;

/** Handle waiting information structure. */
typedef struct handle_wait {
	handle_info_t *info;		/**< Handle being waited for. */
	int event;			/**< Event ID being waited for. */
	void *data;			/**< Internal implementation data pointer. */
	int idx;			/**< Index into array for handle_wait_multiple(). */

	/** Callback function for an event occurring.
	 * @param wait		Wait structure. */
	void (*cb)(struct handle_wait *wait);
} handle_wait_t;

/** Handle type ID definitions. */
#define HANDLE_TYPE_FILE	1	/**< File. */
#define HANDLE_TYPE_DIR		2	/**< Directory. */
#define HANDLE_TYPE_PROCESS	3	/**< Process. */
#define HANDLE_TYPE_THREAD	4	/**< Thread. */
#define HANDLE_TYPE_DEVICE	5	/**< Device. */
#define HANDLE_TYPE_IPC		6	/**< IPC connection. */

/** Standard handle wait events. */
#define HANDLE_EVENT_READ	1	/**< Wait for data to be ready to read. */
#define HANDLE_EVENT_WRITE	2	/**< Wait for handle to be able to accept data. */

/** Start of per-handle type event types. */
#define HANDLE_EVENT_TYPE_START	16

extern void handle_wait_notifier(void *arg1, void *arg2, void *arg3);

extern handle_t handle_create(handle_table_t *table, handle_type_t *type, void *data);
extern int handle_get(handle_table_t *table, handle_t handle, int type, handle_info_t **infop);
extern void handle_release(handle_info_t *info);
extern int handle_close(handle_table_t *table, handle_t handle);
extern int handle_wait(handle_table_t *table, handle_t handle, int event, timeout_t timeout);
extern int handle_wait_multiple(handle_table_t *table, handle_t *handles, int *events, size_t count, timeout_t timeout);

extern int handle_table_init(handle_table_t *table, handle_table_t *parent);
extern void handle_table_destroy(handle_table_t *table);

extern int kdbg_cmd_handles(int argc, char **argv);

extern int sys_handle_close(handle_t handle);
extern int sys_handle_type(handle_t handle);
extern int sys_handle_wait(handle_t handle, int event, timeout_t timeout);
extern int sys_handle_wait_multiple(handle_t *handles, int *events, size_t count, timeout_t timeout);

#endif /* __PROC_HANDLE_H */
