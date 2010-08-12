/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		Temporary shell.
 */

#ifndef __FAILSHELL_H
#define __FAILSHELL_H

#include <kernel/types.h>

#include <cstring>
#include <map>
#include <string>

/** Shell class. */
class Shell {
	friend class HelpCommand;
public:
	/** Command class. */
	class Command {
	public:
		/** Constructor.
		 * @param name		Command name.
		 * @param description	Command description. */
		Command(const char *name, const char *description) :
			m_name(name), m_description(description)
		{
			Shell::AddCommand(this);
		}

		/** Get the command name.
		 * @return		Name of command. */
		const char *GetName(void) { return m_name; }

		/** Get the command description.
		 * @return		Description of command. */
		const char *GetDescription(void) { return m_description; }

		/** Implementation of the command.
		 * @param argc		Argument count.
		 * @param argv		Argument array.
		 * @return		0 on success, other value on failure. */
		virtual int operator ()(int argc, char **argv) = 0;

		/** Implementation of the command.
		 * @param shell		Shell command was called in.
		 * @param argc		Argument count.
		 * @param argv		Argument array.
		 * @return		0 on success, other value on failure. */
		virtual int operator ()(Shell *shell, int argc, char **argv) { return (*this)(argc, argv); }
	private:
		const char *m_name;		/**< Name of command. */
		const char *m_description;	/**< Description of command. */
	};
private:
	/** Type of the command map. */
	typedef std::map<std::string, Command *> CommandMap;
public:
	/** Constructor. */
	Shell(FILE *input) : m_input(input), m_exit(false) {}

	int Run(void);

	/** Exit the shell after the current command finishes. */
	void Exit(void) { m_exit = true; }

	static void AddCommand(Command *cmd);
private:
	char *ReadLine(void);
	bool SplitLine(char *line, int &argc, char **&argv);
	void RunCommand(int argc, char **argv);

	FILE *m_input;			/**< Input stream. */
	bool m_exit;			/**< Whether to exit the shell after the current command. */

	/** List of commands. */
	static CommandMap m_commands;
};

/** Helper macro to check if a help message is wanted. */
#define SHELL_HELP(ac, av)	\
	((ac) > 1 && strcmp((av)[1], "--help") == 0)

#endif /* __FAILSHELL_H */
