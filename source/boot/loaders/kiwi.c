/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Kiwi kernel loader.
 *
 * There are 4 forms of the 'kiwi' configuration command:
 *  - kiwi <kernel path> <module list>
 *    Loads the specified kernel and all modules specified in the given list.
 *  - kiwi <kernel path> <module dir>
 *    Loads the specified kernel and all modules in the given directory.
 *  - kiwi <boot directory>
 *    Uses the file named 'kernel' as the kernel and the directory named
 *    'modules' as the module directory from the specified directory.
 *  - kiwi
 *    Same as above but auto-detects the boot directory.
 */

#include <boot/loader.h>
#include <boot/memory.h>
#include <boot/fs.h>
#include <boot/ui.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <kargs.h>

/** Array of boot paths. */
static const char *kiwi_boot_dirs[] = {
	"/system/boot",
	"/kiwi",
};

/** Load the Kiwi kernel image.
 * @param path		Path to entry to load.
 * @param env		Environment for the kernel. */
static void kiwi_loader_load_kernel(const char *path, environ_t *env) {
	fs_handle_t *handle;

	kprintf("Loading kernel...\n", path);

	if(!(handle = fs_open(NULL, path))) {
		fatal("Could not find kernel image (%s)", path);
	}

	kiwi_loader_arch_load(handle, env);
	fs_close(handle);
}

/** Load a single Kiwi module.
 * @param handle	Handle to module to load.
 * @param name		Name of the module. */
static void kiwi_loader_load_module(fs_handle_t *handle, const char *name) {
	phys_ptr_t addr;
	offset_t size;

	if(handle->directory) {
		return;
	}

	kprintf("Loading %s...\n", name);

	/* Allocate a chunk of memory to load to. */
	size = fs_file_size(handle);
	addr = phys_memory_alloc(ROUND_UP(size, PAGE_SIZE), PAGE_SIZE, true);
	if(!fs_file_read(handle, (void *)((ptr_t)addr), size, 0)) {
		fatal("Could not read module %s", name);
	}

	/* Add the module to the kernel arguments. */
	kargs_module_add(addr, size);
	dprintf("loader: loaded module %s to 0x%" PRIpp " (size: %" PRIu64 ")\n",
	        name, addr, size);
}

/** Callback to load a module from a directory.
 * @param name		Name of the entry.
 * @param handle	Handle to entry.
 * @param data		Data argument passed to fs_dir_read().
 * @return		Whether to continue iteration. */
static bool load_modules_cb(const char *name, fs_handle_t *handle, void *arg) {
	kiwi_loader_load_module(handle, name);
	return true;
}

/** Load a directory of Kiwi modules.
 * @param path		Path to directory. */
static void kiwi_loader_load_modules(const char *path) {
	fs_handle_t *handle;

	if(!(handle = fs_open(NULL, path))) {
		fatal("Could not find module directory (%s)", path);
	} else if(!handle->directory) {
		fatal("Module directory not directory (%s)", path);
	} else if(!handle->mount->type->read_dir) {
		fatal("Cannot use module directory on non-listable FS");
	}

	if(!fs_dir_read(handle, load_modules_cb, NULL)) {
		fatal("Failed to iterate module directory");
	}

	fs_close(handle);
}

/** Load from a Kiwi boot directory.
 * @param path		Path to directory.
 * @param env		Environment for the kernel. */
static void kiwi_loader_load_dir(const char *path, environ_t *env) {
	char *tmp;

	dprintf("loader: loading from boot directory %s\n", path);

	/* Load the kernel. */
	tmp = kmalloc(strlen(path) + strlen("/kernel") + 1);
	strcpy(tmp, path);
	strcat(tmp, "/kernel");
	kiwi_loader_load_kernel(tmp, env);
	kfree(tmp);

	/* Load the modules. */
	tmp = kmalloc(strlen(path) + strlen("/modules") + 1);
	strcpy(tmp, path);
	strcat(tmp, "/modules");
	kiwi_loader_load_modules(tmp);
	kfree(tmp);
}

/** Attempt to auto-detect the boot directory.
 * @param env		Environment for the kernel. */
