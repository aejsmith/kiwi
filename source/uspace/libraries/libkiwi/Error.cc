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
#include <cstring>

/** Get the number of elements in an array. */
#define ARRAYSZ(a)	(sizeof((a)) / sizeof((a)[0]))

using namespace kiwi;
using namespace std;

/** Array of Kiwi API error descriptions. */
static const char *kiwi_error_descriptions[] = {
	"Dummy error.",
};

/** Get the string description of the error.
 * @return		String containing a description of the error. */
string Error::getDescription() const {
	if(m_code < KIWI_ERROR_BASE) {
		return strerror(m_code);
	}

	size_t code = m_code - KIWI_ERROR_BASE;
	if(code >= ARRAYSZ(kiwi_error_descriptions)) {
		return "Unknown error";
	} else {
		return kiwi_error_descriptions[code];
	}
}

/** Get a recovery suggestion for the error.
 * @return		String containing a recovery suggestion for the error.
 *			If no suggestion is available, an empty string will be
 *			returned. */
string Error::getRecoverySuggestion() const {
	/* TODO. */
	return string();
}
