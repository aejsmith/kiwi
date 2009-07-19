# Kiwi AMD64 system call definitions
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

# Maximum number of system call arguments allowed.
MaxArguments(6)

# Type definitions.
Type('int', 1)
Type('char', 1)
Type('bool', 1)
Type('ptr_t', 1)
Type('size_t', 1)
Type('handle_t', 1)
Type('offset_t', 1)
Type('file_size_t', 1)

# System call function generator.
@SyscallGenerator
def SyscallGeneratorFunc(stream, num, name, count):
	# Code is same regardless of argument count for AMD64.
	stream.write('.global %s\n' % (name))
	stream.write('.type %s, @function\n' % (name))
	stream.write('%s:\n' % (name))
	stream.write('	movq	%rcx, %r10\n')
	stream.write('	movq	$0x%x, %%rax\n' % (num))
	stream.write('	syscall\n')
	stream.write('	ret\n')
	stream.write('.size %s, .-%s\n\n' % (name, name))
