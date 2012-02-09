/*
 * Copyright (C) 2009-2010 Alex Smith
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

#ifndef __PC_CONSOLE_H
#define __PC_CONSOLE_H

/** Convert the serial port number from the configuration to a port number. */
#if CONFIG_PC_SERIAL_PORT == 1
# define SERIAL_PORT		0x3F8
#elif CONFIG_PC_SERIAL_PORT == 2
# define SERIAL_PORT		0x2F8
#elif CONFIG_PC_SERIAL_PORT == 3
# define SERIAL_PORT		0x3E8
#elif CONFIG_PC_SERIAL_PORT == 4
# define SERIAL_PORT		0x2E8
#endif

#endif /* __PC_CONSOLE_H */
