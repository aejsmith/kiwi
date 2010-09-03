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
 * @brief		Kiwi RPC interface compiler.
 */

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "CXXCodeGen.h"
#include "Service.h"
#include "krpcgen.h"

using namespace std;

/** Information on the file currently being parsed. */
const char *current_file = NULL;
size_t current_line = 1;
static bool had_error = false;

/** Service currently being compiled. */
static Service *current_service = NULL;

/** Whether to print debug messages. */
static bool verbose_mode = false;

/** Macro to print details of an error on a specific line.
 * @param l		Line the error was on.
 * @param e		Error to print. */
#define COMPILE_ERROR(l, e)	\
	{ \
		cerr << current_file << ':' << l << ": " << e << endl; \
		had_error = true; \
	}

/** Macro to print details of an error in a statement.
 * @param s		Statement the error was in.
 * @param e		Error to print. */
#define STATEMENT_ERROR(s, e)	COMPILE_ERROR((s)->line, e)

/** Macro to print a debug message if in verbose mode.
 * @param m		Message to print. */
#define DEBUG(m)		\
	if(verbose_mode) { \
		cout << current_file << ':' << current_line << ": " << m << endl; \
	}

/** Create a new variable structure.
 * @param name		Name of variable.
 * @param type		Type of variable.
 * @param out		Whether the variable is an output variable.
 * @return		Pointer to the variable structure. */
variable_t *new_variable(const char *name, const char *type, bool out) {
	DEBUG("new_variable(" << name << ", " << type << ", " << out << ")");

	variable_t *variable = new variable_t;
	variable->next = NULL;
	variable->name = strdup(name);
	variable->type = strdup(type);
	variable->out = out;
	variable->line = current_line;
	return variable;
}

/** Create a new service statement structure.
 * @param name		Name of the service.
 * @param ver		Version number.
 * @param stmts		List of statements in the service.
 * @return		Pointer to statement structure. */
statement_t *new_service_stmt(const char *name, unsigned long ver, statement_t *stmts) {
	DEBUG("new_service_stmt(" << name << ")");

	statement_t *stmt = new statement_t;
	stmt->next = NULL;
	stmt->line = current_line;
	stmt->type = statement_t::STATEMENT_SERVICE;
	stmt->data.service.name = strdup(name);
	stmt->data.service.version = ver;
	stmt->data.service.stmts = stmts;
	return stmt;
}

/** Create a new type statement structure.
 * @param name		Name of the type.
 * @param target	Name of the target type.
 * @return		Pointer to statement structure. */
statement_t *new_type_stmt(const char *name, const char *target) {
	DEBUG("new_type_stmt(" << name << ", " << target << ")");

	statement_t *stmt = new statement_t;
	stmt->next = NULL;
	stmt->line = current_line;
	stmt->type = statement_t::STATEMENT_TYPE;
	stmt->data.type.name = strdup(name);
	stmt->data.type.target = strdup(target);
	return stmt;
}

/** Create a new struct statement structure.
 * @param name		Name of the struct.
 * @param entries	List of entries in the struct.
 * @return		Pointer to statement structure. */
statement_t *new_struct_stmt(const char *name, variable_t *entries) {
	DEBUG("new_struct_stmt(" << name << ")");

	statement_t *stmt = new statement_t;
	stmt->next = NULL;
	stmt->line = current_line;
	stmt->type = statement_t::STATEMENT_STRUCT;
	stmt->data.struc.name = strdup(name);
	stmt->data.struc.entries = entries;
	return stmt;
}

/** Create a new function statement structure.
 * @param name		Name of the service.
 * @param params	List of parameters to the function.
 * @return		Pointer to statement structure. */
statement_t *new_function_stmt(const char *name, variable_t *params) {
	DEBUG("new_function_stmt(" << name << ")");

	statement_t *stmt = new statement_t;
	stmt->next = NULL;
	stmt->line = current_line;
	stmt->type = statement_t::STATEMENT_FUNCTION;
	stmt->data.function.name = strdup(name);
	stmt->data.function.params = params;
	return stmt;
}

/** Create a new event statement structure.
 * @param name		Name of the service.
 * @param params	List of parameters to the event.
 * @return		Pointer to statement structure. */
