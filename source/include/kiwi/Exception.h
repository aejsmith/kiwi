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
 * @brief		Kiwi library exceptions.
 *
 * Exceptions are used in the Kiwi API to handle programming errors, rather
 * than runtime errors. For runtime error handling, see Error.h.
 */

#ifndef __KIWI_EXCEPTION_H
#define __KIWI_EXCEPTION_H

#include <exception>
#include <string>

namespace kiwi {

/** Exception thrown when a class/function is used incorrectly. */
class Exception : public std::exception {
public:
	Exception(const std::string &msg);
	virtual ~Exception() throw();
	virtual const char *what() const throw();
protected:
	Exception() {};

	std::string m_string;		/**< String message for the exception. */
};

}

#endif /* __KIWI_EXCEPTION_H */
