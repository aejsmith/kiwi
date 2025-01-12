/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Radix tree implementation.
 *
 * The functions in this file implement a radix tree (aka. Patricia trie),
 * which uses strings as keys.
 *
 * Reference:
 * - Wikipedia: Radix tree
 *   http://en.wikipedia.org/wiki/Radix_tree
 */

#include <lib/radix_tree.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <assert.h>

static inline size_t radix_tree_key_len(unsigned char *key) {
    return strlen((const char *)key);
}

static inline unsigned char *radix_tree_key_dup(unsigned char *key, size_t len) {
    if (len) {
        return (unsigned char *)kstrndup((const char *)key, len, MM_KERNEL);
    } else {
        return (unsigned char *)kstrdup((const char *)key, MM_KERNEL);
    }
}

static inline unsigned char *radix_tree_key_concat(unsigned char *key1, unsigned char *key2) {
    size_t len1 = radix_tree_key_len(key1);
    size_t len2 = radix_tree_key_len(key2);

    unsigned char *concat = kmalloc(len1 + len2 + 1, MM_KERNEL);

    strcpy((char *)concat, (const char *)key1);
    strcpy((char *)(concat + len1), (const char *)key2);

    return concat;
}

static inline unsigned char *radix_tree_key_common(unsigned char *key1, unsigned char *key2) {
    size_t i = 0;
    while (key1[i] == key2[i])
        i++;

    return radix_tree_key_dup(key1, i);
}

static void radix_tree_node_add_child(radix_tree_node_t *parent, radix_tree_node_t *child) {
    unsigned char high = (child->key[0] >> 4) & 0xf;
    unsigned char low  = child->key[0] & 0xf;

    if (!parent->children[high])
        parent->children[high] = kcalloc(1, sizeof(radix_tree_node_ptr_t), MM_KERNEL);

    if (!parent->children[high]->nodes[low]) {
        parent->children[high]->count++;
        parent->child_count++;
    }

    parent->children[high]->nodes[low] = child;
    child->parent = parent;
}

static void radix_tree_node_remove_child(radix_tree_node_t *parent, radix_tree_node_t *child) {
    unsigned char high = (child->key[0] >> 4) & 0xf;
    unsigned char low  = child->key[0] & 0xf;

    assert(parent->children[high]);
    assert(parent->children[high]->nodes[low] == child);
    assert(parent->children[high]->count);

    parent->children[high]->nodes[low] = NULL;
    if (--parent->children[high]->count == 0) {
        kfree(parent->children[high]);
        parent->children[high] = NULL;
    }

    parent->child_count--;
}

static radix_tree_node_t *radix_tree_node_find_child(radix_tree_node_t *parent, unsigned char *key) {
    unsigned char high = (key[0] >> 4) & 0xf;
    unsigned char low  = key[0] & 0xf;

    return (parent->children[high]) ? parent->children[high]->nodes[low] : NULL;
}

static radix_tree_node_t *radix_tree_node_first_child(radix_tree_node_t *node) {
    if (node->child_count) {
        for (size_t i = 0; i < array_size(node->children); i++) {
            if (!node->children[i] || !node->children[i]->count)
                continue;

            for (size_t j = 0; j < array_size(node->children[i]->nodes); j++) {
                if (node->children[i]->nodes[j])
                    return node->children[i]->nodes[j];
            }
        }
    }

    return NULL;
}

static radix_tree_node_t *radix_tree_node_next_sibling(radix_tree_node_t *node) {
    size_t high = (node->key[0] >> 4) & 0xf, low = node->key[0] & 0xf;
    radix_tree_node_t *parent = node->parent;

    for (size_t i = high; i < array_size(parent->children); i++) {
        if (!parent->children[i] || !parent->children[i]->count)
            continue;

        for (size_t j = (i == high) ? (low + 1) : 0; j < array_size(parent->children[i]->nodes); j++) {
            if (parent->children[i]->nodes[j])
                return parent->children[i]->nodes[j];
        }
    }

    return NULL;
}

static radix_tree_node_t *radix_tree_node_alloc(radix_tree_node_t *parent, unsigned char *key, void *value) {
    radix_tree_node_t *node = kcalloc(1, sizeof(radix_tree_node_t), MM_KERNEL);

    node->key   = key;
    node->value = value;

    radix_tree_node_add_child(parent, node);
    return node;
}

