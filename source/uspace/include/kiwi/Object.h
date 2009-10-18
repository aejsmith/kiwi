/* Kiwi API object base class
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
 * @brief		API object base class.
 */

#ifndef __KIWI_OBJECT_H
#define __KIWI_OBJECT_H

namespace kiwi {

/** Make an object noncopyable.
 * @param c		Class name. */
#define KIWI_OBJECT_NONCOPYABLE(c)	\
	c(const c &); \
	const c &operator =(const c &);

/** Define things necessary to allow use of private data classes.
 * @note		This defines a function in the class, GetPrivate(),
 *			that gives access to the class' private data pointer.
 * @note		After using this macro the access level will be
 *			private. The best thing to do is place it at the very
 *			start of the class definition. */
#define KIWI_OBJECT_PRIVATE		\
	friend class Private; \
	protected: \
		class Private; \
	private: \
		inline Private *GetPrivate() { return reinterpret_cast<Private *>(m_private); } \
    		inline const Private *GetPrivate() const { return reinterpret_cast<const Private *>(m_private); }

/** Base class for an API object. */
class Object {
	KIWI_OBJECT_PRIVATE
public:
	virtual ~Object();
protected:
	Object();
	Object(Private *p);
private:
	/** Pointer to private data structure.
	 * @note	This will actually point to an instance of the Private
	 *		class of the class at the bottom of the inheritance
	 *		tree, which should be derived from our Private class. */
	Private *m_private;
};

}

#endif /* __KIWI_OBJECT_H */
