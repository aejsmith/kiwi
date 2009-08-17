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

#include <kernel/fs.h>
#include <kernel/handle.h>
#include <kernel/process.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static inline void mount(const char *dev, const char *path, const char *type, int flags) {
	int ret;

	if((ret = fs_mount(dev, path, type, 0)) != 0) {
		printf("Mount %s on %s failed (%d)\n", type, path, ret);
	}
}

static inline void dir_create(const char *path) {
	int ret;

	if((ret = fs_dir_create(path)) != 0) {
		printf("Create directory %s failed (%d)\n", path, ret);
	}
}

static inline void file_create(const char *path) {
	int ret;

	if((ret = fs_file_create(path)) != 0) {
		printf("Create file %s failed (%d)\n", path, ret);
	}
}

static inline void symlink_create(const char *path, const char *target) {
	int ret;

	if((ret = fs_symlink_create(path, target)) != 0) {
		printf("Create symbolic link %s -> %s failed(%d)\n", path, target, ret);
	}
}

static void dump_tree(char *name, int indent) {
	fs_dir_entry_t *entry;
	char link[4096];
	handle_t handle;
	int ret;

	if(name) {
		ret = fs_setcwd(name);
		if(ret != 0) {
			if(ret != -11) {
				printf("Setcwd failed: %d\n", ret);
				while(1);
			}
			return;
		}
	}

	handle = fs_dir_open(".", 0);
	if(handle < 0) {
		printf("Failed to open: %d\n", handle);
		while(1);
	}

	entry = malloc(4096);
	if(!entry) {
		printf("Malloc failed\n");
		while(1);
	}

	while(true) {
		ret = fs_dir_read(handle, entry, 4096, -1);
		if(ret != 0) {
			if(ret != -10) {
				printf("Read failed: %d\n", ret);
				while(1);
			}
			free(entry);
			if(name) {
				ret = fs_setcwd("..");
				if(ret != 0) {
					printf("Setcwd .. failed: %d", ret);
					while(1);
				}
			}
			handle_close(handle);
			return;
		}

		ret = fs_symlink_read(entry->name, link, 4096);
		if(ret > 0) {
			printf("%-2d - %*s%s -> %s\n", entry->id, indent, "", entry->name, link);
		} else {
			printf("%-2d - %*s%s\n", entry->id, indent, "", entry->name);
			if(strcmp(entry->name, ".") != 0 && strcmp(entry->name, "..") != 0) {
				dump_tree(entry->name, indent + 2);
			}
		}
	}
}

static char array[512];

extern void putch(char ch);

static __attribute__((used)) int putchar(int ch) {
	putch((char)ch);
	return ch;
}

extern char **environ;

int main(int argc, char **argv) {
	char *nargs[] = { (char *)"/system/binaries/hello", (char *)"--hello", (char *)"world", NULL };
	char buf[] = { 'a', 'b', 'c', 'd' };
	handle_t handle;
	size_t bytes;
	int ret, i;

	printf("I'm process %d! My arguments are:\n", process_id(-1));
	for(i = 0; i < argc; i++) {
		printf(" argv[%d] = '%s'\n", i, argv[i]);
	}

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

	handle = fs_file_open("/foo/bar.txt", FS_FILE_READ | FS_FILE_WRITE);
	if(handle >= 0) {
		ret = fs_file_write(handle, buf, sizeof(buf), -1, &bytes);
		ret = fs_file_write(handle, buf, sizeof(buf), -1, &bytes);
		ret = fs_file_write(handle, buf, sizeof(buf), -1, &bytes);

		if((ret = fs_handle_seek(handle, FS_HANDLE_SEEK_SET, 0, NULL)) != 0) {
			printf("Seek(%d) failed (%d)\n", handle, ret);
		} else if((ret = fs_file_read(handle, array, sizeof(array), -1, &bytes)) == 0) {
			array[bytes] = 0;
			printf("Read(%d) returned %d (%zu): '%s'\n", handle, ret, bytes, array);
		} else {
			printf("Read(%d) failed %d (%zu)\n", handle, ret, bytes);
		}

		handle_close(handle);
	}

	symlink_create("/foo/bar/link.txt", "/foo/bar.txt");
	symlink_create("/foo/bar/linkdir", "./..///../..////./foo/bar/../");

	handle = fs_file_open("/foo/bar/link.txt", FS_FILE_READ);
	if(handle >= 0) {
		if((ret = fs_file_read(handle, array, sizeof(array), -1, &bytes)) == 0) {
			array[bytes] = 0;
			printf("Read(%d) returned %d (%zu): '%s'\n", handle, ret, bytes, array);
		} else {
			printf("Read(%d) failed %d (%zu)\n", handle, ret, bytes);
		}

		handle_close(handle);
	} else {
		printf("Failed to open: %d\n", handle);
	}

	handle = fs_file_open("/foo/bar/linkdir/bar.txt", FS_FILE_READ);
	if(handle >= 0) {
		if((ret = fs_file_read(handle, array, sizeof(array), -1, &bytes)) == 0) {
			array[bytes] = 0;
			printf("Handle %d, read returned %d (%zu): '%s'\n", handle, ret, bytes, array);
		} else {
			printf("Handle %d, read failed %d (%zu)\n", handle, ret, bytes);
		}

		handle_close(handle);
	} else {
		printf("Failed to open: %d\n", handle);
	}

	handle = fs_file_open("/meow.txt", FS_FILE_READ);
	fs_unlink("/meow.txt");
	printf("Unlinked, closing...\n");
	handle_close(handle);

	printf("Directory tree:\n");
	dump_tree(NULL, 0);

	handle = process_create(nargs[0], nargs, environ, true);
	printf("Create process returned %d (%d)\n", handle, process_id(handle));
}
