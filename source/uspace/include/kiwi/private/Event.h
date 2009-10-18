/* Kiwi event class private data
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
 * @brief		Event class private data.
 */

#ifndef __KIWI_PRIVATE_EVENT_H
#define __KIWI_PRIVATE_EVENT_H

#include <kiwi/private/Object.h>
#include <kiwi/Event.h>

#include <list>

namespace kiwi {

/** EventFunctorList class private data. */
class EventFunctorList::Private {
	KIWI_OBJECT_PUBLIC(EventFunctorList);
	std::list<EventFunctor *> m_list;
};

}

#endif /* __KIWI_PRIVATE_EVENT_H */
