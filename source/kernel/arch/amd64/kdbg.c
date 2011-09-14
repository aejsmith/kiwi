/*
 * Copyright (C) 2008-2011 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
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

#include <x86/cpu.h>

#include <cpu/intr.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <proc/thread.h>

#include <symbol.h>
#include <kdbg.h>

extern void kdbg_db_handler(unative_t num, intr_frame_t *frame);

/** Structure containing a stack frame. */
typedef struct stack_frame {
	struct stack_frame *next;	/**< Pointer to next stack frame. */
	ptr_t addr;			/**< Function return address. */
} stack_frame_t;

/** Structure containing details of a breakpoint. */
typedef struct breakpoint {
	unative_t dr7;			/**< Value to OR into DR7. */
	ptr_t addr;			/**< Address of the breakpoint. */
} breakpoint_t;

/** Breakpoint/watchpoint tracking structures. */
static breakpoint_t kdbg_breakpoints[4];

/** Set breakpoint settings in the debug registers. */
static inline void kdbg_setup_dreg(void) {
	unative_t dr7 = 0;

	x86_write_dr0(kdbg_breakpoints[0].addr);
	dr7 |= kdbg_breakpoints[0].dr7;
	x86_write_dr1(kdbg_breakpoints[1].addr);
	dr7 |= kdbg_breakpoints[1].dr7;
	x86_write_dr2(kdbg_breakpoints[2].addr);
	dr7 |= kdbg_breakpoints[2].dr7;
	x86_write_dr3(kdbg_breakpoints[3].addr);
	dr7 |= kdbg_breakpoints[3].dr7;

	x86_write_dr7(dr7);
}

/** Debug exception (#DB) handler.
 * @param num		Interrupt number.
 * @param frame		Interrupt stack frame. */
void kdbg_db_handler(unative_t num, intr_frame_t *frame) {
	int reason = KDBG_ENTRY_USER;
	unative_t dr6;
	size_t i;

	/* Work out the reason. */
	dr6 = x86_read_dr6();
	if(!(dr6 & (X86_DR6_B0 | X86_DR6_B1 | X86_DR6_B2 | X86_DR6_B3 | X86_DR6_BD |
	            X86_DR6_BS | X86_DR6_BT))) {
		/* No bits set, assume this came from from kdbg_enter(), in
		 * which case the reason will be in EAX. */
		reason = (unative_t)frame->ax;
	} else {
		if(dr6 & X86_DR6_BS) {
			reason = KDBG_ENTRY_STEPPED;
		} else if(dr6 & (X86_DR6_B0 | X86_DR6_B1 | X86_DR6_B2 | X86_DR6_B3)) {
			for(i = 0; i < ARRAYSZ(kdbg_breakpoints); i++) {
				if(frame->ip == kdbg_breakpoints[i].addr) {
					reason = KDBG_ENTRY_BREAK;
					kdbg_breakpoint_id = i;
				} else if(dr6 & (1UL<<i)) {
					reason = KDBG_ENTRY_WATCH;
					kdbg_breakpoint_id = i;
				}
			}
		}
	}

	kdbg_enter(reason, frame);

	/* Clear the Debug Status Register (DR6). */
	x86_write_dr6(0);

	/* Set the resume flag if resuming from a breakpoint. */
	if(reason == KDBG_ENTRY_BREAK) {
		frame->flags |= X86_FLAGS_RF;
	}
}

/** Call KDBG.
 *
 * Wrapper function for kdbg_main(). If a register structure is provided,
 * then directly calls kdbg_main(), otherwise raises a debug interrupt to
 * generate a register structure and enter KDBG.
 *
 * @param reason	KDBG entry reason.
 * @param frame		Interrupt stack frame (if NULL will generate one).
 */
void kdbg_enter(int reason, intr_frame_t *frame) {
	if(frame == NULL) {
		/* Raise a debug interrupt so we can get into the debugger
		 * with an interrupt frame. Store the entry reason in EAX. */
		__asm__ volatile("int $1" :: "a"((unative_t)reason));
		return;
	}

	/* Disable breakpoints while KDBG is running. */
	x86_write_dr7(0);

	if(kdbg_main(reason, frame) == KDBG_STEP) {
		frame->flags |= X86_FLAGS_TF;
	} else {
		frame->flags &= ~X86_FLAGS_TF;
	}

	/* Work out a new Debug Control Register value. */
	kdbg_setup_dreg();
}

/** KDBG backtrace command.
 * @param argc		Argument count.
 * @param argv		Argument pointer array.
 * @return		KDBG_OK on success, KDBG_FAIL on failure. */
