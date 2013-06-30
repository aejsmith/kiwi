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
 * @brief		AMD64 kernel debugger functions.
 */

#include <arch/frame.h>
#include <arch/kdb.h>
#include <arch/memory.h>

#include <x86/cpu.h>
#include <x86/lapic.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <proc/thread.h>

#include <kdb.h>

/** Structure containing a stack frame. */
typedef struct stack_frame {
	ptr_t next;			/**< Address of next stack frame. */
	ptr_t addr;			/**< Function return address. */
} stack_frame_t;

/** Structure containing details of a breakpoint. */
typedef struct breakpoint {
	unsigned long dr7;		/**< Value to OR into DR7. */
	ptr_t addr;			/**< Address of the breakpoint. */
} breakpoint_t;

extern void kdb_db_handler(intr_frame_t *frame);

/** Breakpoint/watchpoint tracking structures. */
static breakpoint_t kdb_breakpoints[KDB_BREAKPOINT_COUNT];

/** Set breakpoint settings in the debug registers. */
static inline void setup_debug_regs() {
	unsigned long dr7 = 0;

	x86_write_dr0(kdb_breakpoints[0].addr);
	dr7 |= kdb_breakpoints[0].dr7;
	x86_write_dr1(kdb_breakpoints[1].addr);
	dr7 |= kdb_breakpoints[1].dr7;
	x86_write_dr2(kdb_breakpoints[2].addr);
	dr7 |= kdb_breakpoints[2].dr7;
	x86_write_dr3(kdb_breakpoints[3].addr);
	dr7 |= kdb_breakpoints[3].dr7;

	x86_write_dr7(dr7);
}

/** Enter the debugger.
 * @param reason	Reason for entry to the debugger.
 * @param frame		Interrupt frame.
 * @param index		Index of breakpoint or watchpoint that caused entry. */
static void kdb_enter_internal(kdb_reason_t reason, intr_frame_t *frame, unsigned index) {
	/* Disable breakpoints while KDB is running. */
	x86_write_dr7(0);

	if(kdb_main(reason, frame, index) == KDB_STEP) {
		frame->flags |= X86_FLAGS_TF;
	} else {
		frame->flags &= ~X86_FLAGS_TF;
	}

	/* Work out a new Debug Control Register value. */
	setup_debug_regs();
}

/** Debug interrupt handler.
 * @param frame		Interrupt frame. */
void kdb_db_handler(intr_frame_t *frame) {
	kdb_reason_t reason = KDB_REASON_USER;
	unsigned long dr6;
	unsigned i = 0;

	/* Work out the reason. */
	dr6 = x86_read_dr6();
	if(!(dr6 & (X86_DR6_B0 | X86_DR6_B1 | X86_DR6_B2 | X86_DR6_B3 | X86_DR6_BD | X86_DR6_BS | X86_DR6_BT))) {
		/* No bits set, assume this came from from kdb_enter(), in
		 * which case the reason will be in AX. */
		reason = (kdb_reason_t)frame->ax;
	} else {
		if(dr6 & X86_DR6_BS) {
			reason = KDB_REASON_STEP;
		} else if(dr6 & (X86_DR6_B0 | X86_DR6_B1 | X86_DR6_B2 | X86_DR6_B3)) {
			for(; i < ARRAY_SIZE(kdb_breakpoints); i++) {
				if(frame->ip == kdb_breakpoints[i].addr) {
					reason = KDB_REASON_BREAK;
					break;
				} else if(dr6 & (1UL<<i)) {
					reason = KDB_REASON_WATCH;
					break;
				}
			}
		}
	}

	kdb_enter_internal(reason, frame, i);

	/* Clear the Debug Status Register (DR6). */
	x86_write_dr6(0);

	/* Set the resume flag if resuming from a breakpoint so that we do not
	 * immediately break again. */
	if(reason == KDB_REASON_BREAK)
		frame->flags |= X86_FLAGS_RF;
}

/** Enter the kernel debugger.
 * @param reason	Reason for entry.
 * @param frame		Interrupt frame that caused entry (if NULL one will be
 *			generated). */
void kdb_enter(kdb_reason_t reason, intr_frame_t *frame) {
	if(frame) {
		kdb_enter_internal(reason, frame, 0);
	} else {
		/* Raise a debug interrupt so we can get into the debugger
		 * with an interrupt frame. Store the entry reason in EAX,
		 * which will be picked up in the #DB handler above. */
		__asm__ volatile("int $1" :: "a"((unsigned long)reason));
		return;
	}
}

/** Install a kernel breakpoint.
 * @param addr		Address of the breakpoint.
 * @return		Index of added breakpoint, or -1 if none available. */
int arch_kdb_install_breakpoint(ptr_t addr) {
	size_t i;

	for(i = 0; i < ARRAY_SIZE(kdb_breakpoints); i++) {
		if(kdb_breakpoints[i].dr7)
			continue;

		kdb_breakpoints[i].dr7 = (1<<(1+(i*2)));
		kdb_breakpoints[i].addr = addr;
		return i;
	}

	kdb_printf("No free breakpoint slots.\n");
	return -1;
}

