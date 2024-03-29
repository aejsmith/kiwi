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
 * @brief               ARM64 SMP detection code.
 */
#include <kernel.h>
#include <smp.h>

/** Send an IPI interrupt to a single CPU.
 * @param dest          Destination CPU ID. */
void arch_smp_ipi(cpu_id_t dest) {
    fatal_todo();
}

/** Detect all secondary CPUs in the system. */
void arch_smp_detect(void) {
    /* TODO */
}

/** Prepare the SMP boot process. */
__init_text void arch_smp_boot_prepare(void) {
    /* TODO. */
}

/** Boot a secondary CPU.
 * @param cpu           CPU to boot. */
__init_text void arch_smp_boot(cpu_t *cpu) {
    fatal_todo();
}

/** Clean up after secondary CPUs have been booted. */
__init_text void arch_smp_boot_cleanup(void) {
    /* TODO. */
}
