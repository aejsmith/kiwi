/*
 * Copyright (C) 2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Mutex class.
 */

#ifndef __KIWI_SUPPORT_MUTEX_H
#define __KIWI_SUPPORT_MUTEX_H

#include <kernel/status.h>
#include <kiwi/CoreDefs.h>
#include <util/mutex.h>

namespace kiwi {

/** Class implementing a lock with exclusive ownership. */
class KIWI_PUBLIC Mutex {
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

}

#endif /* __KIWI_SUPPORT_MUTEX_H */
