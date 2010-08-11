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
 * @brief		Signal/slot implementation.

 * @todo		Track slots created for methods within Object-derived
 *			classes, so that when the object is destroyed the slots
 *			are automatically removed.
 * @todo		Have the implementation in a separate non-template
 *			class so that std::list is not instantiated for each
 *			instantiation of Signal.
 */

#ifndef __KIWI_SIGNAL_H
#define __KIWI_SIGNAL_H

#include <list>

namespace kiwi {

template <typename... A>
class Signal {
	/** Base class for a slot. */
	class BaseSlot {
	public:
		virtual void operator ()(A...) = 0;
	};

	/** Slot for a non-member function. */
	class Slot : public BaseSlot {
	public:
		Slot(void (*func)(A...)) : m_func(func) {}

		void operator ()(A... args) {
			m_func(args...);
		}
	private:
		void (*m_func)(A...);
	};

	/** Slot for a member function. */
	template <typename T>
	class MemberSlot : public BaseSlot {
	public:
		MemberSlot(T *obj, void (T::*func)(A...)) :
			m_obj(obj), m_func(func)
		{}

		void operator ()(A... args) {
			(m_obj->*m_func)(args...);
		}
	private:
		T *m_obj;
		void (T::*m_func)(A...);
	};

public:
	/** Connect a function to this signal.
	 * @param func		Function to call. */
	void Connect(void (*func)(A...)) {
		m_slots.push_back(static_cast<BaseSlot *>(new Slot(func)));
	}

	/** Connect a member function to this signal.
	 * @param obj		Object to call function on.
	 * @param func		Function to call. */
	template <typename T>
	void Connect(T *obj, void (T::*func)(A...)) {
		m_slots.push_back(static_cast<BaseSlot *>(new MemberSlot<T>(obj, func)));
	}

	/** Connect this signal to another signal.
	 * @param signal	Other signal to emit when emitted. */
	void Connect(Signal<A...> &signal) {
		Connect(&signal, &Signal<A...>::operator ());
	}

	/** Invoke all slots connected to the signal.
	 * @param args		Arguments to pass to the slots. */
	void operator ()(A... args) {
		for(typename std::list<BaseSlot *>::iterator it = m_slots.begin();
		    it != m_slots.end();
		    it++)
		{
			(**it)(args...);
		}
	}
private:
	std::list<BaseSlot *> m_slots;	/**< List of registered slots. */
};

}

#endif /* __KIWI_SIGNAL_H */
