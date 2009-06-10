/* Kiwi userspace initialization module
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
 * @brief		Userspace initialization module.
 *
 * This module contains the code used to set up a userspace environment.
 * It sets up the root filesystem and runs the userspace startup program.
 */

#include <console/kprintf.h>

#include <fs/mount.h>
#include <fs/node.h>

#include <errors.h>
#include <fatal.h>
#include <module.h>

static const char *hello_str = "Hello, World!";
static char test_read_buf[512];

/** Userspace initialization function.
 * @return		0 on success, negative error code on failure. */
static int uinit_init(void) {
	vfs_node_t *parent, *child;
	size_t bytes = 0;
	int ret;

	kprintf(LOG_DEBUG, "uinit: setting up userspace environment...\n");

	/* Mount RamFS as root filesystem. */
	ret = vfs_mount_create("ramfs", 0, &vfs_root_mount);
	if(ret != 0) {
		fatal("Failed to mount root filesystem: %d", ret);
	}

	/* VFS test. */
	ret = vfs_node_lookup(NULL, "", &parent);
	kprintf(LOG_NORMAL, "lookup returned %d 0x%p\n", ret, parent);
	if(ret != 0) {
		return 0;
	}

	ret = vfs_node_create(parent, "foo.txt", VFS_NODE_REGULAR, &child);
	kprintf(LOG_NORMAL, "create returned %d 0x%p\n", ret, child);
	if(ret != 0) {
		return 0;
	}

	ret = vfs_node_write(child, hello_str, strlen(hello_str), 0, &bytes);
	kprintf(LOG_NORMAL, "write returned %d %" PRIs "\n", ret, bytes);
	if(ret != 0) {
		return 0;
	}

	kprintf(LOG_NORMAL, "wrote: '%s'\n", hello_str);

	ret = vfs_node_read(child, test_read_buf, strlen(hello_str), 7, &bytes);
	kprintf(LOG_NORMAL, "read returned %d %" PRIs "\n", ret, bytes);
	if(ret != 0) {
		return 0;
	}

	test_read_buf[13] = 0;
	kprintf(LOG_NORMAL, "read: '%s'\n", test_read_buf);

	/* TODO: Should be able to signal that this module can be unloaded
	 * because it does not need to remain in memory - it just starts off
	 * a process. */
	return 0;
}

MODULE_NAME("uinit");
MODULE_DESC("Userspace initialization module.");
MODULE_FUNCS(uinit_init, NULL);
MODULE_DEPS("vfs", "ramfs");
