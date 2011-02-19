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

import os
import sys
from urlparse import urlparse
from time import time

# Base class of a toolchain component definition.
class ToolchainComponent:
	def __init__(self, manager):
		self.manager = manager
		self.srcdir = os.path.join(os.getcwd(), 'utilities', 'toolchain', self.name)
		self.dldir = self.manager.parentdir

	# Check if the component requires updating.
	def check(self):
		path = os.path.join(self.manager.destdir, '.%s-%s-installed' % (self.name, self.version))
		if not os.path.exists(path):
			return True

		# Check if any of the patches are newer.
		mtime = os.stat(path).st_mtime
		for p in self.patches:
			if os.stat(os.path.join(self.srcdir, p[0])).st_mtime > mtime:
				return True
		return False

	# Download an unpack all sources for the component.
	def download(self):
		for f in self.source:
			name = urlparse(f).path.split('/')[-1]
			target = os.path.join(self.dldir, name)
			if not os.path.exists(os.path.join(self.dldir, name)):
				self.manager.msg(' Downloading source file: %s' % (name))

				# Download to .part and then rename when
				# complete so we can easily do continuing of
				# downloads.
				self.execute('wget -c -O %s %s' % (target + '.part', f))
				os.rename(target + '.part', target)

			# Unpack if this is a tarball.
			if name[-8:] == '.tar.bz2':
				self.execute('tar -C %s -xjf %s' % (self.manager.builddir, target))
			elif name[-7:] == '.tar.gz':
				self.execute('tar -C %s -xzf %s' % (self.manager.builddir, target))

	# Helper function to execute a command and throw an exception if
	# required status not returned.
	def execute(self, cmd, directory='.', expected=0):
		print "+ %s" % (cmd)
		oldcwd = os.getcwd()
		os.chdir(directory)
		if os.system(cmd) != expected:
			os.chdir(oldcwd)
			raise Exception, 'Command did not return expected value'
		os.chdir(oldcwd)

	# Apply all patches for this component.
	def patch(self):
		for (p, d, s) in self.patches:
			name = os.path.join(self.srcdir, p)
			self.execute('patch -Np%d -i %s' % (s, name), d)

	# Performs all required tasks to update this component.
	def _build(self):
		self.manager.msg("Building toolchain component '%s'" % (self.name))
		self.download()
		self.patch()

		# Measure time taken to build.
		start = time()
		self.build()
		end = time()
		self.manager.totaltime += (end - start)

		# Signal that we've updated this.
		self.changed = True
		f = open(os.path.join(self.manager.destdir, '.%s-%s-installed' % (self.name, self.version)), 'w')
		f.write('')
		f.close()
