/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Kernel console device request types.
 */

#ifndef __KERNEL_CONSOLE_H
#define __KERNEL_CONSOLE_H

/** Request types for the kernel console device. */
#define KCONSOLE_GET_LOG_SIZE		32	/**< Get the size of the log buffer. */
#define KCONSOLE_READ_LOG		33	/**< Read the log buffer. */
#define KCONSOLE_CLEAR_LOG		34	/**< Clear the log buffer. */
#define KCONSOLE_UPDATE_PROGRESS	35	/**< Update the boot progress bar. */

#endif /* __KERNEL_CONSOLE_H */
