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

/** Macro to print details of an error.
 * @param e		Error to print. */
#define COMPILE_ERROR(e)	\
	{ \
		cerr << current_file << ':' << current_line << ": " << e << endl; \
		had_error = true; \
	}

/** Macro to print details of an error on a specific line.
 * @param l		Line the error was on.
 * @param e		Error to print. */
#define COMPILE_ERROR_AT(l, e)	\
	{ \
		cerr << current_file << ':' << l << ": " << e << endl; \
		had_error = true; \
	}

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

/** Set the name of the service.
 * @param name		Name to set. */
void set_service_name(const char *name) {
	DEBUG("set_service_name(" << name << ")");

	if(!current_service->setName(name)) {
		COMPILE_ERROR("Service name has already been set.");
	}
}

/** Set the version of the service.
 * @param ver		Version to set. */
void set_service_version(unsigned long ver) {
	DEBUG("set_service_version(" << ver << ")");

	if(ver == 0) {
		COMPILE_ERROR("Service version must be greater than 0.");
	} else if(!current_service->setVersion(ver)) {
		COMPILE_ERROR("Service version has already been set.");
	}
}

/** Add a new type alias.
 * @param name		Name of the alias.
 * @param target	Target type for the alias. */
void add_type(const char *name, const char *target) {
	DEBUG("add_type(" << name << ", " << target << ")");

	Type *dest = current_service->getType(target);
	if(!dest) {
		COMPILE_ERROR("Alias target `" << target << "' does not exist.");
		return;
	}

	Type *alias = new AliasType(name, dest);
	if(!current_service->addType(alias)) {
		COMPILE_ERROR("Type `" << name << "' already exists.");
		delete alias;
	}
}

/** Add a new structure.
 * @param name		Name to give structure.
 * @param entries	Entries for the structure. */
void add_struct(const char *name, variable_t *entries) {
	DEBUG("add_struct(" << name << ")");

	StructType *str = new StructType(name);
	while(entries) {
		Type *type = current_service->getType(entries->type);
		if(!type) {
			COMPILE_ERROR_AT(entries->line, "Entry type `" << entries->type << "' does not exist.");
			delete str;
			return;
		} else if(!str->addEntry(type, entries->name)) {
			COMPILE_ERROR_AT(entries->line, "Duplicate entry name `" << entries->name << "'.");
			delete str;
			return;
		}

		entries = entries->next;
	}

	if(!current_service->addType(str)) {
		COMPILE_ERROR("Type `" << name << "' already exists.");
		delete str;
	}
}

/** Add a new function.
 * @param name		Name of the function.
 * @param params	Parameters for the function. */
void add_function(const char *name, variable_t *params) {
	DEBUG("add_function(" << name << ")");

	Function *func = new Function(name);
	while(params) {
		Type *type = current_service->getType(params->type);
		if(!type) {
			COMPILE_ERROR_AT(params->line, "Parameter type `" << params->type << "' does not exist.");
			return;
		} else if(!func->addParameter(type, params->name, params->out)) {
			COMPILE_ERROR_AT(params->line, "Duplicate parameter name `" << params->name << "'.");
			return;
		}

		params = params->next;
	}

	if(!current_service->addFunction(func)) {
		COMPILE_ERROR("Duplicate function/event name `" << name << "'.");
	}
}

/** Add a new event.
 * @param name		Name of the event.
 * @param params	Parameters for the event. */
void add_event(const char *name, variable_t *params) {
	DEBUG("add_event(" << name << ")");

	Function *func = new Function(name);
	while(params) {
		Type *type = current_service->getType(params->type);
		if(!type) {
			COMPILE_ERROR_AT(params->line, "Parameter type `" << params->type << "' does not exist.");
			return;
		} else if(!func->addParameter(type, params->name, false)) {
			COMPILE_ERROR_AT(params->line, "Duplicate parameter name `" << params->name << "'.");
			return;
		}

		params = params->next;
	}

	if(!current_service->addEvent(func)) {
		COMPILE_ERROR("Duplicate function/event name `" << name << "'.");
	}
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
	current_service = new Service;
	yyin = fopen(current_file, "r");
	if(yyin == NULL) {
		perror(current_file);
		return 1;
	}
	yyparse();
	fclose(yyin);

	/* Check whether enough information has been given. */
	if(current_service->getName().length() == 0) {
		COMPILE_ERROR("Service name has not been set.");
	} else if(current_service->getVersion() == 0) {
		COMPILE_ERROR("Service version has not been set.");
	} else if(current_service->getFunctions().empty() && current_service->getEvents().empty()) {
		COMPILE_ERROR("Service must have at least 1 function/event.");
	}

	/* Check for errors. */
	if(had_error) {
		cerr << "Aborting compilation due to errors." << endl;
		return 1;
	}

	/* Dump the service if in verbose mode. */
	if(verbose_mode) {
		current_service->dump();
	}

	/* Determine which code generator to use. */
	CodeGen *cg;
	if(target == "cxx") {
		cg = new CXXCodeGen(current_service);
	} else {
		cerr << "Unrecognised target `" << target << "'." << endl;
		return 1;
	}

	/* Generate the code. */
	if(server.length() > 0) {
		if(!cg->generateServer(server)) {
			return 1;
		}
	}
	if(client.length() > 0) {
		if(!cg->generateClient(client)) {
			return 1;
		}
	}
	return 0;
}
