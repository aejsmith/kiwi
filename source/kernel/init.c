/*
 * Copyright (C) 2009-2011 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Kernel initialisation functions.
 */

#include <arch/memory.h>

#include <cpu/cpu.h>
#include <cpu/ipi.h>

#include <io/device.h>
#include <io/fs.h>

#include <lib/string.h>
#include <lib/tar.h>

#include <mm/heap.h>
#include <mm/malloc.h>
#include <mm/mmu.h>
#include <mm/page.h>
#include <mm/phys.h>
#include <mm/slab.h>
#include <mm/vm.h>

#include <proc/process.h>
#include <proc/sched.h>
#include <proc/session.h>
#include <proc/thread.h>

#include <security/context.h>

#include <assert.h>
#include <console.h>
#include <dpc.h>
#include <kboot.h>
#include <kdb.h>
#include <kernel.h>
#include <lrm.h>
#include <module.h>
#include <object.h>
#include <setjmp.h>
#include <status.h>
#include <symbol.h>
#include <time.h>

/** Structure describing a boot module. */
typedef struct boot_module {
	list_t header;			/**< Link to modules list. */
	void *mapping;			/**< Pointer to mapped module data. */
	size_t size;			/**< Size of the module data. */
	object_handle_t *handle;	/**< File handle for the module data. */
	char *name;			/**< Name of the module. */
} boot_module_t;

extern void kmain_bsp(uint32_t magic, phys_ptr_t tags);
#if CONFIG_SMP
extern void kmain_ap(cpu_t *cpu);
#endif
extern initcall_t __initcall_start[], __initcall_end[];
extern fs_mount_t *root_mount;

static void kmain_bsp_bottom(void);

/** New stack for the boot CPU. */
static uint8_t boot_stack[KSTACK_SIZE] __init_data __aligned(PAGE_SIZE);

/** Address of the KBoot tag list. */
static phys_ptr_t kboot_tag_list __init_data;

/** The amount to increment the boot progress for each module. */
static int progress_per_module __init_data;
static int current_init_progress __init_data = 10;

/** List of modules/FS images from the loader. */
static LIST_DECLARE(boot_module_list);
static LIST_DECLARE(boot_fsimage_list);

/** Iterate over the KBoot tag list.
 * @param type		Type of tag to iterate.
 * @param current	Pointer to current tag.
 * @return		Virtual address of next tag, or NULL if no more tags
 *			found. This memory must be unmapped after it has been
 *			used. This will be done if it is passed as the current
 *			argument to this function, or if it is passed to
 *			kboot_tag_release(). */
__init_text void *kboot_tag_iterate(uint32_t type, void *current) {
	kboot_tag_t *header = current, *tmp;
	phys_ptr_t next;

	do {
		/* Get the address of the next tag. */
		if(header) {
			next = header->next;
			phys_unmap(header, header->size, true);
		} else {
			next = kboot_tag_list;
		}
		if(!next) {
			return NULL;
		}

		/* First map with only the header size in order to get the full
		 * size of the tag data. */
		tmp = phys_map(next, sizeof(kboot_tag_t), MM_FATAL);
		header = phys_map(next, tmp->size, MM_FATAL);
		phys_unmap(tmp, sizeof(kboot_tag_t), true);
	} while(header->type != type);

	return header;
}

/** Unmap a KBoot tag.
 * @param current	Address of tag to unmap. */
__init_text void kboot_tag_release(void *current) {
	kboot_tag_t *header = current;
	phys_unmap(header, header->size, true);
}

/** Look up a KBoot option tag.
 * @param name		Name to look up.
 * @param type		Required option type.
 * @return		Pointer to tag. Must be released. */
static __init_text kboot_tag_option_t *lookup_option(const char *name, uint32_t type) {
	KBOOT_ITERATE(KBOOT_TAG_OPTION, kboot_tag_option_t, tag) {
		if(strcmp(tag->name, name) == 0) {
			if(tag->type != type) {
				fatal("Boot option '%s' has incorrect type", name);
			}
			return tag;
		}
	}

	fatal("Requested boot option '%s' not found", name);
}

/** Get the value of a KBoot boolean option.
 * @param name		Name of the option.
 * @return		Value of the option. */
__init_text bool kboot_boolean_option(const char *name) {
	kboot_tag_option_t *tag = lookup_option(name, KBOOT_OPTION_BOOLEAN);
	assert(tag->size == sizeof(bool));
	return *(bool *)&tag[1];
}

