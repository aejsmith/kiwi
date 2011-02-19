/*
 * Copyright (C) 2008-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Hash table implementation.
 */

#ifndef __LIB_HASH_H
#define __LIB_HASH_H

#include <lib/list.h>

/** Iterates over a hash table. */
#define HASH_FOREACH(hash, iter) \
	for(size_t _##iter##_i = 0; _##iter##_i < (hash)->entries; _##iter##_i++) \
		LIST_FOREACH(&(hash)->buckets[_##iter##_i], iter)

/** Iterates over a hash table, safe when modifying the table in the loop. */
#define HASH_FOREACH_SAFE(hash, iter) \
	for(size_t _##iter##_i = 0; _##iter##_i < (hash)->entries; _##iter##_i++) \
		LIST_FOREACH_SAFE(&(hash)->buckets[_##iter##_i], iter)

/** Hash table operations structure. */
typedef struct hash_ops {
	/** Obtains a key for a given entry.
	 * @param entry		Entry to get key of.
	 * @return		Key of entry. */
	key_t (*key)(list_t *entry);

	/** Hashes the given key.
	 * @param key		Key to hash.
	 * @return		Generated hash for key. */
	uint32_t (*hash)(key_t key);

	/** Compares two keys.
	 * @param key1		First key.
	 * @param key2		Second key.
	 * @return		True if match, false if not. */
	bool (*compare)(key_t key1, key_t key2);
} hash_ops_t;

/** Structure containing a hash table. */
typedef struct hash {
	list_t *buckets;		/**< Buckets for the table. */
	size_t entries;			/**< Number of buckets. */
	hash_ops_t *ops;		/**< Hash table operations. */
} hash_t;

/** Generic hash/comparision functions. */
extern uint32_t hash_str_hash(key_t key);
extern bool hash_str_compare(key_t key1, key_t key2);
extern uint32_t hash_int_hash(key_t key);
extern bool hash_int_compare(key_t key1, key_t key2);

/** Main functions. */
extern void hash_insert(hash_t *hash, list_t *entry);
extern bool hash_insert_unique(hash_t *hash, list_t *entry);
extern void hash_remove(list_t *entry);
extern list_t *hash_lookup(hash_t *hash, key_t key);
extern status_t hash_init(hash_t *hash, size_t entries, hash_ops_t *ops, int mmflag);

#endif /* __LIB_HASH_H */
