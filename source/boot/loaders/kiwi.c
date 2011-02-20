/*
 * Copyright (C) 2010-2011 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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

#include <lib/string.h>
#include <lib/utility.h>

#include <cpu.h>
#include <fs.h>
#include <kargs.h>
#include <loader.h>
#include <memory.h>
#include <ui.h>
#include <video.h>

/** Array of boot paths. */
static const char *kiwi_boot_dirs[] = {
	"/system/boot",
	"/kiwi",
};

/** Variable to wait to enter the kernel on. */
static atomic_t ap_kernel_wait = 0;

/** Load the Kiwi kernel image.
 * @param path		Path to entry to load.
 * @param env		Environment for the kernel. */
static void kiwi_loader_load_kernel(const char *path, environ_t *env) {
	fs_handle_t *handle;

	kprintf("Loading kernel...\n", path);

	if(!(handle = fs_open(NULL, path))) {
		boot_error("Could not find kernel image %s", path);
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
		boot_error("Could not read module %s", name);
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
		boot_error("Could not find module directory %s", path);
	} else if(!handle->directory) {
		boot_error("Module directory %s not directory", path);
	} else if(!handle->mount->type->read_dir) {
		boot_error("Cannot use module directory on non-listable FS");
	}

	if(!fs_dir_read(handle, load_modules_cb, NULL)) {
		boot_error("Failed to iterate module directory");
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

	boot_error("Could not find Kiwi boot directory");
}

/** AP entry function for booting a Kiwi kernel. */
static void __noreturn kiwi_loader_ap_entry(void) {
	while(!atomic_get(&ap_kernel_wait));
	kiwi_loader_arch_enter();
}

/** Load Kiwi.
 * @param env		Environment for the entry. */
static void __noreturn kiwi_loader_load(environ_t *env) {
	fs_handle_t *handle;
	video_mode_t *mode;
	value_t *value;
	size_t i;

	/* Pull settings out of the environment into the kernel arguments. */
	value = environ_lookup(env, "smp_disabled");
	kernel_args->smp_disabled = value->boolean;
	value = environ_lookup(env, "splash_disabled");
	kernel_args->splash_disabled = value->boolean;
	value = environ_lookup(env, "force_fsimage");
	kernel_args->force_fsimage = value->boolean;

	/* Work out where to load everything. */
	if((value = environ_lookup(env, "kiwi_kernel"))) {
		kiwi_loader_load_kernel(value->string, env);
		if((value = environ_lookup(env, "kiwi_module_list"))) {
			for(i = 0; i < value->list->count; i++) {
				if(!(handle = fs_open(NULL, value->list->values[i].string))) {
					boot_error("Could not open module %s", value->list->values[i].string);
				}
				kiwi_loader_load_module(
					handle,
		                        strrchr(value->list->values[i].string, '/') + 1
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

	/* If SMP is enabled, detect and boot secondary CPUs. */
	if(!kernel_args->smp_disabled) {
		cpu_detect();
	}
	cpu_boot_all(kiwi_loader_ap_entry);

	/* Set the video mode. */
	value = environ_lookup(env, "video_mode");
	mode = value->pointer;
	video_enable(mode);
	kernel_args->fb_width = mode->width;
	kernel_args->fb_height = mode->height;
	kernel_args->fb_depth = mode->bpp;
	kernel_args->fb_addr = mode->addr;

	/* Write final details to the kernel arguments structure. */
	strncpy(kernel_args->boot_fs_uuid, current_disk->fs->uuid, KERNEL_ARGS_UUID_LEN);
	kernel_args->boot_fs_uuid[KERNEL_ARGS_UUID_LEN - 1] = 0;
	kernel_args->boot_cpu = cpu_current_id();
	memory_finalise();

	/* Enter the kernel. */
	atomic_inc(&ap_kernel_wait);
	kiwi_loader_arch_enter();
}

/** Display a configuration menu.
 * @param env		Environment for the entry. */
static void kiwi_loader_configure(environ_t *env) {
	ui_window_t *window = ui_list_create("Kiwi Configuration", true);
	ui_list_insert(window, video_mode_chooser("Video mode", environ_lookup(env, "video_mode")), false);
	ui_list_insert_env(window, env, "smp_disabled", "Disable SMP", false);
	ui_list_insert_env(window, env, "splash_disabled", "Show boot messages", false);
	ui_list_insert_env(window, env, "force_fsimage", "Force filesystem image usage", false);
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
	video_mode_t *mode = NULL;
	value_t value, *exist;

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
	if(!environ_lookup(env, "smp_disabled")) {
		value.type = VALUE_TYPE_BOOLEAN;
		value.boolean = false;
		environ_insert(env, "smp_disabled", &value);
	}
	if(!environ_lookup(env, "splash_disabled")) {
		value.type = VALUE_TYPE_BOOLEAN;
		value.boolean = false;
		environ_insert(env, "splash_disabled", &value);
	}
	if(!environ_lookup(env, "force_fsimage")) {
		value.type = VALUE_TYPE_BOOLEAN;
		value.boolean = false;
		environ_insert(env, "force_fsimage", &value);
	}
	if((exist = environ_lookup(env, "video_mode")) && exist->type == VALUE_TYPE_STRING) {
		mode = video_mode_find_string(exist->string);
	}
	value.type = VALUE_TYPE_POINTER;
	value.pointer = (mode) ? mode : default_video_mode;
	environ_insert(env, "video_mode", &value);

	kiwi_loader_arch_setup(env);
	return true;
}
