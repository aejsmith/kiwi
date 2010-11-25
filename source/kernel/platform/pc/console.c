/*
 * Copyright (C) 2009 Alex Smith
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
 * @brief		PC console code.
 */

#include <arch/io.h>
#include <platform/console.h>
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
