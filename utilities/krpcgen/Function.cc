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