statement_t *new_event_stmt(const char *name, variable_t *params) {
	DEBUG("new_event_stmt(" << name << ")");

	statement_t *stmt = new statement_t;
	stmt->next = NULL;
	stmt->line = current_line;
	stmt->type = statement_t::STATEMENT_EVENT;
	stmt->data.function.name = strdup(name);
	stmt->data.function.params = params;
	return stmt;
}

/** Add a new type alias.
 * @param stmt		Statement to process.
 * @param service	Service to add to. */
static void process_type_stmt(statement_t *stmt, Service *service) {
	Type *dest = service->GetType(stmt->data.type.target);
	if(!dest) {
		STATEMENT_ERROR(stmt, "Alias target `" << stmt->data.type.target << "' does not exist.");
		return;
	}

	Type *alias = new AliasType(stmt->data.type.name, dest);
	if(!service->AddType(alias)) {
		STATEMENT_ERROR(stmt, "Name `" << stmt->data.type.name << "' already exists.");
		delete alias;
	}
}

/** Add a new structure.
 * @param stmt		Statement to process.
 * @param service	Service to add to. */
static void process_struct_stmt(statement_t *stmt, Service *service) {
	StructType *struc = new StructType(stmt->data.struc.name);
	variable_t *entry = stmt->data.struc.entries;
	while(entry) {
		Type *type = service->GetType(entry->type);
		if(!type) {
			COMPILE_ERROR(entry->line, "Entry type `" << entry->type << "' does not exist.");
			delete struc;
			return;
		} else if(!struc->AddEntry(type, entry->name)) {
			COMPILE_ERROR(entry->line, "Duplicate struct entry name `" << entry->name << "'.");
			delete struc;
			return;
		}

		entry = entry->next;
	}

	if(!service->AddType(struc)) {
		STATEMENT_ERROR(stmt, "Name `" << stmt->data.struc.name << "' already exists.");
		delete struc;
	}
}

/** Add a new function.
 * @param stmt		Statement to process.
 * @param service	Service to add to. */
static void process_function_stmt(statement_t *stmt, Service *service) {
	Function *func = new Function(stmt->data.function.name);
	variable_t *param = stmt->data.function.params;
	while(param) {
		Type *type = service->GetType(param->type);
		if(!type) {
			COMPILE_ERROR(param->line, "Parameter type `" << param->type << "' does not exist.");
			delete func;
			return;
		} else if(!func->AddParameter(type, param->name, param->out)) {
			COMPILE_ERROR(param->line, "Duplicate parameter name `" << param->name << "'.");
			delete func;
			return;
		}

		param = param->next;
	}

	bool ret;
	if(stmt->type == statement_t::STATEMENT_FUNCTION) {
		ret = service->AddFunction(func);
	} else {
		ret = service->AddEvent(func);
	}
	if(!ret) {
		STATEMENT_ERROR(stmt, "Name `" << stmt->data.function.name << "' already exists.");
		delete func;
	}
}

/** Process a service statement.
 * @param stmt		Statement to process.
 * @param parent	Service to add to.
 * @return		Pointer to service. */
static Service *process_service_stmt(statement_t *stmt, Service *parent) {
	if(!parent && stmt->data.service.version == 0) {
		STATEMENT_ERROR(stmt, "Version number must be greater than 0.");
	}

	/* Create a service structure for the new service. */
	Service *service = new Service(stmt->data.service.name, stmt->data.service.version, parent);

	/* Process all statements in the service. */
	statement_t *entry = stmt->data.service.stmts;
	while(entry) {
		switch(entry->type) {
		case statement_t::STATEMENT_SERVICE:
			process_service_stmt(entry, service);
			break;
		case statement_t::STATEMENT_TYPE:
			process_type_stmt(entry, service);
			break;
		case statement_t::STATEMENT_STRUCT:
			process_struct_stmt(entry, service);
			break;
		case statement_t::STATEMENT_FUNCTION:
			process_function_stmt(entry, service);
			break;
		case statement_t::STATEMENT_EVENT:
			process_function_stmt(entry, service);
			break;
		}

		entry = entry->next;
	}

	if(service->GetFunctions().empty() && service->GetEvents().empty()) {
		STATEMENT_ERROR(stmt, "Service must have at least 1 function/event.");
	}

	/* Add the service to the parent. */
	if(parent) {
		if(!parent->AddChild(service)) {
			STATEMENT_ERROR(stmt, "Name `" << stmt->data.service.name << "' already exists.");
		}
	}

	return service;
}

