/*
 * Copyright (C) 2013 Alex Smith
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
 * @brief		Main POSIX thread functions.
 */

#include <pthread.h>

/**
 * Get the calling thread's ID.
 *
 * Get the POSIX thread ID of the calling thread. Note that this is not the
 * thread's kernel ID, it is a handle assigned by libsystem and is meaningless
 * to other processes.
 *
 * @return		POSIX thread ID for the calling thread.
 */
pthread_t pthread_self(void) {
	return (void *)0xdeadbeef;
}

/** Determine whether 2 POSIX thread IDs are equal.
 * @param p1		First thread ID.
 * @param p2		Second thread ID.
 * @return		Non-zero if thread IDs are equal, zero otherwise. */
int pthread_equal(pthread_t p1, pthread_t p2) {
	return (p1 == p2);
}