int kdbg_cmd_backtrace(int argc, char **argv) {
	stack_frame_t *frame;
	thread_t *thread;
	size_t off = 0;
	unative_t tid;
	symbol_t *sym;
	ptr_t ip;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s [<thread ID>]\n\n", argv[0]);
		kprintf(LOG_NONE, "Prints out a backtrace for a thread, or the current kernel stack if no\n");
		kprintf(LOG_NONE, "thread specified.\n");
		return KDBG_OK;
	} else if(argc != 1 && argc != 2) {
		kprintf(LOG_NONE, "Incorrect number of arguments. See 'help %s' for more information.\n", argv[0]);
		return KDBG_FAIL;
	}

	/* Get the stack frame. */
	if(argc == 2) {
		if(kdbg_parse_expression(argv[1], &tid, NULL) != KDBG_OK) {
			return KDBG_FAIL;
		} else if(!(thread = thread_lookup_unsafe(tid))) {
			kprintf(LOG_NONE, "Invalid thread ID.\n");
			return KDBG_FAIL;
		}

		frame = (stack_frame_t *)thread->context.bp;
		ip = thread->context.ip;
	} else {
		frame = (stack_frame_t *)curr_kdbg_frame->bp;
		ip = curr_kdbg_frame->ip;
	}

	/* Print out the address of where the exception occurred. */
	sym = symbol_lookup_addr(ip, &off);
	kprintf(LOG_NONE, "--- Interrupt ---\n");
	kprintf(LOG_NONE, "[%p] %s+0x%zx\n", ip, (sym) ? sym->name : "<unknown>", off);

	kprintf(LOG_NONE, "--- Stacktrace ---\n");
	while(frame && frame->addr) {
		sym = symbol_lookup_addr(frame->addr, &off);
		kprintf(LOG_NONE, "[%p] %s+0x%zx\n", frame->addr,
		        (sym) ? sym->name : "<unknown>", off);
		frame = frame->next;
	}

	return KDBG_OK;
}

/** Create a breakpoint.
 * @param argc		Argument count.
 * @param argv		Argument pointer array.
 * @return		KDBG_OK on success, KDBG_FAIL on failure. */
int kdbg_cmd_break(int argc, char **argv) {
	size_t i, off = 0;
	unative_t addr;
	symbol_t *sym;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s <address>\n\n", argv[0]);

		kprintf(LOG_NONE, "Creates a new breakpoint at the given address. The address is treated as an\n");
		kprintf(LOG_NONE, "expression. Be warned that older versions of QEMU do not support breakpoints\n");
		kprintf(LOG_NONE, "well.\n");
		return KDBG_OK;
	} else if(argc != 2) {
		kprintf(LOG_NONE, "Incorrect number of arguments. See 'help %s' for more information.\n", argv[0]);
		return KDBG_FAIL;
	}

	if(kdbg_parse_expression(argv[1], &addr, NULL) != KDBG_OK) {
		return KDBG_FAIL;
	}

	/* Search for a free slot. */
	for(i = 0; i < ARRAYSZ(kdbg_breakpoints); i++) {
		if(kdbg_breakpoints[i].dr7) {
			continue;
		}

		kdbg_breakpoints[i].dr7 = (1<<(1+(i*2)));
		kdbg_breakpoints[i].addr = addr;

		sym = symbol_lookup_addr(kdbg_breakpoints[i].addr, &off);
		kprintf(LOG_NONE, "Created breakpoint %zu [%p] %s+0x%zx\n",
		        i, addr, (sym) ? sym->name : "<unknown>", off);
		return KDBG_OK;
	}

	kprintf(LOG_NONE, "No free breakpoint slots.\n");
	return KDBG_FAIL;
}

/** Delete a breakpoint/watchpoint.
 * @param argc		Argument count.
 * @param argv		Argument pointer array.
 * @return		KDBG_OK on success, KDBG_FAIL on failure. */
int kdbg_cmd_delete(int argc, char **argv) {
	unsigned long num;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s <id>\n\n", argv[0]);
		kprintf(LOG_NONE, "Deletes the breakpoint/watchpoint with the given ID.\n");
		return KDBG_OK;
	} else if(argc < 2) {
		kprintf(LOG_NONE, "ID expected. See 'help %s' for more information.\n", argv[0]);
		return KDBG_FAIL;
	}

	num = strtoul(argv[1], NULL, 0);

	if(num >= ARRAYSZ(kdbg_breakpoints) || !kdbg_breakpoints[num].dr7) {
		kprintf(LOG_NONE, "Breakpoint/watchpoint ID %lu invalid.\n", num);
		return KDBG_FAIL;
	}

	kdbg_breakpoints[num].dr7 = 0;
	kdbg_breakpoints[num].addr = 0;
	return KDBG_OK;
}

/** Create or list breakpoints.
 * @param argc		Argument count.
 * @param argv		Argument pointer array.
 * @return		KDBG_OK on success, KDBG_FAIL on failure. */