/** Set the top level service.
 * @param stmt		Service statement. */
void set_service(statement_t *stmt) {
	DEBUG("set_service()");

	/* Create a service structure for the new service. */
	assert(!current_service);
	assert(stmt->type == statement_t::STATEMENT_SERVICE);
	current_service = process_service_stmt(stmt, 0);
}

/** Print usage information and exit.
 * @param stream	Stream to output to.
 * @param progname	Program name. */
static void usage(ostream &stream, const char *progname) {
	stream << "Usage: " << progname << " [-t <target>] (-s <file>|-c <file>) <input file>" << endl;
	stream << "Options:" << endl;
	stream << " -t <target> - Specify target to generate code for (cxx, kernel)." << endl;
	stream << " -s <file>   - Generate server code." << endl;
	stream << " -c <file>   - Generate client code." << endl;
	stream << "At least one of -s or -c must be specified." << endl;

	exit((stream == cerr) ? 1 : 0);
}

/** Main entry point for the program.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		0 on success, 1 on failure. */
int main(int argc, char **argv) {
	if(argc < 2) {
		usage(cerr, argv[0]);
	}

	/* Parse the command line arguments. */
	string client, server, target;
	for(int i = 1; i < argc; i++) {
		if(strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
			usage(cout, argv[0]);
			return 0;
		} else if(strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
			verbose_mode = true;
		} else if(strcmp(argv[i], "-t") == 0) {
			if(++i == argc || argv[i][0] == 0) {
				cerr << "Option '-t' requires an argument." << endl;
				usage(cerr, argv[0]);
			}
			target = argv[i];
		} else if(strcmp(argv[i], "-s") == 0) {
			if(++i == argc || argv[i][0] == 0) {
				cerr << "Option '-s' requires an argument." << endl;
				usage(cerr, argv[0]);
			}
			server = argv[i];
		} else if(strcmp(argv[i], "-c") == 0) {
			if(++i == argc || argv[i][0] == 0) {
				cerr << "Option '-c' requires an argument." << endl;
				usage(cerr, argv[0]);
			}
			client = argv[i];
		} else if(argv[i][0] == '-') {
			cerr << "Unrecognised argument '" << argv[i] << '\'' << endl;
			usage(cerr, argv[0]);
		} else {
			if(current_file != NULL) {
				cerr << "Can only specify one input file." << endl;
				usage(cerr, argv[0]);
			}
			current_file = argv[i];
		}
	}

	/* Validate arguments and set defaults if not set. */
	if(current_file == NULL) {
		cerr << "No input file specified." << endl;
		usage(cerr, argv[0]);
	}
	if(server.length() == 0 && client.length() == 0) {
		cerr << "No output files specified." << endl;
		usage(cerr, argv[0]);
	}
	if(target.length() == 0) {
		target = "cxx";
	}

	/* Parse the input file. */		
	yyin = fopen(current_file, "r");
	if(yyin == NULL) {
		perror(current_file);
		return 1;
	}
	yyparse();
	fclose(yyin);

	/* Check for errors. */
	if(had_error) {
		cerr << "Aborting compilation due to errors." << endl;
		return 1;
	}

	/* Dump the service if in verbose mode. */
	if(verbose_mode) {
		current_service->Dump();
	}

	/* Determine which code generator to use. */
	CodeGen *cg;
	if(target == "cxx") {
		cg = new CXXCodeGen();
	} else {
		cerr << "Unrecognised target `" << target << "'." << endl;
		return 1;
	}

	/* Generate the code. */
	if(server.length() > 0) {
		if(!cg->GenerateServer(current_service, server)) {
			return 1;
		}
	}
	if(client.length() > 0) {
		if(!cg->GenerateClient(current_service, client)) {
			return 1;
		}
	}
	return 0;
}
