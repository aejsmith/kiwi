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
 * @brief		Error class.
 */

#ifndef __KIWI_ERROR_H
#define __KIWI_ERROR_H

#include <kernel/status.h>
#include <kiwi/CoreDefs.h>

#include <exception>

namespace kiwi {

/** Base class for errors. */
class KIWI_PUBLIC BaseError : public std::exception {
public:
	virtual const char *GetDescription() const throw() = 0;
	virtual const char *GetRecoverySuggestion() const throw();
private:
	const char *what() const throw();
};

/** Class providing information on an error.
 *
 * This class is used to report errors from API functions. It is a wrapper
 * around status_t which allows information to be obtained about an error, such
 * as a human-readable error description and suggestions for recovering from an
 * error.
 *
 * The suggested method for using this class in classes not designed to be used
 * from multiple threads simultaneously is to return a bool stating whether or
 * not the function succeeded, and to have a GetError function that returns a
 * reference to an error object giving details of the error. The suggested
 * method for using this class in classes designed to be used from multiple
 * threads simultaneously is to return a bool stating whether or not the
 * function succeeded, and take an optional pointer to an Error object in which
 * error information will be stored.
 */
class KIWI_PUBLIC Error : public BaseError {
public:
	Error() throw() : m_code(STATUS_SUCCESS) {}
	Error(status_t code) throw() : m_code(code) {}

	bool operator ==(Error &other) const throw() { return other.m_code == m_code; }
	bool operator ==(status_t code) const throw() { return code == m_code; }

	/** Get the status code.
	 * @return		Code describing the error. */
	status_t GetCode() const throw() { return m_code; }

	virtual const char *GetDescription() const throw();
	virtual const char *GetRecoverySuggestion() const throw();
private:
	status_t m_code;		/**< Status code. */
};

}

#endif /* __KIWI_ERROR_H */
