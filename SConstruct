#
# Copyright (C) 2009-2011 Alex Smith
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

# Release information.
version = {
	'KIWI_VER_RELEASE': 0,
	'KIWI_VER_UPDATE': 0,
	'KIWI_VER_REVISION': 0,
}

# C/C++ warning flags.
cc_warning_flags = [
	'-Wall', '-Wextra', '-Werror', '-Wno-variadic-macros',
	'-Wno-unused-parameter', '-Wwrite-strings', '-Wmissing-declarations',
	'-Wredundant-decls', '-Wno-format',
]

# C++ warning flags.
cxx_warning_flags = [
	'-Wold-style-cast', '-Wsign-promo',
]

# Variables to set in host environments. Don't build C code with our normal
# warning flags, Kconfig and Flex/Bison code won't compile with them. Also
# older host G++ versions don't support some flags.
host_flags = {
	'CCFLAGS': ['-pipe'],
	'CFLAGS': ['-std=gnu99'],
	'CXXFLAGS': filter(lambda f: f not in ['-Wmissing-declarations', '-Wno-variadic-macros'], cc_warning_flags),
	'YACCFLAGS': ['-d'],
}

# Variables to set in target environments.
target_flags = {
	'CCFLAGS': cc_warning_flags + ['-gdwarf-2', '-pipe'],
	'CFLAGS': ['-std=gnu99'],
	'CXXFLAGS': cxx_warning_flags,
	'ASFLAGS': ['-D__ASM__'],

	# Set correct shared library link flags.
	'SHCCFLAGS': '$CCFLAGS -fPIC -DSHARED',
	'SHLINKFLAGS': '$LINKFLAGS -shared -Wl,-soname,${TARGET.name}',

	# Override default assembler - it uses as directly, we want to use GCC.
	'ASCOM': '$CC $_CCCOMCOM $ASFLAGS -c -o $TARGET $SOURCES',
}

# Architecture specific flags.
arch_target_flags = {
	'ia32': {
		'CCFLAGS': ['-march=i686']
	}
}

#########################
# Internal build setup. #
#########################

import os
import sys
import SCons.Errors
from utilities.toolchain import ToolchainManager

# Class to parse a Kconfig configuration file.
class ConfigParser(dict):
	def __init__(self, path):
		dict.__init__(self)

		# Parse the configuration file. If it doesn't exist, just
		# return - the dictionary will be empty so Configured() will
		# return false.
		try:
			f = open(path, 'r')
		except IOError:
			return

		# Read and parse the file contents. We return without adding
		# any values if there is a parse error, this will cause
		# Configured() to return false and require the user to reconfig.
		lines = f.readlines()
		f.close()
		values = {}
		for line in lines:
			line = line.strip()

			# Ignore blank lines or comments.
			if not len(line) or line[0] == '#':
				continue

			# Split the line into key/value.
			line = line.split('=', 1)
			if len(line) != 2:
				return
			key = line[0].strip()
			value = line[1].strip()
			if len(key) < 8 or key[0:7] != 'CONFIG_' or not len(value):
				return
			key = line[0].strip()[7:]

			# Work out the correct value.
			if value == 'y':
				value = True
			elif value[0] == '"' and value[-1] == '"':
				value = value[1:-1]
			elif value[0:2] == '0x' and len(value) > 2:
				value = int(value, 16)
			elif value.isdigit():
				value = int(value)
			else:
				print "Unrecognised value type: %s" % (value)
				return

			# Add it to the dictionary.
			values[key] = value

		# Everything was OK, add stuff into the real dictionary.
		for (k, v) in values.items():
			self[k] = v

	# Get a configuration value. This returns None for any accesses to
	# undefined keys.
	def __getitem__(self, key):
		try:
			return dict.__getitem__(self, key)
		except KeyError:
			return None

	# Check whether the build configuration exists.
	def Configured(self):
		return len(self) > 0

