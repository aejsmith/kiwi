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

#ifndef __FUNCTION_H
#define __FUNCTION_H

#include <list>
#include <stdint.h>
#include <string>

#include "Type.h"

/** Class representing a function/event. */
class Function {
public:
	/** Structure containing details of a parameter. */
	struct Parameter {
		Type *type;		/**< Type of the parameter. */
		std::string name;	/**< Name of the parameter. */
		bool out;		/**< Whether is an output parameter. */
	};

	/** Type of the parameter list. */
	typedef std::list<Parameter> ParameterList;

	/** Construct the function.
	 * @param name		Name of the function.  */
	Function(const char *name) : m_name(name) {}

	void Dump() const;
	bool AddParameter(Type *type, const char *name, bool out);

	/** Get the name of the function.
	 * @return		Reference to type name. */
	const std::string &GetName() const { return m_name; }

	/** Get the message ID of the function.
	 * @return		Message ID of the function. */
	uint32_t GetMessageID() const { return m_id; }

	/** Set the message ID of the function.
	 * @param id		ID to set. */
	void SetMessageID(uint32_t id) { m_id = id; }

	/** Get the argument list.
	 * @return		Reference to argument list. */
	const ParameterList &GetParameters() const { return m_params; }
private:
	std::string m_name;		/**< Name of the function. */
	uint32_t m_id;			/**< Message ID of the function. */
	ParameterList m_params;		/**< List of parameters. */
};

#endif /* __FUNCTION_H */
