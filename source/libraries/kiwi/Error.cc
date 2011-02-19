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
 * @brief		Error handling classes.
 *
 * @todo		When we support locales, strings returned should be
 *			localised.
 * @todo		Function to get a list of recovery actions (e.g. Try
 *			Again).
 * @todo		A display error function to display a notification
 *			window for the error with buttons for each recovery
 *			action.
 */

#include <kiwi/Error.h>

using namespace kiwi;

/** Get a description of the error.
 * @return		String error description. */
const char *BaseError::what() const throw() {
	return GetDescription();
}

/** Get a recovery suggestion for the error.
 * @return		Localised string suggesting a recovery action for the
 *			error. If no suggestion is available, an empty string
 *			will be returned. */
const char *BaseError::GetRecoverySuggestion() const throw() {
	return "";
}

/** Get the string description of the error.
 * @return		Localised string describing the error that occurred. */
const char *Error::GetDescription() const throw() {
	if(m_code < 0 || static_cast<size_t>(m_code) >= __kernel_status_size) {
		return "Unknown error";
	} else if(!__kernel_status_strings[m_code]) {
		return "Unknown error";
	}
	return __kernel_status_strings[m_code];
}

/** Get a recovery suggestion for the error.
 * @return		Localised string suggesting a recovery action for the
 *			error. If no suggestion is available, an empty string
 *			will be returned. */
const char *Error::GetRecoverySuggestion() const throw() {
	/* TODO. */
	return "";
}
