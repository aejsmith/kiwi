/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Fatal error function.
 */

#include <boot/console.h>
#include <lib/printf.h>
#include <fatal.h>

/** Helper for fatal_printf().
 * @param ch		Character to display.
 * @param data		If not NULL, newlines will be padded.
 * @param total		Pointer to total character count. */
static void fatal_printf_helper(char ch, void *data, int *total) {
	debug_console.putch(ch);
	main_console.putch(ch);
	if(ch == '\n' && data) {
		debug_console.putch(' ');
		debug_console.putch(' ');
		main_console.putch(' ');
		main_console.putch(' ');
	}
	*total = *total + 1;
}

/** Formatted print function for fatal(). */
static void fatal_printf(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	do_printf(fatal_printf_helper, NULL, fmt, args);
	va_end(args);
}

/** Structure containing a stack frame. */
typedef struct stack_frame {
	struct stack_frame *next;	/**< Pointer to next stack frame. */
	ptr_t addr;			/**< Function return address. */
} stack_frame_t;

static void backtrace(void) {
	stack_frame_t *frame;
	ptr_t addr = 0;

	__asm__ volatile("mov %%ebp, %0" : "=r"(addr));
	frame = (stack_frame_t *)addr;

	fatal_printf("\n--- Stacktrace ---\n");
	while(frame) {
		fatal_printf("%p\n", frame->addr);
		frame = frame->next;
	}
}

/** Display a fatal error message and halt execution.
 * @param fmt		Format string for error message.
 * @param ...		Arguments to substitute into format. */
void fatal(const char *fmt, ...) {
	va_list args;

	main_console.clear();
	fatal_printf("\nA fatal error occurred while trying to load Kiwi:\n\n  ");

	va_start(args, fmt);
	do_printf(fatal_printf_helper, (void *)1, fmt, args);
	va_end(args);

	fatal_printf("\n\n");
	fatal_printf("Ensure that you have enough memory in your system, and that you do\n");
	fatal_printf("not have any malfunctioning hardware. If the problem persists, please\n");
	fatal_printf("report it to http://kiwi.alex-smith.me.uk/\n");

	backtrace();
	while(1);
}
