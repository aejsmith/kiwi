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
import SCons.Errors

# Import build utilities. It'd be nice if SCons let you change the name of
# the site_scons directory.
from utilities.envmgr import EnvironmentManager
from utilities.confmgr import ConfigManager
from utilities.toolchain import ToolchainManager

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

###################
# Utility set-up. #
###################

# Import the build configuration.
config = ConfigManager('build.conf')

# Create the toolchain manager instance.
toolchain = ToolchainManager(config)

# Create the environment manager instance.
envmgr = EnvironmentManager(config, version)

###############
# Main build. #
###############

# If the toolchain is out of date, only allow the toolchain to be built.
if toolchain.check() != 0:
	if not 'toolchain' in COMMAND_LINE_TARGETS and not GetOption('help'):
		raise SCons.Errors.StopError("Toolchain is out of date. Update using the 'toolchain' target.")
	Alias('toolchain', Command('toolchain', [], Action(toolchain.update, None)))
else:
	Export('envmgr', 'config')
	SConscript('SConscript', build_dir=os.path.join('build', '%s-%s' % (config['ARCH'], config['PLATFORM'])))
