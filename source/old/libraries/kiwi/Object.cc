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
 * @brief		API object base class.
 */

#include <kiwi/EventLoop.h>
#include <kiwi/Object.h>

#include <list>

#include "Internal.h"

using namespace kiwi;

/** Internal data for Object. */
struct kiwi::ObjectPrivate {
	typedef std::list<internal::SignalImpl::Slot *> SlotList;

	ObjectPrivate() : destroyed(false) {}

	bool destroyed;			/**< Whether the object is being destroyed. */
	SlotList slots;			/**< Slots associated with this object. */
};

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
	/* Call our OnDestroy signal. */
	try {
		OnDestroy(this);
	} catch(...) {
		libkiwi_warn("Object::~Object: Unexpected exception in OnDestroy handler.");
		throw;
	}

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
