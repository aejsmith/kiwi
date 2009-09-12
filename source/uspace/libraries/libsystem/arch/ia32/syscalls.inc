# Kiwi IA32 system call definitions
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
Type('offset_t', 2)
Type('file_size_t', 2)
Type('identifier_t', 1)
Type('timeout_t', 2)

# System call function generator.
@SyscallGenerator
def SyscallGeneratorFunc(stream, num, name, count):
	# Write out the common part for all system calls.
	stream.write('.global %s\n' % (name))
	stream.write('.type %s, @function\n' % (name))
	stream.write('%s:\n' % (name))
	stream.write('	movl	$0x%x, %%eax\n' % (num))

	# Now the difficult bit - handling different numbers of arguments. We
	# use callee-save registers for some things so must save them. Create
	# a list of pushed registers so we can write out pops for them after
	# without having to do a case for each number of arguments. The list is
	# in the order to be popped in.
	pushed = []
	if count == 1 or count == 2:
		stream.write('	movl	4(%esp), %edx\n')
		if count == 2:
			stream.write('	movl	8(%esp), %ecx\n')
	elif count == 3:
		pushed = ['%edi']
		stream.write('	push	%edi\n')
		stream.write('	movl	8(%esp), %edx\n')
		stream.write('	movl	12(%esp), %ecx\n')
		stream.write('	movl	16(%esp), %edi\n')
	elif count == 4:
		pushed = ['%esi', '%edi']
		stream.write('	push	%edi\n')
		stream.write('	push	%esi\n')
		stream.write('	movl	12(%esp), %edx\n')
		stream.write('	movl	16(%esp), %ecx\n')
		stream.write('	movl	20(%esp), %edi\n')
		stream.write('	movl	24(%esp), %esi\n')
	elif count == 5:
		pushed = ['%ebx', '%esi', '%edi']
		stream.write('	push	%edi\n')
		stream.write('	push	%esi\n')
		stream.write('	push	%ebx\n')
		stream.write('	movl	16(%esp), %edx\n')
		stream.write('	movl	20(%esp), %ecx\n')
		stream.write('	movl	24(%esp), %edi\n')
		stream.write('	movl	28(%esp), %esi\n')
		stream.write('	movl	32(%esp), %ebx\n')
	elif count == 6:
		pushed = ['%ebp', '%ebx', '%esi', '%edi']
		stream.write('	push	%edi\n')
		stream.write('	push	%esi\n')
		stream.write('	push	%ebx\n')
		stream.write('	push	%ebp\n')
		stream.write('	movl	20(%esp), %edx\n')
		stream.write('	movl	24(%esp), %ecx\n')
		stream.write('	movl	28(%esp), %edi\n')
		stream.write('	movl	32(%esp), %esi\n')
		stream.write('	movl	36(%esp), %ebx\n')
		stream.write('	movl	40(%esp), %ebp\n')

	# Perform the actual system call, pop pushed registers and return.
	stream.write('	int	$0x80\n')
	for reg in pushed:
		stream.write('	pop	%s\n' % (reg))
	stream.write('	ret\n')
	stream.write('.size %s, .-%s\n\n' % (name, name))
