/*
 * Copyright (C) 2010-2011 Alex Smith
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
 * @brief		x86 CPU initialisation functions.
 */

#include <arch/io.h>
#include <arch/loader.h>

#include <x86/descriptor.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <platform/loader.h>

#include <assert.h>
#include <console.h>
#include <loader.h>
#include <memory.h>
#include <time.h>

/** Structure defining an interrupt stack frame. */
typedef struct interrupt_frame {
	unative_t gs;			/**< GS. */
	unative_t fs;			/**< FS. */
	unative_t es;			/**< ES. */
	unative_t ds;			/**< DS. */
	unative_t di;			/**< EDI. */
	unative_t si;			/**< ESI. */
	unative_t bp;			/**< EBP. */
	unative_t ksp;			/**< ESP (kernel). */
	unative_t bx;			/**< EBX. */
	unative_t dx;			/**< EDX. */
	unative_t cx;			/**< ECX. */
	unative_t ax;			/**< EAX. */
	unative_t int_no;		/**< Interrupt number. */
	unative_t err_code;		/**< Error code (if applicable). */
	unative_t ip;			/**< IP. */
	unative_t cs;			/**< CS. */
	unative_t flags;		/**< FLAGS. */
	unative_t sp;			/**< SP. */
	unative_t ss;			/**< SS. */
} __packed interrupt_frame_t;

/** Frequency of the PIT. */
#define PIT_FREQUENCY		1193182L

/** Number of IDT entries. */
#define IDT_ENTRY_COUNT		32

extern void interrupt_handler(interrupt_frame_t *frame);
extern uint8_t isr_array[IDT_ENTRY_COUNT][16];

/** Frequency of the booting CPU, used by spin(). */
static uint64_t cpu_frequency = 0;

/** Interrupt descriptor table. */
static idt_entry_t loader_idt[IDT_ENTRY_COUNT];

/** IDT pointer pointing to the loader IDT. */
idt_pointer_t loader_idtp = {
	.limit = sizeof(loader_idt) - 1,
	.base = (ptr_t)&loader_idt,
};

/** Read the Time Stamp Counter.
 * @return		Value of the TSC. */
static inline uint64_t rdtsc(void) {
	uint32_t high, low;
	__asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
	return ((uint64_t)high << 32) | low;
}

/** Function to calculate the CPU frequency.
 * @return		Calculated frequency. */
static uint64_t calculate_cpu_frequency(void) {
	uint16_t shi, slo, ehi, elo, ticks;
	uint64_t start, end, cycles;

	/* First set the PIT to rate generator mode. */
	out8(0x43, 0x34);
	out8(0x40, 0xFF);
	out8(0x40, 0xFF);

	/* Wait for the cycle to begin. */
	do {
		out8(0x43, 0x00);
		slo = in8(0x40);
		shi = in8(0x40);
	} while(shi != 0xFF);

	/* Get the start TSC value. */
	start = rdtsc();

	/* Wait for the high byte to drop to 128. */
	do {
		out8(0x43, 0x00);
		elo = in8(0x40);
		ehi = in8(0x40);
	} while(ehi > 0x80);

	/* Get the end TSC value. */
	end = rdtsc();

	/* Calculate the differences between the values. */
	cycles = end - start;
	ticks = ((ehi << 8) | elo) - ((shi << 8) | slo);

	/* Calculate frequency. */
	return (cycles * PIT_FREQUENCY) / ticks;
}

/** Spin for a certain amount of time.
 * @note		There is a BIOS interrupt to wait for a certain time
 *			period (INT15/AH=86h), however we don't use this so
 *			that this function is usable where it is unsuitable to
 *			perform a BIOS call.
 * @param us		Microseconds to delay for. */
void spin(timeout_t us) {
	/* Work out when we will finish */
	uint64_t target = rdtsc() + ((cpu_frequency / 1000000) * us);

	/* Spin until the target is reached. */
	while(rdtsc() < target) {
		__asm__ volatile("pause");
	}
}

/** Handle an exception.
 * @param frame		Interrupt frame. */
void interrupt_handler(interrupt_frame_t *frame) {
	internal_error("Exception %u (error code %u)\n"
	               "cs: 0x%04" PRIxn "  ds: 0x%04" PRIxn "  es: 0x%04" PRIxn "  "
	               "fs: 0x%04" PRIxn "  gs: 0x%04" PRIxn "\n"
	               "eflags: 0x%08" PRIxn "  esp: 0x%08" PRIxn "\n"
	               "eax: 0x%08" PRIxn "  ebx: 0x%08" PRIxn "  ecx: 0x%08" PRIxn "  edx: 0x%08" PRIxn "\n"
	               "edi: 0x%08" PRIxn "  esi: 0x%08" PRIxn "  ebp: 0x%08" PRIxn "  eip: 0x%08" PRIxn,
	               frame->int_no, frame->err_code, frame->cs, frame->ds, frame->es,
	               frame->fs, frame->gs, frame->flags, frame->ksp, frame->ax,
	               frame->bx, frame->cx, frame->dx, frame->di, frame->si, frame->bp,
	               frame->ip);
	while(1);
}

/** Perform initialisation of the CPU. */
void cpu_init(void) {
	ptr_t addr;
	size_t i;

	/* Fill out the handlers in the IDT. */
	for(i = 0; i < IDT_ENTRY_COUNT; i++) {
		addr = (ptr_t)&isr_array[i];
		loader_idt[i].base0 = (addr & 0xFFFF);
		loader_idt[i].base1 = ((addr >> 16) & 0xFFFF);
		loader_idt[i].sel = SEGMENT_CS;
		loader_idt[i].unused = 0;
		loader_idt[i].flags = 0x8E;
	}

	/* Load the new IDT. */
	__asm__ volatile("lidt %0" :: "m"(loader_idtp));

	/* Calculate the CPU frequency for use by spin(). */
	cpu_frequency = calculate_cpu_frequency();
}
