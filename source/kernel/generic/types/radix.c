/* Kiwi radix tree implementation
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
 *
 * The functions in this file implement a radix tree (aka. Patricia trie),
 * which uses strings as keys.
 *
 * Radix trees seem to be horribly underdocumented... Thanks to JamesM for
 * referring me to his radix tree implementation, helped me understand this
 * much better.
 *
 * Reference:
 * - Wikipedia: Radix tree
 *   http://en.wikipedia.org/wiki/Radix_tree
 */

#include <console/kprintf.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <types/radix.h>

#include <assert.h>
#include <fatal.h>

/** Get length of key string.
 * @param key		Key to get length of.
 * @return		Length of key. */
static inline size_t radix_tree_key_len(unsigned char *key) {
	return strlen((const char *)key);
}

/** Duplicate a key string.
 * @param key		Key to duplicate.
 * @param len		Length to duplicate (if 0 will duplicate entire key).
 * @return		Pointer to duplicated key. */
static inline unsigned char *radix_tree_key_dup(unsigned char *key, size_t len) {
	if(len) {
		return (unsigned char *)kstrndup((const char *)key, len, MM_SLEEP);
	} else {
		return (unsigned char *)kstrdup((const char *)key, MM_SLEEP);
	}
}

/** Concatenate two key strings.
 * @param key1		First key.
 * @param key2		Second key.
 * @return		Pointer to new key. */
static inline unsigned char *radix_tree_key_concat(unsigned char *key1, unsigned char *key2) {
	size_t len1 = radix_tree_key_len(key1), len2 = radix_tree_key_len(key2);
	unsigned char *concat;

	concat = kmalloc(len1 + len2 + 1, MM_SLEEP);
	strcpy((char *)concat, (const char *)key1);
	strcpy((char *)(concat + len1), (const char *)key2);
	return concat;
}

/** Get common prefix of two keys.
 * @param key1		First key.
 * @param key2		Second key.
 * @return		Pointer to duplicated key. */
static inline unsigned char *radix_tree_key_common(unsigned char *key1, unsigned char *key2) {
	size_t i;

	while(key1[i] == key2[i]) {
		i++;
	}

	return radix_tree_key_dup(key1, i);
}

/** Create a new node and adds it to its parent.
 * @param parent	Parent of new node.
 * @param key		Key for new node (should be dynamically allocated).
 * @param value		Value for the node.
 * @return		Allocated node. */
static radix_tree_node_t *radix_tree_node_alloc(radix_tree_node_t *parent, unsigned char *key, void *value) {
	radix_tree_node_t *node = kcalloc(1, sizeof(radix_tree_node_t), MM_SLEEP);

	node->parent = parent;
	node->key = key;
	node->value = value;

	if(parent->children[key[0]] == NULL) {
		parent->child_count++;
	}
	parent->children[key[0]] = node;
	return node;
}

/** Check whether a node's key matches the given string.
 * @param node		Node to match against.
 * @param key		Key to check.
 * @return		0 if no match, 1 if key's partially match, 2 if the
 *			keys are an exact match, or 3 if there is an exact
 *			match between the node's key and the first part of
 *			the supplied key (i.e. the supplied key is longer). */
static int radix_tree_node_match(radix_tree_node_t *node, unsigned char *key) {
	size_t i = 0;

	if(node->key == NULL) {
		return 3;
	} else if(node->key[0] != key[0]) {
		return 0;
	} else {
		while(node->key[i] && key[i]) {
			if(node->key[i] != key[i]) {
				return 1;
			}
			i++;
		}

		if(node->key[i] == 0) {
			return (key[i] == 0) ? 2 : 3;
		} else {
			return 1;
		}
	}
}

/** Internal part of lookup.
 * @param tree		Tree to lookup in.
 * @param key		Key to look for.
 * @return		Pointer to node structure. */
static radix_tree_node_t *radix_tree_node_lookup(radix_tree_t *tree, unsigned char *key) {
	radix_tree_node_t *node = &tree->root;
	size_t i;
	int ret;

	/* No zero-length keys. */
	if(key[0] == 0) {
		return NULL;
	}

	/* Iterate down the tree to find the node. */
	while(true) {
		ret = radix_tree_node_match(node, key);
		if(ret == 2) {
			/* Exact match: return the value. */
			return node;
		} else if(ret == 3) {
			/* Supplied key is longer. */
			i = 0;
			while(node->key && node->key[i]) {
				key++;
				i++;
			}

			/* Look for this key in the child list. */
			if(node->children[key[0]] != NULL) {
				node = node->children[key[0]];
				continue;
			}

			/* Not in child list, nothing to do. */
			return NULL;
		} else {
			/* No match or partial match, nothing more to do. */
			return NULL;
		}
	}
}

/** Insert a value into a radix tree.
 *
 * Inserts a value with the given key into a radix tree. If a node already
 * exists with the same key, then the node's value is replaced with the new
 * value. Zero length keys are not supported.
 *
 * @note		Nodes and keys within a radix tree are dynamically
 *			allocated, so this function must not be called while
 *			spinlocks are held, etc. (all the usual rules).
 *			Allocations are made using MM_SLEEP, so it is possible
 *			for this function to block.
 *
 * @param tree		Tree to insert into.
 * @param key		Key to insert.
 * @param value		Value key corresponds to.
 */