int kdbg_cmd_list(int argc, char **argv) {
	symbol_t *sym;
	size_t i, off;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s\n\n", argv[0]);
		kprintf(LOG_NONE, "List all breakpoints and watchpoints.\n");
		return KDBG_OK;
	}

	for(i = 0; i < ARRAYSZ(kdbg_breakpoints); i++) {
		if(!kdbg_breakpoints[i].dr7) {
			continue;
		}

		sym = symbol_lookup_addr(kdbg_breakpoints[i].addr, &off);
		kprintf(LOG_NONE, "%spoint %zu: [%p] %s+0x%zx\n",
			(kdbg_breakpoints[i].dr7 == (1UL<<(1+(i*2)))) ? "Break" : "Watch",
		        i, kdbg_breakpoints[i].addr, (sym) ? sym->name : "<unknown>",
		        off);
	}

	return KDBG_OK;
}

/** Create a watchpoint.
 * @param argc		Argument count.
 * @param argv		Argument pointer array.
 * @return		KDBG_OK on success, KDBG_FAIL on failure. */
int kdbg_cmd_watch(int argc, char **argv) {
	unative_t addr, size, dr7 = 0;
	size_t i, off = 0;
	bool rw = false;
	symbol_t *sym;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s [--rw] <address> <size>\n\n", argv[0]);

		kprintf(LOG_NONE, "Creates a new watchpoint at the given address. The address is treated as an\n");
		kprintf(LOG_NONE, "expression. If the '--rw' argument is given, the watchpoint will trigger on\n");
		kprintf(LOG_NONE, "reads and writes, rather than just writes. Be warned that older versions of\n");
		kprintf(LOG_NONE, "QEMU do not support watchpoints well.\n");
		return KDBG_OK;
	} else if(argc != 3 && argc != 4) {
		kprintf(LOG_NONE, "Incorrect number of arguments. See 'help %s' for more information.\n", argv[0]);
		return KDBG_FAIL;
	}

	if(argc == 4) {
		if(strcmp(argv[1], "--rw") == 0) {
			rw = true;
		} else {
			kprintf(LOG_NONE, "Unknown argument. See 'help %s' for more information.\n", argv[0]);
			return KDBG_FAIL;
		}
	}

	if(kdbg_parse_expression(argv[argc - 2], &addr, NULL) != KDBG_OK) {
		return KDBG_FAIL;
	} else if(kdbg_parse_expression(argv[argc - 1], &size, NULL) != KDBG_OK) {
		return KDBG_FAIL;
	}

	/* Search for a free slot. */
	for(i = 0; i < ARRAYSZ(kdbg_breakpoints); i++) {
		if(kdbg_breakpoints[i].dr7) {
			continue;
		}

		/* Set the global enable bit for the breakpoint. */
		dr7 = (1<<(1+(i*2)));

		/* Set the condition. */
		dr7 |= (1<<(16+(i*4)));
		if(rw) {
			dr7 |= (1<<(17+(i*4)));
		}

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
			kprintf(LOG_NONE, "Invalid size.\n");
			return KDBG_FAIL;
		}

		kdbg_breakpoints[i].dr7 = dr7;
		kdbg_breakpoints[i].addr = addr;

		sym = symbol_lookup_addr(kdbg_breakpoints[i].addr, &off);
		kprintf(LOG_NONE, "Created watchpoint %zu [%p] %s+0x%zx\n",
		        i, addr, (sym) ? sym->name : "<unknown>", off);
		return KDBG_OK;
	}

	kprintf(LOG_NONE, "No free breakpoint slots.\n");
	return KDBG_FAIL;
}

/** Helper macro for kdbg_register_value(). */
#define KDBG_REGISTER_CHECK(n, l, p, rn, rl, rv)	\
	if((l) == (rl) && strncmp((n), (rn), (l)) == 0) { \
		*(p) = (rv); \
		return KDBG_OK; \
	}

/** Get the value of a register.
 *
 * Gets the value of a register and stores it in the given location.
 *
 * @param name		Name of register.
 * @param len		Length of register name.
 * @param regp		Location to store register value in.
 *
 * @return		KDBG_OK on success, KDBG_FAIL on failure.
 */
