/*
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
 * @brief		Signal/slot implementation.
 *
 * @todo		This really ought to be automatically generated
 *			somehow.
 * @todo		Track slots created for methods within Object-derived
 *			classes, so that when the object is destroyed the slots
 *			are automatically removed.
 */

#ifndef __KIWI_SIGNAL_H
#define __KIWI_SIGNAL_H

#include <list>

namespace kiwi {
namespace internal {

/** Structure representing an unused argument. */
struct UnusedArg {};

/** Base class for a signal. */
class SignalBase {
protected:
	/** Slot base class. */
	class Slot {
	public:
		Slot(SignalBase *signal) : m_signal(signal) {}
		virtual ~Slot();
	private:
		SignalBase *m_signal;
	};

	friend class Slot;

	/** Class to call a slot with the correct arguments. */
	class Emitter {
	public:
		virtual void operator ()(Slot *slot) = 0;
	};

	SignalBase();
	~SignalBase();

	void _Connect(Slot *slot);
	void _Disconnect(Slot *slot);
	void _Emit(Emitter &em);
private:
	std::list<Slot *> *m_slots;
};

}

/** Signal taking 6 arguments.
 * @note		This template has all argument types defaulting to
 *			UnusedArg, which causes signals with less than 6
 *			arguments to use the specialisations below. */
template <typename A1 = internal::UnusedArg, typename A2 = internal::UnusedArg,
          typename A3 = internal::UnusedArg, typename A4 = internal::UnusedArg,
          typename A5 = internal::UnusedArg, typename A6 = internal::UnusedArg>
class Signal : public internal::SignalBase {
	/** Base class for a slot. */
	class Slot : public internal::SignalBase::Slot {
	public:
		Slot(Signal<A1, A2, A3, A4, A5, A6> *signal) : internal::SignalBase::Slot(signal) {}
		virtual void operator ()(A1, A2, A3, A4, A5, A6) = 0;
	};

	/** Slot for a member function. */
	template <typename O>
	class MemberSlot : public Slot {
	public:
		MemberSlot(Signal<A1, A2, A3, A4, A5, A6> *signal, O *obj, void (O::*handler)(A1, A2, A3, A4, A5, A6)) :
			Slot(signal), m_object(obj), m_func(handler)
		{}
		void operator ()(A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6) {
			(m_object->*m_func)(a1, a2, a3, a4, a5, a6);
		}
	private:
		O *m_object;
		void (O::*m_func)(A1, A2, A3, A4, A5, A6);
	};

	/** Slot for a function. */
	class FunctionSlot : public Slot {
	public:
		FunctionSlot(Signal<A1, A2, A3, A4, A5, A6> *signal, void (*handler)(A1, A2, A3, A4, A5, A6)) :
			Slot(signal), m_func(handler)
		{}
		void operator ()(A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6) {
			m_func(a1, a2, a3, a4, a5, a6);
		}
	private:
		void (*m_func)(A1, A2, A3, A4, A5, A6);
	};

	/** Class to call a slot with the correct arguments. */
	class Emitter : public internal::SignalBase::Emitter {
	public:
		Emitter(A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6) :
			m_a1(a1), m_a2(a2), m_a3(a3), m_a4(a4), m_a5(a5), m_a6(a6)
		{}

		void operator ()(internal::SignalBase::Slot *_slot) {
			Slot *slot = static_cast<Slot *>(_slot);
			(*slot)(m_a1, m_a2, m_a3, m_a4, m_a5, m_a6);
		}
	private:
		A1 m_a1;
		A2 m_a2;
		A3 m_a3;
		A4 m_a4;
		A5 m_a5;
		A6 m_a6;
	};
public:
	/** Emit the signal.
	 * @param a1		First argument.
	 * @param a2		Second argument.
	 * @param a3		Third argument.
	 * @param a4		Fourth argument.
	 * @param a5		Fifth argument.
	 * @param a6		Sixth argument. */
	void operator ()(A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6) {
		Emitter em(a1, a2, a3, a4, a5, a6);
		_Emit(em);
	}

	/** Connect a function to this signal.
	 * @param func		Function to call. */
	void Connect(void (*func)(A1, A2, A3, A4, A5, A6)) {
		_Connect(static_cast<internal::SignalBase::Slot *>(new FunctionSlot(this, func)));
	}

	/** Connect a member function to this signal.
	 * @param obj		Object to call function on.
	 * @param func		Function to call. */
	template <typename O>
	void Connect(O *obj, void (O::*func)(A1, A2, A3, A4, A5, A6)) {
		_Connect(static_cast<internal::SignalBase::Slot *>(new MemberSlot<O>(this, obj, func)));
	}

