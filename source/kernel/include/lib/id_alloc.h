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
 * @brief		ID allocator.
 */

#ifndef __LIB_ID_ALLOC_H
#define __LIB_ID_ALLOC_H

#include <lib/bitmap.h>
#include <sync/mutex.h>

/** ID allocator structure. */
typedef struct id_alloc {
	mutex_t lock;			/**< Lock to protect the allocator. */
	bitmap_t bitmap;		/**< Bitmap of IDs. */
} id_alloc_t;

extern int32_t id_alloc_get(id_alloc_t *alloc);
extern void id_alloc_release(id_alloc_t *alloc, int32_t id);
extern void id_alloc_reserve(id_alloc_t *alloc, int32_t id);
extern void id_alloc_init(id_alloc_t *alloc, int32_t max);

#endif /* __LIB_ID_ALLOC_H */