/** Install a kernel watchpoint.
 * @param addr		Address of the watchpoint.
 * @param size		Size of accesses to trigger on.
 * @param rw		Whether to trigger on reads and writes, rather than
 *			just writes.
 * @return		Index of added watchpoint, or -1 if none available. */
int arch_kdb_install_watchpoint(ptr_t addr, size_t size, bool rw) {
	unsigned long dr7;
	size_t i;

	for(i = 0; i < ARRAY_SIZE(kdb_breakpoints); i++) {
		if(kdb_breakpoints[i].dr7)
			continue;

		/* Set the global enable bit for the breakpoint. */
		dr7 = (1<<(1+(i*2)));

		/* Set the condition. */
		dr7 |= (1<<(16+(i*4)));
		if(rw)
			dr7 |= (1<<(17+(i*4)));

		/* Set the size. */
		switch(size) {
		case 1:
			break;
		case 4:
			dr7 |= (1<<(19+(i*4)));
		case 2:
			dr7 |= (1<<(18+(i*4)));
			break;
		case 8:
			dr7 |= (1<<(19+(i*4)));
			break;
		default:
			kdb_printf("Invalid size.\n");
			return -1;
		}

		kdb_breakpoints[i].dr7 = dr7;
		kdb_breakpoints[i].addr = addr;
		return i;
	}

	kdb_printf("No free breakpoint slots.\n");
	return -1;
}

/** Remove a kernel breakpoint.
 * @param index		Index of breakpoint to remove.
 * @return		Whether the breakpoint existed. */
bool arch_kdb_remove_breakpoint(unsigned index) {
	if(index >= ARRAY_SIZE(kdb_breakpoints) || !kdb_breakpoints[index].dr7
		|| (kdb_breakpoints[index].dr7 & ~(1<<(1+(index*2)))))
	{
		kdb_printf("Breakpoint ID %u invalid.\n", index);
		return false;
	}

	kdb_breakpoints[index].dr7 = 0;
	return true;
}

/** Remove a kernel watchpoint.
 * @param index		Index of watchpoint to remove.
 * @return		Whether the breakpoint existed. */
bool arch_kdb_remove_watchpoint(unsigned index) {
	if(index >= ARRAY_SIZE(kdb_breakpoints) || !kdb_breakpoints[index].dr7
		|| !(kdb_breakpoints[index].dr7 & ~(1<<(1+(index*2)))))
	{
		kdb_printf("Watchpoint ID %u invalid.\n", index);
		return false;
	}

	kdb_breakpoints[index].dr7 = 0;
	return true;
}

/** Get details of a breakpoint.
 * @param index		Index of breakpoint.
 * @param addrp		Where to store address of breakpoint.
 * @return		Whether the breakpoint existed. */
bool arch_kdb_get_breakpoint(unsigned index, ptr_t *addrp) {
	if(!kdb_breakpoints[index].dr7 || (kdb_breakpoints[index].dr7 & ~(1<<(1+(index*2)))))
		return false;

	*addrp = kdb_breakpoints[index].addr;
	return true;
}

/** Get details of a watchpoint.
 * @param index		Index of watchpoint.
 * @param addrp		Where to store address of watchpoint.
 * @param sizep		Where to store size of watchpoint.
 * @param rwp		Where to store read-write property.
 * @return		Whether the watchpoint existed. */
bool arch_kdb_get_watchpoint(unsigned index, ptr_t *addrp, size_t *sizep, bool *rwp) {
	if(!kdb_breakpoints[index].dr7 || !(kdb_breakpoints[index].dr7 & ~(1<<(1+(index*2)))))
		return false;

	/* Work out the size. */
	switch((kdb_breakpoints[index].dr7 >> (18 + (index * 4))) & 0x3) {
	case 0:
		*sizep = 1;
		break;
	case 1:
		*sizep = 2;
		break;
	case 2:
		*sizep = 8;
		break;
	case 3:
		*sizep = 4;
		break;
	}

	/* Get RW property. */
	if(kdb_breakpoints[index].dr7 & (1<<(17+(index*4)))) {
		*rwp = true;
	} else {
		*rwp = false;
	}

	*addrp = kdb_breakpoints[index].addr;
	return true;
}

/** Check if an address is within a stack.
 * @param addr		Address to check.
 * @param stack		Stack to check. */
#define IS_IN_STACK(addr, stack)	\
	((addr) >= (ptr_t)(stack) && (addr) < ((ptr_t)(stack) + KSTACK_SIZE))

/** Check if an address lies within the kernel stack.
 * @param thread	Thread to check for.
 * @param addr		Address to check.
 * @return		Whether the address is in the kernel stack. */
static bool is_kstack_address(thread_t *thread, ptr_t addr) {
	if(!thread) {
		thread = curr_thread;
		if(!thread) {
			/* Early boot, assume we are on the kernel stack. */
			return true;
		}
	}

	return (IS_IN_STACK(addr, thread->kstack)
		|| IS_IN_STACK(addr, curr_cpu->arch.double_fault_stack));
}

/** Perform a backtrace.
 * @param thread	Thread to trace. If NULL, use the current frame.
 * @param cb		Backtrace callback. */
