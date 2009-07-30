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
 *
 * Implementation details:
 * - Non-unique keys are not supported.
 * - Nodes are dynamically allocated.
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

#include <mm/malloc.h>

#include <types/avl.h>

#include <assert.h>
#include <errors.h>

/** Get the height of a subtree. Assumes that child heights are up-to-date.
 * @param node		Root node of subtree.
 * @return		Height of the subtree. */
static inline int avl_tree_subtree_height(avl_tree_node_t *node) {
	int left, right;

	if(node == NULL) {
		return 0;
	}

	/* Get the heights of the children and add 1 to account for the node
	 * itself. */
	left = (node->left) ? (node->left->height + 1) : 1;
	right = (node->right) ? (node->right->height + 1) : 1;

	/* Store the largest of the heights and return it. */
	node->height = (right > left) ? right : left;
	return node->height;
}

/** Get the balance factor of a node.
 * @param node		Node to get balance factor of.
 * @return		Balance factor of node. */
static inline int avl_tree_balance_factor(avl_tree_node_t *node) {
	return avl_tree_subtree_height(node->right) - avl_tree_subtree_height(node->left);
}

/** Perform a left rotation.
 * @param node		Root node of the rotation. */
static inline void avl_tree_rotate_left(avl_tree_t *tree, avl_tree_node_t *node) {
	avl_tree_node_t *child;

	/* Store the node's current right child. */
	child = node->right;

	/* Node takes ownership of the child's left child as its right child
	 * (replacing the existing right child). */
	node->right = child->left;
	if(node->right) {
		node->right->parent = node;
	}

	/* Reparent the child to node's parent. */
	child->parent = node->parent;
	if(child->parent == NULL) {
		/* If parent becomes NULL we're at the root of the tree. */
		tree->root = child;
	} else {
		if(child->parent->left == node) {
			child->parent->left = child;
		} else {
			child->parent->right = child;
		}
	}

	/* Child now takes ownership of the old root node as its left child. */
	child->left = node;
	child->left->parent = child;
}

/** Perform a right rotation.
 * @param node		Root node of the rotation. */
static inline void avl_tree_rotate_right(avl_tree_t *tree, avl_tree_node_t *node) {
	avl_tree_node_t *child;

	/* Store the node's current left child. */
	child = node->left;

	/* Node takes ownership of the child's right child as its left child
	 * (replacing the existing left child). */
	node->left = child->right;
	if(node->left) {
		node->left->parent = node;
	}

	/* Reparent the child to node's parent. */
	child->parent = node->parent;
	if(child->parent == NULL) {
		/* If parent becomes NULL we're at the root of the tree. */
		tree->root = child;
	} else {
		if(child->parent->left == node) {
			child->parent->left = child;
		} else {
			child->parent->right = child;
		}
	}

	/* Child now takes ownership of the old root node as its right child. */
	child->right = node;
	child->right->parent = child;
}

/** Balance a node after an insertion.
 * @param node		Node to balance.
 * @param balance	Balance factor of node. */
static inline void avl_tree_balance_node(avl_tree_t *tree, avl_tree_node_t *node, int balance) {
	/* See "AVL Tree Rotations Tutorial" (in Reference at top of file). */
	if(balance > 1) {
		/* Tree is right-heavy, check whether a LR rotation is
		 * necessary (if the right subtree is left-heavy). Note that if
		 * the tree is right-heavy, then node->right is guaranteed not
		 * to be a null pointer. */
		if(avl_tree_balance_factor(node->right) < 0) {
			/* LR rotation. Perform a right rotation of the right
			 * subtree. */
			avl_tree_rotate_right(tree, node->right);
		}

		avl_tree_rotate_left(tree, node);
	} else if(balance < -1) {
		/* Tree is left-heavy, check whether a RL rotation is
		 * necessary (if the left subtree is right-heavy). */
		if(avl_tree_balance_factor(node->left) > 0) {
			/* RL rotation. Perform a left rotation of the left
			 * subtree. */
			avl_tree_rotate_left(tree, node->left);
		}

		avl_tree_rotate_right(tree, node);
	}
}

