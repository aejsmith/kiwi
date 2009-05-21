/* Kiwi AMD64 miscellaneous ASM functions
 * Copyright (C) 2007-2009 Alex Smith
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
 * @brief		AMD64 miscellaneous ASM functions.
 */

#ifndef __ARCH_ASM_H
#define __ARCH_ASM_H

#include <arch/memmap.h>

#include <types.h>

/*
 * Control/debug register functions.
 */

/** Macro to define a control/debug register read function. */
#define READ_CD_REG(name, type)		\
	static inline type read_ ## name(void) { \
		type r; \
		__asm__ volatile("mov %%" #name ", %0" : "=r"(r)); \
		return r; \
	}

/** Macro to define a control/debug register read function. */
#define WRITE_CD_REG(name, type)	\
	static inline void write_ ## name(type val) { \
		__asm__ volatile("mov %0, %%" #name :: "r"(val)); \
	}

/** Read the CR0 register.
 * @return		Value of the CR0 register. */
READ_CD_REG(cr0, unative_t);

/** Read the CR2 register.
 * @return		Value of the CR2 register. */
READ_CD_REG(cr2, unative_t);

/** Read the CR3 register.
 * @return		Value of the CR3 register. */
READ_CD_REG(cr3, unative_t);

/** Read the CR4 register.
 * @return		Value of the CR4 register. */
READ_CD_REG(cr4, unative_t);

/** Write the CR0 register.
 * @param val		New value of the CR0 register. */
WRITE_CD_REG(cr0, unative_t);

/** Write the CR3 register.
 * @param val		New value of the CR3 register. */
WRITE_CD_REG(cr3, unative_t);

/** Write the CR4 register.
 * @param val		New value of the CR4 register. */
WRITE_CD_REG(cr4, unative_t);

/** Read the DR0 register.
 * @return		Value of the DR0 register. */
READ_CD_REG(dr0, unative_t);

/** Read the DR1 register.
 * @return		Value of the DR1 register. */
READ_CD_REG(dr1, unative_t);

/** Read the DR2 register.
 * @return		Value of the DR2 register. */
READ_CD_REG(dr2, unative_t);

/** Read the DR3 register.
 * @return		Value of the DR3 register. */
READ_CD_REG(dr3, unative_t);

/** Read the DR6 register.
 * @return		Value of the DR6 register. */
READ_CD_REG(dr6, unative_t);

/** Read the DR7 register.
 * @return		Value of the DR7 register. */
READ_CD_REG(dr7, unative_t);

/** Write the DR0 register.
 * @param val		New value of the DR0 register. */
WRITE_CD_REG(dr0, unative_t);

/** Write the DR1 register.
 * @param val		New value of the DR1 register. */
WRITE_CD_REG(dr1, unative_t);

/** Write the DR2 register.
 * @param val		New value of the DR2 register. */
WRITE_CD_REG(dr2, unative_t);

/** Write the DR3 register.
 * @param val		New value of the DR3 register. */
WRITE_CD_REG(dr3, unative_t);

/** Write the DR6 register.
 * @param val		New value of the DR6 register. */
WRITE_CD_REG(dr6, unative_t);

/** Write the DR7 register.
 * @param val		New value of the DR7 register. */
WRITE_CD_REG(dr7, unative_t);

#undef READ_CD_REG
#undef WRITE_CD_REG

/*
 * Model specific register functions.
 */

/** Write an MSR.
 * @param msr		MSR to write to.
 * @param value		Value to write. */
static inline void wrmsr(int msr, uint64_t value) {
	__asm__ volatile("wrmsr" :: "a"((uint32_t)value), "d"((uint32_t)(value >> 32)), "c"(msr));
}

/** Read an MSR.
 * @param msr		MSR to read.
 * @return		Value read. */
static inline uint64_t rdmsr(int msr) {
	uint32_t low, high;

	__asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
	return (((uint64_t)high) << 32) | low;
}

/*
 * Stack functions.
 */

/** Get the current stack pointer.
 * @return		Current stack pointer. */
