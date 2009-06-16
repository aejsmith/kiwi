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

#ifndef __LOADER_PRIV_H
#define __LOADER_PRIV_H

#include <console/kprintf.h>

#include <loader/binary.h>
#include <loader/elf.h>
#include <loader/type.h>

#include <errors.h>
#include <module.h>

#if CONFIG_LOADER_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

extern loader_type_t loader_elf_type;

extern loader_type_t *loader_type_match(vfs_node_t *node);

#endif /* __LOADER_PRIV_H */
