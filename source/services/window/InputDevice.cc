/*
 * Copyright (C) 2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
	ret = kern_device_read(m_handle, &event, sizeof(event), 0, NULL);
	if(ret != STATUS_SUCCESS) {
		clog << "Failed to read input event: " << ret << endl;
		return;
	}

	/* Pass through to the device handler. */
	HandleEvent(event);
}
