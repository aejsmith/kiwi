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
	phys_ptr_t tags;		/**< Start of the tag list. */
	ui_window_t *config;		/**< Configuration window. */
	mmu_context_t *mmu;		/**< MMU context. */
} kboot_data_t;

extern mmu_context_t *kboot_arch_load(fs_handle_t *handle, phys_ptr_t *physp);
extern void kboot_arch_enter(mmu_context_t *ctx, phys_ptr_t tags) __noreturn;

/** Add a tag to the tag list.
 * @param data		Loader data structure.
 * @param tag		Address of tag to add. */
static void append_tag(kboot_data_t *data, phys_ptr_t tag) {
	kboot_tag_t *exist;
	phys_ptr_t addr;

	if(data->tags) {
		for(addr = data->tags; addr; addr = exist->next) {
			exist = (kboot_tag_t *)((ptr_t)addr);
		}

		exist->next = tag;
	} else {
		data->tags = tag;
	}
}

/** Allocate a tag in the tag list.
 * @param data		Loader data structure.
 * @param type		Type of the tag.
 * @param size		Size of the tag.
 * @return		Address of allocated tag. */
static void *allocate_tag(kboot_data_t *data, uint32_t type, size_t size) {
	kboot_tag_t *tag;

	assert(size >= sizeof(kboot_tag_t));

	tag = kmalloc(size);
	tag->next = 0;
	tag->type = type;
	tag->size = size;

	append_tag(data, (ptr_t)tag);
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

/** Set a single option.
 * @param data		Loader data pointer.
 * @param name		Name of the option.
 * @param type		Type of the option. */
static void set_option(kboot_data_t *data, const char *name, uint32_t type) {
	kboot_tag_option_t *tag;
	value_t *value;
	size_t size;

	value = environ_lookup(data->env, name);
	switch(type) {
	case KBOOT_OPTION_BOOLEAN:
		size = 1;
		break;
	case KBOOT_OPTION_STRING:
		size = strlen(value->string) + 1;
		break;
	case KBOOT_OPTION_INTEGER:
		size = sizeof(uint64_t);
		break;
	default:
		internal_error("Eeeep.");
	}

	tag = allocate_tag(data, KBOOT_TAG_OPTION, sizeof(*tag) + size);
	strncpy(tag->name, name, sizeof(tag->name));
	tag->name[sizeof(tag->name) - 1] = 0;
	tag->type = type;
	tag->size = size;

	switch(type) {
	case KBOOT_OPTION_BOOLEAN:
		*(bool *)&tag[1] = value->boolean;
		break;
	case KBOOT_OPTION_STRING:
		memcpy(&tag[1], value->string, size);
		break;
	case KBOOT_OPTION_INTEGER:
		*(uint64_t *)&tag[1] = value->integer;
		break;
	}
}

/** Set the video mode.
 * @param data		Loader data pointer. */
static void set_video_mode(kboot_data_t *data) {
	kboot_tag_lfb_t *tag;
	video_mode_t *mode;
	value_t *value;

	value = environ_lookup(data->env, "video_mode");
	mode = value->pointer;
	video_enable(mode);

	tag = allocate_tag(data, KBOOT_TAG_LFB, sizeof(*tag));
	tag->width = mode->width;
	tag->height = mode->height;
	tag->depth = mode->bpp;
	tag->addr = mode->addr;
}

/** Tag iterator to set options in the tag list.
 * @param note		Note header.
 * @param name		Note name.
 * @param desc		Note data.
 * @param _data		KBoot loader data pointer.
 * @return		Whether to continue iteration. */
static bool set_options(elf_note_t *note, const char *name, void *desc, void *_data) {
	kboot_itag_mapping_t *mapping;
	kboot_itag_option_t *option;
	kboot_data_t *data = _data;
	kboot_itag_image_t *image;

	if(strcmp(name, "KBoot") != 0) {
		return true;
	}

	switch(note->n_type) {
	case KBOOT_ITAG_IMAGE:
		image = desc;

		/* Set the video mode if requested. */
		if(image->flags & KBOOT_IMAGE_LFB) {
			set_video_mode(data);
		}

		break;
	case KBOOT_ITAG_OPTION:
		option = desc;
		set_option(data, desc + sizeof(*option), option->type);
		break;
	case KBOOT_ITAG_MAPPING:
		mapping = desc;
		if(!mmu_map(data->mmu, mapping->virt, mapping->phys, mapping->size)) {
			boot_error("Kernel specifies an invalid memory mapping");
		}
		break;
	}

	return true;
}

/** Load the operating system.
 * @param env		Environment for the OS. */
static __noreturn void kboot_loader_load(environ_t *env) {
	kboot_data_t *data = loader_data_get(env);
	kboot_tag_bootdev_t *bootdev;
	kboot_tag_core_t *core;
	phys_ptr_t addr;

	/* We don't report these errors until the user actually tries to run a
	 * menu entry. */
	if(!data->kernel) {
		boot_error("Could not find kernel image");
	} else if(!data->is_kboot) {
		boot_error("Kernel is not a valid KBoot kernel");
	}

	/* Create the core information tag. */
	core = allocate_tag(data, KBOOT_TAG_CORE, sizeof(*core));

	/* Load the kernel image into memory. */
	kprintf("Loading kernel...\n");
	data->mmu = kboot_arch_load(data->kernel, &core->kernel_phys);

	/* Record the boot device. */
	bootdev = allocate_tag(data, KBOOT_TAG_BOOTDEV, sizeof(*bootdev));
	strncpy(bootdev->uuid, current_disk->fs->uuid, sizeof(bootdev->uuid));
	bootdev->uuid[sizeof(bootdev->uuid) - 1] = 0;

	/* Load modules. */
	if(data->modules.type == VALUE_TYPE_LIST) {
		load_module_list(data, data->modules.list);
	} else if(data->modules.type == VALUE_TYPE_STRING) {
		load_module_dir(data, data->modules.string);
	}

	/* Create option tags, set video mode and set up memory mappings. */
	elf_note_iterate(data->kernel, set_options, data);

	/* Finish off the memory map and add memory ranges to the tag list. */
	addr = memory_finalise();
	append_tag(data, addr);

	/* Enter the kernel. */
	kboot_arch_enter(data->mmu, data->tags);
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

/** Tag iterator to add options to the environment.
 * @param note		Note header.
 * @param name		Note name.
 * @param desc		Note data.
 * @param _data		KBoot loader data pointer.
 * @return		Whether to continue iteration. */
static bool add_options(elf_note_t *note, const char *name, void *desc, void *_data) {
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
	data->tags = 0;
	data->config = ui_list_create("Kernel Options", true);

	/* Open the kernel image. */
	data->kernel = fs_open(NULL, args->values[0].string);
	if(!data->kernel) {
		/* The error will be reported when the user tries to boot. */
		return true;
	}

	/* Find all option tags. */
	elf_note_iterate(data->kernel, add_options, data);
	return true;
}
DEFINE_COMMAND("kboot", config_cmd_kboot);