static void radix_tree_node_destroy(radix_tree_node_t *node) {
    /* Do not need to free child node array entries because they are
     * automatically freed when they become empty. */
    kfree(node->key);
    kfree(node);
}

/** Clears all child nodes.
 * @param node          Node to clear.
 * @param helper        Function to call on non-NULL values (can be NULL). */
static void radix_tree_node_clear(radix_tree_node_t *node, radix_tree_clear_helper_t helper) {
    for (size_t i = 0; i < array_size(node->children); i++) {
        /* Test the child array on each iteration - it may be freed
         * automatically by radix_tree_node_remove_child() within the loop. */
        for (size_t j = 0; node->children[i] && j < array_size(node->children[i]->nodes); j++) {
            radix_tree_node_t *child = node->children[i]->nodes[j];
            if (!child)
                continue;

            /* Recurse onto the child. */
            radix_tree_node_clear(child, helper);

            /* Detach from the tree and destroy it. */
            radix_tree_node_remove_child(node, child);
            if (helper && child->value)
                helper(child->value);

            radix_tree_node_destroy(child);
        }
    }
}

/** Check whether a node's key matches the given string.
 * @return              0 if no match, 1 if key's partially match, 2 if the
 *                      keys are an exact match, or 3 if there is an exact
 *                      match between the node's key and the first part of
 *                      the supplied key (i.e. the supplied key is longer). */
static int radix_tree_node_match(radix_tree_node_t *node, unsigned char *key) {
    if (!node->key) {
        return 3;
    } else if (node->key[0] != key[0]) {
        return 0;
    } else {
        size_t i = 0;
        while (node->key[i] && key[i]) {
            if (node->key[i] != key[i])
                return 1;
            i++;
        }

        if (node->key[i] == 0) {
            return (key[i] == 0) ? 2 : 3;
        } else {
            return 1;
        }
    }
}

static radix_tree_node_t *radix_tree_node_lookup(radix_tree_t *tree, unsigned char *key) {
    /* No zero-length keys. */
    if (!key || !key[0])
        return NULL;

    /* Iterate down the tree to find the node. */
    radix_tree_node_t *node = &tree->root;
    while (true) {
        int ret = radix_tree_node_match(node, key);
        if (ret == 2) {
            /* Exact match: return the value. */
            return node;
        } else if (ret == 3) {
            /* Supplied key is longer. */
            size_t i = 0;
            while (node->key && node->key[i]) {
                key++;
                i++;
            }

            /* Look for this key in the child list. */
            node = radix_tree_node_find_child(node, key);
            if (node)
                continue;

            /* Not in child list, nothing to do. */
            return NULL;
        } else {
            /* No match or partial match, nothing more to do. */
            return NULL;
        }
    }
}

/**
 * Inserts a value with the given key into a radix tree. If a node already
 * exists with the same key, then the node's value is replaced with the new
 * value. Zero length keys are not supported.
 *
 * @note                Nodes and keys within a radix tree are dynamically
 *                      allocated, so this function must not be called while
 *                      spinlocks are held, etc. (all the usual rules).
 *                      Allocations are made using MM_KERNEL, so it is possible
 *                      for this function to block.
 *
 * @param tree          Tree to insert into.
 * @param key           Key to insert.
 * @param value         Value key corresponds to.
 */
void radix_tree_insert(radix_tree_t *tree, const char *key, void *value) {
    unsigned char *str = (unsigned char *)key;

    /* No zero-length keys. */
    if (str[0] == 0)
        return;

    /* Iterate down the tree to find the node. */
    radix_tree_node_t *node = &tree->root;
    while (true) {
        int ret = radix_tree_node_match(node, str);
        if (ret == 1) {
            /* Partial match. First get common prefix and create an intermediate
             * node. */
            unsigned char *common    = radix_tree_key_common(str, node->key);
            radix_tree_node_t *inter = radix_tree_node_alloc(node->parent, common, NULL);

            /* Get length of common string. */
            size_t len = radix_tree_key_len(common);

            /* Change the node's key. */
            unsigned char *dup = radix_tree_key_dup(node->key + len, 0);
            kfree(node->key);
            node->key = dup;

            /* Reparent this node to the intermediate node. */
            radix_tree_node_add_child(inter, node);

            /* Now insert what we're inserting. If the uncommon part of the
             * string on what we're inserting is not zero length, create a child
             * node, else set the value on the intermediate node. */
            if (str[len] != 0) {
                dup = radix_tree_key_dup(str + len, 0);
                radix_tree_node_alloc(inter, dup, value);
            } else {
                inter->value = value;
            }

            break;
        } else if (ret == 2) {
            /* Exact match: set the value and return. */
            node->value = value;
            break;
        } else if (ret == 3) {
            /* Supplied key is longer. */
            size_t i = 0;
            while (node->key && node->key[i]) {
                str++;
                i++;
            }

            /* Look for this key in the child list. */
            radix_tree_node_t *child = radix_tree_node_find_child(node, str);
            if (child) {
                node = child;
                continue;
            }

            /* Not in child list, create a new child and finish. */
            radix_tree_node_alloc(node, radix_tree_key_dup(str, 0), value);
            break;
        } else {
            unreachable();
        }
    }
}