	/** Connect this signal to another signal.
	 * @param signal	Other signal to emit when emitted. */
	void Connect(Signal<A1, A2, A3, A4, A5, A6> &signal) {
		Connect(&signal, &Signal<A1, A2, A3, A4, A5, A6>::operator ());
	}
};

/** Signal taking 0 arguments. */
template <>
class Signal<internal::UnusedArg, internal::UnusedArg, internal::UnusedArg,
             internal::UnusedArg, internal::UnusedArg, internal::UnusedArg> : public internal::SignalBase {
	/** Base class for a slot. */
	class Slot : public internal::SignalBase::Slot {
	public:
		Slot(Signal<> *signal) : internal::SignalBase::Slot(signal) {}
		virtual void operator ()() = 0;
	};

	/** Slot for a member function. */
	template <typename O>
	class MemberSlot : public Slot {
	public:
		MemberSlot(Signal<> *signal, O *obj, void (O::*handler)()) :
			Slot(signal), m_object(obj), m_func(handler)
		{}
		void operator ()() {
			(m_object->*m_func)();
		}
	private:
		O *m_object;
		void (O::*m_func)();
	};

	/** Slot for a function. */
	class FunctionSlot : public Slot {
	public:
		FunctionSlot(Signal<> *signal,void (*handler)()) : Slot(signal), m_func(handler) {}
		void operator ()() {
			m_func();
		}
	private:
		void (*m_func)();
	};

	/** Class to call a slot with the correct arguments. */
	class Emitter : public internal::SignalBase::Emitter {
	public:
		Emitter() {}
		void operator ()(internal::SignalBase::Slot *_slot) {
			Slot *slot = static_cast<Slot *>(_slot);
			(*slot)();
		}
	};
public:
	/** Emit the signal. */
	void operator ()() {
		Emitter em;
		_Emit(em);
	}

	/** Connect a function to this signal.
	 * @param func		Function to call. */
	void Connect(void (*func)()) {
		_Connect(static_cast<internal::SignalBase::Slot *>(new FunctionSlot(this, func)));
	}

	/** Connect a member function to this signal.
	 * @param obj		Object to call function on.
	 * @param func		Function to call. */
	template <typename O>
	void Connect(O *obj, void (O::*func)()) {
		_Connect(static_cast<internal::SignalBase::Slot *>(new MemberSlot<O>(this, obj, func)));
	}

	/** Connect this signal to another signal.
	 * @param signal	Other signal to emit when emitted. */
	void Connect(Signal<> &signal) {
		Connect(&signal, &Signal<>::operator ());
	}
};

/** Signal taking 1 argument. */
template <typename A1>
class Signal<A1, internal::UnusedArg, internal::UnusedArg, internal::UnusedArg,
             internal::UnusedArg, internal::UnusedArg> : public internal::SignalBase {
	/** Base class for a slot. */
	class Slot : public internal::SignalBase::Slot {
	public:
		Slot(Signal<A1> *signal) : internal::SignalBase::Slot(signal) {}
		virtual void operator ()(A1) = 0;
	};

	/** Slot for a member function. */
	template <typename O>
	class MemberSlot : public Slot {
	public:
		MemberSlot(Signal<A1> *signal, O *obj, void (O::*handler)(A1)) :
			Slot(signal), m_object(obj), m_func(handler)
		{}
		void operator ()(A1 a1) {
			(m_object->*m_func)(a1);
		}
	private:
		O *m_object;
		void (O::*m_func)(A1);
	};

	/** Slot for a function. */
	class FunctionSlot : public Slot {
	public:
		FunctionSlot(Signal<A1> *signal, void (*handler)(A1)) : Slot(signal), m_func(handler) {}
		void operator ()(A1 a1) {
			m_func(a1);
		}
	private:
		void (*m_func)(A1);
	};

	/** Class to call a slot with the correct arguments. */
	class Emitter : public internal::SignalBase::Emitter {
	public:
		Emitter(A1 a1) : m_a1(a1) {}
		void operator ()(internal::SignalBase::Slot *_slot) {
			Slot *slot = static_cast<Slot *>(_slot);
			(*slot)(m_a1);
		}
	private:
		A1 m_a1;
	};
public:
	/** Emit the signal.
	 * @param a1		First argument. */
	void operator ()(A1 a1) {
		Emitter em(a1);
		_Emit(em);
	}

	/** Connect a function to this signal.
	 * @param func		Function to call. */
	void Connect(void (*func)(A1)) {
		_Connect(static_cast<internal::SignalBase::Slot *>(new FunctionSlot(this, func)));
	}

	/** Connect a member function to this signal.
	 * @param obj		Object to call function on.
	 * @param func		Function to call. */
	template <typename O>
	void Connect(O *obj, void (O::*func)(A1)) {
		_Connect(static_cast<internal::SignalBase::Slot *>(new MemberSlot<O>(this, obj, func)));
	}

	/** Connect this signal to another signal.
	 * @param signal	Other signal to emit when emitted. */
	void Connect(Signal<A1> &signal) {
		Connect(&signal, &Signal<A1>::operator ());
	}
};

/** Signal taking 2 arguments. */
template <typename A1, typename A2>
class Signal<A1, A2, internal::UnusedArg, internal::UnusedArg,
             internal::UnusedArg, internal::UnusedArg> : public internal::SignalBase {
	/** Base class for a slot. */
	class Slot : public internal::SignalBase::Slot {
	public:
		Slot(Signal<A1, A2> *signal) : internal::SignalBase::Slot(signal) {}
		virtual void operator ()(A1, A2) = 0;
	};

	/** Slot for a member function. */
	template <typename O>
	class MemberSlot : public Slot {
	public:
		MemberSlot(Signal<A1, A2> *signal, O *obj, void (O::*handler)(A1, A2)) :
			Slot(signal), m_object(obj), m_func(handler)
		{}
		void operator ()(A1 a1, A2 a2) {
			(m_object->*m_func)(a1, a2);
		}
	private:
		O *m_object;
		void (O::*m_func)(A1, A2);
	};

	/** Slot for a function. */
	class FunctionSlot : public Slot {
	public:
		FunctionSlot(Signal<A1, A2> *signal, void (*handler)(A1, A2)) : Slot(signal), m_func(handler) {}
		void operator ()(A1 a1, A2 a2) {
			m_func(a1, a2);
		}
	private:
		void (*m_func)(A1, A2);
	};

	/** Class to call a slot with the correct arguments. */
	class Emitter : public internal::SignalBase::Emitter {
	public:
		Emitter(A1 a1, A2 a2) : m_a1(a1), m_a2(a2) {}
		void operator ()(internal::SignalBase::Slot *_slot) {
			Slot *slot = static_cast<Slot *>(_slot);
			(*slot)(m_a1, m_a2);
		}
	private:
		A1 m_a1;
		A2 m_a2;
	};
public:
	/** Emit the signal.
	 * @param a1		First argument.
	 * @param a2		Second argument. */
	void operator ()(A1 a1, A2 a2) {
		Emitter em(a1, a2);
		_Emit(em);
	}

	/** Connect a function to this signal.
	 * @param func		Function to call. */
	void Connect(void (*func)(A1, A2)) {
		_Connect(static_cast<internal::SignalBase::Slot *>(new FunctionSlot(this, func)));
	}

	/** Connect a member function to this signal.
	 * @param obj		Object to call function on.
	 * @param func		Function to call. */
	template <typename O>
	void Connect(O *obj, void (O::*func)(A1, A2)) {
		_Connect(static_cast<internal::SignalBase::Slot *>(new MemberSlot<O>(this, obj, func)));
	}

	/** Connect this signal to another signal.
	 * @param signal	Other signal to emit when emitted. */
	void Connect(Signal<A1, A2> &signal) {
		Connect(&signal, &Signal<A1, A2>::operator ());
	}
};

/** Signal taking 3 arguments. */
template <typename A1, typename A2, typename A3>
class Signal<A1, A2, A3, internal::UnusedArg, internal::UnusedArg, internal::UnusedArg> : public internal::SignalBase {
	/** Base class for a slot. */
	class Slot : public internal::SignalBase::Slot {
	public:
		Slot(Signal<A1, A2, A3> *signal) : internal::SignalBase::Slot(signal) {}
		virtual void operator ()(A1, A2, A3) = 0;
	};

