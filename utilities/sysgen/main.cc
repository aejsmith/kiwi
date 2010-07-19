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

#include <boost/foreach.hpp>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>

#include "AMD64Target.h"
#include "IA32Target.h"
#include "sysgen.h"

using namespace std;

/** Type of the system call map. */
typedef map<string, Syscall *> SyscallMap;

/** Information on the file currently being parsed. */
const char *current_file = NULL;
size_t current_line = 1;
static bool had_error = false;

/** Target that code is being generated for. */
static Target *code_target = NULL;

/** Information about the current service. */
static uint16_t current_service = 0;
static uint16_t next_call_id = 0;

/** Map of type names to types. */
static TypeMap type_map;

/** List/map of system calls. */
static SyscallList syscall_list;
static SyscallMap syscall_map;

/** Whether to print debug messages. */
static bool verbose_mode = false;

/** Macro to print details of an error.
 * @param e		Error to print. */
#define COMPILE_ERROR(e)	\
	{ \
		cerr << current_file << ':' << current_line << ": " << e << endl; \
		had_error = true; \
	}

/** Macro to print a debug message if in verbose mode.
 * @param m		Message to print. */
#define DEBUG(m)		\
	if(verbose_mode) { \
		cout << current_file << ':' << current_line << ": " << m << endl; \
	}

/** Construct a system call object.
 * @param name		Name of the system call.
 * @param ret		Return type of the call.
 * @param service	ID of the service for the call.
 * @param id		ID of the call. */
Syscall::Syscall(const char *name, Type *ret, uint16_t service, uint16_t id) :
	m_name(name), m_return_type(ret), m_param_count(0)
{
	m_id = (service << 16) | id;
}

/** Add a parameter to a service.
 * @param type		Type of the parameter.
 * @return		Whether added successfully. */
bool Syscall::addParameter(Type *type) {
	if((m_param_count += type->count) > code_target->maxParameters()) {
		return false;
	}
	return true;
}

/** Create a new parameter structure.
 * @param type		Type of variable.
 * @param next		Next parameter in the list.
 * @return		Pointer to the parameter structure. */
parameter_t *new_parameter(const char *type, parameter_t *next) {
	DEBUG("new_parameter(" << type << ", " << next << ")");

	parameter_t *param = new parameter_t;
	param->next = next;
	param->type = strdup(type);
	return param;
}

/** Set the current service number.
 * @param num		Number to set. */
void set_service_number(unsigned long num) {
	DEBUG("set_service_number(" << num << ")");

	if(num > 0xFFFF) {
		COMPILE_ERROR("Service number must not be greater than 65535");
	}
	current_service = num;
	next_call_id = 0;
}

/** Add a new type alias.
 * @param name		Name of the alias.
 * @param target	Target type for the alias. */
void add_type(const char *name, const char *target) {
	DEBUG("add_type(" << name << ", " << target << ")");

	if(type_map.find(name) != type_map.end()) {
		COMPILE_ERROR("Type `" << name << "' already exists.");
		return;
	}

	TypeMap::iterator it = type_map.find(target);
	if(it == type_map.end()) {
		COMPILE_ERROR("Alias target `" << target << "' does not exist.");
		return;
	}

	type_map[name] = it->second;
}

/** Add a new system call.
 * @param type		Return type of the call.
 * @param name		Name of the call.
 * @param params	Parameters for the call.
 * @param num		Overridden call number (if -1 default will be used). */