/**
 * Removes the value with the given key from a radix tree. If the key is not
 * found in the tree then the function will do nothing.
 *
 * @param tree          Tree to remove from.
 * @param key           Key to remove.
 * @param helper        Helper function to free entry data (can be NULL).
 */
void radix_tree_remove(radix_tree_t *tree, const char *key, radix_tree_clear_helper_t helper) {
    /* Look for the node to delete. If it is not found return. */
    radix_tree_node_t *node = radix_tree_node_lookup(tree, (unsigned char *)key);
    if (!node)
        return;

    if (helper && node->value)
        helper(node->value);

    node->value = NULL;

    /* Now, go up the tree to optimize it. */
    while (node != &tree->root && !node->value) {
        if (node->child_count == 1) {
            /* Only one child: Just need to prepend our key to it. First need to
             * find it... */
            radix_tree_node_t *child = NULL;
            for (size_t i = 0; i < array_size(node->children) && !child; i++) {
                if (!node->children[i])
                    continue;

                for (size_t j = 0; j < array_size(node->children[i]->nodes); j++) {
                    if (node->children[i]->nodes[j]) {
                        child = node->children[i]->nodes[j];
                        break;
                    }
                }
            }

            assert(child);

            /* Detach the child from ourself. */
            radix_tree_node_remove_child(node, child);

            /* Set the new key for the child. */
            unsigned char *concat = radix_tree_key_concat(node->key, child->key);
            kfree(child->key);
            child->key = concat;

            /* Replace us with it in the parent. */
            radix_tree_node_add_child(node->parent, child);

            /* Free ourselves. */
            radix_tree_node_destroy(node);
            return;
        } else if (node->child_count == 0) {
            /* Remove the current node. Save its parent before doing so. */
            radix_tree_node_t *parent = node->parent;
            radix_tree_node_remove_child(parent, node);
            radix_tree_node_destroy(node);

            /* Go up the tree and optimize. */
            node = parent;
        } else {
            break;
        }
    }
}

/** Look up a value in a radix tree.
 * @param tree          Tree to search in.
 * @param key           Key to search for.
 * @return              Value of key if found, NULL if not found. */
void *radix_tree_lookup(radix_tree_t *tree, const char *key) {
    radix_tree_node_t *node = radix_tree_node_lookup(tree, (unsigned char *)key);

    return (node) ? node->value : NULL;
}

/** Initialize a radix tree.
 * @param tree          Tree to destroy. */
void radix_tree_init(radix_tree_t *tree) {
    /* Clear the root node. */
    memset(&tree->root, 0, sizeof(tree->root));
}

/** Clear the contents of a radix tree.
 * @param tree          Tree to clear.
 * @param helper        Helper function that gets called on all non-NULL values
 *                      found in the tree (can be NULL). */
void radix_tree_clear(radix_tree_t *tree, radix_tree_clear_helper_t helper) {
    radix_tree_node_clear(&tree->root, helper);
}

/** Get the node following another node in a radix tree.
 * @param node          Node to get following node of.
 * @return              Following node or NULL if none found. */
radix_tree_node_t *radix_tree_node_next(radix_tree_node_t *node) {
    radix_tree_node_t *orig = node;

    while (node == orig || !node->value) {
        /* Check if we have a child we can use. */
        radix_tree_node_t *tmp = radix_tree_node_first_child(node);
        if (tmp) {
            node = tmp;
            continue;
        }

        /* Go up until we find a parent with a sibling after us. */
        while (node->parent) {
            tmp = radix_tree_node_next_sibling(node);
            if (tmp) {
                node = tmp;
                break;
            }

            node = node->parent;
        }

        /* If we're now at the top then we didn't find any siblings. */
        if (!node->parent)
            return NULL;
    }

    return node;
}
