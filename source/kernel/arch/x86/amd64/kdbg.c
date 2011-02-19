/*
 * Copyright (C) 2008-2009 Alex Smith
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

#include <cpu/intr.h>

#include <lib/string.h>
#include <lib/utility.h>

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

	kprintf(LOG_NONE, "cs: 0x%04" PRIxn "  ss: 0x%04" PRIxn "\n",
	            curr_kdbg_frame->cs, curr_kdbg_frame->ss);
	kprintf(LOG_NONE, "int_no: %" PRIun "  err_code: %" PRIun "  rflags: 0x%016" PRIxn "\n",
	            curr_kdbg_frame->int_no, curr_kdbg_frame->err_code, curr_kdbg_frame->flags);
	kprintf(LOG_NONE, "rax: 0x%016" PRIxn "  rbx: 0x%016" PRIxn "  rcx: 0x%016" PRIxn "\n",
	            curr_kdbg_frame->ax, curr_kdbg_frame->bx, curr_kdbg_frame->cx);
	kprintf(LOG_NONE, "rdx: 0x%016" PRIxn "  rdi: 0x%016" PRIxn "  rsi: 0x%016" PRIxn "\n",
	            curr_kdbg_frame->dx, curr_kdbg_frame->di, curr_kdbg_frame->si);
	kprintf(LOG_NONE, "rbp: 0x%016" PRIxn "  r8:  0x%016" PRIxn "  r9:  0x%016" PRIxn "\n",
	            curr_kdbg_frame->bp, curr_kdbg_frame->r8, curr_kdbg_frame->r9);
	kprintf(LOG_NONE, "r10: 0x%016" PRIxn "  r11: 0x%016" PRIxn "  r12: 0x%016" PRIxn "\n",
	            curr_kdbg_frame->r10, curr_kdbg_frame->r11, curr_kdbg_frame->r12);
	kprintf(LOG_NONE, "r13: 0x%016" PRIxn "  r14: 0x%016" PRIxn "  r15: 0x%016" PRIxn "\n",
	            curr_kdbg_frame->r13, curr_kdbg_frame->r14, curr_kdbg_frame->r15);
	kprintf(LOG_NONE, "rip: 0x%016" PRIxn "  rsp: 0x%016" PRIxn "\n",
	            curr_kdbg_frame->ip, curr_kdbg_frame->sp);
	return KDBG_OK;
}
