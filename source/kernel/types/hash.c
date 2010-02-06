/*
 * Copyright (C) 2008-2009 Alex Smith
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
 * @brief		Hash table implementation.
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <types/hash.h>

#include <assert.h>
#include <errors.h>

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
 *
 * Hash function for a string using the FNV-1 algorithm. See
 * http://www.isthe.com/chongo/tech/comp/fnv/
 *
 * @param key		Pointer to string to hash.
 *
 * @return		Hash generated from string.
 */
uint32_t hash_str_hash(key_t key) {
	register uint32_t hash = FNV_OFFSET_BASIS;
	const char *str = (const char *)((ptr_t)key);

	while(*str) {
		hash = (hash * FNV_PRIME) ^ *str++;
	}

	return hash;
}

/** Comparison function for string keys.
 *
 * Compares the strings pointed to by the given pointers.
 *
 * @param key1		First key to compare.
 * @param key2		Second key to compare.
 *
 * @return		True if the same, false otherwise.
 */
bool hash_str_compare(key_t key1, key_t key2) {
	return (strcmp((const char *)((ptr_t)key1), (const char *)((ptr_t)key2)) == 0) ? true : false;
}

/** Integer hash function.
 *
 * Hash function for an integer using the FNV-1 algorithm. See
 * http://www.isthe.com/chongo/tech/comp/fnv/
 *
 * @param key		Integer to hash.
 *
 * @return		Hash generated from integer.
 */
uint32_t hash_int_hash(key_t key) {
	register uint32_t hash = FNV_OFFSET_BASIS;
	size_t i = 0;

	while(i++ < sizeof(key)) {
		hash = (hash * FNV_PRIME) ^ (key & 0xff);
		key >>= 8;
	}

	return hash;
}

/** Comparison function for integer keys.
 *
 * Compares the given integer hash table keys.
 *
 * @param key1		First key to compare.
 * @param key2		Second key to compare.
 *
 * @return		True if the same, false otherwise.
 */
bool hash_int_compare(key_t key1, key_t key2) {
	return (key1 == key2);
}

/** Insert an entry into a hash table.
 *
 * Inserts the given entry into a hash table.
 *
 * @param hash		Hash table to add to.
 * @param entry		List header for entry to add.
 */
void hash_insert(hash_t *hash, list_t *entry) {
	key_t key = hash->ops->key(entry);

	list_append(&hash->buckets[hash->ops->hash(key) % hash->entries], entry);
}

/** Insert an entry into a hash table.
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
 *
 * Removes the given entry from the hash table it is contained in.
 *
 * @param entry		Entry to remove.
 */
void hash_remove(list_t *entry) {
	list_remove(entry);
}

/** Find an entry in a hash table.
 *
 * Finds an entry with the given key in a hash table.
 *
 * @param hash		Hash table to find in.
 * @param key		Key to find.
 *
 * @return		Pointer to entry's list header if found, NULL if not.
 */
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

/** Initialise a hash table.
 *
 * Initialises a hash table structure and allocates the buckets for it.
 *
 * @param hash		Hash table to initialise.
 * @param entries	Estimated number of entries.
 * @param ops		Hash table operations structure.
 *
 * @return		0 on success, negative error code on failure.
 */
int hash_init(hash_t *hash, size_t entries, hash_ops_t *ops) {
	size_t i;

	if(entries == 0) {
		return -ERR_PARAM_INVAL;
	}

	/* Pick a prime that's at least the estimated number of entries. */
	for(i = 0; i < ARRAYSZ(primes); i++) {
		hash->entries = primes[i];
		if(hash->entries >= entries) {
			break;
		}
	}

	/* Allocate and initialise buckets. */
	hash->buckets = kmalloc(sizeof(list_t) * hash->entries, 0);
	if(hash->buckets == NULL) {
		return -ERR_NO_MEMORY;
	}

	for(i = 0; i < hash->entries; i++) {
		list_init(&hash->buckets[i]);
	}

	hash->ops = ops;
	return 0;
}
