/*
 * Copyright (C) 2009-2020 Alex Smith
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
 * @brief               PC SMP detection code.
 */

#include <x86/lapic.h>
#include <x86/smp.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/phys.h>

#include <pc/acpi.h>

#include <assert.h>
#include <kernel.h>
#include <smp.h>

/** Detect all secondary CPUs in the system. */
void platform_smp_detect(void) {
    /* If the LAPIC is disabled, we cannot use SMP. */
    if (!lapic_enabled()) {
        return;
    } else if (!acpi_supported) {
        return;
    }

    acpi_madt_t *madt = (acpi_madt_t *)acpi_table_find(ACPI_MADT_SIGNATURE);
    if (!madt)
        return;

    size_t length = madt->header.length - sizeof(acpi_madt_t);

    acpi_madt_lapic_t *lapic;
    for (size_t i = 0; i < length; i += lapic->length) {
        lapic = (acpi_madt_lapic_t *)(madt->apic_structures + i);
        if (lapic->type != ACPI_MADT_LAPIC) {
            continue;
        } else if (!(lapic->flags & (1<<0))) {
            /* Ignore disabled processors. */
            continue;
        } else if (lapic->lapic_id == curr_cpu->id) {
            continue;
        }

        cpu_register(lapic->lapic_id, CPU_OFFLINE);
    }
    
    return;
}

/** Prepare the SMP boot process. */
__init_text void platform_smp_boot_prepare(void) {
    x86_smp_boot_prepare();
}

/** Boot a secondary CPU.
 * @param cpu           CPU to boot. */
__init_text void platform_smp_boot(cpu_t *cpu) {
    x86_smp_boot(cpu);
}

/** Clean up after secondary CPUs have been booted. */
__init_text void platform_smp_boot_cleanup(void) {
    x86_smp_boot_cleanup();
}
