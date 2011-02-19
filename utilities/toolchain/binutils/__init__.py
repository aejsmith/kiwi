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
from utilities.toolchain.component import ToolchainComponent

class BinutilsComponent(ToolchainComponent):
	name = 'binutils'
	version = '2.21'
	source = [
		'http://ftp.gnu.org/gnu/binutils/binutils-' + version + '.tar.bz2',
	]
	patches = [
		('binutils-' + version + '-kiwi.patch', 'binutils-' + version, 1),
	]

	def build(self):
		os.mkdir('binutils-build')

		# Work out configure options to use.
		confopts  = '--prefix=%s ' % (self.manager.destdir)
		confopts += '--target=%s ' % (self.manager.target)
		confopts += '--disable-werror '
		# Note: If adding platforms which do not support gold, their
		# GCC target definition must add --no-copy-dt-needed-entries
		# to LINK_SPEC. This behaviour is the default with gold, but
		# the opposite with GNU ld.
		confopts += '--enable-gold '

		# Build and install it.
		self.execute('../binutils-%s/configure %s' % (self.version, confopts), 'binutils-build')
		self.execute('make -j%d' % (self.manager.makejobs), 'binutils-build')
		self.execute('make install', 'binutils-build')