/** Get the value of a KBoot integer option.
 * @param name		Name of the option.
 * @return		Value of the option. */
__init_text uint64_t kboot_integer_option(const char *name) {
	kboot_tag_option_t *tag = lookup_option(name, KBOOT_OPTION_INTEGER);
	assert(tag->size == sizeof(uint64_t));
	return *(uint64_t *)&tag[1];
}

/** Remove a module from the module list.
 * @param mod		Module to remove. */
static __init_text void boot_module_remove(boot_module_t *mod) {
	list_remove(&mod->header);
	if(mod->name) {
		kfree(mod->name);
	}
	object_handle_release(mod->handle);
	phys_unmap(mod->mapping, mod->size, true);
	kfree(mod);

	/* Update progress. */
	current_init_progress += progress_per_module;
	console_update_boot_progress(current_init_progress);
}

/** Look up a kernel module in the boot module list.
 * @param name		Name to look for.
 * @return		Pointer to module if found, NULL if not. */
static __init_text boot_module_t *boot_module_lookup(const char *name) {
	boot_module_t *mod;

	LIST_FOREACH(&boot_module_list, iter) {
		mod = list_entry(iter, boot_module_t, header);

		if(mod->name && strcmp(mod->name, name) == 0) {
			return mod;
		}
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
		if(!dep) {
			fatal("Dependency on '%s' which is not available", name);
		}
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
		mod = kmalloc(sizeof(boot_module_t), MM_FATAL);
		list_init(&mod->header);
		mod->name = NULL;
		mod->size = tag->size;
		mod->mapping = phys_map(tag->addr, tag->size, MM_FATAL);
		mod->handle = file_from_memory(mod->mapping, tag->size);

		/* Figure out the module name, which is needed to resolve
		 * dependencies. If unable to get the name, assume the module
		 * is a filesystem image. */
		tmp = kmalloc(MODULE_NAME_MAX + 1, MM_FATAL);
		if(module_name(mod->handle, tmp) != STATUS_SUCCESS) {
			kfree(tmp);
			list_append(&boot_fsimage_list, &mod->header);
		} else {
			mod->name = tmp;
			list_append(&boot_module_list, &mod->header);
		}

		count++;
	}

	if(!count) {
		fatal("No modules were provided, cannot do anything!");
	}

	/* Determine how much to increase the boot progress by for each
	 * module loaded. */
	progress_per_module = 80 / count;

	/* Load all kernel modules. */
	while(!list_empty(&boot_module_list)) {
		mod = list_entry(boot_module_list.next, boot_module_t, header);
		load_boot_kmod(mod);
	}
}

#if CONFIG_SMP
/** Boot secondary CPUs. */
static __init_text void smp_boot(void) {
	cpu_id_t i;

	for(i = 0; i <= highest_cpu_id; i++) {
		if(cpus[i] && cpus[i]->state == CPU_OFFLINE) {
			cpu_boot(cpus[i]);
		}
	}

	cpu_boot_wait = 2;
}
#endif

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
	LIST_FOREACH(&running_cpus, iter) {
		cpu_dump(list_entry(iter, cpu_t, header));
	}

	/* Bring up the filesystem manager and device manager. */
	device_init();
	fs_init();

	/* Call other initialisation functions. */
	for(initcall = __initcall_start; initcall != __initcall_end; initcall++) {
		(*initcall)();
	}

	console_update_boot_progress(10);

	/* Load modules, then any FS images supplied. Wait until after loading
	 * kernel modules to do FS images, so that we only load FS images if the
	 * boot filesystem could not be mounted. */
	load_modules();
	if(!root_mount) {
		if(list_empty(&boot_fsimage_list)) {
			fatal("Could not find boot filesystem");
		}

		/* Mount a ramfs at the root to extract the images to. */
		ret = fs_mount(NULL, "/", "ramfs", NULL);
		if(ret != STATUS_SUCCESS) {
			fatal("Could not mount ramfs for root (%d)", ret);
		}

		while(!list_empty(&boot_fsimage_list)) {
			mod = list_entry(boot_fsimage_list.next, boot_module_t, header);

			ret = tar_extract(mod->handle, "/");
			if(ret != STATUS_SUCCESS) {
				fatal("Failed to load FS image (%d)", ret);
			}

			boot_module_remove(mod);
		}
	} else {
		while(!list_empty(&boot_fsimage_list)) {
			mod = list_entry(boot_fsimage_list.next, boot_module_t, header);
			boot_module_remove(mod);
		}
	}

	/* Reclaim memory taken up by initialisation code/data. */
	page_late_init();

	console_update_boot_progress(100);

	/* Run the service manager. */
	ret = process_create(pargs, penv, PROCESS_CRITICAL, PRIORITY_CLASS_SYSTEM, kernel_proc, NULL);
	if(ret != STATUS_SUCCESS) {
		fatal("Could not start service manager (%d)", ret);
	}
}

