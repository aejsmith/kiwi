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
 * @brief		Mutex class.
 */

#ifndef __KIWI_SUPPORT_MUTEX_H
#define __KIWI_SUPPORT_MUTEX_H

#include <kiwi/CoreDefs.h>
#include <util/mutex.h>

KIWI_BEGIN_NAMESPACE

/** Class implementing a lock with exclusive ownership. */
class Mutex {
public:
	/** Scoped lock class that automatically releases lock when destroyed. */
	class ScopedLock {
	public:
		ScopedLock(Mutex *lock) : m_lock(lock) { m_lock->Acquire(); }
		ScopedLock(Mutex &lock) : m_lock(&lock) { m_lock->Acquire(); }
		~ScopedLock() { m_lock->Release(); }
	private:
		Mutex *m_lock;
	};

	/** Construct the lock. */
	Mutex() { libc_mutex_init(&m_impl); }

	/** Acquire the lock.
	 * @note		Will block until the lock can be released. */
	void Acquire() { libc_mutex_lock(&m_impl, -1); }

	/** Acquire the lock.
	 * @param timeout	Maximum time to wait to acquire the lock, in
	 *			microseconds. If -1, the function will block
	 *			indefinitely until the lock can be acquired. If
	 *			0, the function will return an error if the
	 *			lock cannot be acquired immediately.
	 * @return		True if the lock was acquired, false if not. */
	bool Acquire(useconds_t timeout) {
		return (libc_mutex_lock(&m_impl, timeout) == STATUS_SUCCESS);
	}

	/** Release the lock. */
	void Release() { libc_mutex_unlock(&m_impl); }
private:
	libc_mutex_t m_impl;		/**< Implementation of the mutex. */
};

KIWI_END_NAMESPACE

#endif /* __KIWI_SUPPORT_MUTEX_H */
