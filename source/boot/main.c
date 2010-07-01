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

#include <boot/config.h>
#include <boot/console.h>
#include <boot/cpu.h>
#include <boot/fs.h>
#include <boot/memory.h>
#include <boot/menu.h>
#include <boot/video.h>
#include <boot/ui.h>

#include <platform/boot.h>

#include <lib/string.h>

#include <fatal.h>
#include <kargs.h>

extern char __bss_start[], __bss_end[];

extern void loader_main(void);
extern void loader_ap_main(void);

/** Waiting variables for the SMP boot process. */
atomic_t ap_boot_wait = 0;
atomic_t ap_kernel_wait = 0;

#if 0
/** Load the kernel.
 * @param dir		Directory to load from. */
static void load_kernel(fs_node_t *dir) {
	fs_node_t *kernel;

	/* Get the kernel from the directory. */
	if(!(kernel = fs_dir_lookup(dir, "kernel"))) {
		fatal("Couldn't find kernel in boot directory");
	}

	kprintf("Loading kernel...\n");
	arch_load_kernel(kernel);
	fs_node_release(kernel);
}

/** Load all boot modules.
 * @param dir		Directory to load from. */
static void load_modules(fs_node_t *dir) {
	fs_dir_entry_t *entry = NULL;
	phys_ptr_t addr;
	fs_node_t *node;

	/* Get the modules directory. */
	if(!(dir = fs_dir_lookup(dir, "modules"))) {
		fatal("Could not find modules directory");
	}

	while((entry = fs_dir_iterate(dir, entry))) {
		if(!(node = fs_dir_lookup(dir, entry->name))) {
			fatal("Could not get entry from module directory");
		} else if(node->type != FS_NODE_FILE) {
			fs_node_release(node);
			continue;
		}

		kprintf("Loading %s...\n", entry->name);

		/* Allocate a chunk of memory to load to. */
		addr = phys_memory_alloc(ROUND_UP(node->size, PAGE_SIZE), PAGE_SIZE, true);
		if(!fs_file_read(node, (void *)((ptr_t)addr), node->size, 0)) {
			fatal("Could not read module %s", entry->name);
		}

		/* Add the module to the kernel arguments. */
		kargs_module_add(addr, node->size);
		dprintf("loader: loaded module %s to 0x%" PRIpp " (size: %" PRIu64 ")\n",
		        entry->name, addr, node->size);
		fs_node_release(node);
	}

	fs_node_release(dir);
}
#endif

/** Main function for the Kiwi bootloader. */
void loader_main(void) {
	/* Zero BSS. */
	memset(__bss_start, 0, __bss_end - __bss_start);

	/* Initialise the console. */
	console_init();

	/* Perform early architecture/platform initialisation. */
	arch_early_init();
	platform_early_init();

	/* Set up the kernel arguments structure and memory manager, and detect
	 * hardware details. */
	kargs_init();
	cpu_init();
	memory_init();
	disk_init();
	video_init();

	ui_window_t *window = ui_textview_create("Debug Log", debug_log);
	ui_window_display(window, true);

	config_init();

	while(true);
#if 0
	/* Display the configuration interface if the user requested. */
	//menu_display();

	/* Do post-menu CPU intialisation, and detect/boot all other CPUs if
	 * SMP was not disabled in the menu. */
	cpu_postmenu_init();
	if(!kernel_args->smp_disabled) {
		cpu_detect();
		cpu_boot_all();
	}

	/* Load the kernel and modules. */
	if(!(node = fs_find_boot_path(boot_filesystem))) {
		fatal("Couldn't get boot directory");
	}
	load_kernel(node);
	load_modules(node);
	fs_node_release(node);

	/* Set the video mode for the kernel. */
	platform_video_enable();

	/* Write final details to the kernel arguments structure. */
	strncpy(kernel_args->boot_fs_uuid, boot_filesystem->uuid, KERNEL_ARGS_UUID_LEN);
	kernel_args->boot_fs_uuid[KERNEL_ARGS_UUID_LEN - 1] = 0;
	memory_finalise();
	kernel_args->boot_cpu = cpu_current_id();

	/* Enter the kernel. */
	atomic_inc(&ap_kernel_wait);
	arch_enter_kernel();
#endif
}

/** Main function for an AP. */
void loader_ap_main(void) {
	/* Do architecture initialisation and then wake up the boot process. */
	cpu_ap_init();
	atomic_inc(&ap_boot_wait);

	/* Wait until the boot CPU signals that we can boot. */
	while(!atomic_get(&ap_kernel_wait));
	//arch_enter_kernel();
	while(1);
}
