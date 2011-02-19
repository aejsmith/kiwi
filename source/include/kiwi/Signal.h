/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		Type-safe callback system.
 */

#ifndef __KIWI_SIGNAL_H
#define __KIWI_SIGNAL_H

#include <kiwi/CoreDefs.h>

#include <type_traits>
#include <list>

namespace kiwi {
namespace internal {

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

}

class Object;

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
	template <typename T, bool object = std::is_base_of<Object, T>::value>
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

	/** Slot for a member function in an Object-derived class. */
	template <typename T>
	class MemberSlot<T, true> : public Slot {
	public:
		MemberSlot(SignalImpl *impl, T *obj, void (T::*func)(A...)) :
			Slot(impl), m_obj(obj), m_func(func)
		{
			/* Tell the Object about this slot, so that when the
			 * object is destroyed the slot will automatically be
			 * removed from the signal. */
			m_obj->AddSlot(this);
		}

		~MemberSlot() {
			/* Remove the slot from the object. */
			m_obj->RemoveSlot(this);
		}

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

}

#endif /* __KIWI_SIGNAL_H */
