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
 * @brief		Type management.
 */

#include <boost/foreach.hpp>
#include <iostream>
#include <typeinfo>
#include <ctype.h>

#include "Type.h"

using namespace std;

/** Dump information about the type. */
void Type::dump() const {
	const char *name = typeid(*this).name();
	while(isdigit(*name)) {
		name++;
	}
	cout << ' ' << m_name << ": " << name << endl;
}

/** Dump information about an integer type. */
void IntegerType::dump() const {
	cout << ' ' << m_name << ": IntegerType(" << m_width << ", " << m_is_signed << ')' << endl;
}

/** Construct an alias type.
 * @param name		Name of the type.
 * @param dest		Type that the alias refers to. */
AliasType::AliasType(const char *name, Type *dest) :
	Type(name)
{
	/* If the type is an alias, get the type it refers to. */
	AliasType *alias = dynamic_cast<AliasType *>(dest);
	if(alias != NULL) {
		m_dest = alias->resolve();
	} else {
		m_dest = dest;
	}
}

/** Dump information about an alias type. */
void AliasType::dump() const {
	cout << ' ' << m_name << ": AliasType(" << m_dest->getName() << ')' << endl;
}

/** Dump information about a structure. */
void StructType::dump() const {
	cout << ' ' << m_name << ": StructType" << endl;
	BOOST_FOREACH(const EntryList::value_type &ent, m_entries) {
		cout << "  " << ent.first->getName() << ' ' << ent.second << endl;
	}
}

/** Add an entry to a structure.
 * @param type		Type of entry to add.
 * @param name		Name of entry.
 * @return		True if added, false if entry with same name exists
 *			already. */
bool StructType::addEntry(Type *type, const char *name) {
	BOOST_FOREACH(EntryList::value_type &ent, m_entries) {
		if(ent.second == name) {
			return false;
		}
	}

	m_entries.push_back(std::make_pair(type, name));
	return true;
}
