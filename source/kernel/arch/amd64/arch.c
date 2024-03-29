/*
 * Copyright (C) 2009-2023 Alex Smith
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
 * @brief               AMD64 architecture main functions.
 */

#include <arch/cpu.h>
#include <arch/io.h>

#include <x86/acpi.h>
#include <x86/console.h>
#include <x86/descriptor.h>
#include <x86/lapic.h>
#include <x86/pic.h>
#include <x86/pit.h>

#include <kernel.h>
#include <time.h>

__init_text void arch_init(void) {
    acpi_init();
    i8042_init();
}

void arch_reboot(void) {
    /* Flush KBoot log. */
    arch_cpu_invalidate_caches();

    /* Try the keyboard controller. */
    uint8_t val;
    do {
        val = in8(0x64);
        if (val & (1 << 0))
            in8(0x60);
    } while (val & (1 << 1));
    out8(0x64, 0xfe);
    spin(msecs_to_nsecs(5));

    /* Fall back on a triple fault. */
    x86_lidt(NULL, 0);
    __asm__ volatile("ud2");
}

void arch_poweroff(void) {
    /* TODO. */
    arch_cpu_halt();
}