	/** Slot for a member function. */
	template <typename O>
	class MemberSlot : public Slot {
	public:
		MemberSlot(Signal<A1, A2, A3> *signal, O *obj, void (O::*handler)(A1, A2, A3)) :
			Slot(signal), m_object(obj), m_func(handler)
		{}
		void operator ()(A1 a1, A2 a2, A3 a3) {
			(m_object->*m_func)(a1, a2, a3);
		}
	private:
		O *m_object;
		void (O::*m_func)(A1, A2, A3);
	};

	/** Slot for a function. */
	class FunctionSlot : public Slot {
	public:
		FunctionSlot(Signal<A1, A2, A3> *signal, void (*handler)(A1, A2, A3)) : Slot(signal), m_func(handler) {}
		void operator ()(A1 a1, A2 a2, A3 a3) {
			m_func(a1, a2, a3);
		}
	private:
		void (*m_func)(A1, A2, A3);
	};

	/** Class to call a slot with the correct arguments. */
	class Emitter : public internal::SignalBase::Emitter {
	public:
		Emitter(A1 a1, A2 a2, A3 a3) : m_a1(a1), m_a2(a2), m_a3(a3) {}
		void operator ()(internal::SignalBase::Slot *_slot) {
			Slot *slot = static_cast<Slot *>(_slot);
			(*slot)(m_a1, m_a2, m_a3);
		}
	private:
		A1 m_a1;
		A2 m_a2;
		A3 m_a3;
	};
public:
	/** Emit the signal.
	 * @param a1		First argument.
	 * @param a2		Second argument.
	 * @param a3		Third argument. */
	void operator ()(A1 a1, A2 a2, A3 a3) {
		Emitter em(a1, a2, a3);
		_Emit(em);
	}

