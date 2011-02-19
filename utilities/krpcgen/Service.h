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
 * @brief		Service class.
 */

#ifndef __SERVICE_H
#define __SERVICE_H

#include <map>
#include <set>
#include <stdint.h>
#include <string>
#include <vector>

#include "Function.h"
#include "Type.h"

/** Class containing details of a service. */
class Service {
	/** Type of the name set. */
	typedef std::set<std::string> NameSet;
public:
	/** Type of the type list. */
	typedef std::list<Type *> TypeList;

	/** Type of the function list. */
	typedef std::list<Function *> FunctionList;

	/** Type of the child list. */
	typedef std::list<Service *> ChildList;

	Service(const char *name, unsigned long ver, Service *parent = 0);

	void Dump() const;
	std::string GetFullName() const;
	void TokeniseName(std::vector<std::string> &tokens) const;
	bool AddType(Type *type);
	Type *GetType(const char *name) const;
	bool AddChild(Service *service);

	/** Add a function to the service.
	 * @param func		Function to add.
	 * @return		True if added, false if name already exists. */
	bool AddFunction(Function *func) { return AddFunctionToList(func, m_functions); }

	/** Add an event to the service.
	 * @param event		Function to add.
	 * @return		True if added, false if name already exists. */
	bool AddEvent(Function *event) { return AddFunctionToList(event, m_events); }

	/** Get the name of the service.
	 * @return		Name of the service. */
	const std::string &GetName() const { return m_name; }

	/** Get the version of the service.
	 * @return		Version of the service. */
	unsigned long GetVersion() const { return m_version; }

	/** Get the parent of the service.
	 * @return		Pointer to parent. */
	Service *GetParent() const { return m_parent; }

	/** Get a reference to the type map.
	 * @return		Reference to type map. */
	const TypeList &GetTypes() const { return m_types; }

	/** Get a reference to the function list.
	 * @return		Reference to function list. */
	const FunctionList &GetFunctions() const { return m_functions; }

	/** Get a reference to the event list.
	 * @return		Reference to event list. */
	const FunctionList &GetEvents() const { return m_events; }

	/** Get a reference to the child list.
	 * @return		Reference to child list. */
	const ChildList &GetChildren() const { return m_children; }
private:
	bool NameExists(const std::string &name);
	bool AddFunctionToList(Function *func, FunctionList &list);

	std::string m_name;		/**< Name of the service. */
	unsigned long m_version;	/**< Service version number. */
	Service *m_parent;		/**< Parent service. */
	TypeList m_types;		/**< List of types. */
	FunctionList m_functions;	/**< List of functions. */
	FunctionList m_events;		/**< List of events. */
	uint32_t m_next_id;		/**< Next message ID. */
	ChildList m_children;		/**< Child services. */
	NameSet m_names;		/**< Set of all names. */
};

#endif /* __SERVICE_H */
