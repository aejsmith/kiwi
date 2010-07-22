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
 */

#include <kiwi/Exception.h>
#include <cstring>
#include <sstream>

using namespace kiwi;
using namespace std;

/** Construct an Exception object. */
Exception::Exception(const string &msg) : m_string(msg) {}

/** Destroy an Exception object. */
Exception::~Exception() throw() {}

/** Return the string associated with the exception.
 * @return		String associated with the exception. */
const char *Exception::what() const throw() {
	return m_string.c_str();
}
