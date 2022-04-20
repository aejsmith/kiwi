/*
 * Copyright (C) 2009-2022 Alex Smith
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
 * @brief               Terminal control functions.
 */

#include <kernel/file.h>
#include <kernel/status.h>

#include <sys/ioctl.h>

#include <errno.h>
#include <stdarg.h>
#include <termios.h>
#include <unistd.h>

#include "libsystem.h"

/** Check whether a file descriptor refers to a TTY.
 * @param fd            File descriptor to check.
 * @return              1 if a TTY, 0 if not. */
int isatty(int fd) {
    file_info_t info;
    unsigned type;
    status_t ret;

    ret = kern_object_type(fd, &type);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return 0;
    }

    bool is_tty = type == OBJECT_TYPE_FILE;

    if (is_tty) {
        ret = kern_file_info(fd, &info);
        if (ret != STATUS_SUCCESS) {
            libsystem_status_to_errno(ret);
            return 0;
        }

        is_tty = info.type == FILE_TYPE_CHAR;

        if (is_tty) {
            struct winsize ws;
            size_t bytes;
            ret = kern_file_request(fd, TIOCGWINSZ, NULL, 0, &ws, sizeof(ws), &bytes);

            is_tty = ret == STATUS_SUCCESS && bytes == sizeof(ws);
        }
    }

    if (!is_tty)
        errno = ENOTTY;

    return is_tty;
}

/**
 * Control a device. The Kiwi implementation of this function only works for
 * terminals, and will return an error if used on anything else.
 *
 * @param fd            File descriptor to device.
 * @param request       Request to perform.
 * @param ...           Optional argument pointer.
 *
 * @return              Request-dependant return code, or -1 for an error.
 */
int ioctl(int fd, int request, ...) {
    status_t ret;

    if (!isatty(fd)) {
        errno = ENOTTY;
        return -1;
    }

    va_list args;
    va_start(args, request);

    void *in = NULL, *out = NULL;
    size_t in_size = 0, out_size = 0;
    int arg;

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
            out = va_arg(args, void *);
            out_size = sizeof(pid_t);
            break;
        case TIOCSPGRP:
            in = va_arg(args, void *);
            in_size = sizeof(pid_t);
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

    ret = kern_file_request(fd, request, in, in_size, out, out_size, NULL);
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

/** Get the path to the terminal device.
 * @param fd            File descriptor to terminal device to get path to.
 * @return              Pointer to path string, or NULL on failure. */
char *ttyname(int fd) {
    /* Our terminals don't exist in the filesystem. */
    errno = ENOSYS;
    return NULL;
}

/** Get the input baud rate from a termios structure.
 * @param tio           Terminal I/O settings structure.
 * @return              Input baud rate. */
speed_t cfgetispeed(const struct termios *tio) {
    return tio->c_ispeed;
}

/** Get the output baud rate from a termios structure.
 * @param tio           Terminal I/O settings structure.
 * @return              Output baud rate. */
speed_t cfgetospeed(const struct termios *tio) {
    return tio->c_ospeed;
}

/**
 * Sets the input baud rate in a termios structure. This will have no effect on
 * the settings of a terminal until the attributes are set using tcsetattr().
 *
 * @param tio           Terminal I/O settings structure.
 * @param speed         Input baud rate to set.
 *
 * @return              0 on success, -1 on failure.
 */
int cfsetispeed(struct termios *tio, speed_t speed) {
    tio->c_ispeed = speed;
    return 0;
}

/**
 * Sets the output baud rate in a termios structure. This will have no effect
 * on the settings of a terminal until the attributes are set using tcsetattr().
 *
 * @param tio           Terminal I/O settings structure.
 * @param speed         Output baud rate to set.
 *
 * @return              0 on success, -1 on failure.
 */
int cfsetospeed(struct termios *tio, speed_t speed) {
    tio->c_ospeed = speed;
    return 0;
}

/** Wait until all data written to a terminal is transmitted.
 * @param fd            File descriptor for terminal.
 * @return              0 on success, -1 on failure. */
int tcdrain(int fd) {
    return ioctl(fd, TIOCDRAIN);
}

/** Suspend or restart data transmission on a terminal.
 * @param fd            File descriptor for terminal.
 * @param action        Action to perform. TCOOFF will suspend output, TCOON
 *                      will resume output. TCIOFF will suspend input, TCION
 *                      will resume input.
 * @return              0 on success, -1 on failure. */
int tcflow(int fd, int action) {
    return ioctl(fd, TCXONC, action);
}

/** Discard the contents of terminal buffers.
 * @param fd            File descriptor for terminal.
 * @param action        Action to perform. TCIFLUSH will flush data received
 *                      but not read. TCOFLUSH will flush data written but not
 *                      transmitted. TCIOFLUSH will flush both.
 * @return              0 on success, -1 on failure. */
int tcflush(int fd, int action) {
    return ioctl(fd, TCFLSH, action);
}

/** Get I/O settings for a terminal.
 * @param fd            File descriptor for terminal.
 * @param tiop          Structure to fill with settings.
 * @return              0 on success, -1 on failure. */
int tcgetattr(int fd, struct termios *tiop) {
    return ioctl(fd, TCGETA, tiop);
}

/** Get process group ID of the terminal's session.
 * @param fd            File descriptor for terminal.
 * @return              Process group ID on success, -1 on failure. */
pid_t tcgetsid(int fd) {
    errno = ENOSYS;
    return -1;
}

/** Send a break for specified duration.
 * @param fd            File descriptor for terminal.
 * @param duration      Duration of break.
 * @return              0 on success, -1 on failure. */
int tcsendbreak(int fd, int duration) {
    errno = ENOSYS;
    return -1;
}

/** Set I/O settings for a terminal.
 * @param fd            File descriptor for terminal.
 * @param action        How to perform the change. If TCSANOW, the change will
 *                      occur immediately. If TCSADRAIN, the change will occur
 *                      after all written data is transmitted. If TCSAFLUSH,
 *                      the change will occur after all written data is
 *                      transmitted, and waiting input will be discarded.
 * @param tiop          Structure containing new terminal settings.
 * @return              0 on success, -1 on failure. */
int tcsetattr(int fd, int action, const struct termios *tio) {
    int request;

    switch (action) {
        case TCSANOW:
            request = TCSETA;
            break;
        case TCSADRAIN:
            request = TCSETAW;
            break;
        case TCSAFLUSH:
            request = TCSETAF;
            break;
        default:
            errno = EINVAL;
            return -1;
    }

    return ioctl(fd, request, tio);
}

/** Get the foreground process group of a terminal
 * @param fd            File descriptor for terminal.
 * @return              Process group ID on success, -1 on failure. */
pid_t tcgetpgrp(int fd) {
    pid_t pgid;

    if (ioctl(fd, TIOCGPGRP, &pgid) < 0)
        return -1;

    return pgid;
}

/** Set the foreground process group of a terminal
 * @param fd            File descriptor for terminal.
 * @param pgid          Process group ID to set.
 * @return              0 on success, -1 on failure. */
int tcsetpgrp(int fd, pid_t pgid) {
    return ioctl(fd, TIOCSPGRP, &pgid);
}
