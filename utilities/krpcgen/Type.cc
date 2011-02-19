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
 * @brief		Type management.
 */

#include <boost/foreach.hpp>
#include <iostream>
#include <typeinfo>
#include <ctype.h>

#include "Type.h"

using namespace std;

/** Dump information about the type. */
void Type::Dump() const {
	const char *name = typeid(*this).name();
	while(isdigit(*name)) {
		name++;
	}
	cout << ' ' << m_name << ": " << name << endl;
}

/** Dump information about an integer type. */
void IntegerType::Dump() const {
	cout << ' ' << m_name << ": IntegerType(" << m_width << ", " << m_is_signed << ')' << endl;
}

/** Construct an alias type.
 * @param name		Name of the type.
 * @param dest		Type that the alias refers to. */
AliasType::AliasType(const char *name, Type *dest) :
	Type(name)
{
	/* If the type is an alias, get the type it refers to. */
	if(AliasType *alias = dynamic_cast<AliasType *>(dest)) {
		m_dest = alias->Resolve();
	} else {
		m_dest = dest;
	}
}

/** Dump information about an alias type. */
void AliasType::Dump() const {
	cout << ' ' << m_name << ": AliasType(" << m_dest->GetName() << ')' << endl;
}

/** Dump information about a structure. */
void StructType::Dump() const {
	cout << ' ' << m_name << ": StructType" << endl;
	BOOST_FOREACH(const EntryList::value_type &ent, m_entries) {
		cout << "  " << ent.first->GetName() << ' ' << ent.second << endl;
	}
}

/** Add an entry to a structure.
 * @param type		Type of entry to add.
 * @param name		Name of entry.
 * @return		True if added, false if entry with same name exists
 *			already. */
bool StructType::AddEntry(Type *type, const char *name) {
	BOOST_FOREACH(EntryList::value_type &ent, m_entries) {
		if(ent.second == name) {
			return false;
		}
	}

	m_entries.push_back(make_pair(type, name));
	return true;
}
