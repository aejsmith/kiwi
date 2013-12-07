/*
 * Copyright (C) 2013 Alex Smith
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
 * @brief		Simple shell.
 */

#include <kernel/fs.h>
#include <kernel/status.h>

#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <unistd.h>

/** Map of all commands. */
typedef std::function<int (std::vector<std::string>)> Command;
typedef std::map<std::string, Command> CommandMap;
static CommandMap shell_commands;

static int cd_command(std::vector<std::string> args) {
	if(args.size() != 2) {
		printf("Error: invalid arguments\n");
		return 1;
	}

	status_t ret = kern_fs_set_curr_dir(args[1].c_str());
	if(ret != STATUS_SUCCESS) {
		printf("Failed to change directory: %d (%s)\n", ret,
			__kernel_status_strings[ret]);
		return 1;
	}

	return 0;
}

static int ln_command(std::vector<std::string> args) {
	if(args.size() != ((args.size() > 1 && args[1][0] == '-') ? 4 : 3)) {
		printf("Error: invalid arguments\n");
		return 1;
	}

	bool symlink = false;
	unsigned idx = 1;
	if(args[1][0] == '-') {
		if(args[1].compare("-s") == 0) {
			symlink = true;
		} else {
			printf("Error: unknown option '%s'\n", args[1].c_str());
		}

		idx++;
	}

	status_t ret = (symlink)
		? kern_fs_create_symlink(args[idx + 1].c_str(), args[idx].c_str())
		: kern_fs_link(args[idx + 1].c_str(), args[idx].c_str());
	if(ret != STATUS_SUCCESS) {
		printf("Failed to create %slink: %d (%s)\n",
			(symlink) ? "sym" : "", ret,
			__kernel_status_strings[ret]);
		return 1;
	}

	return 0;
}

static int ls_command(std::vector<std::string> args) {
	static char buf[FS_PATH_MAX];
	status_t ret;

	std::string dir = (args.size() > 1) ? args[1] : ".";

	handle_t handle;
	ret = kern_fs_open(dir.c_str(), FILE_RIGHT_READ, 0, FS_OPEN, &handle);
	if(ret != STATUS_SUCCESS) {
		printf("Failed to open directory '%s': %d (%s)\n", dir.c_str(),
			ret, __kernel_status_strings[ret]);
		return 1;
	}

	printf("ID    Links  Size       Name\n");
	printf("==    =====  ====       ====\n");

	while(true) {
		dir_entry_t *entry = reinterpret_cast<dir_entry_t *>(buf);
		ret = kern_file_read_dir(handle, entry, FS_PATH_MAX);
		if(ret != STATUS_SUCCESS) {
			free(entry);
			kern_handle_close(handle);
			if(ret != STATUS_NOT_FOUND) {
				printf("Failed to read directory: %d (%s)\n",
					ret, __kernel_status_strings[ret]);
				return 1;
			}

			return 0;
		}

		std::string path = dir + "/" + entry->name;

		/* Get information. */
		file_info_t info;
		ret = kern_fs_info(path.c_str(), false, &info);
		if(ret != STATUS_SUCCESS) {
			printf("Failed to get entry information: %d (%s)\n",
				ret, __kernel_status_strings[ret]);
			free(entry);
			kern_handle_close(handle);
			return ret;
		}

		printf("%-5llu %-6zu %-10llu %s", info.id, info.links, info.size, entry->name);

		ret = kern_fs_read_symlink(path.c_str(), buf, FS_PATH_MAX);
		if(ret == STATUS_SUCCESS) {
			printf(" -> %s\n", buf);
		} else {
			printf("\n");
		}
	}
}

static int mkdir_command(std::vector<std::string> args) {
	if(args.size() != 2) {
		printf("Error: invalid arguments\n");
		return 1;
	}

	status_t ret = kern_fs_create_dir(args[1].c_str());
	if(ret != STATUS_SUCCESS) {
		printf("Failed to create directory: %d (%s)\n", ret,
			__kernel_status_strings[ret]);
		return 1;
	}

	return 0;
}

