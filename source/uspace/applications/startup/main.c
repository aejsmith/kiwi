/* Kiwi userspace startup application
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
 * @brief		Userspace startup application.
 */

#include <kernel/aspace.h>
#include <kernel/fs.h>

#include <stdio.h>
#include <stdlib.h>

static inline void mount(const char *dev, const char *path, const char *type, int flags) {
	int ret;

	printf("Mounting %s on %s... ", type, path);
	ret = fs_mount(dev, path, type, 0);
	printf("%d\n", ret);
}

static inline void dir_create(const char *path) {
	int ret;

	printf("Creating directory %s... ", path);
	ret = fs_dir_create(path);
	printf("%d\n", ret);
}

static inline void file_create(const char *path) {
	int ret;

	printf("Creating file %s... ", path);
	ret = fs_file_create(path);
	printf("%d\n", ret);
}

int main(int argc, char **argv) {
	void *addr;
	int ret;

	printf("Hello from C userspace!\n");
	printf("This is a message!\n");

	ret = aspace_map_anon(NULL, 0x4000, ASPACE_MAP_READ | ASPACE_MAP_WRITE, &addr);
	printf("Map returned %d (%p)\n", ret, addr);
	if(ret == 0) {
		printf("Writing... 1234\n");
		*(int *)addr = 1234;
		printf("Reading... %d\n", *(int *)addr);
	}

	addr = malloc(4322);
	printf("Malloc returned %p\n", addr);
	if(addr) {
		printf("Writing... 1337\n");
		*(int *)addr = 1337;
		printf("Reading... %d\n", *(int *)addr);
	}

	mount(NULL, "/", "ramfs", 0);
	dir_create("/foo");
	file_create("/foo/bar.txt");
	file_create("/foo/../meow.txt");
	file_create("/foo/.././.././foo/.././././bar.txt");
	dir_create("/foo/bar");
	mount(NULL, "/foo/bar", "ramfs", 0);
	file_create("/foo/bar/../cow.txt");
	file_create("/foo/bar/./../bar/zoop.txt");
	file_create("/foo/bar/zoop.txt");
	dir_create("/foo/bar/meow");
	file_create("/foo/bar/meow/ohai.txt");
	while(1);
}
