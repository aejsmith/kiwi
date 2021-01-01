/*
 * Copyright (C) 2009-2021 Alex Smith
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
 * @brief               Circular doubly-linked list implementation.
 */

#pragma once

#include <system/defs.h>

#include <stdbool.h>
#define __need_offsetof
#include <stddef.h>

__SYS_EXTERN_C_BEGIN

/** Doubly linked list node structure. */
typedef struct core_list {
    struct core_list *prev;         /**< Pointer to previous entry. */
    struct core_list *next;         /**< Pointer to next entry. */
} core_list_t;

/** Iterate over a list.
 * @param list          Head of list to iterate.
 * @param iter          Variable name to set to node pointer on each iteration. */
#define core_list_foreach(list, iter) \
    for (core_list_t *iter = (list)->next; iter != (list); iter = iter->next)

/** Iterate over a list in reverse.
 * @param list          Head of list to iterate.
 * @param iter          Variable name to set to node pointer on each iteration. */
#define core_list_foreach_reverse(list, iter) \
    for (core_list_t *iter = (list)->prev; iter != (list); iter = iter->prev)

/** Iterate over a list safely.
 * @note                Safe to use when the loop may modify the list - caches
 *                      the next pointer from the entry before the loop body.
 * @param list          Head of list to iterate.
 * @param iter          Variable name to set to node pointer on each iteration. */
#define core_list_foreach_safe(list, iter) \
    for ( \
        core_list_t *iter = (list)->next, *_##iter = iter->next; \
        iter != (list); \
        iter = _##iter, _##iter = _##iter->next)

/** Iterate over a list in reverse.
 * @note                Safe to use when the loop may modify the list.
 * @param list          Head of list to iterate.
 * @param iter          Variable name to set to node pointer on each iteration. */
#define core_list_foreach_reverse_safe(list, iter) \
    for ( \
        core_list_t *iter = (list)->prev, *_##iter = iter->prev; \
        iter != (list); \
        iter = _##iter, _##iter = _##iter->prev)

/** Initializes a statically defined linked list. */
#define CORE_LIST_INITIALIZER(_var) \
    { \
        .prev = &_var, \
        .next = &_var, \
    }

/** Statically defines a new linked list. */
#define CORE_LIST_DEFINE(_var) \
    core_list_t _var = CORE_LIST_INITIALIZER(_var)

/** Get a pointer to the structure containing a list node.
 * @param entry         List node pointer.
 * @param type          Type of the structure.
 * @param member        Name of the list node member in the structure.
 * @return              Pointer to the structure. */
#define core_list_entry(entry, type, member) \
    ((type *)((char *)entry - offsetof(type, member)))

/** Get a pointer to the next structure in a list.
 * @note                Does not check if the next entry is the head.
 * @param entry         Current entry.
 * @param member        Name of the list node member in the structure.
 * @return              Pointer to the next structure. */
#define core_list_next(entry, member) \
    (core_list_entry((entry)->member.next, __typeof__(*(entry)), member))

/** Get a pointer to the previous structure in a list.
 * @note                Does not check if the previous entry is the head.
 * @param entry         Current entry.
 * @param member        Name of the list node member in the structure.
 * @return              Pointer to the previous structure. */
#define core_list_prev(entry, member) \
    (core_list_entry((entry)->member.prev, __typeof__(*(entry)), member))

/** Get a pointer to the first structure in a list.
 * @note                Does not check if the list is empty.
 * @param list          Head of the list.
 * @param type          Type of the structure.
 * @param member        Name of the list node member in the structure.
 * @return              Pointer to the first structure. */
#define core_list_first(list, type, member) \
    (core_list_entry((list)->next, type, member))

/** Get a pointer to the last structure in a list.
 * @note                Does not check if the list is empty.
 * @param list          Head of the list.
 * @param type          Type of the structure.
 * @param member        Name of the list node member in the structure.
 * @return              Pointer to the last structure. */
#define core_list_last(list, type, member) \
    (core_list_entry((list)->prev, type, member))

/** Checks whether the given list is empty.
 * @param list          List to check. */
static inline bool core_list_empty(const core_list_t *list) {
    return (list->prev == list && list->next == list);
}

/** Check if a list has only a single entry.
 * @param list          List to check. */
static inline bool core_list_is_singular(const core_list_t *list) {
    return (!core_list_empty(list) && list->next == list->prev);
}

/** Internal part of core_list_remove(). */
static inline void core_list_real_remove(core_list_t *entry) {
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;
}

/** Initializes a linked list.
 * @param list          List to initialize. */
static inline void core_list_init(core_list_t *list) {
    list->prev = list->next = list;
}

/** Add an entry to a list before the given entry.
 * @param exist         Existing entry to add before.
 * @param entry         Entry to append. */
static inline void core_list_add_before(core_list_t *exist, core_list_t *entry) {
    core_list_real_remove(entry);

    exist->prev->next = entry;
    entry->next = exist;
    entry->prev = exist->prev;
    exist->prev = entry;
}

/** Add an entry to a list after the given entry.
 * @param exist         Existing entry to add after.
 * @param entry         Entry to append. */
static inline void core_list_add_after(core_list_t *exist, core_list_t *entry) {
    core_list_real_remove(entry);

    exist->next->prev = entry;
    entry->next = exist->next;
    entry->prev = exist;
    exist->next = entry;
}

/** Append an entry to a list.
 * @param list          List to append to.
 * @param entry         Entry to append. */
static inline void core_list_append(core_list_t *list, core_list_t *entry) {
    core_list_add_before(list, entry);
}

/** Prepend an entry to a list.
 * @param list          List to prepend to.
 * @param entry         Entry to prepend. */
static inline void core_list_prepend(core_list_t *list, core_list_t *entry) {
    core_list_add_after(list, entry);
}

/** Remove a list entry from its containing list.
 * @param entry         Entry to remove. */
static inline void core_list_remove(core_list_t *entry) {
    core_list_real_remove(entry);
    core_list_init(entry);
}

/** Splice the contents of one list onto another.
 * @param position      Entry to insert before.
 * @param list          Head of list to insert. Will become empty after the
 *                      operation. */
static inline void core_list_splice_before(core_list_t *position, core_list_t *list) {
    if (!core_list_empty(list)) {
        list->next->prev = position->prev;
        position->prev->next = list->next;
        position->prev = list->prev;
        list->prev->next = position;

        core_list_init(list);
    }
}

/** Splice the contents of one list onto another.
 * @param position      Entry to insert after.
 * @param list          Head of list to insert. Will become empty after the
 *                      operation. */
static inline void core_list_splice_after(core_list_t *position, core_list_t *list) {
    if (!core_list_empty(list)) {
        list->prev->next = position->next;
        position->next->prev = list->prev;
        position->next = list->next;
        list->next->prev = position;

        core_list_init(list);
    }
}

__SYS_EXTERN_C_END
