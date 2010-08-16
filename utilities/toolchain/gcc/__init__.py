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
from utilities.toolchain.component import ToolchainComponent

class GCCComponent(ToolchainComponent):
	name = 'gcc'
	version = '4.5.1'
	source = [
		'http://ftp.gnu.org/gnu/gcc/gcc-' + version + '/gcc-core-' + version + '.tar.bz2',
		'http://ftp.gnu.org/gnu/gcc/gcc-' + version + '/gcc-g++-' + version + '.tar.bz2',
	]
	patches = [
		('gcc-' + version + '-no-fixinc.patch', 'gcc-' + version, 1),
		('gcc-' + version + '-kiwi.patch', 'gcc-' + version, 1),
		('gcc-' + version + '-autoconf.patch', 'gcc-' + version, 1),
	]

	def build(self):
		os.mkdir('gcc-build')

		# Work out configure options to use.
		confopts  = '--prefix=%s ' % (self.manager.destdir)
		confopts += '--target=%s ' % (self.manager.target)
		confopts += '--enable-languages=c,c++ '
		confopts += '--disable-libstdcxx-pch '
		confopts += '--disable-shared '
		if os.uname()[0] == 'Darwin':
			confopts += '--with-libiconv-prefix=/opt/local --with-gmp=/opt/local --with-mpfr=/opt/local'

		# Build and install it.
		self.execute('../gcc-%s/configure %s' % (self.version, confopts), 'gcc-build')
		self.execute('make -j%d all-gcc' % (self.manager.makejobs), 'gcc-build')
		self.execute('make -j%d all-target-libgcc all-target-libstdc++-v3' % (self.manager.makejobs), 'gcc-build')
		self.execute('make install-gcc install-target-libgcc install-target-libstdc++-v3', 'gcc-build')
