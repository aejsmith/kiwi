/* Kiwi signal/slot implementation
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
 */

#include <kiwi/Signal.h>

#include <assert.h>

using namespace kiwi::internal;
using namespace std;

SignalBase::Slot::~Slot() {
	m_signal->_Disconnect(this);
}

SignalBase::SignalBase() : m_slots(0) {}

SignalBase::~SignalBase() {
	list<Slot *>::iterator it;

	if(m_slots) {
		while((it = m_slots->begin()) != m_slots->end()) {
			m_slots->erase(it);
			delete *it;
		}
	}
}

void SignalBase::_Connect(Slot *slot) {
	if(!m_slots) {
		m_slots = new list<Slot *>;
	}
	m_slots->push_back(slot);
}

void SignalBase::_Disconnect(Slot *slot) {
	list<Slot *>::iterator it;

	assert(m_slots);

	for(it = m_slots->begin(); it != m_slots->end(); ++it) {
		if(*it == slot) {
			m_slots->erase(it);
			delete *it;
			return;
		}
	}
}

void SignalBase::_Emit(Emitter &em) {
	list<Slot *>::iterator it;

	if(m_slots) {
		for(it = m_slots->begin(); it != m_slots->end(); ++it) {
			em(*it);
		}
	}
}