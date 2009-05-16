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
from UserDict import UserDict

#################################
# Command-line option handling. #
#################################

verbose = ARGUMENTS.get('V') == '1'
colour = ARGUMENTS.get('NO_COLOUR') != '1'

########################
# Version information. #
########################

# Release information.
version = {
	'KIWI_VER_RELEASE': 0,
	'KIWI_VER_CODENAME': 'Stewie',
	'KIWI_VER_UPDATE': 0,
	'KIWI_VER_REVISION': 0,
}

# If we're working from the Bazaar tree then set revision to the revision
# number.
try:
        from bzrlib import branch
        b = branch.Branch.open(os.getcwd())
        revno, revid = b.last_revision_info()

	version['KIWI_VER_REVISION'] = revno
except:
	pass

########################
# Build configuration. #
########################

# Pull in the build configuration.
with open('build.conf') as f:
	exec f

############################
# Toolchain configuration. #
############################

if os.system('toolchain/build.py --check') != 0:
	if colour:
		sys.stdout.write('\033[1;31mERROR:\033[0m ')
	else:
		sys.stdout.write('ERROR: ')
	print "Your toolchain is out of date. Please ensure that build.conf is"
	print "         correct and update it by running toolchain/build.py"
	Exit(1)

#######################
# Build environments. #
#######################

# This is a pretty funky way of doing things, but hey, it works and I like it.
# Because we require several different build environments - one for each
# application subsystem, one for drivers, one for the kernel and its modules,
# etc. - we have an EnvironmentManager class that manages the creation of new
# environments from the base environment and also acts as a dictionary of
# environments.
class EnvironmentManager(UserDict):
	# Create the base environment that others are based off.
	def __init__(self, verbose, colour):
		UserDict.__init__(self)

		self.verbose = verbose
		self.colour = colour

		# Create the base environment.
		self.base = Environment(ENV = os.environ)

		# Set paths to toolchain components.
		self.base['CC']      = self.get_tool_path('gcc')
		self.base['CXX']     = self.get_tool_path('g++')
		self.base['AS']      = self.get_tool_path('as')
		self.base['LINK']    = self.get_tool_path('ld')
		self.base['OBJDUMP'] = self.get_tool_path('objdump')
		self.base['READELF'] = self.get_tool_path('readelf')
		self.base['NM']      = self.get_tool_path('nm')
		self.base['STRIP']   = self.get_tool_path('strip')
		self.base['AR']      = self.get_tool_path('ar')
		self.base['RANLIB']  = self.get_tool_path('ranlib')
		self.base['OBJCOPY'] = self.get_tool_path('objcopy')

		# Set compilation flags.
		self.base['CCFLAGS']  = '-Wall -Wextra -Werror ' + \
		          '-Wcast-align -Wno-variadic-macros ' + \
		          '-Wno-unused-parameter -Wwrite-strings ' + \
		          '-Wmissing-declarations -Wredundant-decls ' + \
		          '-Wno-format -g ' + config['EXTRA_CCFLAGS']
		self.base['CFLAGS']   = '-std=gnu99 ' + config['EXTRA_CFLAGS']
		self.base['CXXFLAGS'] = config['EXTRA_CXXFLAGS']

		# Override the default assembler - it uses as directly, we want to use GCC.
		self.base['ASCOM']    = '$CC $_CCCOMCOM $ASFLAGS -c -o $TARGET $SOURCES'

		# Make the build quiet if we haven't been told to make it verbose.
		self.base['ARCOMSTR']     = self.get_compile_str('Creating archive:', '$TARGET')
		self.base['ASCOMSTR']     = self.get_compile_str('Compiling ASM source:', '$SOURCE')
		self.base['ASPPCOMSTR']   = self.get_compile_str('Compiling ASM source:', '$SOURCE')
		self.base['CCCOMSTR']     = self.get_compile_str('Compiling C source:', '$SOURCE')
		self.base['CXXCOMSTR']    = self.get_compile_str('Compiling C++ source:', '$SOURCE')
		self.base['LINKCOMSTR']   = self.get_compile_str('Linking:', '$TARGET')
		self.base['RANLIBCOMSTR'] = self.get_compile_str('Indexing archive:', '$TARGET')
		self.base['GENCOMSTR']    = self.get_compile_str('Generating:', '$TARGET')
		self.base['STRIPCOMSTR']  = self.get_compile_str('Stripping:', '$TARGET')

		# Import version information.
		for k, v in version.items():
			self.base[k] = v

	# Gets the full path to a tool in the toolchain.
	def get_tool_path(self, name):
		return os.path.join(config['TOOLCHAIN_DIR'], \
		                    config['TOOLCHAIN_TARGET'], 'bin', \
		                    config['TOOLCHAIN_TARGET'] + "-" + name)

	# Get a string to use for a compilation string.
	def get_compile_str(self, msg, name):
		if not self.verbose:
			if self.colour:
				return '\033[0;32m>>>\033[0;1m %-21s %s\033[0m' % (msg, name)
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

# Create the environment manager instance.
envmgr = EnvironmentManager(verbose, colour)

###############
# Main build. #
###############

Export('envmgr', 'config')
SConscript('SConscript', build_dir=os.path.join('build', '%s-%s' % (config['ARCH'], config['PLATFORM'])))
