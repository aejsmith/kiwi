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

# Set the version string representation.
version['KIWI_VER_STRING'] = '%d.%d.%d' % (version['KIWI_VER_RELEASE'],
                                           version['KIWI_VER_UPDATE'],
                                           version['KIWI_VER_REVISION'])

###############
# Main build. #
###############

# Raise an error if a certain target is not specified.
def RequireTarget(target, error):
	if GetOption('help') or target in COMMAND_LINE_TARGETS:
		return
	raise SCons.Errors.StopError(error)

# Import the build configuration.
config = ConfigManager('config.tpl', '.config', version)
Alias('config', Command('config', [], Action(config.configure, None)))

# Only do the rest of the build if the configuration exists.
if config.configured() and not 'config' in COMMAND_LINE_TARGETS:
	# Initialize the toolchain manager and add the toolchain build target.
	toolchain = ToolchainManager(config)
	Alias('toolchain', Command('toolchain', [], Action(toolchain.update, None)))

	# Create the environment manager instance.
	envmgr = EnvironmentManager(config, version)

	# If the toolchain is out of date, only allow it to be built.
	if toolchain.check() != 0:
		RequireTarget('toolchain', "Toolchain out of date. Update using the 'toolchain' target.")
	else:
		Export('config', 'envmgr')
		env = envmgr['host']
		SConscript('utilities/SConscript', variant_dir=os.path.join('build', 'host'), exports=['env'])
		SConscript('SConscript', variant_dir=os.path.join('build', '%s-%s' % (config['ARCH'], config['PLATFORM'])))
else:
	# Configuration does not exist. All we can do is configure.
	RequireTarget('config', "Build configuration doesn't exist. Please create using 'config' target.")
