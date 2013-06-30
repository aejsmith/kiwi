/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Directory entry cache functions.
 */

#ifndef __IO_ENTRY_CACHE_H
#define __IO_ENTRY_CACHE_H

#include <lib/radix_tree.h>
#include <sync/mutex.h>

struct entry_cache;

/** Operations for an entry cache. */
typedef struct entry_cache_ops {
	/** Look up an entry.
	 * @param cache		Cache to look up for.
	 * @param name		Name of entry to look up.
	 * @param idp		Where to store ID of node entry maps to.
	 * @return		Status code describing result of the operation. */
	status_t (*lookup)(struct entry_cache *cache, const char *name, node_id_t *idp);
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

extern status_t entry_cache_lookup(entry_cache_t *cache, const char *name, node_id_t *idp);
extern void entry_cache_insert(entry_cache_t *cache, const char *name, node_id_t id);
extern void entry_cache_remove(entry_cache_t *cache, const char *name);

#endif /* __IO_ENTRY_CACHE_H */
