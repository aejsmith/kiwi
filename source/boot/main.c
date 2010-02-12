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
 * @brief		Bootloader main function.
 */

#include <arch/boot.h>

#include <boot/console.h>
#include <boot/cpu.h>
#include <boot/memory.h>
#include <boot/menu.h>
#include <boot/vfs.h>

#include <platform/boot.h>

#include <lib/string.h>

#include <fatal.h>
#include <kargs.h>

extern char __bss_start[], __bss_end[];

extern void loader_main(void);
extern void loader_ap_main(void);

/** Waiting variables for the SMP boot process. */
volatile int g_ap_boot_wait = 0;
volatile int g_ap_kernel_wait = 0;

/** Load the kernel.
 * @param dir		Directory to load from. */
static void load_kernel(vfs_node_t *dir) {
	vfs_node_t *kernel;

	/* Get the kernel from the directory. */
	if(!(kernel = vfs_dir_lookup(dir, "kernel"))) {
		fatal("Couldn't find kernel in boot directory");
	}

	arch_load_kernel(kernel);
	vfs_node_release(kernel);
}

/** Load all boot modules.
 * @param dir		Directory to load from. */
static void load_modules(vfs_node_t *dir) {
	vfs_dir_entry_t *entry = NULL;
	vfs_node_t *node;
	phys_ptr_t addr;

	/* Get the modules directory. */
	if(!(dir = vfs_dir_lookup(dir, "modules"))) {
		fatal("Could not find modules directory");
	}

	while((entry = vfs_dir_iterate(dir, entry))) {
		if(!(node = vfs_dir_lookup(dir, entry->name))) {
			fatal("Could not get entry from module directory");
		} else if(node->type != VFS_NODE_FILE) {
			vfs_node_release(node);
			continue;
		}

		/* Allocate a chunk of memory to load to. */
		addr = phys_memory_alloc(ROUND_UP(node->size, PAGE_SIZE), PAGE_SIZE, true);
		if(!vfs_file_read(node, (void *)((ptr_t)addr), node->size, 0)) {
			fatal("Could not read module %s", entry->name);
		}

		/* Add the module to the kernel arguments. */
		kargs_module_add(addr, node->size);
		dprintf("loader: loaded module %s to 0x%" PRIpp " (size: %" PRIu64 ")\n",
		        entry->name, addr, node->size);
		vfs_node_release(node);
	}

	vfs_node_release(dir);
}

/** Main function for the Kiwi bootloader. */
void loader_main(void) {
	vfs_node_t *node;

	/* Zero BSS. */
	memset(__bss_start, 0, __bss_end - __bss_start);

	/* Initialise the consoles. */
	g_console.init();
	g_debug_console.init();

	/* Perform early architecture/platform initialisation. */
	arch_early_init();
	platform_early_init();

	/* Set up the kernel arguments structure and memory manager, and detect
	 * hardware details. */
	kargs_init();
	cpu_early_init();
	memory_init();
	disk_init();
	platform_video_init();

	/* Display the configuration interface if the user requested. */
	menu_display();

	/* Do post-menu CPU intialisation, and detect all other CPUs if SMP
	 * was not disabled in the menu. */
	cpu_postmenu_init();
	if(!g_kernel_args->smp_disabled) {
		cpu_detect();
	}

	/* Load the kernel and modules. */
	if(!(node = vfs_filesystem_boot_path(g_boot_filesystem))) {
		fatal("Couldn't get boot directory");
	}
	load_kernel(node);
	load_modules(node);
	vfs_node_release(node);

	/* Boot all CPUs, and do final CPU setup. */
	cpu_boot_all();

	/* Set the video mode for the kernel. */
	platform_video_enable();

	/* Write final details to the kernel arguments structure. */
	strncpy(g_kernel_args->boot_fs_uuid, g_boot_filesystem->uuid, KERNEL_ARGS_UUID_LEN);
	g_kernel_args->boot_fs_uuid[KERNEL_ARGS_UUID_LEN - 1] = 0;
	memory_finalise();
	g_kernel_args->boot_cpu = cpu_current_id();

	/* Enter the kernel. */
	g_ap_kernel_wait = 1;
	arch_enter_kernel();
}

/** Main function for an AP. */
void loader_ap_main(void) {
	/* Do architecture initialisation and then wake up the boot process. */
	cpu_ap_init();
	g_ap_boot_wait = 1;

	/* Wait until the boot CPU signals that we can boot. */
	while(!g_ap_kernel_wait);
	arch_enter_kernel();
}
