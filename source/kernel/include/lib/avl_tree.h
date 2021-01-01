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
 * @brief               AVL tree implementation.
 */

#pragma once

#include <types.h>

/** AVL tree entry key type. */
typedef uint64_t avl_tree_key_t;

/** AVL tree node structure. */
typedef struct avl_tree_node {
    struct avl_tree_node *parent;   /**< Parent node. */
    struct avl_tree_node *left;     /**< Left-hand child node. */
    struct avl_tree_node *right;    /**< Right-hand child node. */
    int height;                     /**< Height of the node. */
    avl_tree_key_t key;             /**< Key for the node. */
} avl_tree_node_t;

/** AVL tree structure. */
typedef struct avl_tree {
    avl_tree_node_t *root;          /**< Root of the tree. */
} avl_tree_t;

/** Iterates over an AVL tree, setting iter to the node on each iteration. */
#define avl_tree_foreach(tree, iter) \
    for ( \
        avl_tree_node_t *iter = avl_tree_first((tree)); \
        iter; \
        iter = avl_tree_next(iter))

/** Iterates over an AVL tree, setting iter to the node on each iteration.
 * @note                Safe to use when the loop may modify the list. */
#define avl_tree_foreach_safe(tree, iter) \
    for ( \
        avl_tree_node_t *iter = avl_tree_first((tree)), *_##iter = avl_tree_next(iter); \
        iter; \
        iter = _##iter, _##iter = avl_tree_next(iter))

/** Initializes a statically defined AVL tree. */
#define AVL_TREE_INITIALIZER() \
    { \
        .root = NULL, \
    }

/** Statically defines a new AVL tree. */
#define AVL_TREE_DEFINE(_var) \
    avl_tree_t _var = AVL_TREE_INITIALIZER()

/** Get a pointer to the structure containing an AVL tree node.
 * @param node          AVL tree node pointer.
 * @param type          Type of the structure.
 * @param member        Name of the tree node member in the structure.
 * @return              Pointer to the structure. */
#define avl_tree_entry(node, type, member) \
    (type *)((char *)node - offsetof(type, member))

/** Initialize an AVL tree.
 * @param tree          Tree to initialize. */
static inline void avl_tree_init(avl_tree_t *tree) {
    tree->root = NULL;
}

/** Check whether the given AVL tree is empty. */
static inline bool avl_tree_empty(avl_tree_t *tree) {
    return !tree->root;
}

extern void avl_tree_insert(avl_tree_t *tree, avl_tree_key_t key, avl_tree_node_t *node);
extern void avl_tree_remove(avl_tree_t *tree, avl_tree_node_t *node);
extern avl_tree_node_t *avl_tree_lookup_node(avl_tree_t *tree, avl_tree_key_t key);

/** Look up an entry in an AVL tree.
 * @param tree          Tree to look up in.
 * @param key           Key to look for.
 * @param type          Type of the entry.
 * @param member        Name of the tree node member in the structure.
 * @return              Pointer to found structure, or NULL if not found. */
#define avl_tree_lookup(tree, key, type, member)    \
    __extension__ \
    ({ \
        avl_tree_node_t *__node = avl_tree_lookup_node(tree, key); \
        (__node) ? avl_tree_entry(__node, type, member) : NULL; \
    })

extern avl_tree_node_t *avl_tree_first(avl_tree_t *tree);
extern avl_tree_node_t *avl_tree_last(avl_tree_t *tree);
extern avl_tree_node_t *avl_tree_prev(avl_tree_node_t *node);
extern avl_tree_node_t *avl_tree_next(avl_tree_node_t *node);
