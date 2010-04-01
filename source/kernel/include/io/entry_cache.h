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
 * @brief		Directory entry cache functions.
 */

#ifndef __IO_ENTRY_CACHE_H
#define __IO_ENTRY_CACHE_H

#include <lib/radix.h>
#include <sync/mutex.h>

struct entry_cache;

/** Operations for an entry cache. */
typedef struct entry_cache_ops {
	/** Look up an entry.
	 * @param cache		Cache to look up for.
	 * @param name		Name of entry to look up.
	 * @param idp		Where to store ID of node entry maps to.
	 * @return		0 on success, negative error code on failure. */
	int (*lookup)(struct entry_cache *cache, const char *name, node_id_t *idp);
} entry_cache_ops_t;

/** Structure containing a directory entry cache. */
typedef struct entry_cache {
	mutex_t lock;			/**< Lock to protect the cache. */
	radix_tree_t entries;		/**< Tree of name to entry mappings. */
	entry_cache_ops_t *ops;		/**< Operations structure for the cache. */
	void *data;			/**< Implementation-specific data pointer. */
} entry_cache_t;

extern entry_cache_t *entry_cache_create(entry_cache_ops_t *ops, void *data);
extern void entry_cache_destroy(entry_cache_t *cache);

extern int entry_cache_lookup(entry_cache_t *cache, const char *name, node_id_t *idp);
extern void entry_cache_insert(entry_cache_t *cache, const char *name, node_id_t id);
extern void entry_cache_remove(entry_cache_t *cache, const char *name);

#endif /* __IO_ENTRY_CACHE_H */
