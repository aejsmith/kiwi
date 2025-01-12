/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               IPC service functions.
 */

#pragma once

#include <core/ipc.h>

__SYS_EXTERN_C_BEGIN

extern status_t core_service_open(
    const char *name, uint32_t service_flags, uint32_t conn_flags,
    core_connection_t **_conn);
extern status_t core_service_register_port(handle_t port);
extern status_t core_service_get_process(const char *name, handle_t *_handle);

__SYS_EXTERN_C_END
