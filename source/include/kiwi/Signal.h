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
 * @brief		Type-safe callback system.
 */

#ifndef __KIWI_SIGNAL_H
#define __KIWI_SIGNAL_H

#include <kiwi/Object.h>
#include <list>

KIWI_BEGIN_NAMESPACE
KIWI_BEGIN_INTERNAL

/** Internal signal implementation. */
class KIWI_PUBLIC SignalImpl {
public:
	/** Base class for a slot. */
	class Slot {
	public:
		Slot(SignalImpl *impl) : m_impl(impl) {}
		virtual ~Slot();
	private:
		SignalImpl *m_impl;
	};

	// TODO: Replace this.
	typedef std::list<Slot *> SlotList;

	/** Iterator class. */
	class Iterator {
	public:
		Iterator(SignalImpl *impl);
		~Iterator();

		Slot *operator *();
	private:
		SignalImpl *m_impl;
		SlotList::const_iterator *m_iter;
	};

	SignalImpl();
	~SignalImpl();

	void Insert(Slot *slot);
	void Remove(Slot *slot);
private:
	SlotList *m_slots;
	friend class Iterator;
};

KIWI_END_INTERNAL

/** Class implementing a type-safe callback system. */
template <typename... A>
class Signal : public internal::SignalImpl {
	/** Base class for a slot. */
	class Slot : public SignalImpl::Slot {
	public:
		Slot(SignalImpl *impl) : SignalImpl::Slot(impl) {}
		virtual void operator ()(A...) = 0;
	};

	/** Slot for a non-member function. */
	class RegularSlot : public Slot {
	public:
		RegularSlot(SignalImpl *impl, void (*func)(A...)) :
			Slot(impl), m_func(func)
		{}

		void operator ()(A... args) {
			m_func(args...);
		}
	private:
		void (*m_func)(A...);
	};

	/** Slot for a member function. */
	template <typename T>
	class MemberSlot : public Slot {
	public:
		MemberSlot(SignalImpl *impl, T *obj, void (T::*func)(A...)) :
			Slot(impl), m_obj(obj), m_func(func)
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
		Insert(static_cast<SignalImpl::Slot *>(new RegularSlot(this, func)));
	}

	/** Connect a member function to this signal.
	 * @param obj		Object to call function on.
	 * @param func		Function to call. */
	template <typename T>
	void Connect(T *obj, void (T::*func)(A...)) {
		Insert(static_cast<SignalImpl::Slot *>(new MemberSlot<T>(this, obj, func)));
	}

	/** Connect this signal to another signal.
	 * @param signal	Other signal to emit when emitted. */
	void Connect(Signal<A...> &signal) {
		Connect(&signal, &Signal<A...>::operator ());
	}

	/** Invoke all slots connected to the signal.
	 * @param args		Arguments to pass to the slots. */
	void operator ()(A... args) {
		SignalImpl::Iterator it(this);
		SignalImpl::Slot *slot;
		while((slot = *it)) {
			(*static_cast<Slot *>(slot))(args...);
		}
	}
};

KIWI_END_NAMESPACE

#endif /* __KIWI_SIGNAL_H */