void add_syscall(const char *type, const char *name, parameter_t *params, int num) {
	DEBUG("add_syscall(" << type << ", " << name << ", " << params << ")");

	if(syscall_map.find(name) != syscall_map.end()) {
		COMPILE_ERROR("System call `" << name << "' already exists.");
		return;
	}

	Type *ret = NULL;
	if(strcmp(type, "void") != 0) {
		TypeMap::iterator it = type_map.find(type);
		if(it == type_map.end()) {
			COMPILE_ERROR("Return type `" << type << "' does not exist.");
			return;
		}
		ret = it->second;
	}

	/* Get the call number. */
	if(num < 0) {
		num = next_call_id++;
	} else {
		next_call_id = num + 1;
	}

	Syscall *call = new Syscall(name, ret, current_service, num);
	while(params) {
		if(strcmp(params->type, "void") == 0) {
			COMPILE_ERROR("Parameters cannot be `void'.");
			return;
		}

		TypeMap::iterator it = type_map.find(params->type);
		if(it == type_map.end()) {
			COMPILE_ERROR("Parameter type `" << params->type << "' does not exist.");
			return;
		} else if(!call->addParameter(it->second)) {
			COMPILE_ERROR("System call `" << name << "' has too many parameters.");
			return;
		}
		params = params->next;
	}

	syscall_list.push_back(call);
	syscall_map[name] = call;
}

/** Print usage information and exit.
 * @param stream	Stream to output to.
 * @param progname	Program name. */
static void usage(ostream &stream, const char *progname) {
	stream << "Usage: " << progname << " [--no-errno] [-o <output>] <target> <input...>" << endl;
	stream << "Options:" << endl;
	stream << " --no-errno  - The generated code should not set errno." << endl;
	stream << " -o <output> - File to write generated code to." << endl;
	stream << " <target>    - Target architecture to generate code for." << endl;
	stream << " <input...>  - Input files." << endl;

	exit((stream == cerr) ? 1 : 0);
}

/** Main entry point for the program.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		0 on success, 1 on failure. */
int main(int argc, char **argv) {
	bool use_errno = true;
	string output("-");
	int i;

	/* Parse the command line arguments. */
	for(i = 1; i < argc; i++) {
		if(strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
			usage(cout, argv[0]);
			return 0;
		} else if(strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
			verbose_mode = true;
		} else if(strcmp(argv[i], "--no-errno") == 0) {
			use_errno = false;
		} else if(strcmp(argv[i], "-o") == 0) {
			if(++i == argc || argv[i][0] == 0) {
				cerr << "Option '-o' requires an argument." << endl;
				usage(cerr, argv[0]);
			}
			output = argv[i];
		} else if(argv[i][0] == '-') {
			cerr << "Unrecognised argument '" << argv[i] << '\'' << endl;
			usage(cerr, argv[0]);
		} else {
			break;
		}
	}

	/* Must be at least two more arguments (target/input). */
	if((argc - i) < 2) {
		usage(cerr, argv[0]);
	}

	/* Find the target and add in its types. */
	if(strcmp(argv[i], "amd64") == 0) {
		code_target = new AMD64Target();
	} else if(strcmp(argv[i], "ia32") == 0) {
		code_target = new IA32Target();
	} else {
		cerr << "Unrecognised target `" << argv[i] << "'." << endl;
		return 1;
	}
	code_target->addTypes(type_map);

	/* Parse each input file. */
	for(i = i + 1; i < argc; i++) {
		current_file = argv[i];
		current_line = 1;

		yyin = fopen(current_file, "r");
		if(yyin == NULL) {
			perror(current_file);
			return 1;
		}
		yyparse();
		fclose(yyin);
	}

	/* Check whether enough information has been given. */
	if(syscall_list.empty()) {
		COMPILE_ERROR("At least 1 system call must be defined.");
	}

	/* Check for errors. */
	if(had_error) {
		cerr << "Aborting compilation due to errors." << endl;
		return 1;
	}

	/* Open the output file and generate the code. */
	if(output == "-") {
		code_target->generate(cout, syscall_list, use_errno);
	} else {
		ofstream stream;
		stream.open(output.c_str(), ofstream::trunc);
		if(stream.fail()) {
			cerr << "Failed to create output file `" << output << "'." << endl;
			return 1;
		}

		code_target->generate(stream, syscall_list, use_errno);
		stream.close();
	}

	return 0;
}
