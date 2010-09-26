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
 * @brief		Mouse device class.
 */

#include <drivers/input.h>

#include <kernel/device.h>
#include <kernel/status.h>

#include <cassert>
#include <iostream>

#include "Cursor.h"
#include "MouseDevice.h"
#include "Session.h"
#include "WindowServer.h"

using namespace std;

/** Initialise the mouse device.
 * @param server	Server to send input to.
 * @param handle	Handle to device. */
MouseDevice::MouseDevice(WindowServer *server, handle_t handle) :
	m_server(server)
{
	SetHandle(handle);
}

/** Register events for the device. */
void MouseDevice::RegisterEvents() {
	RegisterEvent(DEVICE_EVENT_READABLE);
}

/** Handle an event.
 * @param id		ID of the event. */
void MouseDevice::EventReceived(int id) {
	assert(id == DEVICE_EVENT_READABLE);

	/* Read the event structure. */
	input_event_t event;
	status_t ret = device_read(m_handle, &event, sizeof(event), 0, NULL);
	if(ret != STATUS_SUCCESS) {
		clog << "Failed to read mouse event: " << ret << endl;
		return;
	}

	/* Pass the input along to the current session. */
	switch(event.type) {
	case INPUT_EVENT_REL_X:
		m_server->GetActiveSession()->GetCursor()->MoveRelative(event.value, 0);
		break;
	case INPUT_EVENT_REL_Y:
		m_server->GetActiveSession()->GetCursor()->MoveRelative(0, event.value);
		break;
	case INPUT_EVENT_BTN_DOWN:
		m_server->GetActiveSession()->GetCursor()->Down(event.value);
		break;
	case INPUT_EVENT_BTN_UP:
		m_server->GetActiveSession()->GetCursor()->Up(event.value);
		break;
	}
}
