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

# TODO:
#  - Eventually we should have separate build directories for each different
#    target. We build stuff targeting the host system (build utilities), an
#    architecture and a hardware platform (kernel and loader) and just an
#    architecture (userspace, these are not platform-dependant). Separating
#    these off into separate build directories would prevent unnecessary
#    rebuilds, for example if the hardware platform was changed it would not be
#    necessary to rebuild userspace. I'm not sure how to do this (or even if
#    it's possible) with SCons.

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

#########################
# Internal build setup. #
#########################

import os, sys, SCons.Errors
sys.path = [os.path.abspath(os.path.join('utilities', 'build'))] + sys.path

import vcs
from kconfig import ConfigParser
from toolchain import ToolchainManager

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
		assert self.config.configured()

		env = Environment(platform='posix', ENV=os.environ)
		self._SetupEnvironment(env, target_flags)
		self._MergeFlags(env, flags)

		# Add in extra compilation flags from the configuration.
		if self.config.has_key('ARCH_CCFLAGS'):
			env['CCFLAGS'] += self.config['ARCH_CCFLAGS'].split()
		if self.config.has_key('PLATFORM_CCFLAGS'):
			env['CCFLAGS'] += self.config['PLATFORM_CCFLAGS'].split()
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

# Change the Decider to MD5-timestamp to speed up the build a bit.
Decider('MD5-timestamp')

# Set revision to the VCS revision number.
version['KIWI_VER_REVISION'] = vcs.revision_id()

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
Alias('config', env.ConfigMenu('__config', ['Kconfig']))

# Only do the rest of the build if the configuration exists.
if config.configured() and not 'config' in COMMAND_LINE_TARGETS:
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
