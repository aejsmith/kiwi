# Copyright (C) 2009-2010 Alex Smith
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
# one for the kernel and its modules, etc. we have an EnvironmentManager class
# that manages the creation of new environments from the base environment and
# also acts as a dictionary of environments.
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
		if os.environ.has_key('CC') and os.path.basename(os.environ['CC']) == 'ccc-analyzer':
			self.base['CC'] = os.environ['CC']
			self.base['ENV']['CCC_CC'] = self.GetToolPath('gcc')
		else:
			self.base['CC'] = self.GetToolPath('gcc')
		self.base['CXX'] = self.GetToolPath('g++')
		self.base['AS'] = self.GetToolPath('as')
		self.base['OBJDUMP'] = self.GetToolPath('objdump')
		self.base['READELF'] = self.GetToolPath('readelf')
		self.base['NM'] = self.GetToolPath('nm')
		self.base['STRIP'] = self.GetToolPath('strip')
		self.base['AR'] = self.GetToolPath('ar')
		self.base['RANLIB'] = self.GetToolPath('ranlib')
		self.base['OBJCOPY'] = self.GetToolPath('objcopy')

		# Set paths to build utilities.
		self.base['SYSGEN'] = os.path.join(os.getcwd(), 'utilities', 'sysgen.py')
		self.base['GENSYMTAB'] = os.path.join(os.getcwd(), 'utilities', 'gensymtab.py')
		self.base['BIN2HEX'] = os.path.join(os.getcwd(), 'utilities', 'bin2hex.py')

		# Set compilation flags.
		self.base['CCFLAGS'] = [
			'-Wall', '-Wextra', '-Werror', '-Wcast-align',
			'-Wno-variadic-macros', '-Wno-unused-parameter',
			'-Wwrite-strings', '-Wmissing-declarations',
			'-Wredundant-decls', '-Wno-format', '-gdwarf-2', '-pipe'
		]
		self.base['CFLAGS'] = ['-std=gnu99']
		self.base['CXXFLAGS'] = ['-Wold-style-cast', '-Wsign-promo']
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
		self.base['ARCOMSTR'] = self.GetCompileString('AR', '$TARGET')
		self.base['ASCOMSTR'] = self.GetCompileString('ASM', '$SOURCE')
		self.base['ASPPCOMSTR'] = self.GetCompileString('ASM', '$SOURCE')
		self.base['CCCOMSTR'] = self.GetCompileString('CC', '$SOURCE')
		self.base['SHCCCOMSTR'] = self.GetCompileString('CC', '$SOURCE')
		self.base['CXXCOMSTR'] = self.GetCompileString('CXX', '$SOURCE')
		self.base['SHCXXCOMSTR'] = self.GetCompileString('CXX', '$SOURCE')
		self.base['LINKCOMSTR'] = self.GetCompileString('LINK', '$TARGET')
		self.base['SHLINKCOMSTR'] = self.GetCompileString('SHLINK', '$TARGET')
		self.base['RANLIBCOMSTR'] = self.GetCompileString('RANLIB', '$TARGET')
		self.base['GENCOMSTR'] = self.GetCompileString('GEN', '$TARGET')
		self.base['STRIPCOMSTR'] = self.GetCompileString('STRIP', '$TARGET')

		# Import version information.
		for k, v in version.items():
			self.base[k] = v

	# Gets the full path to a tool in the toolchain.
	def GetToolPath(self, name):
		return os.path.join(self.config['TOOLCHAIN_DIR'], \
		                    self.config['TOOLCHAIN_TARGET'], 'bin', \
		                    self.config['TOOLCHAIN_TARGET'] + "-" + name)

	# Get a string to use for a compilation string.
	def GetCompileString(self, msg, name):
		if self.verbose:
			return None
		elif self.colour:
			return ' \033[0;32m%-6s\033[0m %s' % (msg, name)
		else:
			return ' %-6s %s' % (msg, name)

	# Create a new build environment.
	def Create(self, name, flags=None):
		return self.Clone(name, self.base, flags)

	# Create a new environment based on an existing environment.
	def Clone(self, name, base, flags=None):
		if type(base) == str:
			base = self[base]
		elif type(base) != type(self.base):
			raise ValueError, 'Invalid base specified.'
		self[name] = base.Clone()
		if flags:
			# MergeFlags only handles lists. Add anything else
			# manually.
			merge = {}
			for (k, v) in flags.items():
				if type(v) == list:
					if base.has_key(k):
						merge[k] = v
					else:
						self[name][k] = v
				elif type(v) == dict and self[name].has_key(k) and type(self[name][k]) == dict:
					self[name][k].update(v)
				else:
					self[name][k] = v
			self[name].MergeFlags(merge)
		return self[name]