void arch_kdb_backtrace(thread_t *thread, kdb_backtrace_cb_t cb) {
	stack_frame_t *frame;
	unsigned long *sp;
	ptr_t bp;

	/* Get the stack frame. */
	if(thread) {
		sp = (unsigned long *)thread->arch.saved_rsp;
		bp = sp[5];
	} else {
		bp = curr_kdb_frame->bp;
	}

	while(bp && is_kstack_address(thread, bp)) {
		frame = (stack_frame_t *)bp;

		if(frame->addr)
			cb(frame->addr);

		bp = frame->next;
	}
}

/** Get the value of a register.
 * @param name		Name of register.
 * @param len		Length of register name.
 * @param regp		Location to store register value in.
 * @return		Whether the register name was valid. */
bool arch_kdb_register_value(const char *name, size_t len, unsigned long *regp) {
	#define KDB_REGISTER_CHECK(rn, rl, rv)	\
		if(len == (rl) && strncasecmp(name, (rn), (rl)) == 0) { \
			*regp = (rv); \
			return true; \
		}

	KDB_REGISTER_CHECK("cs", 2, curr_kdb_frame->cs);
	KDB_REGISTER_CHECK("num", 6, curr_kdb_frame->num);
	KDB_REGISTER_CHECK("err_code", 8, curr_kdb_frame->err_code);
	KDB_REGISTER_CHECK("r15", 3, curr_kdb_frame->r15);
	KDB_REGISTER_CHECK("r14", 3, curr_kdb_frame->r14);
	KDB_REGISTER_CHECK("r13", 3, curr_kdb_frame->r13);
	KDB_REGISTER_CHECK("r12", 3, curr_kdb_frame->r12);
	KDB_REGISTER_CHECK("r11", 3, curr_kdb_frame->r11);
	KDB_REGISTER_CHECK("r10", 3, curr_kdb_frame->r10);
	KDB_REGISTER_CHECK("r9", 2, curr_kdb_frame->r9);
	KDB_REGISTER_CHECK("r8", 2, curr_kdb_frame->r8);
	KDB_REGISTER_CHECK("rbp", 3, curr_kdb_frame->bp);
	KDB_REGISTER_CHECK("rsi", 3, curr_kdb_frame->si);
	KDB_REGISTER_CHECK("rdi", 3, curr_kdb_frame->di);
	KDB_REGISTER_CHECK("rdx", 3, curr_kdb_frame->dx);
	KDB_REGISTER_CHECK("rcx", 3, curr_kdb_frame->cx);
	KDB_REGISTER_CHECK("rbx", 3, curr_kdb_frame->bx);
	KDB_REGISTER_CHECK("rax", 3, curr_kdb_frame->ax);
	KDB_REGISTER_CHECK("rip", 3, curr_kdb_frame->ip);
	KDB_REGISTER_CHECK("rflags", 6, curr_kdb_frame->flags);
	KDB_REGISTER_CHECK("rsp", 3, curr_kdb_frame->sp);
	KDB_REGISTER_CHECK("ss", 2, curr_kdb_frame->ss);
	return false;
}

/** Print out all registers. */
void arch_kdb_dump_registers(void) {
	kdb_printf("RAX: 0x%016lx  RBX: 0x%016lx  RCX: 0x%016lx\n",
		curr_kdb_frame->ax, curr_kdb_frame->bx, curr_kdb_frame->cx);
	kdb_printf("RDX: 0x%016lx  RDI: 0x%016lx  RSI: 0x%016lx\n",
		curr_kdb_frame->dx, curr_kdb_frame->di, curr_kdb_frame->si);
	kdb_printf("RBP: 0x%016lx  R8:  0x%016lx  R9:  0x%016lx\n",
		curr_kdb_frame->bp, curr_kdb_frame->r8, curr_kdb_frame->r9);
	kdb_printf("R10: 0x%016lx  R11: 0x%016lx  R12: 0x%016lx\n",
		curr_kdb_frame->r10, curr_kdb_frame->r11, curr_kdb_frame->r12);
	kdb_printf("R13: 0x%016lx  R14: 0x%016lx  R15: 0x%016lx\n",
		curr_kdb_frame->r13, curr_kdb_frame->r14, curr_kdb_frame->r15);
	kdb_printf("RIP: 0x%016lx  RSP: 0x%016lx  RFL: 0x%016lx\n",
		curr_kdb_frame->ip, curr_kdb_frame->sp, curr_kdb_frame->flags);
	kdb_printf("CS:  0x%04lx  SS: 0x%04lx  EC:  %lu\n",
		curr_kdb_frame->cs, curr_kdb_frame->ss,
		curr_kdb_frame->err_code);
}

#if CONFIG_SMP
/** Trap all other CPUs to wait for KDB to exit. */
void arch_kdb_trap_cpus(void) {
	/* The NMI handler checks kdb_running and spins until it is 0. */
	lapic_ipi(LAPIC_IPI_DEST_ALL, 0, LAPIC_IPI_NMI, 0);
}
#endif

/** Register architecture-specific KDB commands. */
__init_text void arch_kdb_init(void) {
	/* Nothing happens. */
}
