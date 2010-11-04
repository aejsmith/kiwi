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
 * @brief		Base input device class.
 */

#include <drivers/input.h>

#include <kernel/device.h>
#include <kernel/status.h>

#include <cassert>
#include <iostream>

#include "InputDevice.h"

using namespace std;

/** Input device constructor.
 * @param manager	Manager of the device.
 * @param handle	Handle to the device. */
InputDevice::InputDevice(InputManager *manager, handle_t handle) :
	m_manager(manager)
{
	SetHandle(handle);
}

/** Register events for the device. */
void InputDevice::RegisterEvents() {
	RegisterEvent(DEVICE_EVENT_READABLE);
}

/** Handle an event.
 * @param id		ID of the event. */
void InputDevice::HandleEvent(int id) {
	input_event_t event;
	status_t ret;

	assert(id == DEVICE_EVENT_READABLE);

	/* Read the event structure. */
	ret = device_read(m_handle, &event, sizeof(event), 0, NULL);
	if(ret != STATUS_SUCCESS) {
		clog << "Failed to read input event: " << ret << endl;
		return;
	}

	/* Pass through to the device handler. */
	HandleEvent(event);
}
