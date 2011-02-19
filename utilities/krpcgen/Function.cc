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
 * @brief		Function class.
 */

#include <boost/foreach.hpp>
#include <iostream>
#include "Function.h"

using namespace std;

/** Dump information about the function. */
void Function::Dump() const {
	cout << ' ' << m_name << '(';
	for(ParameterList::const_iterator it = m_params.begin(); it != m_params.end(); ) {
		if(it->out) {
			cout << "out ";
		}
		cout << it->type->GetName() << ' ' << it->name;
		if(++it != m_params.end()) {
			cout << ", ";
		}
	}

	cout << ')' << endl;
}

/** Add a parameter to a function.
 * @param type		Type of the parameter.
 * @param name		Name of the parameter.
 * @param out		Whether the parameter is for output.
 * @return		True if added, false if parameter already exists with
 *			the same name. */
bool Function::AddParameter(Type *type, const char *name, bool out) {
	BOOST_FOREACH(const Parameter &param, m_params) {
		if(param.name == name) {
			return false;
		}
	}

	Parameter p = { type, name, out };
	m_params.push_back(p);
	return true;
}
