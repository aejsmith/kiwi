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

#include <lib/hash.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <assert.h>
#include <status.h>

/* Credit for primes table: Aaron Krowne
 *  http://br.endernet.org/~akrowne/
 *  http://planetmath.org/encyclopedia/GoodHashTablePrimes.html */
static const uint32_t primes[] = {
	53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593, 49157,
	98317, 196613, 393241, 786433, 1572869, 3145739, 6291469,
	12582917, 25165843, 50331653, 100663319, 201326611, 402653189,
	805306457, 1610612741
};

/** String hash function.
 * @param key		Pointer to string to hash.
 * @return		Hash generated from string. */
uint32_t hash_str_hash(key_t key) {
	return fnv_hash_string((const char *)((ptr_t)key));
}

/** Comparison function for string keys.
 * @param key1		First key to compare.
 * @param key2		Second key to compare.
 * @return		True if the same, false otherwise. */
bool hash_str_compare(key_t key1, key_t key2) {
	return (strcmp((const char *)((ptr_t)key1), (const char *)((ptr_t)key2)) == 0) ? true : false;
}

/** Integer hash function.
 * @param key		Integer to hash.
 * @return		Hash generated from integer. */
uint32_t hash_int_hash(key_t key) {
	return fnv_hash_integer(key);
}

/** Comparison function for integer keys.
 * @param key1		First key to compare.
 * @param key2		Second key to compare.
 * @return		True if the same, false otherwise. */
bool hash_int_compare(key_t key1, key_t key2) {
	return (key1 == key2);
}

/** Insert an entry into a hash table.
 * @param hash		Hash table to add to.
 * @param entry		List header for entry to add. */
void hash_insert(hash_t *hash, list_t *entry) {
	key_t key = hash->ops->key(entry);
	list_append(&hash->buckets[hash->ops->hash(key) % hash->entries], entry);
}

/**
 * Insert an entry into a hash table.
 *
 * Inserts the given entry into a hash table, ensuring that no other entry
 * exists with the same key.
 *
 * @param hash		Hash table to add to.
 * @param entry		List header for entry to add.
 *
 * @return		True if added, false if not unique.
 */
bool hash_insert_unique(hash_t *hash, list_t *entry) {
	key_t key = hash->ops->key(entry);

	if(hash_lookup(hash, hash->ops->key(entry)) != NULL) {
		return false;
	}

	list_append(&hash->buckets[hash->ops->hash(key) % hash->entries], entry);
	return true;
}

/** Remove an entry from a hash table.
 * @param entry		Entry to remove. */
void hash_remove(list_t *entry) {
	list_remove(entry);
}

/** Find an entry in a hash table.
 * @param hash		Hash table to find in.
 * @param key		Key to find.
 * @return		Pointer to entry's list header if found, NULL if not. */
list_t *hash_lookup(hash_t *hash, key_t key) {
	list_t *bucket;

	bucket = &hash->buckets[hash->ops->hash(key) % hash->entries];
	LIST_FOREACH(bucket, iter) {
		if(hash->ops->compare(key, hash->ops->key(iter))) {
			return iter;
		}
	}

	return NULL;
}

/** Initialize a hash table.
 * @param hash		Hash table to initialize.
 * @param entries	Estimated number of entries.
 * @param ops		Hash table operations structure.
 * @param mmflag	Allocation flags.
 * @return		Status code describing result of the operation. */
status_t hash_init(hash_t *hash, size_t entries, hash_ops_t *ops, int mmflag) {
	size_t i;

	assert(entries);

	/* Pick a prime that's at least the estimated number of entries. */
	for(i = 0; i < ARRAYSZ(primes); i++) {
		hash->entries = primes[i];
		if(hash->entries >= entries) {
			break;
		}
	}

	/* Allocate and initialize buckets. */
	hash->buckets = kmalloc(sizeof(list_t) * hash->entries, mmflag);
	if(hash->buckets == NULL) {
		return STATUS_NO_MEMORY;
	}

	for(i = 0; i < hash->entries; i++) {
		list_init(&hash->buckets[i]);
	}

	hash->ops = ops;
	return STATUS_SUCCESS;
}
