/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Synchronous I/O multiplexing.
 */

#pragma once

#define __NEED_struct_timespec
#define __NEED_struct_timeval
#define __NEED_suseconds_t
#define __NEED_time_t
#include <bits/alltypes.h>

#include <sys/types.h>

#include <signal.h>
#include <time.h>

__SYS_EXTERN_C_BEGIN

#define FD_SETSIZE              1024
#define __FD_SET_BITS_PER_WORD  (8 * sizeof(unsigned long))
#define __FD_SET_WORDS          (FD_SETSIZE / __FD_SET_BITS_PER_WORD)

typedef struct fd_set {
    unsigned long words[__FD_SET_WORDS];
} fd_set;

#define FD_ZERO(set) \
    do { \
        for (unsigned __i = 0; __i < __FD_SET_WORDS; __i++) \
            (set)->words[__i] = 0; \
    } while (0)

#define FD_SET(fd, set) \
    ((set)->words[(fd) / __FD_SET_BITS_PER_WORD] |= (1ul << ((fd) % __FD_SET_BITS_PER_WORD)))

#define FD_CLR(fd, set) \
    ((set)->words[(fd) / __FD_SET_BITS_PER_WORD] &= ~(1ul << ((fd) % __FD_SET_BITS_PER_WORD)))

#define FD_ISSET(fd, set) \
    (!!((set)->words[(fd) / __FD_SET_BITS_PER_WORD] & (1ul << ((fd) % __FD_SET_BITS_PER_WORD))))

extern int pselect(
    int nfds, fd_set *__restrict readfds, fd_set *__restrict writefds,
    fd_set *__restrict errorfds, const struct timespec *__restrict timeout,
    const sigset_t *__restrict sigmask);
extern int select(
    int nfds, fd_set *__restrict readfds, fd_set *__restrict writefds,
    fd_set *__restrict errorfds, struct timeval *__restrict timeout);

__SYS_EXTERN_C_END
