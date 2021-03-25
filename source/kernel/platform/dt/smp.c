/*
 * Copyright (C) 2009-2021 Alex Smith
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
 * @brief               DT SMP detection code.
 */
#include <kernel.h>
#include <smp.h>

/** Detect all secondary CPUs in the system. */
void platform_smp_detect(void) {
    /* TODO */
}

/** Prepare the SMP boot process. */
__init_text void platform_smp_boot_prepare(void) {
    fatal_todo();
}

/** Boot a secondary CPU.
 * @param cpu           CPU to boot. */
__init_text void platform_smp_boot(cpu_t *cpu) {
    fatal_todo();
}

/** Clean up after secondary CPUs have been booted. */
__init_text void platform_smp_boot_cleanup(void) {
    fatal_todo();
}
