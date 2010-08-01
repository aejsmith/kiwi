/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		API object base class.
 */

#include <kiwi/EventLoop.h>
#include <kiwi/Object.h>

using namespace kiwi;

/** Constructor for Object.
 * @note		Protected - Object cannot be instantiated directly. */
Object::Object() {}

/** Destructor for Object. */
Object::~Object() {}

/** Schedule the object for deletion when control returns to the event loop. */
void Object::deleteLater() {
	EventLoop *loop = EventLoop::instance();
	if(loop) {
		loop->deleteObject(this);
	}
}
