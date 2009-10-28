/* Kiwi API object base class
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
 * @brief		API object base class.
 */

#include <kiwi/private/Object.h>

using namespace std;
using namespace kiwi;

/** Destructor for Object::Private. */
Object::Private::~Private() {}

/** Constructor for Object.
 * @note		Protected - Object cannot be instantiated directly. */
Object::Object() : m_private(new Object::Private) {}

/** Constructor for Object.
 * @note		Protected - Object cannot be instantiated directly.
 * @param p		Private data pointer. */
Object::Object(Private *p) : m_private(p) {}

/** Destructor for Object. */
Object::~Object() {
	delete m_private;
}