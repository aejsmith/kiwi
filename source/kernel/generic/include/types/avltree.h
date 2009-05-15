/* Kiwi AVL tree implementation
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
 * @brief		AVL tree implementation.
 */

#ifndef __TYPES_AVL_H
#define __TYPES_AVL_H

#include <types.h>

/** AVL tree node structure. */
typedef struct avltree_node {
	struct avltree_node *parent;	/**< Parent node. */
	struct avltree_node *left;	/**< Left-hand child node. */
	struct avltree_node *right;	/**< Right-hand child node. */

	int height;			/**< Height of the node. */

	key_t key;			/**< Key for the node. */
	void *value;			/**< Value associated with the node. */
} avltree_node_t;

/** AVL tree structure. */
typedef struct avltree {
	avltree_node_t *root;		/**< Root of the tree. */
} avltree_t;

/** Iterates over an AVL tree, setting iter to the node on each iteration. */
#define AVLTREE_FOREACH(tree, iter)		\
	for(avltree_node_t *iter = avltree_node_first((tree)); iter != NULL; iter = avltree_node_next(iter))

/** Initializes a statically declared AVL tree. */
#define AVLTREE_INITIALIZER()			\
	{ \
		.root = NULL, \
	}

/** Statically declares a new AVL tree. */
#define AVLTREE_DECLARE(_var)			\
	avltree_t _var = AVLTREE_INITIALIZER()

/** Gets an AVL tree node's data pointer and casts it to a certain type. */
#define avltree_entry(node, type)		\
	((node) ? (type *)(node->value) : NULL)

/** Checks whether the given AVL tree is empty. */
#define avltree_empty(tree) 			\
	((tree)->root == NULL)

/** Initialize an AVL tree.
 * @param tree		Tree to initialize. */
static inline void avltree_init(avltree_t *tree) {
	tree->root = NULL;
}

/** Main operations. */
extern void avltree_insert(avltree_t *tree, key_t key, void *value, avltree_node_t **nodep);
extern void avltree_remove(avltree_t *tree, key_t key);
extern void *avltree_lookup(avltree_t *tree, key_t key);

/** Iterator helper functions. */
extern avltree_node_t *avltree_node_first(avltree_t *tree);
extern avltree_node_t *avltree_node_last(avltree_t *tree);
extern avltree_node_t *avltree_node_prev(avltree_node_t *node);
extern avltree_node_t *avltree_node_next(avltree_node_t *node);

#endif /* __TYPES_AVL_H */
