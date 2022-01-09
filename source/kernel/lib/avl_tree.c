/*
 * Copyright (C) 2009-2022 Alex Smith
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
 *
 * Reference:
 * - Wikipedia - AVL tree:
 *   http://en.wikipedia.org/wiki/AVL_Tree
 * - Wikipedia - Tree rotation:
 *   http://en.wikipedia.org/wiki/Tree_rotation
 * - AVL Tree Rotations Tutorial:
 *   http://fortheloot.com/public/AVLTreeTutorial.rtf
 * - AVL Trees: Tutorial and C++ Implementation:
 *   http://www.cmcrossroads.com/bradapp/ftp/src/libs/C++/AvlTrees.html
 */

#include <lib/avl_tree.h>

#include <mm/malloc.h>

#include <assert.h>

/** Get the height of a subtree. Assumes that child heights are up-to-date. */
static inline int avl_tree_subtree_height(avl_tree_node_t *node) {
    if (!node)
        return 0;

    /* Get the heights of the children and add 1 to account for the node itself. */
    int left  = (node->left) ? (node->left->height + 1) : 1;
    int right = (node->right) ? (node->right->height + 1) : 1;

    /* Store the largest of the heights and return it. */
    node->height = (right > left) ? right : left;
    return node->height;
}

static inline int avl_tree_balance_factor(avl_tree_node_t *node) {
    return avl_tree_subtree_height(node->right) - avl_tree_subtree_height(node->left);
}

static inline void avl_tree_rotate_left(avl_tree_t *tree, avl_tree_node_t *node) {
    /* Store the node's current right child. */
    avl_tree_node_t *child = node->right;

    /* Node takes ownership of the child's left child as its right child
     * (replacing the existing right child). */
    node->right = child->left;
    if (node->right)
        node->right->parent = node;

    /* Reparent the child to node's parent. */
    child->parent = node->parent;
    if (!child->parent) {
        /* If parent becomes NULL we're at the root of the tree. */
        tree->root = child;
    } else {
        if (child->parent->left == node) {
            child->parent->left = child;
        } else {
            child->parent->right = child;
        }
    }

    /* Child now takes ownership of the old root node as its left child. */
    child->left  = node;
    node->parent = child;
}

static inline void avl_tree_rotate_right(avl_tree_t *tree, avl_tree_node_t *node) {
    /* Store the node's current left child. */
    avl_tree_node_t *child = node->left;

    /* Node takes ownership of the child's right child as its left child
     * (replacing the existing left child). */
    node->left = child->right;
    if (node->left)
        node->left->parent = node;

    /* Reparent the child to node's parent. */
    child->parent = node->parent;
    if (!child->parent) {
        /* If parent becomes NULL we're at the root of the tree. */
        tree->root = child;
    } else {
        if (child->parent->left == node) {
            child->parent->left = child;
        } else {
            child->parent->right = child;
        }
    }

    /* Child now takes ownership of the old root node as its right child. */
    child->right = node;
    node->parent = child;
}

static inline void avl_tree_balance_node(avl_tree_t *tree, avl_tree_node_t *node, int balance) {
    /* See "AVL Tree Rotations Tutorial" (in Reference at top of file). */
    if (balance > 1) {
        /* Tree is right-heavy, check whether a LR rotation is necessary (if
         * the right subtree is left-heavy). Note that if the tree is right-
         * heavy, then node->right is guaranteed not to be a null pointer. */
        if (avl_tree_balance_factor(node->right) < 0) {
            /* LR rotation. Perform a right rotation of the right subtree. */
            avl_tree_rotate_right(tree, node->right);
        }

        avl_tree_rotate_left(tree, node);
    } else if (balance < -1) {
        /* Tree is left-heavy, check whether a RL rotation is necessary (if the
         * left subtree is right-heavy). */
        if (avl_tree_balance_factor(node->left) > 0) {
            /* RL rotation. Perform a left rotation of the left subtree. */
            avl_tree_rotate_left(tree, node->left);
        }

        avl_tree_rotate_right(tree, node);
    }
}

/** Insert a node in an AVL tree.
 * @param tree          Tree to insert into.
 * @param key           Key to give the node.
 * @param node          Node that the key will map to. This function does NOT
 *                      check if the node is already in another tree: it must
 *                      be removed by the caller if it is. */
void avl_tree_insert(avl_tree_t *tree, avl_tree_key_t key, avl_tree_node_t *node) {
    node->left   = NULL;
    node->right  = NULL;
    node->height = 0;
    node->key    = key;

    /* If tree is currently empty, just insert and finish. */
    if (!tree->root) {
        node->parent = NULL;
        tree->root   = node;
        return;
    }

    /* Descend to where we want to insert the node. */
    avl_tree_node_t **next = &tree->root;
    avl_tree_node_t *curr  = NULL;
    while (*next) {
        curr = *next;

        /* Ensure that the key is unique. */
        assert(key != curr->key);

        /* Get the next pointer. */
        next = (key > curr->key) ? &curr->right : &curr->left;
    }

    /* We now have an insertion point for the new node. */
    node->parent = curr;
    *next = node;

    /* Now go back up the tree and check its balance. */
    while (curr) {
        int balance = avl_tree_balance_factor(curr);
        if (balance < -1 || balance > 1)
            avl_tree_balance_node(tree, curr, balance);

        curr = curr->parent;
    }
}

/** Remove a node from an AVL tree.
 * @param tree          Tree to remove from.
 * @param node          Node to remove. */
