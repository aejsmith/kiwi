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
#include <cpu/intr.h>
#include <cpu/ipi.h>

#include <io/device.h>
#include <io/fs.h>

#include <lib/string.h>
#include <lib/tar.h>

#include <mm/kheap.h>
#include <mm/malloc.h>
#include <mm/page.h>
#include <mm/slab.h>
#include <mm/vm.h>

#include <proc/process.h>
#include <proc/sched.h>
#include <proc/session.h>
#include <proc/thread.h>

#include <security/context.h>

#include <console.h>
#include <dpc.h>
#include <kboot.h>
#include <kernel.h>
#include <lrm.h>
#include <module.h>
#include <object.h>
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

extern void kmain_bsp(phys_ptr_t tags);
extern initcall_t __initcall_start[], __initcall_end[];
extern fs_mount_t *root_mount;

static void kmain_bsp_bottom(void);

/** New stack for the boot CPU. */
static uint8_t boot_stack[KSTACK_SIZE] __init_data __aligned(PAGE_SIZE);

/** Address of the KBoot tag list. */
phys_ptr_t kboot_tag_list __init_data;

#if 0
/** The amount to increment the boot progress for each module. */
static int current_init_progress __init_data = 10;
static int progress_per_module __init_data;

/** List of modules/FS images from the bootloader. */
static LIST_DECLARE(boot_module_list);
static LIST_DECLARE(boot_fsimage_list);

/** Remove a module from the module list.
 * @param mod		Module to remove. */
