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
 * @todo		When we support locales, strings returned should be
 *			localised.
 * @todo		Function to get a list of recovery actions (e.g. Try
 *			Again).
 * @todo		A display error function to display a notification
 *			window for the error with buttons for each recovery
 *			action.
 * @todo		Include a function name parameter to OSError to report
 *			the function the error occurred in?
 */

#include <kiwi/Error.h>

using namespace kiwi;

/** Get the string description of the error.
 * @return		Localised string describing the error that occurred. */
const char *Error::GetDescription() const throw() {
	return "Unknown error";
}

/** Get a recovery suggestion for the error.
 * @return		Localised string suggesting a recovery action for the
 *			error. If no suggestion is available, an empty string
 *			will be returned. */
const char *Error::GetRecoverySuggestion() const throw() {
	return "";
}

/** Get the error description (function for std::exception compatibility).
 * @return		String containing a description of the error. */
const char *Error::what() const throw() {
	return GetDescription();
}

/** Get the string description of the error.
 * @return		Localised string describing the error that occurred. */
const char *OSError::GetDescription() const throw() {
	if(m_code < 0 || static_cast<size_t>(m_code) >= __kernel_status_size) {
		return Error::GetDescription();
	} else if(!__kernel_status_strings[m_code]) {
		return Error::GetDescription();
	}
	return __kernel_status_strings[m_code];
}

/** Get a recovery suggestion for the error.
 * @return		Localised string suggesting a recovery action for the
 *			error. If no suggestion is available, an empty string
 *			will be returned. */
const char *OSError::GetRecoverySuggestion() const throw() {
	/* TODO. */
	return "";
}
