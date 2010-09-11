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
 * @brief		Display class.
 */

#include <kernel/device.h>
#include <kernel/object.h>
#include <kernel/status.h>

#include <cassert>
#include <iostream>
#include <exception>

#include "Display.h"

using namespace kiwi;
using namespace std;

/** Display constructor.
 * @param path		Device tree path to display. */
Display::Display(const char *path) {
	status_t ret;

	/* Open the device. */
	handle_t handle;
	ret = device_open(path, &handle);
	if(ret != STATUS_SUCCESS) {
		clog << "Failed to open display device " << path << " (" << ret << ")" << endl;
		throw exception();
	}
	SetHandle(handle);

	/* Get mode information. */
	size_t count;
	ret = device_request(m_handle, DISPLAY_MODE_COUNT, NULL, 0, &count, sizeof(count), NULL);
	if(ret != STATUS_SUCCESS) {
		clog << "Failed to get mode count for " << path << " (" << ret << ")" << endl;
		throw exception();
	}
	display_mode_t *modes = new display_mode_t[count];
	ret = device_request(m_handle, DISPLAY_GET_MODES, NULL, 0, modes, sizeof(*modes) * count, NULL);
	if(ret != STATUS_SUCCESS) {
		clog << "Failed to get modes for " << path << " (" << ret << ")" << endl;
		delete[] modes;
		throw exception();
	}
	for(size_t i = 0; i < count; i++) {
		m_modes.push_back(modes[i]);
	}
	delete[] modes;

	/* Try to get the preferred display mode. */
	ret = device_request(m_handle, DISPLAY_GET_PREFERRED_MODE, NULL, 0, &m_current_mode, sizeof(display_mode_t), NULL);
	if(ret != STATUS_SUCCESS) {
		clog << "Failed to get preferred mode for " << path << " (" << ret << ")" << endl;
		throw exception();
	}

	/* Set the mode. */
	ret = device_request(m_handle, DISPLAY_SET_MODE, &m_current_mode.id, sizeof(m_current_mode.id), NULL, 0, NULL);
	if(ret != STATUS_SUCCESS) {
		clog << "Failed to set mode for " << path << " (" << ret << ")" << endl;
		throw exception();
	}

	/* Create the surface for the framebuffer. */
	m_surface = new Surface(m_handle, m_current_mode.offset, m_current_mode.width,
	                        m_current_mode.height, m_current_mode.format);
}

/** Display destructor. */
Display::~Display() {}

/** Register events with the event loop. */
void Display::RegisterEvents() {
	RegisterEvent(DISPLAY_EVENT_REDRAW);
}

/** Event callback function.
 * @param event		Event number. */
void Display::EventReceived(int event) {
	assert(event == DISPLAY_EVENT_REDRAW);
}
