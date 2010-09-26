/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Input device manager.
 *
 * This class watches the input device directory for new devices and creates
 * devices for them.
 *
 * @todo		The kernel doesn't actually have any facilities to
 *			watch a device directory yet. It also doesn't let us
 *			get attributes.
 */

#include <kernel/device.h>
#include <kernel/object.h>
#include <kernel/status.h>

#include <iostream>

#include "InputManager.h"
#include "MouseDevice.h"

using namespace std;

/** Initialise the input manager.
 * @param server	Server that the manager is for. */
InputManager::InputManager(WindowServer *server) :
	m_server(server)
{
	handle_t handle;
	status_t ret;

	/* See above TODO. Just hardcode devices for now. */
	ret = device_open("/input/1", &handle);
	if(ret == STATUS_SUCCESS) {
		new MouseDevice(server, handle);
	} else {
		clog << "Failed to open /input/1: " << ret << endl;
	}
}
