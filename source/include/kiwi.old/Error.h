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
 * @brief		Error handling classes.
 *
 * This file defines exception classes for reporting errors from API functions.
 * Exceptions are used to report runtime errors, as opposed to programming
 * errors (e.g. invalid argument), which are reported via assertions.
 *
 * All exceptions derive from the base Error class, which provides functions to
 * get information about the error, as well as convenience functions for
 * reporting the error to the user.
 *
 * The OSError class wraps kernel status codes. There are several classes
 * derived from this which allow the type of operation that caused the error to
 * be determined if necessary (e.g. IPCError is thrown from IPC-related
 * functions). Some of these derived classes provide extra information about
 * an error, for example ProcessError can get the name of missing libraries and
 * symbols that were discovered during process creation.
 */

#ifndef __KIWI_ERROR_H
#define __KIWI_ERROR_H

#include <kernel/status.h>
#include <kernel/types.h>

#include <exception>

namespace kiwi {

/** Base class for all Kiwi API exceptions. */
class Error : public std::exception {
public:
	virtual const char *GetDescription() const throw();
	virtual const char *GetRecoverySuggestion() const throw();
	const char *what() const throw();
};

/** Exception representing errors raised by the operating system.
 * @note		Some parts of the API may throw exceptions derived from
 *			this. For example, Process throws ProcessError which
 *			provides extra information such as the name of missing
 *			libraries/symbols for certain errors. When you do not
 *			require such information, you should catch this, or if
 *			you do not care about the status code, catch Error. */
class OSError : public Error {
public:
	OSError(status_t code) : m_code(code) {}

	bool operator ==(status_t code) const { return code == m_code; }

	/** Get the status code.
	 * @return		Code describing the error. */
	status_t GetCode() const { return m_code; }

	virtual const char *GetDescription() const throw();
	virtual const char *GetRecoverySuggestion() const throw();
private:
	status_t m_code;		/**< Status code. */
};

}

#endif /* __KIWI_ERROR_H */
