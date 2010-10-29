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
 * @brief		Error class.
 *
 * This file defines a class used to report errors from API functions. It is
 * a wrapper around status_t which allows information to be obtained about an
 * error, such as a human-readable error description and suggestions for
 * recovering from an error.
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

#ifndef __KIWI_ERROR_H
#define __KIWI_ERROR_H

#include <kiwi/CoreDefs.h>

KIWI_BEGIN_NAMESPACE

/** Class providing information on an error. */
class Error {
public:
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

KIWI_END_NAMESPACE

#endif /* __KIWI_ERROR_H */
