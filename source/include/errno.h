/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		Error numbers definitions.
 */

#ifndef __ERRNO_H
#define __ERRNO_H

#ifdef __cplusplus
extern "C" {
#endif

#define ESUCCESS		0	/**< Success. */

/** Standard C error number definitions. */
#define ERANGE			1	/**< Numerical result out of range. */
#define EILSEQ			2	/**< Illegal byte sequence. */
#define EDOM			3	/**< Numerical argument out of domain. */

/** POSIX error numbers. */
#define E2BIG			4	/**< Argument list too long. */
#define EACCES			5	/**< Permission denied. */
#define EADDRINUSE		6	/**< Address in use. */
#define EADDRNOTAVAIL		7	/**< Address not available. */
#define EAFNOSUPPORT		8	/**< Address family not supported. */
#define EAGAIN			9	/**< Resource unavailable, try again. */
#define EALREADY		10	/**< Connection already in progress. */
#define EBADF			11	/**< Bad file descriptor. */
#define EBADMSG			12	/**< Bad message. */
#define EBUSY			13	/**< Device or resource busy. */
#define ECANCELED		14	/**< Operation canceled. */
#define ECHILD			15	/**< No child processes. */
#define ECONNABORTED		16	/**< Connection aborted. */
#define ECONNREFUSED		17	/**< Connection refused. */
#define ECONNRESET		18	/**< Connection reset. */
#define EDEADLK			19	/**< Resource deadlock would occur. */
#define EDESTADDRREQ		20	/**< Destination address required. */
#define EDQUOT			21	/**< Reserved. */
#define EEXIST			22	/**< File exists. */
#define EFAULT			23	/**< Bad address. */
#define EFBIG			24	/**< File too large. */
#define EHOSTUNREACH		25	/**< Host is unreachable. */
#define EIDRM			26	/**< Identifier removed. */
#define EINPROGRESS		27	/**< Operation in progress. */
#define EINTR			28	/**< Interrupted function. */
#define EINVAL			29	/**< Invalid argument. */
#define EIO			30	/**< I/O error. */
#define EISCONN			31	/**< Socket is connected. */
#define EISDIR			32	/**< Is a directory. */
#define ELOOP			33	/**< Too many levels of symbolic links. */
#define EMFILE			34	/**< Too many open files. */
#define EMLINK			35	/**< Too many links. */
#define EMSGSIZE		36	/**< Message too large. */
#define EMULTIHOP		37	/**< Reserved. */
#define ENAMETOOLONG		38	/**< Filename too long. */
#define ENETDOWN		39	/**< Network is down. */
#define ENETRESET		40	/**< Connection aborted by network. */
#define ENETUNREACH		41	/**< Network unreachable. */
#define ENFILE			42	/**< Too many files open in system. */
#define ENOBUFS			43	/**< No buffer space available. */
#define ENODEV			44	/**< No such device. */
#define ENOENT			45	/**< No such file or directory. */
#define ENOEXEC			46	/**< Executable file format error. */
#define ENOLCK			47	/**< No locks available. */
#define ENOLINK			48	/**< Reserved. */
#define ENOMEM			49	/**< Out of memory. */
#define ENOMSG			50	/**< No message of the desired type. */
#define ENOPROTOOPT		51	/**< Protocol not available. */
#define ENOSPC			52	/**< No space left on device. */
#define ENOSYS			53	/**< Function not implemented. */
#define ENOTCONN		54	/**< The socket is not connected. */
#define ENOTDIR			55	/**< Not a directory. */
#define ENOTEMPTY		56	/**< Directory not empty. */
#define ENOTSOCK		57	/**< Not a socket. */
#define ENOTSUP			58	/**< Operation not supported. */
#define ENOTTY			59	/**< Inappropriate I/O control operation. */
#define ENXIO			60	/**< No such device or address. */
#define EOPNOTSUPP		61	/**< Operation not supported on socket. */
#define EOVERFLOW		62	/**< Value too large to be stored in data type. */
#define EPERM			63	/**< Operation not permitted. */
#define EPIPE			64	/**< Broken pipe. */
#define EPROTO			65	/**< Protocol error. */
#define EPROTONOSUPPORT		66	/**< Protocol not supported. */
#define EPROTOTYPE		67	/**< Protocol wrong type for socket. */
#define EROFS			68	/**< Read-only file system. */
#define ESPIPE			69	/**< Invalid seek. */
#define ESRCH			70	/**< No such process. */
#define ESTALE			71	/**< Reserved. */
#define ETIMEDOUT		72	/**< Connection timed out. */
#define ETXTBSY			73	/**< Text file busy. */
#define EXDEV			74	/**< Cross-device link. */
#define EWOULDBLOCK		EAGAIN	/**< Operation would block. */

extern int *__libc_errno_location(void);
#define errno (*__libc_errno_location())

#ifdef __cplusplus
}
#endif

#endif /* __ERRNO_H */
