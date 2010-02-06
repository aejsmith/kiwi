/*
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Kiwi shell.
 */

#include <kernel/fs.h>

#include <kiwi/Process.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

#include "failshell.h"

using namespace kiwi;
using namespace std;

/** Map of shell commands. */
map<string, Shell::Command *> Shell::m_commands;

/** Help command. */
class HelpCommand : Shell::Command {
public:
	HelpCommand() : Command("help", "Print a list of available commands.") {}
	int operator ()(int argc, char **argv) { return 0; }

	/** Print a list of built-in commands.
	 * @param shell		Shell command was called in.
	 * @param argc		Argument count.
	 * @param argv		Argument array.
	 * @return		0 on success, other value on failure. */
	int operator ()(Shell *shell, int argc, char **argv) {
		const char *nargs[] = { NULL, "--help", NULL };
		map<string, Shell::Command *>::iterator it;

		/* If we want a specific command, call it with --help as an
		 * argument. */
		if(argc > 1 && !SHELL_HELP(argc, argv)) {
			nargs[0] = argv[1];

			it = Shell::m_commands.find(argv[1]);
			if(it != Shell::m_commands.end()) {
				(*it->second)(shell, 2, const_cast<char **>(nargs));
				return 0;
			}

			cout << "Requested help on invalid command '" << argv[1] << "'" << endl;
			return 1;
		}

		cout << "Command       Info" << endl;
		cout << "=======       ====" << endl;

		for(it = Shell::m_commands.begin(); it != Shell::m_commands.end(); it++) {
			printf("%-12s  %s\n", it->second->Name(), it->second->Description());
		}

		return 0;
	}
};

/** Instance of the help command. */
static HelpCommand help_command;

/** Exit command. */
class ExitCommand : Shell::Command {
public:
	ExitCommand() : Command("exit", "Exit the shell.") {}
	int operator ()(int argc, char **argv) { return 0; }

	/** Exit the shell.
	 * @param shell		Shell command was called in.
	 * @param argc		Argument count.
	 * @param argv		Argument array.
	 * @return		0 on success, other value on failure. */
	int operator ()(Shell *shell, int argc, char **argv) {
		if(SHELL_HELP(argc, argv)) {
			cout << "Usage: " << argv[0] << endl;
			return 0;
		}

		shell->Exit();
		return 0;
	}
};

/** Instance of the exit command. */
static ExitCommand exit_command;

/** Add a command to the shell.
 * @param cmd		Command to add. */
void Shell::AddCommand(Command *cmd) {
	m_commands.insert(pair<string, Command *>(cmd->Name(), cmd));
}

/** Main loop for the shell.
 * @return		Process exit code. */
int Shell::Run(void) {
	char **argv, *line, cwd[4096];
	int argc;

	while(true) {
		if(fs_getcwd(cwd, 4096) == 0) {
			cout << "Kiwi:" << cwd << "> ";
		} else {
			cout << "Kiwi> ";
		}
		if(!(line = ReadLine())) {
			cout << endl << "Out of memory" << endl;
			return 1;
		}

		/* Split the string up. */
		if(!SplitLine(line, argc, argv)) {
			cout << "Out of memory" << endl;
			return 1;
		} else if(!argc) {
			cout << "You must enter a command!" << endl;
			continue;
		}

		RunCommand(argc, argv);
		free(line);
		free(argv);

		if(m_exit) {
			return 0;
		}
	}
	return 0;
}

/** Get a line of input.
 * @return		Pointer to buffer, or NULL if out of memory. */
char *Shell::ReadLine(void) {
	char *buf = NULL, *tmp;
	size_t count = 0;
	int ch;

	do {
		if((ch = fgetc(m_input)) == EOF) {
			free(buf);
			return NULL;
		} else if(!(tmp = reinterpret_cast<char *>(realloc(buf, count + 1)))) {
			free(buf);
			return NULL;
		}
		buf = tmp;

		if(ch == '\b') {
			if(count > 0) {
				buf[--count] = 0;
			}
		} else {
			buf[count++] = (ch == '\n') ? 0 : static_cast<unsigned char>(ch);
		}
	} while(ch != '\n');

	return buf;
}

/** Split a line into arguments/count.
 * @param line		Line to split (will be modified).
 * @param argc		Where to store argument count.
 * @param argv		Where to store argument array.
 * @return		True on success, false on failure. */
bool Shell::SplitLine(char *line, int &argc, char **&argv) {
	char **tmp, *tok;

	argc = 0;
	argv = 0;

	/* Split up the string into an array of tokens, seperated by spaces. */
	while((tok = strsep(&line, " "))) {
		if(!tok[0]) {
			continue;
		} else if(!(tmp = reinterpret_cast<char **>(realloc(argv, (argc + 2) * sizeof(char *))))) {
			free(argv);
			return false;
		}
		argv = tmp;

		/* Must NULL-terminate the array as process creation requires
		 * it. */
		argv[argc++] = tok;
		argv[argc] = NULL;
	}

	return true;
}

/** Run a command.
 * @param argc		Argument count.
 * @param argv		Argument array. */
void Shell::RunCommand(int argc, char **argv) {
	map<string, Command *>::iterator it;
	Process proc;
	int ret;

	/* Try to match it against a built-in command. */
	it = Shell::m_commands.find(argv[0]);
	if(it != Shell::m_commands.end()) {
		if((ret = (*it->second)(this, argc, argv)) != 0) {
			cout << "Command returned error status " << ret << endl;
		}
		return;
	}

	/* Run a process. */
	if(!proc.Create(argv)) {
		cout << "Failed to run command '" << argv[0] << "'" << endl;
	}
	proc.WaitTerminate();
}

/** Main function for FailShell.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		Process exit code. */
int main(int argc, char **argv) {
	Shell shell(stdin);

	cout << endl;
	cout << "Welcome to FailShell! (process " << Process::GetCurrentID() << ")" << endl;
	return shell.Run();
}
