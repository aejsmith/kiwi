/*
 * Copyright (C) 2011 Alex Smith
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
 * @brief		KBoot kernel loader.
 *
 * There are 2 forms of the 'kboot' configuration command:
 *  - kboot <kernel path> <module list>
 *    Loads the specified kernel and all modules specified in the given list.
 *  - kboot <kernel path> <module dir>
 *    Loads the specified kernel and all modules in the given directory.
 */

#include <arch/mmu.h>

#include <lib/string.h>

#include <assert.h>
#include <config.h>
#include <console.h>
#include <elf.h>
#include <fs.h>
#include <kboot.h>
#include <loader.h>
#include <memory.h>
#include <ui.h>
#include <video.h>

/** Macro to make the code a little nicer. */
#define vtype(a, i, t)	((a)->values[(i)].type == (t))

/** Data for the KBoot loader. */
typedef struct kboot_data {
	environ_t *env;			/**< Environment back pointer. */
	fs_handle_t *kernel;		/**< Handle to the kernel image. */
	bool is_kboot;			/**< Whether the image is a KBoot image. */
	value_t modules;		/**< Modules to load. */
	kboot_tag_t *tags;		/**< Start of the tag list. */
	ui_window_t *config;		/**< Configuration window. */
} kboot_data_t;

extern mmu_context_t *kboot_arch_load(fs_handle_t *handle);

/** Allocate a tag in the tag list.
 * @param data		Loader data structure.
 * @param type		Type of the tag.
 * @param size		Size of the tag.
 * @return		Address of allocated tag. */
static void *allocate_tag(kboot_data_t *data, uint32_t type, size_t size) {
	kboot_tag_t *tag, *exist;

	assert(size >= sizeof(kboot_tag_t));

	tag = kmalloc(size);
	tag->next = 0;
	tag->type = type;

	/* Add at the end of the list. */
	if(data->tags) {
		for(exist = data->tags; exist->next; exist = (kboot_tag_t *)((ptr_t)exist->next));
		exist->next = (ptr_t)tag;
	} else {
		data->tags = tag;
	}

	return tag;
}

/** Load a single module.
 * @param data		Loader data structure.
 * @param handle	Handle to module to load.
 * @param name		Name of the module. */
static void load_module(kboot_data_t *data, fs_handle_t *handle, const char *name) {
	kboot_tag_module_t *tag;
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

	/* Add the module to the tag list. */
	tag = allocate_tag(data, KBOOT_TAG_MODULE, sizeof(*tag));
	tag->addr = addr;
	tag->size = size;

	dprintf("kboot: loaded module %s to 0x%" PRIpp " (size: %" PRIu64 ")\n",
	        name, addr, size);
}

/** Load a list of modules.
 * @param data		Loader data structure.
 * @param list		List to load. */
static void load_module_list(kboot_data_t *data, value_list_t *list) {
	fs_handle_t *handle;
	size_t i;

	for(i = 0; i < list->count; i++) {
		handle = fs_open(NULL, list->values[i].string);
		if(!handle) {
			boot_error("Could not open module %s", list->values[i].string);
		}

		load_module(data, handle, strrchr(list->values[i].string, '/') + 1);
		fs_close(handle);
	}
}

/** Callback to load a module from a directory.
 * @param name		Name of the entry.
 * @param handle	Handle to entry.
 * @param data		Data argument passed to fs_dir_read().
 * @return		Whether to continue iteration. */
static bool load_modules_cb(const char *name, fs_handle_t *handle, void *arg) {
	load_module(arg, handle, name);
	return true;
}

/** Load a directory of modules.
 * @param data		Loader data structure.
 * @param path		Path to directory. */
static void load_module_dir(kboot_data_t *data, const char *path) {
	fs_handle_t *handle;

	if(!(handle = fs_open(NULL, path))) {
		boot_error("Could not find module directory %s", path);
	} else if(!handle->directory) {
		boot_error("Module directory %s not directory", path);
	} else if(!handle->mount->type->read_dir) {
		boot_error("Cannot use module directory on non-listable FS");
	}

	if(!fs_dir_read(handle, load_modules_cb, data)) {
		boot_error("Failed to iterate module directory");
	}

	fs_close(handle);
}

/** Load the operating system.
 * @param env		Environment for the OS. */
