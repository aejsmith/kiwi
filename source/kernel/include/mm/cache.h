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
 * @brief		Page-based data cache.
 */

#ifndef __MM_CACHE_H
#define __MM_CACHE_H

#include <lib/avl.h>
#include <mm/page.h>
#include <sync/mutex.h>

struct vm_cache;
struct vm_page;

/** Structure containing operations for a page cache. */
typedef struct vm_cache_ops {
	/** Read a page of data from the source.
	 * @note		If not provided, pages that need to be
	 *			allocated will be zero-filled.
	 * @param cache		Cache being read from.
	 * @param buf		Buffer to read into.
	 * @param offset	Offset to read from.
	 * @param nonblock	Whether the operation is required to not block.
	 * @return		0 on success, negative error code on failure. */
	int (*read_page)(struct vm_cache *cache, void *buf, offset_t offset, bool nonblock);

	/** Write a page of data to the source.
	 * @note		If not provided, pages in the cache will never
	 *			be marked as modified.
	 * @param cache		Cache to write to.
	 * @param buf		Buffer containing data to write.
	 * @param offset	Offset to write from.
	 * @param nonblock	Whether the operation is required to not block.
	 * @return		0 on success, negative error code on failure. */
	int (*write_page)(struct vm_cache *cache, const void *buf, offset_t offset, bool nonblock);

	/** Determine whether a page can be evicted.
	 * @note		If not provided, then behaviour will be as
	 *			though the function returns false.
	 * @param cache		Cache the page belongs to.
	 * @param page	 	Page to check.
	 * @return		Whether the page can be evicted. */
	bool (*evict_page)(struct vm_cache *cache, struct vm_page *page);
} vm_cache_ops_t;

/** Structure containing a page-based data cache. */
typedef struct vm_cache {
	mutex_t lock;			/**< Lock protecting cache. */
	avl_tree_t pages;		/**< Tree of pages. */
	offset_t size;			/**< Size of the cache. */
	vm_cache_ops_t *ops;		/**< Pointer to operations structure. */
	void *data;			/**< Cache data pointer. */
	bool deleted;			/**< Whether the cache is destroyed. */
} vm_cache_t;

extern vm_cache_t *vm_cache_create(offset_t size, vm_cache_ops_t *ops, void *data);
extern int vm_cache_read(vm_cache_t *cache, void *buf, size_t count, offset_t offset,
                         bool nonblock, size_t *bytesp);
extern int vm_cache_write(vm_cache_t *cache, const void *buf, size_t count, offset_t offset,
                          bool nonblock, size_t *bytesp);
extern int vm_cache_get_page(vm_cache_t *cache, offset_t offset, phys_ptr_t *physp);
extern void vm_cache_release_page(vm_cache_t *cache, offset_t offset, phys_ptr_t phys);
extern void vm_cache_resize(vm_cache_t *cache, offset_t size);
extern int vm_cache_flush(vm_cache_t *cache);
extern int vm_cache_destroy(vm_cache_t *cache, bool discard);

extern void vm_cache_flush_page(vm_page_t *page);
extern void vm_cache_evict_page(vm_page_t *page);

extern int kdbg_cmd_cache(int argc, char **argv);

extern void vm_cache_init(void);

#endif /* __MM_CACHE_H */
