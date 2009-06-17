/* Kiwi VFS internal functions/definitions
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
 * @brief		VFS internal functions/definitions.
 */

#ifndef __VFS_PRIV_H
#define __VFS_PRIV_H

#include <console/kprintf.h>

#include <fs/mount.h>
#include <fs/node.h>
#include <fs/type.h>

#include <mm/flags.h>

#include <errors.h>
#include <module.h>

#if CONFIG_VFS_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

extern vfs_mount_t *vfs_root_mount;

/** Filesystem type functions. */
extern vfs_type_t *vfs_type_lookup(const char *name, bool ref);

/** Filesystem node functions. */
extern vfs_node_t *vfs_node_alloc(const char *name, vfs_mount_t *mount, int mmflag);
extern int vfs_node_free(vfs_node_t *node, bool destroy);
extern void vfs_node_cache_init(void);

/** Filesystem mount functions/variables. */
extern void vfs_mount_reclaim_nodes(void);

#endif /* __VFS_PRIV_H */
