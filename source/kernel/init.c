/*
 * Copyright (C) 2009-2012 Alex Smith
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
 * @brief		Kernel initialization functions.
 */

#include <arch/memory.h>

#include <io/device.h>
#include <io/fs.h>

#include <lib/string.h>
#include <lib/tar.h>

#include <mm/kmem.h>
#include <mm/malloc.h>
#include <mm/mmu.h>
#include <mm/page.h>
#include <mm/phys.h>
#include <mm/slab.h>
#include <mm/vm.h>

#include <proc/process.h>
#include <proc/sched.h>
#include <proc/thread.h>

#include <assert.h>
#include <console.h>
#include <cpu.h>
#include <dpc.h>
#include <kboot.h>
#include <kdb.h>
#include <kernel.h>
#include <module.h>
#include <object.h>
#include <smp.h>
#include <status.h>
#include <symbol.h>
#include <time.h>

KBOOT_IMAGE(KBOOT_IMAGE_LOG);

/** Structure describing a boot module. */
typedef struct boot_module {
	list_t header;			/**< Link to modules list. */
	void *mapping;			/**< Pointer to mapped module data. */
	size_t size;			/**< Size of the module data. */
	object_handle_t *handle;	/**< File handle for the module data. */
	char *name;			/**< Name of the module. */
} boot_module_t;

extern void kmain_bsp(uint32_t magic, kboot_tag_t *tags);
#if CONFIG_SMP
extern void kmain_ap(cpu_t *cpu);
#endif

extern initcall_t __initcall_start[], __initcall_end[];
extern fs_mount_t *root_mount;

static void init_thread(void *arg1, void *arg2);

/** Address of the KBoot tag list. */
static kboot_tag_t *kboot_tag_list __init_data;

/** The amount to increment the boot progress for each module. */
static int progress_per_module __init_data;
static int current_init_progress __init_data = 10;

/** List of modules/FS images from the loader. */
static LIST_DECLARE(boot_module_list);
static LIST_DECLARE(boot_fsimage_list);

/** Main entry point of the kernel.
 * @param magic		KBoot magic number.
 * @param tags		Tag list pointer. */
__init_text void kmain_bsp(uint32_t magic, kboot_tag_t *tags) {
	status_t ret;

	/* Save the tag list address. */
	kboot_tag_list = tags;

	/* Make the debugger available as soon as possible. */
	kdb_init();

	/* Bring up the console. */
	console_early_init();
	log_early_init();
	kprintf(LOG_NOTICE, "kernel: version %s booting...\n", kiwi_ver_string);

	/* Check the magic number. */
	if(magic != KBOOT_MAGIC) {
		kprintf(LOG_ERROR, "Not loaded by a KBoot-compliant loader\n");
		arch_cpu_halt();
	}

	/* Set up the CPU subsystem and initialize the boot CPU. */
	cpu_early_init();

	/* Initialize kernel memory management subsystems. */
	page_early_init();
	mmu_init();
	page_init();
	kmem_init();
	slab_init();
	malloc_init();

	/* Properly set up the console. */
	console_init();

	/* Finish initializing the CPU subsystem and set up the platform. */
	cpu_init();
	platform_init();
	time_init();
	#if CONFIG_SMP
	smp_init();
	#endif
	slab_late_init();

	#if CONFIG_DEBUGGER_DELAY > 0
	/* Delay to allow GDB to be connected. */
	kprintf(LOG_NOTICE, "kernel: waiting %d seconds for a debugger...\n", CONFIG_DEBUGGER_DELAY);
	spin(SECS2NSECS(CONFIG_DEBUGGER_DELAY));
	#endif

	/* Perform other initialization tasks. */
	symbol_init();
	handle_init();
	process_init();
	thread_init();
	sched_init();
	dpc_init();

	/* Bring up the VM system. */
	vm_init();

	/* Create the second stage initialization thread. */
	ret = thread_create("init", NULL, 0, init_thread, NULL, NULL, NULL);
	if(ret != STATUS_SUCCESS)
		fatal("Could not create second-stage initialization thread");

	/* Finally begin executing other threads. */
	sched_enter();
}

#if CONFIG_SMP
/** Kernel entry point for a secondary CPU.
 * @param cpu		Pointer to CPU structure for the CPU. */
