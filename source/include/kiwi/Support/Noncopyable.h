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
 * @brief		Noncopyable class.
 */

#ifndef __KIWI_SUPPORT_NONCOPYABLE_H
#define __KIWI_SUPPORT_NONCOPYABLE_H

#include <kiwi/CoreDefs.h>

KIWI_BEGIN_NAMESPACE

/** Base class that prevents derived classes from being copied. */
class Noncopyable {
protected:
	Noncopyable() {}
	~Noncopyable() {}
private:
	Noncopyable(const Noncopyable &);
	const Noncopyable &operator =(const Noncopyable &);
};

KIWI_END_NAMESPACE

#endif /* __KIWI_SUPPORT_NONCOPYABLE_H */
