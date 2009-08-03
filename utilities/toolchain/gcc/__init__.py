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
from utilities.toolchain.component import ToolchainComponent

class GCCComponent(ToolchainComponent):
	name = 'gcc'
	version = '4.4.1'
	depends = ['binutils']
	source = [
		'http://ftp.gnu.org/gnu/gcc/gcc-' + version + '/gcc-core-' + version + '.tar.bz2',
		'http://ftp.gnu.org/gnu/gcc/gcc-' + version + '/gcc-g++-' + version + '.tar.bz2',
	]
	patches = [
		('gcc-' + version + '-kiwi.patch', 'gcc-' + version, 1),
		('gcc-' + version + '-no-fixinc.patch', 'gcc-' + version, 1),
	]

	def build(self):
		os.mkdir('gcc-build')

		# Build and install it.
		self.execute(
			'../gcc-%s/configure --prefix=%s --target=%s ' \
			'--enable-languages=c,c++' \
			% (self.version, self.manager.destdir, self.manager.target),
			'gcc-build'
		)
		self.execute('make -j%d all-gcc all-target-libgcc' % (self.manager.makejobs), 'gcc-build')
		self.execute('make install-gcc install-target-libgcc', 'gcc-build')
