/*
 * Copyright (C) 2009 Alex Smith
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
 * @brief		PC console code.
 */

#include <arch/io.h>
#include <platform/pc/console.h>
#include <kernel.h>

#ifdef SERIAL_PORT
/** Print a character to the serial console.
 * @param ch		Character to print. */
static void serial_console_putch(unsigned char ch) {
	while(!(in8(SERIAL_PORT + 5) & 0x20));
	if(ch == '\n') {
		serial_console_putch('\r');
	}
	out8(SERIAL_PORT, ch);
}

/** Initialise the serial console. */
static void serial_console_init(void) {
        out8(SERIAL_PORT + 1, 0x00);  /* Disable all interrupts */
        out8(SERIAL_PORT + 3, 0x80);  /* Enable DLAB (set baud rate divisor) */
        out8(SERIAL_PORT + 0, 0x03);  /* Set divisor to 3 (lo byte) 38400 baud */
        out8(SERIAL_PORT + 1, 0x00);  /*                  (hi byte) */
        out8(SERIAL_PORT + 3, 0x03);  /* 8 bits, no parity, one stop bit */
        out8(SERIAL_PORT + 2, 0xC7);  /* Enable FIFO, clear them, with 14-byte threshold */
        out8(SERIAL_PORT + 4, 0x0B);  /* IRQs enabled, RTS/DSR set */
}

/** Serial port console. */
static console_t serial_console = {
#if CONFIG_DEBUG
	.min_level = LOG_DEBUG,
#else
	.min_level = LOG_NORMAL,
#endif
	.init = serial_console_init,
	.putch = serial_console_putch,
};
#endif

/** Set up the console. */
void __init_text console_early_init(void) {
#ifdef SERIAL_PORT
	uint8_t status = in8(SERIAL_PORT + 6);

	/* Only add the serial device when it is present. */
	if((status & ((1<<4) | (1<<5))) && status != 0xFF) {
		console_register(&serial_console);
	}
#endif
}
