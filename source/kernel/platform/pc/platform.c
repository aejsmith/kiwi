/*
 * Copyright (C) 2009-2011 Alex Smith
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
 * @brief		PC platform core code.
 */

#include <arch/cpu.h>
#include <arch/io.h>

#include <x86/descriptor.h>
#include <x86/lapic.h>

#include <pc/acpi.h>
#include <pc/console.h>
#include <pc/pic.h>
#include <pc/pit.h>

#include <kernel.h>
#include <kboot.h>
#include <time.h>

/** PC platform initialization. */
__init_text void platform_init(void) {
	pic_init();
	acpi_init();

	/* If the LAPIC is not available, we must use the PIT as the timer. */
	if(!lapic_enabled())
		pit_init();

	i8042_init();
}

/** Reboot the system. */
void platform_reboot(void) {
	uint8_t val;

	arch_cpu_invalidate_caches();

	/* Try the keyboard controller. */
	do {
		val = in8(0x64);
		if(val & (1<<0))
			in8(0x60);
	} while(val & (1<<1));
	out8(0x64, 0xfe);
	spin(MSECS2NSECS(5));

	/* Fall back on a triple fault. */
	x86_lidt(NULL, 0);
	__asm__ volatile("ud2");
}

/** Power off the system. */
void platform_poweroff(void) {
	/* TODO. */
	arch_cpu_halt();
}