	/** Connect a function to this signal.
	 * @param func		Function to call. */
	void Connect(void (*func)(A1, A2, A3)) {
		_Connect(static_cast<internal::SignalBase::Slot *>(new FunctionSlot(this, func)));
	}

	/** Connect a member function to this signal.
	 * @param obj		Object to call function on.
	 * @param func		Function to call. */
	template <typename O>
	void Connect(O *obj, void (O::*func)(A1, A2, A3)) {
		_Connect(static_cast<internal::SignalBase::Slot *>(new MemberSlot<O>(this, obj, func)));
	}

	/** Connect this signal to another signal.
	 * @param signal	Other signal to emit when emitted. */
	void Connect(Signal<A1, A2, A3> &signal) {
		Connect(&signal, &Signal<A1, A2, A3>::operator ());
	}
};

/** Signal taking 4 arguments. */
template <typename A1, typename A2, typename A3, typename A4>
class Signal<A1, A2, A3, A4, internal::UnusedArg, internal::UnusedArg> : public internal::SignalBase {
	/** Base class for a slot. */
	class Slot : public internal::SignalBase::Slot {
	public:
		Slot(Signal<A1, A2, A3, A4> *signal) : internal::SignalBase::Slot(signal) {}
		virtual void operator ()(A1, A2, A3, A4) = 0;
	};

	/** Slot for a member function. */
	template <typename O>
	class MemberSlot : public Slot {
	public:
		MemberSlot(Signal<A1, A2, A3, A4> *signal, O *obj, void (O::*handler)(A1, A2, A3, A4)) :
			Slot(signal), m_object(obj), m_func(handler)
		{}
		void operator ()(A1 a1, A2 a2, A3 a3, A4 a4) {
			(m_object->*m_func)(a1, a2, a3, a4);
		}
	private:
		O *m_object;
		void (O::*m_func)(A1, A2, A3, A4);
	};

	/** Slot for a function. */
	class FunctionSlot : public Slot {
	public:
		FunctionSlot(Signal<A1, A2, A3, A4> *signal, void (*handler)(A1, A2, A3, A4)) :
			Slot(signal), m_func(handler)
		{}
		void operator ()(A1 a1, A2 a2, A3 a3, A4 a4) {
			m_func(a1, a2, a3, a4);
		}
	private:
		void (*m_func)(A1, A2, A3, A4);
	};

	/** Class to call a slot with the correct arguments. */
	class Emitter : public internal::SignalBase::Emitter {
	public:
		Emitter(A1 a1, A2 a2, A3 a3, A4 a4) : m_a1(a1), m_a2(a2), m_a3(a3), m_a4(a4) {}
		void operator ()(internal::SignalBase::Slot *_slot) {
			Slot *slot = static_cast<Slot *>(_slot);
			(*slot)(m_a1, m_a2, m_a3, m_a4);
		}
	private:
		A1 m_a1;
		A2 m_a2;
		A3 m_a3;
		A4 m_a4;
	};
public:
	/** Emit the signal.
	 * @param a1		First argument.
	 * @param a2		Second argument.
	 * @param a3		Third argument.
	 * @param a4		Fourth argument. */
	void operator ()(A1 a1, A2 a2, A3 a3, A4 a4) {
		Emitter em(a1, a2, a3, a4);
		_Emit(em);
	}