static inline ptr_t read_sp(void) {
	ptr_t ret;

	__asm__ volatile("movq %%rsp, %0" : "=r"(ret));
	return ret;
}

/** Get the base of the current stack.
 * @return		Base of current stack. */
static inline unative_t *get_stack_base(void) {
	return (unative_t *)(read_sp() & ~(KSTACK_SIZE - 1));
}

/*
 * Flags functions.
 */

/** Get EFLAGS.
 * @return		Current value of EFLAGS. */
static inline unative_t get_flags(void) {
	unative_t flags;

	__asm__ volatile("pushf; pop %0" : "=rm"(flags));
	return flags;
}

/** Set EFLAGS.
 * @param flags		New value for EFLAGS. */
static inline void set_flags(unative_t flags) {
	__asm__ volatile("push %0; popf" :: "rm"(flags));
}

/*
 * x87 FPU functions.
 */

/** Initialize FPU state. */
static inline void fninit(void) {
	__asm__ volatile("fninit");
}

/** Save FPU state.
 * @param area		Area to save to. */
static inline void fnsave(char *area) {
	__asm__ volatile("fnsave %0" :: "m"(*area));
}

/** Restore FPU state.
 * @param area		Area to restore from. */
static inline void frstor(char *area) {
	__asm__ volatile("frstor %0" :: "m"(*area));
}

/** Save FPU state.
 * @param area		Area to save to. */
static inline void fxsave(char *area) {
	__asm__ volatile("fxsave %0" :: "m"(*area));
}

/** Restore FPU state.
 * @param area		Area to restore from. */
static inline void fxrstor(char *area) {
	__asm__ volatile("fxrstor %0" :: "m"(*area));
}

/*
 * Miscellaneous.
 */

/** Spin loop hint using the PAUSE instruction to be more friendly to certain
 * CPUs (Pentium 4 and Xeon, mostly) in terms of performance and energy
 * consumption - see PAUSE instruction in Intel Instruction Set Reference N-Z
 * manual for more information. */
static inline void spin_loop_hint(void) {
	__asm__ volatile("pause");
}

/** Place the CPU in an idle state until an interrupt occurs. */
static inline void idle(void) {
	__asm__ volatile("sti; hlt; cli");
}

/** Load a value into TR (Task Register).
 * @param sel		Selector to load. */
static inline void ltr(uint32_t sel) {
	__asm__ volatile("ltr %%ax" :: "a"(sel));
}

/** Set the GDTR register.
 * @param base		Virtual address of GDT.
 * @param limit		Size of GDT. */
static inline void lgdt(ptr_t base, uint16_t limit) {
	gdt_pointer_t gdtp;

	gdtp.limit = limit;
	gdtp.base = base;

	__asm__ volatile("lgdt %0" :: "m"(gdtp));
}

/** Set the IDTR register.
 * @param base		Base address of IDT.
 * @param limit		Size of IDT. */
static inline void lidt(ptr_t base, uint16_t limit) {
	idt_pointer_t idtp;

	idtp.limit = limit;
	idtp.base = base;

	__asm__ volatile("lidt %0" :: "m"(idtp));
}

/** Invalidate a TLB entry.
 * @param addr		Address to invalidate. */
static inline void invlpg(ptr_t addr) {
	__asm__ volatile("invlpg (%0)" :: "r"(addr));
}

/** Set the GS register.
 * @param sel		Selector to set. */
static inline void set_gs(uint32_t sel) {
	__asm__ volatile("mov %%ax, %%gs" :: "a"(sel));
}

/** Execute the CPUID instruction.
 * @param level		CPUID level.
 * @param a		Where to store EAX value.
 * @param b		Where to store EBX value.
 * @param c		Where to store ECX value.
 * @param d		Where to store EDX value. */
static inline void cpuid(uint32_t level, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
	__asm__ volatile("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "0"(level));
}

/** Execute the SWAPGS instruction. */
static inline void swapgs(void) {
	__asm__ volatile("swapgs");
}

#endif /* __ARCH_ASM_H */
