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

#include <boot/vfs.h>

#include <lib/string.h>

#include <platform/boot.h>
#include <platform/multiboot.h>

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
			} else if(strncmp(tok, "boot-path=", 10) == 0) {
				boot_path_override = tok + 10;
			}
		}
	}
}