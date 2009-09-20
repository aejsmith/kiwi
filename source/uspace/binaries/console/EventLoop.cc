/* Kiwi console event loop
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
 * @brief		Console event loop.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "EventLoop.h"

/** Singleton event loop instance. */
EventLoop EventLoop::m_instance;

/** EventLoop constructor. */
EventLoop::EventLoop() :
	m_handles(0), m_events(0), m_callbacks(0), m_datas(0)
{
}

/** Run the event loop. */
void EventLoop::Run(void) {
	int ret;

	while(true) {
		if((ret = handle_wait_multiple(m_handles, m_events, m_count, -1)) < 0) {
			printf("Failed to wait for events (%d)\n", ret);
			return;
		}

		m_callbacks[ret](m_datas[ret]);
	}
}

/** Add a handle to the event loop.
 * @param handle	Handle to use.
 * @param event		Event to wait for on the handle.
 * @param callback	Function to call when event happens.
 * @param data		Data argument to pass to callback. */
void EventLoop::AddHandle(handle_t handle, int event, Callback callback, void *data) {
	m_handles = reinterpret_cast<handle_t *>(realloc(m_handles, sizeof(handle_t) * (m_count + 1)));
	m_events = reinterpret_cast<int *>(realloc(m_events, sizeof(int) * (m_count + 1)));
	m_callbacks = reinterpret_cast<Callback *>(realloc(m_callbacks, sizeof(Callback) * (m_count + 1)));
	m_datas = reinterpret_cast<void **>(realloc(m_datas, sizeof(void *) * (m_count + 1)));

	m_handles[m_count] = handle;
	m_events[m_count] = event;
	m_callbacks[m_count] = callback;
	m_datas[m_count] = data;
	m_count++;
}
