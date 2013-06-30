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

#ifndef __KIWI_OBJECT_H
#define __KIWI_OBJECT_H

#include <kiwi/Signal.h>

namespace kiwi {

struct ObjectPrivate;

/** Base class for an API object. */
class KIWI_PUBLIC Object {
public:
	virtual ~Object();

	void DeleteLater();

	void AddSlot(internal::SignalImpl::Slot *slot);
	void RemoveSlot(internal::SignalImpl::Slot *slot);

	/** Signal emitted when the object is destroyed.
	 * @note		Handlers should NOT throw any exceptions.
	 * @param		Pointer to the object. */
	Signal<Object *> OnDestroy;
protected:
	Object();
private:
	ObjectPrivate *m_priv;		/**< Internal data for the object. */
};

}

#endif /* __KIWI_OBJECT_H */