int kdbg_register_value(const char *name, size_t len, unative_t *regp) {
	KDBG_REGISTER_CHECK(name, len, regp, "cs", 2, curr_kdbg_frame->cs);
	KDBG_REGISTER_CHECK(name, len, regp, "int_no", 6, curr_kdbg_frame->int_no);
	KDBG_REGISTER_CHECK(name, len, regp, "err_code", 8, curr_kdbg_frame->err_code);
	KDBG_REGISTER_CHECK(name, len, regp, "r15", 3, curr_kdbg_frame->r15);
	KDBG_REGISTER_CHECK(name, len, regp, "r14", 3, curr_kdbg_frame->r14);
	KDBG_REGISTER_CHECK(name, len, regp, "r13", 3, curr_kdbg_frame->r13);
	KDBG_REGISTER_CHECK(name, len, regp, "r12", 3, curr_kdbg_frame->r12);
	KDBG_REGISTER_CHECK(name, len, regp, "r11", 3, curr_kdbg_frame->r11);
	KDBG_REGISTER_CHECK(name, len, regp, "r10", 3, curr_kdbg_frame->r10);
	KDBG_REGISTER_CHECK(name, len, regp, "r9", 2, curr_kdbg_frame->r9);
	KDBG_REGISTER_CHECK(name, len, regp, "r8", 2, curr_kdbg_frame->r8);
	KDBG_REGISTER_CHECK(name, len, regp, "rbp", 3, curr_kdbg_frame->bp);
	KDBG_REGISTER_CHECK(name, len, regp, "rsi", 3, curr_kdbg_frame->si);
	KDBG_REGISTER_CHECK(name, len, regp, "rdi", 3, curr_kdbg_frame->di);
	KDBG_REGISTER_CHECK(name, len, regp, "rdx", 3, curr_kdbg_frame->dx);
	KDBG_REGISTER_CHECK(name, len, regp, "rcx", 3, curr_kdbg_frame->cx);
	KDBG_REGISTER_CHECK(name, len, regp, "rbx", 3, curr_kdbg_frame->bx);
	KDBG_REGISTER_CHECK(name, len, regp, "rax", 3, curr_kdbg_frame->ax);
	KDBG_REGISTER_CHECK(name, len, regp, "rip", 3, curr_kdbg_frame->ip);
	KDBG_REGISTER_CHECK(name, len, regp, "rflags", 6, curr_kdbg_frame->flags);
	KDBG_REGISTER_CHECK(name, len, regp, "rsp", 3, curr_kdbg_frame->sp);
	KDBG_REGISTER_CHECK(name, len, regp, "ss", 2, curr_kdbg_frame->ss);
	kprintf(LOG_NONE, "KDBG: Invalid register name '%.*s'\n", len, name);
	return KDBG_FAIL;
}

/** Print out all registers.
 *
 * Prints out the value of all registers in the current KDBG register set.
 *
 * @param argc		Argument count.
 * @param argv		Argument array.
 *
 * @return		Always returns KDBG_OK.
 */
int kdbg_cmd_regs(int argc, char **argv) {
	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s\n\n", argv[0]);

		kprintf(LOG_NONE, "Prints out the values contained in the current CPU register set. If you wish\n");
		kprintf(LOG_NONE, "to get the value of a single register, use the 'print' command instead.\n");

		return KDBG_OK;
	}

	kprintf(LOG_NONE, "cs: 0x%04" PRIxN "  ss: 0x%04" PRIxN "\n",
	            curr_kdbg_frame->cs, curr_kdbg_frame->ss);
	kprintf(LOG_NONE, "int_no: %" PRIuN "  err_code: %" PRIuN "  rflags: 0x%016" PRIxN "\n",
	            curr_kdbg_frame->int_no, curr_kdbg_frame->err_code, curr_kdbg_frame->flags);
	kprintf(LOG_NONE, "rax: 0x%016" PRIxN "  rbx: 0x%016" PRIxN "  rcx: 0x%016" PRIxN "\n",
	            curr_kdbg_frame->ax, curr_kdbg_frame->bx, curr_kdbg_frame->cx);
	kprintf(LOG_NONE, "rdx: 0x%016" PRIxN "  rdi: 0x%016" PRIxN "  rsi: 0x%016" PRIxN "\n",
	            curr_kdbg_frame->dx, curr_kdbg_frame->di, curr_kdbg_frame->si);
	kprintf(LOG_NONE, "rbp: 0x%016" PRIxN "  r8:  0x%016" PRIxN "  r9:  0x%016" PRIxN "\n",
	            curr_kdbg_frame->bp, curr_kdbg_frame->r8, curr_kdbg_frame->r9);
	kprintf(LOG_NONE, "r10: 0x%016" PRIxN "  r11: 0x%016" PRIxN "  r12: 0x%016" PRIxN "\n",
	            curr_kdbg_frame->r10, curr_kdbg_frame->r11, curr_kdbg_frame->r12);
	kprintf(LOG_NONE, "r13: 0x%016" PRIxN "  r14: 0x%016" PRIxN "  r15: 0x%016" PRIxN "\n",
	            curr_kdbg_frame->r13, curr_kdbg_frame->r14, curr_kdbg_frame->r15);
	kprintf(LOG_NONE, "rip: 0x%016" PRIxN "  rsp: 0x%016" PRIxN "\n",
	            curr_kdbg_frame->ip, curr_kdbg_frame->sp);
	return KDBG_OK;
}
