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
