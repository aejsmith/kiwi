/*
 * Copyright (C) 2009 Alex Smith
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
 * @brief		IPC system.
 */

#ifndef __IPC_IPC_H
#define __IPC_IPC_H

#include <kernel/ipc.h>

extern int kdbg_cmd_endpoint(int argc, char **argv);
extern int kdbg_cmd_port(int argc, char **argv);

#endif /* __IPC_IPC_H */