/** Main entry point of the kernel.
 * @param magic		KBoot magic number.
 * @param tags		Physical address of KBoot tag list. */
__init_text void kmain_bsp(uint32_t magic, phys_ptr_t tags) {
	jmp_buf buf;

	/* Save the tag list address. */
	kboot_tag_list = tags;

	/* Make the debugger available as soon as possible. */
	kdb_init();

	/* Bring up the debug console. */
	console_early_init();

	/* Check the magic number. */
	if(magic != KBOOT_MAGIC) {
		kprintf(LOG_ERROR, "Not loaded by a KBoot-compliant loader\n");
		cpu_halt();
	}

	/* Currently we are running on a stack set up for us by the loader.
	 * This stack is most likely in a mapping that will go away once the
	 * memory management subsystems are brought up. Therefore, the first
	 * thing we do is move over to a new stack that is mapped along with
	 * the kernel. */
	initjmp(buf, kmain_bsp_bottom, boot_stack, KSTACK_SIZE);
	longjmp(buf, 1);
}

/** Initialisation code for the boot CPU. */
static __init_text void kmain_bsp_bottom(void) {
	thread_t *thread;
	status_t ret;

	/* Do early CPU subsystem and CPU initialisation. */
	cpu_early_init();
	cpu_early_init_percpu(&boot_cpu);

	/* Initialise the security subsystem. */
	security_init();

	/* Initialise kernel memory management subsystems. */
	page_init();
	mmu_init();
	mmu_init_percpu();
	heap_init();
	slab_init();
	malloc_init();

	/* Set up the console. */
	console_init();
	kprintf(LOG_NOTICE, "kernel: version %s booting...\n", kiwi_ver_string);

	/* Perform more per-CPU initialisation that can be done now the memory
	 * management subsystems are up. */
	cpu_init_percpu();

	/* Initialise the platform. */
	platform_init();

	/* Get the time from the hardware. */
	time_init();

#if CONFIG_DEBUGGER_DELAY > 0
	/* Delay to allow GDB to be connected. */
	kprintf(LOG_NOTICE, "kernel: waiting %d seconds for a debugger...\n", CONFIG_DEBUGGER_DELAY);
	spin(SECS2USECS(CONFIG_DEBUGGER_DELAY));
#endif

	/* Properly initialise the CPU subsystem, and detect other CPUs. */
	cpu_init();
	slab_late_init();
#if CONFIG_SMP
	ipi_init();
#endif

	/* Perform other initialisation tasks. */
	symbol_init();
	handle_init();
	session_init();
	process_init();
	thread_init();
	sched_init();
	thread_reaper_init();
	dpc_init();
	lrm_init();

	/* Bring up the VM system. */
	vm_init();

	/* Create the second stage initialisation thread. */
	ret = thread_create("init", NULL, 0, init_thread, NULL, NULL, NULL, &thread);
	if(ret != STATUS_SUCCESS) {
		fatal("Could not create second-stage initialisation thread");
	}
	thread_run(thread);

	/* Finally begin executing other threads. */
	sched_enter();
}

#if CONFIG_SMP
/** Kernel entry point for a secondary CPU.
 * @param cpu		Pointer to CPU structure for the CPU. */
__init_text void kmain_ap(cpu_t *cpu) {
#if 0
	/* Switch to the kernel page map and do architecture-specific
	 * initialisation of this CPU. */
	page_map_switch(&kernel_page_map);
	arch_ap_init(cpu);

	/* Initialise the scheduler. */
	sched_init();

	/* Signal that we're up and add ourselves to the running CPU list. */
	cpu->state = CPU_RUNNING;
	list_append(&running_cpus, &curr_cpu->header);
	cpu_boot_wait = 1;

	/* Wait for remaining CPUs to be brought up. */
	while(cpu_boot_wait != 2) {
		cpu_spin_hint();
	}

	/* Begin scheduling threads. */
	sched_enter();
#endif
	while(true);
}
#endif
