# Kiwi toolchain build script
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

from utilities.toolchain.binutils import BinutilsComponent
from utilities.toolchain.gcc import GCCComponent

from SCons.Script import *

# Class to manage building and updating the toolchain.
class ToolchainManager:
	def __init__(self, config):
		self.config = config
		self.parentdir = config['TOOLCHAIN_DIR']
		self.destdir = os.path.join(config['TOOLCHAIN_DIR'], config['TOOLCHAIN_TARGET'])
		self.builddir = os.path.join(self.destdir, 'build-tmp')
		self.target = config['TOOLCHAIN_TARGET']
		self.makejobs = config['TOOLCHAIN_MAKE_JOBS']
		self.totaltime = 0

		# Keep sorted in dependency order.
		self.components = [
			BinutilsComponent(self),
			GCCComponent(self),
		]

	# Write a status message.
	def msg(self, msg):
		print '\033[1;34m>>>\033[0;1m %s\033[0m' % (msg)

	# Check whether any dependencies of a component have changed.
	def checkdeps(self, c):
		for d in c.depends:
			for c in self.components:
				if c.name == d and c.changed:
					return True
		return False

	# Repairs any links within the toolchain directory.
	def repair(self):
		# Remove existing stuff.
		self.remove('%s/%s/include' % (self.destdir, self.target))
		self.remove('%s/%s/lib' % (self.destdir, self.target))

		# Link into the source tree.
		os.symlink('%s/build/%s-%s/source/uspace/include' % (os.getcwd(), self.config['ARCH'], self.config['PLATFORM']),
		           '%s/%s/include' % (self.destdir, self.target))
		os.symlink('%s/build/%s-%s/source/uspace/libraries' % (os.getcwd(), self.config['ARCH'], self.config['PLATFORM']),
		           '%s/%s/lib' % (self.destdir, self.target))

	# Remove a file, symbolic link or directory tree.
	def remove(self, path):
		if not os.path.lexists(path):
			return

		# Handle symbolic links first as isfile() and isdir() follow
		# links.
		if os.path.islink(path) or os.path.isfile(path):
			os.remove(path)
		elif os.path.isdir(path):
			for root, dirs, files in os.walk(path, topdown=False):
				for name in files:
					os.remove(os.path.join(root, name))
				for name in dirs:
					os.rmdir(os.path.join(root, name))
			os.rmdir(path)
		else:
			raise Exception, "Unhandled type during remove (%s)" % (path)

	# Build a component.
	def build(self, c):
		# Create the target directory and change into it.
		os.makedirs(self.builddir)
		olddir = os.getcwd()
		os.chdir(self.builddir)

		# Perform the actual build.
		c._build()

		# Change to the old directory and clean up the build directory.
		os.chdir(olddir)
		self.remove(self.builddir)

	# Check if an update is required.
	def check(self):
		for c in self.components:
			if self.checkdeps(c) or c.check():
				return 1
		self.repair()
		return 0

	# Rebuilds any components of the toolchain that are out of date.
	def update(self, target, source, env):
		# Remove any existing build directory and create the target
		# directory if required. Also remove symlinks while building
		# as it interferes with build.
		self.remove(self.builddir)
		if not os.path.exists(self.destdir):
			os.makedirs(self.destdir)
		else:
			self.remove('%s/%s/include' % (self.destdir, self.target))
			self.remove('%s/%s/lib' % (self.destdir, self.target))

		# Build necessary components.
		for c in self.components:
			if self.checkdeps(c) or c.check():
				self.build(c)

		self.repair()

		if self.totaltime != 0:
			self.msg('Toolchain updated in %d seconds' % (self.totaltime))
