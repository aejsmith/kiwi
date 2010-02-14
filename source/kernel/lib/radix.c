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

#include <lib/radix.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

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
	size_t i = 0;

	while(key1[i] == key2[i]) {
		i++;
	}

	return radix_tree_key_dup(key1, i);
}

/** Add a node as a node's child.
 * @param parent	Parent node.
 * @param child		New child node. */
static void radix_tree_node_add_child(radix_tree_node_t *parent, radix_tree_node_t *child) {
	unsigned char high = (child->key[0] >> 4) & 0xF;
	unsigned char low = child->key[0] & 0xF;

	if(!parent->children[high]) {
		parent->children[high] = kcalloc(1, sizeof(radix_tree_node_ptr_t), MM_SLEEP);
	}
	if(!parent->children[high]->nodes[low]) {
		parent->children[high]->count++;
		parent->child_count++;
	}

	parent->children[high]->nodes[low] = child;
	child->parent = parent;
}

/** Remove child from a node.
 * @param parent	Parent node.
 * @param child		Child node to remove. */
static void radix_tree_node_remove_child(radix_tree_node_t *parent, radix_tree_node_t *child) {
	unsigned char high = (child->key[0] >> 4) & 0xF;
	unsigned char low = child->key[0] & 0xF;

	assert(parent->children[high]);
	assert(parent->children[high]->nodes[low] == child);
	assert(parent->children[high]->count);

	parent->children[high]->nodes[low] = NULL;
	if(--parent->children[high]->count == 0) {
		kfree(parent->children[high]);
		parent->children[high] = NULL;
	}

	parent->child_count--;
}

/** Find child of a node.
 * @param parent	Parent node.
 * @param key		Key to search for. */
static radix_tree_node_t *radix_tree_node_find_child(radix_tree_node_t *parent, unsigned char *key) {
	unsigned char high = (key[0] >> 4) & 0xF;
	unsigned char low = key[0] & 0xF;

	return (parent->children[high]) ? parent->children[high]->nodes[low] : NULL;
}

/** Get first child of a radix tree node.
 * @param node		Node to get first child of.
 * @return		Pointer to child, or NULL if no children. */
static radix_tree_node_t *radix_tree_node_first_child(radix_tree_node_t *node) {
	size_t i, j;

	if(node->child_count) {
		for(i = 0; i < ARRAYSZ(node->children); i++) {
			if(!node->children[i] || !node->children[i]->count) {
				continue;
			}
			for(j = 0; j < ARRAYSZ(node->children[i]->nodes); j++) {
				if(node->children[i]->nodes[j]) {
					return node->children[i]->nodes[j];
				}
			}
		}
	}

	return NULL;
}

/** Get the next sibling of a node.
 * @param node		Node to get next sibling of.
 * @return		Pointer to sibling, or NULL if there isn't one. */
static radix_tree_node_t *radix_tree_node_next_sibling(radix_tree_node_t *node) {
	size_t high = (node->key[0] >> 4) & 0xF, low = node->key[0] & 0xF;
	radix_tree_node_t *parent = node->parent;
	size_t i, j;

	for(i = high; i < ARRAYSZ(parent->children); i++) {
		if(!parent->children[i] || !parent->children[i]->count) {
			continue;
		}
		for(j = (i == high) ? (low + 1) : 0; j < ARRAYSZ(parent->children[i]->nodes); j++) {
			if(parent->children[i]->nodes[j]) {
				return parent->children[i]->nodes[j];
			}
		}
	}

	return NULL;
}

/** Create a new node and adds it to its parent.
 * @param parent	Parent of new node.
 * @param key		Key for new node (should be dynamically allocated).
 * @param value		Value for the node.
 * @return		Allocated node. */
static radix_tree_node_t *radix_tree_node_alloc(radix_tree_node_t *parent, unsigned char *key, void *value) {
	radix_tree_node_t *node = kcalloc(1, sizeof(radix_tree_node_t), MM_SLEEP);

	node->key = key;
	node->value = value;

	radix_tree_node_add_child(parent, node);
	return node;
}

/** Destroy a node.
 * @param node		Node to destroy. */
static void radix_tree_node_destroy(radix_tree_node_t *node) {
	/* Do not need to free child node array entries because they are
	 * automatically freed when they become empty. */
	kfree(node->key);
	kfree(node);
}

/** Clears all child nodes.
 * @param node		Node to clear.
 * @param helper	Function to call on non-NULL values (can be NULL). */