__init_text void kmain_ap(cpu_t *cpu) {
	/* Indicate that we have reached the kernel. */
	smp_boot_status = SMP_BOOT_ALIVE;

	/* Initialize everything. */
	cpu_early_init_percpu(cpu);
	mmu_init_percpu();
	cpu_init_percpu();
	sched_init_percpu();

	/* Signal that we're up. */
	smp_boot_status = SMP_BOOT_BOOTED;

	/* Wait for remaining CPUs to be brought up. */
	while(smp_boot_status != SMP_BOOT_COMPLETE)
		arch_cpu_spin_hint();

	/* Begin scheduling threads. */
	sched_enter();
}
#endif

/** Remove a module from the module list.
 * @param mod		Module to remove. */
static __init_text void boot_module_remove(boot_module_t *mod) {
	list_remove(&mod->header);
	if(mod->name)
		kfree(mod->name);

	object_handle_release(mod->handle);
	phys_unmap(mod->mapping, mod->size, true);
	kfree(mod);

	/* Update progress. */
	current_init_progress += progress_per_module;
	update_boot_progress(current_init_progress);
}

/** Look up a kernel module in the boot module list.
 * @param name		Name to look for.
 * @return		Pointer to module if found, NULL if not. */
static __init_text boot_module_t *boot_module_lookup(const char *name) {
	boot_module_t *mod;

	LIST_FOREACH(&boot_module_list, iter) {
		mod = list_entry(iter, boot_module_t, header);

		if(mod->name && strcmp(mod->name, name) == 0)
			return mod;
	}

	return NULL;
}

/** Load a kernel module provided at boot.
 * @param mod		Module to load. */
static __init_text void load_boot_kmod(boot_module_t *mod) {
	char name[MODULE_NAME_MAX + 1];
	boot_module_t *dep;
	status_t ret;

	/* Try to load the module and all dependencies. */
	while(true) {
		ret = module_load(mod->handle, name);
		if(ret == STATUS_SUCCESS) {
			boot_module_remove(mod);
			return;
		} else if(ret != STATUS_MISSING_LIBRARY) {
			fatal("Could not load module %s (%d)", (mod->name) ? mod->name : "<noname>", ret);
		}

		/* Unloaded dependency, try to find it and load it. */
		dep = boot_module_lookup(name);
		if(!dep)
			fatal("Dependency on '%s' which is not available", name);

		load_boot_kmod(dep);
	}
}

/** Load boot-time kernel modules. */
static __init_text void load_modules(void) {
	boot_module_t *mod;
	size_t count = 0;
	char *tmp;

	/* Firstly, populate our module list with the module details from the
	 * loader. This is done so that it's much easier to look up module
	 * dependencies, and also because the module loader requires handles
	 * rather than a chunk of memory. */
	KBOOT_ITERATE(KBOOT_TAG_MODULE, kboot_tag_module_t, tag) {
		mod = kmalloc(sizeof(boot_module_t), MM_BOOT);
		list_init(&mod->header);
		mod->name = NULL;
		mod->size = tag->size;
		mod->mapping = phys_map(tag->addr, tag->size, MM_BOOT);
		mod->handle = file_from_memory(mod->mapping, tag->size);

		/* Figure out the module name, which is needed to resolve
		 * dependencies. If unable to get the name, assume the module
		 * is a filesystem image. */
		tmp = kmalloc(MODULE_NAME_MAX + 1, MM_BOOT);
		if(module_name(mod->handle, tmp) != STATUS_SUCCESS) {
			kfree(tmp);
			list_append(&boot_fsimage_list, &mod->header);
		} else {
			mod->name = tmp;
			list_append(&boot_module_list, &mod->header);
		}

		count++;
	}

	if(!count)
		fatal("No modules were provided, cannot do anything!");

	/* Determine how much to increase the boot progress by for each
	 * module loaded. */
	progress_per_module = 80 / count;

	/* Load all kernel modules. */
	while(!list_empty(&boot_module_list)) {
		mod = list_first(&boot_module_list, boot_module_t, header);
		load_boot_kmod(mod);
	}
}

/** Second-stage intialization thread.
 * @param arg1		Unused.
 * @param arg2		Unused. */
