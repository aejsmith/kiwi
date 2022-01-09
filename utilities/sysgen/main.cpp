/*
 * Copyright (C) 2009-2022 Alex Smith
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
 * @brief               Kiwi system call code generator.
 */

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>

#include "amd64_target.h"
#include "arm64_target.h"
#include "sysgen.h"

using namespace std;

/** Type of the system call map. */
typedef map<string, Syscall *> SyscallMap;

/** Information on the file currently being parsed. */
const char *current_file = NULL;
size_t current_line = 1;
static bool had_error = false;

/** Next system call id. */
static long next_call_id = 0;

/** Map of type names to types. */
static TypeMap type_map;

/** List/map of system calls. */
static SyscallList syscall_list;
static SyscallMap syscall_map;

/** Whether to print debug messages. */
static bool verbose_mode = false;

/** Macro to print details of an error.
 * @param e             Error to print. */
#define compile_error(e) \
    { \
        cerr << current_file << ':' << current_line << ": " << e << endl; \
        had_error = true; \
    }

/** Macro to print a debug message if in verbose mode.
 * @param m             Message to print. */
#define debug(m)                \
    if (verbose_mode) \
        cout << current_file << ':' << current_line << ": " << m << endl;

/** Create a new identifier structure.
 * @param str           Value of identifier.
 * @param next          Next identifier in the list.
 * @return              Pointer to the identifier structure. */
identifier_t *new_identifier(const char *str, identifier_t *next) {
    debug("new_identifier(" << str << ", " << next << ")");

    identifier_t *ident = new identifier_t;
    ident->next = next;
    ident->str = strdup(str);
    return ident;
}

/** Add a new type alias.
 * @param name          Name of the alias.
 * @param target        Target type for the alias. */
void add_type(const char *name, const char *target) {
    debug("add_type(" << name << ", " << target << ")");

    if (type_map.find(name) != type_map.end()) {
        compile_error("Type `" << name << "' already exists.");
        return;
    }

    TypeMap::iterator it = type_map.find(target);
    if (it == type_map.end()) {
        compile_error("Alias target `" << target << "' does not exist.");
        return;
    }

    type_map[name] = it->second;
}

/** Add a new system call.
 * @param name          Name of the call.
 * @param params        Parameters for the call.
 * @param attribs       Attributes for the call.
 * @param num           Overridden call number (if -1 next number will be used). */
void add_syscall(const char *name, identifier_t *params, identifier_t *attribs, long num) {
    debug("add_syscall(" << name << ", " << params << ")");

    if (syscall_map.find(name) != syscall_map.end()) {
        compile_error("System call `" << name << "' already exists.");
        return;
    }

    /* Get the call number. */
    if (num < 0) {
        num = next_call_id++;
    } else {
        next_call_id = num + 1;
    }

    Syscall *call = new Syscall(name, num);

    while (params) {
        TypeMap::iterator it = type_map.find(params->str);
        if (it == type_map.end()) {
            compile_error("Parameter type `" << params->str << "' does not exist.");
        } else {
            call->add_param(it->second);
        }

        params = params->next;
    }

    while (attribs) {
        if (strcmp(attribs->str, "hidden") == 0) {
            call->set_attribute(Syscall::kHiddenAttribute);
        } else if (strcmp(attribs->str, "wrapped") == 0) {
            call->set_attribute(Syscall::kWrappedAttribute);
        } else {
            compile_error("Invalid attribute `" << attribs->str << "'.");
        }

        attribs = attribs->next;
    }

    syscall_list.push_back(call);
    syscall_map[name] = call;
}

/** Generate a kernel call table.
 * @param stream        Stream to output to.
 * @param name          Name to give table. */
static void generate_kernel_table(ostream &stream, const string &name) {
    stream << "/* This file is automatically generated. Do not edit! */" << endl;
    stream << "#include <lib/utility.h>" << endl;
    stream << "#include <syscall.h>" << endl;

    for (const Syscall *call : syscall_list) {
        stream << "extern void " << call->name() << "(void);" << endl;
    }

    stream << "syscall_t " << name << "[] = {" << endl;

    for (const Syscall *call : syscall_list) {
        stream << "     [" << call->id() << "] = { .addr = (ptr_t)" << call->name();
        stream << ", .count = " << call->num_params() << " }," << endl;
    }

    stream << "};" << endl;
    stream << "size_t " << name << "_size = array_size(" << name << ");" << endl;
}

