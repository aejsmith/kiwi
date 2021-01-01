/*
 * Copyright (C) 2009-2021 Alex Smith
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
 * @brief               User file API.
 *
 * The user file API allows creation of file object handles where operations on
 * them are implemented by a user mode process (the one which created the file).
 *
 * Every operation on a user file results in a message being sent to the file's
 * creator via an IPC connection. The operation will wait until a reply to that
 * operation is sent back over the connection.
 *
 * Each operation is sent with a serial number. The reply must include the same
 * serial number in order to match it with the right operation. There is no
 * need to reply to operations in the same order that they are received, as the
 * serial number takes care of this.
 *
 * By the time that an operation is completed, the thread which initiated the
 * operation may have cancelled it (e.g. due to being interrupted). To handle
 * this, when sending the reply message for an operation, if the serial number
 * does not match a currently outstanding operation, the call to
 * kern_connection_send() will return STATUS_CANCELLED. Depending on the
 * implementation of the user file, this may need to be handled to ensure that
 * data is not lost. For example, for a read operation, in response to a
 * cancellation the data that was to be returned might need to be added back to
 * an input buffer, so that it can be returned to a subsequent operation rather
 * than lost.
 */

#pragma once

#include <kernel/file.h>

__KERNEL_EXTERN_C_BEGIN

/** User file operation message IDs. */
enum {
    /**
     * Read the file (kern_file_read()).
     *
     * Input:
     *   Arguments:
     *     USER_FILE_MESSAGE_ARG_SERIAL      = Operation serial.
     *     USER_FILE_MESSAGE_ARG_FLAGS       = Current handle flags.
     *     USER_FILE_MESSAGE_ARG_READ_OFFSET = Offset in the file to read from.
     *     USER_FILE_MESSAGE_ARG_READ_SIZE   = Size of data to read.
     *
     * Reply:
     *   Arguments:
     *     USER_FILE_MESSAGE_ARG_SERIAL      = Operation serial (as input).
     *     USER_FILE_MESSAGE_ARG_READ_STATUS = Status code.
     *   Data:
     *     Data read from the file.
     *
     * Data size for the reply is used as the actual size read, can be less or
     * equal (but not more) than the operation requested, as per
     * kern_file_read().
     */
    USER_FILE_OP_READ = 0,

    /**
     * Write the file (kern_file_write()).
     *
     * Input:
     *   Arguments:
     *     USER_FILE_MESSAGE_ARG_SERIAL       = Operation serial.
     *     USER_FILE_MESSAGE_ARG_FLAGS        = Current handle flags.
     *     USER_FILE_MESSAGE_ARG_WRITE_OFFSET = Offset in the file to write to.
     *   Data:
     *     Data to write to the file (size specified in message).
     *
     * Reply:
     *   Arguments:
     *     USER_FILE_MESSAGE_ARG_SERIAL       = Operation serial (as input).
     *     USER_FILE_MESSAGE_ARG_WRITE_STATUS = Status code.
     *     USER_FILE_MESSAGE_ARG_WRITE_SIZE   = Actual size written.
     *
     * Size given in the reply is used as the actual size written, can be less
     * or equal (but not more) than the operation requested, as per
     * kern_file_write().
     */
    USER_FILE_OP_WRITE = 1,

    /**
     * Get file info (kern_file_info()).
     *
     * Input:
     *   Arguments:
     *     USER_FILE_MESSAGE_ARG_SERIAL = Operation serial.
     *
     * Reply:
     *   Arguments:
     *     USER_FILE_MESSAGE_ARG_SERIAL = Operation serial (as input).
     *   Data:
     *     file_info_t for the file.
     *
     * Certain fields of the returned information are ignored and filled in by
     * the kernel: mount, type (always overridden to the type the file was
     * created with).
     */
    USER_FILE_OP_INFO = 2,

    /**
     * Perform a file-specific operation (kern_file_request()).
     *
     * Input:
     *   Arguments:
     *     USER_FILE_MESSAGE_ARG_SERIAL         = Operation serial.
     *     USER_FILE_MESSAGE_ARG_FLAGS          = Current handle flags.
     *     USER_FILE_MESSAGE_ARG_REQUEST_NUM    = Request number.
     *   Data:
     *     Input data passed to the request (size specified in message).
     *
     * Reply:
     *   Arguments:
     *     USER_FILE_MESSAGE_ARG_SERIAL         = Operation serial.
     *     USER_FILE_MESSAGE_ARG_REQUEST_STATUS = Status code.
     *   Data:
     *     Output data.
     */
    USER_FILE_OP_REQUEST = 3,
};

/** User file message fields. */
enum {
    USER_FILE_MESSAGE_ARG_SERIAL            = 0,
    USER_FILE_MESSAGE_ARG_FLAGS             = 1,

    USER_FILE_MESSAGE_ARG_READ_OFFSET       = 2,
    USER_FILE_MESSAGE_ARG_READ_SIZE         = 3,

    USER_FILE_MESSAGE_ARG_READ_STATUS       = 1,

    USER_FILE_MESSAGE_ARG_WRITE_OFFSET      = 2,

    USER_FILE_MESSAGE_ARG_WRITE_STATUS      = 1,
    USER_FILE_MESSAGE_ARG_WRITE_SIZE        = 2,

    USER_FILE_MESSAGE_ARG_REQUEST_NUM       = 2,

    USER_FILE_MESSAGE_ARG_REQUEST_STATUS    = 1,
};

extern status_t kern_user_file_create(
    file_type_t type, uint32_t access, uint32_t flags, handle_t *_conn,
    handle_t *_file);

__KERNEL_EXTERN_C_END