static __noreturn void kboot_loader_load(environ_t *env) {
	kboot_data_t *data = loader_data_get(env);
	mmu_context_t *mmu;

	/* We don't report these errors until the user actually tries to run a
	 * menu entry. */
	if(!data->kernel) {
		boot_error("Could not find kernel image");
	} else if(!data->is_kboot) {
		boot_error("Kernel is not a valid KBoot kernel");
	}

	/* Load the kernel image into memory. */
	kprintf("Loading kernel...\n");
	mmu = kboot_arch_load(data->kernel);

	/* Load modules. */
	if(data->modules.type == VALUE_TYPE_LIST) {
		load_module_list(data, data->modules.list);
	} else if(data->modules.type == VALUE_TYPE_STRING) {
		load_module_dir(data, data->modules.string);
	}

	while(1);
}

/** Display a configuration menu.
 * @param env		Environment for the OS. */
static void kboot_loader_configure(environ_t *env) {
	kboot_data_t *data = loader_data_get(env);
	ui_window_display(data->config, 0);
}

/** KBoot loader type. */
static loader_type_t kboot_loader_type = {
	.load = kboot_loader_load,
	.configure = kboot_loader_configure,
};

/** Tag iterator to add configuration settings to the environment.
 * @param note		Note header.
 * @param name		Note name.
 * @param desc		Note data.
 * @param _data		KBoot loader data pointer.
 * @return		Whether to continue iteration. */
static bool add_config_tags(elf_note_t *note, const char *name, void *desc, void *_data) {
	const char *opt_name, *opt_desc;
	kboot_itag_option_t *option;
	kboot_data_t *data = _data;
	kboot_itag_image_t *image;
	video_mode_t *mode = NULL;
	value_t *exist, value;
	void *opt_default;

	if(strcmp(name, "KBoot") != 0) {
		return true;
	}

	switch(note->n_type) {
	case KBOOT_ITAG_IMAGE:
		image = desc;

		if(data->is_kboot) {
			dprintf("kboot: warning: image contains multiple image tags, ignoring extras\n");
			break;
		}
		data->is_kboot = true;

		/* If the kernel wants a video mode, add a video mode chooser. */
		if(image->flags & KBOOT_IMAGE_LFB) {
			if((exist = environ_lookup(data->env, "video_mode")) && exist->type == VALUE_TYPE_STRING) {
				mode = video_mode_find_string(exist->string);
			}
			value.type = VALUE_TYPE_POINTER;
			value.pointer = (mode) ? mode : default_video_mode;
			environ_insert(data->env, "video_mode", &value);

			ui_list_insert(data->config, video_mode_chooser("Video mode",
				environ_lookup(data->env, "video_mode")), false);
		}

		break;
	case KBOOT_ITAG_OPTION:
		option = desc;
		opt_name = desc + sizeof(*option);
		opt_desc = desc + sizeof(*option) + option->name_len;
		opt_default = desc + sizeof(*option) + option->name_len + option->desc_len;

		switch(option->type) {
		case KBOOT_OPTION_BOOLEAN:
			value.type = VALUE_TYPE_BOOLEAN;
			value.boolean = *(bool *)opt_default;
			break;
		case KBOOT_OPTION_STRING:
			value.type = VALUE_TYPE_STRING;
			value.string = opt_default;
			break;
		case KBOOT_OPTION_INTEGER:
			value.type = VALUE_TYPE_INTEGER;
			value.integer = *(uint64_t *)opt_default;
			break;
		}

		exist = environ_lookup(data->env, opt_name);
		if(!exist || exist->type != value.type) {
			environ_insert(data->env, opt_name, &value);
		}

		ui_list_insert_env(data->config, data->env, opt_name, opt_desc, false);
		break;
	}

	return true;
}

/** Load a KBoot kernel and modules.
 * @param args		Command arguments.
 * @param env		Environment for the command.
 * @return		Whether completed successfully. */
static bool config_cmd_kboot(value_list_t *args, environ_t *env) {
	kboot_data_t *data;

	if(args->count != 2 || !vtype(args, 0, VALUE_TYPE_STRING) ||
	   (!vtype(args, 1, VALUE_TYPE_LIST) && !vtype(args, 1, VALUE_TYPE_STRING))) {
		dprintf("kboot: invalid arguments\n");
		return false;
	}

	data = kmalloc(sizeof(*data));
	loader_type_set(env, &kboot_loader_type);
	loader_data_set(env, data);

	value_copy(&args->values[1], &data->modules);
	data->env = env;
	data->is_kboot = false;
	data->tags = NULL;
	data->config = ui_list_create("Kernel Options", true);

	/* Open the kernel image. */
	data->kernel = fs_open(NULL, args->values[0].string);
	if(!data->kernel) {
		/* The error will be reported when the user tries to boot. */
		return true;
	}

	/* Find all configuration tags. */
	elf_note_iterate(data->kernel, add_config_tags, data);
	return true;
}
DEFINE_COMMAND("kboot", config_cmd_kboot);
