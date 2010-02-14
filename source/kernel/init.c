/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		Kernel initialisation functions.
 */

#include <arch/arch.h>

#include <cpu/cpu.h>
#include <cpu/intr.h>
#include <cpu/ipi.h>

#include <io/vfs.h>

#include <lib/string.h>

#include <mm/kheap.h>
#include <mm/malloc.h>
#include <mm/page.h>
#include <mm/slab.h>
#include <mm/vm.h>

#include <platform/platform.h>

#include <proc/handle.h>
#include <proc/process.h>
#include <proc/sched.h>
#include <proc/thread.h>

#include <console.h>
#include <errors.h>
#include <fatal.h>
#include <init.h>
#include <kargs.h>
#include <module.h>
#include <tar.h>
#include <version.h>

/** Structure describing a boot module. */
typedef struct boot_module {
	list_t header;		/**< Link to modules list. */
	vfs_node_t *node;	/**< Node containing module data. */
	char *name;		/**< Name of the module. */
} boot_module_t;

extern void kmain(kernel_args_t *args, uint32_t cpu);
extern initcall_t __initcall_start[], __initcall_end[];

/** Rendezvous variables for SMP boot. */
static atomic_t init_rendezvous_1 __init_data = 0;
static atomic_t init_rendezvous_2 __init_data = 0;
static atomic_t init_rendezvous_3 __init_data = 0;

/** The amount to increment the boot progress for each module. */
static int init_current_progress __init_data = 10;
static int init_progress_per_module __init_data;

/** Lock to serialise the SMP boot. */
static SPINLOCK_DECLARE(smp_boot_spinlock);

/** List of modules from the bootloader. */
static LIST_DECLARE(boot_module_list);

/** Remove a module from the module list.
 * @param mod		Module to remove. */
