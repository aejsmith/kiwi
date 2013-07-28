/*
 * Copyright (C) 2010-2013 Alex Smith
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
 * @brief		POSIX time function.
 */

#include <kernel/time.h>
#include <sys/time.h>
#include <errno.h>

/** Get the current time.
 * @param tv		Structure to fill with time since epoch.
 * @param tz		Pointer to timezone (ignored).
 * @return		0 on success, -1 on failure. */
int gettimeofday(struct timeval *tv, void *tz) {
	nstime_t ktime;

	kern_unix_time(&ktime);
	tv->tv_sec = ktime / 1000000000;
	tv->tv_usec = ktime % 1000000000;
	return 0;
}
