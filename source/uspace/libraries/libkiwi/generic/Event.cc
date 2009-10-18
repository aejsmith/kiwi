/* Kiwi event handling classes
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
 * @brief		Event handling classes.
 */

#include <kiwi/private/Event.h>

#include <iostream>

using namespace kiwi;
using namespace std;

/** Constructor for the Event class. */
Event::Event(Object *obj) : m_object(obj) {}

/** Constructor for EventFunctorList. */
EventFunctorList::EventFunctorList() : m_private(new EventFunctorList::Private) {}

/** Destructor for EventFunctorList. */
EventFunctorList::~EventFunctorList() {
	list<EventFunctor *>::iterator it;
	Private *p = GetPrivate();

	/* Clear out the list. */
	for(it = p->m_list.begin(); it != p->m_list.end(); ++it) {
		delete (*it);
		p->m_list.erase(it);
	}

	delete m_private;
}

/** Add a functor to the list.
 * @param func		Functor to add. Must be dynamically allocated, will
 *			be deleted when the list is destroyed. */
void EventFunctorList::Insert(EventFunctor *func) {
	Private *p = GetPrivate();
	p->m_list.push_back(func);
}

/** Invoke all functors on the list.
 * @param event		Event to pass to functors. */
void EventFunctorList::operator ()(Event &event) {
	list<EventFunctor *>::iterator it;
	Private *p = GetPrivate();

	for(it = p->m_list.begin(); it != p->m_list.end(); ++it) {
		(**it)(event);
	}
}
