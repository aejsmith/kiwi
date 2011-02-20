/*
 * Copyright (C) 2010-2011 Alex Smith
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
 * @brief		Bootloader main function.
 */

#include <arch/loader.h>

#include <platform/loader.h>

#include <lib/string.h>

#include <config.h>
#include <console.h>
#include <cpu.h>
#include <fs.h>
#include <kargs.h>
#include <loader.h>
#include <memory.h>
#include <menu.h>
#include <video.h>

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
