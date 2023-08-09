/*
 * Copyright (C) 2009-2023 Alex Smith
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
 * @brief               Input/output multiplexing.
 */

#pragma once

#define __NEED_size_t
#include <bits/alltypes.h>

__SYS_EXTERN_C_BEGIN

/** Poll file descriptor information. */
struct pollfd {
    int fd;                         /**< File descriptor being polled. */
    short events;                   /**< Events to wait for. */
    short revents;                  /**< Events returned from the poll() call. */
};

/** Type used to store a number of file descriptors. */
typedef size_t nfds_t;

/** Events for poll(). */
#define POLLIN          (1<<0)      /**< Normal data may be read without blocking. */
#define POLLPRI         (1<<1)      /**< High-priority data may be read without blocking. */
#define POLLRDBAND      (1<<2)      /**< Priority data may be read without blocking. */
#define POLLOUT         (1<<3)      /**< Normal data may be written without blocking. */
#define POLLWRBAND      (1<<4)      /**< Priority data may be written without blocking. */
#define POLLERR         (1<<5)      /**< An error has occurred (output only). */
#define POLLHUP         (1<<6)      /**< Device has been disconnected (output only). */
#define POLLNVAL        (1<<7)      /**< Invalid file descriptor (output only). */
#define POLLRDNORM      POLLIN      /**< Equivalent to POLLIN. */
#define POLLWRNORM      POLLOUT     /**< Equivalent to POLLOUT. */

extern int poll(struct pollfd *fds, nfds_t nfds, int timeout);

__SYS_EXTERN_C_END
