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
 * @brief		Service class.
 */

#include <boost/foreach.hpp>
#include <iostream>
#include <utility>

#include "Service.h"

using namespace std;

/** Construct a service. */
Service::Service() :
	m_version(0), m_next_id(1)
{
	/* Add built-in types. */
	addType(new BytesType("bytes"));
	addType(new BooleanType("bool"));
	addType(new StringType("string"));
	addType(new IntegerType("int8", 8, true));
	addType(new IntegerType("int16", 16, true));
	addType(new IntegerType("int32", 32, true));
	addType(new IntegerType("int64", 64, true));
	addType(new IntegerType("uint8", 8, false));
	addType(new IntegerType("uint16", 16, false));
	addType(new IntegerType("uint32", 32, false));
	addType(new IntegerType("uint64", 64, false));
}

/** Dump the state of the service. */
void Service::dump() const {
	cout << "Name: " << m_name << endl;
	cout << "Version: " << m_version << endl;
	cout << "Types:" << endl;
	BOOST_FOREACH(const TypeMap::value_type &type, m_types) {
		type.second->dump();
	}
	cout << "Functions:" << endl;
	BOOST_FOREACH(const Function *func, m_functions) {
		func->dump();
	}
	cout << "Events:" << endl;
	BOOST_FOREACH(const Function *event, m_events) {
		event->dump();
	}
}

/** Split the service namespace into tokens.
 * @param tokens	Vector to place tokens into. */
void Service::tokeniseName(vector<string> &tokens) const {
	size_t last = 0, pos = m_name.find_first_of('.');
	while(pos != string::npos || last != string::npos) {
		tokens.push_back(m_name.substr(last, pos - last));
		last = m_name.find_first_not_of('.', pos);
		pos = m_name.find_first_of('.', last);
	}
}

/** Set the name of the service.
 * @param name		Name to set.
 * @return		True if OK, false if name was already set. */
bool Service::setName(const char *name) {
	if(m_name.length() > 0) {
		return false;
	}

	m_name = name;
	return true;
}

/** Set the version of the service.
 * @param ver		Version to set.
 * @return		True if OK, false if version was already set. */
bool Service::setVersion(unsigned long ver) {
	if(m_version > 0) {
		return false;
	}

	m_version = ver;
	return true;
}

/** Add a new type to a service.
 * @param type		Type to add.
 * @return		True if added, false if there is already a type with
 *			the same name. */
bool Service::addType(Type *type) {
	if(getType(type->getName().c_str())) {
		return false;
	}

	m_types[type->getName()] = type;
	return true;
}

/** Look up a type in a service.
 * @param name		Name of the type to find.
 * @return		Pointer to type if found, NULL if not. */
Type *Service::getType(const char *name) const {
	TypeMap::const_iterator it = m_types.find(name);
	if(it == m_types.end()) {
		return NULL;
	}

	return it->second;
}

/** Add a function to a function list.
 * @param func		Function to add.
 * @param list		List to add to.
 * @return		True if added, false if function with same name already
 *			exists. */
bool Service::addFunctionToList(Function *func, FunctionList &list) {
	NameMap::iterator it = m_func_names.find(func->getName());
	if(it != m_func_names.end()) {
		return false;
	}

	func->setMessageID(m_next_id++);
	list.push_back(func);
	m_func_names.insert(make_pair(func->getName(), func));
	return true;
}