static void init_thread(void *arg1, void *arg2) {
	const char *pargs[] = { "/system/services/svcmgr", NULL }, *penv[] = { NULL };
	initcall_t *initcall;
	boot_module_t *mod;
	status_t ret;

	/* Bring up all detected secondary CPUs. */
	#if CONFIG_SMP
	smp_boot();
	#endif

	kprintf(LOG_NOTICE, "cpu: detected %zu CPU(s):\n", cpu_count);
	LIST_FOREACH(&running_cpus, iter)
		cpu_dump(list_entry(iter, cpu_t, header));

	/* Bring up the filesystem manager and device manager. */
	device_init();
	fs_init();

	/* Call other initialization functions. */
	for(initcall = __initcall_start; initcall != __initcall_end; initcall++)
		(*initcall)();

	update_boot_progress(10);

	/* Load modules, then any FS images supplied. Wait until after loading
	 * kernel modules to do FS images, so that we only load FS images if the
	 * boot filesystem could not be mounted. */
	load_modules();
	if(!root_mount) {
		if(list_empty(&boot_fsimage_list))
			fatal("Could not find boot filesystem");

		/* Mount a ramfs at the root to extract the images to. */
		ret = fs_mount(NULL, "/", "ramfs", NULL);
		if(ret != STATUS_SUCCESS)
			fatal("Could not mount ramfs for root (%d)", ret);

		while(!list_empty(&boot_fsimage_list)) {
			mod = list_first(&boot_fsimage_list, boot_module_t, header);

			ret = tar_extract(mod->handle, "/");
			if(ret != STATUS_SUCCESS)
				fatal("Failed to load FS image (%d)", ret);

			boot_module_remove(mod);
		}
	} else {
		while(!list_empty(&boot_fsimage_list)) {
			mod = list_first(&boot_fsimage_list, boot_module_t, header);
			boot_module_remove(mod);
		}
	}

	/* Reclaim memory taken up by initialization code/data. */
	page_late_init();
	kmem_late_init();

	update_boot_progress(100);

	/* Run the service manager. */
	ret = process_create(pargs, penv, PROCESS_CRITICAL, PRIORITY_CLASS_SYSTEM,
		kernel_proc, NULL);
	if(ret != STATUS_SUCCESS)
		fatal("Could not start service manager (%d)", ret);
}

/** Iterate over the KBoot tag list.
 * @param type		Type of tag to iterate.
 * @param current	Pointer to current tag, NULL to start from beginning.
 * @return		Virtual address of next tag, or NULL if no more tags
 *			found. */
__init_text void *kboot_tag_iterate(uint32_t type, void *current) {
	kboot_tag_t *header = current;

	do {
		header = (header)
			? (kboot_tag_t *)ROUND_UP((ptr_t)header + header->size, 8)
			: kboot_tag_list;
		if(header->type == KBOOT_TAG_NONE)
			header = NULL;
	} while(header && type && header->type != type);

	return header;
}

/** Look up a KBoot option tag.
 * @param name		Name to look up.
 * @param type		Required option type.
 * @return		Pointer to the option value. */
static __init_text void *lookup_option(const char *name, uint32_t type) {
	const char *found;

	KBOOT_ITERATE(KBOOT_TAG_OPTION, kboot_tag_option_t, tag) {
		found = kboot_tag_data(tag, 0);
		if(strcmp(found, name) == 0) {
			if(tag->type != type)
				fatal("Kernel option `%s' has incorrect type", name);

			return kboot_tag_data(tag, ROUND_UP(tag->name_size, 8));
		}
	}

	/* If an option is specified in an image, there should be a tag
	 * passed to the kernel for it. */
	fatal("Expected kernel option `%s' not found", name);
}

/** Get the value of a KBoot boolean option.
 * @param name		Name of the option.
 * @return		Value of the option. */
__init_text bool kboot_boolean_option(const char *name) {
	return *(bool *)lookup_option(name, KBOOT_OPTION_BOOLEAN);
}

/** Get the value of a KBoot integer option.
 * @param name		Name of the option.
 * @return		Value of the option. */
__init_text uint64_t kboot_integer_option(const char *name) {
	return *(uint64_t *)lookup_option(name, KBOOT_OPTION_INTEGER);
}

/** Get the value of a KBoot string option.
 * @param name		Name of the option.
 * @return		Pointer to the option string. */
__init_text const char *kboot_string_option(const char *name) {
	return (const char  *)lookup_option(name, KBOOT_OPTION_STRING);
}
