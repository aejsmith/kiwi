/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Inter-Process Communication (IPC) interface.
 */

#pragma once

#include <kernel/object.h>
#include <kernel/security.h>

__KERNEL_EXTERN_C_BEGIN

/** Maximum length of data that can be attached to a message. */
#define IPC_DATA_MAX            16384

/** Maximum number of messages that can be queued at a time. */
#define IPC_QUEUE_MAX           256

/** Number of inline message arguments. */
#define IPC_MESSAGE_ARGS_COUNT  6

/** Structure describing an IPC message. */
typedef struct ipc_message {
    uint32_t id;                            /**< Message identifier. */
    uint16_t flags;                         /**< Message flags. */
    uint16_t size;                          /**< Size of attached data. */
    nstime_t timestamp;                     /**< System time at which message was sent. */
    uint64_t args[IPC_MESSAGE_ARGS_COUNT];  /**< Inline message arguments. */
} ipc_message_t;

/** IPC message flags. */
enum {
    IPC_MESSAGE_HANDLE          = (1<<0),   /**< Message has an attached handle. */
    IPC_MESSAGE_SECURITY        = (1<<1),   /**< Message has an attached security context. */
};

/** IPC port event IDs. */
enum {
    PORT_EVENT_CONNECTION       = 1,        /**< A connection is being made to the port. */
};

/** IPC connection event IDs. */
enum {
    CONNECTION_EVENT_HANGUP     = 1,        /**< Remote end hung up or port was deleted. */
    CONNECTION_EVENT_MESSAGE    = 2,        /**< A message is received. */
};

/** Special process port IDs (negative values to distinguish from handles). */
#define PROCESS_ROOT_PORT           (-2)

extern status_t kern_port_create(handle_t *_handle);
extern status_t kern_port_listen(handle_t handle, nstime_t timeout, handle_t *_handle);

extern status_t kern_connection_open(handle_t port, nstime_t timeout, handle_t *_handle);

extern status_t kern_connection_open_remote(handle_t handle, handle_t *_process);

extern status_t kern_connection_status(handle_t handle);
extern status_t kern_connection_send(
    handle_t handle, const ipc_message_t *msg, const void *data,
    handle_t attached, nstime_t timeout);
extern status_t kern_connection_receive(
    handle_t handle, ipc_message_t *msg, security_context_t *security,
    nstime_t timeout);
extern status_t kern_connection_receive_data(handle_t handle, void *data);
extern status_t kern_connection_receive_handle(handle_t handle, handle_t *_attached);

__KERNEL_EXTERN_C_END
