/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
