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

#include <kernel/errors.h>
#include <kernel/fs.h>
#include <kernel/handle.h>
#include <kernel/process.h>

#include <string.h>
#include <stdlib.h>

#include <cstdio>
#include <new>

/** Class to print a directory tree. */
class DirTreePrinter {
public:
	DirTreePrinter() : m_indent(0) {}

	/** Print out a directory tree starting from a directory.
	 * @param name		Name of directory to print.
	 * @return		0 on sucess, negative error code on failure. */
	int print(const char *name) {
		fs_dir_entry_t *entry;
		handle_t handle;
		char *link;
		int ret;

		ret = fs_setcwd(name);
		if(ret != 0) {
			if(ret != -ERR_TYPE_INVAL) {
				printf("Setcwd failed: %d\n", ret);
				return ret;
			}
			return 0;
		}

		handle = fs_dir_open(".", 0);
		if(handle < 0) {
			printf("Failed to open: %d\n", handle);
			return handle;
		}

		entry = reinterpret_cast<fs_dir_entry_t *>(malloc(4096));
		if(!entry) {
			printf("Malloc failed\n");
			return -ERR_NO_MEMORY;
		}

		while(true) {
			ret = fs_dir_read(handle, entry, 4096, -1);
			if(ret != 0) {
				if(ret != -ERR_NOT_FOUND) {
					printf("Read failed: %d\n", ret);
					return ret;
				}
				free(entry);
				if(strcmp(name, "/") != 0) {
					ret = fs_setcwd("..");
					if(ret != 0) {
						printf("Setcwd .. failed: %d", ret);
						return ret;
					}
				}
				handle_close(handle);
				return 0;
			}

			link = new char[4096];
			ret = fs_symlink_read(entry->name, link, 4096);
			if(ret > 0) {
				printf("%-2d - %*s%s -> %s\n", entry->id, m_indent, "", entry->name, link);
				delete link;
			} else {
				delete link;
				printf("%-2d - %*s%s\n", entry->id, m_indent, "", entry->name);
				if(strcmp(entry->name, ".") != 0 && strcmp(entry->name, "..") != 0) {
					m_indent += 2;
					ret = print(entry->name);
					m_indent -= 2;

					if(ret != 0) {
						return ret;
					}
				}
			}
		}
	}
private:
	int m_indent;
};

extern char **environ;

int main(int argc, char **argv) {
	char *nargs[] = { (char *)"/system/binaries/hello", (char *)"--hello", (char *)"world", NULL };
	DirTreePrinter printer;
	handle_t handle;
	int i;

	printf("I'm process %d! My arguments are:\n", process_id(-1));
	for(i = 0; i < argc; i++) {
		printf(" argv[%d] = '%s'\n", i, argv[i]);
	}

	printf("Directory tree:\n");
	printer.print("/");

	handle = process_create(nargs, environ, true);
	printf("Create process returned %d (%d)\n", handle, process_id(handle));
	handle_close(handle);
	while(1);
}