static int mount_command(std::vector<std::string> args) {
	if(args.size() < 2) {
		printf("Error: invalid arguments\n");
		return 1;
	}

	size_t idx = 1;
	uint32_t flags = 0;

	if(args[1][0] == '-') {
		for(size_t i = 1; i < args[1].size(); i++) {
			switch(args[1][i]) {
			case 'r':
				flags |= FS_MOUNT_READ_ONLY;
				break;
			default:
				printf("Error: invalid arguments\n");
				return 1;
			}
		}

		idx++;
	}

	if((args.size() - idx) < 3 || (args.size() - idx) > 4) {
		printf("Error: invalid arguments\n");
		return 1;
	}

	status_t ret = kern_fs_mount(args[idx].c_str(), args[idx + 1].c_str(),
		args[idx + 2].c_str(), flags,
		((args.size() - idx) == 4) ? args[idx + 3].c_str() : NULL);
	if(ret != STATUS_SUCCESS) {
		printf("Failed to mount filesystem: %d (%s)\n", ret,
			__kernel_status_strings[ret]);
		return 1;
	}

	return 0;
}


static int unmount_command(std::vector<std::string> args) {
	size_t idx = 1;
	uint32_t flags = 0;

	if(args.size() > 1 && args[1][0] == '-') {
		for(size_t i = 1; i < args[1].size(); i++) {
			switch(args[1][i]) {
			case 'f':
				flags |= FS_UNMOUNT_FORCE;
				break;
			default:
				printf("Error: invalid arguments\n");
				return 1;
			}
		}

		idx++;
	}

	if((args.size() - idx) != 1) {
		printf("Error: invalid arguments\n");
		return 1;
	}

	status_t ret = kern_fs_unmount(args[idx].c_str(), flags);
	if(ret != STATUS_SUCCESS) {
		printf("Failed to unmount filesystem: %d (%s)\n", ret,
			__kernel_status_strings[ret]);
		return 1;
	}

	return 0;
}

static int unlink_command(std::vector<std::string> args) {
	if(args.size() != 2) {
		printf("Error: invalid arguments\n");
		return 1;
	}

	status_t ret = kern_fs_unlink(args[1].c_str());
	if(ret != STATUS_SUCCESS) {
		printf("Failed to unlink: %d (%s)\n", ret,
			__kernel_status_strings[ret]);
		return 1;
	}

	return 0;
}

static void split(const std::string &line, std::vector<std::string> &args) {
	args.clear();

	size_t pos = line.find(' ');
	size_t initial_pos = 0;

	while(pos != std::string::npos) {
		if(pos != initial_pos)
			args.push_back(line.substr(initial_pos, pos - initial_pos));
		initial_pos = pos + 1;
		pos = line.find(' ', initial_pos);
	}

	if(line.size() != initial_pos)
		args.push_back(line.substr(initial_pos, line.size() - initial_pos));
}

int main(int argc, char **argv) {
	shell_commands.insert(std::make_pair("cd", cd_command));
	shell_commands.insert(std::make_pair("ln", ln_command));
	shell_commands.insert(std::make_pair("ls", ls_command));
	shell_commands.insert(std::make_pair("mkdir", mkdir_command));
	shell_commands.insert(std::make_pair("mount", mount_command));
	shell_commands.insert(std::make_pair("unlink", unlink_command));
	shell_commands.insert(std::make_pair("unmount", unmount_command));

	printf("\nWelcome to Kiwi!\n\n");

	while(true) {
		char buf[FS_PATH_MAX];
		status_t ret;

		ret = kern_fs_curr_dir(buf, FS_PATH_MAX);
		if(ret == STATUS_SUCCESS) {
			printf("Kiwi:%s> ", buf);
		} else {
			printf("Kiwi> ");
		}

		std::string line;
		while(true) {
			char ch = getchar();

			if(ch == '\n') {
				putchar('\n');
				break;
			} else if(ch == '\b') {
				if(line.size()) {
					line.pop_back();
					putchar('\b');
					putchar(' ');
					putchar('\b');
				}
			} else if(isprint(ch)) {
				line.push_back(ch);
				putchar(ch);
			}
		}

		std::vector<std::string> args;
		split(line, args);
		if(!args.size())
			continue;

		auto it = shell_commands.find(args[0]);
		if(it != shell_commands.end()) {
			it->second(args);
			continue;
		}

		pid_t pid = fork();
		if(pid == 0) {
			char **argv = new char *[args.size() + 1];
			for(size_t i = 0; i < args.size(); i++)
				argv[i] = strdup(args[i].c_str());
			argv[args.size()] = NULL;

			execvp(argv[0], argv);
			printf("Error: failed to execute '%s': %s\n", argv[0], strerror(errno));
			exit(EXIT_FAILURE);
		} else if(pid > 0) {
			wait(NULL);
		} else {
			perror("fork");
			continue;
		}
	}
}
