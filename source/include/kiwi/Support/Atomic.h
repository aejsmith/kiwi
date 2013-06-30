/*
 * Copyright (C) 2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Atomic operations wrapper class.
 */

#ifndef __KIWI_SUPPORT_ATOMIC_H
#define __KIWI_SUPPORT_ATOMIC_H

#include <kiwi/CoreDefs.h>
#include <type_traits>

namespace kiwi {

/** Class providing atomic operations on a type.
 * @param T		Type to operate on (must be integral or pointer). */
template <typename T>
class Atomic {
	static_assert(
		std::is_integral<T>::value || std::is_pointer<T>::value,
		"T must be an integral or pointer type"
	);
public:
	Atomic() : m_value(0) {}
	Atomic(const T &value) : m_value(value) {}

	/** Cast to T.
	 * @return		The value cast to T. */
	operator T() const { return m_value; }

	/** Atomically increment the value.
	 * @return		New value. */
	T &operator ++() {
		return __sync_add_and_fetch(&m_value, 1);
	}

	/** Atomically increment the value.
	 * @return		Previous value. */
	T operator ++(int) {
		return __sync_fetch_and_add(&m_value, 1);
	}

	/** Atomically decrement the value.
	 * @return		New value. */
	T &operator --() {
		return __sync_sub_and_fetch(&m_value, 1);
	}

	/** Atomically decrement the value.
	 * @return		Previous value. */
	T operator --(int) {
		return __sync_fetch_and_sub(&m_value, 1);
	}

	/** Atomically add a value.
	 * @param value		Value to add.
	 * @return		New value. */
	T operator +=(const T &value) {
		return __sync_add_and_fetch(&m_value, value);
	}

	/** Atomically subtract a value.
	 * @param value		Value to subtract.
	 * @return		New value. */
	T operator -=(const T &value) {
		return __sync_sub_and_fetch(&m_value, value);
	}

	/** Atomic bitwise OR operator.
	 * @param value		Value to OR with.
	 * @return		New value. */
	T operator |=(const T &value) {
		return __sync_or_and_fetch(&m_value, value);
	}

	/** Atomic bitwise AND operator.
	 * @param value		Value to AND with.
	 * @return		New value. */
	T operator &=(const T &value) {
		return __sync_and_and_fetch(&m_value, value);
	}

	/** Atomic bitwise XOR operator.
	 * @param value		Value to XOR with.
	 * @return		New value. */
	T operator ^=(const T &value) {
		return __sync_xor_and_fetch(&m_value, value);
	}

	/** Test-and-set operation.
	 * @param test		Value to test against.
	 * @param set		Value to set if test succeeds.
	 * @return		True if set, false if were not equal. */
	bool TestAndSet(const T &test, const T &set) {
		return __sync_bool_compare_and_swap(&m_value, test, set);
	}

	/** Atomically add a value.
	 * @param value		Value to add.
	 * @return		Previous value. */
	T FetchAdd(const T &value) {
		return __sync_fetch_and_add(&m_value, value);
	}

	/** Atomically subtract a value.
	 * @param value		Value to subtract.
	 * @return		Previous value. */
	T FetchSub(const T &value) {
		return __sync_fetch_and_sub(&m_value, value);
	}

	/** Atomic bitwise OR operator.
	 * @param value		Value to OR with.
	 * @return		Previous value. */
	T FetchOR(const T &value) {
		return __sync_fetch_and_or(&m_value, value);
	}

	/** Atomic bitwise AND operator.
	 * @param value		Value to AND with.
	 * @return		Previous value. */
	T FetchAND(const T &value) {
		return __sync_fetch_and_and(&m_value, value);
	}

	/** Atomic bitwise XOR operator.
	 * @param value		Value to XOR with.
	 * @return		Previous value. */
	T FetchXOR(const T &value) {
		return __sync_fetch_and_xor(&m_value, value);
	}
private:
	volatile T m_value;
};

}

#endif /* __KIWI_SUPPORT_ATOMIC_H */
