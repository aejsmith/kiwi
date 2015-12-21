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
 * @brief               Device control function.
 */

#include <kernel/device.h>
#include <kernel/status.h>

#include <sys/ioctl.h>

#include <errno.h>
#include <stdarg.h>
#include <termios.h>
#include <unistd.h>

#include "libsystem.h"

/** Control a device.
 * @note                The Kiwi implementation of this function only works for
 *                      terminal devices, and will return an error if used on
 *                      anything else.
 * @param fd            File descriptor to device.
 * @param request       Request to perform.
 * @param ...           Optional argument pointer.
 * @return              Request-dependant return code, or -1 for an error. */
int ioctl(int fd, int request, ...) {
    void *in = NULL, *out = NULL;
    size_t in_size = 0, out_size = 0;
    status_t ret;
    va_list args;
    int arg;

    if (!isatty(fd)) {
        errno = ENOTTY;
        return -1;
    }

    va_start(args, request);

    /* Convert the request and arguments to what the kernel wants. */
    switch (request) {
    case TIOCDRAIN:
        break;
    case TCXONC:
    case TCFLSH:
        arg = va_arg(args, int);
        in = &arg;
        in_size = sizeof(arg);
        break;
    case TCGETA:
        out = va_arg(args, void *);
        out_size = sizeof(struct termios);
        break;
    case TCSETA:
    case TCSETAW:
    case TCSETAF:
        in = va_arg(args, void *);
        in_size = sizeof(struct termios);
        break;
    case TIOCGPGRP:
        in = va_arg(args, void *);
        in_size = sizeof(int);
        break;
    case TIOCSPGRP:
        out = va_arg(args, void *);
        out_size = sizeof(int);
        break;
    case TIOCGWINSZ:
        out = va_arg(args, void *);
        out_size = sizeof(struct winsize);
        break;
    case TIOCSWINSZ:
        in = va_arg(args, void *);
        in_size = sizeof(struct winsize);
        break;
    default:
        errno = EINVAL;
        break;
    }

    va_end(args);

    /* Perform the request. */
    ret = kern_device_request(fd, request, in, in_size, out, out_size, NULL);
    if (ret != STATUS_SUCCESS) {
        if (ret == STATUS_INVALID_REQUEST) {
            errno = ENOTTY;
        } else {
            libsystem_status_to_errno(ret);
        }

        return -1;
    }

    return 0;
}
