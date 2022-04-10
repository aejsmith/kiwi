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
 * @brief               POSIX service IPC protocol.
 */

#pragma once

#include <signal.h>

#define POSIX_SERVICE_NAME "org.kiwi.posix"

/** POSIX service message IDs. */
enum {
    /**
     * Retrieve a handle to the condition object that will be set when a signal
     * is pending for the process. This can be waited on with
     * kern_object_callback() to implement asynchronous signal handling.
     *
     * Reply:
     *  - Data   = posix_reply_get_signal_condition_t
     *  - Handle = Condition object handle
     */
    POSIX_REQUEST_GET_SIGNAL_CONDITION = 0,

    /**
     * Gets the next pending signal and clears it from the pending set. If no
     * more signals are pending after this returns, the signal condition will
     * be unset.
     *
     * Reply:
     *  - Data = posix_reply_get_pending_signal_t
     */
    POSIX_REQUEST_GET_PENDING_SIGNAL = 1,

    /**
     * Sets the action for a signal.
     *
     * Request:
     *  - Data = posix_request_set_signal_action_t
     *
     * Reply:
     *  - Data = posix_reply_set_signal_action_t
     */
    POSIX_REQUEST_SET_SIGNAL_ACTION = 2,

    /**
     * Sets the current signal mask.
     *
     * Request:
     *  - Data = posix_request_set_signal_mask_t
     *
     * Reply:
     *  - Data = posix_reply_set_signal_mask_t
     */
    POSIX_REQUEST_SET_SIGNAL_MASK = 3,

    /**
     * Implements the kill() function.
     *
     * Request:
     *  - Data  = posix_request_kill_t
     *  - Flags = CORE_MESSAGE_SEND_SECURITY
     *
     * Reply:
     *  - Data = posix_reply_kill_t
     */
    POSIX_REQUEST_KILL = 4,

    /**
     * Implements the getpgid() function.
     *
     * Request:
     *  - Data = posix_request_getpgid_t
     *
     * Reply:
     *  - Data = posix_reply_getpgid_t
     */
    POSIX_REQUEST_GETPGID = 5,

    /**
     * Implements the setpgid() function.
     *
     * Request:
     *  - Data  = posix_request_setpgid_t
     *  - Flags = CORE_MESSAGE_SEND_SECURITY
     *
     * Reply:
     *  - Data = posix_reply_setpgid_t
     */
    POSIX_REQUEST_SETPGID = 6,

    /**
     * Implements the getsid() function.
     *
     * Request:
     *  - Data = posix_request_getsid_t
     *
     * Reply:
     *  - Data = posix_reply_getsid_t
     */
    POSIX_REQUEST_GETSID = 7,

    /**
     * Implements the setsid() function.
     *
     * Request:
     *  - Flags = CORE_MESSAGE_SEND_SECURITY
     *
     * Reply:
     *  - Data = posix_reply_setsid_t
     */
    POSIX_REQUEST_SETSID = 8,
};

typedef struct posix_reply_get_signal_condition {
    int32_t err;                    /**< Error number (0 on success). */
} posix_reply_get_signal_condition_t;

typedef struct posix_reply_get_pending_signal {
    /**
     * Information for the pending signal. If no more signals are pending,
     * si_signo will be 0.
     */
    siginfo_t info;
} posix_reply_get_pending_signal_t;

/** Signal dispositions. */
enum {
    POSIX_SIGNAL_DISPOSITION_DEFAULT    = 0,
    POSIX_SIGNAL_DISPOSITION_IGNORE     = 1,
    POSIX_SIGNAL_DISPOSITION_HANDLER    = 2,
};

typedef struct posix_request_set_signal_action {
    int32_t num;                    /**< Signal number. */
    uint32_t disposition;           /**< New signal disposition. */
    uint32_t flags;                 /**< Signal action flags (SA_*). */
} posix_request_set_signal_action_t;

typedef struct posix_reply_set_signal_action {
    int32_t err;                    /**< Error number (0 on success). */
} posix_reply_set_signal_action_t;

typedef struct posix_request_set_signal_mask {
    uint32_t mask;                  /**< New signal mask. */
} posix_request_set_signal_mask_t;

typedef struct posix_reply_set_signal_mask {
    int32_t err;                    /**< Error number (0 on success). */
} posix_reply_set_signal_mask_t;

typedef struct posix_request_kill {
    int32_t pid;                    /**< PID to signal. */
    int32_t num;                    /**< Signal number. */
} posix_request_kill_t;

typedef struct posix_reply_kill {
    int32_t err;                    /**< Error number (0 on success). */
} posix_reply_kill_t;

typedef struct posix_request_getpgid {
    int32_t pid;                    /**< PID to get for. */
} posix_request_getpgid_t;

typedef struct posix_reply_getpgid {
    int32_t err;                    /**< Error number (0 on success). */
    int32_t pgid;                   /**< PGID. */
} posix_reply_getpgid_t;

typedef struct posix_request_setpgid {
    int32_t pid;                    /**< PID to set for. */
    int32_t pgid;                   /**< New PGID. */
} posix_request_setpgid_t;

typedef struct posix_reply_setpgid {
    int32_t err;                    /**< Error number (0 on success). */
} posix_reply_setpgid_t;

typedef struct posix_request_getsid {
    int32_t pid;                    /**< PID to get for. */
} posix_request_getsid_t;

typedef struct posix_reply_getsid {
    int32_t err;                    /**< Error number (0 on success). */
    int32_t sid;                    /**< SID. */
} posix_reply_getsid_t;

typedef struct posix_reply_setsid {
    int32_t err;                    /**< Error number (0 on success). */
    int32_t sid;                    /**< SID. */
} posix_reply_setsid_t;
