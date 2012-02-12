/*
 * Copyright (C) 2008-2011 Alex Smith
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
 * @brief		AMD64 SMP support.
 */

#include <arch/barrier.h>
#include <arch/memory.h>

#include <x86/lapic.h>
#include <x86/mmu.h>
#include <x86/tsc.h>

#include <lib/string.h>

#include <mm/kmem.h>
#include <mm/mmu.h>
#include <mm/page.h>
#include <mm/phys.h>

#include <assert.h>
#include <kernel.h>
#include <smp.h>
#include <time.h>

extern char __ap_trampoline_start[], __ap_trampoline_end[];
extern void kmain_ap(cpu_t *cpu);

/** MMU context used by APs while booting. */
static mmu_context_t *ap_mmu_context;

/** Page reserved to copy the AP bootstrap code to. */
phys_ptr_t ap_bootstrap_page = 0;

/** Send an IPI interrupt to a single CPU.
 * @param dest		Destination CPU ID. */
void arch_smp_ipi(cpu_id_t dest) {
	lapic_ipi(LAPIC_IPI_DEST_SINGLE, (uint32_t)dest, LAPIC_IPI_FIXED, LAPIC_VECT_IPI);
}

/** Prepare the SMP boot process. */
__init_text void arch_smp_boot_prepare(void) {
	void *mapping;

	/* Copy the trampoline code to the page reserved by the paging
	 * initialization code. */
	mapping = phys_map(ap_bootstrap_page, PAGE_SIZE, MM_FATAL);
	memcpy(mapping, __ap_trampoline_start, __ap_trampoline_end - __ap_trampoline_start);
	phys_unmap(mapping, PAGE_SIZE, false);

	/* Create a temporary MMU context for APs to use while booting which
	 * identity maps the bootstrap code at its physical location. */
	ap_mmu_context = mmu_context_create(MM_FATAL);
	mmu_context_lock(ap_mmu_context);
	mmu_context_map(ap_mmu_context, (ptr_t)ap_bootstrap_page, ap_bootstrap_page, true, true, MM_FATAL);
	mmu_context_unlock(ap_mmu_context);
}

/** Start the target CPU and wait until it is alive.
 * @param id		CPU ID to boot.
 * @return		Whether the CPU responded in time. */
static __init_text bool boot_cpu_and_wait(cpu_id_t id) {
	useconds_t delay;

	/* Send an INIT IPI to the AP to reset its state and delay 10ms. */
	lapic_ipi(LAPIC_IPI_DEST_SINGLE, id, LAPIC_IPI_INIT, 0x00);
	spin(10000);

	/* Send a SIPI. The vector argument specifies where to look for the
	 * bootstrap code, as the SIPI will start execution from 0x000VV000,
	 * where VV is the vector specified in the IPI. We don't do what the
	 * MP Specification says here because QEMU assumes that if a CPU is
	 * halted (even by the 'hlt' instruction) then it can accept SIPIs.
	 * If the CPU reaches the idle loop before the second SIPI is sent, it
	 * will fault. */
	lapic_ipi(LAPIC_IPI_DEST_SINGLE, id, LAPIC_IPI_SIPI, ap_bootstrap_page >> 12);
	spin(10000);

	/* If the CPU is up, then return. */
	if(smp_boot_status > SMP_BOOT_INIT) {
		return true;
	}

	/* Send a second SIPI and then check in 10ms intervals to see if it
	 * has booted. If it hasn't booted after 5 seconds, fail. */
	lapic_ipi(LAPIC_IPI_DEST_SINGLE, id, LAPIC_IPI_SIPI, ap_bootstrap_page >> 12);
	for(delay = 0; delay < 5000000; delay += 10000) {
		if(smp_boot_status > SMP_BOOT_INIT) {
			return true;
		}
		spin(10000);
	}

	return false;
}

/** Boot a secondary CPU.
 * @param cpu		CPU to boot. */
__init_text void arch_smp_boot(cpu_t *cpu) {
	void *mapping;

	kprintf(LOG_DEBUG, "cpu: booting CPU %" PRIu32 "...\n", cpu->id);
	assert(lapic_enabled());

	/* Allocate a double fault stack for the new CPU. This is also used as
	 * the initial stack while initializing the AP, before it enters the
	 * scheduler. */
	cpu->arch.double_fault_stack = kmem_alloc(KSTACK_SIZE, MM_FATAL);

	/* Fill in details required by the bootstrap code. */
	mapping = phys_map(ap_bootstrap_page, PAGE_SIZE, MM_FATAL);
	*(uint64_t *)(mapping + 16) = (ptr_t)kmain_ap;
	*(uint64_t *)(mapping + 24) = (ptr_t)cpu;
	*(uint64_t *)(mapping + 32) = (ptr_t)cpu->arch.double_fault_stack + KSTACK_SIZE;
	*(uint32_t *)(mapping + 40) = (ptr_t)ap_mmu_context->pml4;
	memory_barrier();
	phys_unmap(mapping, PAGE_SIZE, false);

	/* Kick the CPU into life. */
	if(!boot_cpu_and_wait(cpu->id)) {
		fatal("CPU %" PRIu32 " timed out while booting", cpu->id);
	}

	/* The TSC of the AP must be synchronised against the boot CPU. */
	tsc_init_source();

	/* Finally, wait for the CPU to complete its initialization. */
	while(smp_boot_status != SMP_BOOT_BOOTED) {
		cpu_spin_hint();
	}
}

/** Clean up after secondary CPUs have been booted. */
__init_text void arch_smp_boot_cleanup(void) {
	/* Destroy the temporary MMU context. */
	mmu_context_destroy(ap_mmu_context);

	/* Free the bootstrap page. */
	phys_free(ap_bootstrap_page, PAGE_SIZE);
}
