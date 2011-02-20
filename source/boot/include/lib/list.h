/*
 * Copyright (C) 2008-2011 Alex Smith
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
 * @brief		Linked list implementation.
 */

#ifndef __LIB_LIST_H
#define __LIB_LIST_H

#include <types.h>

/** Structure containing a circular doubly linked list. */
typedef struct list {
	struct list *prev;		/**< Pointer to previous entry. */
	struct list *next;		/**< Pointer to next entry. */
} list_t;

/** Iterates over a list, setting iter to the list entry on each iteration. */
#define LIST_FOREACH(list, iter)		\
	for(list_t *iter = (list)->next; iter != (list); iter = iter->next)

/** Iterates over a list in reverse, setting iter to the list entry on each
 *  iteration. */
#define LIST_FOREACH_R(list, iter)		\
	for(list_t *iter = (list)->prev; iter != (list); iter = iter->prev)

/** Iterates over a list, setting iter to the list entry on each iteration.
 * @note		Safe to use when the loop may modify the list - caches
 *			the next pointer from the entry before the loop body. */
#define LIST_FOREACH_SAFE(list, iter)		\
	for(list_t *iter = (list)->next, *_##iter = iter->next; \
	    iter != (list); iter = _##iter, _##iter = _##iter->next)

/** Iterates over a list in reverse, setting iter to the list entry on each
 *  iteration.
 * @note		Safe to use when the loop may modify the list. */
#define LIST_FOREACH_SAFE_R(list, iter)		\
	for(list_t *iter = (list)->prev, *_##iter = iter->prev; \
	    iter != (list); iter = _##iter, _##iter = _##iter->prev)

/** Initialises a statically declared linked list. */
#define LIST_INITIALISER(_var)			\
	{ \
		.prev = &_var, \
		.next = &_var, \
	}

/** Statically declares a new linked list. */
#define LIST_DECLARE(_var)			\
	list_t _var = LIST_INITIALISER(_var)

/** Gets a pointer to the structure containing a list header, given the
 *  structure type and the member name of the list header. */
#define list_entry(entry, type, member)		\
	(type *)((char *)entry - offsetof(type, member))

/** Checks whether the given list is empty. */
#define list_empty(list)			\
	(((list)->prev == (list)) && ((list)->next) == (list))

/** Internal part of list_remove(). */
static inline void list_real_remove(list_t *entry) {
	entry->prev->next = entry->next;
	entry->next->prev = entry->prev;
}

/** Initialises a linked list.
 * @param list		List to initialise. */
static inline void list_init(list_t *list) {
	list->prev = list->next = list;
}

/** Add an entry to a list before the given entry.
 * @param exist		Existing entry to add before.
 * @param entry		Entry to append. */
static inline void list_add_before(list_t *exist, list_t *entry) {
	list_real_remove(entry);

	exist->prev->next = entry;
	entry->next = exist;
	entry->prev = exist->prev;
	exist->prev = entry;
}

/** Add an entry to a list after the given entry.
 * @param exist		Existing entry to add after.
 * @param entry		Entry to append. */
static inline void list_add_after(list_t *exist, list_t *entry) {
	list_real_remove(entry);

	exist->next->prev = entry;
	entry->next = exist->next;
	entry->prev = exist;
	exist->next = entry;
}

/** Append an entry to a list.
 * @param list		List to append to.
 * @param entry		Entry to append. */
static inline void list_append(list_t *list, list_t *entry) {
	list_add_before(list, entry);
}

/** Prepend an entry to a list.
 * @param list		List to prepend to.
 * @param entry		Entry to prepend. */
static inline void list_prepend(list_t *list, list_t *entry) {
	list_add_after(list, entry);
}

/** Remove a list entry from its containing list.
 * @param entry		Entry to remove. */
static inline void list_remove(list_t *entry) {
	list_real_remove(entry);
	list_init(entry);
}

#endif /* __LIB_LIST_H */