static void radix_tree_node_clear(radix_tree_node_t *node, radix_tree_clear_helper_t helper) {
	radix_tree_node_t *child;
	size_t i, j;

	for(i = 0; i < ARRAYSZ(node->children); i++) {
		/* Test the child array on each iteration - it may be freed
		 * automatically by radix_tree_node_remove_child() within the
		 * loop. */
		for(j = 0; node->children[i] && j < ARRAYSZ(node->children[i]->nodes); j++) {
			if(!(child = node->children[i]->nodes[j])) {
				continue;
			}

			/* Recurse onto the child. */
			radix_tree_node_clear(child, helper);

			/* Detach from the tree and destroy it. */
			radix_tree_node_remove_child(node, child);
			if(helper && child->value != NULL) {
				helper(child->value);
			}
			radix_tree_node_destroy(child);
		}
	}
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
	if(!key || !key[0]) {
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
			node = radix_tree_node_find_child(node, key);
			if(node) {
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
	radix_tree_node_t *node = &tree->root, *inter, *child;
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
			/* Partial match. First get common prefix and create an
			 * intermediate node. */
			common = radix_tree_key_common(str, node->key);
			inter = radix_tree_node_alloc(node->parent, common, NULL);

			/* Get length of common string. */
			len = radix_tree_key_len(common);

			/* Change the node's key. */
			dup = radix_tree_key_dup(node->key + len, 0);
			kfree(node->key);
			node->key = dup;

			/* Reparent this node to the intermediate node. */
			radix_tree_node_add_child(inter, node);

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
			child = radix_tree_node_find_child(node, str);
			if(child) {
				node = child;
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
 * @param helper	Helper function to free entry data (can be NULL).
 */
void radix_tree_remove(radix_tree_t *tree, const char *key, radix_tree_clear_helper_t helper) {
	radix_tree_node_t *node, *child, *parent;
	unsigned char *concat;
	size_t i, j;

	/* Look for the node to delete. If it is not found return. */
	node = radix_tree_node_lookup(tree, (unsigned char *)key);
	if(node == NULL) {
		return;
	}

	if(helper && node->value) {
		helper(node->value);
	}
	node->value = NULL;

	/* Now, go up the tree to optimize it. */
	while(node != &tree->root && !node->value) {
		if(node->child_count == 1) {
			/* Only one child: Just need to prepend our key to it.
			 * First need to find it... */
			for(i = 0, child = NULL; i < ARRAYSZ(node->children) && !child; i++) {
				if(!node->children[i]) {
					continue;
				}

				for(j = 0; j < ARRAYSZ(node->children[i]->nodes); j++) {
					if(node->children[i]->nodes[j]) {
						child = node->children[i]->nodes[j];
						break;
					}
				}
			}
			if(!child) {
				fatal("Child count inconsistent in radix tree");
			}

			/* Detach the child from ourself. */
			radix_tree_node_remove_child(node, child);

			/* Set the new key for the child. */
			concat = radix_tree_key_concat(node->key, child->key);
			kfree(child->key);
			child->key = concat;

			/* Replace us with it in the parent. */
			radix_tree_node_add_child(node->parent, child);

			/* Free ourselves. */
			radix_tree_node_destroy(node);
			return;
		} else if(node->child_count == 0) {
			/* Remove the current node. Save its parent before
			 * doing so. */
			parent = node->parent;
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

/** Initialise a radix tree.
 *
 * Initialises a radix tree structure.
 *
 * @param tree		Tree to destroy.
 */
void radix_tree_init(radix_tree_t *tree) {
	/* Clear the root node. */
	memset(&tree->root, 0, sizeof(tree->root));
}

/** Clear a radix tree.
 *
 * Clears out the contents of a radix tree.
 *
 * @param tree		Tree to clear.
 * @param helper	Helper function that gets called on all non-NULL values
 *			found in the tree (can be NULL).
 */
void radix_tree_clear(radix_tree_t *tree, radix_tree_clear_helper_t helper) {
	radix_tree_node_clear(&tree->root, helper);
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
			fatal("Destroying non-empty radix tree %p", tree);
		}
	}
}

/** Get the node following a node in a radix tree.
 *
 * Gets the node with a key that follows another node's key in a radix tree.
 *
 * @param node		Node to get following node of.
 * 
 * @return		Following node or NULL if none found.
 */
radix_tree_node_t *radix_tree_node_next(radix_tree_node_t *node) {
	radix_tree_node_t *orig = node, *tmp;

	while(node == orig || node->value == NULL) {
		/* Check if we have a child we can use. */
		if((tmp = radix_tree_node_first_child(node))) {
			node = tmp;
			continue;
		}

		/* Go up until we find a parent with a sibling after us. */
		while(node->parent) {
			if((tmp = radix_tree_node_next_sibling(node))) {
				node = tmp;
				break;
			}
			node = node->parent;
		}

		/* If we're now at the top then we didn't find any siblings. */
		if(!node->parent) {
			return NULL;
		}		
	}

	return node;
}
