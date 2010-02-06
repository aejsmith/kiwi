/*
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Radix tree implementation.
 */

#ifndef __TYPES_RADIX_H
#define __TYPES_RADIX_H

#include <types.h>

struct radix_tree_node;

/** Radix tree node pointer structure. */
typedef struct radix_tree_node_ptr {
	/** Array of nodes. */
	struct radix_tree_node *nodes[16];

	/** Count of nodes. */
	size_t count;
} radix_tree_node_ptr_t;

/** Radix tree node structure. */
typedef struct radix_tree_node {
	unsigned char *key;		/**< Key for this node. */
	void *value;			/**< Node value. */
	size_t child_count;		/**< Number of child nodes. */

	/** Pointer to parent node. */
	struct radix_tree_node *parent;

	/** Two-level array of child nodes (each level has 16 entries). */
	radix_tree_node_ptr_t *children[16];
} radix_tree_node_t;

/** Radix tree structure. */
typedef struct radix_tree {
	radix_tree_node_t root;		/**< Root node. */
} radix_tree_t;

/** Iterates over all nodes with non-NULL values in a radix tree, setting iter
 *  to the node on each iteration. */
#define RADIX_TREE_FOREACH(tree, iter)		\
	for(radix_tree_node_t *iter = radix_tree_node_next(&(tree)->root); iter != NULL; \
	    iter = radix_tree_node_next(iter))

/** Gets a radix tree node's data pointer and casts it to a certain type. */
#define radix_tree_entry(node, type)		\
	((node) ? (type *)(node->value) : NULL)

/** Helper for radix_tree_clear() that is called on all non-NULL values. */
typedef void (*radix_tree_clear_helper_t)(void *);

/** Check if a radix tree is empty.
 * @param tree		Tree to check.
 * @return		True if empty, false if not. */
static inline bool radix_tree_empty(radix_tree_t *tree) {
	return (tree->root.child_count == 0);
}

extern void radix_tree_insert(radix_tree_t *tree, const char *key, void *value);
extern void radix_tree_remove(radix_tree_t *tree, const char *key, radix_tree_clear_helper_t helper);
extern void *radix_tree_lookup(radix_tree_t *tree, const char *key);

extern void radix_tree_init(radix_tree_t *tree);
extern void radix_tree_clear(radix_tree_t *tree, radix_tree_clear_helper_t helper);
extern void radix_tree_destroy(radix_tree_t *tree);

extern radix_tree_node_t *radix_tree_node_next(radix_tree_node_t *node);

#endif /* __TYPES_RADIX_H */