static void __init_text boot_module_remove(boot_module_t *mod) {
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
static boot_module_t *boot_module_lookup(const char *name) {
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
static void __init_text load_boot_kmod(boot_module_t *mod) {
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

/** Load modules loaded by the bootloader.
 * @param args		Kernel arguments structure. */
static void __init_text load_modules(kernel_args_t *args) {
	kernel_args_module_t *amod;
	boot_module_t *mod;
	phys_ptr_t addr;
	char *tmp;

	if(!args->module_count) {
		fatal("No modules were provided, cannot do anything!");
	}

	/* Firstly, populate our module list with the module details from the
	 * bootloader. This is done so that it's much easier to look up module
	 * dependencies, and also because the module loader requires handles
	 * rather than a chunk of memory. */
	for(addr = args->modules; addr;) {
		amod = phys_map(addr, sizeof(kernel_args_module_t), MM_FATAL);

		/* Create a structure for the module and map the data into
		 * memory. */
		mod = kmalloc(sizeof(boot_module_t), MM_FATAL);
		list_init(&mod->header);
		mod->name = NULL;
		mod->size = amod->size;
		mod->mapping = phys_map(amod->base, mod->size, MM_FATAL);
		mod->handle = file_from_memory(mod->mapping, mod->size);

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

		addr = amod->next;
		phys_unmap(amod, sizeof(kernel_args_module_t), true);
	}

	/* Determine how much to increase the boot progress by for each
	 * module loaded. */
	progress_per_module = 80 / args->module_count;

	/* Load all kernel modules. */
	while(!list_empty(&boot_module_list)) {
		mod = list_entry(boot_module_list.next, boot_module_t, header);
		load_boot_kmod(mod);
	}
}

/** Second-stage intialization thread.
 * @param args		Kernel arguments structure pointer.
 * @param arg2		Thread argument (unused). */
static void init_thread(void *args, void *arg2) {
	const char *pargs[] = { "/system/services/svcmgr", NULL }, *penv[] = { NULL };
	initcall_t *initcall;
	boot_module_t *mod;
	status_t ret;

	/* Bring up the filesystem manager and device manager. */
	device_init();
	fs_init(args);

	/* Bring up secondary CPUs. The first rendezvous sets off their
	 * initialisation, the second waits for them to complete. */
	init_rendezvous(args, &init_rendezvous_2);
	init_rendezvous(args, &init_rendezvous_3);

	/* Call other initialisation functions. */
	for(initcall = __initcall_start; initcall != __initcall_end; initcall++) {
		(*initcall)();
	}

	console_update_boot_progress(10);

	/* Load modules, then any FS images supplied. Wait until after loading
	 * kernel modules to do FS images, so that we only load FS images if the
	 * boot filesystem could not be mounted. */
	load_modules(args);
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
#endif

/** Main entry point of the kernel.
 * @param tags		Physical address of KBoot tag list. */
__init_text void kmain_bsp(phys_ptr_t tags) {
	context_t ctx;

	/* Save the tag list address. */
	kboot_tag_list = tags;

	/* Currently we are running on a stack set up for us by the loader.
	 * This stack is most likely in a mapping that will go away once the
	 * memory management subsystems are brought up. Therefore, the first
	 * thing we do is move over to a new stack that is mapped along with
	 * the kernel. */
	context_init(&ctx, (ptr_t)kmain_bsp_bottom, boot_stack);
	context_restore(&ctx);
}

/** Initialisation code for the boot CPU. */
static __init_text void kmain_bsp_bottom(void) {
	/* Bring up the debug console. */
	console_early_init();

	/* Perform early architecture/platform initialisation. */
	cpu_early_init();
	arch_premm_init();
	//platform_premm_init();

	kprintf(LOG_DEBUG, "Hello, World\n");
	while(1);
#if 0
	thread_t *thread;
	status_t ret;

	/* Wait for all CPUs to enter the kernel. */
	init_rendezvous(args, &init_rendezvous_1);

	if(id == args->boot_cpu) {
		console_early_init();

		/* Perform early architecture/platform initialisation. */
		cpu_early_init(args);
		arch_premm_init(args);
		platform_premm_init(args);

		/* Initialise kernel memory management subsystems. */
		security_init();
		vmem_early_init();
		kheap_early_init();
		page_init(args);
		vmem_init();
		slab_init();
		kheap_init();
		malloc_init();

		/* Set up the console. */
		console_init(args);
		kprintf(LOG_NORMAL, "kernel: version %s booting (%" PRIu32 " CPU(s))\n",
		        kiwi_ver_string, cpu_count);

		/* Perform second stage architecture/platform initialisation. */
		arch_postmm_init(args);
		platform_postmm_init(args);

#if CONFIG_DEBUGGER_DELAY > 0
		/* Delay to allow GDB to be connected. */
		kprintf(LOG_NORMAL, "kernel: waiting %d seconds for a debugger...\n", CONFIG_DEBUGGER_DELAY);
		spin(SECS2USECS(CONFIG_DEBUGGER_DELAY));
#endif
		/* Perform other initialisation tasks. */
		symbol_init();
		time_init();
		cpu_init(args);
		ipi_init();
		handle_init();
		session_init();
		process_init();
		thread_init();
		sched_init();
		thread_reaper_init();
		dpc_init();
		lrm_init();

		/* Now that the scheduler is up, the vmem periodic maintenance
		 * timer can be registered. */
		vmem_late_init();

		/* Bring up the VM system. */
		vm_init();

		/* Create the second stage initialisation thread. */
		ret = thread_create("init", NULL, 0, init_thread, args, NULL, NULL, &thread);
		if(ret != STATUS_SUCCESS) {
			fatal("Could not create second-stage initialisation thread");
		}
		thread_run(thread);

		/* Finally begin executing other threads. */
		sched_enter();
	} else {
		/* Wait for the boot CPU to do its initialisation. */
		init_rendezvous(args, &init_rendezvous_2);

		spinlock_lock(&smp_boot_spinlock);

		/* Switch to the kernel page map and do architecture-specific
		 * initialisation of this CPU. */
		page_map_switch(&kernel_page_map);
		arch_ap_init(args, cpus[id]);

		/* We're running, add ourselves to the running CPU list. */
		list_append(&cpus_running, &curr_cpu->header);

		/* Do scheduler initialisation. */
		sched_init();

		spinlock_unlock(&smp_boot_spinlock);

		/* Perform the final rendezvous and then enter the scheduler. */
		init_rendezvous(args, &init_rendezvous_3);
		sched_enter();
	}
#endif
}
