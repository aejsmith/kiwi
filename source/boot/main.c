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

/** Load the kernel. */
static void load_kernel(void) {
	vfs_node_t *dir, *kernel;

	/* Find the boot directory. */
	if(!(dir = vfs_filesystem_boot_path(g_boot_filesystem))) {
		fatal("Couldn't get boot directory");
	}

	/* Get the kernel from the directory. */
	if(!(kernel = vfs_dir_lookup(dir, "kernel"))) {
		fatal("Couldn't find kernel in boot directory");
	}

	/* Ask the architecture to load the kernel. */
	arch_load_kernel(kernel);

	vfs_node_release(kernel);
	vfs_node_release(dir);
}

/** Load all boot modules. */
static void load_modules(void) {

}

/** Main function for the Kiwi bootloader. */
void loader_main(void) {
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
	load_kernel();
	load_modules();

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