static void kiwi_loader_detect_dir(environ_t *env) {
	fs_handle_t *handle;
	size_t i;

	for(i = 0; i < ARRAYSZ(kiwi_boot_dirs); i++) {
		handle = fs_open(NULL, kiwi_boot_dirs[i]);
		if(handle) {
			if(handle->directory) {
				fs_close(handle);
				kiwi_loader_load_dir(kiwi_boot_dirs[i], env);
				return;
			}
			fs_close(handle);
		}
	}

	fatal("Could not find Kiwi boot directory");
}

/** Load Kiwi.
 * @param env		Environment for the entry. */
static void __noreturn kiwi_loader_load(environ_t *env) {
	fs_handle_t *handle;
	value_t *value;
	size_t i;

	/* Pull settings out of the environment into the kernel arguments. */
	if((value = environ_lookup(env, "splash_disabled"))) {
		kernel_args->splash_disabled = value->integer;
	}
	if((value = environ_lookup(env, "smp_disabled"))) {
		kernel_args->smp_disabled = value->integer;
	}

	/* Work out where to load everything. */
	if((value = environ_lookup(env, "kiwi_kernel"))) {
		kiwi_loader_load_kernel(value->string, env);
		if((value = environ_lookup(env, "kiwi_module_list"))) {
			for(i = 0; i < value->list->count; i++) {
				if(!(handle = fs_open(NULL, value->list->values[i].string))) {
					fatal("Could not open module %s", value->list->values[i].string);
				}
				kiwi_loader_load_module(
					handle,
		                        strrchr(value->list->values[i].string, '/') - 1
				);
				fs_close(handle);
			}
		} else if((value = environ_lookup(env, "kiwi_module_dir"))) {
			kiwi_loader_load_modules(value->string);
		}
	} else if((value = environ_lookup(env, "kiwi_dir"))) {
		kiwi_loader_load_dir(value->string, env);
	} else {
		kiwi_loader_detect_dir(env);
	}

	while(1);
}

/** Display a configuration menu.
 * @param env		Environment for the entry. */
static void kiwi_loader_configure(environ_t *env) {
	ui_window_t *window = ui_list_create("Kiwi Configuration", true);
	ui_list_insert_env(window, env, "splash_disabled", "Disable splash screen", false);
	ui_list_insert_env(window, env, "smp_disabled", "Disable SMP", false);
	kiwi_loader_arch_configure(env, window);
	ui_window_display(window, 0);
}

/** Kiwi kernel loader type. */
static loader_type_t kiwi_loader_type = {
	.load = kiwi_loader_load,
	.configure = kiwi_loader_configure,
};

/** Macro to make the code below a little nicer. */
#define vtype(a, i, t)	((a)->values[(i)].type == (t))

/** Load a Kiwi kernel.
 * @param args		Command arguments.
 * @param env		Environment for the command.
 * @return		Whether completed successfully. */
bool config_cmd_kiwi(value_list_t *args, environ_t *env) {
	value_t value;

	if(args->count == 2 && vtype(args, 0, VALUE_TYPE_STRING) && vtype(args, 1, VALUE_TYPE_LIST)) {
		environ_insert(env, "kiwi_kernel", &args->values[0]);
		environ_insert(env, "kiwi_module_list", &args->values[1]);
	} else if(args->count == 2 && vtype(args, 0, VALUE_TYPE_STRING) && vtype(args, 0, VALUE_TYPE_STRING)) {
		environ_insert(env, "kiwi_kernel", &args->values[0]);
		environ_insert(env, "kiwi_module_dir", &args->values[1]);
	} else if(args->count == 1 && vtype(args, 0, VALUE_TYPE_STRING)) {
		environ_insert(env, "kiwi_dir", &args->values[0]);
	} else if(args->count != 0) {
		dprintf("config: kiwi: invalid arguments\n");
		return false;
	}

	/* Set the loader type. */
	loader_type_set(env, &kiwi_loader_type);

	/* Add in configuration items. */
	if(!environ_lookup(env, "splash_disabled")) {
		value.type = VALUE_TYPE_INTEGER;
		value.integer = 0;
		environ_insert(env, "splash_disabled", &value);
	}
	if(!environ_lookup(env, "smp_disabled")) {
		value.type = VALUE_TYPE_INTEGER;
		value.integer = 0;
		environ_insert(env, "smp_disabled", &value);
	}
	kiwi_loader_arch_setup(env);
	return true;
}
