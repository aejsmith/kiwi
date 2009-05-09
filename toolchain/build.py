#!/usr/bin/env python

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

from manager import ToolchainManager

if not os.path.exists('build.conf'):
	print 'Please run this script from the root of the source tree'
	sys.exit(1)

# Read in the configuration.
with open('build.conf') as f:
	exec f

def main():
	if len(sys.argv) > 1 and sys.argv[1] == '--check':
		return ToolchainManager(config).check()
	else:
		return ToolchainManager(config).update()

if __name__ == '__main__':
	sys.exit(main())
