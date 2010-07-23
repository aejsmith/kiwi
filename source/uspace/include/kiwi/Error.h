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
 * @brief		Error handling class.
 */

#ifndef __KIWI_ERROR_H
#define __KIWI_ERROR_H

#include <kernel/errors.h>
#include <string>

namespace kiwi {

/** Error range definitions. */
enum {
	KIWI_ERROR_BASE = KERNEL_ERROR_END + 1,
};

/** Kiwi API error numbers. */
enum {
	/* FIXME: Rename this when a proper error is added. */
	ERR_DUMMY_ERROR = KIWI_ERROR_BASE,
};

/** Error class that can provide extra information about an error. */
class Error {
public:
	/** Construct the object.
	 * @param code		Error code for the object. */
	Error(int code = 0) : m_code(code) {}

	const Error &operator =(int code) { m_code = code; return *this; }
	const Error &operator =(const Error &error) { m_code = error.m_code; return *this; }
	bool operator ==(int code) const { return code == m_code; }
	bool operator ==(const Error &error) const { return error.m_code == m_code; }

	std::string getDescription() const;
	std::string getRecoverySuggestion() const;

	/** Get the error code of this object.
	 * @return		Object's error code. */
	int getCode() const { return m_code; }
private:
	int m_code;			/**< Error code for the error. */
};

};

#endif /* __KIWI_ERROR_H */
