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
 * @brief		AMD64 SMP boot code.
 */

#include <arch/barrier.h>
#include <arch/memory.h>

#include <x86/lapic.h>

#include <cpu/cpu.h>

#include <lib/string.h>

#include <mm/heap.h>
#include <mm/page.h>
#include <mm/phys.h>

#include <assert.h>
#include <console.h>
#include <time.h>

extern char __ap_trampoline_start[], __ap_trampoline_end[];
extern void kmain_ap(cpu_t *cpu);

/** Page reserved to copy the AP boostrap code to. */
phys_ptr_t ap_bootstrap_page = 0;

/** Atomic variable for paused CPUs to wait on. */
atomic_t cpu_pause_wait = 0;

/** Whether cpu_halt_all() has been called. */
atomic_t cpu_halting_all = 0;

/** Pause execution of other CPUs.
 *
 * Pauses execution of all CPUs other than the CPU that calls the function.
 * This is done using an NMI, so CPUs will be paused even if they have
 * interrupts disabled. Use cpu_resume_all() to resume CPUs after using this
 * function.
 */
void cpu_pause_all(void) {
	cpu_t *cpu;

	atomic_set(&cpu_pause_wait, 1);

	LIST_FOREACH(&running_cpus, iter) {
		cpu = list_entry(iter, cpu_t, header);
		if(cpu->id != cpu_current_id()) {
			lapic_ipi(LAPIC_IPI_DEST_SINGLE, cpu->id, LAPIC_IPI_NMI, 0);
		}
	}
}

/** Resume CPUs paused with cpu_pause_all(). */
void cpu_resume_all(void) {
	atomic_set(&cpu_pause_wait, 0);
}

/** Halt all other CPUs. */
void cpu_halt_all(void) {
	cpu_t *cpu;

	atomic_set(&cpu_halting_all, 1);

	/* Have to do this rather than just use LAPIC_IPI_DEST_ALL, because
	 * during early boot, secondary CPUs do not have an IDT set up so
	 * sending them an NMI IPI results in a triple fault. */
	LIST_FOREACH(&running_cpus, iter) {
		cpu = list_entry(iter, cpu_t, header);
		if(cpu->id != cpu_current_id()) {
			lapic_ipi(LAPIC_IPI_DEST_SINGLE, cpu->id, LAPIC_IPI_NMI, 0);
		}
	}
}

/** Boot a secondary CPU.
 * @param cpu		CPU to boot. */
void cpu_boot(cpu_t *cpu) {
	page_map_t *pmap;
	useconds_t delay;
	void *mapping;

	kprintf(LOG_DEBUG, "cpu: booting CPU %" PRIu32 "...\n", cpu->id);
	assert(lapic_enabled());
	cpu_boot_wait = 0;

	/* Allocate a double fault stack for the new CPU. This is also used as
	 * the initial stack while initialising the AP, before it enters the
	 * scheduler. */
	cpu->arch.double_fault_stack = heap_alloc(KSTACK_SIZE, MM_FATAL);

	/* Create a temporary page map for the AP to use while booting. This is
	 * necessary because the bootstrap code needs to be identity mapped
	 * while it enables paging. */
	pmap = page_map_create(MM_FATAL);
	page_map_lock(pmap);
	page_map_insert(pmap, (ptr_t)ap_bootstrap_page, ap_bootstrap_page, true, true, MM_FATAL);
	page_map_unlock(pmap);

	/* Copy the trampoline code to the page reserved by the paging
	 * initialisation code. */
	mapping = phys_map(ap_bootstrap_page, PAGE_SIZE, MM_FATAL);
	memcpy(mapping, __ap_trampoline_start, __ap_trampoline_end - __ap_trampoline_start);

	/* Fill in details required by the bootstrap code. */
	*(uint64_t *)(mapping + 16) = (ptr_t)kmain_ap;
	*(uint64_t *)(mapping + 24) = (ptr_t)cpu;
	*(uint64_t *)(mapping + 32) = (ptr_t)cpu->arch.double_fault_stack + KSTACK_SIZE;
	*(uint32_t *)(mapping + 40) = (ptr_t)pmap->cr3;

	memory_barrier();
	phys_unmap(mapping, PAGE_SIZE, true);

	/* Send an INIT IPI to the AP to reset its state and delay 10ms. */
	lapic_ipi(LAPIC_IPI_DEST_SINGLE, cpu->id, LAPIC_IPI_INIT, 0x00);
	spin(10000);

	/* Send a SIPI. The vector argument specifies where to look for the
	 * bootstrap code, as the SIPI will start execution from 0x000VV000,
	 * where VV is the vector specified in the IPI. We don't do what the
	 * MP Specification says here because QEMU assumes that if a CPU is
	 * halted (even by the 'hlt' instruction) then it can accept SIPIs.
	 * If the CPU reaches the idle loop before the second SIPI is sent, it
	 * will fault. */
	lapic_ipi(LAPIC_IPI_DEST_SINGLE, cpu->id, LAPIC_IPI_SIPI, ap_bootstrap_page >> 12);
	spin(10000);

	/* If the CPU is up, then return. */
	if(cpu_boot_wait) {
		return;
	}

	/* Send a second SIPI and then check in 10ms intervals to see if it
	 * has booted. If it hasn't booted after 5 seconds, fail. */
	lapic_ipi(LAPIC_IPI_DEST_SINGLE, cpu->id, LAPIC_IPI_SIPI, ap_bootstrap_page >> 12);
	for(delay = 0; delay < 5000000; delay += 10000) {
		if(cpu_boot_wait) {
			page_map_destroy(pmap);
			return;
		}
		spin(10000);
	}

	fatal("CPU %" PRIu32 " timed out while booting", cpu->id);
}