# Class for build environment management. Because we have several build
# environments, this class acts like a dictionary of environments, and assists
# in the creation of new ones.
class EnvironmentManager(dict):
	def __init__(self, verbose, config):
		dict.__init__(self)
		self.config = config

		# Create compile strings that will be added to all environments.
		verbose = ARGUMENTS.get('V') == '1'
		def CompileString(msg, name):
			if verbose:
				return None
			return ' \033[0;32m%-6s\033[0m %s' % (msg, name)
		self.variables = {
			'ARCOMSTR':     CompileString('AR',     '$TARGET'),
			'ASCOMSTR':     CompileString('ASM',    '$SOURCE'),
			'ASPPCOMSTR':   CompileString('ASM',    '$SOURCE'),
			'CCCOMSTR':     CompileString('CC',     '$SOURCE'),
			'SHCCCOMSTR':   CompileString('CC',     '$SOURCE'),
			'CXXCOMSTR':    CompileString('CXX',    '$SOURCE'),
			'SHCXXCOMSTR':  CompileString('CXX',    '$SOURCE'),
			'YACCCOMSTR':   CompileString('YACC',   '$SOURCE'),
			'LEXCOMSTR':    CompileString('LEX',    '$SOURCE'),
			'LINKCOMSTR':   CompileString('LINK',   '$TARGET'),
			'SHLINKCOMSTR': CompileString('SHLINK', '$TARGET'),
			'RANLIBCOMSTR': CompileString('RANLIB', '$TARGET'),
			'GENCOMSTR':    CompileString('GEN',    '$TARGET'),
			'STRIPCOMSTR':  CompileString('STRIP',  '$TARGET'),
		}

		# Create an array of builders that will be added to all
		# environments.
		self.builders = {
			'LDScript': Builder(action=Action(
				'$CC $_CCCOMCOM $ASFLAGS -E -x c $SOURCE | grep -v "^\#" > $TARGET',
				'$GENCOMSTR'
			)),
		}

	# Merge flags into an environment.
	def _MergeFlags(self, env, flags):
		if not flags:
			return

		# The MergeFlags function in Environment only handles
		# lists. Add anything else manually.
		merge = {}
		for (k, v) in flags.items():
			if type(v) == list:
				if env.has_key(k):
					merge[k] = v
				else:
					env[k] = v
			elif type(v) == dict and env.has_key(k) and type(env[k]) == dict:
				env[k].update(v)
			else:
				env[k] = v
		env.MergeFlags(merge)

	# Perform common setup for an environment.
	def _SetupEnvironment(self, env, flags):
		# Add variables/builders.
		for (k, v) in self.variables.items():
			env[k] = v
		for (k, v) in self.builders.items():
			env['BUILDERS'][k] = v
		for (k, v) in flags.items():
			env[k] = v

	# Add a variable to all environments and all future environments.
	def AddVariable(self, name, value):
		self.variables[name] = value
		for (k, v) in self.items():
			self[k][name] = value

	# Add a builder to all environments and all future environments.
	def AddBuilder(self, name, builder):
		self.builders[name] = builder
		for (k, v) in self.items():
			self[k]['BUILDERS'][name] = builder

	# Add a build tool to all environments and all future environments.
	def AddTool(self, name, depends, act):
		if type(depends) != list:
			depends = [depends]
		def dep_emitter(target, source, env):
			for dep in depends:
				Depends(target, dep)
			return (target, source)
		self.AddBuilder(name, Builder(action=act, emitter=dep_emitter))

	# Create an environment for building for the host system.
	def CreateHost(self, name, flags=None):
		env = Environment(ENV=os.environ)
		self._SetupEnvironment(env, host_flags)
		self._MergeFlags(env, flags)
		self[name] = env
		return env

	# Create an environment for building for the target system. This
	# requires that the configuration has been set up correctly.
	def Create(self, name, flags=None):
		assert self.config.Configured()

		env = Environment(platform='posix', ENV=os.environ)
		self._SetupEnvironment(env, target_flags)
		if arch_target_flags.has_key(self.config['ARCH']):
			self._MergeFlags(env, arch_target_flags[self.config['ARCH']])
		self._MergeFlags(env, flags)

		# Add in extra compilation flags from the configuration.
		env['CCFLAGS'] += self.config['EXTRA_CCFLAGS'].split()
		env['CFLAGS'] += self.config['EXTRA_CFLAGS'].split()
		env['CXXFLAGS'] += self.config['EXTRA_CXXFLAGS'].split()

		# If doing a debug build, set -fno-omit-frame-pointer.
		if config['DEBUG']:
			env['CCFLAGS'] += ['-fno-omit-frame-pointer']

		# Set paths to toolchain components.
		def ToolPath(name):
			return os.path.join(
				self.config['TOOLCHAIN_DIR'],
				self.config['TOOLCHAIN_TARGET'], 'bin',
				self.config['TOOLCHAIN_TARGET'] + "-" + name
			)
		if os.environ.has_key('CC') and os.path.basename(os.environ['CC']) == 'ccc-analyzer':
			env['CC'] = os.environ['CC']
			env['ENV']['CCC_CC'] = ToolPath('gcc')
		else:
			env['CC'] = ToolPath('gcc')
		if os.environ.has_key('CXX') and os.path.basename(os.environ['CXX']) == 'c++-analyzer':
			env['CXX'] = os.environ['CXX']
			env['ENV']['CCC_CXX'] = ToolPath('g++')
			print env['CXX']
		else:
			env['CXX'] = ToolPath('g++')
		env['AS']      = ToolPath('as')
		env['OBJDUMP'] = ToolPath('objdump')
		env['READELF'] = ToolPath('readelf')
		env['NM']      = ToolPath('nm')
		env['STRIP']   = ToolPath('strip')
		env['AR']      = ToolPath('ar')
		env['RANLIB']  = ToolPath('ranlib')
		env['OBJCOPY'] = ToolPath('objcopy')
		env['LD']      = ToolPath('ld')

		self._MergeFlags(env, flags)
		self[name] = env
		return env

	# Create a new environment based on an existing environment.
	def Clone(self, name, base, flags=None):
		self[name] = self[base].Clone()
		self._MergeFlags(self[name], flags)
		return self[name]

