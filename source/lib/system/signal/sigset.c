/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief               Signal set manipulation functions.
 */

#include <errno.h>
#include <signal.h>

/** Add a signal to a signal set.
 * @param set           Set to add to.
 * @param num           Signal to add.
 * @return              0 on success, -1 on failure. */
int sigaddset(sigset_t *set, int num) {
    if (num < 1 || num >= NSIG) {
        errno = EINVAL;
        return -1;
    }

    *set |= (1 << num);
    return 0;
}

/** Remove a signal from a signal set.
 * @param set           Set to remove from.
 * @param num           Signal to remove.
 * @return              0 on success, -1 on failure. */
int sigdelset(sigset_t *set, int num) {
    if (num < 1 || num >= NSIG) {
        errno = EINVAL;
        return -1;
    }

    *set &= ~(1 << num);
    return 0;
}

/** Clear all signals in a signal set.
 * @param set           Set to clear.
 * @return              Always 0. */
int sigemptyset(sigset_t *set) {
    *set = 0;
    return 0;
}

/** Set all signals in a signal set.
 * @param set           Set to fill.
 * @return              Always 0. */
int sigfillset(sigset_t *set) {
    *set = -1;
    return 0;
}

/** Check if a signal is included in a set.
 * @param set           Set to check.
 * @param num           Signal number to check for.
 * @return              1 if member, 0 if not, -1 if signal number is invalid. */
int sigismember(const sigset_t *set, int num) {
    if (num < 1 || num >= NSIG) {
        errno = EINVAL;
        return -1;
    }

    return (*set & (1 << num)) ? 1 : 0;
}
