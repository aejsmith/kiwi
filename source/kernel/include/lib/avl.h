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
 * @brief		AVL tree implementation.
 */

#ifndef __LIB_AVL_H
#define __LIB_AVL_H

#include <types.h>

/** AVL tree node structure. */
typedef struct avl_tree_node {
	struct avl_tree_node *parent;	/**< Parent node. */
	struct avl_tree_node *left;	/**< Left-hand child node. */
	struct avl_tree_node *right;	/**< Right-hand child node. */

	int height;			/**< Height of the node. */

	key_t key;			/**< Key for the node. */
	void *value;			/**< Value associated with the node. */
} avl_tree_node_t;

/** AVL tree structure. */
typedef struct avl_tree {
	avl_tree_node_t *root;		/**< Root of the tree. */
} avl_tree_t;

/** Iterates over an AVL tree, setting iter to the node on each iteration. */
#define AVL_TREE_FOREACH(tree, iter)		\
	for(avl_tree_node_t *iter = avl_tree_node_first((tree)); iter != NULL; iter = avl_tree_node_next(iter))

/** Iterates over an AVL tree, setting iter to the node on each iteration.
 * @note		Safe to use when the loop may modify the list. */
#define AVL_TREE_FOREACH_SAFE(tree, iter)		\
	for(avl_tree_node_t *iter = avl_tree_node_first((tree)), *_##iter = avl_tree_node_next(iter); \
	    iter != NULL; iter = _##iter, _##iter = avl_tree_node_next(iter))

/** Initialises a statically declared AVL tree. */
#define AVL_TREE_INITIALISER()			\
	{ \
		.root = NULL, \
	}

/** Statically declares a new AVL tree. */
#define AVL_TREE_DECLARE(_var)			\
	avl_tree_t _var = AVL_TREE_INITIALISER()

/** Gets an AVL tree node's data pointer and casts it to a certain type. */
#define avl_tree_entry(node, type)		\
	((node) ? (type *)(node->value) : NULL)

/** Checks whether the given AVL tree is empty. */
#define avl_tree_empty(tree) 			\
	((tree)->root == NULL)

/** Initialise an AVL tree.
 * @param tree		Tree to initialise. */
static inline void avl_tree_init(avl_tree_t *tree) {
	tree->root = NULL;
}

/** Main operations. */
extern void avl_tree_insert(avl_tree_t *tree, key_t key, void *value, avl_tree_node_t **nodep);
extern void avl_tree_remove(avl_tree_t *tree, key_t key);
extern void *avl_tree_lookup(avl_tree_t *tree, key_t key);

/** Iterator helper functions. */
extern avl_tree_node_t *avl_tree_node_first(avl_tree_t *tree);
extern avl_tree_node_t *avl_tree_node_last(avl_tree_t *tree);
extern avl_tree_node_t *avl_tree_node_prev(avl_tree_node_t *node);
extern avl_tree_node_t *avl_tree_node_next(avl_tree_node_t *node);

#endif /* __LIB_AVL_H */