# Raise an error if a certain target is not specified.
def RequireTarget(target, error):
	if GetOption('help') or target in COMMAND_LINE_TARGETS:
		return
	raise SCons.Errors.StopError(error)

# If working from the Mercurial tree then set revision to the revision number.
try:
	from mercurial import ui, hg
	version['KIWI_VER_REVISION'] = hg.repository(ui.ui(), '.')['tip'].rev()
except:
	pass

# Set the version string.
version['KIWI_VER_STRING'] = '%d.%d.%d' % (
	version['KIWI_VER_RELEASE'],
	version['KIWI_VER_UPDATE'],
	version['KIWI_VER_REVISION']
)

# Create the configuration parser and environment manager.
config = ConfigParser('.config')
envmgr = EnvironmentManager(ARGUMENTS.get('V') == '1', config)
Export('config', 'envmgr', 'version')

# Create the host environment and get targets for build utilities.
env = envmgr.CreateHost('host')
SConscript('utilities/SConscript', variant_dir=os.path.join('build', 'host'), exports=['env'])

# Add targets to run the configuration interface.
env['ENV']['KERNELVERSION'] = version['KIWI_VER_STRING']
Alias('config', env.ConfigMenu('config', ['Kconfig']))

# Only do the rest of the build if the configuration exists.
if config.Configured() and not 'config' in COMMAND_LINE_TARGETS:
	# Initialise the toolchain manager and add the toolchain build target.
	toolchain = ToolchainManager(config)
	Alias('toolchain', Command('__toolchain', [], Action(toolchain.update, None)))

	# If the toolchain is out of date, only allow it to be built.
	if toolchain.check() != 0:
		RequireTarget('toolchain', "Toolchain out of date. Update using the 'toolchain' target.")
	else:
		SConscript('SConscript', variant_dir=os.path.join('build', '%s-%s' % (config['ARCH'], config['PLATFORM'])))
else:
	# Configuration does not exist. All we can do is configure.
	RequireTarget('config', "Configuration missing or out of date. Please update using 'config' target.")

# Change the Decider to MD5-timestamp to speed up the build a bit.
Decider('MD5-timestamp')
