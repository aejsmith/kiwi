/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
     * Implements the alarm() function.
     *
     * Request:
     *  - Data = posix_request_alarm_t
     *
     * Reply:
     *  - Data = posix_reply_alarm_t
     */
    POSIX_REQUEST_ALARM = 5,

    /**
     * Implements the getpgid() function.
     *
     * Request:
     *  - Data = posix_request_getpgid_t
     *
     * Reply:
     *  - Data = posix_reply_getpgid_t
     */
    POSIX_REQUEST_GETPGID = 6,

    /**
     * Implements the setpgid() function.
     *
     * Request:
     *  - Data  = posix_request_setpgid_t
     *
     * Reply:
     *  - Data = posix_reply_setpgid_t
     */
    POSIX_REQUEST_SETPGID = 7,

    /**
     * Implements the getsid() function.
     *
     * Request:
     *  - Data = posix_request_getsid_t
     *
     * Reply:
     *  - Data = posix_reply_getsid_t
     */
    POSIX_REQUEST_GETSID = 8,

    /**
     * Implements the setsid() function.
     *
     * Reply:
     *  - Data = posix_reply_setsid_t
     */
    POSIX_REQUEST_SETSID = 9,

    /**
     * Implements the posix_get_pgrp_session() function.
     *
     * Request:
     *  - Data = posix_request_get_pgrp_session_t
     *
     * Reply:
     *  - Data = posix_reply_get_pgrp_session_t
     */
    POSIX_REQUEST_GET_PGRP_SESSION = 10,

    /**
     * Implements the posix_set_session_terminal() function.
     *
     * Request:
     *  - Data   = posix_request_set_session_terminal_t
     *  - Handle = Read+write handle to terminal
     *
     * Reply:
     *  - Data   = posix_reply_set_session_terminal_t
     */
    POSIX_REQUEST_SET_SESSION_TERMINAL = 11,

    /**
     * Gets a handle to the controlling terminal for the calling process.
     *
     * Request:
     *  - Data   = posix_request_get_terminal_t
     *
     * Reply:
     *  - Data   = posix_reply_get_terminal_t
     *  - Handle = Controlling terminal handle (on success)
     */
    POSIX_REQUEST_GET_TERMINAL = 12,
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

typedef struct posix_request_alarm {
    uint32_t seconds;               /**< Number of seconds to signal in. */
} posix_request_alarm_t;

typedef struct posix_reply_alarm {
    int32_t err;                    /**< Error number (0 on success). */
    uint32_t remaining;             /**< Previous remaining time. */
} posix_reply_alarm_t;

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

typedef struct posix_request_get_pgrp_session {
    int32_t pgid;                   /**< PGID to get for. */
} posix_request_get_pgrp_session_t;

typedef struct posix_reply_get_pgrp_session {
    int32_t err;                    /**< Error number (0 on success). */
    int32_t sid;                    /**< SID. */
} posix_reply_get_pgrp_session_t;

typedef struct posix_request_set_session_terminal {
    int32_t sid;                    /**< SID to set for. */
} posix_request_set_session_terminal_t;

typedef struct posix_reply_set_session_terminal {
    int32_t err;                    /**< Error number (0 on success). */
} posix_reply_set_session_terminal_t;

typedef struct posix_request_get_terminal {
    uint32_t access;                /**< Access flags (kernel). */
    uint32_t flags;                 /**< Handle flags (kernel). */
} posix_request_get_terminal_t;

typedef struct posix_reply_get_terminal {
    int32_t err;                    /**< Error number (0 on success). */
} posix_reply_get_terminal_t;
