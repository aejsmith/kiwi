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
Option('USE_CLANG', 'Use Clang/LLVM to build the kernel.', False)

Option('EXTRA_FSIMAGE', 'Path to a directory containing extra FS image files.', '')

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

#######
Section('Kernel configuration')
#######

Option('HANDLE_MAX', 'Maximum number of open handles for a process.', 1024)
Option('KLOG_SIZE', 'Size of the kernel log buffer in characters.', 16384)

#######
Section('Kernel debugging')
#######

Option('SLAB_STATS', 'Slab allocator statistics.', False)
Option('DEBUG', 'Basic debugging output.', True)
Option('TRACE_SYSCALLS', 'Print out details of every system call on the debug console.', False, {'DEBUG': lambda x: x})
Option('PAGE_DEBUG', 'Physical memory manager debug output (VERY excessive).', False, {'DEBUG': lambda x: x})
Option('VMEM_DEBUG', 'Vmem allocator debug output.', False, {'DEBUG': lambda x: x})
Option('KHEAP_DEBUG', 'Kernel heap allocator debug output.', False, {'DEBUG': lambda x: x})
Option('SLAB_DEBUG', 'Slab allocator debug output.', False, {'DEBUG': lambda x: x})
Option('VM_DEBUG', 'Virtual memory manager debug output.', False, {'DEBUG': lambda x: x})
Option('PROC_DEBUG', 'Process/thread management debug output.', False, {'DEBUG': lambda x: x})
Option('SCHED_DEBUG', 'Scheduler debug output (VERY excessive).', False, {'DEBUG': lambda x: x})
Option('MODULE_DEBUG', 'Module loader debug output.', False, {'DEBUG': lambda x: x})
Option('OBJECT_DEBUG', 'Object manager debug output (VERY excessive).', False, {'DEBUG': lambda x: x})
Option('FS_DEBUG', 'Filesystem debugging output.', False, {'DEBUG': lambda x: x})
Option('CACHE_DEBUG', 'Page cache debugging output.', False, {'DEBUG': lambda x: x})
Option('DEVICE_DEBUG', 'Device manager debugging output.', False, {'DEBUG': lambda x: x})
Option('IPC_DEBUG', 'IPC debugging output.', False, {'DEBUG': lambda x: x})

#######
Section('Module configuration')
#######

Option('MODULE_FS_EXT2', 'Ext2 filesystem module.', True)
Option('MODULE_FS_EXT2_DEBUG', 'Debugging output from the Ext2 driver.', False, {
	'DEBUG': lambda x: x,
	'MODULE_FS_EXT2': lambda x: x,
})
Option('MODULE_PLATFORM_BIOS', 'PC BIOS interrupt interface.', True, {'PLATFORM': lambda x: x in ['pc']})

#######
Section('Driver configuration')
#######

Option('DRIVER_BUS_PCI', 'PCI bus manager.', True)
Option('DRIVER_DISK_ATA', 'Generic ATA device driver.', True, {'DRIVER_BUS_PCI': lambda x: x})
Option('DRIVER_DISPLAY_VBE', 'VBE display device driver.', True, {'MODULE_PLATFORM_BIOS': lambda x: x})
Option('DRIVER_INPUT_I8042', 'i8042 keyboard/mouse port driver.', True)

#####################################
# Configuration validation function #
#####################################

@PostConfig
def PostConfigFunc(config):
	config['ARCH_%s' % config['ARCH'].upper()] = True
	config['SRCARCH'] = config['ARCH']
	if config['ARCH'] == 'ia32' or config['ARCH'] == 'amd64':
		config['SRCARCH'] = 'x86'
		if config['ARCH'] == 'ia32':
			config['TOOLCHAIN_TARGET'] = 'i686-kiwi'
			config['ARCH_32BIT'] = True
		elif config['ARCH'] == 'amd64':
			config['TOOLCHAIN_TARGET'] = 'x86_64-kiwi'
			config['ARCH_64BIT'] = True

		config['ARCH_LITTLE_ENDIAN'] = True
