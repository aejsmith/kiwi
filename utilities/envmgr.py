# Kiwi build system
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

import os
import sys

from SCons.Script import *

# This is a pretty funky way of doing things, but hey, it works and I like it.
# Because we require several different build environments - one for userspace,
# one for drivers, one for the kernel and its modules, etc. we have an
# EnvironmentManager class that manages the creation of new environments from
# the base environment and also acts as a dictionary of environments.
class EnvironmentManager(dict):
	# Create the base environment that others are based off.
	def __init__(self, config, version):
		dict.__init__(self)

		self.config = config
		self.verbose = ARGUMENTS.get('V') == '1'
		self.colour = ARGUMENTS.get('NO_COLOUR') != '1'

		# Create the base environment.
		self.base = Environment(platform='posix', ENV=os.environ)

		# Set paths to toolchain components.
		self.base['CC'] = self.get_tool_path('gcc')
		self.base['CXX'] = self.get_tool_path('g++')
		self.base['AS'] = self.get_tool_path('as')
		self.base['OBJDUMP'] = self.get_tool_path('objdump')
		self.base['READELF'] = self.get_tool_path('readelf')
		self.base['NM'] = self.get_tool_path('nm')
		self.base['STRIP'] = self.get_tool_path('strip')
		self.base['AR'] = self.get_tool_path('ar')
		self.base['RANLIB'] = self.get_tool_path('ranlib')
		self.base['OBJCOPY'] = self.get_tool_path('objcopy')

		# Set paths to build utilities.
		self.base['SYSGEN'] = os.path.join(os.getcwd(), 'utilities', 'sysgen.py')
		self.base['GENSYMTAB'] = os.path.join(os.getcwd(), 'utilities', 'gensymtab.py')
		self.base['BIN2HEX'] = os.path.join(os.getcwd(), 'utilities', 'bin2hex.py')

		# Set compilation flags.
		self.base['CCFLAGS'] = [
			'-Wall', '-Wextra', '-Werror', '-Wcast-align',
			'-Wno-variadic-macros', '-Wno-unused-parameter',
			'-Wwrite-strings', '-Wmissing-declarations',
			'-Wredundant-decls', '-Wno-format', '-g', '-pipe'
		]
		self.base['CFLAGS'] = ['-std=gnu99']
		self.base['CXXFLAGS'] = []
		self.base['ASFLAGS'] = ['-D__ASM__']

		# Add in extra compilation flags from the configuration.
		self.base['CCFLAGS'] += self.config['EXTRA_CCFLAGS'].split()
		self.base['CFLAGS'] += self.config['EXTRA_CFLAGS'].split()
		self.base['CXXFLAGS'] += self.config['EXTRA_CXXFLAGS'].split()

		# Set shared library compilation flags.
		self.base['SHCCFLAGS'] = '$CCFLAGS -fPIC -DSHARED'
		self.base['SHLINKFLAGS'] = '$LINKFLAGS -shared -Wl,-soname,${TARGET.name}'

		# Override the default assembler - it uses as directly, we want to use GCC.
		self.base['ASCOM'] = '$CC $_CCCOMCOM $ASFLAGS -c -o $TARGET $SOURCES'

		# Make the build quiet if we haven't been told to make it verbose.
		self.base['ARCOMSTR'] = self.get_compile_str('Creating archive:', '$TARGET')
		self.base['ASCOMSTR'] = self.get_compile_str('Compiling ASM source:', '$SOURCE')
		self.base['ASPPCOMSTR'] = self.get_compile_str('Compiling ASM source:', '$SOURCE')
		self.base['CCCOMSTR'] = self.get_compile_str('Compiling C source:', '$SOURCE')
		self.base['SHCCCOMSTR'] = self.get_compile_str('Compiling C source:', '$SOURCE')
		self.base['CXXCOMSTR'] = self.get_compile_str('Compiling C++ source:', '$SOURCE')
		self.base['SHCXXCOMSTR'] = self.get_compile_str('Compiling C++ source:', '$SOURCE')
		self.base['LINKCOMSTR'] = self.get_compile_str('Linking:', '$TARGET')
		self.base['SHLINKCOMSTR'] = self.get_compile_str('Linking (shared):', '$TARGET')
		self.base['RANLIBCOMSTR'] = self.get_compile_str('Indexing archive:', '$TARGET')
		self.base['GENCOMSTR'] = self.get_compile_str('Generating:', '$TARGET')
		self.base['STRIPCOMSTR'] = self.get_compile_str('Stripping:', '$TARGET')

		# Import version information.
		for k, v in version.items():
			self.base[k] = v

	# Gets the full path to a tool in the toolchain.
	def get_tool_path(self, name):
		return os.path.join(self.config['TOOLCHAIN_DIR'], \
		                    self.config['TOOLCHAIN_TARGET'], 'bin', \
		                    self.config['TOOLCHAIN_TARGET'] + "-" + name)

	# Get a string to use for a compilation string.
	def get_compile_str(self, msg, name):
		if not self.verbose:
			if self.colour:
				return '\033[0;32m>>>\033[0;1m %-21s \033[0;0m %s\033[0m' % (msg, name)
			else:
				return '>>> %-21s %s' % (msg, name)
		else:
			return None

	# Create a new environment based on an existing environment.
	def Create(self, name, base=None):
		if not base:
			base = self.base
		self[name] = base.Clone()
		return self[name]
