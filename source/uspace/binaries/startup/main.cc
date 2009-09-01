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

#include <kiwi/Process.h>

#include <string.h>

#include <cstdio>
#include <cstdlib>

using namespace kiwi;

extern "C" int module_load(const char *path, char *depbuf);

/** Load a module in a directory. */
static int load_module(const char *dir, const char *name) {
	char path[4096], depbuf[64];
	int ret;

	strcpy(path, dir);
	strcat(path, "/");
	strcat(path, name);

	while((ret = module_load(path, depbuf)) == -ERR_DEP_MISSING) {
		strcat(depbuf, ".km");
		if((ret = load_module(dir, depbuf)) != 0) {
			return ret;
		}
	}

	if(ret != 0) {
		if(ret != -ERR_ALREADY_EXISTS) {
			printf("startup: failed to load %s (%d)\n", path, ret);
		}
	} else {
		printf("startup: loaded module %s\n", path);
	}
	return ret;
}

/** Load all modules in the modules directory. */
static int load_modules(const char *dir) {
	fs_dir_entry_t *entry;
	handle_t handle;
	int ret;

	if((handle = fs_dir_open(dir, 0)) < 0) {
		printf("Failed to open module directory (%d)\n", handle);
		return handle;
	} else if(!(entry = reinterpret_cast<fs_dir_entry_t *>(malloc(4096)))) {
		printf("Failed to allocate directory entry buffer\n");
		return -ERR_NO_MEMORY;
	}

	while(true) {
		if((ret = fs_dir_read(handle, entry, 4096, -1)) != 0) {
			if(ret != -ERR_NOT_FOUND) {
				printf("Read module directory failed (%d)\n", ret);
				return ret;
			}

			free(entry);
			handle_close(handle);
			return 0;
		} else if(strcmp(entry->name, ".") == 0 || strcmp(entry->name, "..") == 0) {
			continue;
		}

		load_module(dir, entry->name);
	}
}

int main(int argc, char **argv) {
	Process *proc;
	int ret;

	if((ret = load_modules("/system/modules")) != 0) {
		return -ret;
	}

	ret = Process::create(proc, "failshell");
	if(ret != 0) {
		printf("Failed to create process (%d)\n", ret);
	} else {
		delete proc;
	}
	while(1);
}
