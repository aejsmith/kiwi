/* Kiwi x86 SMP boot code
 * Copyright (C) 2008-2009 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		x86 SMP boot code.
 */

#include <arch/x86/lapic.h>

#include <console/kprintf.h>

#include <cpu/cpu.h>
#include <cpu/intr.h>
#include <cpu/smp.h>

#include <lib/string.h>

#include <mm/kheap.h>
#include <mm/page.h>

#include <time/timer.h>

#include <assert.h>
#include <fatal.h>
#include <kdbg.h>

extern char __ap_trampoline_start[], __ap_trampoline_end[];
extern void __kernel_ap_entry(void);

/** Stack pointer AP should use during boot. */
void *ap_stack_ptr = 0;

/** Waiting variable to wait for CPUs to boot. */
atomic_t ap_boot_wait = 0;

/** Waiting variable for smp_boot_delay(). */
static atomic_t smp_boot_delay_wait = 0;

/** CPU boot delay timer handler. */
static bool smp_boot_delay_handler(void) {
	atomic_set(&smp_boot_delay_wait, 1);
	return false;
}

/** Delay for a number of µseconds during CPU startup.
 * @param us		Number of µseconds to wait. */
static void smp_boot_delay(uint64_t us) {
	timer_t timer;

	atomic_set(&smp_boot_delay_wait, 0);

	timer_init(&timer, TIMER_FUNCTION, smp_boot_delay_handler);
	timer_start(&timer, us * 1000);

	while(atomic_get(&smp_boot_delay_wait) == 0);
}

/** Boot a secondary CPU.
 * @param cpu		CPU to boot. */
static void smp_boot(cpu_t *cpu) {
	uint32_t delay = 0;
	uint32_t *dest;
	size_t size;
	void *stack;

	kprintf(LOG_DEBUG, "cpu: booting CPU %" PRIu32 " (%p)...\n", cpu->id, cpu);
	atomic_set(&ap_boot_wait, 0);

	/* Copy the trampoline code to 0x7000 and set the entry point address. */
	size = (ptr_t)__ap_trampoline_end - (ptr_t)__ap_trampoline_start;
	dest = page_phys_map(0x7000, size, MM_FATAL);
	memcpy(dest, (void *)__ap_trampoline_start, size);
	dest[1] = (uint32_t)KA2PA(__kernel_ap_entry);
	page_phys_unmap(dest, size);

	/* Allocate a new stack and set the CPU structure pointer. */
	stack = kheap_alloc(KSTACK_SIZE, MM_FATAL);
	*(ptr_t *)stack = (ptr_t)cpu;
	ap_stack_ptr = stack + KSTACK_SIZE;

	/* Send an INIT IPI to the AP to reset its state and delay 10ms. */
	lapic_ipi(LAPIC_IPI_DEST_SINGLE, cpu->id, LAPIC_IPI_INIT, 0x00);
	smp_boot_delay(10000);

	/* Send a SIPI. The 0x07 argument specifies where to look for the
	 * bootstrap code, as the SIPI will start execution from 0x000VV000,
	 * where VV is the vector specified in the IPI. We don't do what the
	 * MP Specification says here because QEMU assumes that if a CPU is
	 * halted (even by the 'hlt' instruction) then it can accept SIPIs.
	 * If the CPU reaches the idle loop before the second SIPI is sent, it
	 * will fault. */
	lapic_ipi(LAPIC_IPI_DEST_SINGLE, cpu->id, LAPIC_IPI_SIPI, 0x07);
	smp_boot_delay(10000);

	/* If the CPU is up, then return. */
	if(atomic_get(&ap_boot_wait)) {
		return;
	}

	/* Send a second SIPI and then check in 10ms intervals to see if it
	 * has booted. If it hasn't booted after 5 seconds, fail. */
	lapic_ipi(LAPIC_IPI_DEST_SINGLE, cpu->id, LAPIC_IPI_SIPI, 0x07);
	while(delay < 5000000) {
		if(atomic_get(&ap_boot_wait)) {
			return;
		}
		smp_boot_delay(10000);
		delay += 10000;
	}

	fatal("CPU %" PRIu32 " timed out while booting", cpu->id);
}

/** Boots all detected secondary CPUs. */
void smp_boot_cpus(void) {
	size_t i;

	for(i = 0; i <= cpu_id_max; i++) {
		if(cpus[i]->state == CPU_DOWN) {
			smp_boot(cpus[i]);
		}
	}
}
