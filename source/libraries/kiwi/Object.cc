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
 * @brief		API object base class.
 */

#include <kiwi/EventLoop.h>
#include <kiwi/Object.h>

#include <list>

#include "Internal.h"

using namespace kiwi;

KIWI_BEGIN_NAMESPACE

/** Internal data for Object. */
struct ObjectPrivate {
	typedef std::list<internal::SignalImpl::Slot *> SlotList;

	ObjectPrivate() : destroyed(false) {}

	bool destroyed;			/**< Whether the object is being destroyed. */
	SlotList slots;			/**< Slots associated with this object. */
};

KIWI_END_NAMESPACE

/** Constructor for Object.
 * @note		Protected - Object cannot be instantiated directly. */
Object::Object() {
	m_priv = new ObjectPrivate;
}

/** Schedule the object for deletion when control returns to the event loop. */
void Object::DeleteLater() {
	EventLoop *loop = EventLoop::Instance();
	if(loop) {
		loop->DeleteObject(this);
	} else {
		libkiwi_warn("Object::DeleteLater: Called without an event loop, will not be deleted.");
	}
}

/** Add a slot to the object.
 * @param slot		Slot to add. This slot will be removed from its signal
 *			when the object is destroyed. */
void Object::AddSlot(internal::SignalImpl::Slot *slot) {
	m_priv->slots.push_back(slot);
}

/** Remove a slot from the object.
 * @param slot		Slot to remove. */
void Object::RemoveSlot(internal::SignalImpl::Slot *slot) {
	if(!m_priv->destroyed) {
		m_priv->slots.remove(slot);
	}
}

/** Destructor for Object. */
Object::~Object() {
	/* Set the destroyed flag. This is to speed up slot removal: deleting
	 * a slot will cause RemoveSlot() to be called, which checks the
	 * flag to see if it needs to bother removing from the list. */
	m_priv->destroyed = true;

	/* Remove all slots pointing to this object from the Signal that they
	 * are for. */
	for(auto it = m_priv->slots.begin(); it != m_priv->slots.end(); ++it) {
		delete *it;
	}

	delete m_priv;
}
