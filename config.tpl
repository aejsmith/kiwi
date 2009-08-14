# Kiwi build configuration template
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

#######
Section('Basic build configuration')
#######

Option('EXTRA_CCFLAGS', 'Extra compiler options for both C and C++ sources.', '-O2')
Option('EXTRA_CFLAGS', 'Extra compiler options for C sources.', '')
Option('EXTRA_CXXFLAGS', 'Extra compiler options for C++ sources.', '')

Option('QEMU_BINARY', 'Path to QEMU binary to use for the qtest target.', 'qemu')
Option('QEMU_OPTS', 'Extra options to pass to QEMU.', '-serial stdio -vga std')

Option('TOOLCHAIN_DIR', 'Directory to store toolchain builds in.', '/please/change/me')
Option('TOOLCHAIN_MAKE_JOBS', 'Argument to pass to -j for make when building toolchain.', 1)

#######
Section('Architecture selection')
#######

Choice('ARCH', 'Architecture to compile for.', (
	('amd64', '64-bit AMD64/Intel 64 CPUs.'),
	('ia32', '32-bit Intel/AMD CPUs.'),
), 'amd64')
Choice('PLATFORM', 'Platform to compile for.', (
	('pc', 'Standard PC.', {
		'ARCH': lambda x: x in ['ia32', 'amd64'],
	}),
), 'pc')

#######
Section('IA32/AMD64-specific options', {'ARCH': lambda x: x in ['ia32', 'amd64']})
#######

Choice('X86_SERIAL_PORT', 'Serial port to use for kernel console output.', (
	(0, 'Disabled.'),
	(1, 'COM1.'),
	(2, 'COM2.'),
	(3, 'COM3.'),
	(4, 'COM4.'),
), 1)
Option('X86_NX', 'Use No-Execute/Execute-Disable support.', True)
Option('X86_OPTIM_STR', 'Use optimized string functions.', True)

#######
Section('Kernel configuration')
#######

Option('HANDLE_MAX', 'Maximum number of open handles for a process.', 1024)

#######
Section('Kernel debug output')
#######

Option('DEBUG', 'Basic debugging output.', True)
Option('PAGE_DEBUG', 'Physical memory manager debug output (VERY excessive).', False, {'DEBUG': lambda x: x})
Option('VMEM_DEBUG', 'Vmem allocator debug output.', False, {'DEBUG': lambda x: x})
Option('KHEAP_DEBUG', 'Kernel heap allocator debug output.', False, {'DEBUG': lambda x: x})
Option('SLAB_DEBUG', 'Slab allocator debug output.', False, {'DEBUG': lambda x: x})
Option('VM_DEBUG', 'Virtual memory manager debug output.', False, {'DEBUG': lambda x: x})
Option('PROC_DEBUG', 'Process/thread management debug output.', False, {'DEBUG': lambda x: x})
Option('SCHED_DEBUG', 'Scheduler debug output (VERY excessive).', False, {'DEBUG': lambda x: x})
Option('MODULE_DEBUG', 'Module loader debug output.', False, {'DEBUG': lambda x: x})
Option('HANDLE_DEBUG', 'Handle manager debug output (VERY excessive).', False, {'DEBUG': lambda x: x})
Option('VFS_DEBUG', 'VFS debugging output.', False, {'DEBUG': lambda x: x})

#######
#Section('Module configuration')
#######

#####################################
# Configuration validation function #
#####################################

@PostConfig
def PostConfigFunc(config):
	config['ARCH_%s' % config['ARCH'].upper()] = True
	if config['ARCH'] == 'ia32' or config['ARCH'] == 'amd64':
		if config['ARCH'] == 'ia32':
			config['TOOLCHAIN_TARGET'] = 'i686-kiwi'
			config['ARCH_32BIT'] = True
		elif config['ARCH'] == 'amd64':
			config['TOOLCHAIN_TARGET'] = 'x86_64-kiwi'
			config['ARCH_64BIT'] = True

		config['ARCH_LITTLE_ENDIAN'] = True
		if config['X86_OPTIM_STR']:
			config['ARCH_HAS_MEMCPY'] = True
			config['ARCH_HAS_MEMSET'] = True
			config['ARCH_HAS_MEMMOVE'] = True