/** Internal part of node lookup.
 * @param tree		Tree to look up in.
 * @param key		Key to look for.
 * @return		Pointer to node if found, NULL if not. */
static avl_tree_node_t *avl_tree_lookup_internal(avl_tree_t *tree, key_t key) {
	avl_tree_node_t *node = tree->root;

	/* Descend down the tree to find the required node. */
	while(node != NULL) {
		if(node->key > key) {
			node = node->left;
		} else if(node->key < key) {
			node = node->right;
		} else {
			return node;
		}
	}

	return NULL;
}

/** Insert a node in an AVL tree.
 *
 * Inserts a node into an AVL tree. The node's key will be set to the given
 * key value.
 *
 * @param tree		Tree to insert into.
 * @param key		Key to give the node.
 * @param value		Value the key is associated with.
 * @param nodep		Where to store pointer to node if required.
 */
void avl_tree_insert(avl_tree_t *tree, key_t key, void *value, avl_tree_node_t **nodep) {
	avl_tree_node_t **next, *curr = NULL, *node;
	int balance;

	/* Check if the key is unique. */
	if(avl_tree_lookup(tree, key) != NULL) {
		fatal("Attempted to insert duplicate key into AVL tree");
	}

	/* Create and set up the node. */
	node = kmalloc(sizeof(avl_tree_node_t), MM_SLEEP);
	node->parent = NULL;
	node->left = NULL;
	node->right = NULL;
	node->height = 0;
	node->key = key;
	node->value = value;

	/* Store the node pointer if needed. */
	if(nodep != NULL) {
		*nodep = node;
	}

	/* If tree is currently empty, just insert and finish. */
	if(tree->root == NULL) {
		tree->root = node;
		return;
	}

	/* Descend to where we want to insert the node. */
	next = &tree->root;
	while((*next) != NULL) {
		curr = *next;

		/* We checked that the key is unique, so this should not be the
		 * case. */
		assert(key != curr->key);

		/* Get the next pointer. */
		next = (key > curr->key) ? &curr->right : &curr->left;
	}

	assert(curr);

	/* We now have an insertion point for the new node. */
	node->parent = curr;
	*next = node;

	/* Now go back up the tree and check its balance. */
	while(curr != NULL) {
		balance = avl_tree_balance_factor(curr);
		if(balance < -1 || balance > 1) {
			avl_tree_balance_node(tree, curr, balance);
		}
		curr = curr->parent;
	}
}

/** Remove a node from an AVL tree.
 *
 * Removes the given node from its containing AVL tree.
 *
 * @todo		This could probably be a bit better.
 *
 * @param tree		Tree to remove from.
 * @param node		Node to remove.
 */
