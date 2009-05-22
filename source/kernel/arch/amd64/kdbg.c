/* Kiwi AMD64 kernel debugger functions
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
 * @brief		AMD64 kernel debugger functions.
 */

#include <arch/asm.h>
#include <arch/memmap.h>
#include <arch/page.h>

#include <cpu/intr.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <symtab.h>
#include <kdbg.h>

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
	KDBG_REGISTER_CHECK(name, len, regp, "gs", 2, curr_kdbg_frame->gs);
	KDBG_REGISTER_CHECK(name, len, regp, "fs", 2, curr_kdbg_frame->fs);
	KDBG_REGISTER_CHECK(name, len, regp, "cs", 2, curr_kdbg_frame->cs);
	KDBG_REGISTER_CHECK(name, len, regp, "int_no", 6, curr_kdbg_frame->int_no);
	KDBG_REGISTER_CHECK(name, len, regp, "err_code", 8, curr_kdbg_frame->err_code);
	KDBG_REGISTER_CHECK(name, len, regp, "r15",    3, curr_kdbg_frame->r15);
	KDBG_REGISTER_CHECK(name, len, regp, "r14",    3, curr_kdbg_frame->r14);
	KDBG_REGISTER_CHECK(name, len, regp, "r13",    3, curr_kdbg_frame->r13);
	KDBG_REGISTER_CHECK(name, len, regp, "r12",    3, curr_kdbg_frame->r12);
	KDBG_REGISTER_CHECK(name, len, regp, "r11",    3, curr_kdbg_frame->r11);
	KDBG_REGISTER_CHECK(name, len, regp, "r10",    3, curr_kdbg_frame->r10);
	KDBG_REGISTER_CHECK(name, len, regp, "r9",     2, curr_kdbg_frame->r9);
	KDBG_REGISTER_CHECK(name, len, regp, "r8",     2, curr_kdbg_frame->r8);
	KDBG_REGISTER_CHECK(name, len, regp, "rbp",    3, curr_kdbg_frame->bp);
	KDBG_REGISTER_CHECK(name, len, regp, "rsi",    3, curr_kdbg_frame->si);
	KDBG_REGISTER_CHECK(name, len, regp, "rdi",    3, curr_kdbg_frame->di);
	KDBG_REGISTER_CHECK(name, len, regp, "rdx",    3, curr_kdbg_frame->dx);
	KDBG_REGISTER_CHECK(name, len, regp, "rcx",    3, curr_kdbg_frame->cx);
	KDBG_REGISTER_CHECK(name, len, regp, "rbx",    3, curr_kdbg_frame->bx);
	KDBG_REGISTER_CHECK(name, len, regp, "rax",    3, curr_kdbg_frame->ax);
	KDBG_REGISTER_CHECK(name, len, regp, "rip",    3, curr_kdbg_frame->ip);
	KDBG_REGISTER_CHECK(name, len, regp, "rflags", 6, curr_kdbg_frame->flags);
	KDBG_REGISTER_CHECK(name, len, regp, "rsp",    3, curr_kdbg_frame->sp);
	KDBG_REGISTER_CHECK(name, len, regp, "ss",     2, curr_kdbg_frame->ss);
	kprintf(LOG_KDBG, "KDBG: Invalid register name '%.*s'\n", len, name);
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
		kprintf(LOG_KDBG, "Usage: %s\n\n", argv[0]);

		kprintf(LOG_KDBG, "Prints out the values contained in the current CPU register set. If you wish\n");
		kprintf(LOG_KDBG, "to get the value of a single register, use the 'print' command instead.\n");

		return KDBG_OK;
	}

	kprintf(LOG_KDBG, "cs: 0x%04" PRIxn "  ss: 0x%04" PRIxn "  gs: 0x%04" PRIxn "  fs: 0x%04" PRIxn "\n",
	            curr_kdbg_frame->cs, curr_kdbg_frame->ss, curr_kdbg_frame->gs, curr_kdbg_frame->fs);
	kprintf(LOG_KDBG, "int_no: %" PRIun "  err_code: %" PRIun "  rflags: 0x%016" PRIxn "\n",
	            curr_kdbg_frame->int_no, curr_kdbg_frame->err_code, curr_kdbg_frame->flags);
	kprintf(LOG_KDBG, "rax: 0x%016" PRIxn "  rbx: 0x%016" PRIxn "  rcx: 0x%016" PRIxn "\n",
	            curr_kdbg_frame->ax, curr_kdbg_frame->bx, curr_kdbg_frame->cx);
	kprintf(LOG_KDBG, "rdx: 0x%016" PRIxn "  rdi: 0x%016" PRIxn "  rsi: 0x%016" PRIxn "\n",
	            curr_kdbg_frame->dx, curr_kdbg_frame->di, curr_kdbg_frame->si);
	kprintf(LOG_KDBG, "rbp: 0x%016" PRIxn "  r8:  0x%016" PRIxn "  r9:  0x%016" PRIxn "\n",
	            curr_kdbg_frame->bp, curr_kdbg_frame->r8, curr_kdbg_frame->r9);
	kprintf(LOG_KDBG, "r10: 0x%016" PRIxn "  r11: 0x%016" PRIxn "  r12: 0x%016" PRIxn "\n",
	            curr_kdbg_frame->r10, curr_kdbg_frame->r11, curr_kdbg_frame->r12);
	kprintf(LOG_KDBG, "r13: 0x%016" PRIxn "  r14: 0x%016" PRIxn "  r15: 0x%016" PRIxn "\n",
	            curr_kdbg_frame->r13, curr_kdbg_frame->r14, curr_kdbg_frame->r15);
	kprintf(LOG_KDBG, "rip: 0x%016" PRIxn "  rsp: 0x%016" PRIxn "\n",
	            curr_kdbg_frame->ip, curr_kdbg_frame->sp);
	return KDBG_OK;
}
