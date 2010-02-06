#!/usr/bin/env python
#
# Copyright (C) 2009 Alex Smith
#
# Kiwi is open source software, released under the terms of the Non-Profit
# Open Software License 3.0. You should have received a copy of the
# licensing information along with the source code distribution. If you
# have not received a copy of the license, please refer to the Kiwi
# project website.
#
# Please note that if you modify this file, the license requires you to
# ADD your name to the list of contributors. This boilerplate is not the
# license itself; please refer to the copy of the license you have received
# for complete terms.

import sys, os

# System call list parser class.
class SyscallParser:
	def __init__(self):
		# Initialize variables.
		self.max_arguments = None
		self.generator_func = None
		self.types = {}
		self.syscalls = []
		self.curr_service = None
		self.curr_num = None

	# Parses a provided input file.
	def parse(self, filename):
		# Create a dictionary of directives that the script can use.
		directives = {
			'MaxArguments': self.MaxArgumentsDirective,
			'Type': self.TypeDirective,
			'SyscallGenerator': self.SyscallGeneratorDirective,
			'Service': self.ServiceDirective,
			'Syscall': self.SyscallDirective,
		}

		# Parse the file.
		execfile(filename, directives, None)

	# Write out all defined system calls.
	def write(self, stream):
		# Check if we have a generator function.
		if not self.generator_func:
			raise Exception, 'No generator function was specified in any input file.'

		# Write out all of the defined system calls.
		for call in self.syscalls:
			self.generator_func(stream, call[0], call[1], self.get_argc(call[1], call[2]))

	# Get the number of arguments taken by a type.
	def get_type_argc(self, name, t):
		try:
			return self.types[t]
		except KeyError:
			raise Exception, "Unknown argument type '%s' for '%s'." % (t, name)

	# Get the number of arguments taken by a list of types.
	def get_argc(self, name, types):
		ret = 0
		for t in types:
			ret += self.get_type_argc(name, t)

		# Check whether we support this number of arguments
		if self.max_arguments == None:
			raise Exception, 'A maximum argument count was not specified.'
		elif ret > self.max_arguments:
			raise Exception, "Number of arguments for '%s' (%d) greater than maximum." % (name, ret)
		return ret

	###############
	# Directives. #
	###############

	# Set the maximum supported number of arguments.
	def MaxArgumentsDirective(self, args):
		if type(args) != int:
			raise Exception, 'Invalid argument type to MaxArguments.'
		self.max_arguments = args

	# Define a new type.
	def TypeDirective(self, name, args):
		if type(name) != str or type(args) != int:
			raise Exception, 'Invalid argument type to Type.'
		elif self.types.has_key(name):
			raise Exception, "Defining already defined type '%s'" % (name)
		self.types[name] = args

	# Set the system call generator function.
	def SyscallGeneratorDirective(self, func):
		self.generator_func = func

	# Begin definition of a service.
	def ServiceDirective(self, num):
		if type(num) != int:
			raise Exception, 'Invalid argument type to Service.'
		self.curr_service = num
		self.curr_num = 0

	# Define a system call in the current service.
	def SyscallDirective(self, name, args, num=None):
		if type(name) != str or type(args) != list:
			raise Exception, 'Invalid argument type to Syscall.'
		elif self.curr_service == None:
			raise Exception, "Attempt to define system call '%s' outside of Service." % (name)

		if num:
			self.syscalls.append(((self.curr_service << 16) + num, name, args))
			self.curr_num = num + 1
		else:
			self.syscalls.append(((self.curr_service << 16) + self.curr_num, name, args))
			self.curr_num += 1

# Main function for the system call parser.
def main():
	# Check arguments.
	if len(sys.argv) < 3:
		print "Usage: %s <output-file> <input-1> [<input-2>...]" % (sys.argv[0])
		return 1

	# Parse all the inputs in the order provided.
	parser = SyscallParser()
	for i in range(2, len(sys.argv)):
		parser.parse(sys.argv[i])

	# Write out all system calls.
	stream = open(sys.argv[1], 'w')
	stream.write('/* This file is automatically generated. Do not edit. */\n\n')
	parser.write(stream)
	stream.close()
	return 0

if __name__ == '__main__':
	sys.exit(main())