void avl_tree_remove(avl_tree_t *tree, avl_tree_node_t *node) {
    avl_tree_node_t *start;

    /* First we need to detach the node from the tree. */
    if (node->left) {
        /* Left node exists. Descend onto it, and then find the right-most node,
         * which will replace the node that we're removing. */
        avl_tree_node_t *child = node->left;
        while (child->right)
            child = child->right;

        if (child != node->left) {
            if (child->left) {
                /* There is a left subtree. This must be moved up to replace
                 * child. */
                child->left->parent = child->parent;
                child->parent->right = child->left;
                start = child->left;
            } else {
                /* Detach the child. */
                child->parent->right = NULL;
                start = child->parent;
            }

            child->left = node->left;
        } else {
            /* The left child has no right child. It will replace the node being
             * deleted as-is. */
            start = child;
        }

        /* Replace the node and fix up pointers. */
        child->right = node->right;
        child->parent = node->parent;
        if (child->right)
            child->right->parent = child;
        if (child->left)
            child->left->parent = child;
        if (node->parent) {
            if (node->parent->left == node) {
                node->parent->left = child;
            } else {
                node->parent->right = child;
            }
        } else {
            assert(node == tree->root);
            tree->root = child;
        }
    } else if (node->right) {
        /* Left node doesn't exist but right node does. This is easy. Just
         * replace the node with its right child. */
        node->right->parent = node->parent;
        if (node->parent) {
            if (node->parent->left == node) {
                node->parent->left = node->right;
            } else {
                node->parent->right = node->right;
            }
        } else {
            assert(node == tree->root);
            tree->root = node->right;
        }
        start = node->right;
    } else {
        /* Node is a leaf. If it is the only element in the tree, then just
         * remove it and return - no rebalancing required. Otherwise, remove it
         * and then rebalance. */
        if (node->parent) {
            if (node->parent->left == node) {
                node->parent->left = NULL;
            } else {
                node->parent->right = NULL;
            }
        } else {
            assert(node == tree->root);
            tree->root = NULL;
            return;
        }
        start = node->parent;
    }

    /* Start now points to where we want to start rebalancing from. */
    while (start) {
        int balance = avl_tree_balance_factor(start);
        if (balance < -1 || balance > 1)
            avl_tree_balance_node(tree, start, balance);

        start = start->parent;
    }
}

/** Look up a node in an AVL tree.
 * @param tree          Tree to look up in.
 * @param key           Key to look for.
 * @return              Pointer to node if found, NULL if not. */
avl_tree_node_t *avl_tree_lookup_node(avl_tree_t *tree, avl_tree_key_t key) {
    /* Descend down the tree to find the required node. */
    avl_tree_node_t *node = tree->root;
    while (node) {
        if (node->key > key) {
            node = node->left;
        } else if (node->key < key) {
            node = node->right;
        } else {
            return node;
        }
    }

    return NULL;
}

/**
 * Gets a pointer to the first node (the one with the lowest key) in an AVL
 * tree by descending down the tree's left-hand side.
 *
 * @param tree          Tree to get from.
 *
 * @return              Pointer to node, or NULL if tree empty.
 */
avl_tree_node_t *avl_tree_first(avl_tree_t *tree) {
    avl_tree_node_t *node = tree->root;
    if (node) {
        /* Descend down the left-hand side of the tree to find the smallest
         * node. */
        while (node->left)
            node = node->left;
    }

    return node;
}

/**
 * Gets a pointer to the last node (the one with the highest key) in an AVL
 * tree by descending down the tree's right-hand side.
 *
 * @param tree          Tree to get from.
 *
 * @return              Pointer to node, or NULL if tree empty.
 */
avl_tree_node_t *avl_tree_last(avl_tree_t *tree) {
    avl_tree_node_t *node = tree->root;
    if (node) {
        /* Descend down the right-hand side of the tree to find the largest
         * node. */
        while (node->right)
            node = node->right;
    }

    return node;
}

/** Get the node preceding another node in an AVL tree.
 * @param node          Node to get preceding node of.
 * @return              Preceding node or NULL if none found. */
avl_tree_node_t *avl_tree_prev(avl_tree_node_t *node) {
    if (!node)
        return NULL;

    /* If there's a left-hand child, move onto it and then go as far right as
     * we can. */
    if (node->left) {
        node = node->left;
        while (node->right)
            node = node->right;

        return node;
    } else {
        /* There's no left-hand children, go up until we find an ancestor that
         * is the right-hand child of its parent. */
        while (node->parent && node == node->parent->left)
            node = node->parent;

        /* The parent will now point to the preceding node (or NULL, if we reach
         * the top of the tree). */
        return node->parent;
    }
}

/** Get the node following another node in an AVL tree.
 * @param node          Node to get following node of.
 * @return              Following node or NULL if none found. */
avl_tree_node_t *avl_tree_next(avl_tree_node_t *node) {
    if (!node)
        return NULL;

    /* If there's a right-hand child, move onto it and then go as far left as
     * we can. */
    if (node->right) {
        node = node->right;
        while (node->left)
            node = node->left;

        return node;
    } else {
        /* There's no right-hand children, go up until we find an ancestor that
         * is the left-hand child of its parent. */
        while (node->parent && node == node->parent->right)
            node = node->parent;

        /* The parent will now point to the following node (or NULL, if we
         * reach the top of the tree). */
        return node->parent;
    }
}
