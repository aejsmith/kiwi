/* Kiwi shell
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

#include <kernel/handle.h>
#include <kernel/process.h>

#include <kiwi/Process.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "failshell.h"

using namespace kiwi;

/** Array of shell commands. */
Shell::Command **Shell::m_commands = NULL;
size_t Shell::m_command_count = 0;

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
		size_t i;

		/* If we want a specific command, call it with --help as an
		 * argument. */
		if(argc > 1 && !SHELL_HELP(argc, argv)) {
			nargs[0] = argv[1];
			for(i = 0; i < Shell::m_command_count; i++) {
				if(strcmp(argv[1], Shell::m_commands[i]->name()) == 0) {
					(*Shell::m_commands[i])(shell, 2, const_cast<char **>(nargs));
					return 0;
				}
			}

			printf("Requested help on invalid command '%s'\n", argv[1]);
			return 1;
		}

		printf("Command       Info\n");
		printf("=======       ====\n");

		for(i = 0; i < Shell::m_command_count; i++) {
			printf("%-12s  %s\n", Shell::m_commands[i]->name(), Shell::m_commands[i]->description());
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
			printf("Usage: %s\n", argv[0]);
			return 0;
		}

		shell->exit();
		return 0;
	}
};

/** Instance of the exit command. */
static ExitCommand exit_command;

/** Add a command to the shell.
 * @param cmd		Command to add. */
void Shell::add_command(Command *cmd) {
	m_commands = reinterpret_cast<Command **>(realloc(m_commands, (m_command_count + 1) * sizeof(Command *)));
	if(m_commands == NULL) {
		printf("Could not add commands!");
		::exit(1);
	}

	m_commands[m_command_count++] = cmd;
}

/** Main loop for the shell.
 * @return		Process exit code. */
int Shell::run(void) {
	char **argv, *line;
	int argc;

	while(true) {
		/* Print a prompt and read in the line. */
		printf("Kiwi> ");
		if(!(line = readline())) {
			printf("\nOut of memory\n");
			return 1;
		}

		/* Split the string up. */
		if(!splitline(line, argc, argv)) {
			printf("Out of memory\n");
			return 1;
		} else if(!argc) {
			printf("You must enter a command!\n");
			continue;
		}

		do_command(argc, argv);
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
char *Shell::readline(void) {
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
				putchar(ch);
				putchar(' ');
				putchar(ch);
			}
		} else {
			buf[count++] = (ch == '\n') ? 0 : (unsigned char)ch;
			putchar(ch);
		}
	} while(ch != '\n');

	return buf;
}

/** Split a line into arguments/count.
 * @param line		Line to split (will be modified).
 * @param argc		Where to store argument count.
 * @param argv		Where to store argument array.
 * @return		True on success, false on failure. */
bool Shell::splitline(char *line, int &argc, char **&argv) {
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
void Shell::do_command(int argc, char **argv) {
	Process *process;
	size_t i;
	int ret;

	/* Try to match it against a built-in command. */
	for(i = 0; i < m_command_count; i++) {
		if(strcmp(argv[0], m_commands[i]->name()) == 0) {
			if((ret = (*m_commands[i])(this, argc, argv)) != 0) {
				printf("Command returned error status %d\n", ret);
			}
			return;
		}
	}

	if((ret = Process::create(process, argv)) != 0) {
		printf("Failed to run command '%s' (%d)\n", argv[0], ret);
	} else {
		handle_wait(process->get_handle_id(), PROCESS_EVENT_DEATH, 0);
		delete process;
	}
}

/** Main function for FailShell.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		Process exit code. */
int main(int argc, char **argv) {
	Shell shell(stdin);

	printf("\n");
	printf("Welcome to FailShell! (process %d)\n", Process::get_current_id());
	return shell.run();
}
