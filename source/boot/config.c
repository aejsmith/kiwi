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
#include <boot/error.h>
#include <boot/fs.h>
#include <boot/memory.h>
#include <boot/menu.h>

#include <lib/ctype.h>
#include <lib/string.h>
#include <lib/utility.h>

/** Structure containing details of a command to run. */
typedef struct command_list_entry {
	list_t header;			/**< Link to command list. */
	char *name;			/**< Name of the command. */
	value_list_t *args;		/**< List of arguments. */
} command_list_entry_t;

/** Structure containing an environment entry. */
typedef struct environ_entry {
	list_t header;			/**< Link to environment. */
	char *name;			/**< Name of entry. */
	value_t value;			/**< Value of the entry. */
} environ_entry_t;

/** Length of the temporary buffer. */
#define TEMP_BUF_LEN		512

/** Character returned from get_next_char() for end-of-file. */
#define EOF			-1

static void value_list_destroy(value_list_t *list);
static value_list_t *value_list_copy(value_list_t *source);
static command_list_t *command_list_copy(command_list_t *source);
static void command_list_destroy(command_list_t *list);
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
	"set \"hidden\" true\n"
	"entry \"Kiwi\" {\n"
#if CONFIG_DEBUG
	"	set \"splash_disabled\" true\n"
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

/** Commands allowed in the top level of the configuration. */
static command_t top_level_commands[] = {
	{ "entry", config_cmd_entry },
	{ "set", config_cmd_set },
};

/** Overridden configuration file path. */
char *config_file_override = NULL;

/** Root environment. */
environ_t *root_environ = NULL;

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

/** Copy the contents of one value to another.
 * @param source	Source value.
 * @param dest		Destination value. */
static void value_copy(value_t *source, value_t *dest) {
	dest->type = source->type;
	switch(dest->type) {
	case VALUE_TYPE_INTEGER:
		dest->integer = source->integer;
		break;
	case VALUE_TYPE_BOOLEAN:
		dest->boolean = source->boolean;
		break;
	case VALUE_TYPE_STRING:
		dest->string = kstrdup(source->string);
		break;
	case VALUE_TYPE_LIST:
		dest->list = value_list_copy(source->list);
		break;
	case VALUE_TYPE_COMMAND_LIST:
		dest->cmds = command_list_copy(source->cmds);
		break;
	case VALUE_TYPE_POINTER:
		dest->pointer = source->pointer;
		break;
	}
}

/** Destroy a value.
 * @param value		Value to destroy. */
static void value_destroy(value_t *value) {
	switch(value->type) {
	case VALUE_TYPE_STRING:
		if(value->string) {
			kfree(value->string);
		}
		break;
	case VALUE_TYPE_LIST:
		if(value->list) {
			value_list_destroy(value->list);
		}
		break;
	case VALUE_TYPE_COMMAND_LIST:
		if(value->cmds) {
			command_list_destroy(value->cmds);
		}
		break;
	default:
		break;
	}
}

/** Copy a value list.
 * @param source	Source list.
 * @return		Pointer to destination list. */
static value_list_t *value_list_copy(value_list_t *source) {
	value_list_t *dest = kmalloc(sizeof(value_list_t));
	size_t i;

	dest->count = source->count;
	if(source->count) {
		dest->values = kmalloc(sizeof(value_t) * source->count);
		for(i = 0; i < source->count; i++) {
			value_copy(&source->values[i], &dest->values[i]);
		}
	} else {
		dest->values = NULL;
	}
	return dest;
}

/** Destroy an argument list.
 * @param list		List to destroy. */
static void value_list_destroy(value_list_t *list) {
	size_t i;

	for(i = 0; i < list->count; i++) {
		value_destroy(&list->values[i]);
	}

	kfree(list->values);
	kfree(list);
}

/** Copy a command list.
 * @param source	Source list.
 * @return		Pointer to destination list. */
static command_list_t *command_list_copy(command_list_t *source) {
	command_list_t *dest = kmalloc(sizeof(command_list_t));
	command_list_entry_t *entry, *copy;

	list_init(dest);
	LIST_FOREACH(source, iter) {
		entry = list_entry(iter, command_list_entry_t, header);
		copy = kmalloc(sizeof(command_list_entry_t));
		list_init(&copy->header);
		copy->name = kstrdup(entry->name);
		copy->args = value_list_copy(entry->args);
		list_append(dest, &copy->header);
	}
	return dest;
}

/** Destroy a command list.
 * @param list		List to destroy. */