	/** Connect a function to this signal.
	 * @param func		Function to call. */
	void Connect(void (*func)(A1, A2, A3, A4)) {
		_Connect(static_cast<internal::SignalBase::Slot *>(new FunctionSlot(this, func)));
	}

	/** Connect a member function to this signal.
	 * @param obj		Object to call function on.
	 * @param func		Function to call. */
	template <typename O>
	void Connect(O *obj, void (O::*func)(A1, A2, A3, A4)) {
		_Connect(static_cast<internal::SignalBase::Slot *>(new MemberSlot<O>(this, obj, func)));
	}

	/** Connect this signal to another signal.
	 * @param signal	Other signal to emit when emitted. */
	void Connect(Signal<A1, A2, A3, A4> &signal) {
		Connect(&signal, &Signal<A1, A2, A3, A4>::operator ());
	}
};

/** Signal taking 5 arguments. */
template <typename A1, typename A2, typename A3, typename A4, typename A5>
class Signal<A1, A2, A3, A4, A5, internal::UnusedArg> : public internal::SignalBase {
	/** Base class for a slot. */
	class Slot : public internal::SignalBase::Slot {
	public:
		Slot(Signal<A1, A2, A3, A4, A5> *signal) : internal::SignalBase::Slot(signal) {}
		virtual void operator ()(A1, A2, A3, A4, A5) = 0;
	};

	/** Slot for a member function. */
	template <typename O>
	class MemberSlot : public Slot {
	public:
		MemberSlot(Signal<A1, A2, A3, A4, A5> *signal, O *obj, void (O::*handler)(A1, A2, A3, A4, A5)) :
			Slot(signal), m_object(obj), m_func(handler)
		{}
		void operator ()(A1 a1, A2 a2, A3 a3, A4 a4, A5 a5) {
			(m_object->*m_func)(a1, a2, a3, a4, a5);
		}
	private:
		O *m_object;
		void (O::*m_func)(A1, A2, A3, A4, A5);
	};

	/** Slot for a function. */
	class FunctionSlot : public Slot {
	public:
		FunctionSlot(Signal<A1, A2, A3, A4, A5> *signal, void (*handler)(A1, A2, A3, A4, A5)) :
			Slot(signal), m_func(handler)
		{}
		void operator ()(A1 a1, A2 a2, A3 a3, A4 a4, A5 a5) {
			m_func(a1, a2, a3, a4, a5);
		}
	private:
		void (*m_func)(A1, A2, A3, A4, A5);
	};

	/** Class to call a slot with the correct arguments. */
	class Emitter : public internal::SignalBase::Emitter {
	public:
		Emitter(A1 a1, A2 a2, A3 a3, A4 a4, A5 a5) : m_a1(a1), m_a2(a2), m_a3(a3), m_a4(a4), m_a5(a5) {}
		void operator ()(internal::SignalBase::Slot *_slot) {
			Slot *slot = static_cast<Slot *>(_slot);
			(*slot)(m_a1, m_a2, m_a3, m_a4, m_a5);
		}
	private:
		A1 m_a1;
		A2 m_a2;
		A3 m_a3;
		A4 m_a4;
		A5 m_a5;
	};
public:
	/** Emit the signal.
	 * @param a1		First argument.
	 * @param a2		Second argument.
	 * @param a3		Third argument.
	 * @param a4		Fourth argument.
	 * @param a5		Fifth argument. */
	void operator ()(A1 a1, A2 a2, A3 a3, A4 a4, A5 a5) {
		Emitter em(a1, a2, a3, a4, a5);
		_Emit(em);
	}

	/** Connect a function to this signal.
	 * @param func		Function to call. */
	void Connect(void (*func)(A1, A2, A3, A4, A5)) {
		_Connect(static_cast<internal::SignalBase::Slot *>(new FunctionSlot(this, func)));
	}

	/** Connect a member function to this signal.
	 * @param obj		Object to call function on.
	 * @param func		Function to call. */
	template <typename O>
	void Connect(O *obj, void (O::*func)(A1, A2, A3, A4, A5)) {
		_Connect(static_cast<internal::SignalBase::Slot *>(new MemberSlot<O>(this, obj, func)));
	}

	/** Connect this signal to another signal.
	 * @param signal	Other signal to emit when emitted. */
	void Connect(Signal<A1, A2, A3, A4, A5> &signal) {
		Connect(&signal, &Signal<A1, A2, A3, A4, A5>::operator ());
	}
};

}

#endif /* __KIWI_SIGNAL_H */
