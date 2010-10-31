/*
 * Copyright (C) 2010 Alex Smith
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

#include <kiwi/Signal.h>

using namespace kiwi::internal;
using namespace kiwi;

/** Destroy a slot, removing it from the list. */
SignalImpl::Slot::~Slot() {
	m_impl->Remove(this);
}

/** Construct a new iterator.
 * @param impl		Pointer to signal implementation. */
SignalImpl::Iterator::Iterator(SignalImpl *impl) : m_impl(impl) {
	m_iter = new SlotList::const_iterator(m_impl->m_slots->begin());
}

/** Destroy an iterator. */
SignalImpl::Iterator::~Iterator() {
	delete m_iter;
}

/** Get the next slot from an iterator.
 * @return		Pointer to next slot, or NULL if no more slots. */
SignalImpl::Slot *SignalImpl::Iterator::operator *() {
	if(*m_iter == m_impl->m_slots->end()) {
		return 0;
	}

	return *((*m_iter)++);
}

/** Construct a signal implementation object. */
SignalImpl::SignalImpl() {
	m_slots = new SlotList();
}

/** Destroy a signal, freeing all slots in the list. */
SignalImpl::~SignalImpl() {
	SlotList::iterator it;
	while((it = m_slots->begin()) != m_slots->end()) {
		delete *it;
	}
	delete m_slots;
}

/** Insert a slot into the signal's list.
 * @param slot		Slot to add. */
void SignalImpl::Insert(Slot *slot) {
	m_slots->push_back(slot);
}

/** Remove a slot from the signal's list.
 * @param slot		Slot to remove. */
void SignalImpl::Remove(Slot *slot) {
	m_slots->remove(slot);
}
