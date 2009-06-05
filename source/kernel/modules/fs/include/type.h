/* Kiwi VFS filesystem type management
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
 * @brief		VFS filesystem type management.
 */

#ifndef __FS_TYPE_H
#define __FS_TYPE_H

#include <fs/filesystem.h>
#include <fs/node.h>

#include <mm/flags.h>

#include <types/list.h>
#include <types/refcount.h>

/** Filesystem type description structure. */
typedef struct vfs_type {
	list_t header;			/**< Link to types list. */

	const char *name;		/**< Name of the FS type. */
	refcount_t count;		/**< Reference count of mounts using this FS type. */
	int flags;			/**< Flags specifying various traits about this FS type. */

	/**
	 * Main operations.
	 */

	/** Check whether a device contains this filesystem type.
	 * @note		If a filesystem type does not provide this
	 *			function, then it is assumed that the FS does
	 *			not use a backing device (e.g. RamFS).
	 * @param dev		Device to check.
	 * @return		True if the device contains a FS of this type,
	 *			false if not. */
	//bool (*check)(device_t *dev);

	/** Set up an instance of this filesystem.
	 * @note		It is guaranteed that the device will contain
	 *			the correct FS type when this is called, as
	 *			the check operation is called prior to this.
	 * @param fs		Filesystem structure for the FS. This structure
	 *			will contain a pointer to the device the FS
	 *			resides on (will be NULL if no source).
	 * @return		0 on success, negative error code on failure. */
	int (*create)(vfs_filesystem_t *fs);

	/** Destroy an instance of this filesystem.
	 * @note		To filesystem driver writers: this does not
	 *			mean literally destroy the filesystem.
	 * @param fs		Filesystem structure being deleted.
	 * @return		0 on success, negative error code on failure. */
	int (*destroy)(vfs_filesystem_t *fs);

	/**
	 * Page manipulation functions.
	 */

	/** Get a page to use for a node's data.
	 * @note		If this operation is not provided, then the
	 *			VFS will allocate an anonymous, zeroed page
	 *			to use for node data.
	 * @param node		Node to get page for.
	 * @param offset	Offset within the node the page is for.
	 * @param mmflag	Allocation flags.
	 * @param physp		Where to store address of page obtained.
	 * @return		0 on success, negative error code on failure. */
	int (*get_page)(vfs_node_t *node, offset_t offset, int mmflag, phys_ptr_t *physp);

	/** Read a page from a node.
	 * @note		If the page straddles across the end of the
	 *			file, then only the part of the file that
	 *			exists will be read.
	 * @note		If this operation is not provided by a FS
	 *			type, then it is assumed that the page given
	 *			by the get_page operation already contains the
	 *			correct data. The reason this operation is
	 *			provided rather than just having data read
	 *			in by the get_page operation is so that the
	 *			FS implementation does not always have to deal
	 *			with mapping and unmapping physical memory.
	 * @param node		Node being read from.
	 * @param page		Pointer to mapped page to read into.
	 * @param offset	Offset within the file to read from.
	 * @param nonblock	Whether the read is required to not block.
	 * @return		0 on success, negative error code on failure. */
	int (*read_page)(vfs_node_t *node, void *page, offset_t offset, bool nonblock);

	/** Flush changes to a page within a node.
	 * @note		If the page straddles across the end of the
	 *			file, then only the part of the file that
	 *			exists will be written back. If it is desired
	 *			to resize the file, the resize operation must
	 *			be called.
	 * @note		If this operation is not provided, then it
	 *			is assumed that modified pages should always
	 *			remain in the cache until its destruction (for
	 *			example, RamFS does this).
	 * @param node		Node being written to.
	 * @param page		Pointer to mapped page being written.
	 * @param offset	Offset within the file to write to.
	 * @param nonblock	Whether the write is required to not block.
	 * @return		0 on success, negative error code on failure. */
	int (*flush_page)(vfs_node_t *node, void *page, offset_t offset, bool nonblock);

	/** Free a page previously obtained via get_page.
	 * @param node		Node that page was used for.
	 * @param page		Address of page to free. */
	int (*free_page)(vfs_node_t *node, phys_ptr_t page);

	/**
	 * Node modification functions.
	 */

	/** Modify the size of a node.
	 * @param node		Node being resized.
	 * @param size		New size of the file.
	 * @return		0 on success, negative error code on failure. */
	int (*resize)(vfs_node_t *node, file_size_t size);

	/** Find a child node.
	 * @param parent	Node to search under.
	 * @param node		Node structure to fill with information about
	 *			child. The name of the child to search for is
	 *			stored in this structure when this function is
	 *			called.
	 * @return		0 on success, negative error code on failure. */
	int (*find_node)(vfs_node_t *parent, vfs_node_t *node);

	/** Clean up data associated with a node.
	 * @param node		Node to clean up. */
	void (*free_node)(vfs_node_t *node);
} vfs_type_t;

/** Filesystem trait flags. */
#define VFS_TYPE_RDONLY		(1<<0)	/**< Filesystem type is read-only. */

extern vfs_type_t *vfs_type_lookup(const char *name);
extern int vfs_type_register(vfs_type_t *type);
extern int vfs_type_unregister(vfs_type_t *type);

#endif /* __FS_TYPE_H */
