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
 * @brief		Timer class.
 */

#include <kernel/time.h>

#include <kiwi/Error.h>
#include <kiwi/Timer.h>

#include <cassert>

#include "Internal.h"

using namespace kiwi;

/** Constructor for Timer.
 * @param mode		Mode for the timer. If kOneShotMode, the timer will
 *			only fire once after it is started. If kPeriodicMode,
 *			it will fire periodically after being started, until it
 *			is stopped with Stop().
 * @throw Error		If unable to create the timer. This can only happen if
 *			the process' handle table is full. */
Timer::Timer(Mode mode) : m_mode(mode), m_running(false) {
	handle_t handle;
	status_t ret;

	assert(mode == kOneShotMode || mode == kPeriodicMode);

	ret = kern_timer_create(0, &handle);
	if(unlikely(ret != STATUS_SUCCESS)) {
		throw Error(ret);
	}

	SetHandle(handle);
}

/** Start the timer.
 * @param interval	Interval for the timer. */
void Timer::Start(useconds_t interval) {
	status_t ret;
	int mode;

	assert(interval > 0);

	mode = (m_mode == kPeriodicMode) ? TIMER_PERIODIC : TIMER_ONESHOT;
	ret = kern_timer_start(m_handle, interval, mode);
	assert(ret == STATUS_SUCCESS);
	m_running = true;
}

/** Stop the timer. */
void Timer::Stop() {
	status_t ret = kern_timer_stop(m_handle, 0);
	assert(ret == STATUS_SUCCESS);
	m_running = false;
}

/** Register events with the event loop. */
void Timer::RegisterEvents() {
	RegisterEvent(TIMER_EVENT);
}

/** Handle an event on the timer.
 * @param event		Event ID. */
void Timer::HandleEvent(int event) {
	assert(event == TIMER_EVENT);

	if(m_mode == kOneShotMode) {
		m_running = false;
	}

	OnTimer();
}
