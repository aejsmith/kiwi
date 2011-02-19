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
 * @brief		x86 exception handling routines.
 */

#include <arch/x86/descriptor.h>
#include <arch/boot.h>
#include <arch/intr.h>

#include <boot/error.h>

#include <platform/boot.h>

/** Number of IDT entries. */
#define IDT_ENTRY_COUNT		32

extern void interrupt_handler(intr_frame_t *frame);
extern uint8_t isr_array[IDT_ENTRY_COUNT][16];

/** Interrupt descriptor table. */
static idt_entry_t loader_idt[IDT_ENTRY_COUNT];

/** IDT pointer loaded into the IDTR register. */
idt_pointer_t loader_idtp = {
	.limit = sizeof(loader_idt) - 1,
	.base = (ptr_t)&loader_idt,
};

/** Initialise the IDT. */
void idt_init(void) {
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
}

/** Handle an exception.
 * @param frame		Interrupt frame. */
void interrupt_handler(intr_frame_t *frame) {
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
