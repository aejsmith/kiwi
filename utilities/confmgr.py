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
from SCons.Script import *

# Class to manage build configuration.
class ConfigManager(UserDict):
	# Import the build configuration into the configuration manager.
	def __init__(self, path):
		UserDict.__init__(self)

		# Read in the configuration file - it is a Python script, so
		# just open the file stream and exec it.
		with open(path) as f:
			exec f

		# The build configuration file sets the config variable. Now
		# import the variables in this into ourself.
		for k, v in config.items():
			self[k] = v

		# Set any special settings.
		self.check_validity('ARCH', ['ia32', 'amd64'])
		if self['ARCH'] == 'ia32' or self['ARCH'] == 'amd64':
			self.check_validity('PLATFORM', ['pc'])
			if self['ARCH'] == 'ia32':
				self['TOOLCHAIN_TARGET'] = 'i686-kiwi'
				self['ARCH_32BIT'] = True
			elif self['ARCH'] == 'amd64':
				self['TOOLCHAIN_TARGET'] = 'x86_64-kiwi'
				self['ARCH_64BIT'] = True

			self['ARCH_LITTLE_ENDIAN'] = True
			self['ARCH_HAS_MEMCPY'] = True
			self['ARCH_HAS_MEMSET'] = True
			self['ARCH_HAS_MEMMOVE'] = True

	# Check validity of the given key.
	def check_validity(self, key, allowed):
		for a in allowed:
			if self[key] == a:
				return
		raise Exception, "Unknown value '%s' for config['%s']" % (config[key], key)
