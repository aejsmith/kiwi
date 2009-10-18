/* Kiwi Object class private data
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
 * @brief		Object class private data.
 */

#ifndef __KIWI_PRIVATE_OBJECT_H
#define __KIWI_PRIVATE_OBJECT_H

#include <kiwi/Object.h>

/** Define things to allow public classes to access their private class.
 * @note		This should be placed at the start of a private class
 *			definition.
 * @param c		Public class name. */
#define KIWI_OBJECT_PUBLIC(c)	\
	friend class c;

namespace kiwi {

/** Object class private data. */
class Object::Private {
	KIWI_OBJECT_PUBLIC(Object);
protected:
	virtual ~Private();
};

}

#endif /* __KIWI_PRIVATE_OBJECT_H */