static void __init_text boot_module_remove(boot_module_t *mod) {
	list_remove(&mod->header);
	if(mod->name) {
		kfree(mod->name);
	}
	vfs_node_release(mod->node);
	kfree(mod);

	/* Update progress. */
	init_current_progress += init_progress_per_module;
	console_update_boot_progress(init_current_progress);
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

/** Extract a TAR archive to the root FS.
 * @param mod		Boot module containing archive.
 * @return		Whether the module was a TAR archive. */
static bool __init_text boot_module_load_tar(boot_module_t *mod) {
	tar_header_t *hdr;
	vfs_node_t *node;
	int64_t size;
	size_t bytes;
	void *buf;
	int ret;

	buf = hdr = kmalloc(mod->node->size, MM_FATAL);
	if(vfs_file_read(mod->node, buf, mod->node->size, 0, &bytes) != 0 || bytes != mod->node->size) {
		fatal("Could not read TAR file data");
	}

	/* Check format of module. */
	if(strncmp(hdr->magic, "ustar", 5) != 0) {
		kfree(buf);
		return false;
	}

	/* If any TAR files are loaded it means we should mount a RamFS at the
	 * root, if this has not already been done. */
	if(!vfs_root_mount) {
		if((ret = vfs_mount(NULL, "/", "ramfs", 0)) != 0) {
			fatal("Could not mount RamFS at root (%d)", ret);
		}
	}

	/* Loop until we encounter two null bytes (EOF). */
	while(hdr->name[0] && hdr->name[1]) {
		if(strncmp(hdr->magic, "ustar", 5) != 0) {
			fatal("TAR file format is not correct");
		}

		/* All fields in the header are stored as ASCII - convert the
		 * size to an integer (base 8). */
		size = strtoll(hdr->size, NULL, 8);

		/* Handle the entry based on its type flag. */
		switch(hdr->typeflag) {
		case REGTYPE:
		case AREGTYPE:
			if((ret = vfs_file_create(hdr->name, &node)) != 0) {
				fatal("Failed to create regular file %s (%d)", hdr->name, ret);
			} else if((ret = vfs_file_write(node, (void *)((ptr_t)hdr + 512), size, 0, &bytes)) != 0) {
				fatal("Failed to write file %s (%d)", hdr->name, ret);
			} else if((int64_t)bytes != size) {
				fatal("Did not write all data for file %s (%zu, %zu)", hdr->name, bytes, size);
			}
			vfs_node_release(node);
			break;
		case DIRTYPE:
			if((ret = vfs_dir_create(hdr->name, NULL)) != 0) {
				fatal("Failed to create directory %s (%d)", hdr->name, ret);
			}
			break;
		case SYMTYPE:
			if((ret = vfs_symlink_create(hdr->name, hdr->linkname, NULL)) != 0) {
				fatal("Failed to create symbolic link %s (%d)", hdr->name, ret);
			}
			break;
		default:
			kprintf(LOG_DEBUG, "init: unhandled TAR type flag '%c'\n", hdr->typeflag);
			break;
		}

		/* 512 for the header, plus the file size if necessary. */
		hdr = (tar_header_t *)(ptr_t)((ptr_t)hdr + 512 + ((size != 0) ? ROUND_UP(size, 512) : 0));
	}

	kfree(buf);
	boot_module_remove(mod);
	return true;
}

/** Load a kernel module provided at boot.
 * @param mod		Module to load.
 * @return		Whether the file was a kernel module. */
static bool __init_text boot_module_load_kmod(boot_module_t *mod) {
	char name[MODULE_NAME_MAX + 1];
	boot_module_t *dep;
	int ret;

	/* Try to load the module and all dependencies. */
	while(true) {
		if((ret = module_load_node(mod->node, name)) == 0) {
			boot_module_remove(mod);
			return true;
		} else if(ret == -ERR_TYPE_INVAL) {
			return false;
		} else if(ret != -ERR_DEP_MISSING) {
			fatal("Could not load module %s (%d)", mod->name);
		}

		/* Unloaded dependency, try to find it and load it. */
		if(!(dep = boot_module_lookup(name)) || !boot_module_load_kmod(dep)) {
			fatal("Dependency on '%s' which is not available", name);
		}
	}
}

/** Load modules loaded by the bootloader.
 * @param args		Kernel arguments structure. */
static void __init_text load_modules(kernel_args_t *args) {
	kernel_args_module_t *amod;
	boot_module_t *mod;
	phys_ptr_t addr;
	void *mapping;
	char *tmp;

	if(!args->module_count) {
		fatal("No modules were provided, cannot do anything!");
	}

	/* Firstly, populate our module list with the module details from the
	 * bootloader. This is done so that it's much easier to look up module
	 * dependencies, and also because the module loader requires VFS
	 * nodes rather than a chunk of memory. */
	for(addr = args->modules; addr;) {
		amod = page_phys_map(addr, sizeof(kernel_args_module_t), MM_FATAL);

		mod = kmalloc(sizeof(boot_module_t), MM_FATAL);
		list_init(&mod->header);
		mod->name = NULL;

		/* Map in the data and convert it into a VFS node. */
		mapping = page_phys_map(amod->base, amod->size, MM_FATAL);
		if(vfs_file_from_memory(mapping, amod->size, &mod->node) != 0) {
			fatal("Failed to create node from module data");
		}
		page_phys_unmap(mapping, amod->size, true);

		/* Figure out the module name. Do not fail if unable to get
		 * the name, may be a filesystem image or something. */
		tmp = kmalloc(MODULE_NAME_MAX + 1, MM_FATAL);
		if(module_name(mod->node, tmp) != 0) {
			kfree(tmp);
		} else {
			mod->name = tmp;
		}

		list_append(&boot_module_list, &mod->header);

		addr = amod->next;
		page_phys_unmap(amod, sizeof(kernel_args_module_t), true);
	}

	/* Determine how much to increase the boot progress by for each
	 * module loaded. */
	init_progress_per_module = 80 / args->module_count;

	/* Now keep on loading until all modules have been loaded. */
	while(!list_empty(&boot_module_list)) {
		mod = list_entry(boot_module_list.next, boot_module_t, header);
		if(!boot_module_load_tar(mod) && !boot_module_load_kmod(mod)) {
			fatal("Module with unknown format in module list");
		}
	}
}

/** Wait until all CPUs reach a certain point.
 * @param args		Kernel arguments structure.
 * @param var		Variable to wait on. */
static inline void init_rendezvous(kernel_args_t *args, atomic_t *var) {
	atomic_inc(var);
	while(atomic_get(var) < (int)args->cpu_count);
}

/** Second-stage intialization thread.
 * @param args		Kernel arguments structure pointer.
 * @param arg2		Thread argument (unused). */
static void init_thread(void *args, void *arg2) {
	const char *pargs[] = { "/system/services/svcmgr", NULL }, *penv[] = { NULL };
	initcall_t *initcall;
	int ret;

	/* Initialise other things. */
	vfs_init();

	/* Bring up secondary CPUs. The first rendezvous sets off their
	 * initialisation, the second waits for them to complete. */
	init_rendezvous(args, &init_rendezvous_2);
	init_rendezvous(args, &init_rendezvous_3);

	/* Call other initialisation functions. */
	for(initcall = __initcall_start; initcall != __initcall_end; initcall++) {
		(*initcall)();
	}

	console_update_boot_progress(10);

	/* Load modules and mount the root filesystem. */
	load_modules(args);
	vfs_mount_root(args);

	/* Reclaim memory taken up by initialisation code/data. */
	page_late_init();

	console_update_boot_progress(100);

	/* Run the startup process. */
	if((ret = process_create(pargs, penv, PROCESS_CRITICAL, 0, PRIORITY_SYSTEM,
	                         kernel_proc, NULL)) != 0) {
		fatal("Could not create startup process (%d)", ret);
	}
}

/** Main function of the kernel.
 * @param args          Arguments from the bootloader.
 * @param cpu           CPU that the function is running on. */
void __init_text kmain(kernel_args_t *args, uint32_t cpu) {
	thread_t *thread;

	/* Wait for all CPUs to enter the kernel. */
	init_rendezvous(args, &init_rendezvous_1);

	if(cpu == args->boot_cpu) {
		cpu_early_init(args);
		console_early_init();

		/* Perform early architecture/platform initialisation. */
		arch_premm_init(args);
		platform_premm_init(args);

		/* Initialise memory management subsystems. */
		vmem_early_init();
		kheap_early_init();
		vmem_init();
		page_init(args);
		slab_init();
		kheap_init();
		malloc_init();
		vm_init();

		/* Set up the console. */
		console_init(args);
		kprintf(LOG_NORMAL, "kernel: version %s booting (%" PRIu32 " CPU(s))\n",
		        kiwi_ver_string, cpu_count);

		/* Perform second stage architecture/platform initialisation. */
		arch_postmm_init(args);
		platform_postmm_init(args);

		/* Register all other CPUs, and set up the IPI system. */
		cpu_init(args);
		ipi_init();

		/* Bring up the scheduler and friends. */
		process_init();
		thread_init();
		sched_init();
		thread_reaper_init();

		/* Now that the thread system is up and all CPUs have been
		 * regsitered, the slab allocator's reclaim thread can be
		 * started and the magazine layer can be enabled. */
		slab_late_init();

		/* Create the second stage initialisation thread. */
		if(thread_create("init", kernel_proc, 0, init_thread, args, NULL, &thread) != 0) {
			fatal("Could not create second-stage initialisation thread");
		}
		thread_run(thread);

		/* Finally begin executing other threads. */
		sched_enter();
	} else {
		/* Wait for the boot CPU to do its initialisation. */
		init_rendezvous(args, &init_rendezvous_2);

		spinlock_lock(&smp_boot_spinlock, 0);

		/* Switch to the kernel page map, set the CPU pointer and do
		 * architecture/platform initialisation. */
		page_map_switch(&kernel_page_map);
		cpu_set_pointer((ptr_t)cpus[cpu]);
		arch_ap_init(args);
		platform_ap_init(args);

		/* We're running, add ourselves to the running CPU list. */
		list_append(&cpus_running, &curr_cpu->header);

		/* Do scheduler initialisation. */
		sched_init();

		spinlock_unlock(&smp_boot_spinlock);

		/* Perform the final rendezvous and then enter the scheduler. */
		init_rendezvous(args, &init_rendezvous_3);
		sched_enter();
	}
}
