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
 * @brief		Kiwi system call code generator.
 */

#ifndef __SYSGEN_H
#define __SYSGEN_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
#include <iostream>
#include <list>
#include <map>
#include <string>

/** Structure representing a type. */
struct Type {
	Type(size_t c = 0) : count(c) {}
	size_t count;			/**< Number of parameters this type takes. */
};

/** Type of the type map. */
typedef std::map<std::string, Type> TypeMap;

/** Class representing a system call. */
class Syscall {
public:
	/** Attributes for a call. */
	enum {
		kHiddenAttribute = (1<<0),
		kWrappedAttribute = (1<<1),
	};

	Syscall(const char *name, unsigned long id) :
		m_name(name), m_id(id), m_param_count(0), m_attributes(0)
	{}

	/** Add a parameter to the call.
	 * @param type		Type of the call. */
	void AddParameter(Type type) {
		m_param_count += type.count;
	}

	bool SetAttribute(const char *name);

	/** Get the name of the call.
	 * @return		Reference to call name. */
	const std::string &GetName() const { return m_name; }

	/** Get the ID of the call.
	 * @return		ID of the function. */
	unsigned long GetID() const { return m_id; }

	/** Get the parameter count.
	 * @return		Number of parameters. */
	size_t GetParameterCount() const { return m_param_count; }

	/** Get the call attributes.
	 * @return		Attributes for the call. */
	int GetAttributes() const { return m_attributes; }
private:
	std::string m_name;		/**< Name of the call. */
	unsigned long m_id;		/**< ID of the call. */
	size_t m_param_count;		/**< Number of parameters. */
	int m_attributes;		/**< Attributes for the call. */
};

/** Type of a system call list. */
typedef std::list<Syscall *> SyscallList;

/** Base class for a code generation target. */
class Target {
public:
	virtual ~Target() {}

	/** Add the target's basic types to the type map.
	 * @param map		Map to add to. */
	virtual void AddTypes(TypeMap &map) = 0;

	/** Generate system call functions.
	 * @param stream	Stream to write to.
	 * @param calls		List of calls to generate. */
	virtual void Generate(std::ostream &stream, const SyscallList &calls) = 0;
};

extern "C" {
#endif

/** Structure used to represent an identifier during parsing. */
typedef struct identifier {
	struct identifier *next;	/**< Next identifier in the list. */
	char *str;			/**< Identifier. */
} identifier_t;

extern FILE *yyin;
extern const char *current_file;
extern size_t current_line;

extern identifier_t *new_identifier(const char *str, identifier_t *next);
extern void add_type(const char *name, const char *target);
extern void add_syscall(const char *name, identifier_t *params, identifier_t *attribs, long num);

extern int yylex(void);
extern int yyparse(void);

#ifdef __cplusplus
}
#endif

#endif /* __SYSGEN_H */
