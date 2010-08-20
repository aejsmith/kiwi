/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		Directory handling functions.
 */

#ifndef __DIRENT_PRIV_H
#define __DIRENT_PRIV_H

#include <kernel/fs.h>
#include <kernel/object.h>
#include <kernel/status.h>

#include <dirent.h>

#include "../libc.h"

/** Size of the internal directory entry buffer. */
#define DIRSTREAM_BUF_SIZE	0x1000

struct __dstream_internal {
	handle_t handle;		/**< Handle to the directory. */
	char buf[DIRSTREAM_BUF_SIZE];	/**< Buffer for entry structures. */
};

#endif /* __DIRENT_PRIV_H */
