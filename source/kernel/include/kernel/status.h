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
 * @brief               Kernel status code definitions.
 */

#pragma once

/** Definitions of status codes returned by kernel functions. */
#define STATUS_SUCCESS              0   /**< Operation completed successfully. */
#define STATUS_NOT_IMPLEMENTED      1   /**< Operation not implemented. */
#define STATUS_NOT_SUPPORTED        2   /**< Operation not supported. */
#define STATUS_WOULD_BLOCK          3   /**< Operation would block. */
#define STATUS_INTERRUPTED          4   /**< Interrupted while blocking. */
#define STATUS_TIMED_OUT            5   /**< Timed out while waiting. */
#define STATUS_INVALID_SYSCALL      6   /**< Invalid system call number. */
#define STATUS_INVALID_ARG          7   /**< Invalid argument specified. */
#define STATUS_INVALID_HANDLE       8   /**< Non-existant handle or handle with incorrect type. */
#define STATUS_INVALID_ADDR         9   /**< Invalid memory location specified. */
#define STATUS_INVALID_REQUEST      10  /**< Invalid file request specified. */
#define STATUS_INVALID_EVENT        11  /**< Invalid/unsupported object event specified. */
#define STATUS_OVERFLOW             12  /**< Integer overflow. */
#define STATUS_NO_MEMORY            13  /**< Out of memory. */
#define STATUS_NO_HANDLES           14  /**< No handles are available. */
#define STATUS_PROCESS_LIMIT        15  /**< Process limit reached. */
#define STATUS_THREAD_LIMIT         16  /**< Thread limit reached. */
#define STATUS_READ_ONLY            17  /**< Object cannot be modified. */
#define STATUS_PERM_DENIED          18  /**< Operation not permitted. */
#define STATUS_ACCESS_DENIED        19  /**< Requested access rights denied. */
#define STATUS_NOT_DIR              20  /**< Path component is not a directory. */
#define STATUS_NOT_REGULAR          21  /**< Path does not refer to a regular file. */
#define STATUS_NOT_SYMLINK          22  /**< Path does not refer to a symbolic link. */
#define STATUS_NOT_MOUNT            23  /**< Path does not refer to root of a mount. */
#define STATUS_NOT_FOUND            24  /**< Requested object could not be found. */
#define STATUS_NOT_EMPTY            25  /**< Directory is not empty. */
#define STATUS_ALREADY_EXISTS       26  /**< Object already exists. */
#define STATUS_TOO_SMALL            27  /**< Provided buffer is too small. */
#define STATUS_TOO_LARGE            28  /**< Provided buffer is too large. */
#define STATUS_TOO_LONG             29  /**< Provided string is too long. */
#define STATUS_DIR_FULL             30  /**< Directory is full. */
#define STATUS_UNKNOWN_FS           31  /**< Filesystem has an unrecognised format. */
#define STATUS_CORRUPT_FS           32  /**< Corruption detected on the filesystem. */
#define STATUS_FS_FULL              33  /**< No space is available on the filesystem. */
#define STATUS_SYMLINK_LIMIT        34  /**< Exceeded nested symbolic link limit. */
#define STATUS_IN_USE               35  /**< Object is in use. */
#define STATUS_DEVICE_ERROR         36  /**< An error occurred during a hardware operation. */
#define STATUS_STILL_RUNNING        37  /**< Process/thread is still running. */
#define STATUS_NOT_RUNNING          38  /**< Process/thread is not running. */
#define STATUS_UNKNOWN_IMAGE        39  /**< Executable image has an unrecognised format. */
#define STATUS_MALFORMED_IMAGE      40  /**< Executable image format is incorrect. */
#define STATUS_MISSING_LIBRARY      41  /**< Required library not found. */
#define STATUS_MISSING_SYMBOL       42  /**< Referenced symbol not found. */
#define STATUS_TRY_AGAIN            43  /**< Attempt the operation again. */
#define STATUS_DIFFERENT_FS         44  /**< Link source and destination on different FS. */
#define STATUS_IS_DIR               45  /**< Path refers to a directory. */
#define STATUS_CONN_HUNGUP          46  /**< Connection was hung up. */
#define STATUS_CANCELLED            47  /**< Operation was cancelled. */
#define STATUS_INCORRECT_TYPE       48  /**< Object is not of the correct type. */
#define STATUS_PIPE_CLOSED          49  /**< Read end of pipe has been closed. */
#define STATUS_NET_DOWN             50  /**< Network is down. */
#define STATUS_ADDR_NOT_SUPPORTED   51  /**< Address family is not supported. */
#define STATUS_PROTO_NOT_SUPPORTED  52  /**< Protocol is not supported. */
#define STATUS_MSG_TOO_LONG         53  /**< Socket message is too long. */
#define STATUS_DEST_ADDR_REQUIRED   54  /**< Destination address is required. */
#define STATUS_NET_UNREACHABLE      55  /**< Network is unreachable. */
#define STATUS_HOST_UNREACHABLE     56  /**< No route to host. */
#define STATUS_IN_PROGRESS          57  /**< Operation is in progress. */
#define STATUS_ALREADY_IN_PROGRESS  58  /**< Operation is already in progress. */
#define STATUS_ALREADY_CONNECTED    59  /**< Socket is already connected. */
#define STATUS_CONNECTION_REFUSED   60  /**< Destination refused the connection. */
#define STATUS_NOT_CONNECTED        61  /**< Socket is not connected. */

#if !defined(__KERNEL) && !defined(__ASM__)

#include <kernel/types.h>

__KERNEL_EXTERN_C_BEGIN

extern const char *kern_status_string(status_t status);

__KERNEL_EXTERN_C_END

#endif /* !__KERNEL && !__ASM__ */