/** Generate a call number header.
 * @param stream        Stream to output to.
 * @param name          Header file guard name. */
static void generate_header(ostream &stream, const string &name) {
    stream << "/* This file is automatically generated. Do not edit! */" << endl;
    stream << "#ifndef " << name << endl;
    stream << "#define " << name << endl << endl;

    for (const Syscall *call : syscall_list) {
        stream << "#define __NR_" << call->name() << ' ' << call->id() << endl;
    }

    stream << endl << "#endif" << endl;
}

/** Print usage information and exit.
 * @param stream        Stream to output to.
 * @param progname      Program name. */
static void usage(ostream &stream, const char *progname) {
    stream << "Usage: " << progname << " [-o <output>] [(-t <name>|-h <name>)] <arch> <input>" << endl;
    stream << "Options:" << endl;
    stream << " -o <output> - File to write generated code to. Defaults to stdout." << endl;
    stream << " -t <name>   - Generate a kernel system call table." << endl;
    stream << " -n <name>   - Generate a system call number header." << endl;
    stream << " <arch>      - Architecture to generate code for." << endl;
    stream << " <input>     - System call definition file." << endl;

    exit((&stream == &cerr) ? 1 : 0);
}

/** Main entry point for the program.
 * @param argc          Argument count.
 * @param argv          Argument array.
 * @return              0 on success, 1 on failure. */
int main(int argc, char **argv) {
    string output("-"), table, header;
    int i;

    /* Parse the command line arguments. */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(cout, argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            verbose_mode = true;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (++i == argc || argv[i][0] == 0) {
                cerr << "Option '-o' requires an argument." << endl;
                usage(cerr, argv[0]);
            }

            output = argv[i];
        } else if (strcmp(argv[i], "-t") == 0) {
            if (header.length()) {
                cerr << "Options '-t' and '-n' are mutually exclusive." << endl;
                usage(cerr, argv[0]);
            } else if (++i == argc || argv[i][0] == 0) {
                cerr << "Option '-t' requires an argument." << endl;
                usage(cerr, argv[0]);
            }

            table = argv[i];
        } else if (strcmp(argv[i], "-n") == 0) {
            if (table.length()) {
                cerr << "Options '-t' and '-n' are mutually exclusive." << endl;
                usage(cerr, argv[0]);
            } else if (++i == argc || argv[i][0] == 0) {
                cerr << "Option '-n' requires an argument." << endl;
                usage(cerr, argv[0]);
            }

            header = argv[i];
        } else if (argv[i][0] == '-') {
            cerr << "Unrecognised argument '" << argv[i] << '\'' << endl;
            usage(cerr, argv[0]);
        } else {
            break;
        }
    }

    /* Must be two more arguments (target/input). */
    if ((argc - i) != 2)
        usage(cerr, argv[0]);

    /* Find the target and add in its types. */
    Target *target;
    if (strcmp(argv[i], "amd64") == 0) {
        target = new AMD64Target();
    } else if (strcmp(argv[i], "arm64") == 0) {
        target = new ARM64Target();
    } else {
        cerr << "Unrecognised target `" << argv[i] << "'." << endl;
        return 1;
    }
    target->add_types(type_map);

    /* Parse the input file. */
    current_file = argv[++i];
    current_line = 1;
    yyin = fopen(current_file, "r");
    if (yyin == NULL) {
        perror(current_file);
        return 1;
    }
    yyparse();
    fclose(yyin);

    /* Check whether enough information has been given. */
    if (syscall_list.empty())
        compile_error("At least 1 system call must be defined.");

    /* Check for errors. */
    if (had_error) {
        cerr << "Aborting compilation due to errors." << endl;
        return 1;
    }

    /* Open the output file and generate the code. */
    if (output == "-") {
        if (table.length()) {
            generate_kernel_table(cout, table);
        } else if (header.length()) {
            generate_header(cout, header);
        } else {
            target->generate(cout, syscall_list);
        }
    } else {
        ofstream stream;
        stream.open(output.c_str(), ofstream::trunc);
        if (stream.fail()) {
            cerr << "Failed to create output file `" << output << "'." << endl;
            return 1;
        }

        if (table.length()) {
            generate_kernel_table(stream, table);
        } else if (header.length()) {
            generate_header(stream, header);
        } else {
            target->generate(stream, syscall_list);
        }

        stream.close();
    }

    return 0;
}
