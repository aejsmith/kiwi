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

#ifndef __SERVICE_H
#define __SERVICE_H

#include <map>
#include <stdint.h>
#include <string>
#include <vector>

#include "Function.h"
#include "Type.h"

/** Class containing details of a service. */
class Service {
	/** Type of the name map. */
	typedef std::map<std::string, Function *> NameMap;
public:
	/** Type of the type map. */
	typedef std::map<std::string, Type *> TypeMap;

	/** Type of the function list. */
	typedef std::list<Function *> FunctionList;

	Service();

	void Dump() const;
	void TokeniseName(std::vector<std::string> &tokens) const;
	bool SetName(const char *name);
	bool SetVersion(unsigned long ver);
	bool AddType(Type *type);
	Type *GetType(const char *name) const;

	/** Add a function to the service.
	 * @param func		Function to add.
	 * @return		True if added, false if function with same name
	 *			already exists. */
	bool AddFunction(Function *func) { return AddFunctionToList(func, m_functions); }

	/** Add an event to the service.
	 * @param event		Function to add.
	 * @return		True if added, false if event with same name
	 *			already exists. */
	bool AddEvent(Function *event) { return AddFunctionToList(event, m_events); }

	/** Get the name of the service.
	 * @return		Name of the service. */
	const std::string &GetName() const { return m_name; }

	/** Get the version of the service.
	 * @return		Version of the service. */
	unsigned long GetVersion() const { return m_version; }

	/** Get a reference to the type map.
	 * @return		Reference to type map. */
	const TypeMap &GetTypes() const { return m_types; }

	/** Get a reference to the function list.
	 * @return		Reference to function list. */
	const FunctionList &GetFunctions() const { return m_functions; }

	/** Get a reference to the event list.
	 * @return		Reference to event list. */
	const FunctionList &GetEvents() const { return m_events; }
private:
	bool AddFunctionToList(Function *func, FunctionList &list);

	std::string m_name;		/**< Name of the service. */
	unsigned long m_version;	/**< Service version number. */
	TypeMap m_types;		/**< Map of type names to types. */
	FunctionList m_functions;	/**< List of functions. */
	FunctionList m_events;		/**< List of events. */
	NameMap m_func_names;		/**< Map of names to functions/events. */
	uint32_t m_next_id;		/**< Next message ID. */
};

#endif /* __SERVICE_H */
