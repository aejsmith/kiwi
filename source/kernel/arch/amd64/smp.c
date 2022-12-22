/*
 * Copyright (C) 2009-2022 Alex Smith
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
 * @brief               AMD64 SMP support.
 */

#include <arch/barrier.h>
#include <arch/stack.h>

#include <x86/acpi.h>
#include <x86/lapic.h>
#include <x86/mmu.h>
#include <x86/smp.h>
#include <x86/tsc.h>

#include <lib/string.h>

#include <mm/aspace.h>
#include <mm/kmem.h>
#include <mm/mmu.h>
#include <mm/page.h>
#include <mm/phys.h>

#include <assert.h>
#include <kernel.h>
#include <smp.h>
#include <time.h>

/** MMU context used by APs while booting. */
static mmu_context_t *ap_mmu_context;

/** Page reserved to copy the AP bootstrap code to. */
static phys_ptr_t ap_bootstrap_page;

/** Send an IPI interrupt to a single CPU.
 * @param dest          Destination CPU ID. */
void arch_smp_ipi(cpu_id_t dest) {
    lapic_ipi(LAPIC_IPI_DEST_SINGLE, (uint32_t)dest, LAPIC_IPI_FIXED, LAPIC_VECT_IPI);
}

/** Detect all secondary CPUs in the system. */
void arch_smp_detect(void) {
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
__init_text void arch_smp_boot_prepare(void) {
    /* Allocate a low memory page for the trampoline code. */
    phys_alloc(PAGE_SIZE, 0, 0, 0, 0x100000, MM_BOOT, &ap_bootstrap_page);

    /* Copy the trampoline code to the page reserved by the paging
     * initialization code. */
    void *mapping = phys_map(ap_bootstrap_page, PAGE_SIZE, MM_BOOT);
    memcpy(mapping, __ap_trampoline_start, __ap_trampoline_end - __ap_trampoline_start);
    phys_unmap(mapping, PAGE_SIZE);

    /* Create a temporary MMU context for APs to use while booting which
     * identity maps the bootstrap code at its physical location. */
    ap_mmu_context = mmu_context_create(MM_BOOT);
    mmu_context_lock(ap_mmu_context);
    mmu_context_map(
        ap_mmu_context,
        (ptr_t)ap_bootstrap_page,
        ap_bootstrap_page,
        MMU_ACCESS_READ | MMU_ACCESS_WRITE | MMU_ACCESS_EXECUTE,
        MM_BOOT);
    mmu_context_unlock(ap_mmu_context);
}

/** Start the target CPU and wait until it is alive.
 * @param id            CPU ID to boot.
 * @return              Whether the CPU responded in time. */
static __init_text bool boot_cpu_and_wait(cpu_id_t id) {
    /* Send an INIT IPI to the AP to reset its state and delay 10ms. */
    lapic_ipi(LAPIC_IPI_DEST_SINGLE, id, LAPIC_IPI_INIT, 0x00);
    spin(msecs_to_nsecs(10));

    /* Send a SIPI. The vector argument specifies where to look for the
     * bootstrap code, as the SIPI will start execution from 0x000VV000, where
     * VV is the vector specified in the IPI. We don't do what the MP
     * Specification says here because QEMU assumes that if a CPU is halted
     * (even by the 'hlt' instruction) then it can accept SIPIs. If the CPU
     * reaches the idle loop before the second SIPI is sent, it will fault. */
    lapic_ipi(LAPIC_IPI_DEST_SINGLE, id, LAPIC_IPI_SIPI, ap_bootstrap_page >> 12);
    spin(msecs_to_nsecs(10));

    /* If the CPU is up, then return. */
    if (smp_boot_status > SMP_BOOT_INIT)
        return true;

    /* Send a second SIPI and then check in 10ms intervals to see if it has
     * booted. If it hasn't booted after 5 seconds, fail. */
    lapic_ipi(LAPIC_IPI_DEST_SINGLE, id, LAPIC_IPI_SIPI, ap_bootstrap_page >> 12);
    for (nstime_t delay = 0; delay < secs_to_nsecs(5); delay += msecs_to_nsecs(10)) {
        if (smp_boot_status > SMP_BOOT_INIT)
            return true;

        spin(msecs_to_nsecs(10));
    }

    return false;
}

/** Boot a secondary CPU.
 * @param cpu           CPU to boot. */
__init_text void arch_smp_boot(cpu_t *cpu) {
    kprintf(LOG_DEBUG, "cpu: booting CPU %" PRIu32 "...\n", cpu->id);
    assert(lapic_enabled());

    /* Allocate a double fault stack for the new CPU. This is also used as the
     * initial stack while initializing the AP, before it enters the scheduler. */
    cpu->arch.double_fault_stack = kmem_alloc(KSTACK_SIZE, MM_BOOT);

    /* Fill in details required by the bootstrap code. */
    void *mapping = phys_map(ap_bootstrap_page, PAGE_SIZE, MM_BOOT);
    *(uint64_t *)(mapping + 16) = (ptr_t)kmain_secondary;
    *(uint64_t *)(mapping + 24) = (ptr_t)cpu;
    *(uint64_t *)(mapping + 32) = (ptr_t)cpu->arch.double_fault_stack + KSTACK_SIZE;
    *(uint32_t *)(mapping + 40) = (ptr_t)ap_mmu_context->arch.pml4;
    memory_barrier();
    phys_unmap(mapping, PAGE_SIZE);

    /* Kick the CPU into life. */
    if (!boot_cpu_and_wait(cpu->id))
        fatal("CPU %" PRIu32 " timed out while booting", cpu->id);

    /* The TSC of the AP must be synchronised against the boot CPU. */
    tsc_init_source();

    /* Finally, wait for the CPU to complete its initialization. */
    while (smp_boot_status != SMP_BOOT_BOOTED)
        arch_cpu_spin_hint();
}

/** Clean up after secondary CPUs have been booted. */
__init_text void arch_smp_boot_cleanup(void) {
    /* Destroy the temporary MMU context. */
    mmu_context_destroy(ap_mmu_context);

    /* Free the bootstrap page. */
    phys_free(ap_bootstrap_page, PAGE_SIZE);
}
