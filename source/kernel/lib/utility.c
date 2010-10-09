/*
 * Copyright (C) 2007-2010 Alex Smith
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
 * @brief		Utility functions.
 *
 * Reference:
 *  - Fowler/Noll/Vo (FNV) Hash
 *    http://www.isthe.com/chongo/tech/comp/fnv/
 */

#include <lib/utility.h>

/** 32-bit FNV_prime. */
#define FNV_PRIME		16777619UL

/** Defined in the FNV description. Result of hashing a known string with the
 *  FNV-0 algorithm and the above prime. */
#define FNV_OFFSET_BASIS	2166136261UL

/** Compute the FNV-1 hash of a string.
 * @param str		String to hash.
 * @return		Generated hash. */
uint32_t fnv_hash_string(const char *str) {
	register uint32_t hash = FNV_OFFSET_BASIS;

	while(*str) {
		hash = (hash * FNV_PRIME) ^ *str++;
	}

	return hash;
}

/** Compute the FNV-1 hash of an integer.
 * @param val		Value to hash.
 * @return		Generated hash. */
uint32_t fnv_hash_integer(uint64_t val) {
	register uint32_t hash = FNV_OFFSET_BASIS;
	size_t i = 0;

	while(i++ < sizeof(val)) {
		hash = (hash * FNV_PRIME) ^ (val & 0xff);
		val >>= 8;
	}

	return hash;
}
