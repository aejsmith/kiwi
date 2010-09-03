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

/** Construct a service.
 * @param name		Name of the service.
 * @param ver		Version of the service.
 * @param parent	Parent of the service. */
Service::Service(const char *name, unsigned long ver, Service *parent) :
	m_name(name), m_version((parent) ? parent->m_version : ver),
	m_parent(parent), m_next_id(1)
{
	/* Add built-in types. */
	AddType(new BytesType("bytes"));
	AddType(new BooleanType("bool"));
	AddType(new StringType("string"));
	AddType(new IntegerType("int8", 8, true));
	AddType(new IntegerType("int16", 16, true));
	AddType(new IntegerType("int32", 32, true));
	AddType(new IntegerType("int64", 64, true));
	AddType(new IntegerType("uint8", 8, false));
	AddType(new IntegerType("uint16", 16, false));
	AddType(new IntegerType("uint32", 32, false));
	AddType(new IntegerType("uint64", 64, false));
}

/** Dump the state of the service. */
void Service::Dump() const {
	cout << "Name: " << m_name << endl;
	cout << "Version: " << m_version << endl;
	cout << "Types:" << endl;
	BOOST_FOREACH(const TypeMap::value_type &type, m_types) {
		type.second->Dump();
	}
	cout << "Functions:" << endl;
	BOOST_FOREACH(const Function *func, m_functions) {
		func->Dump();
	}
	cout << "Events:" << endl;
	BOOST_FOREACH(const Function *event, m_events) {
		event->Dump();
	}
}

/** Get the full name of the service.
 * @return		Full name of the service. */
std::string Service::GetFullName() const {
	string str;
	if(m_parent) {
		str += m_parent->GetFullName();
		str += '.';
	}
	str += m_name;
	return str;
}

/** Split the service namespace into tokens.
 * @param tokens	Vector to place tokens into. */
void Service::TokeniseName(vector<string> &tokens) const {
	size_t last = 0, pos = m_name.find_first_of('.');
	while(pos != string::npos || last != string::npos) {
		tokens.push_back(m_name.substr(last, pos - last));
		last = m_name.find_first_not_of('.', pos);
		pos = m_name.find_first_of('.', last);
	}
}

/** Add a new type to a service.
 * @param type		Type to add.
 * @return		True if added, false if the name already exists. */
bool Service::AddType(Type *type) {
	if(NameExists(type->GetName())) {
		return false;
	}

	m_types[type->GetName()] = type;
	return true;
}

/** Look up a type in a service.
 * @param name		Name of the type to find.
 * @return		Pointer to type if found, NULL if not. */
Type *Service::GetType(const char *name) const {
	TypeMap::const_iterator it = m_types.find(name);
	if(it == m_types.end()) {
		/* Look up in the parent. */
		if(m_parent) {
			return m_parent->GetType(name);
		} else {
			return NULL;
		}
	}

	return it->second;
}

/** Add a child to the service.
 * @param service	Service to add.
 * @return		True if added, false if the name already exists. */
bool Service::AddChild(Service *service) {
	if(NameExists(service->GetName())) {
		return false;
	}

	m_children.push_back(service);
	m_names.insert(service->GetName());
	return true;
}

/** Check if a name exists in the service.
 * @param name		Name to check.
 * @return		Whether the name exists. */
bool Service::NameExists(const std::string &name) {
	return (m_names.find(name) != m_names.end());
}

/** Add a function to a function list.
 * @param func		Function to add.
 * @param list		List to add to.
 * @return		True if added, false if the name already exists. */
bool Service::AddFunctionToList(Function *func, FunctionList &list) {
	if(NameExists(func->GetName())) {
		return false;
	}

	func->SetMessageID(m_next_id++);
	list.push_back(func);
	m_names.insert(func->GetName());
	return true;
}
