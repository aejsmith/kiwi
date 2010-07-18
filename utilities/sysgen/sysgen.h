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
	Type(size_t w, size_t c, bool s) : width(w), count(c), is_signed(s) {}

	size_t width;			/**< Width of the type in bits. */
	size_t count;			/**< Number of parameters this type takes. */
	bool is_signed;			/**< Whether the type is signed. */
};

/** Type of the type map. */
typedef std::map<std::string, Type *> TypeMap;

/** Class representing a system call. */
class Syscall {
public:
	Syscall(const char *name, Type *ret, uint16_t service, uint16_t id);
	bool addParameter(Type *type);

	/** Get the name of the call.
	 * @return		Reference to call name. */
	const std::string &getName() const { return m_name; }

	/** Get the return value type of the call.
	 * @return		Return value type of the call. */
	Type *getReturnType() const { return m_return_type; }

	/** Get the ID of the call.
	 * @return		ID of the function. */
	uint32_t getID() const { return m_id; }

	/** Get the parameter count.
	 * @return		Number of parameters. */
	size_t getParameterCount() const { return m_param_count; }
private:
	std::string m_name;		/**< Name of the call. */
	Type *m_return_type;		/**< Return value type. */
	uint32_t m_id;			/**< ID of the call. */
	size_t m_param_count;		/**< Number of parameters. */
};

/** Type of a system call list. */
typedef std::list<Syscall *> SyscallList;

/** Base class for a code generation target. */
class Target {
public:
	virtual void addTypes(TypeMap &map) = 0;
	virtual size_t maxParameters() = 0;
	virtual void generate(std::ostream &stream, const SyscallList &calls) = 0;
};

extern "C" {
#endif

/** Structure used to represent a parameter during parsing. */
typedef struct parameter {
	struct parameter *next;		/**< Next parameter in the list. */
	char *type;			/**< Type name for the parameter. */
} parameter_t;

extern FILE *yyin;
extern const char *current_file;
extern size_t current_line;

extern parameter_t *new_parameter(const char *type, parameter_t *next);

extern void set_service_number(unsigned long num);
extern void add_type(const char *name, const char *target);
extern void add_syscall(const char *type, const char *name, parameter_t *params, int num);

extern int yylex(void);
extern int yyparse(void);

#ifdef __cplusplus
}
#endif

#endif /* __SYSGEN_H */
