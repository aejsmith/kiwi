/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Core socket API.
 */

#pragma once

#include <kernel/socket.h>

#include <sys/types.h>

__SYS_EXTERN_C_BEGIN

/** Ancient name compatibility. */
#define PF_UNSPEC       AF_UNSPEC
#define PF_INET         AF_INET
#define PF_INET6        AF_INET6
#define PF_UNIX         AF_UNIX

/** Linux-compatible flags for socket()/socketpair() type argument. */
#define SOCK_CLOEXEC    (1<<16)
#define SOCK_NONBLOCK   (1<<17)
#define __SOCK_TYPE     0xffff

extern int accept(int socket, struct sockaddr *__restrict addr, socklen_t *__restrict addr_len);
extern int bind(int socket, const struct sockaddr *addr, socklen_t addr_len);
extern int connect(int socket, const struct sockaddr *addr, socklen_t addr_len);
extern int getpeername(int socket, struct sockaddr *__restrict addr, socklen_t *__restrict addr_len);
extern int getsockname(int socket, struct sockaddr *__restrict addr, socklen_t *__restrict addr_len);
extern int getsockopt(
    int socket, int level, int opt_name, void *__restrict opt_value,
    socklen_t *__restrict opt_len);
extern int listen(int socket, int backlog);
extern ssize_t recv(int socket, void *buf, size_t length, int flags);
extern ssize_t recvfrom(
    int socket, void *__restrict buf, size_t length, int flags,
    struct sockaddr *__restrict addr, socklen_t *__restrict addr_len);
/* ssize_t recvmsg(int, struct msghdr *, int); */
extern ssize_t send(int socket, const void *buf, size_t length, int flags);
/* ssize_t sendmsg(int, const struct msghdr *, int); */
extern ssize_t sendto(
    int socket, const void *buf, size_t length, int flags,
    const struct sockaddr *addr, socklen_t addr_len);
extern int setsockopt(
    int socket, int level, int opt_name, const void *opt_value,
    socklen_t opt_len);
extern int shutdown(int socket, int how);
extern int sockatmark(int socket);
extern int socket(int domain, int type, int protocol);
extern int socketpair(int domain, int type, int protocol, int sockets[2]);

__SYS_EXTERN_C_END