void radix_tree_insert(radix_tree_t *tree, const char *key, void *value) {
	unsigned char *str = (unsigned char *)key, *common, *dup;
	radix_tree_node_t *node = &tree->root, *inter;
	size_t i, len;
	int ret;

	/* No zero-length keys. */
	if(str[0] == 0) {
		return;
	}

	/* Iterate down the tree to find the node. */
	while(true) {
		ret = radix_tree_node_match(node, str);
		if(ret == 1) {
			/* Partial match. First get common prefix and create
			 * an intermediate node. */
			common = radix_tree_key_common(str, node->key);
			inter = radix_tree_node_alloc(node->parent, common, NULL);

			/* Get length of common string. */
			len = radix_tree_key_len(common);

			/* Change the node's key. */
			dup = radix_tree_key_dup(node->key + len, 0);
			kfree(node->key);
			node->key = dup;

			/* Reparent this node to the intermediate node. */
			inter->children[node->key[0]] = node;
			inter->child_count++;
			node->parent = inter;

			/* Now insert what we're inserting. If the uncommon
			 * part of the string on what we're inserting is not
			 * zero length, create a child node, else set the
			 * value on the intermediate node. */
			if(str[len] != 0) {
				dup = radix_tree_key_dup(str + len, 0);
				radix_tree_node_alloc(inter, dup, value);
			} else {
				inter->value = value;
			}
			break;
		} else if(ret == 2) {
			/* Exact match: set the value and return. */
			node->value = value;
			break;
		} else if(ret == 3) {
			/* Supplied key is longer. */
			i = 0;
			while(node->key && node->key[i]) {
				str++;
				i++;
			}

			/* Look for this key in the child list. */
			if(node->children[str[0]] != NULL) {
				node = node->children[str[0]];
				continue;
			}

			/* Not in child list, create a new child and finish. */
			radix_tree_node_alloc(node, radix_tree_key_dup(str, 0), value);
			break;
		} else {
			fatal("Should not get here (radix_tree_insert)");
		}
	}
}

/** Remove a value from a radix tree.
 *
 * Removes the value with the given key from a radix tree. If the key is not
 * found in the tree then the function will do nothing.
 *
 * @param tree		Tree to remove from.
 * @param key		Key to remove.
 */
void radix_tree_remove(radix_tree_t *tree, const char *key) {
	radix_tree_node_t *node, *child = NULL, *parent;
	unsigned char *concat;
	size_t i;

	/* Look for the node to delete. If it is not found return. */
	node = radix_tree_node_lookup(tree, (unsigned char *)key);
	if(node == NULL) {
		return;
	}

	/* We have the node we wish to remove. Set the value to NULL. */
	node->value = NULL;

	/* Now, go up the tree to optimize it. */
	while(node != &tree->root && !node->value) {
		if(node->child_count == 1) {
			/* Only one child: Just need to prepend our key to it.
			 * First need to find it... */
			for(i = 0; i < ARRAYSZ(node->children); i++) {
				if(node->children[i]) {
					child = node->children[i];
					break;
				}
			}
			if(!child) {
				fatal("Child count inconsistent in radix tree");
			}

			/* Set the new key for the child. */
			concat = radix_tree_key_concat(node->key, child->key);
			kfree(child->key);
			child->key = concat;

			/* Replace us with it in the parent. */
			node->parent->children[node->key[0]] = child;
			child->parent = node->parent;

			/* Free ourselves. */
			kfree(node->key);
			kfree(node);
			return;
		} else if(node->child_count == 0) {
			/* Remove the current node. Save its parent before doing so. */
			parent = node->parent;
			parent->children[node->key[0]] = NULL;
			parent->child_count--;
			kfree(node->key);
			kfree(node);

			/* Go up the tree and optimize. */
			node = parent;
		} else {
			break;
		}
	}
}

/** Look up a value in a radix tree.
 *
 * Looks up the value associated with a key within a radix tree.
 *
 * @param tree		Tree to search in.
 * @param key		Key to search for.
 *
 * @return		Value of key if found, NULL if not found.
 */
void *radix_tree_lookup(radix_tree_t *tree, const char *key) {
	radix_tree_node_t *node = radix_tree_node_lookup(tree, (unsigned char *)key);
	return (node) ? node->value : NULL;
}

/** Initialize a radix tree.
 *
 * Initializes a radix tree structure.
 *
 * @param tree		Tree to destroy.
 */
void radix_tree_init(radix_tree_t *tree) {
	/* Clear the root node. */
	memset(&tree->root, 0, sizeof(tree->root));
}

/** Destroy a radix tree.
 *
 * Destroys a radix tree structure. The tree MUST be empty.
 *
 * @param tree		Tree to destroy.
 */
void radix_tree_destroy(radix_tree_t *tree) {
	size_t i;

	for(i = 0; i < ARRAYSZ(tree->root.children); i++) {
		if(tree->root.children[i] != NULL) {
			fatal("Destroying non-empty radix tree 0x%p", tree);
		}
	}
}

static void radix_tree_dump_node(radix_tree_node_t *node, int indent) {
	size_t i;

	kprintf(LOG_NORMAL, "%*sNode 0x%p: '%s' -> %p\n", indent, "", node,
	        (node->key) ? node->key : (unsigned char *)"", node->value);

	for(i = 0; i < ARRAYSZ(node->children); i++) {
		if(node->children[i]) {
			radix_tree_dump_node(node->children[i], indent + 2);
		}
	}
}

/** Dump a radix tree. */
void radix_tree_dump(radix_tree_t *tree) {
	radix_tree_dump_node(&tree->root, 0);
}
