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
#include <boot/error.h>
#include <boot/fs.h>
#include <boot/loader.h>
#include <boot/memory.h>
#include <boot/menu.h>
#include <boot/video.h>

#include <platform/boot.h>

#include <lib/string.h>

#include <kargs.h>

extern char __bss_start[], __bss_end[];
extern void loader_main(void);

/** Main function for the Kiwi bootloader. */
void loader_main(void) {
	loader_type_t *type;
	environ_t *env;
	value_t *value;
	disk_t *disk;

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
	config_init();

	/* Display the menu interface. */
	env = menu_display();

	/* Set the current filesystem. */
	if((value = environ_lookup(env, "device")) && value->type == VALUE_TYPE_STRING) {
		if(!(disk = disk_lookup(value->string))) {
			boot_error("Could not find device %s", value->string);
		}
		current_disk = disk;
	}

	/* Load the operating system. */
	type = loader_type_get(env);
	type->load(env);
}
