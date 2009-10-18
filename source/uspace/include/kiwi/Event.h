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

#ifndef __KIWI_EVENT_H
#define __KIWI_EVENT_H

#include <kiwi/Object.h>

namespace kiwi {

/** Base class for all events. */
class Event {
public:
	Event(Object *obj);

	/** Get the object that the event came from.
	 * @return		Pointer to object that event came from. */
	Object *GetObject() const {
		return m_object;
	}

	/** Get the time that the event occurred at.
	 * @return		Time that event occurred at. */
	//uint64_t GetTimeStamp() const {
	//	return m_timestamp;
	//}
private:
	Object *m_object;
	//uint64_t m_timestamp;
};

/** Base class for a functor to do something with an event. */
class EventFunctor {
public:
	virtual void operator ()(Event &) = 0;
};

/** Class implementing a list of event functors. */
class EventFunctorList {
	KIWI_OBJECT_PRIVATE
public:
	EventFunctorList();
	~EventFunctorList();
	void Insert(EventFunctor *func);
	void operator ()(Event &event);
private:
	Private *m_private;
};

/** Template class implementing an event callback system.
 * @param T		Type of the class that will be passed to handlers.
 *			Must be derived from Event. */
template <typename T>
class Signal {
	/** Functor representing a member handler. */
	template <typename O>
	class MemberHandler : public EventFunctor {
	public:
		MemberHandler(O *obj, void (O::*handler)(T &)) : m_object(obj), m_func(handler) {}
		virtual void operator ()(Event &event) {
			(m_object->*m_func)(static_cast<T &>(event));
		}
	private:
		O *m_object;
		void (O::*m_func)(T &);
	};

	/** Functor representing a non-member handler. */
	class Handler : public EventFunctor {
	public:
		Handler(void (*handler)(T &)) : m_func(handler) {}
		virtual void operator ()(Event &event) {
			m_func(static_cast<T &>(event));
		}
	private:
		void (*m_func)(T &);
	};
public:
	/** Register a handler.
	 * @param func		Function to call when signalled. */
	void Connect(void (*func)(T &)) {
		m_list.Insert(new Handler(func));
	}

	/** Register a handler.
	 * @param recipient	Object that will handle the event.
	 * @param func		Handler function (member of recipient). */
	template <typename O>
	void Connect(O *recipient, void (O::*func)(T &)) {
		m_list.Insert(new MemberHandler<O>(recipient, func));
	}

	/** Emit the signal.
	 * @param event		Reference to event information object. */
	void operator ()(T &event) {
		m_list(event);
	}
private:
	EventFunctorList m_list;
};

}

#endif /* __KIWI_EVENT_H */
