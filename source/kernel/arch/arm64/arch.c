/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 architecture main functions.
 */

#include <arch/cpu.h>

#include <console.h>
#include <kboot.h>
#include <kernel.h>

__init_text void arch_console_early_init(kboot_tag_video_t *video, kboot_tag_serial_t *serial) {
    /* We rely on generic code to set everything up from what KBoot gave us. */
}

__init_text void arch_init(void) {
    
}

void arch_reboot(void) {
    fatal_todo();
}

void arch_poweroff(void) {
    /* TODO. */
    arch_cpu_halt();
}
