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
 * @brief		PC platform startup code.
 */

#include <arch/x86/descriptor.h>
#include <arch/io.h>

#include <boot/config.h>

#include <lib/string.h>

#include <platform/boot.h>

#include <time.h>

#include "multiboot.h"

extern char *video_mode_override;

/** Early PC platform startup code. */
void platform_early_init(void) {
	char *tok, *cmdline;

	/* If booted with Multiboot, parse the command line. */
	if(multiboot_magic == MB_LOADER_MAGIC) {
		cmdline = multiboot_cmdline;
		while((tok = strsep(&cmdline, " "))) {
			if(strncmp(tok, "video-mode=", 11) == 0) {
				video_mode_override = tok + 11;
			} else if(strncmp(tok, "config-file=", 12) == 0) {
				config_file_override = tok + 12;
			}
		}
	}
}

/** Reboot the system. */
void platform_reboot(void) {
	uint8_t val;

	/* Try the keyboard controller. */
	do {
		val = in8(0x64);
		if(val & (1<<0)) {
			in8(0x60);
		}
	} while(val & (1<<1));
	out8(0x64, 0xfe);
	spin(5000);

	/* Fall back on a triple fault. */
	lidt(0, 0);
	__asm__ volatile("ud2");
}
