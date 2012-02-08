/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		AVL tree implementation.
 */

#ifndef __LIB_AVL_TREE_H
#define __LIB_AVL_TREE_H

#include <types.h>

/** AVL tree node structure. */
typedef struct avl_tree_node {
	struct avl_tree_node *parent;	/**< Parent node. */
	struct avl_tree_node *left;	/**< Left-hand child node. */
	struct avl_tree_node *right;	/**< Right-hand child node. */
	int height;			/**< Height of the node. */
	key_t key;			/**< Key for the node. */
	void *value;			/**< Value for the node. */
} avl_tree_node_t;

/** AVL tree structure. */
typedef struct avl_tree {
	avl_tree_node_t *root;		/**< Root of the tree. */
} avl_tree_t;

/** Iterates over an AVL tree, setting iter to the node on each iteration. */
#define AVL_TREE_FOREACH(tree, iter)		\
	for(avl_tree_node_t *iter = avl_tree_first((tree)); iter != NULL; iter = avl_tree_node_next(iter))

/** Iterates over an AVL tree, setting iter to the node on each iteration.
 * @note		Safe to use when the loop may modify the list. */
#define AVL_TREE_FOREACH_SAFE(tree, iter)		\
	for(avl_tree_node_t *iter = avl_tree_first((tree)), *_##iter = avl_tree_node_next(iter); \
	    iter != NULL; iter = _##iter, _##iter = avl_tree_node_next(iter))

/** Initializes a statically declared AVL tree. */
#define AVL_TREE_INITIALIZER()			\
	{ \
		.root = NULL, \
	}

/** Statically declares a new AVL tree. */
#define AVL_TREE_DECLARE(_var)			\
	avl_tree_t _var = AVL_TREE_INITIALIZER()

/** Gets an AVL tree node's data pointer and casts it to a certain type.
 * @note		Evaluates to NULL if the node pointer is NULL. */
#define avl_tree_entry(node, type)	\
	((node) ? (type *)node->value : NULL)

/** Checks whether the given AVL tree is empty. */
#define avl_tree_empty(tree) 			\
	((tree)->root == NULL)

/** Initialize an AVL tree.
 * @param tree		Tree to initialize. */
static inline void avl_tree_init(avl_tree_t *tree) {
	tree->root = NULL;
}

extern void avl_tree_insert(avl_tree_t *tree, avl_tree_node_t *node, key_t key, void *value);
extern void avl_tree_remove(avl_tree_t *tree, avl_tree_node_t *node);
extern void avl_tree_dyn_insert(avl_tree_t *tree, key_t key, void *value);
extern void avl_tree_dyn_remove(avl_tree_t *tree, key_t key);
extern void *avl_tree_lookup(avl_tree_t *tree, key_t key);

extern avl_tree_node_t *avl_tree_first(avl_tree_t *tree);
extern avl_tree_node_t *avl_tree_last(avl_tree_t *tree);
extern avl_tree_node_t *avl_tree_node_prev(avl_tree_node_t *node);
extern avl_tree_node_t *avl_tree_node_next(avl_tree_node_t *node);

#endif /* __LIB_AVL_TREE_H */