static void command_list_destroy(command_list_t *list) {
	command_list_entry_t *command;

	LIST_FOREACH_SAFE(list, iter) {
		command = list_entry(iter, command_list_entry_t, header);
		list_remove(&command->header);
		if(command->args) {
			value_list_destroy(command->args);
		}
		kfree(command->name);
		kfree(command);
	}

	kfree(list);
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
			syntax_error("expected space");
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
		} else if(ch == 't') {
			value->type = VALUE_TYPE_BOOLEAN;
			value->boolean = true;
			if(get_next_char() != 'r' || get_next_char() != 'u' || get_next_char() != 'e') {
				syntax_error("unexpected character");
				goto fail;
			}
		} else if(ch == 'f') {
			value->type = VALUE_TYPE_BOOLEAN;
			value->boolean = false;
			if(get_next_char() != 'a' || get_next_char() != 'l' || get_next_char() != 's' ||
			   get_next_char() != 'e') {
				syntax_error("unexpected character");
				goto fail;
			}
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
	command_list_entry_t *command;
	bool in_comment = false;
	command_list_t *list;
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
			command = kmalloc(sizeof(command_list_entry_t));
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

/** Load a configuration file.
 * @param path		Path of the file (used for debugging).
 * @param buf		Pointer to NULL-terminated buffer containing file data.
 * @return		Whether the file was loaded successfully. */
static bool config_load_internal(const char *path, const char *buf) {
	command_list_t *list;
	bool ret;

	current_file = buf;
	current_file_path = path;
	current_file_offset = 0;
	current_file_size = strlen(buf);
	current_line = 1;
	current_col = 0;

	if(!(list = parse_command_list(EOF))) {
		return false;
	}

	root_environ = environ_create();
	ret = command_list_exec(list, top_level_commands, ARRAYSZ(top_level_commands), root_environ);
	command_list_destroy(list);
	return ret;
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

/** Execute a single command from a command list.
 * @param entry		Entry to execute.
 * @param commands	Array of commands that can be used.
 * @param count		Number of commands.
 * @param env		Environment to execute command in.
 * @return		Whether successful. */
static bool command_exec(command_list_entry_t *entry, command_t *commands, int count, environ_t *env) {
	int i;

	for(i = 0; i < count; i++) {
		if(strcmp(commands[i].name, entry->name) == 0) {
			return commands[i].func(entry->args, env);
		}
	}

	dprintf("config: unknown command '%s'\n", entry->name);
	return false;
}

/** Execute a command list.
 * @param list		List of commands.
 * @param commands	Array of commands that can be used.
 * @param count		Number of commands.
 * @param env		Environment to execute commands in.
 * @return		Whether all of the commands completed successfully. */
bool command_list_exec(command_list_t *list, command_t *commands, int count, environ_t *env) {
	command_list_entry_t *entry;

	LIST_FOREACH(list, iter) {
		entry = list_entry(iter, command_list_entry_t, header);
		if(!command_exec(entry, commands, count, env)) {
			return false;
		}
	}

	return true;
}

/** Insert a value into a value list.
 * @param list		List to insert into.
 * @param value		Value to insert (will be copied). */
void value_list_insert(value_list_t *list, value_t *value) {
	list->values = krealloc(list->values, sizeof(value_t) * (list->count + 1));
	value_copy(value, &list->values[list->count++]);
}

/** Create a new environment.
 * @return		Pointer to created environment. */
environ_t *environ_create(void) {
	environ_t *env = kmalloc(sizeof(environ_t));
	list_init(env);
	return env;
}

/** Look up an entry in an environment.
 * @param env		Environment to look up in.
 * @param name		Name of entry to look up.
 * @return		Pointer to value if found, NULL if not. */
value_t *environ_lookup(environ_t *env, const char *name) {
	environ_entry_t *entry;

	LIST_FOREACH(env, iter) {
		entry = list_entry(iter, environ_entry_t, header);
		if(strcmp(entry->name, name) == 0) {
			return &entry->value;
		}
	}

	return NULL;
}

/** Insert an entry into an environment.
 * @param env		Environment to insert into.
 * @param name		Name of entry to look up.
 * @param value		Value to insert. Will be copied. */
void environ_insert(environ_t *env, const char *name, value_t *value) {
	environ_entry_t *entry;

	/* Look for an existing entry with the same name. */
	LIST_FOREACH(env, iter) {
		entry = list_entry(iter, environ_entry_t, header);
		if(strcmp(entry->name, name) == 0) {
			value_destroy(&entry->value);
			value_copy(value, &entry->value);
			return;
		}
	}

	/* Create a new entry. */
	entry = kmalloc(sizeof(environ_entry_t));
	list_init(&entry->header);
	entry->name = kstrdup(name);
	value_copy(value, &entry->value);
	list_append(env, &entry->header);
}

/** Set a value in the environment.
 * @param args		Argument list.
 * @param env		Environment to set in.
 * @return		Whether successful. */
bool config_cmd_set(value_list_t *args, environ_t *env) {
	if(args->count != 2 || args->values[0].type != VALUE_TYPE_STRING) {
		dprintf("config: set: invalid arguments\n");
		return false;
	}

	environ_insert(env, args->values[0].string, &args->values[1]);
	return true;
}

/** Load the bootloader configuration. */
void config_init(void) {
	size_t i;

	if(config_file_override) {
		if(!config_load(NULL, config_file_override)) {
			boot_error("Specified configuration file does not exist");
		}
	} else {
		/* Try the various paths. */
		for(i = 0; i < ARRAYSZ(config_file_paths); i++) {
			if(config_load(NULL, config_file_paths[i])) {
				return;
			}
		}

		/* No configuration was loaded, use the default. */
		if(!config_load_internal("<default>", default_config)) {
			internal_error("Could not load default configuration");
		}
	}
}
