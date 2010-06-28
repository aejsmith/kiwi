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
 * @brief		Configuration file parser.
 *
 * @fixme		The parser is a little bit shit...
 */

#include <boot/config.h>
#include <boot/console.h>
#include <boot/fs.h>
#include <boot/memory.h>

#include <lib/ctype.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <fatal.h>

/** Structure containing details of a command to run. */
typedef struct command {
	list_t header;			/**< Link to command list. */
	char *name;			/**< Name of the command. */
	value_list_t *args;		/**< List of arguments. */
} command_t;

/** Structure containing an environment entry. */
typedef struct environ_entry {
	list_t header;			/**< Link to environment. */
	char *name;			/**< Name of entry. */
	value_t *value;			/**< Value of the entry. */
} environ_entry_t;

/** Length of the temporary buffer. */
#define TEMP_BUF_LEN		512

/** Character returned from get_next_char() for end-of-file. */
#define EOF			-1

static command_list_t *parse_command_list(char endch);

/** Temporary buffer to collect strings in. */
static char temp_buf[TEMP_BUF_LEN];
static size_t temp_buf_idx = 0;

/** Current parsing state. */
static const char *current_file;	/**< Pointer to data for current file. */
static const char *current_file_path;	/**< Current configuration file path. */
static size_t current_file_offset;	/**< Current offset in the file. */
static size_t current_file_size;		/**< Size of the current file. */
static int current_line;		/**< Current line in the file. */
static int current_col;			/**< Current column in the file (minus 1). */

/** Default configuration for when no configuration exists. */
static const char *default_config =
	"set \"default\" 0\n"
	"set \"timeout\" 0\n"
	"entry \"Kiwi\" {\n"
#if CONFIG_DEBUG
	"	set \"splash_disabled\" 0\n"
#endif
	"	kiwi\n"
	"}\n";

/** Configuration file paths to try. */
static const char *config_file_paths[] = {
	"/system/boot/loader.cfg",
	"/boot/loader.cfg",
	"/kiwi/loader.cfg",
	"/loader.cfg",
};

/** Overridden configuration file path. */
char *config_file_override = NULL;

/** Read a character from the input file.
 * @return		Character read. */
static char get_next_char(void) {
	char ch;

	if(current_file_offset < current_file_size) {
		ch = current_file[current_file_offset++];
		if(ch == '\n') {
			current_line++;
			current_col = 0;
		} else if(ch == '\t') {
			current_col += 8 - (current_col % 8);
		} else {
			current_col++;
		}
		return ch;
	} else {
		return EOF;
	}
}

/** Rewind the input by a character. */
static void rewind_input(void) {
	if(current_file_offset > 0) {
		current_file_offset--;
		if(current_col > 0) {
			current_col--;
		} else if(current_line > 1) {
			current_line--;
		}
	}
}

/** Print details of a syntax error.
 * @param fmt		Message format.
 * @param ...		Arguments to substitute into format string. */
static void syntax_error(const char *fmt, ...) {
	va_list args;

	dprintf("config: %s:%d:%d: error: ", current_file_path, current_line, current_col);
	va_start(args, fmt);
	dvprintf(fmt, args);
	va_end(args);
	dprintf("\n");
}

/** Destroy an argument list.
 * @param list		List to destroy. */
static void value_list_destroy(value_list_t *list) {
	// FIXME
}

/** Destroy a command list.
 * @param list		List to destroy. */
static void command_list_destroy(command_list_t *list) {
	// FIXME
}

/** Parse an integer.
 * @param intp		Where to store integer parsed.
 * @return		Whether successful. */
static bool parse_integer(int *intp) {
	char ch;

	while(true) {
		ch = get_next_char();
		if(isdigit(ch)) {
			temp_buf[temp_buf_idx++] = ch;
		} else {
			temp_buf[temp_buf_idx] = 0;
			*intp = strtoul(temp_buf, NULL, 0);
			temp_buf_idx = 0;
			return true;
		}
	}
}

/** Parse a string.
 * @return		Pointer to string on success, NULL on failure. */
static char *parse_string(void) {
	bool escaped = false;
	char *ret, ch;

	while(true) {
		ch = get_next_char();
		if(ch == EOF) {
			syntax_error("unexpected EOF, expected end of string");
			return NULL;
		} else if(!escaped && ch == '"') {
			temp_buf[temp_buf_idx] = 0;
			ret = kstrdup(temp_buf);
			temp_buf_idx = 0;
			return ret;
		} else {
			if(ch == '\\' && !escaped) {
				escaped = true;
			} else {
				temp_buf[temp_buf_idx++] = ch;
				escaped = false;
			}
		}
	}
}

/** Parse an value list.
 * @param endch		End character for the list.
 * @return		Pointer to list on success, NULL on failure. */
