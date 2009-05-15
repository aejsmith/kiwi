/* Kiwi kernel console functions
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
 * @brief		Kernel console functions.
 */

#include <arch/page.h>

#include <console/console.h>

#include <sync/spinlock.h>

#include <types.h>

/** Number of characters in the log buffer. */
#define KLOG_SIZE	PAGE_SIZE

/** List of kernel consoles. */
static LIST_DECLARE(console_list);
static SPINLOCK_DECLARE(console_lock);

/** Cyclic kernel log buffer. Defined as a structure to make access easier. */
static struct {
	unsigned char level;		/**< Log level. */
	unsigned char ch;		/**< Character. */
} klog_buffer[KLOG_SIZE] __aligned(PAGE_SIZE);

/** Start of the log buffer. */
static uint32_t klog_start = 0;

/** Number of characters in the buffer. */
static uint32_t klog_length = 0;

/** Write a character to the console.
 *
 * Writes a character to all currently registered consoles, and stores it in
 * the kernel log buffer.
 *
 * @param level		Log level.
 * @param ch		Character to print.
 */
void console_putch(unsigned char level, char ch) {
	console_t *cons;
#if !CONFIG_DEBUG
	if(level == LOG_DEBUG) {
		return;
	}
#endif
	spinlock_lock(&console_lock, 0);

	LIST_FOREACH(&console_list, iter) {
		cons = list_entry(iter, console_t, header);

		if((level == LOG_DEBUG && !cons->debug)) {
			continue;
		} else if(unlikely(!cons->putch)) {
			continue;
		}

		cons->putch((unsigned char)ch);
	}

	/* Store in the log buffer. */
	if(level != LOG_FATAL) {
		klog_buffer[(klog_start + klog_length) % KLOG_SIZE].level = level;
		klog_buffer[(klog_start + klog_length) % KLOG_SIZE].ch = (unsigned char)ch;
		if(klog_length < KLOG_SIZE) {
			klog_length++;
		} else {
			klog_start = (klog_start + 1) % KLOG_SIZE;
		}
	}

	spinlock_unlock(&console_lock);
}

/** Register a console.
 *
 * Registers a console to be outputted to upon a console_putch() call. The
 * current contents of the kernel log buffer will be outputted to this console
 * after it has been initialized.
 *
 * @param cons		Console to register.
 */
void console_register(console_t *cons) {
	spinlock_lock(&console_lock, 0);

	list_init(&cons->header);
	list_append(&console_list, &cons->header);

	if(cons->init) {
		cons->init();
	}

	spinlock_unlock(&console_lock);
}

/** Force unlock of console_lock.
 *
 * Forces the console lock to be unlocked. For use during _fatal() calls to
 * ensure that it does not break due to nested locking.
 */
void console_unlock(void) {
	atomic_set(&console_lock.locked, 0);
}