void avl_tree_remove(avl_tree_t *tree, key_t key) {
	avl_tree_node_t *child, *start, *node;
	int balance;

	/* Find the node. */
	node = avl_tree_lookup_internal(tree, key);
	if(node == NULL) {
		return;
	}

	/* First we need to detach the node from the tree. */
	if(node->left != NULL) {
		/* Left node exists. Descend onto it, and then find the
		 * right-most node, which will replace the node that we're
		 * removing. */
		child = node->left;
		while(child->right != NULL) {
			child = child->right;
		}

		if(child != node->left) {
			if(child->left != NULL) {
				/* There is a left subtree. This must be moved up to
				 * replace child. */
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
			/* The left child has no right child. It will replace
			 * the node being deleted as-is. */
			start = child;
		}

		/* Replace the node and fix up pointers. */
		child->right = node->right;
		child->parent = node->parent;
		if(child->right != NULL) {
			child->right->parent = child;
		}
		if(child->left != NULL) {
			child->left->parent = child;
		}
		if(node->parent != NULL) {
			if(node->parent->left == node) {
				node->parent->left = child;
			} else {
				node->parent->right = child;
			}
		} else {
			assert(node == tree->root);
			tree->root = child;
		}
	} else if(node->right != NULL) {
		/* Left node doesn't exist but right node does. This is easy.
		 * Just replace the node with its right child. */
		node->right->parent = node->parent;
		if(node->parent != NULL) {
			if(node->parent->left == node) {
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
		/* Node is a leaf. If it is the only element in the tree,
		 * then just remove it and return - no rebalancing required.
		 * Otherwise, remove it and then rebalance. */
		if(node->parent != NULL) {
			if(node->parent->left == node) {
				node->parent->left = NULL;
			} else {
				node->parent->right = NULL;
			}
		} else {
			assert(node == tree->root);
			tree->root = NULL;

			kfree(node);
			return;
		}
		start = node->parent;
	}

	kfree(node);

	/* Start now points to where we want to start rebalancing from. */
	while(start != NULL) {
		balance = avl_tree_balance_factor(start);
		if(balance < -1 || balance > 1) {
			avl_tree_balance_node(tree, start, balance);
		}
		start = start->parent;
	}
}

/** Look up a node in an AVL tree.
 *
 * Looks up the node with the given key in an AVL tree.
 *
 * @param tree		Tree to look up in.
 * @param key		Key to look for.
 *
 * @return		Node's value if found, NULL if not.
 */
void *avl_tree_lookup(avl_tree_t *tree, key_t key) {
	avl_tree_node_t *node = avl_tree_lookup_internal(tree, key);

	return (node != NULL) ? node->value : NULL;
}

/** Get the first node in an AVL tree.
 *
 * Gets a pointer to the first node (the one with the lowest key) in an AVL
 * tree by descending down the tree's left-hand side.
 *
 * @param tree		Tree to get from.
 *
 * @return		Pointer to node, or NULL if tree empty.
 */
avl_tree_node_t *avl_tree_node_first(avl_tree_t *tree) {
	avl_tree_node_t *node = tree->root;

	/* If the tree is empty return now. */
	if(node == NULL) {
		return NULL;
	}

	/* Descend down the left-hand side of the tree to find the smallest
	 * node. */
	while(node->left != NULL) {
		node = node->left;
	}

	return node;
}

/** Get the last node in an AVL tree.
 *
 * Gets a pointer to the last node (the one with the highest key) in an AVL
 * tree by descending down the tree's right-hand side.
 *
 * @param tree		Tree to get from.
 *
 * @return		Pointer to node, or NULL if tree empty.
 */
avl_tree_node_t *avl_tree_node_last(avl_tree_t *tree) {
	avl_tree_node_t *node = tree->root;

	/* If the tree is empty return now. */
	if(node == NULL) {
		return NULL;
	}

	/* Descend down the right-hand side of the tree to find the largest
	 * node. */
	while(node->right != NULL) {
		node = node->right;
	}

	return node;
}

/** Get the node preceding a node in an AVL tree.
 *
 * Gets the node with a key that precedes an existing node's key in an AVL
 * tree.
 *
 * @param node		Node to get preceding node of.
 * 
 * @return		Preceding node or NULL if none found.
 */
avl_tree_node_t *avl_tree_node_prev(avl_tree_node_t *node) {
	/* If there's a left-hand child, move onto it and then go as far
	 * right as we can. */
	if(node->left != NULL) {
		node = node->left;
		while(node->right) {
			node = node->right;
		}

		return node;
	} else {
		/* There's no left-hand children, go up until we find an
		 * ancestor that is the right-hand child of its parent. */
		while(node->parent && node == node->parent->left) {
			node = node->parent;
		}

		/* The parent will now point to the preceding node (or NULL,
		 * if we reach the top of the tree). */
		return node->parent;
	}
}

/** Get the node following a node in an AVL tree.
 *
 * Gets the node with a key that follows an existing node's key in an AVL
 * tree.
 *
 * @param node		Node to get following node of.
 * 
 * @return		Following node or NULL if none found.
 */
avl_tree_node_t *avl_tree_node_next(avl_tree_node_t *node) {
	/* If there's a right-hand child, move onto it and then go as far
	 * left as we can. */
	if(node->right != NULL) {
		node = node->right;
		while(node->left) {
			node = node->left;
		}

		return node;
	} else {
		/* There's no right-hand children, go up until we find an
		 * ancestor that is the left-hand child of its parent. */
		while(node->parent && node == node->parent->right) {
			node = node->parent;
		}

		/* The parent will now point to the following node (or NULL,
		 * if we reach the top of the tree). */
		return node->parent;
	}
}