static value_list_t *parse_value_list(char endch) {
	bool need_space = false;
	value_list_t *list;
	value_t *value;
	char ch;

	list = kmalloc(sizeof(value_list_t));
	list->values = NULL;
	list->count = 0;

	while(true) {
		ch = get_next_char();
		if(ch == endch) {
			return list;
		} else if(isspace(ch)) {
			need_space = false;
			continue;
		} else if(need_space) {
			syntax_error("expected space", ch);
			goto fail;
		}

		/* Start of a new argument. */
		list->values = krealloc(list->values, sizeof(value_t) * (list->count + 1));
		value = &list->values[list->count++];
		need_space = true;
		if(isdigit(ch)) {
			/* Integers are a bit dodgy. They have no special
			 * start/end character like everything else, so
			 * parse_integer() returns when it encounters anything
			 * that's not a digit. In this case we have to return
			 * to the previous character before continuing. */
			value->type = VALUE_TYPE_INTEGER;
			rewind_input();
			if(!parse_integer(&value->integer)) {
				goto fail;
			}
			rewind_input();
		} else if(ch == '"') {
			value->type = VALUE_TYPE_STRING;
			if(!(value->string = parse_string())) {
				goto fail;
			}
		} else if(ch == '[') {
			value->type = VALUE_TYPE_LIST;
			if(!(value->list = parse_value_list(']'))) {
				goto fail;
			}
		} else if(ch == '{') {
			value->type = VALUE_TYPE_COMMAND_LIST;
			if(!(value->cmds = parse_command_list('}'))) {
				goto fail;
			}
		}
	}
fail:
	value_list_destroy(list);
	return NULL;
}

/** Parse a command list.
 * @param endch		Character signalling the end of the list.
 * @return		Pointer to list on success, NULL on failure. */
static command_list_t *parse_command_list(char endch) {
	bool in_comment = false;
	command_list_t *list;
	command_t *command;
	char ch;

	list = kmalloc(sizeof(command_list_t));
	list_init(list);

	while(true) {
		ch = get_next_char();
		if(in_comment) {
			if(ch == '\n') {
				in_comment = false;
			}
			continue;
		} else if(ch == endch || isspace(ch)) {
			if(temp_buf_idx == 0) {
				if(ch == endch) {
					return list;
				} else {
					continue;
				}
			}

			temp_buf[temp_buf_idx] = 0;
			if(strcmp(temp_buf, "#") == 0) {
				in_comment = true;
				temp_buf_idx = 0;
				continue;
			}

			/* End of command name, push it onto the list. */
			command = kmalloc(sizeof(command_t));
			list_init(&command->header);
			command->name = kstrdup(temp_buf);
			list_append(list, &command->header);
			temp_buf_idx = 0;
			if(ch == '\n') {
				command->args = kmalloc(sizeof(value_list_t));
				command->args->values = NULL;
				command->args->count = 0;
			} else {
				if(!(command->args = parse_value_list('\n'))) {
					goto fail;
				}
			}
		} else if(ch == EOF) {
			syntax_error("unexpected end of file");
			goto fail;
		} else {
			temp_buf[temp_buf_idx++] = ch;
			continue;
		}
	}
fail:
	command_list_destroy(list);
	return NULL;
}

static void dump_command_list(command_list_t *list, int indent);

static void dump_value_list(value_list_t *list, int indent) {
	for(int i = 0; i < list->count; i++) {
		dprintf("%*sArgument %d: ", indent, "", i);
		switch(list->values[i].type) {
		case VALUE_TYPE_INTEGER:
			dprintf("%d\n", list->values[i].integer);
			break;
		case VALUE_TYPE_STRING:
			dprintf("'%s'\n", list->values[i].string);
			break;
		case VALUE_TYPE_LIST:
			dprintf("\n");
			dump_value_list(list->values[i].list, indent + 1);
			break;
		case VALUE_TYPE_COMMAND_LIST:
			dprintf("\n");
			dump_command_list(list->values[i].cmds, indent + 1);
			break;
		case VALUE_TYPE_POINTER:
			dprintf("%p\n", list->values[i].pointer);
			break;
		}
	}
}

static void dump_command_list(command_list_t *list, int indent) {
	command_t *command;
	int i = 0;

	LIST_FOREACH(list, iter) {
		command = list_entry(iter, command_t, header);
		dprintf("%*sCommand %d: '%s':\n", indent, "", i, command->name);
		dump_value_list(command->args, indent + 1);
		i++;
	}
}

/** Load a configuration file.
 * @param path		Path of the file (used for debugging).
 * @param buf		Pointer to NULL-terminated buffer containing file data.
 * @return		Whether the file was loaded successfully. */
static bool config_load_internal(const char *path, const char *buf) {
	command_list_t *list;

	current_file = buf;
	current_file_path = path;
	current_file_offset = 0;
	current_file_size = strlen(buf);
	current_line = 1;
	current_col = 0;

	if(!(list = parse_command_list(EOF))) {
		return false;
	}

	dump_command_list(list, 0);
	return true;
}

/** Load a configuration file.
 * @param mount		Mount that the file is on.
 * @param path		Path of the file.
 * @return		Whether the file was loaded successfully. */
static bool config_load(fs_mount_t *mount, const char *path) {
	fs_handle_t *handle;
	size_t size;
	char *buf;
	bool ret;

	if(!(handle = fs_open(mount, path))) {
		return false;
	}

	size = fs_file_size(handle);
	buf = kmalloc(size);
	if(!fs_file_read(handle, buf, size, 0)) {
		kfree(buf);
		fs_close(handle);
		return false;
	}

	ret = config_load_internal(path, buf);
	kfree(buf);
	fs_close(handle);
	return ret;
}

/** Load the bootloader configuration. */
void config_init(void) {
	size_t i;

	if(config_file_override) {
		if(!config_load(boot_filesystem, config_file_override)) {
			fatal("Specified configuration file does not exist");
		}
	} else {
		/* Try the various paths. */
		for(i = 0; i < ARRAYSZ(config_file_paths); i++) {
			if(config_load(boot_filesystem, config_file_paths[i])) {
				return;
			}
		}

		/* No configuration was loaded, use the default. */
		config_load_internal("<default>", default_config);
	}
}
